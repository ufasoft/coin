/*######     Copyright (c) 1997-2013 Ufasoft  http://ufasoft.com  mailto:support@ufasoft.com,  Sergey Pavlov  mailto:dev@ufasoft.com #######################################
#                                                                                                                                                                          #
# This program is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation;  #
# either version 3, or (at your option) any later version. This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the      #
# implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details. You should have received a copy of the GNU #
# General Public License along with this program; If not, see <http://www.gnu.org/licenses/>                                                                               #
##########################################################################################################################################################################*/

#pragma once

#include <el/libext/win32/ext-com.h>

namespace Ext {

//#include "extmfc.h"

class CComObjectRootBase;
class Stream;

#ifdef _DEBUG
#	undef _ASSERTE
#	define _ASSERTE(expr)  (void) ((expr) || (Ext::ThrowImp(E_FAIL), 1))
#endif

//!!! #ifndef ASSERT //!!!
//!!! #	define ASSERT(f) ((void*)0)
//!!! #endif

template <class T> class CRefPtr {
	T *PtrAssign(T* lp);
public:
	T* p;

	typedef T _PtrClass;
	CRefPtr() {p=NULL;}
	CRefPtr(T* lp)
	{
		if ((p = lp) != NULL)
			p->ExternalAddRef();
	}
	CRefPtr(const CRefPtr<T>& lp)
	{
		if ((p = lp.p) != NULL)
			p->ExternalAddRef();
	}
	~CRefPtr() {if (p) p->ExternalRelease();}
	void Release() {if (p) p->ExternalRelease(); p=NULL;}
	operator T*() const {return (T*)p;}
	T& operator*() {ASSERT(p!=NULL); return *p; }
	//The assert on operator& usually indicates a bug.  If this is really
	//what is needed, however, take the address of the p member explicitly.
	T** operator&()
	{
		if (p)
			Throw(E_EXT_NonEmptyPointer);
		return &p;
	}
	T* operator->() const { ASSERT(p!=NULL); return p; }
	T* operator=(T* lp){return PtrAssign(lp);}
	T* operator=(const CRefPtr<T>& lp)
	{
		return PtrAssign(lp.p);
	}
	bool operator!() const {return !p;}
	operator bool() const {return p;}
	bool operator<(const CRefPtr<T>& lp) const {return p < lp.p;}
	bool operator==(const CRefPtr<T>& lp) const { return p == lp.p; }
	bool operator==(const T *q) const { return p == q; }
};

template <class T> T* CRefPtr<T>::PtrAssign(T* lp)
{
	if (lp)
		lp->ExternalAddRef();
	if (p)
		p->ExternalRelease();
	return p = lp;
}

struct _ATL_CONNMAP_ENTRY
{
	DWORD_PTR dwOffset;
};



/*!!!CComPtr<IUnknown>::CComPtr<IUnknown>(IUnknown *lp)
:CComPtrBase(lp, piid)
{}*/

/*!!!
EXT_API IUnknown *AtlComPtrAssign(IUnknown** pp, IUnknown* lp);
EXT_API IUnknown *AtlComQIPtrAssignEx(IUnknown** pp, IUnknown* lp, REFIID riid);*/

AFX_API CComPtr<IDispatch> AFXAPI AsDispatch(const VARIANT& v);



#define DECLARE_STANDARD_DISPATCH() \
	STDMETHOD(GetTypeInfoCount)(UINT *pctinfo) { \
	return CComDispatchImpl::GetTypeInfoCount(pctinfo); \
} \
	STDMETHOD(GetTypeInfo)(UINT itinfo, LCID lcid, ITypeInfo **ppTypeInfoOut) { \
	return CComDispatchImpl::GetTypeInfo(itinfo, lcid, ppTypeInfoOut); \
} \
	STDMETHOD(GetIDsOfNames)(REFIID riid, OLECHAR **rgszNames, UINT cnames, LCID lcid, DISPID *rgdispid) { \
	return CComDispatchImpl::GetIDsOfNames(riid, rgszNames, cnames, lcid, rgdispid); \
} \
	STDMETHOD(Invoke)(DISPID dispid, REFIID riid, LCID lcid, WORD wFlags, DISPPARAMS *pdispparams, VARIANT *pVarResult, EXCEPINFO *pexcepinfo, UINT *puArgErr) { \
	return CComDispatchImpl::Invoke(dispid, riid, lcid, wFlags, pdispparams, pVarResult, pexcepinfo, puArgErr); \
} 

#define DECLARE_STANDARD_CLASSFACTORY() \
	STDMETHOD(CreateInstance)(LPUNKNOWN pUnkOuter, REFIID riid, void** ppvObj) { \
	return CComClassFactoryImpl::CreateInstance(pUnkOuter, riid, ppvObj); \
} \
	STDMETHOD(LockServer)(BOOL fLock) { \
	return CComClassFactoryImpl::LockServer(fLock); \
} \
	STDMETHOD(GetLicInfo)(LPLICINFO li)\
{\
	return CComClassFactoryImpl::GetLicInfo(li);\
}\
	STDMETHOD(RequestLicKey)(DWORD dw, BSTR* key)\
{\
	return CComClassFactoryImpl::RequestLicKey(dw, key);\
}\
	STDMETHOD(CreateInstanceLic)(LPUNKNOWN pOuter, LPUNKNOWN res, REFIID riid, BSTR key,	LPVOID* ppv)\
{\
	return CComClassFactoryImpl::CreateInstanceLic(pOuter, res, riid, key, ppv);\
}\

/*#define DECLARE_STANDARD_ENUM(T) \
STDMETHOD(Next)(unsigned long celt, void * rgvar, unsigned long * pceltFetched); \
STDMETHOD(Skip)(unsigned long celt); \
STDMETHOD(Reset)(); \
STDMETHOD(Clone)(IEnum##T **ppEnumOut); \
}*/


class CTLibAttr
{
public:
	CComPtr<ITypeLib> m_iTL;
	TLIBATTR *m_ptla;

