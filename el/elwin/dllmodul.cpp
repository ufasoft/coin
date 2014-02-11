/*######     Copyright (c) 1997-2013 Ufasoft  http://ufasoft.com  mailto:support@ufasoft.com,  Sergey Pavlov  mailto:dev@ufasoft.com #######################################
#                                                                                                                                                                          #
# This program is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation;  #
# either version 3, or (at your option) any later version. This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the      #
# implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details. You should have received a copy of the GNU #
# General Public License along with this program; If not, see <http://www.gnu.org/licenses/>                                                                               #
##########################################################################################################################################################################*/

#include <el/ext.h>

#include <el/libext/ext-os-api.h>
#include <el/libext/win32/extwin32.h>
//!!!R #include <el/libext/win32/extmfc.h>

extern "C" {
#ifdef _M_IX86
	int _afxForceUSRDLL;
#else
	int __afxForceUSRDLL;
#endif
}


#pragma warning(disable: 4073)
#pragma init_seg(lib)

namespace Ext {

#ifdef _AFXDLL

static LRESULT CALLBACK AfxWndProcDllStatic(HWND, UINT, WPARAM, LPARAM);

static AFX_MODULE_STATE afxModuleState(true, &AfxWndProcDllStatic);

static LRESULT CALLBACK AfxWndProcDllStatic(HWND hWnd, UINT nMsg, WPARAM wParam, LPARAM lParam) {
	AFX_MANAGE_STATE(&afxModuleState);
	MSG msg = { hWnd, nMsg, wParam, lParam };
	return AfxWndProc(msg);
}

AFX_MODULE_STATE* _AfxGetOleModuleState() {
	return &afxModuleState;
}

AFX_MODULE_STATE * AFXAPI AfxGetStaticModuleState() {
	AFX_MODULE_STATE* pModuleState = &afxModuleState;
	return pModuleState;
}

#else

AFX_MODULE_STATE* AFXAPI AfxGetStaticModuleState() {
	AFX_MODULE_STATE* pModuleState = AfxGetModuleState();
	return pModuleState;
}

#endif

} // Ext::

extern "C" BOOL WINAPI RawDllMain(HINSTANCE hInstance, DWORD dwReason, LPVOID) {

#if UCFG_OS_IMPTLS
	if (dwReason == DLL_PROCESS_ATTACH) {
		BOOL r = Ext::InitTls(hInstance);
		if (!r)
			return r;
	}
#endif


#ifdef _AFXDLL
	switch (dwReason)
	{
	case DLL_PROCESS_ATTACH:
		AfxGetThreadState()->m_pPrevModuleState = AfxSetModuleState(&afxModuleState);
		break;
	case DLL_PROCESS_DETACH:
		// restore module state after cleanup
		_AFX_THREAD_STATE* pState = AfxGetThreadState();
		AfxSetModuleState(pState->m_pPrevModuleState);
#ifdef _DEBUG
		pState->m_pPrevModuleState = NULL;
#endif
	}
#endif
	return TRUE;
}

extern "C" BOOL (WINAPI* _pRawDllMain)(HINSTANCE, DWORD, LPVOID) = &RawDllMain;

extern "C" BOOL WINAPI DllMain(HINST hInstance, DWORD dwReason, LPVOID ) {		//!!!
	switch (dwReason) {
	case DLL_PROCESS_ATTACH:
		{
	#ifdef _AFXDLL
			// wire up resources from core DLL
			AfxCoreInitModule();

			AFX_MODULE_STATE *pModuleState = AfxGetStaticModuleState(); //!!!
			pModuleState->m_hCurrentInstanceHandle = (HMODULE)hInstance;
			pModuleState->m_hCurrentResourceHandle = (HMODULE)hInstance;
#endif
	//!!!		AfxGetThreadState()->m_pPrevModuleState = AfxSetModuleState(pModuleState);
			AfxWinInit((HINSTANCE)hInstance, 0, _T(""), 0);
			CWinApp *pApp = AfxGetApp();
			if (pApp)
				pApp->InitInstance();
	#ifdef _AFXDLL
			AfxSetModuleState(AfxGetThreadState()->m_pPrevModuleState);
			AfxGetThreadState()->m_pPrevModuleState = 0;
	#else
			AfxInitLocalData(hInstance);
	#endif
		}
		break;
	case DLL_PROCESS_DETACH:
		{
#	ifdef _AFXDLL
 			AfxGetThreadState()->m_pPrevModuleState = AfxSetModuleState(&afxModuleState);
#	endif
			CWinApp* pApp = AfxGetApp();
			if (pApp)
				pApp->ExitInstance();
		}
#	ifndef _AFXDLL
		AfxTermLocalData(hInstance, true);
#	endif
		break;
	case DLL_THREAD_ATTACH:
		break;
	case DLL_THREAD_DETACH:
		AFX_MANAGE_STATE(&afxModuleState);
		AfxTermThread((HINSTANCE)hInstance);
		break;
	}
	return TRUE;
}

class CFlag {
public:
	bool m_bool;

	CFlag()
		:	m_bool(true)
	{}

	~CFlag() {
		m_bool = false;
	}
};

CFlag g_flag;


CDllServer *CDllServer::I;
static CDllServer s_dllServer;

#if UCFG_COM_IMPLOBJ

STDAPI DllCanUnloadNow() {
	if (g_flag.m_bool)
	{
		AFX_MANAGE_STATE(AfxGetStaticModuleState());
		return AfxDllCanUnloadNow();
	}
	else
		return S_OK;
}

STDAPI DllGetClassObject(REFCLSID rclsid, REFIID riid, LPVOID* ppv) {
	AFX_MANAGE_STATE(AfxGetStaticModuleState());
	return AfxDllGetClassObject(rclsid, riid, ppv);
}
#endif

STDAPI DllRegisterServer() {
	AFX_MANAGE_STATE(AfxGetStaticModuleState());
	try {
		return CDllServer::I->OnRegister();
	} catch (RCExc e) {
		return e.HResult;
	}
}

STDAPI DllUnregisterServer() {
	AFX_MANAGE_STATE(AfxGetStaticModuleState());
	try {
		return CDllServer::I->OnUnregister();
	} catch (RCExc e) {
		return e.HResult;
	}
}

void __cdecl AfxTermDllState()
{
	AfxTlsRelease();
}

//!!!char _afxTermDllState = (char)(AfxTlsAddRef(), atexit(&AfxTermDllState));

