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

CLSID AFXAPI ProgIDToCLSID(RCString s) {
	CLSID clsid;
	OleCheck(CLSIDFromProgID(s, &clsid));
	return clsid;
}

CUnkPtr AFXAPI CreateComObject(const CLSID& clsid, DWORD ctx) {
	CUnkPtr r;
	OleCheck(CoCreateInstance(clsid, 0, ctx, IID_IUnknown, (void**)&r));
	return r; 
}

CUnkPtr AFXAPI CreateComObject(RCString s, DWORD ctx) {
	return CreateComObject(ProgIDToCLSID(s), ctx);
}

String AFXAPI StringFromIID(const IID& iid) {
	COleString str;
	OleCheck(::StringFromIID(iid, &str));
	return str;
}

String AFXAPI StringFromCLSID(const CLSID& clsid) {
	COleString str;
	OleCheck(::StringFromCLSID(clsid, &str));
	return str;
}

String AFXAPI StringFromGUID(const GUID& guid) {
	OLECHAR buf[100];
	Win32Check(StringFromGUID2(guid, buf, _countof(buf)));
	return buf;
}

CLSID AFXAPI StringToCLSID(RCString s) {
	CLSID clsid;
	OleCheck(::CLSIDFromString(Bstr(s), &clsid));
	return clsid;
}

CComPtrBase::CComPtrBase(const CComPtrBase& p)
	:	m_unk(p.m_unk)
{
	if (m_unk)
		m_unk->AddRef();
}

CComPtrBase::CComPtrBase(IUnknown *unk, const IID *piid) {
	if (unk) {
		if (piid)
			OleCheck(unk->QueryInterface(*piid, (void**)&m_unk));
		else
			(m_unk = unk)->AddRef();
	} else
		m_unk = 0;
}

bool CComPtrBase::operator==(IUnknown *lp) const {
	return m_unk == lp;
}

void CComPtrBase::Release() {
	if (m_unk)
		exchange(m_unk, (IUnknown*)0)->Release();
}

void CComPtrBase::Assign(IUnknown *lp, const IID *piid) {
	Release();
	if (lp) {
		if (piid)
			OleCheck(lp->QueryInterface(*piid, (void**)&m_unk));
		else
			(m_unk = lp)->AddRef();
	}
}

/*!!!R
CUnkPtr::CUnkPtr(IUnknown *unk)
:	m_unk(unk)
{
	if (m_unk)
		m_unk->AddRef();
}

CUnkPtr::~CUnkPtr() {
	if (m_unk)
		m_unk->Release();
}

CUnkPtr::CUnkPtr()
:	m_unk(0)
{
}

CUnkPtr::CUnkPtr(const CUnkPtr& p)
:	m_unk(p.m_unk)
{
	if (m_unk)
		m_unk->AddRef();
}

void CUnkPtr::Attach(IUnknown *unk)
{
	if (m_unk)
		m_unk->Release();
	m_unk = unk;
}

IUnknown **CUnkPtr::operator&() {
	if (m_unk)
		Throw(E_EXT_InterfaceAlreadyAssigned);
	return &m_unk;
}

CUnkPtr& CUnkPtr::operator=(const CUnkPtr& p) {
	return operator=(p.m_unk);
}

CUnkPtr& CUnkPtr::operator=(IUnknown *unk) {
	if (m_unk != unk) {
		if (m_unk)
			m_unk->Release();
		if (m_unk = unk)
			m_unk->AddRef();
	}
	return _self;
}
*/

IUnknown **CComPtrBase::operator&() {
	if (m_unk)
		Throw(E_EXT_InterfaceAlreadyAssigned);
	return &m_unk;
}


void CIStream::ReadBuffer(void *buf, size_t count) const {
	DWORD dw;
	OleCheck(m_stream->Read(buf, (ULONG)count, &dw));
	if (count != dw)
		Throw(E_EXT_EndOfStream);
}

void CIStream::WriteBuffer(const void *buf, size_t count) {
	OleCheck(m_stream->Write(buf, (UINT)count, 0));
}

bool CIStream::Eof() const {
	return Position == Length;
}

void CIStream::Flush() {
	OleCheck(m_stream->Commit(STGC_DEFAULT));
}

/*!!!R
Blob CIStream::Read(int size) {
	Blob blob;
	blob.Size = size;
	DWORD dw;
	OleCheck(m_stream->Read(blob.data(), size, &dw));
	if (size != dw)
		Throw(E_EXT_EndOfStream);
	return blob;
}

void CIStream::Write(const Blob& blob) {
	OleCheck(m_stream->Write(blob.constData(), (UINT)blob.Size, 0));
}*/

Int64 CIStream::Seek(Int64 offset, SeekOrigin origin) const {
	LARGE_INTEGER li;
	li.QuadPart = offset;
	ULARGE_INTEGER r;
	OleCheck(m_stream->Seek(li, (DWORD)origin, &r));
	return r.QuadPart;
}

