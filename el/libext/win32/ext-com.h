/*######     Copyright (c) 1997-2013 Ufasoft  http://ufasoft.com  mailto:support@ufasoft.com,  Sergey Pavlov  mailto:dev@ufasoft.com #######################################
#                                                                                                                                                                          #
# This program is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation;  #
# either version 3, or (at your option) any later version. This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the      #
# implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details. You should have received a copy of the GNU #
# General Public License along with this program; If not, see <http://www.gnu.org/licenses/>                                                                               #
##########################################################################################################################################################################*/

#pragma once

#include <el/libext/win32/ext-win.h>

namespace Ext {

class CInternalUnknown;
class CComGeneralClass;
class COleControlContainer;
class COleControlSite;

#if UCFG_BLOB_POLYMORPHIC

class COleBlob : public Blob {
public:
	COleBlob()
		:	Blob(new COleBlobBuf)
	{}

	COleBlob(const void *buf, int len) {
		COleBlobBuf *p = new COleBlobBuf;
		p->Init(len, buf);
		m_pData = p;  
	}

	COleBlob(const VARIANT& v)
		:	Blob(new COleBlobBuf)
	{
		SetVariant(v);
	}

	void AttachBSTR(BSTR bstr);
	BSTR DetachBSTR();
};

#endif


//#include "extmfc.h"


class CComPtrBase {
public:
	CComPtrBase()
		:	m_unk(0)
	{}

	CComPtrBase(const CComPtrBase& p);
	CComPtrBase(IUnknown *unk, const IID *piid = 0);

	~CComPtrBase() {
		if (m_unk)
			m_unk->Release();
	}

	void Attach(IUnknown *unk);
	void Release();
	bool operator==(IUnknown *lp) const;
	bool operator!() const { return m_unk == 0; }
protected:
	IUnknown *m_unk;

	IUnknown **operator&();
	void Assign(IUnknown *lp, const IID *piid);

	//!!!  operator CUnknownHelper() const;
};

template <class T, const IID* piid = 
#ifdef _MSC_VER
&__uuidof(T)
#else
0 //!!!
#endif
> class /*!!!AFX_CLASS*/ CComPtr2 : public CComPtrBase {
	typedef CComPtrBase base;
public:
	typedef CComPtr2 class_type;

	CComPtr2()
	{}

	CComPtr2(const CComPtr2& p)
		:	CComPtrBase((T*)p)
	{
	}

	CComPtr2(T * lp)
		:	CComPtrBase(lp)
	{}

	CComPtr2(IUnknown *unk, const IID *piid)
		:	base(unk, piid)
	{}


/*!!!	CComPtr2(IUnknown *unk)
		:	CComPtrBase(unk, piid)
	{}*/

	/*!!!  CComPtr(CUnknownHelper uh)
	:CComPtrBase(uh.m_unk, piid)
	{}*/

	T *get_P() const { return (T*)m_unk; }
	DEFPROP_GET(T*, P);

	operator T*() const {
		return P;
	}

	T **operator&() {
		return (T**)CComPtrBase::operator&();
	}

	T *operator=(const CComPtr2& p) {
		return operator=((T*)p);
	}


	/*!!!  T *operator=(CUnknownHelper uh) {
	Assign(uh.m_unk, piid);
	return (T*)m_unk;
	}

	T *operator=(CUnkPtr up) {
	Assign((IUnknown*)up, piid);
	return (T*
	
	)m_unk;
	}*/

	void Assign(IUnknown *lp) {
		base::Assign(lp, piid);
	}	

	void Assign(IUnknown *lp, const IID* p) {
		base::Assign(lp, p);
	}	

	T *operator=(T *cp) {
		base::Assign(cp, 0);
		return (T*)m_unk;
	}

	bool operator==(T *lp) const {
		return CComPtrBase::operator==(lp);
	}

	T* operator->() const {
		return (T*)m_unk;
	}

#ifdef _MSC_VER  
	template <class Q> void QueryInterface(Q** pp) const {
		OleCheck(m_unk->QueryInterface(__uuidof(Q), (void**)pp));
	}

	template <class Q> bool TryQueryInterface(Q** pp) const {
		HRESULT hr = m_unk->QueryInterface(__uuidof(Q), (void**)pp);
		if (hr == E_NOINTERFACE)
			return false;
		OleCheck(hr);
		return true;
	}
#endif

	T *Detach() {
		return (T*)exchange(m_unk, (IUnknown*)0);
	}
};

template <class T, const IID* piid = 
#ifdef _MSC_VER
&__uuidof(T)
#else
0 //!!!
#endif
> class /*!!!AFX_CLASS*/ CComPtr : public CComPtr2<T, piid> {
public:
	CComPtr()
	{}

	CComPtr(T * lp)
		:	CComPtr2(lp)
	{}

	CComPtr(IUnknown *unk)
		:	CComPtr2(unk, piid)
	{}

	
	T *operator=(IUnknown *unk) {
		Assign(unk);
		return (T*)m_unk;
	}
};

template <>
class /*!!!AFX_CLASS*/ CComPtr<IUnknown> : public CComPtr2<IUnknown> {
public:
	CComPtr()
	{}