	CTLibAttr(ITypeLib *iTL);
	~CTLibAttr();
};

class AFX_CLASS COleDispatchDriver
{
public:
	IDispatch *m_lpDispatch;
	bool m_bAutoRelease;

	COleDispatchDriver(LPDISPATCH lpDispatch = 0, bool bAutoRelease = true);
	~COleDispatchDriver();
	void AttachDispatch(LPDISPATCH lpDispatch, bool bAutoRelease = true);
	void ReleaseDispatch();
	void InvokeHelperV(DISPID dwDispID, WORD wFlags, VARTYPE vtRet, void* pvRet, const BYTE* pbParamInfo, va_list argList);
	void InvokeHelper(DISPID dwDispID, WORD wFlags, VARTYPE vtRet, void* pvRet, const BYTE* pbParamInfo, ...);
	void GetProperty(DISPID dwDispID, VARTYPE vtProp, void* pvProp) const;
	void SetProperty(DISPID dwDispID, VARTYPE vtProp, ...);
};

class AFX_CLASS CDispatchDriver : public COleDispatchDriver
{
public:
	CDispatchDriver();
	CDispatchDriver(IDispatch *pdisp);
	~CDispatchDriver();
	bool HasProperty(RCString name);
	COleVariant GetProperty(RCString name);
	void SetProperty(RCString name, const VARIANT& v);
	COleVariant CallMethodEx(RCString name, const char* pbParamInfo, va_list argList);
	COleVariant CallMethod(RCString name, const char* pbParamInfo, ...);
};



#if UCFG_COM_IMPLOBJ

class CComClassFactory : public CComClassFactoryImpl,
	public IClassFactory2
{
	DECLARE_STANDARD_UNKNOWN()
	DECLARE_STANDARD_CLASSFACTORY()

public:
	CComClassFactory();
	~CComClassFactory();
	CComObjectRootBase *CreateInstance();
	void OnFinalRelease();

	_EXT_BEGIN_COM_MAP(CComClassFactory)
		_EXT_COM_INTERFACE_ENTRY(IClassFactory)
		_EXT_COM_INTERFACE_ENTRY(IClassFactory2)
		_EXT_END_COM_MAP()
};

class EXTAPI CComDispatchImpl : /*!!!virtual*/ public CComObjectRootBase {
public:
	IDispatch *GetIDispatch(bool bAddRef);