void CIStream::SetSize(DWORDLONG libNewSize) {
	ULARGE_INTEGER uli;
	uli.QuadPart = libNewSize;
	OleCheck(m_stream->SetSize(uli));
}

DateTime CIStream::get_ModTime() {
	STATSTG statstg;
	OleCheck(m_stream->Stat(&statstg, STATFLAG_NONAME));
	return statstg.mtime;
}

void CIStorage::CreateFile(RCString name, DWORD grfMode) {
	if (m_storage)
		Throw(E_EXT_AlreadyOpened);
	OleCheck(StgCreateDocfile(name, grfMode, 0, &m_storage));
}

void CIStorage::OpenFile(RCString name, DWORD grfMode) {
	if (m_storage)
		Throw(E_EXT_AlreadyOpened);
	OleCheck(StgOpenStorage(name, 0, grfMode, 0, 0, &m_storage));
}

CIStream CIStorage::CreateStream(RCString name, DWORD grfMode) {
	CIStream stream;
	OleCheck(m_storage->CreateStream(name, grfMode, 0, 0, &stream.m_stream));
	return stream;
}

CIStorage CIStorage::CreateStorage(RCString name, DWORD grfMode) {
	CIStorage storage;
	OleCheck(m_storage->CreateStorage(name, grfMode, 0, 0, &storage.m_storage));
	return storage;
}

CIStream CIStorage::OpenStream(RCString name, DWORD grfMode) {
	CIStream stream;
	OleCheck(m_storage->OpenStream(name, 0, grfMode, 0, &stream.m_stream));
	return stream;
}

CIStorage CIStorage::OpenStorage(RCString name, DWORD grfMode) {
	CIStorage storage;
	OleCheck(m_storage->OpenStorage(name, 0, grfMode, 0, 0, &storage.m_storage));
	return storage;
}

CIStream CIStorage::TryOpenStream(RCString name, DWORD grfMode) {
	CIStream stream;
	HRESULT hr = m_storage->OpenStream(name, 0, grfMode, 0, &stream.m_stream);
	if (hr != STG_E_FILENOTFOUND)
		OleCheck(hr);
	return stream;
}

void CIStorage::DestroyElement(RCString name) {
	OleCheck(m_storage->DestroyElement(name));
}

void CIStorage::RenameElement(RCString srcName, RCString dstName) {
	OleCheck(m_storage->RenameElement(srcName, dstName));
}

void CIStorage::CopyTo(CIStorage& stg) {
	OleCheck(m_storage->CopyTo(0, 0, 0, stg.m_storage));
}

void CIStorage::Commit() {
	OleCheck(m_storage->Commit(STGC_DEFAULT));
}

void CIStorage::Revert() {
	OleCheck(m_storage->Revert());
}

Blob::Blob(BSTR bstr) {
	int len = SysStringByteLen(bstr);
	m_pData = new(len) CStringBlobBuf(bstr, len);
}

Blob::operator COleVariant() const {
	COleVariant v;
	if (m_pData) {
		if (!(v.bstrVal = SysAllocStringByteLen((char*)constData(), (UINT)Size)))
			Throw(E_OUTOFMEMORY);
		v.vt = VT_BSTR;
	} else
		v.vt = VT_NULL;
	return v;
}

void Blob::SetVariant(const VARIANT& v) {
	switch (v.vt) {
	case VT_ARRAY | VT_UI1:
		{
			SAFEARRAY *psa = v.parray;
			CSafeArray sa(psa);
			int len = sa.GetUBound()+1;
			Size = len;
			memcpy(data(), CSafeArrayAccessData(sa).GetData(), len);
		}
		break;
	case VT_BSTR:
		{
			int len = SysStringByteLen(v.bstrVal);
			Size = len;
			memcpy(data(), v.bstrVal, len);
		}
		break;
	default:
		Throw(E_EXT_IncorrectVariant);
	}
}

#if UCFG_BLOB_POLYMORPHIC

COleBlobBuf::COleBlobBuf()
:	m_bstr(SysAllocStringByteLen("", 0))
{
}

COleBlobBuf::~COleBlobBuf() {
	Empty();
}

CBlobBufBase *COleBlobBuf::Clone() {
	COleBlobBuf *p = new COleBlobBuf;
	p->m_bstr = SysAllocStringByteLen((const char*)m_bstr, *(DWORD*)m_bstr);
	return p;
}

void COleBlobBuf::Init(size_t len, const void *buf) {
	m_bstr = SysAllocStringByteLen((char*)buf, (UINT)len);
	if (!m_bstr)
		Throw(E_OUTOFMEMORY);
	if (!buf)
		memset(m_bstr, 0, len);
}

void COleBlobBuf::Init2(size_t len, const void *buf, size_t copyLen) {
	m_bstr = SysAllocStringByteLen(0, (UINT)len);
	if (!m_bstr)
		Throw(E_OUTOFMEMORY);
	memcpy(m_bstr, buf, copyLen);
	memset(((BYTE*)m_bstr)+copyLen, 0, len-copyLen);
}