	CComPtr(IUnknown *unk)
		:	CComPtr2(unk, &__uuidof(IUnknown))
	{}
};

class AFX_CLASS COleCurrency {
public:
	CURRENCY m_cur;

	enum CurrencyStatus {
		valid = 0,
		invalid = 1,    // Invalid currency (overflow, div 0, etc.)
		null = 2,       // Literally has no value
	};
	CurrencyStatus m_status;

	COleCurrency();
	COleCurrency(CURRENCY cySrc);

	CurrencyStatus GetStatus() const;
	void SetStatus(CurrencyStatus status); 	
	//!!!  __declspec(property(get=GetStatus, put=SetStatus)) CurrencyStatus Status;
};


class EXTAPI COleVariant : public VARIANT {
public:
	COleVariant();
	COleVariant(const COleVariant& varSrc);
	COleVariant(const VARIANT& varSrc);
	COleVariant(LPCSTR lpszSrc, VARTYPE vtSrc = VT_BSTR);
	COleVariant(LPCWSTR lpsz);
	COleVariant(const String& strSrc);
	COleVariant(BYTE nSrc);
	explicit COleVariant(bool b);
	COleVariant(short nSrc, VARTYPE vtSrc = VT_I2);
	COleVariant(int lSrc, VARTYPE vtSrc = VT_I4);
	COleVariant(long lSrc, VARTYPE vtSrc = VT_I4);
	COleVariant(const COleCurrency& curSrc);
	COleVariant(float fltSrc);
	COleVariant(double dblSrc);
	COleVariant(DateTime timeSrc);
	COleVariant(IUnknown *unk);
	COleVariant(IDispatch *disp);

	/*!!!R														// cannot cast from IDispatch-inherited types
	template <class I>
	COleVariant(const CComPtr<I>& ifc) {
		vt = VT_UNKNOWN;
		if (punkVal = (IUnknown*)ifc)
			punkVal->AddRef();
	}

	COleVariant(const CComPtr<IDispatch>& ifc) {
		vt = VT_DISPATCH;
		if (punkVal = (IDispatch*)ifc)
			punkVal->AddRef();
	} */

	~COleVariant();
	const COleVariant& operator=(const COleVariant& v);
	const COleVariant& operator=(const VARIANT& v);
	const COleVariant& operator=(LPCTSTR lpszSrc);
	const COleVariant& operator=(const String& strSrc);
	const COleVariant& operator=(BYTE nSrc);
	const COleVariant& operator=(short nSrc);
	const COleVariant& operator=(long lSrc);

	const COleVariant& operator=(int v) { return operator=((long)v); }

	const COleVariant& operator=(const COleCurrency& curSrc);
	const COleVariant& operator=(float fltSrc);
	const COleVariant& operator=(double dblSrc);
	const COleVariant& operator=(DateTime dateSrc);
	operator LPVARIANT() { return this; }
	void Clear();
	void Attach(const VARIANT& varSrc);
	VARIANT Detach();
	bool operator==(const VARIANT& varSrc) const;
	void ChangeType(VARTYPE vartype, LPVARIANT pSrc = 0);
};

class EXTAPI COleSafeArray : public COleVariant {
public:
	DWORD m_dwElementSize;
	DWORD m_dwDims;

