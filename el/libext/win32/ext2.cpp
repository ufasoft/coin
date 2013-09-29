/*######     Copyright (c) 1997-2013 Ufasoft  http://ufasoft.com  mailto:support@ufasoft.com,  Sergey Pavlov  mailto:dev@ufasoft.com #######################################
#                                                                                                                                                                          #
# This program is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation;  #
# either version 3, or (at your option) any later version. This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the      #
# implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details. You should have received a copy of the GNU #
# General Public License along with this program; If not, see <http://www.gnu.org/licenses/>                                                                               #
##########################################################################################################################################################################*/

#include <el/ext.h>

#include <wincon.h>

#include <el/libext/win32/ext-win.h>
#include <el/libext/win32/ext-full-win.h>

#pragma warning(disable: 4073)
#pragma init_seg(lib)

#if !UCFG_WCE
//#	include <AccCtrl.h>
#	include <sddl.h>
//#	include <aclapi.h>
#	include <winternl.h>
#endif

#if UCFG_WIN32_FULL
#	pragma comment(lib, "version")
#endif

namespace Ext {

using namespace std;

#if !UCFG_WCE

void NamedPipe::Create(const NamedPipeCreateInfo& ci) {
	HANDLE h = ::CreateNamedPipe(ci.Name, ci.OpenMode, ci.PipeMode, ci.MaxInstances, 4096, 4096, 0, ci.Sa);
	Win32Check(h != INVALID_HANDLE_VALUE);
	Attach(h);
}

bool NamedPipe::Connect(LPOVERLAPPED ovl) {
	if (ovl)
		Win32Check(::ResetEvent(ovl->hEvent));
	bool r = ::ConnectNamedPipe(Handle(_self), ovl) || ::GetLastError() == ERROR_PIPE_CONNECTED;
	if (ovl) {
		if (!r) {
			int dw = ::WaitForSingleObjectEx(ovl->hEvent, INFINITE, TRUE);
			if (dw == WAIT_IO_COMPLETION)
				Throw(E_EXT_ThreadStopped);
			if (dw != 0)
				Throw(E_FAIL);
			dw = GetOverlappedResult(*ovl);		//!!!
			r = true;
		}
	} else
		Win32Check(r, ERROR_PIPE_LISTENING);
	return r;
}

#endif


FileVersionInfo::FileVersionInfo(RCString fileName) {
	String s = fileName != nullptr ? fileName : AfxGetModuleState()->FileName;
	DWORD dw;
	int size = GetFileVersionInfoSize((TCHAR*)(const TCHAR*)s, &dw);
	if (!size && !fileName)
		return; //!!!
	Win32Check(size);
	m_blob.Size = size;
	Win32Check(GetFileVersionInfo((TCHAR*)(const TCHAR*)s, 0, size, m_blob.data()));
}

String FileVersionInfo::GetStringFileInfo(RCString s) {
	UINT size;
	void *p, *q;
	Win32Check(::VerQueryValue(m_blob.data(), _T("\\VarFileInfo\\Translation"), &q, &size));
	Win32Check(::VerQueryValue(m_blob.data(),
		(TCHAR*)(const TCHAR*)(_T("\\StringFileInfo\\")+Convert::ToString((*(WORD*)q << 16) | *(WORD*)((char*)q+2), "X8")+_T("\\")+s),
		&p, &size));
	return (TCHAR*)p;
}

const VS_FIXEDFILEINFO& FileVersionInfo::get_FixedInfo() {
	UINT size;
	void *p;
	Win32Check(::VerQueryValue(m_blob.data(), _T("\\"), &p, &size));
	return *(VS_FIXEDFILEINFO*)p;
}


String AFXAPI TryGetVersionString(const FileVersionInfo& vi, RCString name, RCString val) {
	//!!! FileVersionInfo vi(System.ExeFilePath);  
	UINT size;
	void *p, *q;
	Win32Check(::VerQueryValue((void*)vi.m_blob.data(), _T("\\VarFileInfo\\Translation"), &q, &size));
	BOOL b = ::VerQueryValue((void*)vi.m_blob.data(), (TCHAR*)(const TCHAR*)(_T("\\StringFileInfo\\")+Convert::ToString((*(WORD*)q << 16) | *(WORD*)((char*)q+2), "X8")+_T("\\")+name), &p, &size);
	if (b)
		return (TCHAR*)p;
	else
		return val;
}

bool AFXAPI IsConsole() {
	BYTE *base = (BYTE*)GetModuleHandle(0);
	IMAGE_DOS_HEADER *dh = (IMAGE_DOS_HEADER*)base;
	IMAGE_OPTIONAL_HEADER32 *oh = (IMAGE_OPTIONAL_HEADER32*)(base+dh->e_lfanew+IMAGE_SIZEOF_FILE_HEADER+4);
	return oh->Subsystem == IMAGE_SUBSYSTEM_WINDOWS_CUI;
}





/*!!!D
String ExtractFilePath(RCString path)
{
CSplitPath sp = SplitPath(path);  
return sp.m_drive+sp.m_dir;
}*/



/*!!!
HRESULT ProcessOleException(CException *e)
{
HRESULT hr = COleException::Process(e);
e->Delete();
return hr;
}*/

void AFXAPI SelfRegisterDll(RCString path) {
	CDynamicLibrary lib;
	lib.Load(path);
	typedef HRESULT (STDAPICALLTYPE *CTLREGPROC)();
	CTLREGPROC proc = (CTLREGPROC)lib.GetProcAddress("DllRegisterServer");
	OleCheck(proc());
}



CStringVector AsciizArrayToStringArray(const TCHAR *p) {
	vector<String> vec;
	for (; *p; p+=_tcslen(p)+1)
		vec.push_back(p);
	return vec;
}


SIZE_T Process::ReadMemory(LPCVOID base, LPVOID buf, SIZE_T size) {
	SIZE_T r;
	Win32Check(::ReadProcessMemory(HandleAccess(_self), base, buf, size, &r));
	return r;
}

SIZE_T Process::WriteMemory(LPVOID base, LPCVOID buf, SIZE_T size) {
	SIZE_T r;
	Win32Check(::WriteProcessMemory(HandleAccess(_self), base, (void*)buf, size, &r)); //!!!CE
	return r;
}

DWORD Process::VirtualProtect(void *addr, size_t size, DWORD flNewProtect) {
	DWORD r;
	if (GetCurrentProcessId() == ID)
		Win32Check(::VirtualProtect(addr, size, flNewProtect, &r));
	else
#if UCFG_WCE
		Throw(E_FAIL);
#else
		Win32Check(::VirtualProtectEx(HandleAccess(_self), addr, size, flNewProtect, &r));
#endif
	return r;
}

MEMORY_BASIC_INFORMATION Process::VirtualQuery(const void *addr) {
	MEMORY_BASIC_INFORMATION r;
	SIZE_T size;
	if (GetCurrentProcessId() == ID)
		size = ::VirtualQuery(addr, &r, sizeof r);
	else
#if UCFG_WCE
		Throw(E_FAIL);
#else
		size = ::VirtualQueryEx(HandleAccess(_self), addr, &r, sizeof r);
#endif
	if (!size)
		ZeroStruct(r);
	return r;
}

DWORD Process::get_ID() const {
	if (!m_pid) {
#if UCFG_WCE
		Throw(E_FAIL);
#else
		typedef DWORD (WINAPI *PFN_GetProcessId)(HANDLE);
		DlProcWrap<PFN_GetProcessId> pfn("KERNEL32.DLL", "GetProcessId");
		if (pfn)
			m_pid = pfn(Handle(_self));
		else {
			/*!!!R
			typedef enum _PROCESSINFOCLASS {
				ProcessBasicInformation = 0,
				ProcessWow64Information = 26
			} PROCESSINFOCLASS;

			typedef void *PPEB;

			typedef struct _PROCESS_BASIC_INFORMATION {
				PVOID Reserved1;
				PPEB PebBaseAddress;
				PVOID Reserved2[2];
				ULONG_PTR UniqueProcessId;
				PVOID Reserved3;
			} PROCESS_BASIC_INFORMATION;
			typedef PROCESS_BASIC_INFORMATION *PPROCESS_BASIC_INFORMATION;
*/
			typedef NTSTATUS (WINAPI * PFN_QueryInformationProcess)(
				HANDLE ProcessHandle,
				PROCESSINFOCLASS ProcessInformationClass,
				PVOID ProcessInformation,
				ULONG ProcessInformationLength,
				PULONG ReturnLength);

			DlProcWrap<PFN_QueryInformationProcess> ntQIP("NTDLL.DLL", "NtQueryInformationProcess");
			if (!ntQIP)
				Throw(E_FAIL);

			PROCESS_BASIC_INFORMATION info;
			ULONG returnSize;
			ntQIP(Handle(_self), ProcessBasicInformation, &info, sizeof(info), &returnSize);  // Get basic information.
			m_pid = (DWORD)info.UniqueProcessId;		
		}
#endif
	}
	return m_pid;
}

struct SMainWindowHandleInfo {
	DWORD m_pid;
	HWND m_hwnd;
};

static BOOL CALLBACK EnumWindowsProc_MainWindowHandle(HWND hwnd, LPARAM lParam) {
	SMainWindowHandleInfo& info = *(SMainWindowHandleInfo*)lParam;
	if ((::GetWindowLong(hwnd, GWL_STYLE) & WS_VISIBLE)) {
		DWORD pidwin;
		::GetWindowThreadProcessId(hwnd, &pidwin);
		if (pidwin == info.m_pid) {
			info.m_hwnd = hwnd;
			::SetLastError(0);
			return FALSE;
		}
	}
	return TRUE;
}

HWND Process::get_MainWindowHandle() const {
	SMainWindowHandleInfo info = { ID };
	Win32Check(::EnumWindows(&EnumWindowsProc_MainWindowHandle, (LPARAM)&info), 0);
	return info.m_hwnd;
}

#if !UCFG_WCE


DWORD Process::get_Version() const {
	DWORD r = GetProcessVersion(ID);
	Win32Check(r);
	return r;
}

typedef WINBASEAPI BOOL (WINAPI *PFN_IsWow64Process)(HANDLE hProcess, PBOOL Wow64Process);

bool Process::get_IsWow64() {
	BOOL r = FALSE;
	DlProcWrap<PFN_IsWow64Process> pfnIsWow64Process("KERNEL32.DLL", "IsWow64Process");
	if (pfnIsWow64Process)
		Win32Check(pfnIsWow64Process(HandleAccess(_self), &r));
	return r;
}

CTimesInfo Process::get_Times() const {
	CTimesInfo r;
	Win32Check(::GetProcessTimes(HandleAccess(_self), &r.m_tmCreation, &r.m_tmExit, &r.m_tmKernel, &r.m_tmUser));
	return r;
}

#endif  // !UCFG_WCE

#ifdef WIN32

#if UCFG_EXTENDED
String AFXAPI AfxLoadString(UINT nIDS) {
	HINSTANCE h = AfxFindResourceHandle(MAKEINTRESOURCE(nIDS/16+1), RT_STRING);
	TCHAR buf[255];
	int nLen = ::LoadString(h, nIDS , buf, _countof(buf));
	Win32Check(nLen != 0);
	return buf;
}
#else
String AFXAPI AfxLoadString(UINT nIDS) {
	HINSTANCE h = GetModuleHandle(0);		//!!! only this module
	TCHAR buf[255];
	int nLen = ::LoadString(h, nIDS , buf, _countof(buf));
	Win32Check(nLen != 0);
	return buf;
}
#endif // UCFG_EXTENDED

#	if UCFG_COM

void String::Load(UINT nID) {
	_self = AfxLoadString(nID);
}

/*!!!R
bool String::LoadString(UINT nID) {			//!!!comp
	try {
		Load(nID);
		return true;
	} catch (RCExc) {
		return false;
	}
}*/

#	endif
#endif // WIN32


//!!!R#if UCFG_EXTENDED

bool Process::Start() {
	STARTUPINFO si = {sizeof si};
	StaticAssert(SW_SHOWNORMAL == 1, Invalid_SW_SHOWNORMAL);
	si.wShowWindow = SW_SHOWNORMAL; // same as SW_NORMAL Critical for running iexplore
	SafeHandle hI, hO, hE;
#if UCFG_WCE
	STARTUPINFO *psi = 0;
	bool bInheritHandles = false;
	LPCTSTR pCurrentDirectory = 0;
#else
	if (StartInfo.RedirectStandardInput || StartInfo.RedirectStandardOutput || StartInfo.RedirectStandardError) {
		si.hStdInput = StdHandle::Get(STD_INPUT_HANDLE);
		si.hStdOutput = StdHandle::Get(STD_OUTPUT_HANDLE);
		si.hStdError = StdHandle::Get(STD_ERROR_HANDLE);
		si.dwFlags |= STARTF_USESTDHANDLES;
#if	 UCFG_EXTENDED
		HANDLE hRead, hWrite;
		SECURITY_ATTRIBUTES sattr = { sizeof(sattr), 0, TRUE };
		if (StartInfo.RedirectStandardInput) {
			Win32Check(::CreatePipe(&hRead, &hWrite, &sattr, 0));
			si.hStdInput = hRead;
			hI.Attach(hRead);
			StandardInput.m_pFile->Duplicate(hWrite, DUPLICATE_CLOSE_SOURCE|DUPLICATE_SAME_ACCESS);
		}
		if (StartInfo.RedirectStandardOutput) {
			Win32Check(::CreatePipe(&hRead, &hWrite, &sattr, 0));
			si.hStdOutput = hWrite;
			hO.Attach(hWrite);
			StandardOutput.m_pFile->Duplicate(hRead, DUPLICATE_CLOSE_SOURCE|DUPLICATE_SAME_ACCESS);
		}
		if (StartInfo.RedirectStandardError) {
			Win32Check(::CreatePipe(&hRead, &hWrite, &sattr, 0));
			si.hStdError = hWrite;
			hE.Attach(hWrite);
			StandardError.m_pFile->Duplicate(hRead, DUPLICATE_CLOSE_SOURCE|DUPLICATE_SAME_ACCESS);
		}
#	endif
	}
	STARTUPINFO *psi = &si;
	bool bInheritHandles = bool(si.dwFlags & STARTF_USESTDHANDLES);
	String dir = StartInfo.WorkingDirectory.IsEmpty() ? nullptr : StartInfo.WorkingDirectory;
	LPCTSTR pCurrentDirectory = dir;
#endif

	PROCESS_INFORMATION pi;
	String cls = StartInfo.FileName;
	if (!StartInfo.Arguments.IsEmpty())
		cls += " "+StartInfo.Arguments;
	size_t len = (cls.Length+1)*sizeof(TCHAR);
	TCHAR *cl = (TCHAR*)alloca(len);
	memcpy(cl, (const TCHAR*)cls, len);
	String fileName = StartInfo.FileName.IsEmpty() ? nullptr : StartInfo.FileName;
	DWORD flags = StartInfo.Flags;
#if !UCFG_WCE
	if (StartInfo.CreateNoWindow)
		flags |= CREATE_NO_WINDOW;
#endif
	Win32Check(::CreateProcess(0, cl, 0, 0, bInheritHandles, flags, 0, (LPTSTR)pCurrentDirectory, psi, &pi)); //!!! BOOL is not bool
	Attach(pi.hProcess);
	m_pid = pi.dwProcessId;
	ptr<CWinThread> pThread(new CWinThread);
	pThread->Attach(pi.hThread, pi.dwThreadId);
	return true;
}

unique_ptr<Process> Process::Start(ProcessStartInfo psi) {
	unique_ptr<Process> p(new Process);
	p->StartInfo = psi;
	p->Start();
	return p;
} 

//!!!R #endif


DWORD COperatingSystem::GetSysColor(int nIndex) {
	SetLastError(0);
	DWORD r = ::GetSysColor(nIndex);
	Win32Check(!GetLastError());
	return r;
}

bool COperatingSystem::Is64BitNativeSystem() {
#if UCFG_WIN32_FULL
	typedef void (__stdcall *PFN)(SYSTEM_INFO *psi);
	CDynamicLibrary dll("kernel32.dll");
	if (PFN pfn = (PFN)GetProcAddress(dll.m_hModule, String("GetNativeSystemInfo"))) {
		SYSTEM_INFO si;
		pfn(&si);
		return si.wProcessorArchitecture == PROCESSOR_ARCHITECTURE_AMD64;
	}
#endif
	return false;
}


void AFXAPI WinExecAndWait(RCString name) {
	STARTUPINFO info = { sizeof info };
	//!!!	STARTUPINFO *pinfo = &info;
	PROCESS_INFORMATION procInfo;
#if UCFG_WCE
	STARTUPINFO *psi = 0;
#else
	info.dwFlags = STARTF_USESHOWWINDOW;
	info.wShowWindow = SW_NORMAL;
	STARTUPINFO *psi = &info;
#endif
	vector<TCHAR> bufName(_tcslen(name)+1);
	_tcscpy(&bufName[0], name);
	if (::CreateProcess(0, &bufName[0], 0, 0, false, 0, 0, 0, psi, &procInfo)) {
		CloseHandle(procInfo.hThread);
		WaitForSingleObject(procInfo.hProcess, INFINITE);
		CloseHandle(procInfo.hProcess);
	}
}


COsVersion AFXAPI GetOsVersion() {
	COperatingSystem::COsVerInfo vi = System.Version;
	switch (vi.dwPlatformId) {
#if !UCFG_WCE
	case VER_PLATFORM_WIN32_NT:
		switch (vi.dwMajorVersion) {
		case 4:
			return OSVER_NT4;
		case 5:
			switch (vi.dwMinorVersion)
			{
			case 0: return OSVER_2000;
			case 1: return OSVER_XP;
			default: return OSVER_SERVER_2003;
			}
		case 6:
			switch (vi.dwMinorVersion) {
			case 0:
				switch (vi.wProductType) {
				case VER_NT_DOMAIN_CONTROLLER:
				case VER_NT_SERVER:
					return OSVER_2008;
				}
				return OSVER_VISTA;
			case 1:
				switch (vi.wProductType) {
				case VER_NT_DOMAIN_CONTROLLER:
				case VER_NT_SERVER:
					return OSVER_2008_R2;
				}
				return OSVER_7;
			default:
				return OSVER_8;
			}
		default:
			return OSVER_FUTURE;
		}
	case VER_PLATFORM_WIN32_WINDOWS:
		switch (vi.dwMajorVersion) {
		case 4:
			return OSVER_98; //!!! maybe 95;
		default:
			return OSVER_ME;
		}
#else
	case VER_PLATFORM_WIN32_CE:
		switch (vi.dwMajorVersion) {
		case 3: return OSVER_CE;
		case 4: return OSVER_CE_4;
		case 5: return OSVER_CE_5;
		case 6: return OSVER_CE_6;
		default: return OSVER_CE_FUTURE;
		}
#endif
	default:
		Throw(E_FAIL);

	}
}

/*!!!R
void *AlignedMalloc(size_t size, int align)
{
BYTE *pb = new BYTE[size+align+4-1];
void *r = (void*)(DWORD_PTR(pb+align+4-1) / align * align);
*((void**)r-1) = pb;
return r;
}

void AlignedFree(void *p)
{
if (p)
delete[] *((BYTE**)p-1);
}
*/

#if UCFG_EXTENDED

class CSetNewHandler {
	_PNH m_prev;

	static int __cdecl MyNewHandler(size_t) {
		Throw(E_OUTOFMEMORY);
		return 0; //!!!
	}
public:
	CSetNewHandler() {
		m_prev = _set_new_handler(MyNewHandler);
	}

	~CSetNewHandler() {
		_set_new_handler(m_prev);
	}
} g_setNewHandler;

#endif

/*!!!
void * __cdecl operator new(size_t size)
{
if (void *p = ::operator new(size))
return p;
Throw(E_OUTOFMEMORY);
}
*/


#if UCFG_USE_OLD_MSVCRTDLL
extern "C" {

	extern "C" _CRTIMP void __cdecl _assert(const char *, const char *, unsigned);

	void __stdcall my_wassert(const wchar_t *_Message, const wchar_t *_File, unsigned _Line) {
		_assert(String(_Message), String(_File), _Line);
	}
}
#endif



} // Ext::