CBlobBufBase *COleBlobBuf::SetSize(size_t size) {
	COleBlobBuf *p = new COleBlobBuf;
	p->Init2(size, m_bstr, min(size, (size_t)*((DWORD*)m_bstr-1)));
	Release();
	return p;
}

void COleBlobBuf::Empty() {
	SysFreeString(Detach());
}

void COleBlobBuf::Attach(BSTR bstr) {
	Empty();
	m_bstr = bstr;
}

BSTR COleBlobBuf::Detach() {
	return exchange(m_bstr, BSTR(0));
}

void COleBlob::AttachBSTR(BSTR bstr) {
	m_pData->Attach(bstr);
}

BSTR COleBlob::DetachBSTR() {
	Cow();
	return m_pData->Detach();
}

#endif // UCFG_BLOB_POLYMORPHIC

CTypeOfIndex AFXAPI TypeOfIndex(const VARIANT& v) {
	switch (v.vt & ~VT_BYREF) {
	case VT_I2:
	case VT_I4:
		return TI_INTEGER;
	case VT_BSTR:
		return TI_STRING;
	case VT_VARIANT:
		switch (v.pvarVal->vt & ~VT_BYREF)
		{
		case VT_I2:
		case VT_I4:
			return TI_INTEGER;
		case VT_BSTR:
			return TI_STRING;
		default:
			return TI_OTHER;
		}
	default:
		return TI_OTHER;
	};
}

void AFXAPI ConvertToImmediate(COleVariant& v) {
	OleCheck(VariantCopyInd(&v, &v));
}

WORD AFXAPI AsWord(const VARIANT& v) {
	COleVariant r;
	r.ChangeType(VT_I2, &(VARIANT&)v);
	return r.iVal;
}           

double AFXAPI AsDouble(const VARIANT& v) {
	COleVariant r;
	r.ChangeType(VT_R8, &(VARIANT&)v);
	return r.dblVal;
}

CUnkPtr AFXAPI AsUnknown(const VARIANT& v) {
	switch (v.vt) {
	case VT_EMPTY:
		return 0;
	case VT_UNKNOWN:
		return v.punkVal;
	case VT_DISPATCH:
		return v.pdispVal;
	default:
		Throw(E_EXT_IncorrectVariant);
		return 0;    
	}
}

int AFXAPI AsOptionalInteger(const VARIANT& v, int r) {
	COleVariant vv = AsImmediate(v);
	if (vv.vt == VT_ERROR)
		return r;
	else
		return Convert::ToInt32(vv);
}

String AFXAPI AsOptionalString(const VARIANT& v, String s) {
	COleVariant vv = AsImmediate(v);
	if (vv.vt == VT_ERROR)
		return s;
	else
		return Convert::ToString(vv);
}

DATE AFXAPI AsDate(const VARIANT& v) {
	COleVariant r = AsImmediate(v);
	r.ChangeType(VT_DATE, &(VARIANT&)r);
	return r.date;
}

CY AFXAPI AsCurrency(const VARIANT& v) {
	COleVariant r = AsImmediate(v);
	r.ChangeType(VT_CY, &(VARIANT&)r);
	return r.cyVal;
}

Blob AFXAPI AsOptionalBlob(const VARIANT& v, const Blob& blob) {
	COleVariant vv = AsImmediate(v);
	if (vv.vt == VT_ERROR)
		return blob;
	else
		return Blob(vv);
}

CUnkPtr AFXAPI AsOptionalUnknown(const VARIANT& v) {
	COleVariant vv = AsImmediate(v);
	if (vv.vt == VT_ERROR)
		return 0;
	else
		return vv.punkVal;
}

COleVariant AFXAPI AsImmediate(const VARIANT& v) {
	COleVariant r;
	OleCheck(VariantCopyInd(&r, (VARIANT*)&v));
	return r;
}

CUniType AFXAPI UniType(const COleVariant& v) {
	switch (v.vt)
	{
	case VT_UI1:
	case VT_I2:
	case VT_I4:
		return UT_INT;
	case VT_CY:
		return UT_CURRENCY;
	case VT_R4:
	case VT_R8:
	case VT_DATE:
		return UT_FLOAT;
	case VT_BSTR:
		return UT_STRING;
	default:
		return UT_OTHER;
	};
}


CUsingCOM::CUsingCOM(DWORD dwCoInit)
	:	m_bInitialized(false)
{
	Initialize(dwCoInit);
}

CUsingCOM::CUsingCOM(_NoInit)
	:	m_bInitialized(false)
{
}

CUsingCOM::~CUsingCOM() {
	Uninitialize();
}