	COleSafeArray();
	COleSafeArray(const COleSafeArray& varSrc);
	COleSafeArray(const VARIANT& varSrc);
	__forceinline ~COleSafeArray() {}
	COleSafeArray& operator=(const VARIANT& v);
	void CreateOneDim(VARTYPE vtSrc, DWORD dwElements, const void* pvSrcData = 0, long nLBound = 0);
	DWORD GetOneDimSize();
	void ResizeOneDim(DWORD dwElements);
	void Create(VARTYPE vtSrc, DWORD dwDims, DWORD* rgElements);
	void Create(VARTYPE vtSrc, DWORD dwDims, SAFEARRAYBOUND* rgsabound);
	void AccessData(void** ppvData);
	void UnaccessData();
	void GetLBound(DWORD dwDim, long* pLBound) const;
	void GetUBound(DWORD dwDim, long* pUBound) const;
	void GetElement(long* rgIndices, void* pvData) const;
	void PutElement(long* rgIndices, void* pvData);
	void Redim(SAFEARRAYBOUND* psaboundNew);
	DWORD GetDim();
	DWORD GetElemSize();
};

class AFX_CLASS CSafeArray {
	SAFEARRAY *&m_sa;
public:
	typedef CSafeArray class_type;

	CSafeArray(SAFEARRAY *&sa)
		:	m_sa(sa)
	{}

	CSafeArray(const CSafeArray& sa)
		:	m_sa(sa.m_sa)
	{}

	virtual ~CSafeArray() {
	}

	SAFEARRAY *Copy() const {
		SAFEARRAY *result;
		OleCheck(SafeArrayCopy(m_sa, &result));
		return result;
	}

	void Destroy() {
		if (!IsNULL()) {
			OleCheck(SafeArrayDestroy(m_sa));
			m_sa = 0;
		}
	}

	UINT get_DimCount() {
		return SafeArrayGetDim(m_sa);
	}
	DEFPROP_GET(UINT, DimCount);

	void GetElement(long index, void *p) const {
		OleCheck(::SafeArrayGetElement(m_sa, &index, p));
	}

	COleVariant GetElement(long idx1, long idx2) const {
		COleVariant r;
		long idxs[2] = {idx1, idx2};
		OleCheck(SafeArrayGetElement(m_sa, idxs, &r));
		return r;
	}

	VARTYPE get_Vartype() const;
	DEFPROP_GET(VARTYPE, Vartype);

	COleVariant operator[](long idx) const;
 
	long GetLBound(unsigned dim = 1) const {
		long result;
		OleCheck(SafeArrayGetLBound(m_sa, dim, &result));
		return result;
	}

	long GetUBound(unsigned dim = 1) const {
		long result;
		OleCheck(SafeArrayGetUBound(m_sa, dim, &result));
		return result;
	}

	void PutElement(long index, void *p) {
		OleCheck(SafeArrayPutElement(m_sa, &index, p));
	}

	void Redim(int elems, int lbound = 0);

	void *AccessData() const {
		void *p;
		OleCheck(SafeArrayAccessData(m_sa, &p));
		return p;
	}

	void UnaccessData() const {
		OleCheck(SafeArrayUnaccessData(m_sa));
	}

	bool IsNULL() const {
		return m_sa == 0;
	}

	void reset(SAFEARRAY *sa) {
		m_sa = sa;
	}
};

class AFX_CLASS CSafeArrayAccessData {
	const CSafeArray& m_sa;
	void *m_p;
public:
	CSafeArrayAccessData(const CSafeArray& sa)
		:	m_sa(sa)
	{
		m_p = m_sa.AccessData();
	}

	~CSafeArrayAccessData() {
		m_sa.UnaccessData();
	}

	void *GetData() {
		return m_p;
	}
};

class AFX_CLASS COleSafeArrayAccessData {
	COleSafeArray& m_sa;
	void *m_p;
public:
	COleSafeArrayAccessData(COleSafeArray& sa);
	~COleSafeArrayAccessData();
	void *GetData();
};

class AFX_CLASS COleString {
public:
	LPOLESTR m_str;

	COleString()
		:	m_str(0)
	{}

	~COleString() {
		::CoTaskMemFree(m_str);
	}

	operator String() {
		return m_str;
	}

