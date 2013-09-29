/*######     Copyright (c) 1997-2013 Ufasoft  http://ufasoft.com  mailto:support@ufasoft.com,  Sergey Pavlov  mailto:dev@ufasoft.com #######################################
#                                                                                                                                                                          #
# This program is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation;  #
# either version 3, or (at your option) any later version. This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the      #
# implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details. You should have received a copy of the GNU #
# General Public License along with this program; If not, see <http://www.gnu.org/licenses/>                                                                               #
##########################################################################################################################################################################*/

#include <el/ext.h>

#include <el/libext/win32/ext-win.h>

void __stdcall  _set_com_error_handler(void (__stdcall *pHandler)(HRESULT hr, IErrorInfo* perrinfo)); //!!! not in VISTA

namespace Ext {
using namespace std;



void CComPtrBase::Attach(IUnknown *unk) {
	Release();
	m_unk = unk;
}


/*!!!CComPtrBase::operator CUnknownHelper() const {
return m_unk;
}*/


/*!!!HRESULT OleCheck(HRESULT hr) {
if (FAILED(hr))
Throw(hr);
return hr;
}*/

CComPtr<IDispatch> AFXAPI AsDispatch(const VARIANT& v) {
	switch (v.vt)
	{
	case VT_EMPTY:
		break;
	case VT_UNKNOWN:
		return v.punkVal;
	case VT_DISPATCH:
		return v.pdispVal;
	default:
		Throw(E_EXT_IncorrectVariant);
	}
	return CComPtr<IDispatch>();
}

CUnkPtr AFXAPI CreateComObject(RCString assembly, const CLSID& clsid) {
	CDynamicLibrary dll(assembly);
	LoadLibrary(assembly); // AddRef to prevent unload
	typedef HRESULT (__stdcall *PFNDllGetClassObject)(REFCLSID rclsid, REFIID riid, LPVOID* ppv);
	PFNDllGetClassObject pfn = (PFNDllGetClassObject)dll.GetProcAddress(_T("DllGetClassObject"));
	CComPtr<IClassFactory> cf;
	OleCheck(pfn(clsid, IID_IClassFactory, (void**)&cf));
	CUnkPtr r;
	OleCheck(cf->CreateInstance(0, IID_IUnknown, (void**)&r));
	return r;
}

CUnkPtr AFXAPI CreateLicensedComObject(const CLSID& clsid, RCString license) {
	CComPtr<IClassFactory> cf;
	OleCheck(CoGetClassObject(clsid, CLSCTX_SERVER, 0, IID_IClassFactory, (void**)&cf));
	CUnkPtr r;
	HRESULT hr = cf->CreateInstance(0, IID_IUnknown, (void**)&r);
	if (hr == CLASS_E_NOTLICENSED) {
		CComPtr<IClassFactory2> cf2 = cf;
		OleCheck(cf2->CreateInstanceLic(0, 0, IID_IUnknown, Bstr(license), (void**)&r));
	} else
		OleCheck(hr);
	return r;
}

#if UCFG_OLE
CUnkPtr AFXAPI CreateLicensedComObject(LPCTSTR s, RCString license) {
	return CreateLicensedComObject(ProgIDToCLSID(s), license);
}
#endif


/*!!!
void AfxUpdateRegistryClass(const CLSID& clsid, LPCTSTR lpszProgID, LPCTSTR lpszVerIndProgID, UINT nDescID,
DWORD dwFlags, bool bRegister) {
if (bRegister)
AfxRegisterClassHelper(clsid, lpszProgID, lpszVerIndProgID, nDescID, dwFlags);
else
AfxUnregisterClassHelper(clsid, lpszProgID, lpszVerIndProgID);
}*/


CTLibAttr::CTLibAttr(ITypeLib *iTL)
	:	m_iTL(iTL)
{
	OleCheck(m_iTL->GetLibAttr(&m_ptla));
}

CTLibAttr::~CTLibAttr() {
	m_iTL->ReleaseTLibAttr(m_ptla);
}


#ifndef _WIN64//!!!
	static void __stdcall Ext_com_raise_error(HRESULT hr, IErrorInfo* perrinfo) {
		throw ComExc(hr, perrinfo);
	}