void CUsingCOM::Initialize(DWORD dwCoInit) {
	if (!m_bInitialized) {
#if UCFG_WCE
		OleCheck(::CoInitializeEx(0, dwCoInit));
#else
		typedef HRESULT(__stdcall *C_CoInitializeEx)(LPVOID, DWORD);
		m_dllOle.Load("ole32.dll");
		C_CoInitializeEx coInitializeEx;
		if (GetProcAddress(coInitializeEx, m_dllOle, "CoInitializeEx"))
			OleCheck(coInitializeEx(0, dwCoInit));
		else if (dwCoInit == COINIT_APARTMENTTHREADED)
			OleCheck(::CoInitialize(0));
		else
			Throw(E_EXT_DCOMnotInstalled);
#endif
		m_bInitialized = true;
	}
}

void CUsingCOM::Uninitialize() {
	if (m_bInitialized) {
		CoFreeUnusedLibraries();
		CoUninitialize();
		m_bInitialized = false;
	}
}

COleVariant::~COleVariant() {
	Clear();
}

COleVariant AFXAPI GetElement(const VARIANT& sa, long idx1) {
	COleVariant v;
	if (BYTE(sa.vt) == VT_VARIANT)
		OleCheck(SafeArrayGetElement(sa.parray, &idx1, &v));
	else {
		OleCheck(SafeArrayGetElement(sa.parray, &idx1, &v.bVal));
		v.vt = BYTE(sa.vt);
	}
	return v;
}

COleVariant AFXAPI GetElement(const VARIANT& sa, long idx1, long idx2) {
	long idx[2] = {idx1, idx2};
	COleVariant v;
	if (BYTE(sa.vt) == VT_VARIANT)
		OleCheck(SafeArrayGetElement(sa.parray, idx, &v));
	else {
		OleCheck(SafeArrayGetElement(sa.parray, idx, &v.bVal));
		v.vt = BYTE(sa.vt);
	}
	return v;
}

COleVariant::COleVariant() {
	::VariantInit(this);
}

COleVariant::COleVariant(const COleVariant& varSrc) {
	::VariantInit(this);
	OleCheck(::VariantCopy(this, (VARIANT*)&varSrc));
}

COleVariant::COleVariant(IUnknown *unk) {
	vt = VT_UNKNOWN;
	if (punkVal = unk)
		punkVal->AddRef();
}

COleVariant::COleVariant(bool b) {
	vt = VT_BOOL;
	V_BOOL(this) = b ? AFX_OLE_TRUE : AFX_OLE_FALSE;
}

COleVariant::COleVariant(IDispatch *disp) {
	vt = VT_DISPATCH;
	if (pdispVal = disp)
		pdispVal->AddRef();
}

COleVariant::COleVariant(const String& strSrc) {
	VariantInit(this);
	operator=(strSrc);
}

const COleVariant& COleVariant::operator=(const String& strSrc) {
	Clear();
	vt = VARTYPE((bstrVal = strSrc.AllocSysString()) ? VT_BSTR : VT_NULL);
	return _self;
}

void COleVariant::Clear() {
	OleCheck(::VariantClear(this));
}

void COleVariant::ChangeType(VARTYPE vartype, LPVARIANT pSrc) {
	if (!pSrc)
		pSrc = this;
	if (pSrc != this || vartype != vt) {
		if (vartype == VT_VARIANT)
			OleCheck(::VariantCopy(this, pSrc));
		else
			OleCheck(::VariantChangeType(this, pSrc, 0, vartype));
	}
}

Int32 Convert::ToInt32(const VARIANT& v) {
	COleVariant var;
	var.ChangeType(VT_I4, &(VARIANT&)v);
	return var.lVal; 
}

Int64 Convert::ToInt64(const VARIANT& v) {
	COleVariant var;
	var.ChangeType(VT_I8, &(VARIANT&)v);
#if UCFG_WCE
	return *(UInt64*)&v.lVal;		//!!!verify
#else
	return var.llVal; 
#endif
}

double Convert::ToDouble(const VARIANT& v) {
	COleVariant r;
	r.ChangeType(VT_R8, &(VARIANT&)v);
	return r.dblVal;
}

String Convert::ToString(const VARIANT& v) {
	if (v.vt == VT_NULL)
		return nullptr;
	COleVariant r;
	r.ChangeType(VT_BSTR, &(VARIANT&)v);
	return r.bstrVal;
}

bool Convert::ToBoolean(const VARIANT& v) {
	COleVariant r;
	r.ChangeType(VT_BOOL, &(VARIANT&)v);
	return r.boolVal != FALSE;
}



CComBSTR::CComBSTR()
:	m_str(0)
{
}

CComBSTR::CComBSTR(LPCOLESTR pSrc) {
	m_str = ::SysAllocString(pSrc);
}

CComBSTR::CComBSTR(LPCSTR pSrc) {
	m_str = String(pSrc).AllocSysString();
}

CComBSTR::~CComBSTR() {
	SysFreeString(m_str);
}

CComBSTR::operator BSTR() const {
	return m_str;
}

BSTR *CComBSTR::operator&() {
	ASSERT(!m_str);
	return &m_str;
}

void CComBSTR::Attach(BSTR src) {
	ASSERT(!m_str);
	m_str = src;
}

BSTR CComBSTR::Detach() {
	return exchange(m_str, (BSTR)0);
}