	LPOLESTR* operator &() {
		return &m_str;
	}
};

class AFX_CLASS CRegisterClassObject {
	DWORD m_dw;
public:
	CRegisterClassObject(const CLSID& clsid, IUnknown *unk, DWORD clsctx = CLSCTX_LOCAL_SERVER | CLSCTX_REMOTE_SERVER, DWORD flags = REGCLS_MULTI_SEPARATE);
	~CRegisterClassObject();
};

class AFX_CLASS CRegisterActiveObject {
	DWORD m_dw;
public:
	CRegisterActiveObject(const CLSID& clsid, IUnknown *unk, DWORD dwFlags = ACTIVEOBJECT_STRONG);
	~CRegisterActiveObject();
};


typedef CComPtr<IUnknown> CUnkPtr;	//!!! temporary

/*!!!R
class CUnkPtr {
	IUnknown *m_unk;
public:
	CUnkPtr();
	CUnkPtr(const CUnkPtr& p);
	CUnkPtr(IUnknown *unk);
	~CUnkPtr();
	IUnknown **operator&();
	CUnkPtr& operator=(const CUnkPtr& p);
	CUnkPtr& operator=(IUnknown *unk);
	operator LPUNKNOWN() const {return m_unk;}
	bool operator==(IUnknown *unk) const {return m_unk == unk;}
	void Attach(IUnknown *unk);

	IUnknown* operator->() const {
		return m_unk;
	}

#ifdef _MSC_VER  
	template <class Q> void QueryInterface(Q** pp) const {
		OleCheck(m_unk->QueryInterface(__uuidof(Q), (void**)pp));
	}

	template <class Q> bool TryQueryInterface(Q** pp) const {
		HRESULT hr = m_unk->QueryInterface(__uuidof(Q), (void**)pp);
		if (hr == E_NOINTERFACE)
			return false;
		OleCheck(hr);
		return true;
	}
#endif
};
*/

/*!!!
class CUnknownHelper
{
public:
IUnknown *m_unk;

CUnknownHelper(IUnknown *unk)
:m_unk(unk)
{}
};*/

#define DEFINE_COM_CONSTRUCTORS(C, I) \
	C() {}                      \
	C(const C& c) :CComPtr<I>(c) {}   \
	C(I *i) :CComPtr<I>(i) {}  \
	C(IUnknown *i) :CComPtr<I>(i) {}



template <class T> class CComMem {
public:
	CPointer<T> m_p;

	~CComMem() {
		::CoTaskMemFree(m_p);
	}

	T** operator&() {
		if (m_p)
			Throw(E_EXT_InterfaceAlreadyAssigned);
		return &m_p;
	}
};

class CVariantIterator {		//!!!
	COleSafeArray m_ar;
	int m_i;
	CComPtr<IEnumVARIANT> m_en;
public:
	CVariantIterator(const VARIANT& ar);
	bool Next(COleVariant& v);
};

const BYTE VT_ARRAY_EX = 99;

inline bool VarIsArray(const VARIANT& v) {
	return v.vt & VT_ARRAY;
}

AFX_API COleVariant AFXAPI GetElement(const VARIANT& sa, long idx1);
AFX_API COleVariant AFXAPI GetElement(const VARIANT& sa, long idx1, long idx2);
AFX_API void AFXAPI PutElement(long* rgIndices, COleSafeArray& sa, const VARIANT& v);
AFX_API bool AFXAPI ConnectedToInternet();


class AFX_CLASS CIStream : public Stream {
public:
	typedef CIStream class_type;

	CComPtr<IStream> m_stream;

	CIStream(IStream *stream = 0)
		:	m_stream(stream)
	{}

	void ReadBuffer(void *buf, size_t count) const override;
	void WriteBuffer(const void *buf, size_t count) override;
	bool Eof() const override;
	void Flush() override;
	EXT_API Int64 Seek(Int64 offset, SeekOrigin origin) const override;

	/*!!!R
	Blob Read(int size);
	void Write(const Blob& blob);
	*/
	
	void SetSize(DWORDLONG libNewSize);

	DateTime get_ModTime();
	DEFPROP_GET(DateTime, ModTime);

	operator IStream*() {
		return m_stream;
	}
};

class AFX_CLASS CIStorage {
public:
	CComPtr<IStorage> m_storage;

	CIStorage(IStorage *storage = 0)
		:	m_storage(storage)
	{}

	void CreateFile(RCString name, DWORD grfMode = STGM_READWRITE | STGM_SHARE_EXCLUSIVE);
	void OpenFile(RCString name, DWORD grfMode = STGM_READWRITE | STGM_SHARE_EXCLUSIVE);
	CIStream CreateStream(RCString name, DWORD grfMode = STGM_READWRITE | STGM_SHARE_EXCLUSIVE);
	CIStorage CreateStorage(RCString name, DWORD grfMode = STGM_READWRITE | STGM_SHARE_EXCLUSIVE);
	CIStream OpenStream(RCString name, DWORD grfMode = STGM_READWRITE | STGM_SHARE_EXCLUSIVE);
	CIStorage OpenStorage(RCString name, DWORD grfMode = STGM_READWRITE | STGM_SHARE_EXCLUSIVE);
	CIStream TryOpenStream(RCString name, DWORD grfMode = STGM_READWRITE | STGM_SHARE_EXCLUSIVE);
	void DestroyElement(RCString name);
	void RenameElement(RCString srcName, RCString dstName);
	void CopyTo(CIStorage& stg);
	void Commit();
	void Revert();