	virtual IDispatch *GetDispatch() =0;
	__declspec(property(get=GetDispatch)) IDispatch *Dispatch;
protected:
	HRESULT GetTypeInfoCount(UINT* pctinfo);
	HRESULT GetTypeInfo(UINT itinfo, LCID lcid, ITypeInfo** pptinfo);
	HRESULT GetIDsOfNames(REFIID riid, LPOLESTR* rgszNames, UINT cNames, LCID lcid, DISPID* rgdispid);
	HRESULT Invoke(DISPID dispidMember, REFIID riid, LCID lcid, WORD wFlags, DISPPARAMS* pdispparams, VARIANT* pvarResult, EXCEPINFO* pexcepinfo, UINT* puArgErr);

	virtual ITypeInfo *get_TypeInfo() =0;
	DEFPROP_VIRTUAL_GET(ITypeInfo *, TypeInfo);
};

template <class C, class I> class IDispatchImpl : public CComDispatchImpl, public I {
public:
	DECLARE_STANDARD_UNKNOWN()
	DECLARE_STANDARD_DISPATCH()

	static CComObjectRootBase *_CreateInstance() 	{
		return new C;
	}

	IDispatch *GetDispatch() {
		return this;
	}

	void QueryInterface(I **pi) {
		AddRef();
		*pi = this;
	}

	_EXT_BEGIN_COM_MAP(C)
		_EXT_COM_INTERFACE_ENTRY(IDispatch)
		_EXT_COM_INTERFACE_ENTRY(I)
	_EXT_END_COM_MAP()
protected:
	ITypeInfo *get_TypeInfo() {
		return m_pClass->GetTypeInfo(&__uuidof(I));
	}  
};

template <class T> class CEnumArray {
public:
	std::vector<T> m_ar;

	virtual ~CEnumArray() {} //!!!

	STDMETHOD_(ULONG, AddRef)() =0;
	STDMETHOD_(ULONG, Release)() =0;
};

class CComEnumImpl : public CComObjectRootBase {
};

template <class T>
class _Copy {
public:
	static void copy(T* p1, T* p2) {memcpy(p1, p2, sizeof(T));}
	static void init(T*) {}
	static void destroy(T*) {}
};

template<>
class _Copy<VARIANT>
{
public:
	static void copy(VARIANT* p1, VARIANT* p2) {OleCheck(VariantCopy(p1, p2));}
	static void init(VARIANT* p) {p->vt = VT_EMPTY;}
	static void destroy(VARIANT* p) {VariantClear(p);}
};

template<>
class _Copy<CONNECTDATA>
{
public:
	static void copy(CONNECTDATA* p1, CONNECTDATA* p2)
	{
		*p1 = *p2;
		if (p1->pUnk)
			p1->pUnk->AddRef();
	}
	static void init(CONNECTDATA* ) {}
	static void destroy(CONNECTDATA* p) {if (p->pUnk) p->pUnk->Release();}
};

template <class T>
class _CopyInterface
{
public:
	static void copy(T** p1, T** p2)
	{
		*p1 = *p2;
		if (*p1)
			(*p1)->AddRef();
	}
	static void init(T** ) {}
	static void destroy(T** p) {if (*p) (*p)->Release();}
};

template <class I, class T, class C = _Copy<T> > class IComEnumImpl : public CComEnumImpl, public I
{
public:
	int m_i;
	CEnumArray<T> *m_ar;

	IComEnumImpl(const IComEnumImpl& impl)
		:m_i(impl.m_i),
		m_ar(impl.m_ar)     
	{
	}

	IComEnumImpl(CEnumArray<T> *ar)
		:m_i(0),
		m_ar(ar)
	{
		m_ar->AddRef();
	}

	~IComEnumImpl()
	{
		m_ar->Release();
	}

	DECLARE_STANDARD_UNKNOWN()

	_EXT_BEGIN_COM_MAP(IComEnumImpl)
		_EXT_COM_INTERFACE_ENTRY(I)
		_EXT_END_COM_MAP()

		STDMETHOD(Next)(ULONG celt, T* rgelt, ULONG* pceltFetched)
		METHOD_BEGIN {
		size_t nRem = m_ar->m_ar.size()-m_i;
	HRESULT hr = nRem<celt ? S_FALSE : S_OK;
	ULONG nMin = min((size_t)celt, nRem);
	if (pceltFetched)
		*pceltFetched = nMin;
	while (nMin--)
	{
		C::copy(rgelt, &m_ar->m_ar[m_i]);
		rgelt++;
		m_i++;
	}
	return hr;
	} METHOD_END

		STDMETHOD(Skip)(ULONG celt)
		METHOD_BEGIN {
		m_i += celt;
	if (m_i > m_ar->m_ar.size())
		return S_FALSE;
	} METHOD_END

		STDMETHOD(Reset)()
		METHOD_BEGIN {
		} METHOD_END

		STDMETHOD(Clone)(I** ppEnum)
		METHOD_BEGIN {
		IComEnumImpl *p = new IComEnumImpl(_self);
	return p->InternalQueryInterface(__uuidof(I), (void**)ppEnum);
	} METHOD_END
};

// We want the offset of the connection point relative to the connection
// point container base class
#define _EXT_BEGIN_CONNECTION_POINT_MAP(x)\
	typedef x _atl_conn_classtype;\
	static const _ATL_CONNMAP_ENTRY* GetConnMap(int* pnEntries) {\
	static const _ATL_CONNMAP_ENTRY _entries[] = {
// CONNECTION_POINT_ENTRY computes the offset of the connection point to the
// IConnectionPointContainer interface
#define _EXT_CONNECTION_POINT_ENTRY(ifc){offsetofclass(_ICPLocator<ifc>, _atl_conn_classtype)-\
	offsetofclass(IConnectionPointContainerImpl<_atl_conn_classtype>, _atl_conn_classtype)},
#define _EXT_END_CONNECTION_POINT_MAP() {(DWORD)-1} }; \
	if (pnEntries) *pnEntries = sizeof(_entries)/sizeof(_ATL_CONNMAP_ENTRY) - 1; \
	return _entries;}

template <class T> class IConnectionPointContainerImpl : public IConnectionPointContainer,
public CEnumArray<IConnectionPoint*>
{
protected:  
	virtual CComClass *GetComClass() =0;
public:
	IConnectionPointContainerImpl()
	{
		//!!!m_ar.clear();
		int nCPCount;
		const _ATL_CONNMAP_ENTRY* pEntry = T::GetConnMap(&nCPCount);
		while (pEntry->dwOffset != (DWORD)-1)
		{
			m_ar.push_back((IConnectionPoint*)(PtrToInt(this)+pEntry->dwOffset));
			pEntry++;
		}
	}

	STDMETHOD(EnumConnectionPoints)(IEnumConnectionPoints **ppEnum)
		METHOD_BEGIN_EX {
		IComEnumImpl<IEnumConnectionPoints, IConnectionPoint*, _CopyInterface<IConnectionPoint> > *pEnum =
		new IComEnumImpl<IEnumConnectionPoints, IConnectionPoint*, _CopyInterface<IConnectionPoint> >(this);
	return pEnum->InternalQueryInterface(IID_IEnumConnectionPoints, (void**)ppEnum);
	} METHOD_END

		STDMETHOD(FindConnectionPoint)(REFIID riid, IConnectionPoint **ppCP)
		METHOD_BEGIN_EX {
		if (ppCP == NULL)
			return E_POINTER;
	*ppCP = NULL;
	HRESULT hRes = CONNECT_E_NOCONNECTION;
	const _ATL_CONNMAP_ENTRY* pEntry = T::GetConnMap(NULL);
	IID iid;
	while (pEntry->dwOffset != (DWORD)-1)
	{
		IConnectionPoint* pCP = (IConnectionPoint*)((DWORD_PTR)this+pEntry->dwOffset);
		if (SUCCEEDED(pCP->GetConnectionInterface(&iid)) &&
			InlineIsEqualGUID(riid, iid))
		{
			*ppCP = pCP;
			pCP->AddRef();
			hRes = S_OK;
			break;
		}
		pEntry++;
	}
	return hRes;
	} METHOD_END
};

template <class I> class _ICPLocator
{
public:
	//this method needs a different name than QueryInterface
	STDMETHOD(_LocCPQueryInterface)(REFIID riid, void ** ppvObject) = 0;
	virtual ULONG STDMETHODCALLTYPE AddRef(void) = 0;\
		virtual ULONG STDMETHODCALLTYPE Release(void) = 0;
};

template <class I> class CConnectionPointImpl : public _ICPLocator<I>, public CEnumArray<CONNECTDATA>
{
public:
	/*!!!  BEGIN_COM_MAP(CConnectionPointImpl)
	COM_INTERFACE_ENTRY(IConnectionPoint)
	END_COM_MAP()*/

	STDMETHOD(_LocCPQueryInterface)(REFIID riid, void ** ppvObject)
	{
		if (riid == IID_IConnectionPoint || riid == IID_IUnknown)
		{
			if (ppvObject == NULL)
				return E_POINTER;
			*ppvObject = this;
			((_ICPLocator<I>*)this)->AddRef();
			return S_OK;
		}
		else
			return E_NOINTERFACE;
	}
};

template <class T, class I> class IConnectionPointImpl : public CConnectionPointImpl<I>
{
public:
	~IConnectionPointImpl()
	{
		while (m_ar.size())
			OleCheck(Unadvise(m_ar[0].dwCookie));
	}

	STDMETHOD(GetConnectionInterface)(IID* piid)
	{
		*piid = __uuidof(I);
		return S_OK;
	}

	STDMETHOD(GetConnectionPointContainer)(IConnectionPointContainer** ppCPC)
	METHOD_BEGIN_EX {
			return ((T*)this)->ExternalQueryInterface(IID_IConnectionPointContainer, (void**)ppCPC);
	} METHOD_END

		STDMETHOD(Advise)(IUnknown* pUnkSink, DWORD* pdwCookie)
		METHOD_BEGIN_EX {
		CONNECTDATA cd;
	IID iid;
	OleCheck(GetConnectionInterface(&iid));
	OleCheck(pUnkSink->QueryInterface(iid, (void**)&cd.pUnk));
	*pdwCookie = cd.dwCookie = (DWORD)(DWORD_PTR)cd.pUnk;
	m_ar.push_back(cd);
	} METHOD_END

		STDMETHOD(Unadvise)(DWORD dwCookie)
		METHOD_BEGIN_EX {
		for (vector<CONNECTDATA>::iterator i(m_ar.begin()); i!=m_ar.end(); ++i)
		{
			if (i->dwCookie == dwCookie)
			{
				i->pUnk->Release();
				m_ar.erase(i);
				break;
			}
		}
		} METHOD_END

			STDMETHOD(EnumConnections)(IEnumConnections** ppEnum)
			METHOD_BEGIN_EX {
			IComEnumImpl<IEnumConnections, CONNECTDATA> *pEnum = new IComEnumImpl<IEnumConnections, CONNECTDATA>(this);
		return pEnum->InternalQueryInterface(IID_IEnumConnections, (void**)ppEnum);
		} METHOD_END
protected:  
	virtual CComClass *GetComClass() =0;
};

template <class T> class IPropertyNotifySinkCP : public IConnectionPointImpl<T, IPropertyNotifySink> {
};

template <class I> class ISupportErrorInfoImpl : public ISupportErrorInfo {
public:
	STDMETHOD(InterfaceSupportsErrorInfo)(REFIID riid) {
		return riid == __uuidof(I) ? S_OK : S_FALSE;
	}
};

#if !UCFG_WCE

class COleControlSite : public CComObjectRootBase,
	public IOleClientSite,
	public IOleInPlaceSite,
	public IOleControlSite              
{
	STDMETHOD(SaveObject)();
	STDMETHOD(GetMoniker)(DWORD, DWORD, LPMONIKER*);
	STDMETHOD(GetContainer)(LPOLECONTAINER*);
	STDMETHOD(ShowObject)();
	STDMETHOD(OnShowWindow)(BOOL);
	STDMETHOD(RequestNewObjectLayout)();

	STDMETHOD(GetWindow)(HWND*);
	STDMETHOD(ContextSensitiveHelp)(BOOL);
	STDMETHOD(CanInPlaceActivate)();
	STDMETHOD(OnInPlaceActivate)();
	STDMETHOD(OnUIActivate)();
	STDMETHOD(GetWindowContext)(LPOLEINPLACEFRAME*, LPOLEINPLACEUIWINDOW*, LPRECT, LPRECT, LPOLEINPLACEFRAMEINFO);
	STDMETHOD(Scroll)(SIZE);
	STDMETHOD(OnUIDeactivate)(BOOL);
	STDMETHOD(OnInPlaceDeactivate)();
	STDMETHOD(DiscardUndoState)();
	STDMETHOD(DeactivateAndUndo)();
	STDMETHOD(OnPosRectChange)(LPCRECT);

	STDMETHOD(OnControlInfoChanged)();
	STDMETHOD(LockInPlaceActive)(BOOL fLock);
	STDMETHOD(GetExtendedControl)(LPDISPATCH* ppDisp);
	STDMETHOD(TransformCoords)(POINTL* lpptlHimetric, POINTF* lpptfContainer, DWORD flags);
	STDMETHOD(TranslateAccelerator)(LPMSG lpMsg, DWORD grfModifiers);
	STDMETHOD(OnFocus)(BOOL fGotFocus);
	STDMETHOD(ShowPropertyFrame)();

	_EXT_BEGIN_COM_MAP(COleControlSite)
		_EXT_COM_INTERFACE_ENTRY(IOleClientSite)
		_EXT_COM_INTERFACE_ENTRY(IOleInPlaceSite)
		_EXT_COM_INTERFACE_ENTRY(IOleControlSite)
		_EXT_END_COM_MAP()
protected:
	void OnFinalRelease();
	void AttachWindow();
	void DetachWindow();
	void CreateOrLoad(REFCLSID clsid, Stream *pStream, BOOL bStorage, BSTR bstrLicKey);
	void SetExtent();
public:
	DECLARE_STANDARD_UNKNOWN()

	CWnd *m_pWndCtrl;
	COleControlContainer *m_pCtrlCont;
	COleDispatchDriver m_dispDriver;
	UINT m_nID;
	DWORD m_dwStyleMask;
	DWORD m_dwStyle;
	DWORD m_dwMiscStatus;
	Rectangle m_rect;
	HWND m_hWnd;
	CComPtr<IOleObject> m_iObject;
	CComPtr<IOleInPlaceObject> m_iInPlaceObject;

	COleControlSite(COleControlContainer *pCtrlCont);
	~COleControlSite();
	void CreateControl(CWnd* pWndCtrl, REFCLSID clsid, LPCTSTR lpszWindowName, DWORD dwStyle, const RECT& rect, UINT nID,
		Stream *pStream = 0, BOOL bStorage=FALSE, BSTR bstrLicKey=NULL);
	void CreateControl(CWnd* pWndCtrl, REFCLSID clsid, LPCTSTR lpszWindowName, DWORD dwStyle, const POINT* ppt,
		const SIZE* psize, UINT nID, Stream *pStream = 0, BOOL bStorage=FALSE, BSTR bstrLicKey=NULL);
	virtual void DoVerb(LONG nVerb, LPMSG lpMsg = NULL);
	void InvokeHelperV(DISPID dwDispID, WORD wFlags, VARTYPE vtRet, void* pvRet, const BYTE* pbParamInfo, va_list argList);
	virtual void SetPos(const CWnd* pWndInsertAfter, int x, int y, int cx, int cy, UINT nFlags);
	virtual void DestroyControl();
	virtual void ShowWindow(int nCmdShow);
};

class COleControlContainer : public CComObjectRootBase,
	public IOleInPlaceFrame,
	public IOleContainer
{
	STDMETHOD(GetWindow)(HWND*);
	STDMETHOD(ContextSensitiveHelp)(BOOL);
	STDMETHOD(GetBorder)(LPRECT);
	STDMETHOD(RequestBorderSpace)(LPCBORDERWIDTHS);
	STDMETHOD(SetBorderSpace)(LPCBORDERWIDTHS);
	STDMETHOD(SetActiveObject)(LPOLEINPLACEACTIVEOBJECT, LPCOLESTR);
	STDMETHOD(InsertMenus)(HMENU, LPOLEMENUGROUPWIDTHS);
	STDMETHOD(SetMenu)(HMENU, HOLEMENU, HWND);
	STDMETHOD(RemoveMenus)(HMENU);
	STDMETHOD(SetStatusText)(LPCOLESTR);
	STDMETHOD(EnableModeless)(BOOL);
	STDMETHOD(TranslateAccelerator)(LPMSG, WORD);

	STDMETHOD(ParseDisplayName)(LPBINDCTX, LPOLESTR, ULONG*, LPMONIKER*);
	STDMETHOD(EnumObjects)(DWORD, LPENUMUNKNOWN*);
	STDMETHOD(LockContainer)(BOOL);

	_EXT_BEGIN_COM_MAP(COleControlContainer)
		_EXT_COM_INTERFACE_ENTRY(IOleInPlaceFrame)
		_EXT_COM_INTERFACE_ENTRY(IOleContainer)
		_EXT_END_COM_MAP()
public:
	DECLARE_STANDARD_UNKNOWN()

	CWnd *m_pWnd;

	typedef std::map<HWND, ptr<COleControlSite> > CMapSite;
	CMapSite m_mapSite;

	COleControlContainer(CWnd *pWnd);
	COleControlSite *CreateControl(CWnd* pWndCtrl, REFCLSID clsid, LPCTSTR lpszWindowName, DWORD dwStyle, const RECT& rect, UINT nID,
		Stream *pStream = 0, BOOL bStorage=FALSE, BSTR bstrLicKey=NULL);
	COleControlSite *CreateControl(CWnd* pWndCtrl, REFCLSID clsid, LPCTSTR lpszWindowName, DWORD dwStyle, const POINT* ppt, const SIZE* psize,
		UINT nID, Stream *pStream = 0, BOOL bStorage=FALSE, BSTR bstrLicKey=NULL);
	COleControlSite *FindItem(UINT nID) const;
	void GetDlgItem(int nID, HWND* phWnd) const;
	void AttachControlSite(CWnd *pWnd);
	void OnFinalRelease();
};

#endif

class CGlobalAlloc {
public:
	HGLOBAL m_handle;