COleVariant::COleVariant(const VARIANT& varSrc) {
	VariantInit(this);
	OleCheck(VariantCopy(this, (VARIANT*)&varSrc));
}

COleVariant::COleVariant(LPCSTR lpszSrc, VARTYPE vtSrc) {
	vt = VT_BSTR;
	bstrVal = 0;	
	switch (vtSrc)
	{
#ifndef _UNICODE
case VT_BSTRT: //!!! This semantic only for UNICODE DAO
#endif
case VT_BSTR:
	bstrVal = String(lpszSrc).AllocSysString();
	break;
	//!!!		if (lpszSrc)
	//!!!			bstrVal = ::SysAlloStringByteLen(lpszSrc, lstrlen(lpszSrc));
	//!!!		break;
default:
	Throw(E_FAIL);
	}
}

COleVariant::COleVariant(LPCWSTR lpsz) {
	VariantInit(this);
	operator=(String(lpsz));
}

COleVariant::COleVariant(BYTE nSrc) {
	vt = VT_UI1;
	bVal = nSrc;
}

COleVariant::COleVariant(short nSrc, VARTYPE vtSrc) {
	if (vtSrc == VT_BOOL) {
		vt = VT_BOOL;
		if (!nSrc)
			V_BOOL(this) = AFX_OLE_FALSE;
		else
			V_BOOL(this) = AFX_OLE_TRUE;
	} else {
		vt = VT_I2;
		iVal = nSrc;
	}
}

COleVariant::COleVariant(int lSrc, VARTYPE vtSrc) {
	if (vtSrc == VT_ERROR) {
		vt = VT_ERROR;
		scode = lSrc;
	} else if (vtSrc == VT_BOOL) {
		vt = VT_BOOL;
		if (!lSrc)
			V_BOOL(this) = AFX_OLE_FALSE;
		else
			V_BOOL(this) = AFX_OLE_TRUE;
	} else {
		vt = VT_I4;
		lVal = lSrc;
	}
}

COleVariant::COleVariant(long lSrc, VARTYPE vtSrc) {
	if (vtSrc == VT_ERROR) {
		vt = VT_ERROR;
		scode = lSrc;
	} else if (vtSrc == VT_BOOL) {
		vt = VT_BOOL;
		if (!lSrc)
			V_BOOL(this) = AFX_OLE_FALSE;
		else
			V_BOOL(this) = AFX_OLE_TRUE;
	} else {
		vt = VT_I4;
		lVal = lSrc;
	}
}

COleVariant::COleVariant(float fltSrc) {
	vt = VT_R4;
	fltVal = fltSrc;
}

COleVariant::COleVariant(double dblSrc) {
	vt = VT_R8;
	dblVal = dblSrc;
}

COleVariant::COleVariant(DateTime timeSrc) {
	vt = VT_DATE;
	date = timeSrc.ToOADate();
}

const COleVariant& COleVariant::operator=(const COleVariant& v) {
	return operator=((const VARIANT&)v);
}

const COleVariant& COleVariant::operator=(const VARIANT& v) {
	OleCheck(VariantCopy(this, (VARIANT*)&v));
	return _self;
}

const COleVariant& COleVariant::operator=(LPCTSTR lpszSrc) {  
	return operator=(String(lpszSrc));
}

const COleVariant& COleVariant::operator=(BYTE nSrc) {
	Clear();
	vt = VT_UI1;
	bVal = nSrc;
	return _self;
}

const COleVariant& COleVariant::operator=(short nSrc) {
	Clear();
	vt = VT_I2;
	iVal = nSrc;
	return _self;
}

const COleVariant& COleVariant::operator=(long lSrc) {
	Clear();
	vt = VT_I4;
	lVal = lSrc;
	return _self;
}

const COleVariant& COleVariant::operator=(float fltSrc) {
	Clear();
	vt = VT_R4;
	fltVal = fltSrc;
	return _self;
}

const COleVariant& COleVariant::operator=(double dblSrc) {
	Clear();
	vt = VT_R8;
	dblVal = dblSrc;
	return _self;
}

const COleVariant& COleVariant::operator=(DateTime dateSrc) {
	Clear();
	vt = VT_DATE;
	date = dateSrc.ToOADate();
	return _self;
}