	operator IStorage*() {
		return m_storage;
	}
};


class CBstrReadStream : public CMemReadStream {
public:
	CBstrReadStream(BSTR bstr)
		:	CMemReadStream(ConstBuf(bstr, SysStringByteLen(bstr)))
	{}
};

#if UCFG_COM_IMPLOBJ
class CComClass {
public:
	//!!!  CComTypeLibHolder *m_pTL;
	CComGeneralClass *m_pGeneralClass;
	AFX_MODULE_STATE *m_pModuleState;

	typedef std::unordered_map<Guid, CComPtr<ITypeInfo>> CTypeInfoMap;
	CTypeInfoMap m_mapTI;
	IID m_iid;
	CComPtr<ITypeInfo> m_iTI;

	CComClass(CComGeneralClass *gc = 0);
	~CComClass();
	ITypeInfo *GetTypeInfo(const IID *piid);
};

class CComObjectRootBase : public Object {
	typedef CComObjectRootBase class_type;
	typedef Object base;
public:
	CComClass *m_pClass;
	IUnknown *m_pUnkOuter;
//!!!	long m_dwRef;
	std::unique_ptr<CInternalUnknown> m_pInternalUnknown;

	CComObjectRootBase();
	virtual ~CComObjectRootBase();
	virtual DWORD InternalAddRef();
	virtual DWORD InternalRelease();
	virtual HRESULT InternalQueryInterface(REFIID iid, LPVOID *ppvObj) =0;

	static HRESULT AFXAPI InternalQueryInterface(void *pThis, const _ATL_INTMAP_ENTRY* pEntries, REFIID iid, LPVOID* ppvObj);
	static HRESULT AFXAPI _Chain(void* pv, REFIID iid, void** ppvObject, DWORD_PTR dw);
	virtual void OnFinalRelease();
	HRESULT ExternalQueryInterface(const _ATL_INTMAP_ENTRY *pEntries, REFIID iid, LPVOID *ppvObj);
	HRESULT InternalQueryInterface(const _ATL_INTMAP_ENTRY *pEntries, REFIID iid, LPVOID *ppvObj);
	DWORD ExternalAddRef();
	DWORD ExternalRelease();
	HRESULT ExternalQueryInterface(REFIID iid, LPVOID *ppvObj);
	ITypeInfo *GetTypeInfo(const IID *piid);
	//!!!  virtual const _ATL_INTMAP_ENTRY *_GetEntries() =0;

protected:
	CComClass *GetComClass() {
		return m_pClass;
	}

	EXT_DISABLE_COPY_CONSTRUCTOR;

	friend class AFX_MAINTAIN_STATE_COM;
};

typedef CComObjectRootBase *(AFXAPI EXT_ATL_CREATORFUNC)();

class CInternalUnknown : public IUnknown {
public:
	STDMETHOD(QueryInterface)(REFIID riid, void **ppvObjOut) { \
		if (riid == IID_IUnknown)
		{
			m_pRoot->InternalAddRef();
			*ppvObjOut = this;
			return S_OK;
		}
		return m_pRoot->InternalQueryInterface(riid, ppvObjOut); \
	} \
	STDMETHOD_(ULONG, AddRef)(void) { \
	return m_pRoot->InternalAddRef(); \
		} \
		STDMETHOD_(ULONG, Release)(void) { \
		return m_pRoot->InternalRelease(); \
		} 

		CComObjectRootBase *m_pRoot;