	CGlobalAlloc(const Blob& blob);
	~CGlobalAlloc();
	HGLOBAL Detach();
};

class CILockBytes {
public:
	CComPtr<ILockBytes> m_iLockBytes;

	CILockBytes();
	CILockBytes(const Blob& blob);
};

class CDocFile {
public:
	CILockBytes& m_lb;
	CComPtr<IStorage> m_iStorage;

	CDocFile(CILockBytes& lb, bool bCreate = false);
};

class CDefaultParam : public COleVariant {
public:
	CDefaultParam()
	{
		vt = VT_ERROR;
		scode = DISP_E_PARAMNOTFOUND;
	};
};


class AFX_CLASS AFX_MAINTAIN_STATE_COM {
protected:
	AFX_MODULE_STATE*& m_refModuleState;
	AFX_MODULE_STATE* m_pPrevModuleState;
	//!!!	_AFX_THREAD_STATE* m_pThreadState;
public:
	HRESULT HResult;
	String Description;	

	AFX_MAINTAIN_STATE_COM(CComObjectRootBase *pBase);
	AFX_MAINTAIN_STATE_COM(CComClass *pComClass);
	~AFX_MAINTAIN_STATE_COM();
	void SetFromExc(const Exc& e);
};

#endif

AFX_API String AFXAPI StringFromIID(const IID& iid);
AFX_API String AFXAPI StringFromCLSID(const CLSID& clsid);
AFX_API String AFXAPI StringFromGUID(const GUID& guid);
AFX_API CLSID AFXAPI StringToCLSID(RCString s);
AFX_API CLSID AFXAPI ProgIDToCLSID(RCString s);

AFX_API bool AFXAPI AfxOleCanExitApp();
AFX_API void AFXAPI AfxOleLockApp();
AFX_API void AFXAPI AfxOleUnlockApp();
AFX_API void AFXAPI AfxOleOnReleaseAllObjects();

AFX_API void AFXAPI ConvertToImmediate(COleVariant& v);
AFX_API CTypeOfIndex AFXAPI TypeOfIndex(const VARIANT& v);
AFX_API __int64 AFXAPI AsInt64(const VARIANT& v);
AFX_API WORD AFXAPI AsWord(const VARIANT& v);
AFX_API double AFXAPI AsDouble(const VARIANT& v);


AFX_API CUniType AFXAPI UniType(const COleVariant& v);


AFX_API bool AFXAPI AsOptionalBoolean(const VARIANT& v, bool r = false);
AFX_API int AFXAPI AsOptionalInteger(const VARIANT& v, int r = 0);
AFX_API String AFXAPI AsOptionalString(const VARIANT& v, String s = "");
AFX_API CStringVector AFXAPI AsStringArray(const VARIANT& v);
AFX_API CStringVector AFXAPI AsOptionalStringArray(const VARIANT& v);
AFX_API DATE AFXAPI AsDate(const VARIANT& v);
AFX_API CY AFXAPI AsCurrency(const VARIANT& v);
AFX_API Blob AFXAPI AsOptionalBlob(const VARIANT& v, const Blob& blob = Blob());
//!!!AFX_EXT_API COleSafeArray AsOleSafeArray(const Blob& blob);
AFX_API void AFXAPI InitializeSecurity();



} // Ext::
