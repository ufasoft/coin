/*######     Copyright (c) 1997-2013 Ufasoft  http://ufasoft.com  mailto:support@ufasoft.com,  Sergey Pavlov  mailto:dev@ufasoft.com #######################################
#                                                                                                                                                                          #
# This program is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation;  #
# either version 3, or (at your option) any later version. This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the      #
# implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details. You should have received a copy of the GNU #
# General Public License along with this program; If not, see <http://www.gnu.org/licenses/>                                                                               #
##########################################################################################################################################################################*/

#pragma once

#if UCFG_COM
	typedef WCHAR OLECHAR;
	typedef OLECHAR *BSTR;
	typedef OLECHAR *LPOLESTR;
	typedef double DATE;

	typedef struct tagVARIANT VARIANT;
	class _bstr_t;
	typedef unsigned short VARTYPE;
#endif

//!!!#include "libext.h"


namespace Ext {



class CStringBlobBuf;

class COleVariant;

class CBlobBufBase {
public:
	volatile Int32 m_dwRef;

#if UCFG_STRING_CHAR == 16
	typedef Char16 Char;
#elif UCFG_STRING_CHAR == 32
	typedef Char32 Char;
#else
	typedef wchar_t Char;
#endif	

	CBlobBufBase()
		:	m_dwRef(1)
	{}

	void AddRef() noexcept {
		Interlocked::Increment(m_dwRef);
	}

#if UCFG_BLOB_POLYMORPHIC
	virtual ~CBlobBufBase() {}
	virtual Char *GetBSTR() =0; //!!!was BSTR
	virtual CBlobBufBase *Clone() =0;
	virtual size_t GetSize() =0;
	virtual CBlobBufBase *SetSize(size_t size) =0;

	virtual CStringBlobBuf *AsStringBlobBuf();
	virtual void Attach(Char *bstr); //!!!was BSTR
	virtual Char *Detach(); //!!!was BSTR

	void Release() {
		if (!Interlocked::Decrement(m_dwRef))			// Allowed only of destrucotr is virtual
			delete this;
	}
#endif
};

class EXTAPI CStringBlobBuf : public CBlobBufBase {
public:
#ifdef WDM_DRIVER
	UNICODE_STRING m_us;
#endif
	char * volatile m_pChar;
#ifdef _WIN64  //!!!
	UInt32 m_pad;
#endif
	UInt32 m_size;

	CStringBlobBuf(size_t len = 0);
	CStringBlobBuf(const void *p, size_t len);
	CStringBlobBuf(size_t len, const void *buf, size_t copyLen);

	~CStringBlobBuf() {
		if (m_pChar)
			Free(m_pChar);
	}

	void * AFXAPI operator new(size_t sz);
	__forceinline void operator delete(void *p) { Free(p); }
	void * AFXAPI operator new(size_t sz, size_t len);
	__forceinline void operator delete(void *p, size_t) { Free(p); }

	CStringBlobBuf *AsStringBlobBuf() { return this; }

	Char *GetBSTR() { return (Char*)(this+1); } //!!! was BSTR
	size_t GetSize() { return m_size; }

	CStringBlobBuf *Clone();
	CStringBlobBuf *SetSize(size_t size);
	static CStringBlobBuf* AFXAPI RefEmptyBlobBuf();

#if !UCFG_BLOB_POLYMORPHIC
	void Release() {
		if (!Interlocked::Decrement(m_dwRef))
			delete this;
	}
#endif
};

#if UCFG_WIN32 && UCFG_BLOB_POLYMORPHIC && UCFG_COM
class COleBlobBuf : public CBlobBufBase {
public:
	COleBlobBuf();
	~COleBlobBuf();
	void Init(size_t len, const void *buf = 0);
	void Init2(size_t len, const void *buf, size_t copyLen);
	void Empty();
	void Attach(BSTR bstr);
	BSTR Detach();

	Char *GetBSTR() {
		return m_bstr;
	}

