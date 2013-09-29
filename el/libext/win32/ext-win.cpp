/*######     Copyright (c) 1997-2013 Ufasoft  http://ufasoft.com  mailto:support@ufasoft.com,  Sergey Pavlov  mailto:dev@ufasoft.com #######################################
#                                                                                                                                                                          #
# This program is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation;  #
# either version 3, or (at your option) any later version. This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the      #
# implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details. You should have received a copy of the GNU #
# General Public License along with this program; If not, see <http://www.gnu.org/licenses/>                                                                               #
##########################################################################################################################################################################*/

#include <el/ext.h>

#include <windows.h>
#include <commctrl.h>

#if UCFG_WND
#	include <el/libext/win32/ext-wnd.h>
#endif

#if UCFG_GUI
#	include <el/gui/gdi.h>
#	include <el/gui/menu.h>

#	if UCFG_EXTENDED
#		include <el/gui/controls.h>
#	endif
#	include <el/gui/ext-image.h>
#endif

#include <el/libext/win32/ext-win.h>
#include <el/libext/win32/ext-full-win.h>

#pragma warning(disable: 4073)
#pragma init_seg(lib)

#if UCFG_CRASH_DUMP
#	include <el/comp/crashdump.h>
#endif

namespace Ext {
#if UCFG_EXTENDED && UCFG_WIN_MSG
#	include "mfcimpl.h"
#endif

using namespace std;

#if UCFG_EXTENDED && UCFG_GUI

CCursor Cursor;

#else

/*!!!R String AFXAPI AfxProcessError(HRESULT hr) {
	return Convert::ToString(hr, 16);
}*/

#endif // UCFG_EXTENDED


#if UCFG_USE_DECLSPEC_THREAD
__declspec(thread) HRESULT t_lastHResult;
HRESULT AFXAPI GetLastHResult() { return t_lastHResult; }
void AFXAPI SetLastHResult(HRESULT hr) { t_lastHResult = hr; }
#else
CTls t_lastHResult;
HRESULT AFXAPI GetLastHResult() { return (HRESULT)(uintptr_t)t_lastHResult.Value; }
void AFXAPI SetLastHResult(HRESULT hr) { t_lastHResult.Value = (void*)(uintptr_t)hr; }
#endif // UCFG_USE_DECLSPEC_THREAD

int AFXAPI Win32Check(LRESULT i) {
	if (i)
		return (int)i;
	DWORD dw = ::GetLastError();
	if (dw & 0xFFFF0000)
		Throw(dw);
	if (dw)
		Throw(HRESULT_FROM_WIN32(dw));
	else if (HRESULT hr = GetLastHResult())
		Throw(hr);
	else
		Throw(E_EXT_UnknownWin32Error);
}

bool AFXAPI Win32Check(BOOL b, DWORD allowableError) {
	Win32Check(b || ::GetLastError()==allowableError);
	return b;
}

void CSyncObject::AttachCreated(HANDLE h) {
	Attach(h);
	m_bAlreadyExists = GetLastError()==ERROR_ALREADY_EXISTS;
}

ProcessModule::ProcessModule(class Process& process, HMODULE hModule)
	:	Process(&process)
	,	HModule(hModule)
{
	Win32Check(::GetModuleInformation(Handle(process), hModule, &m_mi, sizeof m_mi));
}

#if !UCFG_WCE

HANDLE StdHandle::Get(DWORD nStdHandle) {
	HANDLE h = ::GetStdHandle(nStdHandle);
	Win32Check(h != INVALID_HANDLE_VALUE);
	return h;
}

vector<String> COperatingSystem::get_LogicalDriveStrings() {
	vector<String> r;
	int n = Win32Check(::GetLogicalDriveStrings(0, 0))+1;
	TCHAR *p = (TCHAR*)alloca(n*sizeof(TCHAR));
	n = Win32Check(::GetLogicalDriveStrings(n, p));
	for (; *p; p+=_tcslen(p)+1)
		r.push_back(p);
	return r;
}

String ProcessModule::get_FileName() const {
	TCHAR buf[MAX_PATH];
	Win32Check(::GetModuleFileNameEx(Handle(*Process), HModule, buf, _countof(buf)));
	return buf;
}

String ProcessModule::get_ModuleName() const {
	TCHAR buf[MAX_PATH];
	Win32Check(::GetModuleBaseName(Handle(*Process), HModule, buf, _countof(buf)));
	return buf;
}

vector<ProcessModule> Process::get_Modules() {
	vector<HMODULE> ar(5);
	for (bool bContinue=true; bContinue;) {
		DWORD cbNeeded;
		BOOL r = ::EnumProcessModules(HandleAccess(_self), &ar[0], ar.size()*sizeof(HMODULE), &cbNeeded);
		Win32Check(r);
		bContinue = cbNeeded > ar.size()*sizeof(HMODULE);
		ar.resize(cbNeeded/sizeof(HMODULE));
	}
	vector<ProcessModule> arm;
	for (int i=0; i<ar.size(); ++i)
		arm.push_back(ProcessModule(_self, ar[i]));
	return arm;
}

static String ToDosPath(RCString lpath) {
	vector<String> lds = System.LogicalDriveStrings;
	for (int i=0; i<lds.size(); ++i) {
		String ld = lds[i];
		String dd = ld.Right(1)=="\\" ? ld.Left(ld.Length-1) : ld;
		vector<String> v = System.QueryDosDevice(dd);
		if (v.size()) {
			String lp = v[0];
			if (lp.Length < lpath.Length && !lp.CompareNoCase(lpath.Left(lp.Length))) {
				return ((ld.Right(1) == "\\" && lpath[0] == '\\') ? dd : ld) + lpath.Mid(lp.Length);
			}
		}
	}
	return lpath;
}


typedef DWORD (WINAPI *PFN_GetProcessImageFileName)(HANDLE hProcess, LPTSTR lpImageFileName, DWORD nSize);

static CDynamicLibrary s_dllPsapi("psapi.dll");

String Process::get_MainModuleFileName() {
	TCHAR buf[MAX_PATH];
	try {
		DBG_LOCAL_IGNORE_NAME(HRESULT_FROM_WIN32(ERROR_PARTIAL_COPY), ignERROR_PARTIAL_COPY);	
		DBG_LOCAL_IGNORE_NAME(HRESULT_FROM_WIN32(ERROR_INVALID_HANDLE), ignERROR_INVALID_HANDLE);

		Win32Check(::GetModuleFileNameEx(Handle(_self), 0, buf, _countof(buf)));
		return buf;
	} catch (RCExc) {
	}
	DlProcWrap<PFN_GetProcessImageFileName> pfnGetProcessImageFileName(s_dllPsapi, EXT_WINAPI_WA_NAME(GetProcessImageFileName));
	if (pfnGetProcessImageFileName) {
		Win32Check(pfnGetProcessImageFileName(Handle(_self), buf, _countof(buf)));
		return ToDosPath(buf);
	} else
		Throw(HRESULT_FROM_WIN32(ERROR_PARTIAL_COPY));
}

#endif // !UCFG_WCE

Process::Process()
	:	SafeHandle(0, false)
{
	CommonInit();
}

Process::Process(ProcessEnum current)
	:	SafeHandle(0, false)
{
	CommonInit();
	Attach(::GetCurrentProcess(), false);
	m_pid = GetCurrentProcessId();
}

Process::Process(DWORD id, DWORD dwAccess, bool bInherit)
	:	SafeHandle(0, false)
	,	m_pid(id)
{
	CommonInit();
	if (id)
		Attach(::OpenProcess(dwAccess, bInherit, id));
	/*!!!
	else
	{
		Attach(::GetCurrentProcess());
		m_bOwn = false;
		m_ID = GetCurrentProcessId();
	}*/
}

void Process::CommonInit() {
#if UCFG_EXTENDED && !UCFG_WCE
	StandardInput.m_pFile = &m_fileIn;
	StandardOutput.m_pFile = &m_fileOut;
	StandardError.m_pFile = &m_fileErr;
#endif
}

void COperatingSystem::MessageBeep(UINT uType) {
	Win32Check(::MessageBeep(uType));
}

CWinFileFind::CWinFileFind(LPCTSTR filespec) {
	First(filespec);
}

CWinFileFind::~CWinFileFind() {
	if (INVALID_HANDLE_VALUE != m_h)
		Win32Check(::FindClose(m_h));
}

void CWinFileFind::First(LPCTSTR filespec) {
	m_h = ::FindFirstFile(filespec, &m_findData);
	if (!(m_b = (INVALID_HANDLE_VALUE != m_h)))
		switch (::GetLastError())
		{
		case ERROR_FILE_NOT_FOUND:
		case ERROR_NO_MORE_FILES:
			break;
		default:
			Win32Check(false);
		}
}

bool CWinFileFind::Next(WIN32_FIND_DATA& findData) {
	findData = m_findData;
	bool r = m_b;
	if (m_h != INVALID_HANDLE_VALUE)
		m_b = Win32Check(::FindNextFile(m_h, &m_findData), ERROR_NO_MORE_FILES);
	return r;
}




//!!!#if !UCFG_WCE && UCFG_EXTENDED

#if !UCFG_WCE

DWORD File::GetOverlappedResult(OVERLAPPED& ov, bool bWait) {
	DWORD r;
	Win32Check(::GetOverlappedResult(HandleAccess(_self), &ov, &r, bWait));
	return r;
}


DWORD File::DeviceIoControlAndWait(int code, LPCVOID bufIn, size_t nIn, LPVOID bufOut, size_t nOut) {
	DWORD dw;
	COvlEvent ovl;
	if (!DeviceIoControl(code, bufIn, nIn, bufOut, nOut, &dw, &ovl))
		dw = GetOverlappedResult(ovl);
	return dw;
}
#endif


//!!!#endif // UCFG_WCE


#ifdef _AFXDLL
	AFX_MODULE_STATE _afxBaseModuleState(true, &AfxWndProcBase);
#else
	AFX_MODULE_STATE _afxBaseModuleState(true); //!!!
#endif

EXT_DATA CThreadLocal<_AFX_THREAD_STATE> _afxThreadState;

_AFX_THREAD_STATE * AFXAPI AfxGetThreadState() {
	return _afxThreadState.GetData();
}

AFX_MODULE_THREAD_STATE::AFX_MODULE_THREAD_STATE()
	:	m_pfnNewHandler(0)
#if UCFG_COM_IMPLOBJ
	,	m_comClass(new CComClass)
#endif
{
}

AFX_MODULE_THREAD_STATE::~AFX_MODULE_THREAD_STATE() {
#if UCFG_COM_IMPLOBJ
	delete m_comClass;
	m_comClass = 0;
#endif
}

CThreadHandleMaps& AFX_MODULE_THREAD_STATE::GetHandleMaps() {
	if (!m_handleMaps.get())
		m_handleMaps.reset(new CThreadHandleMaps);
	return *m_handleMaps.get();
}

AFX_MAINTAIN_STATE2::AFX_MAINTAIN_STATE2(AFX_MODULE_STATE* pNewState) {
	m_pThreadState = _afxThreadState;
	m_pPrevModuleState = m_pThreadState->m_pModuleState;
	m_pThreadState->m_pModuleState = pNewState;
}

AFX_MAINTAIN_STATE2::~AFX_MAINTAIN_STATE2() {
	m_pThreadState->m_pModuleState = m_pPrevModuleState;
}

AFX_MODULE_STATE * AFXAPI AfxGetModuleState() {
	AFX_MODULE_STATE *r;
	if (!(r=_afxThreadState->m_pModuleState))
		r = &_afxBaseModuleState;
	return r;
}

AFX_MODULE_THREAD_STATE * AFXAPI AfxGetModuleThreadState() {
	return AfxGetModuleState()->m_thread.GetData();
}

void AFX_MODULE_STATE::SetHInstance(HMODULE hModule) {
	m_hCurrentInstanceHandle = m_hCurrentResourceHandle = hModule;
#if UCFG_CRASH_DUMP
	if (CCrashDump::I)
		CCrashDump::I->Modules.insert(hModule);
#endif
}

#ifdef _AFXDLL

AFX_MODULE_STATE::AFX_MODULE_STATE(bool bDLL, WNDPROC pfnAfxWndProc)
	:	m_pfnAfxWndProc(pfnAfxWndProc)
	,	m_dwVersion(_MFC_VER),
#else
AFX_MODULE_STATE::AFX_MODULE_STATE(bool bDLL)
	:	
#endif
	m_bDLL(bDLL)
	,	m_fRegisteredClasses(0)
	,	m_pfnFilterToolTipMessage(0)
{
}

AFX_MODULE_STATE::~AFX_MODULE_STATE() {
}

String AFX_MODULE_STATE::get_FileName() {
	TCHAR szModule[_MAX_PATH];
	Win32Check(::GetModuleFileName(m_hCurrentInstanceHandle, szModule, _MAX_PATH));
	return szModule;
}

_AFX_THREAD_STATE::_AFX_THREAD_STATE()
	:	m_bDlgCreate(false)
	,	m_hLockoutNotifyWindow(0)
	,	m_nLastHit(0)
	,	m_nLastStatus(0)
	,	m_bInMsgFilter(false)
	,	m_pLastInfo(0)
{
}

_AFX_THREAD_STATE::~_AFX_THREAD_STATE() {
#if UCFG_EXTENDED && UCFG_GUI
	delete m_pToolTip.get();
#endif
	delete (TOOLINFO*)m_pLastInfo;
}

bool AFXAPI AfxIsValidAddress(const void* lp, UINT nBytes, bool bReadWrite) {
	return true;
}

CWinApp * AFXAPI AfxGetApp() {
	return AfxGetModuleState()->m_pCurrentWinApp;
}

CWinApp::CWinApp(RCString lpszAppName)
	:	m_nWaitCursorCount(0)
	,	m_hcurWaitCursorRestore(0)
{
	m_appName = lpszAppName;
	_AFX_CMDTARGET_GETSTATE()->m_pCurrentWinApp = this;
}

CWinApp::~CWinApp() {
	AFX_MODULE_STATE* pModuleState = _AFX_CMDTARGET_GETSTATE();
	if (pModuleState->m_currentAppName == m_appName)
		pModuleState->m_currentAppName = "";
	if (pModuleState->m_pCurrentWinApp == this)
		pModuleState->m_pCurrentWinApp = 0;
}

void AFXAPI AfxWinInit(HINSTANCE hInstance, HINSTANCE hPrevInstance, RCString lpCmdLine, int nCmdShow) {
	AFX_MODULE_STATE* pModuleState = AfxGetModuleState();
	pModuleState->SetHInstance(hInstance);
	// fill in the initial state for the application
	if (CAppBase *pApp = AfxGetCApp()) {
		//!!!    pApp->InitCOM();
		// Windows specific initialization (not done if no CWinApp)
#if UCFG_COMPLEX_WINAPP
		pApp->m_hInstance = hInstance;
		pApp->m_cmdLine = lpCmdLine;
		pApp->m_nCmdShow = nCmdShow;
#	if UCFG_EXTENDED
		pApp->SetCurrentHandles();
#	endif
#endif
	}
}


void CSingleLock::Lock(DWORD dwTimeOut) {
	m_pObject->Lock(dwTimeOut);
	m_bAcquired = true;
}

void CSingleLock::Unlock() {
	if (m_bAcquired) {
		m_pObject->Unlock();
		m_bAcquired = false;
	}
}

BOOL CWinApp::InitInstance() {
	return TRUE;
}

//#if UCFG_GUI

DialogResult MessageBox::Show(RCString text) {
#if UCFG_COMPLEX_WINAPP && UCFG_EXTENDED
	return (DialogResult)AfxMessageBox(text);
#else
	return (DialogResult)::MessageBox(0, text, _T("Message"), MB_OK); //!!!
#endif
}

DialogResult MessageBox::Show(RCString text, RCString caption, int buttons, MessageBoxIcon icon) {
	return (DialogResult)::MessageBox(0, text, caption, buttons | int(icon));
}

//#endif // UCFG_GUI

typedef WINBASEAPI BOOL (WINAPI *PFN_Wow64DisableWow64FsRedirection)(PVOID *OldValue);
typedef WINBASEAPI BOOL (WINAPI *PFN_Wow64RevertWow64FsRedirection)(PVOID OldValue);

void Wow64FsRedirectionKeeper::Disable() {
	DlProcWrap<PFN_Wow64DisableWow64FsRedirection> pfn("KERNEL32.DLL", "Wow64DisableWow64FsRedirection");
	if (pfn) {
		if (Win32Check(pfn(&m_oldValue), ERROR_INVALID_FUNCTION))
			m_bDisabled = true;
	}
}

Wow64FsRedirectionKeeper::~Wow64FsRedirectionKeeper() {
	if (m_bDisabled) {
		DlProcWrap<PFN_Wow64RevertWow64FsRedirection> pfn("KERNEL32.DLL", "Wow64RevertWow64FsRedirection");
		if (pfn)
			Win32Check(pfn(m_oldValue));
	}
}

CVirtualMemory::CVirtualMemory(void *lpAddress, DWORD dwSize, DWORD flAllocationType, DWORD flProtect)
	:	m_address(0)
{
	Allocate(lpAddress, dwSize, flAllocationType, flProtect);
}

CVirtualMemory::~CVirtualMemory() {
	Free();
}

void CVirtualMemory::Allocate(void *lpAddress, DWORD dwSize, DWORD flAllocationType, DWORD flProtect) {
	if (m_address)
		Throw(E_EXT_NonEmptyPointer);
	Win32Check((m_address = ::VirtualAlloc(lpAddress, dwSize, flAllocationType, flProtect)) != 0);
}

void CVirtualMemory::Commit(void *lpAddress, DWORD dwSize, DWORD flProtect) {
	Win32Check(::VirtualAlloc(lpAddress, dwSize, MEM_COMMIT, flProtect) != 0);
}

void CVirtualMemory::Free() {
	if (m_address) {
		Win32Check(::VirtualFree(m_address, 0, MEM_RELEASE));
		m_address = 0;
	}
}

CHeap::CHeap() {
	Win32Check((m_h = ::HeapCreate(0, 0, 0))!=0);
}

CHeap::~CHeap() {
	try {
		Destroy();
	} catch (RCExc) {
	}
}

void CHeap::Destroy() {
	if (m_h)
		Win32Check(::HeapDestroy(exchange(m_h, (HANDLE)0)));
}

size_t CHeap::Size(void *p, DWORD flags) {
	size_t r = ::HeapSize(m_h, flags, p);
	if (r == (size_t)-1)
		Throw(E_FAIL);
	return r;
}

void *CHeap::Alloc(size_t size, DWORD flags) {
	void *r = ::HeapAlloc(m_h, flags, size);
	if (!r)
		Throw(E_OUTOFMEMORY);
	return r;
}

void CHeap::Free(void *p, DWORD flags) {
	Win32Check(::HeapFree(m_h, flags, p));
}

bool AFXAPI AfxOleCanExitApp() {
#if UCFG_COM_IMPLOBJ
	return !AfxGetModuleState()->m_comModule.GetLockCount();
#else
	return true;
#endif
}

IMPLEMENT_DYNAMIC(CCmdTarget, Object)

CCmdTarget::CCmdTarget() {
	m_pModuleState = AfxGetModuleState();
	ASSERT(m_pModuleState != NULL);
}

CCmdTarget::~CCmdTarget() {
}

BOOL CCmdTarget::InitInstance() {
	return FALSE;
}

const AFX_MSGMAP* CCmdTarget::GetThisMessageMap() {
	static const AFX_MSGMAP_ENTRY _messageEntries[] =
	{
		{ 0, 0, AfxSig_end, 0 }     // nothing here
	};
	static const AFX_MSGMAP messageMap =
	{
		NULL,
		&_messageEntries[0]
	};
	return &messageMap;	
}

const AFX_MSGMAP* CCmdTarget::GetMessageMap() const {
	return GetThisMessageMap();
}

CodepageCvt::result CodepageCvt::do_out(mbstate_t& state, const wchar_t *_First1, const wchar_t *_Last1, const wchar_t *& _Mid1, char *_First2, char *_Last2, char *& _Mid2) const {
	int n = ::WideCharToMultiByte(m_cp, 0, _First1, int(_Last1-_First1), _First2, int(_Last2-_First2), 0, 0);
	if (n > 0) {
		_Mid1 = _Last1;
		_Mid2 = _First2+n;
		return ok;
	}
	return error;
}


HRESULT CDllServer::OnRegister() {
#if UCFG_COM_IMPLOBJ
	return COleObjectFactory::UpdateRegistryAll();
#else
	return S_OK;
#endif
}

HRESULT CDllServer::OnUnregister() {
#if UCFG_COM_IMPLOBJ
	return COleObjectFactory::UpdateRegistryAll(false);
#else
	return S_OK;
#endif
}



} // Ext::


