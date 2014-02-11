/*######     Copyright (c) 1997-2013 Ufasoft  http://ufasoft.com  mailto:support@ufasoft.com,  Sergey Pavlov  mailto:dev@ufasoft.com #######################################
#                                                                                                                                                                          #
# This program is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation;  #
# either version 3, or (at your option) any later version. This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the      #
# implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details. You should have received a copy of the GNU #
# General Public License along with this program; If not, see <http://www.gnu.org/licenses/>                                                                               #
##########################################################################################################################################################################*/

#include <el/ext.h>


namespace Ext {
using namespace std;

#if UCFG_BLOB_POLYMORPHIC
CStringBlobBuf *CBlobBufBase::AsStringBlobBuf() {
	Throw(E_EXT_BlobIsNotString);
}

void CBlobBufBase::Attach(CBlobBufBase::Char *bstr) {
	Throw(E_EXT_BlobIsNotString);
}

CBlobBufBase::Char *CBlobBufBase::Detach() {
	Throw(E_EXT_BlobIsNotString);
}
#endif

CStringBlobBuf::CStringBlobBuf(size_t len)
	:	m_pChar(0)
#ifdef _WIN64
	,	m_pad(0)
#endif
	,	m_size(len)
{
	*(UNALIGNED String::Char*)((byte*)(this+1)+len) = 0;
}

CStringBlobBuf::CStringBlobBuf(const void *p, size_t len)
	:	m_pChar(0)
#ifdef _WIN64
	,	m_pad(0)
#endif
	,	m_size(len)
{
	if (p)
		memcpy(this+1, p, len);
	else
		memset(this+1, 0, len);
	*(UNALIGNED String::Char*)((byte*)(this+1)+len) = 0;
}

CStringBlobBuf::CStringBlobBuf(size_t len, const void *buf, size_t copyLen)
	:	m_pChar(0)
#ifdef _WIN64
	,	m_pad(0)
#endif
	,	m_size(len)
{
	memcpy(this+1, buf, copyLen);
	memset((byte*)(this+1)+copyLen, 0, len-copyLen+sizeof(String::Char));
}

void * AFXAPI CStringBlobBuf::operator new(size_t sz) {
	return Malloc(sz + sizeof(String::Char));
}

void * AFXAPI CStringBlobBuf::operator new(size_t sz, size_t len) {
	if ((ssize_t)len < 0)
		Throw(E_INVALIDARG);
	return Malloc(sz + len + sizeof(String::Char));
}

CStringBlobBuf *CStringBlobBuf::Clone() {
	return new(m_size) CStringBlobBuf(this+1, m_size);
}

CStringBlobBuf *CStringBlobBuf::SetSize(size_t size) {
	CStringBlobBuf *d;
	size_t copyLen = min(size, size_t(m_size));
	if (m_dwRef == 1) {
		if (m_pChar)
			Free(exchange(m_pChar, nullptr));
#if UCFG_HAS_REALLOC
		d = (CStringBlobBuf*)Realloc(this, size+sizeof(CStringBlobBuf)+sizeof(String::Char));
#else
		d = (CStringBlobBuf*)Malloc(size+sizeof(CStringBlobBuf)+sizeof(String::Char));
		memcpy(d, this, std::min((size_t)d->m_size+sizeof(CStringBlobBuf)+sizeof(String::Char), size+sizeof(CStringBlobBuf)+sizeof(String::Char)));
		Free(this);
#endif
		d->m_size = size;	
		memset((byte*)(d+1)+copyLen, 0, size-copyLen+sizeof(String::Char));
		return d;
	} else {
		d = new(size) CStringBlobBuf(size, this+1, copyLen);
		Release();
		return d;
	}
}

static CStringBlobBuf *s_emptyStringBlobBuf;

static CStringBlobBuf *CreateStringBlobBuf() {
	return s_emptyStringBlobBuf = new CStringBlobBuf();
}

CStringBlobBuf *CStringBlobBuf::RefEmptyBlobBuf() {
	CStringBlobBuf *r = s_emptyStringBlobBuf;
	if (!r)
		r = CreateStringBlobBuf();		
	r->AddRef();
	return r;
}

Blob::Blob(const Blob& blob)
	:	m_pData(blob.m_pData)
{
	if (m_pData)
		m_pData->AddRef();
}

Blob::Blob(const void *buf, size_t len) {
	m_pData = new(len) CStringBlobBuf(buf, len);
}

Blob::Blob(const ConstBuf& mb) {
	m_pData = new(mb.Size) CStringBlobBuf(mb.P, mb.Size);
}

Blob::Blob(const Buf& mb) {
	m_pData = new(mb.Size) CStringBlobBuf(mb.P, mb.Size);
}

Blob::~Blob() {
	if (m_pData)
		m_pData->Release();
}

void Blob::AssignIfNull(const Blob& val) {
	impl_class *pData = val.m_pData;
	if (!Interlocked::CompareExchange(m_pData, pData, (impl_class*)0) && pData)
		pData->AddRef();
}

Blob& Blob::operator=(const Blob& val) {
	if (m_pData != val.m_pData) {
		if (m_pData)
			m_pData->Release();
		if (m_pData = val.m_pData)
			m_pData->AddRef();
	}
	return _self;
}

Blob& Blob::operator+=(const ConstBuf& mb) {
	size_t prevSize = Size;
	if (mb.P == constData()) {
		Size = prevSize+mb.Size;
		memcpy(data()+prevSize, data(), mb.Size);
	} else {
		Size = prevSize+mb.Size;
		memcpy(data()+prevSize, mb.P, mb.Size);
	}
	return *this;
}


#if defined(_MSC_VER) && defined(_M_IX86)

inline bool mem2equal(const void *x, const void *y, size_t siz) {
	__asm {
		mov esi, x
		mov edi, y
		mov ecx, siz
		xor  eax, eax
		rep cmpsw 
		setz al
	}
}

#endif


bool Blob::operator==(const Blob& blob) const {
	if (m_pData == blob.m_pData)
		return true;
	if (m_pData) {
		size_t size = Size;
		if (!blob.m_pData || size!=blob.Size)
			return false;
#if defined(_MSC_VER) && defined(_M_IX86)
		return mem2equal((const wchar_t*)constData(), (const wchar_t*)blob.constData(), (size+1)/2);		// because trailing 2 zeros allow compare by 2-bytes
#else
		return !memcmp(constData(), blob.constData(), size);
#endif
	}
	return false;
}

#undef memcmp
#pragma intrinsic(memcmp)	//!!!

bool Blob::operator<(const Blob& blob) const {
	if (!m_pData)
		return blob.m_pData;
	if (!blob.m_pData)
		return false;
	int n = memcmp(constData(), blob.constData(), min(Size, blob.Size));
	return n ? n<0 : Size<blob.Size;
}

void Blob::Cow() {
	if (m_pData->m_dwRef > 1)
		exchange(m_pData, m_pData->Clone())->Release();
}

byte *Blob::data() {
	Cow();
	return (byte*)m_pData->GetBSTR();
}

Blob Blob::FromHexString(RCString s) {
	int len = s.Length/2;
	Blob blob(0, len);
	for (int i=0; i<len; ++i)
		blob.data()[i] = Convert::ToByte(s.Substring(i*2, 2), 16);
	return blob;
}

void Blob::put_Size(size_t size) {
	m_pData = m_pData ? m_pData->SetSize(size) : new(size) CStringBlobBuf(size);
}

void Blob::Replace(size_t offset, size_t size, const ConstBuf& mb) {
	Cow();
	byte *data;
	size_t newSize = get_Size()+mb.Size-size;
	if (mb.Size >= size) {		
		put_Size(newSize);
		data = (byte*)m_pData->GetBSTR();
		memmove(data+offset+mb.Size, data+offset+size, newSize-offset-mb.Size);
	} else {
		data = (byte*)m_pData->GetBSTR();
		memmove(data+offset+mb.Size, data+offset+size, Size-offset-size);
		put_Size(newSize);
		data = (byte*)m_pData->GetBSTR();
	}
	memcpy(data+offset, mb.P, mb.Size);
}

ostream& __stdcall operator<<(ostream& os, const ConstBuf& cbuf) {
	ios::fmtflags flags = os.flags();
	if (const byte *p = cbuf.P) {
		for (size_t i=0, size=cbuf.Size; i<size; ++i)
			os << hex << setw(2) << setfill('0') << int(p[i]);
	} else
		os << "NULL";
	os.flags(flags);
	return os;
}



} // Ext::