	size_t GetSize() { return *((UInt32*)GetBSTR()-1); }

	CBlobBufBase *Clone();
	CBlobBufBase *SetSize(size_t size);
private:
	BSTR m_bstr;
};
#endif

class EXTAPI Blob {
	typedef Blob class_type;
public:
	typedef const byte const_iterator;

#if UCFG_BLOB_POLYMORPHIC
	typedef CBlobBufBase impl_class;
#else
	typedef CStringBlobBuf impl_class;
#endif

	impl_class *m_pData;

	Blob()
		:	m_pData(CStringBlobBuf::RefEmptyBlobBuf())
	{}

	Blob(const Blob& blob);

	Blob(impl_class *pData)
		:	m_pData(pData)
	{}

	Blob(std::nullptr_t)
		:	m_pData(0)
	{}

	Blob(const void *buf, size_t len);
	Blob(const ConstBuf& mb);
	Blob(const Buf& mb);
#if UCFG_COM
	Blob(BSTR bstr);
#endif

#if UCFG_COM
	__forceinline Blob(const VARIANT& v)
		:	m_pData(new CStringBlobBuf)
	{
		SetVariant(v);
	}

	operator COleVariant() const;
#endif

	~Blob();

	void swap(Blob& x) {
		std::swap(m_pData, x.m_pData);
	}
	
	void AssignIfNull(const Blob& val);
	Blob& operator=(const Blob& val);
	Blob& operator=(const ConstBuf& mb) { return operator=(Blob(mb)); }
	bool operator==(const Blob& blob) const;
	bool operator<(const Blob& blob) const;

	bool operator!=(const Blob& blob) const { return !operator==(blob); }

	Blob& operator+=(const ConstBuf& mb);

	bool operator!() const { return !m_pData; }

	operator ConstBuf() const { return m_pData ? ConstBuf(constData(), Size) : ConstBuf(0, 0); }

	static Blob AFXAPI FromHexString(RCString s);

	size_t get_Size() const { return m_pData->GetSize(); }
	void put_Size(size_t size);
	DEFPROP_CONST(size_t, Size);

	// we don't use property feature to explicit call constData() for efficiency
	byte *data();
	__forceinline const byte *data() const { return (const byte*)m_pData->GetBSTR(); }
	__forceinline const byte *constData() const { return (const byte*)m_pData->GetBSTR(); }

	const byte *begin() const { return constData(); }
	const byte *end() const { return constData()+Size; }

	byte operator[](size_t idx) const {
		if (idx >= Size)
			Throw(E_EXT_IndexOutOfRange);
		return constData()[idx];
	}

	byte& operator[](size_t idx) {
		if (idx >= Size)
			Throw(E_EXT_IndexOutOfRange);
		return data()[idx];
	}

	void Replace(size_t offset, size_t size, const ConstBuf& mb);
protected:
	void Cow();
#if UCFG_COM
	void SetVariant(const VARIANT& v);
#endif

private:
//	EXPLICIT_OPERATOR_BOOL() const;												// don't public it. Ambiguous type conversions
friend class String;
};

inline void swap(Blob& x, Blob& y) {
	x.swap(y);
}

inline Blob operator+(const ConstBuf& mb1, const ConstBuf& mb2) {
	Blob r(0, mb1.Size+mb2.Size);
	memcpy(r.data(), mb1.P, mb1.Size);
	memcpy(r.data()+mb1.Size, mb2.P, mb2.Size);
	return r;
}

EXT_API std::ostream& __stdcall operator<<(std::ostream& os, const ConstBuf& cbuf);

inline std::ostream& __stdcall operator<<(std::ostream& os, const Blob& blob) {
	return os << ConstBuf(blob);
}

typedef const Blob& RCBlob;


} // Ext::

namespace EXT_HASH_VALUE_NS {
	inline size_t hash_value(const Ext::Blob& blob) {
		return Ext::hash_value(blob.constData(), blob.Size);
	}
}

EXT_DEF_HASH(Ext::Blob)