#if defined(_EXT) || !defined(_AFXDLL)
	extern "C" int _charmax = CHAR_MAX;
#endif

#if UCFG_WCE
#	undef DEFINE_GUID
#	define DEFINE_GUID(name, l, w1, w2, b1, b2, b3, b4, b5, b6, b7, b8) \
        EXTERN_C const CLSID DECLSPEC_SELECTANY name \
                = { l, w1, w2, { b1, b2,  b3,  b4,  b5,  b6,  b7,  b8 } }

#define DECLSPEC_SELECTANY  __declspec(selectany)

extern "C" {
DEFINE_GUID(GUID_NULL, 0L, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
DEFINE_OLEGUID(IID_IUnknown,            0x00000000L, 0, 0);
DEFINE_OLEGUID(IID_IClassFactory,       0x00000001L, 0, 0);
DEFINE_GUID(CLSID_XMLHTTPRequest, 0xED8C108E, 0x4349, 0x11D2, 0x91, 0xA4, 0x00, 0xC0, 0x4F, 0x79, 0x69, 0xE8);
} // "C"

#endif // UCFG_WCE

#if UCFG_EXTENDED && !UCFG_WCE

namespace Ext {

CWindowsHook::CWindowsHook()
	:	m_handle(0)
{}

CWindowsHook::~CWindowsHook() {
	Unhook();
}

void CWindowsHook::Set(int idHook, HOOKPROC lpfn, HINSTANCE hMod, DWORD dwThreadId) {
	m_handle = SetWindowsHookEx(idHook, lpfn, hMod, dwThreadId);
	Win32Check(m_handle != 0);
}

void CWindowsHook::Unhook() {
	if (m_handle)
		::UnhookWindowsHookEx(exchange(m_handle, (HHOOK)0));
	//!!!    Win32Check(::UnhookWindowsHookEx(m_handle));
}

LRESULT CWindowsHook::CallNext(int nCode, WPARAM wParam, LPARAM lParam) {
	return CallNextHookEx(m_handle, nCode, wParam, lParam);
}

} // Ext::

#endif