static bool _AfxCompareSafeArrays(SAFEARRAY* parray1, SAFEARRAY* parray2) {
	BOOL bCompare = FALSE;

	// If one is NULL they must both be NULL to compare
	if (parray1 == NULL || parray2 == NULL) {
		return parray1 == parray2;
	}

	// Dimension must match and if 0, then arrays compare
	DWORD dwDim1 = ::SafeArrayGetDim(parray1);
	DWORD dwDim2 = ::SafeArrayGetDim(parray2);
	if (dwDim1 != dwDim2)
		return FALSE;
	else if (dwDim1 == 0)
		return TRUE;

	// Element size must match
	DWORD dwSize1 = ::SafeArrayGetElemsize(parray1);
	DWORD dwSize2 = ::SafeArrayGetElemsize(parray2);
	if (dwSize1 != dwSize2)
		return FALSE;

	long* pLBound1 = NULL;
	long* pLBound2 = NULL;
	long* pUBound1 = NULL;
	long* pUBound2 = NULL;

	void* pData1 = NULL;
	void* pData2 = NULL;

	try
	{
		// Bounds must match
		pLBound1 = (long*)alloca(dwDim1*sizeof(long));
		pLBound2 = (long*)alloca(dwDim2*sizeof(long));
		pUBound1 = (long*)alloca(dwDim1*sizeof(long));
		pUBound2 = (long*)alloca(dwDim2*sizeof(long));

		size_t nTotalElements = 1;

		// Get and compare bounds
		for (DWORD dwIndex = 0; dwIndex < dwDim1; dwIndex++)
		{
			OleCheck(::SafeArrayGetLBound(
				parray1, dwIndex+1, &pLBound1[dwIndex]));
			OleCheck(::SafeArrayGetLBound(
				parray2, dwIndex+1, &pLBound2[dwIndex]));
			OleCheck(::SafeArrayGetUBound(
				parray1, dwIndex+1, &pUBound1[dwIndex]));
			OleCheck(::SafeArrayGetUBound(
				parray2, dwIndex+1, &pUBound2[dwIndex]));

			// Check the magnitude of each bound
			if (pUBound1[dwIndex] - pLBound1[dwIndex] !=
				pUBound2[dwIndex] - pLBound2[dwIndex])
			{

				return FALSE;
			}

			// Increment the element count
			nTotalElements *= pUBound1[dwIndex] - pLBound1[dwIndex] + 1;
		}

		// Access the data
		OleCheck(::SafeArrayAccessData(parray1, &pData1));
		OleCheck(::SafeArrayAccessData(parray2, &pData2));

		// Calculate the number of bytes of data and compare
		size_t nSize = nTotalElements * dwSize1;
		int nOffset = memcmp(pData1, pData2, nSize);
		bCompare = nOffset == 0;

		// Release the array locks
		OleCheck(::SafeArrayUnaccessData(parray1));
		OleCheck(::SafeArrayUnaccessData(parray2));
	} catch (RCExc) {
		// Release the array locks
		if (pData1)
			OleCheck(::SafeArrayUnaccessData(parray1));
		if (pData2)
			OleCheck(::SafeArrayUnaccessData(parray2));
		throw;
	}
	return bCompare;
}

bool COleVariant::operator==(const VARIANT& var) const {
	if (&var == this)
		return true;
	if (var.vt != vt)
		return false;
	switch (vt)
	{
	case VT_EMPTY:
	case VT_NULL:
		return true;
	case VT_BOOL:
		return V_BOOL(&var) == V_BOOL(this);
	case VT_UI1:
		return var.bVal == bVal;
	case VT_I2:
		return var.iVal == iVal;
	case VT_I4:
		return var.lVal == lVal;
	case VT_CY:
		return (var.cyVal.Hi == cyVal.Hi && var.cyVal.Lo == cyVal.Lo);
	case VT_R4:
		return var.fltVal == fltVal;
	case VT_R8:
		return var.dblVal == dblVal;
	case VT_DATE:
		return var.date == date;
	case VT_BSTR:
		return SysStringByteLen(var.bstrVal) == SysStringByteLen(bstrVal) &&
			memcmp(var.bstrVal, bstrVal, SysStringByteLen(bstrVal)) == 0;
	case VT_ERROR:
		return var.scode == scode;
	case VT_DISPATCH:
	case VT_UNKNOWN:
		return var.punkVal == punkVal;
	default:
		if (vt & VT_ARRAY && !(vt & VT_BYREF))
			return _AfxCompareSafeArrays(var.parray, parray);
		else
			Throw(E_EXT_UnsupportedVariantType);  // VT_BYREF not supported
		// fall through
	}
	return false;
}

void COleVariant::Attach(const VARIANT& varSrc) {
	Clear();
	memcpy(this, &varSrc, sizeof(VARIANT));
}

VARIANT COleVariant::Detach() {
	VARIANT varResult = _self;
	vt = VT_EMPTY;
	return varResult;
}

COleSafeArray::COleSafeArray()
:	m_dwDims(0)
,	m_dwElementSize(0)
{
}

COleSafeArray::COleSafeArray(const COleSafeArray& varSrc)
:	COleVariant(varSrc)
{
	m_dwDims = GetDim();
	m_dwElementSize = GetElemSize();
}

COleSafeArray::COleSafeArray(const VARIANT& varSrc) {
	_self = varSrc;
	m_dwDims = GetDim();
	m_dwElementSize = GetElemSize();
}

COleSafeArray& COleSafeArray::operator=(const VARIANT& v) {
	COleVariant::operator=(v);
	return _self;
}

