/*######     Copyright (c) 1997-2013 Ufasoft  http://ufasoft.com  mailto:support@ufasoft.com,  Sergey Pavlov  mailto:dev@ufasoft.com #######################################
#                                                                                                                                                                          #
# This program is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation;  #
# either version 3, or (at your option) any later version. This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the      #
# implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details. You should have received a copy of the GNU #
# General Public License along with this program; If not, see <http://www.gnu.org/licenses/>                                                                               #
##########################################################################################################################################################################*/

#include <el/ext.h>

#include <el/libext/win32/ext-win.h>

namespace Ext {
using namespace std;

CComModule::CComModule()
	:	m_nLockCount(0)
{
}

void CComModule::Init(_ATL_OBJMAP_ENTRY* objMap) {
	//!!!  for (; objMap->pclsid; objMap++)
	//    m_objList.Add(objMap);
}

void CComModule::Lock() {
	Interlocked::Increment(m_nLockCount);
}

void CComModule::Unlock() {
	Interlocked::Decrement(m_nLockCount);
}

HRESULT CComModule::ProcessError(HRESULT hr, RCString desc) {
//!!!#if !UCFG_WCE
	AFX_MODULE_STATE *pMS = AfxGetModuleState();
	CComPtr<ICreateErrorInfo> iCEI;
	OleCheck(::CreateErrorInfo(&iCEI));
	String s = desc;
	if (s.IsEmpty())
		s = AfxProcessError(hr);
	OleCheck(iCEI->SetDescription(Bstr(s)));
	CComPtr<IErrorInfo> iEI = iCEI;
	OleCheck(::SetErrorInfo(0, iEI));
//#endif
	return hr;
}

LONG CComModule::GetLockCount() {
	return m_nLockCount;
}


AFX_MAINTAIN_STATE_COM::AFX_MAINTAIN_STATE_COM(CComObjectRootBase *pBase)
	:	m_refModuleState(_afxThreadState->m_pModuleState)
	,	HResult(S_OK)
{
	m_pPrevModuleState = exchange(m_refModuleState, pBase->GetComClass()->m_pModuleState);
}

AFX_MAINTAIN_STATE_COM::AFX_MAINTAIN_STATE_COM(CComClass *pComClass)
	:	m_refModuleState(_afxThreadState->m_pModuleState)
	,	HResult(S_OK)
{
	m_pPrevModuleState = exchange(m_refModuleState, pComClass->m_pModuleState);
}

AFX_MAINTAIN_STATE_COM::~AFX_MAINTAIN_STATE_COM() {
	if (FAILED(HResult))
		HResult = CComModule::ProcessError(HResult, Description);
	m_refModuleState = m_pPrevModuleState;
}

void AFX_MAINTAIN_STATE_COM::SetFromExc(const Exc& e) {
	HResult = e.HResult;
	Description = e.Message;
}

void CComTypeLibHolder::Load() {
	OleCheck(m_libid == GUID_NULL ? ::LoadTypeLib(AfxGetModuleState()->FileName, &m_iTypeLib)
									: ::LoadRegTypeLib(m_libid, m_verMajor, m_verMinor, m_lcid, &m_iTypeLib));
}

CComClass::CComClass(CComGeneralClass *gc)
	:	m_pGeneralClass(gc)
	,	m_pModuleState(AfxGetModuleState())
{
	ZeroStruct(m_iid);
	if (gc) {
		AFX_MODULE_STATE *pMS = AfxGetModuleState();
		if (!pMS->m_typeLib.m_iTypeLib)
			pMS->m_typeLib.Load();
		AfxGetModuleThreadState()->m_classList.push_back(unique_ptr<CComClass>(this));
	}
}

ITypeInfo *CComClass::GetTypeInfo(const IID *piid) {
	if (m_iid == *piid)
		return m_iTI;
	CTypeInfoMap::iterator i = m_mapTI.find(*piid);
	if (i != m_mapTI.end())
		return i->second;
	CComPtr<ITypeInfo> iTI;
	OleCheck(AfxGetModuleState()->m_typeLib.m_iTypeLib->GetTypeInfoOfGuid(*piid, &iTI));
	m_mapTI[*piid] = iTI; //!!! SetAt
	m_iid = *piid;
	m_iTI = iTI;
	return iTI;
}


CComClass::~CComClass() {
	//!!!  if (m_pGeneralClass)
	//    AfxGetModuleThreadState()->m_classList.Remove(this);
}


CComObjectRootBase::CComObjectRootBase()
	:	m_pUnkOuter(0)
{
	AFX_MODULE_STATE *pMS = AfxGetModuleState();
	if (!pMS->m_pComClass.get())
		pMS->m_pComClass.reset(new CComClass);
	m_pClass = pMS->m_pComClass.get();
//!!!R	pMS->m_comModule.Lock();
}

CComObjectRootBase::~CComObjectRootBase() {
//!!!R	AfxGetModuleState()->m_comModule.Unlock();
}


DWORD CComObjectRootBase::InternalAddRef() {
	return Interlocked::Increment(m_dwRef);
}

DWORD CComObjectRootBase::InternalRelease() {
	LONG r = Interlocked::Decrement(m_dwRef);
	if (!r) {
		AFX_MANAGE_STATE(m_pClass->m_pModuleState);
		OnFinalRelease();
	}
	return r;
}

void CComObjectRootBase::OnFinalRelease() {
	delete this;
}

HRESULT AFXAPI CComObjectRootBase::InternalQueryInterface(void *pThis, const _ATL_INTMAP_ENTRY *pEntries, REFIID iid, LPVOID* ppvObj) {
	if (!ppvObj)
		return E_POINTER;
	*ppvObj = 0;
	if (iid == IID_IUnknown) {		// use first interface
		IUnknown* pUnk = (IUnknown*)((DWORD_PTR)pThis+pEntries->dw);
		pUnk->AddRef();
		*ppvObj = pUnk;
		return S_OK;
	}
	for (;pEntries->pFunc; pEntries++) {
		bool bBlind = !pEntries->piid;
		if (bBlind || *(pEntries->piid) == iid) {
			if (pEntries->pFunc == _EXT_SIMPLEMAPENTRY) {	//offset
				IUnknown* pUnk = (IUnknown*)((DWORD_PTR)pThis+pEntries->dw);
				pUnk->AddRef();
				*ppvObj = pUnk;
				return S_OK;
			} else {
				HRESULT hr = pEntries->pFunc(pThis, iid, ppvObj, pEntries->dw);
				if (SUCCEEDED(hr) || !bBlind)
					return hr;
				continue;
			}
		}
	}
	return E_NOINTERFACE;
}

HRESULT CComObjectRootBase::ExternalQueryInterface(const _ATL_INTMAP_ENTRY *pEntries, REFIID iid, LPVOID *ppvObj) {
	return InternalQueryInterface(this, pEntries, iid, ppvObj);
}

DWORD CComObjectRootBase::ExternalAddRef() {
	if (m_pUnkOuter)
		return m_pUnkOuter->AddRef();
	return InternalAddRef();
}

DWORD CComObjectRootBase::ExternalRelease() {
	if (m_pUnkOuter)
		return m_pUnkOuter->Release();
	return InternalRelease();
}

HRESULT CComObjectRootBase::ExternalQueryInterface(REFIID iid, LPVOID *ppvObj) {
	if (m_pUnkOuter)
		return m_pUnkOuter->QueryInterface(iid, ppvObj);
	return InternalQueryInterface(iid, ppvObj);
}

ITypeInfo *CComObjectRootBase::GetTypeInfo(const IID *piid) {
	return m_pClass->GetTypeInfo(piid);
}


STDMETHODIMP CIStreamWrap::Read(void *pv, ULONG cb, ULONG *pcbRead)
METHOD_BEGIN {
	size_t len = (size_t)std::min(DWORDLONG(cb), m_stm.Length-m_stm.Position);
	m_stm.ReadBuffer(pv, len);
	if (pcbRead)
		*pcbRead = len;  
	if (cb && !len)
		return S_FALSE;
} METHOD_END

STDMETHODIMP CIStreamWrap::Write(const void *pv, ULONG cb, ULONG *pcbWritten)
METHOD_BEGIN {
	m_stm.WriteBuffer(pv, cb);
	if (pcbWritten)
		*pcbWritten = cb;  
} METHOD_END

STDMETHODIMP CIStreamWrap::Seek(LARGE_INTEGER dlibMove, DWORD dwOrigin, ULARGE_INTEGER *plibNewPosition)
METHOD_BEGIN {
	UInt64 newPos = m_stm.Seek(dlibMove.QuadPart, (SeekOrigin)dwOrigin);
	if (plibNewPosition)
		plibNewPosition->QuadPart = newPos;
} METHOD_END

STDMETHODIMP CIStreamWrap::SetSize(ULARGE_INTEGER libNewSize)
METHOD_BEGIN {
	return E_NOTIMPL;
} METHOD_END

STDMETHODIMP CIStreamWrap::CopyTo(IStream *pstm, ULARGE_INTEGER cb, ULARGE_INTEGER *pcbRead, ULARGE_INTEGER *pcbWritten)
METHOD_BEGIN {
	void *p = alloca(cb.LowPart);
	m_stm.ReadBuffer(p, cb.LowPart);
	CIStream(pstm).WriteBuffer(p, cb.LowPart);
	*pcbWritten = cb;
} METHOD_END

STDMETHODIMP CIStreamWrap::Commit(DWORD grfCommitFlags)
METHOD_BEGIN {
} METHOD_END

STDMETHODIMP CIStreamWrap::Revert()
METHOD_BEGIN {
} METHOD_END

STDMETHODIMP CIStreamWrap::LockRegion(ULARGE_INTEGER libOffset, ULARGE_INTEGER cb, DWORD dwLockType)
METHOD_BEGIN {
} METHOD_END

STDMETHODIMP CIStreamWrap::UnlockRegion(ULARGE_INTEGER libOffset, ULARGE_INTEGER cb, DWORD dwLockType)
METHOD_BEGIN {
} METHOD_END

STDMETHODIMP CIStreamWrap::Stat(STATSTG *pstatstg, DWORD grfStateBits)
METHOD_BEGIN {
	ZeroStruct(*pstatstg);
//!!!	if (grfStateBits & STATFLAG_NONAME)
//!!!		;
	pstatstg->type = STGTY_LOCKBYTES;
	pstatstg->cbSize.QuadPart = m_stm.Length;
} METHOD_END

STDMETHODIMP CIStreamWrap::Clone(IStream **ppstm)
METHOD_BEGIN {
} METHOD_END

HRESULT CComDispatchImpl::GetTypeInfoCount(UINT* pctinfo) {
	*pctinfo = 1;
	return S_OK;
}

HRESULT CComDispatchImpl::GetTypeInfo(UINT itinfo, LCID lcid, ITypeInfo** pptinfo)
METHOD_BEGIN {
	(*pptinfo = TypeInfo)->AddRef();
} METHOD_END

HRESULT CComDispatchImpl::GetIDsOfNames(REFIID riid, LPOLESTR* rgszNames, UINT cNames, LCID lcid, DISPID* rgdispid)
METHOD_BEGIN {
	return TypeInfo->GetIDsOfNames(rgszNames, cNames, rgdispid);
} METHOD_END

HRESULT CComDispatchImpl::Invoke(DISPID dispidMember, REFIID riid, LCID lcid, WORD wFlags, DISPPARAMS* pdispparams, VARIANT* pvarResult, EXCEPINFO* pexcepinfo, UINT* puArgErr)
	METHOD_BEGIN {
	return TypeInfo->Invoke(Dispatch, dispidMember, wFlags, pdispparams, pvarResult, pexcepinfo, puArgErr);
} METHOD_END

IDispatch *CComDispatchImpl::GetIDispatch(bool bAddRef) {
	IDispatch *disp = GetDispatch();
	if (bAddRef)
		disp->AddRef();
	return disp;
}

CComClassFactoryImpl::CComClassFactoryImpl()
	:	m_bLicenseChecked(false)
	,	m_bLicenseValid(false)
{
}

DWORD CComClassFactoryImpl::InternalAddRef() {
	if (!m_dwRef)
		AfxGetModuleState()->m_comModule.Lock();
	return CComObjectRootBase::InternalAddRef();
}

HRESULT CComClassFactoryImpl::CreateInstance(LPUNKNOWN pUnkOuter, REFIID riid, void** ppvObj) {
	return CreateInstanceLic(pUnkOuter, 0, riid, 0, ppvObj);
}

HRESULT CComClassFactoryImpl::LockServer(BOOL fLock)
METHOD_BEGIN {
		if (fLock)
			AfxGetModuleState()->m_comModule.Lock();
		else
			AfxGetModuleState()->m_comModule.Unlock();
} METHOD_END

HRESULT CComClassFactoryImpl::GetLicInfo(LPLICINFO li)
METHOD_BEGIN {
	li->fLicVerified = IsLicenseValid();
	li->fRuntimeKeyAvail = !GetLicenseKey().IsEmpty();
} METHOD_END

HRESULT CComClassFactoryImpl::RequestLicKey(DWORD dw, BSTR* key)
METHOD_BEGIN {
	*key = 0;
	if (IsLicenseValid())
		*key = GetLicenseKey().AllocSysString();
	else
		return CLASS_E_NOTLICENSED;
} METHOD_END

HRESULT CComClassFactoryImpl::CreateInstanceLic(LPUNKNOWN pOuter, LPUNKNOWN res, REFIID riid, BSTR key, LPVOID* ppv)
METHOD_BEGIN {
	*ppv = 0;
	if (key && !VerifyLicenseKey(key) || !key && !IsLicenseValid())
		return CLASS_E_NOTLICENSED;
	CComObjectRootBase *obj = CreateInstance();
	obj->m_pClass = m_pObjClass;
	if (obj->m_pUnkOuter = pOuter) {
		obj->m_pInternalUnknown.reset(new CInternalUnknown(obj));
		*ppv = obj->m_pInternalUnknown.get();
		obj->m_pInternalUnknown->AddRef();
	} else
		return obj->ExternalQueryInterface(riid, ppv);
} METHOD_END

bool CComClassFactoryImpl::IsLicenseValid() {
	if (!m_bLicenseChecked) {
		m_bLicenseValid = VerifyUserLicense();
		m_bLicenseChecked = true;
	}
	return m_bLicenseValid;
}

bool CComClassFactoryImpl::VerifyUserLicense() {
	return true;
}

bool CComClassFactoryImpl::VerifyLicenseKey(RCString key) {
	return key == GetLicenseKey();
}

String CComClassFactoryImpl::GetLicenseKey() {
	return "";
}

CComClassFactory::CComClassFactory() {
	AFX_MODULE_THREAD_STATE *pTS = AfxGetModuleThreadState();
	pTS->m_factories.push_back(unique_ptr<CComClassFactoryImpl>(this));
	AfxGetModuleState()->m_comModule.Unlock();
}

CComClassFactory::~CComClassFactory() {
	AfxGetModuleState()->m_comModule.Lock();
}

CComObjectRootBase *CComClassFactory::CreateInstance() {
	return m_pObjClass->m_pGeneralClass->m_pfnCreateInstance();
}

void CComClassFactory::OnFinalRelease() {
	AfxGetModuleState()->m_comModule.Unlock();
}

HRESULT AFXAPI AfxDllCanUnloadNow() {
	try {
		return AfxOleCanExitApp() ? S_OK : S_FALSE;
	} catch (RCExc e) {
		return e.HResult;
	}
}

void AFXAPI AfxOleLockApp() {
	AfxGetModuleState()->m_comModule.Lock();
}

void AFXAPI AfxOleUnlockApp() {
	AfxGetModuleState()->m_comModule.Unlock();
	AFX_MODULE_STATE* pModuleState = AfxGetModuleState();
	if (AfxOleCanExitApp()) {
		AfxOleOnReleaseAllObjects();
	}
}

void AFXAPI AfxOleOnReleaseAllObjects() {
}

COleObjectFactory::COleObjectFactory(REFCLSID clsid, CRuntimeClass* pRuntimeClass, BOOL bMultiInstance, LPCTSTR lpszProgID)
	:	m_bRegistered(0)
	,	m_clsid(clsid)
	,	m_pRuntimeClass(pRuntimeClass)
{
	m_pModuleState->m_factoryList.push_back(this);
}

COleObjectFactory::~COleObjectFactory() {
	AFX_MODULE_STATE::CFactories& ar = m_pModuleState->m_factoryList;
	ar.erase(std::remove(ar.begin(), ar.end(), this), ar.end());
}

bool COleObjectFactory::IsRegistered() const {
	return m_dwRegister;
}

REFCLSID COleObjectFactory::GetClassID() const {
	return m_clsid;
}

void COleObjectFactory::Register() {
	m_bRegistered = true;
}

void COleObjectFactory::Unregister() {
}

bool COleObjectFactory::IsLicenseValid() {
	if (!m_bLicenseChecked) {
		m_bLicenseValid = (BYTE)VerifyUserLicense();
		m_bLicenseChecked = TRUE;
	}
	return m_bLicenseValid;
}

CCmdTarget *COleObjectFactory::OnCreateObject() {
	return (CCmdTarget*)m_pRuntimeClass->CreateObject();
}

BOOL COleObjectFactory::UpdateRegistry(BOOL bRegister) {
	return true;
}

BOOL COleObjectFactory::VerifyUserLicense() {
	return true;
}

BOOL COleObjectFactory::GetLicenseKey(DWORD dwReserved, BSTR *pbstrKey) {
	return true;
}

BOOL COleObjectFactory::VerifyLicenseKey(BSTR bstrKey) {
	return true;
}

CComGeneralClass::CComGeneralClass(const CLSID& clsid, EXT_ATL_CREATORFUNC *pfn, int descID, RCString progID, RCString indProgID, DWORD flags)
	:	m_clsid(clsid)
	,	m_pfnCreateInstance(pfn)
	,	m_descID(descID)
	,	m_flags(flags)
	,	m_progID(progID)
{
	if (indProgID != nullptr)
		m_indProgID = indProgID;
	else {
		for (size_t i=m_progID.Length; i--;) {
			if (!isdigit(m_progID[i])) {
				if (i == m_progID.Length-1)
					m_indProgID = m_progID;
				else
					m_indProgID = m_progID.Left(i);
				break;
			}
		}
	}
#if UCFG_EXTENDED
	AfxGetModuleState()->m_comModule.m_classList.push_back(this);
#endif
}

CComGeneralClass::~CComGeneralClass() {
#if UCFG_EXTENDED
	Remove(AfxGetModuleState()->m_comModule.m_classList, this);
#endif
}

void CComGeneralClass::Register() {
#if UCFG_EXTENDED
	String moduleName = AfxGetModuleState()->FileName;
	String sClsid = StringFromCLSID(m_clsid);
	String sDesc;
	sDesc.Load(m_descID);
	if (!m_progID.IsEmpty()) {
		RegistryKey key(HKEY_CLASSES_ROOT, m_progID);
		key.SetValue("", sDesc);
		RegistryKey ck(key, "CLSID");
		ck.SetValue("", sClsid);
	}
	if (m_progID != m_indProgID) {
		RegistryKey key(HKEY_CLASSES_ROOT, m_indProgID);
		key.SetValue("", sDesc);
		RegistryKey ck(key, "CLSID");
		ck.SetValue("", sClsid);
	}
	RegistryKey key(HKEY_CLASSES_ROOT, "CLSID\\"+sClsid);
	key.SetValue("", sDesc);
	RegistryKey(key, "ProgID").SetValue("", m_progID);
	if (m_progID != m_indProgID)
		RegistryKey(key, "VersionIndependentProgID").SetValue("", m_indProgID);
	if (AfxGetModuleState()->m_hCurrentInstanceHandle == GetModuleHandle(0)) {
		RegistryKey lsk(key, "LocalServer32");
		lsk.SetValue("", moduleName);
	} else {
		RegistryKey isk(key, "InprocServer32");
		isk.SetValue("", moduleName);
		String tm;
		if (m_flags & THREADFLAGS_BOTH)
			tm = "both";
		else if (m_flags & THREADFLAGS_APARTMENT)
			tm = "Apartment";
		if (!tm.IsEmpty())
			isk.SetValue("ThreadingModel", tm);
	}
#endif
}

void CComGeneralClass::Unregister() {
	try {
		if (!m_progID.IsEmpty())
			Registry::ClassesRoot.DeleteSubKeyTree(m_progID);
	} catch (RCExc) {
	}
	try {
		if (!m_progID.IsEmpty())
			Registry::ClassesRoot.DeleteSubKeyTree(m_indProgID);
	} catch (RCExc) {
	}
	try {
		Registry::ClassesRoot.DeleteSubKeyTree("CLSID\\"+StringFromCLSID(m_clsid));
	} catch (RCExc) {
	}
}


template <class T, const CLSID* pclsid = &CLSID_NULL>
class CComCoClass {
public:
	static const CLSID& WINAPI GetObjectCLSID() {return *pclsid;}
};

HRESULT AFXAPI AfxDllGetClassObject(REFCLSID rclsid, REFIID riid, LPVOID* ppv) {
	try {
		*ppv = 0;
		DWORD lData1 = rclsid.Data1;
		// search factories defined in the application
		AFX_MODULE_STATE *pModuleState = AfxGetModuleState();
		AFX_MODULE_THREAD_STATE *pTS = AfxGetModuleThreadState();
		/*!!!    for (int i=0; i<pModuleState->m_factoryList.size(); i++)
		{
		COleObjectFactory *pFactory = pModuleState->m_factoryList[i];
		if (pFactory->m_bRegistered != 0 && IsEqualCLSID(rclsid, pFactory->m_clsid))
		{
		// found suitable class factory -- query for correct interface
		SCODE sc = pFactory->InternalQueryInterface(&riid, ppv);
		return S_OK;
		}
		}*/
		if (pModuleState->m_comModule.m_classList.size() != pTS->m_classList.size()) {
			for (int i=0; i<pModuleState->m_comModule.m_classList.size(); i++)
				new CComClass(pModuleState->m_comModule.m_classList[i].get());
		}
		{
			AFX_MODULE_THREAD_STATE::CFactories& fl = pTS->m_factories;
			for (int i=0; i<fl.size(); i++) {
				CComClassFactoryImpl *cf = fl[i].get();
				if (cf->m_pObjClass->m_pGeneralClass->m_clsid == rclsid) {
					return cf->ExternalQueryInterface(riid, ppv);
				}
			}
		}
		AFX_MODULE_THREAD_STATE::CClassList& cl = pTS->m_classList;
		for (int j=0; j<cl.size(); j++) {
			CComClass *pClass = cl[j].get();
			if (pClass->m_pGeneralClass->m_clsid == rclsid) {
				CComClassFactoryImpl *cf = new CComClassFactory;
				cf->m_pClass = pTS->m_comClass;
				cf->m_pObjClass = pClass;
				return cf->ExternalQueryInterface(riid, ppv);
			}
		}
		Throw(CLASS_E_CLASSNOTAVAILABLE);
		return S_OK;
	} catch (RCExc e) {
		return e.HResult;
	}
}

void CComModule::RegisterServer(bool bRegTypeLib, const CLSID *pCLSID) {
	if (bRegTypeLib)
		RegisterTypeLib();
}

void CComModule::UnregisterServer(bool bUnRegTypeLib, const CLSID *pCLSID) {
	if (bUnRegTypeLib)
		UnregisterTypeLib();
}


void CComModule::UnregisterTypeLib() {
#if UCFG_WCE
	Throw(E_NOTIMPL);	//!!!TODO
#else
	AFX_MODULE_STATE *pMS = AfxGetModuleState();
	CComPtr<ITypeLib> iTL;
	if (pMS->m_typeLib.m_libid == GUID_NULL) {
		OleCheck(::LoadTypeLib(pMS->FileName, &iTL));
		CTLibAttr tla(iTL);
		try  {//!!! if already unregistered
			OleCheck(::UnRegisterTypeLib(tla.m_ptla->guid, tla.m_ptla->wMajorVerNum, tla.m_ptla->wMinorVerNum,
				tla.m_ptla->lcid, tla.m_ptla->syskind));
		} catch (RCExc) {
		}
	}
#endif
}


void COleObjectFactory::UnregisterAll() {
	AFX_MODULE_STATE* pModuleState = AfxGetModuleState();
	vector<ptr<CComGeneralClass>>& cl = pModuleState->m_comModule.m_classList;
	for (int i=0; i<cl.size(); i++)
		cl[i]->Unregister();
	pModuleState->m_comModule.UnregisterTypeLib();
}

void COleObjectFactory::RegisterAll() {
	AFX_MODULE_STATE* pModuleState = AfxGetModuleState();
	pModuleState->m_comModule.RegisterTypeLib();
	vector<ptr<CComGeneralClass>>& cl = pModuleState->m_comModule.m_classList;
	for (int i=0; i<cl.size(); i++)
		cl[i]->Register();
}

HRESULT AFXAPI COleObjectFactory::UpdateRegistryAll(bool bRegister) {
	try {
		AFX_MODULE_STATE* pModuleState = AfxGetModuleState();
		if (bRegister)
			pModuleState->m_comModule.RegisterTypeLib();
		vector<ptr<CComGeneralClass>>& cl = pModuleState->m_comModule.m_classList;
		for (int i=0; i<cl.size(); i++)
			if (bRegister)
				cl[i]->Register();
			else
				cl[i]->Unregister();
		if (!bRegister)
			pModuleState->m_comModule.UnregisterTypeLib();
		return S_OK;
	} catch (RCExc e) {
		return e.HResult;
	}
}

void CComModule::RegisterTypeLib() {
	AFX_MODULE_STATE *pMS = AfxGetModuleState();
	CComPtr<ITypeLib> iTL;
	if (pMS->m_typeLib.m_libid == GUID_NULL) {
		String dirName = Path::GetDirectoryName(pMS->FileName);
		OleCheck(LoadTypeLib(pMS->FileName, &iTL));
		const OLECHAR *pFilename = pMS->FileName,
					*pDirName = dirName;
		OleCheck(::RegisterTypeLib(iTL, (OLECHAR*)pFilename, (OLECHAR*)pDirName));
	}
}

}  // Ext::



