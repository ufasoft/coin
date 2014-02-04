#include <el/ext.h>

#if UCFG_WIN32
#	include <el/libext/win32/ext-win.h>
#endif

#if UCFG_WIN32_FULL
#	include <dbghelp.h>
#	pragma comment(lib, "dbghelp")

#	pragma warning(disable : 4740)  // flow in or out of inline asm code suppresses global optimization
#endif


#pragma warning(disable: 4073)
#pragma init_seg(lib)				// to initialize DateTime::MaxValue early

namespace Ext {

using namespace std;

bool CStackTrace::Use = false; // UCFG_DEBUG;

CStackTrace CStackTrace::FromCurrentThread() {
	CStackTrace r;

#if UCFG_WIN32
	DWORD machineType = IMAGE_FILE_MACHINE_I386;
	CONTEXT ctx = { CONTEXT_CONTROL };
#ifdef _M_X64
	machineType = IMAGE_FILE_MACHINE_AMD64;
	::RtlCaptureContext(&ctx);
#endif

	STACKFRAME64 sf = { 0 };
#ifdef _M_IX86
	__asm {
	Label:
		mov [ctx.Ebp], ebp;
		mov [ctx.Esp], esp;
		mov eax, [Label];
		mov [ctx.Eip], eax;
	}

	sf.AddrPC.Offset      = ctx.Eip;
	sf.AddrStack.Offset   = ctx.Esp;
	sf.AddrFrame.Offset   = ctx.Ebp;
#elif defined(_M_X64)
	sf.AddrPC.Offset      = ctx.Rip;
	sf.AddrStack.Offset   = ctx.Rsp;
	sf.AddrFrame.Offset   = ctx.Rbp;
#endif
	sf.AddrPC.Mode        = AddrModeFlat;
	sf.AddrStack.Mode     = AddrModeFlat;
	sf.AddrFrame.Mode     = AddrModeFlat;

	while (::StackWalk64(machineType, ::GetCurrentProcess(), ::GetCurrentThread(), &sf, 0, 0, &SymFunctionTableAccess64, &SymGetModuleBase64, 0)) {
		r.Frames.push_back(sf.AddrPC.Offset);
	}
#endif // UCFG_WIN32
	return r;
}

#if UCFG_WIN32

static mutex s_csSym;

class CSym {
public:
	HANDLE m_hProcess;
	CBool m_bInited;

	CSym()
		:	m_hProcess(0)
	{}

	~CSym() {
		try {
 			if (HANDLE h = exchange(m_hProcess, (HANDLE)0))
				Win32Check(::SymCleanup(h));
		} catch (RCExc) {
		}		
	}

	void Init() {
		EXT_LOCK (s_csSym) {
			try {
				if (!m_hProcess) {
					HANDLE h = ::GetCurrentProcess();
					Win32Check(::SymInitialize(h, 0, TRUE));
					m_hProcess = h;
				}
			} catch (RCExc) {
			}
		}
	}
} s_sym;

#endif


String CStackTrace::ToString() const {
#if UCFG_WIN32
	s_sym.Init();

	const int MAX_NAME_LEN = 256;
	SYMBOL_INFO *si = (SYMBOL_INFO*)alloca(sizeof(SYMBOL_INFO)+sizeof(TCHAR)*MAX_NAME_LEN);
	si->SizeOfStruct = sizeof(SYMBOL_INFO);
	si->MaxNameLen = MAX_NAME_LEN;

	Process proc = Process::GetCurrentProcess();
#endif

	ostringstream os;
	for (size_t i=0; i<Frames.size(); ++i) {
		UInt64 addr = Frames[i];
		os << setw(AddrSize*2) << setfill('0') << hex << addr;

#if UCFG_WIN32
		if (::SymFromAddr(s_sym.m_hProcess, addr, 0, si)) {
			os << "  ";
			DWORD64 hm = SymGetModuleBase64(s_sym.m_hProcess, addr);
			if (hm) {
				ProcessModule mod(proc, (HMODULE)hm);
				os << mod.ModuleName << "!";
			}			
			os << si->Name;
		}
#endif
		os << "\n";
	}
	return os.str();
}


} // Ext::