void COleSafeArray::CreateOneDim(VARTYPE vtSrc, DWORD dwElements, const void* pvSrcData, long nLBound) {
	SAFEARRAYBOUND rgsabound;
	rgsabound.cElements = dwElements;
	rgsabound.lLbound = nLBound;
	Create(vtSrc, 1, &rgsabound);

	// Copy over the data if neccessary
	if (pvSrcData != NULL) {
		void* pvDestData;
		AccessData(&pvDestData);
		memcpy(pvDestData, pvSrcData, GetElemSize() * dwElements);
		UnaccessData();
	}
}

DWORD COleSafeArray::GetOneDimSize() {
	long nUBound, nLBound;

	GetUBound(1, &nUBound);
	GetLBound(1, &nLBound);

	return nUBound + 1 - nLBound;
}

void COleSafeArray::ResizeOneDim(DWORD dwElements) {
	SAFEARRAYBOUND rgsabound;

	rgsabound.cElements = dwElements;
	rgsabound.lLbound = 0;

	Redim(&rgsabound);
}

void COleSafeArray::Create(VARTYPE vtSrc, DWORD dwDims, DWORD* rgElements) {
	// Allocate and fill proxy array of bounds (with lower bound of zero)
	SAFEARRAYBOUND* rgsaBounds = (SAFEARRAYBOUND*)alloca(dwDims*sizeof(SAFEARRAYBOUND));

	for (DWORD dwIndex = 0; dwIndex < dwDims; dwIndex++) {
		// Assume lower bound is 0 and fill in element count
		rgsaBounds[dwIndex].lLbound = 0;
		rgsaBounds[dwIndex].cElements = rgElements[dwIndex];
	}

	Create(vtSrc, dwDims, rgsaBounds);
}

void COleSafeArray::Create(VARTYPE vtSrc, DWORD dwDims, SAFEARRAYBOUND* rgsabound) {
	OleCheck(VariantClear(this));

	parray = ::SafeArrayCreate(vtSrc, dwDims, rgsabound);
	if (!parray)
		Throw(E_OUTOFMEMORY);
	vt = unsigned short(vtSrc | VT_ARRAY);
	m_dwDims = dwDims;
	m_dwElementSize = GetElemSize();
}

void COleSafeArray::AccessData(void** ppvData) {
	OleCheck(::SafeArrayAccessData(parray, ppvData));
}

void COleSafeArray::UnaccessData() {
	OleCheck(::SafeArrayUnaccessData(parray));
}

void COleSafeArray::GetLBound(DWORD dwDim, long* pLBound) const {
	OleCheck(::SafeArrayGetLBound(parray, dwDim, pLBound));
}

void COleSafeArray::GetUBound(DWORD dwDim, long* pUBound) const {
	OleCheck(::SafeArrayGetUBound(parray, dwDim, pUBound));
}

void COleSafeArray::GetElement(long* rgIndices, void* pvData) const {
	OleCheck(::SafeArrayGetElement(parray, rgIndices, pvData));
}

void COleSafeArray::PutElement(long* rgIndices, void* pvData) {
	OleCheck(::SafeArrayPutElement(parray, rgIndices, pvData));
}

void COleSafeArray::Redim(SAFEARRAYBOUND* psaboundNew) {
	OleCheck(::SafeArrayRedim(parray, psaboundNew));
}

DWORD COleSafeArray::GetDim() {
	return ::SafeArrayGetDim(parray);
}

DWORD COleSafeArray::GetElemSize() {
	return ::SafeArrayGetElemsize(parray);
}

void CSafeArray::Redim(int elems, int lbound) {
	SAFEARRAYBOUND sab = {elems, lbound};
	OleCheck(::SafeArrayRedim(m_sa, &sab));
}

VARTYPE CSafeArray::get_Vartype() const {
	VARTYPE vt;
#if UCFG_WCE
	OleCheck(::API_SafeArrayGetVartype(m_sa, &vt));
#else
	OleCheck(::SafeArrayGetVartype(m_sa, &vt));
#endif
	return vt;
}

COleVariant CSafeArray::operator[](long idx) const {
	switch (Vartype)
	{
	case VT_BSTR:
		{
			CComBSTR bstr;
			GetElement(idx, &bstr);
			return bstr;
		}
	case VT_VARIANT:
		{
			COleVariant v;
			GetElement(idx, &v);
			return v;
		}
	default:
		Throw(E_NOTIMPL);
	}
}

COleSafeArrayAccessData::COleSafeArrayAccessData(COleSafeArray& sa)
:	m_sa(sa)
{
	m_sa.AccessData(&m_p);
}

COleSafeArrayAccessData::~COleSafeArrayAccessData() {
	m_sa.UnaccessData();
}

void *COleSafeArrayAccessData::GetData() {
	return m_p;
}

void AFXAPI PutElement(long* rgIndices, COleSafeArray& sa, const VARIANT& v) {
	COleVariant vv;
	vv.ChangeType(BYTE(sa.vt), (VARIANT*)&v);
	switch (BYTE(sa.vt))
	{
	case VT_VARIANT:
		sa.PutElement(rgIndices, &vv);
		break;
	case VT_BSTR:
	case VT_DISPATCH:
	case VT_UNKNOWN:
		sa.PutElement(rgIndices, vv.bstrVal);
		break;
	default:
		sa.PutElement(rgIndices, &vv.bVal);
	}
}