		CInternalUnknown(CComObjectRootBase *pRoot)
			:m_pRoot(pRoot)
		{}
};
#endif // UCFG_COM_IMPLOBJ

struct _ATL_CHAINDATA {
	DWORD_PTR dwOffset;
	const _ATL_INTMAP_ENTRY* (*pFunc)();
};

template <class base, class derived>
class _CComChainData {
public:
	static _ATL_CHAINDATA data;
};

template <class base, class derived>
_ATL_CHAINDATA _CComChainData<base, derived>::data =
{offsetofclass(base, derived), base::_GetEntries};

#define _EXT_BEGIN_COM_MAP(x) \
typedef x _ComMapClass; \
static const _ATL_INTMAP_ENTRY *_GetEntries() { \
static const _ATL_INTMAP_ENTRY _entries[] = { 

#define _EXT_DECLARE_GET_CONTROLLING_UNKNOWN() public:\
virtual IUnknown* GetControllingUnknown() {return GetUnknown();}

#ifndef _ATL_NO_UUIDOF
#define _ATL_IIDOF(x) __uuidof(x)
#else
#define _ATL_IIDOF(x) IID_##x
#endif

#define COM_INTERFACE_ENTRY_BREAK(x)\
{&_ATL_IIDOF(x), \
NULL, \
_Break},

#define COM_INTERFACE_ENTRY_NOINTERFACE(x)\
{&_ATL_IIDOF(x), \
NULL, \
_NoInterface},

#define _EXT_COM_INTERFACE_ENTRY(x)\
{&_ATL_IIDOF(x), \
offsetofclass(x, _ComMapClass)-offsetofclass(CComObjectRootBase, _ComMapClass), \
_EXT_SIMPLEMAPENTRY},

#define _EXT_COM_INTERFACE_ENTRY_CHAIN(classname)\
{NULL, \
(DWORD_PTR)&_CComChainData<classname, CComObjectRootBase/*!!!_ComMapClass*/>::data, \
_Chain},

#define _EXT_END_COM_MAP() {NULL, 0, 0}}; return _entries;};

#define _EXT_DECLARE_REGISTRY(class, pid, vpid, nid, flags)\
static void UpdateRegistry(BOOL bRegister)\
{\
return AfxUpdateRegistryClass(GetObjectCLSID(), pid, vpid, nid, \
flags, bRegister);\
}

class AFX_CLASS CComTypeLibHolder {
public:
	Guid m_libid;
	WORD m_verMajor,
		m_verMinor;
	LCID m_lcid;
	CComPtr<ITypeLib> m_iTypeLib;

	void Load();
};


#define THREADFLAGS_APARTMENT 0x1
#define THREADFLAGS_BOTH 0x2
#define AUTPRXFLAG 0x4

#if UCFG_COM_IMPLOBJ

class CComGeneralClass : public Object {
public:
	CLSID m_clsid;
	String m_progID;
	String m_indProgID;
	int m_descID;
	DWORD m_flags;
	EXT_ATL_CREATORFUNC *m_pfnCreateInstance;

	CComGeneralClass(const CLSID& clsid, EXT_ATL_CREATORFUNC *pfn, int descID, RCString progID = nullptr, RCString indProgID = nullptr,
		DWORD flags = THREADFLAGS_APARTMENT);
	virtual ~CComGeneralClass();
	virtual void Register();
	virtual void Unregister();
};

struct _ATL_OBJMAP_ENTRY {
	const CLSID* pclsid;
	EXT_ATL_CREATORFUNC *pfnCreateInstance;
	EXT_ATL_CREATORFUNC *pfnGetClassObject;
	CUnkPtr m_iCF;
};

class AFX_CLASS CComModule {
public:
	LONG m_nLockCount;
	std::unique_ptr<CComTypeLibHolder> m_tlHolder;
	std::vector<ptr<CComGeneralClass>> m_classList;

	CComModule();
	void Init(_ATL_OBJMAP_ENTRY* objMap);
	void RegisterServer(bool bRegTypeLib = false, const CLSID *pCLSID = 0);
	void UnregisterServer(bool bUnRegTypeLib, const CLSID *pCLSID);
//#if !UCFG_WCE
	void RegisterTypeLib();
	void UnregisterTypeLib();
//#endif
	LONG GetLockCount();
	void Lock();
	void Unlock();

	static HRESULT AFXAPI ProcessError(HRESULT hr, RCString desc);
};

#define _EXT_BEGIN_OBJECT_MAP(x) static _ATL_OBJMAP_ENTRY x[] = {
#define _EXT_END_OBJECT_MAP()   {0, 0}};
#define _EXT_OBJECT_ENTRY(clsid, class) {&clsid, class::CreateInstance},

#define METHOD_BEGIN { AFX_MAINTAIN_STATE_COM _ctlState(this); try
#define METHOD_BEGIN_EX { AFX_MAINTAIN_STATE_COM _ctlState(GetComClass()); try
#define METHOD_END catch(RCExc e) { _ctlState.SetFromExc(e); } return _ctlState.HResult; }

#define DECLARE_STANDARD_UNKNOWN() \
STDMETHOD(QueryInterface)(REFIID riid, void **ppvObjOut) { \
return ExternalQueryInterface(riid, ppvObjOut); \
} \
STDMETHOD_(ULONG, AddRef)(void) { \
return ExternalAddRef(); \
} \
STDMETHOD_(ULONG, Release)(void) { \
return ExternalRelease(); \
} \
HRESULT InternalQueryInterface(REFIID iid, LPVOID *ppvObj) \
{ \
return ExternalQueryInterface(_GetEntries(), iid, ppvObj); \
}

class CComClassFactoryImpl : /*!!!virtual*/ public CComObjectRootBase {
	bool IsLicenseValid();
protected:
	virtual bool VerifyUserLicense();
	virtual bool VerifyLicenseKey(RCString key);
	virtual String GetLicenseKey();
public:
	CComClass *m_pObjClass;
	bool m_bLicenseChecked,
		m_bLicenseValid;

	CComClassFactoryImpl();
	DWORD InternalAddRef();
	HRESULT CreateInstance(LPUNKNOWN pUnkOuter, REFIID riid, void** ppvObj);
	HRESULT LockServer(BOOL fLock);
	HRESULT GetLicInfo(LPLICINFO li);
	HRESULT RequestLicKey(DWORD dw, BSTR* key);
	HRESULT CreateInstanceLic(LPUNKNOWN pOuter, LPUNKNOWN res, REFIID riid, BSTR key,	LPVOID* ppv);

	virtual CComObjectRootBase *CreateInstance() =0;
};

class EXTAPI COleObjectFactory : public CCmdTarget {
protected:
	DWORD m_dwRegister;
	CLSID m_clsid;                  // registered class ID
	CRuntimeClass* m_pRuntimeClass;
	bool m_bLicenseChecked;
	bool m_bLicenseValid;
	bool m_bRegistered;             // is currently registered w/ system

	virtual CCmdTarget *OnCreateObject();
	virtual BOOL UpdateRegistry(BOOL bRegister);
	virtual BOOL VerifyUserLicense();
	virtual BOOL GetLicenseKey(DWORD dwReserved, BSTR *pbstrKey);
	virtual BOOL VerifyLicenseKey(BSTR bstrKey);
public:
	COleObjectFactory(REFCLSID clsid, CRuntimeClass* pRuntimeClass, BOOL bMultiInstance, LPCTSTR lpszProgID);
	~COleObjectFactory();
	virtual bool IsRegistered() const;
	REFCLSID GetClassID() const;
	virtual void Register();
	void Unregister();
	bool IsLicenseValid();
	static void AFXAPI UnregisterAll();
	static void AFXAPI RegisterAll();
	static HRESULT AFXAPI UpdateRegistryAll(bool bRegister = true);

	friend AFX_API HRESULT AFXAPI AfxDllGetClassObject(REFCLSID rclsid, REFIID riid, LPVOID* ppv);
};
#endif // UCFG_COM_IMPLOBJ



struct AFX_CLASS _AFX_OCC_DIALOG_INFO {
	CPointer<DLGTEMPLATE> m_pNewTemplate;
	DLGITEMTEMPLATE **m_ppOleDlgItems;

	_AFX_OCC_DIALOG_INFO();
	~_AFX_OCC_DIALOG_INFO();
};

class AFX_CLASS COccManager {
protected:
	HWND CreateDlgControl(CWnd *pWndParent, HWND hwAfter, BOOL bDialogEx, LPDLGITEMTEMPLATE pItem, WORD nMsg, BYTE *lpData,
		DWORD cb);
	void BindControls(CWnd *pWndParent);
public:
	COccManager();
	virtual COleControlContainer *CreateContainer(CWnd *pWnd);
	virtual COleControlSite *CreateSite(COleControlContainer *pCtrlCont);
	virtual const DLGTEMPLATE *PreCreateDialog(_AFX_OCC_DIALOG_INFO* pDialogInfo, const DLGTEMPLATE* pOrigTemplate);
	virtual void CreateDlgControls(CWnd *pWndParent, const CResID& resID, _AFX_OCC_DIALOG_INFO* pOccDialogInfo);
	virtual void CreateDlgControls(CWnd *pWndParent, void *lpResource, _AFX_OCC_DIALOG_INFO* pOccDialogInfo);
	virtual DLGTEMPLATE *SplitDialogTemplate(const DLGTEMPLATE* pTemplate, DLGITEMTEMPLATE** ppOleDlgItems);
};

#if UCFG_COM_IMPLOBJ
class EXT_API CIStreamWrap : public CComObjectRootBase, public IStream {
	typedef CComObjectRootBase base;
public:
	Stream& m_stm;
	CBool m_bAutoDelete;

	CIStreamWrap(Stream& stm, bool bAutoDelete = false)
		:	m_stm(stm)
		,	m_bAutoDelete(bAutoDelete)
	{}

	DECLARE_STANDARD_UNKNOWN()

	_EXT_BEGIN_COM_MAP(CIStreamWrap)
		_EXT_COM_INTERFACE_ENTRY(IStream)
	_EXT_END_COM_MAP()

	STDMETHOD(Read)(void *pv, ULONG cb, ULONG *pcbRead);
	STDMETHOD(Write)(const void *pv, ULONG cb, ULONG *pcbWritten);
	STDMETHOD(Seek)(LARGE_INTEGER dlibMove, DWORD dwOrigin, ULARGE_INTEGER *plibNewPosition);
	STDMETHOD(SetSize)(ULARGE_INTEGER libNewSize);
	STDMETHOD(CopyTo)(IStream *pstm, ULARGE_INTEGER cb, ULARGE_INTEGER *pcbRead, ULARGE_INTEGER *pcbWritten);
	STDMETHOD(Commit)(DWORD grfCommitFlags);
	STDMETHOD(Revert)();
	STDMETHOD(LockRegion)(ULARGE_INTEGER libOffset, ULARGE_INTEGER cb, DWORD dwLockType);
	STDMETHOD(UnlockRegion)(ULARGE_INTEGER libOffset, ULARGE_INTEGER cb, DWORD dwLockType);
	STDMETHOD(Stat)(STATSTG *pstatstg, DWORD grfStateBits);
	STDMETHOD(Clone)(IStream **ppstm);
protected:
	void OnFinalRelease() {
		if (m_bAutoDelete)
			base::OnFinalRelease();
	}
};
#endif


AFX_API CUnkPtr AFXAPI AsUnknown(const VARIANT& v);
EXT_API COleSafeArray AFXAPI AsVariant(const CStringVector& ar);
AFX_API CUnkPtr AFXAPI AsOptionalUnknown(const VARIANT& v);
AFX_API CUnkPtr AFXAPI AsCanonicalIUnknown(IUnknown *unk);
AFX_API COleVariant AFXAPI AsImmediate(const VARIANT& v);
AFX_API bool AFXAPI IsEqual(IUnknown *unk1, IUnknown *unk2);
AFX_API CUnkPtr AFXAPI CreateRemoteComObject(RCString machineName, const CLSID& clsid);
AFX_API CUnkPtr AFXAPI CreateLicensedRemoteComObject(RCString machineName, const CLSID& clsid, RCString license);

CUnkPtr AFXAPI CreateComObject(const CLSID& clsid, DWORD ctx = CLSCTX_SERVER);
AFX_API CUnkPtr AFXAPI CreateComObject(RCString s, DWORD ctx = CLSCTX_SERVER);
AFX_API CUnkPtr AFXAPI CreateComObject(RCString assembly, const CLSID& clsid);
AFX_API CUnkPtr AFXAPI CreateLicensedComObject(const CLSID& clsid, RCString license);
AFX_API CUnkPtr AFXAPI CreateLicensedComObject(LPCTSTR s, RCString license);

enum CTypeOfIndex {
	TI_INTEGER, TI_STRING, TI_OTHER
};

enum CUniType {
	UT_INT, UT_CURRENCY, UT_FLOAT, UT_STRING, UT_OTHER
};

#define AFX_OLE_TRUE (-1)
#define AFX_OLE_FALSE 0

class CComBSTR {
public:
	BSTR m_str;

	CComBSTR();
	CComBSTR(LPCOLESTR pSrc);
	CComBSTR(LPCSTR pSrc);
	~CComBSTR();
	operator BSTR() const;
	BSTR *operator&();
	void Attach(BSTR src);
	BSTR Detach();
};

class ComExc : public Exc {
protected:
	_com_error m_comError;
public:
	ComExc(HRESULT hr, IErrorInfo* perrinfo = 0)
		:	Exc(hr)
		,	m_comError(hr, perrinfo)
	{}

	String get_Message() const override { return (LPCWSTR)m_comError.Description(); }
};


} // Ext::