	static struct CComRaiserInit {
		CComRaiserInit() {
			_set_com_error_handler(&Ext_com_raise_error);
		}
	} s_comRaiserInit;

#endif

void AFXAPI InitializeSecurity() {
	static bool fInitialized = false;
	if (!fInitialized) {
		typedef HRESULT(__stdcall *C_CoInitializeSecurity)(PSECURITY_DESCRIPTOR pSecDesc,
			LONG                         cAuthSvc,
			void                  *asAuthSvc, //!!!! SOLE_...
			void                        *pReserved1,
			DWORD                        dwAuthnLevel,
			DWORD                        dwImpLevel,
			void                        *pReserved2,
			DWORD                        dwCapabilities,
			void                        *pReserved3 );
		HMODULE h = GetModuleHandle(_T("ole32.dll"));
		Win32Check(h != 0);
		C_CoInitializeSecurity coInitializeSecurity = (C_CoInitializeSecurity)GetProcAddress(h, String("CoInitializeSecurity"));
		if (!coInitializeSecurity)
			Throw(E_EXT_DCOMnotInstalled);
		HRESULT hr = coInitializeSecurity(0, -1, 0, 0, RPC_C_AUTHN_LEVEL_NONE, RPC_C_IMP_LEVEL_ANONYMOUS, 0, 0, 0);
		if (FAILED(hr) && hr != RPC_E_TOO_LATE)
			OleCheck(hr);
		fInitialized = true;
	}
}

CUnkPtr AFXAPI CreateRemoteComObject(RCString machineName, const CLSID& clsid) {
	InitializeSecurity();
	typedef HRESULT(__stdcall *C_CoCreateInstanceEx)(REFCLSID, IUnknown*, DWORD, COSERVERINFO*, DWORD, MULTI_QI*);
	HMODULE h = GetModuleHandle(_T("ole32.dll"));
	Win32Check(h != 0);
	C_CoCreateInstanceEx coCreateInstanceEx = (C_CoCreateInstanceEx)GetProcAddress(h, String("CoCreateInstanceEx"));
	if (!coCreateInstanceEx)
		Throw(E_EXT_DCOMnotInstalled);
	COSERVERINFO si; ZeroStruct(si);
	si.pwszName = (LPWSTR)(const wchar_t*)machineName;
	MULTI_QI mqi; ZeroStruct(mqi);
	mqi.pIID = &IID_IUnknown;
	OleCheck(coCreateInstanceEx(clsid, 0, (/*CLSCTX_INPROC_SERVER|*/CLSCTX_LOCAL_SERVER|CLSCTX_REMOTE_SERVER), &si, 1, &mqi));
	OleCheck(mqi.hr);
	CUnkPtr r;
	r.Attach(mqi.pItf);
	return r;
}

CUnkPtr AFXAPI CreateLicensedRemoteComObject(RCString machineName, const CLSID& clsid, RCString license) {
	InitializeSecurity();
	typedef HRESULT(__stdcall *C_CoCreateInstanceEx)(REFCLSID, IUnknown*, DWORD, COSERVERINFO*, DWORD, MULTI_QI*);
	HMODULE h = GetModuleHandle(_T("ole32.dll"));
	Win32Check(h != 0);
	C_CoCreateInstanceEx coCreateInstanceEx = (C_CoCreateInstanceEx)GetProcAddress(h, String("CoCreateInstanceEx"));
	if (!coCreateInstanceEx)
		Throw(E_EXT_DCOMnotInstalled);
	COSERVERINFO si; ZeroStruct(si);
	si.pwszName = (LPWSTR)(const wchar_t*)machineName;
	CComPtr<IClassFactory> cf;
	OleCheck(CoGetClassObject(clsid, CLSCTX_SERVER|CLSCTX_REMOTE_SERVER, &si, IID_IClassFactory, (void**)&cf));
	CUnkPtr r;
	HRESULT hr = cf->CreateInstance(0, IID_IUnknown, (void**)&r);
	if (hr == CLASS_E_NOTLICENSED) {
		CComPtr<IClassFactory2> cf2 = cf;
		OleCheck(cf2->CreateInstanceLic(0, 0, IID_IUnknown, Bstr(license), (void**)&r));
	} else
		OleCheck(hr);
	return r;
}

CUnkPtr AFXAPI AsCanonicalIUnknown(IUnknown *unk) {
	CUnkPtr r;
	OleCheck(unk->QueryInterface(IID_IUnknown, (void**)&r));
	return r;
}

bool AFXAPI IsEqual(IUnknown *unk1, IUnknown *unk2) {
	return AsCanonicalIUnknown(unk1) == AsCanonicalIUnknown(unk2);
}


#if !UCFG_WCE

CRegisterActiveObject::CRegisterActiveObject(const CLSID& clsid, IUnknown *unk, DWORD dwFlags) {
	OleCheck(::RegisterActiveObject(unk, clsid, dwFlags, &m_dw));
}

CRegisterActiveObject::~CRegisterActiveObject() {
	OleCheck(::RevokeActiveObject(m_dw, 0));
}

CRegisterClassObject::CRegisterClassObject(const CLSID& clsid, IUnknown *unk, DWORD clsctx, DWORD flags) {
	OleCheck(::CoRegisterClassObject(clsid, unk, clsctx, flags, &m_dw));
}

CRegisterClassObject::~CRegisterClassObject() {
	OleCheck(CoRevokeClassObject(m_dw));
}

#endif //!UCFG_WCE



CStringVector AFXAPI AsOptionalStringArray(const VARIANT& v) {
	vector<String> ar;
	COleVariant vv = AsImmediate(v);
	if (vv.vt == VT_ERROR)
		return ar;
	ar = AsStringArray(v);
	return ar;
}



}  // Ext::