bool AFXAPI AsOptionalBoolean(const VARIANT& v, bool r) {
	COleVariant vv = AsImmediate(v);
	if (vv.vt == VT_ERROR)
		return r;
	else
		return Convert::ToBoolean(vv);
}

CStringVector AFXAPI AsStringArray(const VARIANT& v) {
	vector<String> r;
	COleSafeArray ar;
	ar.Attach(v);
	for (long i=0; i<ar.GetOneDimSize(); i++) {
		if (BYTE(ar.vt) == VT_BSTR) {
			CComBSTR bstr;
			ar.GetElement(&i, &bstr);
			r.push_back(BSTR(bstr));
		} else if (BYTE(ar.vt) == VT_VARIANT) {
			COleVariant s;
			ar.GetElement(&i, &s);
			r.push_back(Convert::ToString(s));
		}
		else
			Throw(E_EXT_VarTypeInNotStringCompatible);
	}
	ar.Detach();
	return r;
}

COleSafeArray AFXAPI AsVariant(const CStringVector& ar) {
	COleSafeArray sa;
	sa.CreateOneDim(VT_BSTR, (DWORD)ar.size());
	for (long i=0; i<ar.size(); i++)
		sa.PutElement(&i, Bstr(ar[i]));
	return sa;
}


COleVariant BinaryReader::ReadVariantOfType(VARTYPE vt) const {
	COleVariant result;
	switch (vt)
	{
	case VT_EMPTY:
	case VT_NULL:
		result.vt = vt;
		break;
	case VT_UI1:
		result = ReadByte();
		break;
	case VT_I2:
		result = ReadInt16();
		break;
	case VT_I4:
		result = ReadInt32();
		break;
	case VT_R4:
		result = ReadSingle();
		break;
	case VT_R8:
		result = ReadDouble();
		break;
	case VT_CY:
		{
			CY cy;
			_self >> cy;
			result = cy;
			break;
		}
	case VT_DATE:
		{
			DATE date;
			_self >> date;
			result = date;
			break;
		}
	case VT_BOOL:
		result = COleVariant(ReadBoolean());
		break;
	case VT_BSTR:
		{
			DWORD len = ReadUInt32();
			result.bstrVal = SysAllocStringByteLen(0, len);
			result.vt = VT_BSTR;
			Read(result.bstrVal, len);
		}
		break;
	case VT_VARIANT:
		_self >> result;
		break;
	case VT_ARRAY_EX:
		{
			BYTE elType, dims;
			_self >> elType >> dims;
			switch (dims)
			{
			case 1:
				{
					LONG dim1, dim2;
					_self >> dim1 >> dim2;
					COleSafeArray sa;
					sa.CreateOneDim(elType, dim2-dim1+1, 0, dim1);
					for (long i=dim1; i<=dim2; i++)
						PutElement(&i, sa, ReadVariantOfType(elType));
					return sa;
				}
			case 2:
				{
					LONG dim1, dim2, dim3, dim4;
					_self >> dim1 >> dim2 >> dim3 >> dim4;
					COleSafeArray sa;
					SAFEARRAYBOUND sab[2] = {{dim2-dim1+1, dim1},
					{dim4-dim3+1, dim3}};
					sa.Create(elType, 2, sab);
					for (long i=dim1; i<=dim2; i++)
						for (long j=dim3; j<=dim4; j++)
						{
							long ind[2] = {i, j};
							PutElement(ind, sa, ReadVariantOfType(elType));
						}
						return sa;
				}
			default:
				Throw(E_EXT_InvalidDimCount);
			}
			break;
		}
	default:
		Throw(E_EXT_VartypeNotSupported);
	}
	return result;
}

const BinaryReader& BinaryReader::operator>>(CY& v) const {
	return Read(&v, sizeof v);
}

const BinaryReader& BinaryReader::operator>>(COleVariant& v) const {
	v = ReadVariantOfType(ReadByte());
	return _self;
}

COleVariant::COleVariant(const COleCurrency& curSrc) {
	vt = VT_CY;
	cyVal = curSrc.m_cur;
}

const COleVariant& COleVariant::operator=(const COleCurrency& curSrc) {
	Clear();
	vt = VT_CY;
	cyVal = curSrc.m_cur;
	return _self;
}

COleCurrency::COleCurrency() {
	m_cur.Hi = 0;
	m_cur.Lo = 0;
	SetStatus(valid);
}

COleCurrency::COleCurrency(CURRENCY cySrc) {
	m_cur = cySrc;
	SetStatus(valid);
}

COleCurrency::CurrencyStatus COleCurrency::GetStatus() const {
	return m_status;
}

void COleCurrency::SetStatus(CurrencyStatus status) {
	m_status = status;
}




} // Ext::


