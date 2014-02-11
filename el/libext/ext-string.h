/*######     Copyright (c) 1997-2013 Ufasoft  http://ufasoft.com  mailto:support@ufasoft.com,  Sergey Pavlov  mailto:dev@ufasoft.com #######################################
#                                                                                                                                                                          #
# This program is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation;  #
# either version 3, or (at your option) any later version. This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the      #
# implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details. You should have received a copy of the GNU #
# General Public License along with this program; If not, see <http://www.gnu.org/licenses/>                                                                               #
##########################################################################################################################################################################*/

#pragma once


namespace Ext {

template <typename T>
class explicit_cast {
public:
	explicit_cast(T t)
		:	m_t(t)
	{}

	operator T() const { return m_t; }
private:
	T m_t;
};


class Encoding;

class EXTCLASS String {
public:
	typedef String class_type;

	typedef CBlobBufBase::Char Char;
	typedef Char value_type;
	typedef std::char_traits<Char> traits_type;
	typedef ssize_t difference_type;

	class const_iterator {
	public:
		typedef Char value_type;
		typedef std::random_access_iterator_tag iterator_category;
		typedef String::difference_type difference_type;
		typedef const Char *pointer;
		typedef const Char& reference;

		const_iterator()
			:	m_p(0)
		{}

		reference operator*() const { return *m_p; }

		const_iterator& operator++() {
			++m_p;
			return *this;
		}

		const_iterator operator++(int) {
			const_iterator r(*this);
			operator++();
			return r;
		}

		bool operator==(const_iterator it) const { return m_p == it.m_p; }
		bool operator!=(const_iterator it) const { return !operator==(it); }

		const_iterator operator+(difference_type diff) const { return const_iterator(m_p + diff); }
		difference_type operator-(const_iterator it) const { return m_p - it.m_p; }
	private:
		const Char *m_p;

		explicit const_iterator(const Char* p)
			:	m_p(p)
		{}

		friend class String;
	};


	String() {}

	String(RCString s)
		:	m_blob(s.m_blob)
	{
	}

	String(const char *lpch, ssize_t nLength);
#if defined(_NATIVE_WCHAR_T_DEFINED) && UCFG_STRING_CHAR/8 == __SIZEOF_WCHAR_T__
	String(const UInt16 *lpch, ssize_t nLength);
	String(const UInt16 *lpsz);
//	void Init(const UInt16 *lpch, ssize_t nLength);
	void Init(const unsigned short *lpch, ssize_t nLength);
#endif

	String(const char *lpch, int start, ssize_t nLength, Encoding *enc)
		:	m_blob(nullptr)
	{
		Init(enc, lpch+start, nLength);
	}

	String(const Char *lpch, ssize_t nLength)
		:	m_blob(nullptr)
	{
		Init(lpch, nLength);
	}

	String(const_iterator b, const_iterator e)
		:	m_blob(nullptr)
	{
		Init(b.m_p, e-b);
	}

	explicit String(char ch, ssize_t nRepeat = 1);
	explicit String(Char ch, ssize_t nRepeat = 1);
	String(const char *lpsz);
	String(const Char *lpsz);
#if UCFG_STRING_CHAR/8 != __SIZEOF_WCHAR_T__
	String(const wchar_t *lpsz);
#endif
	EXT_API String(const std::string& s);
	EXT_API String(const std::wstring& s);

	EXT_API String(const std::vector<Char>& vec);

	String (std::nullptr_t p)
		:	m_blob(p)
	{
	}


#if UCFG_COM
	String(const _bstr_t& bstr);

	BSTR get_Bstr() const { return m_blob.m_pData ? m_blob.m_pData->GetBSTR() : 0; }
	DEFPROP_GET_CONST(BSTR, Bstr);

//!!!	operator _bstr_t() const { return _bstr_t(Bstr, true); }
#endif

#if UCFG_USE_REGEX
	template <typename I>
	String(const std::sub_match<I>& sb)
		:	m_blob(nullptr)
	{
		operator=(sb.str());
	}
#endif

	void swap(String& x) {
		m_blob.swap(x.m_blob);
	}

	//!!!	String(const string& s);

	bool operator!() const { return !m_blob.m_pData; }

	const char *c_str() const;
	operator const char *() const { return c_str(); }
	operator const Char *() const { return m_blob.m_pData ? (const Char*)m_blob.m_pData->GetBSTR() : 0; }

	const_iterator begin() const { return const_iterator(m_blob.m_pData ? (const Char*)m_blob.m_pData->GetBSTR() : 0); }
	const_iterator end() const { return const_iterator(m_blob.m_pData ? (const Char*)m_blob.m_pData->GetBSTR()+Length : 0); }

	EXT_API operator explicit_cast<std::string>() const;

	operator explicit_cast<std::wstring>() const {
		const Char *p = *this,
			       *e = p+Length;
		return std::wstring(p, e);
	}

#ifdef WDM_DRIVER
	String(UNICODE_STRING *pus);
	operator UNICODE_STRING*() const;
	inline operator UNICODE_STRING() const {
		if (m_blob.m_pData)
			return *(operator UNICODE_STRING*());
		UNICODE_STRING us = { 0 };
		return us;
	}
#endif

	Char operator[](int nIndex) const;
	Char operator[](size_t nIndex) const { return operator[]((int)nIndex); }
	//!!!D  String ToOem() const;
	Char GetAt(size_t idx) const { return (*this)[idx]; }
	void SetAt(size_t nIndex, char c);
	void SetAt(size_t nIndex, unsigned char c) { SetAt(nIndex, (char)c); }
	void SetAt(size_t nIndex, Char ch);

	String& operator=(const String& stringSrc) {
		m_blob = stringSrc.m_blob;
		return *this;
	}

	String& operator=(std::nullptr_t p) {
		return operator=((const Char*)0);
	}

	String& operator=(const char * lpsz);
	String& operator=(const Char * lpsz);

	String& operator=(EXT_RV_REF(String) rv) {
		swap(rv);
		return *this;
	}

	String& operator+=(const String& s);
	void CopyTo(char *ar, size_t size) const;
	void CopyTo(Char *ar, size_t size) const;
	int Compare(const String& s) const;
	int CompareNoCase(const String& s) const;
	bool IsEmpty() const noexcept;
	bool empty() const { return IsEmpty(); }
	void Empty();
	int Find(Char c) const;
	int LastIndexOf(Char c) const;
	int Find(const String& s, int nStart = 0) const;

	bool Contains(const String& s) const { return Find(s) != -1; }

	int FindOneOf(const String& sCharSet) const;
	String Mid(int nFirst, int nCount = INT_MAX) const;
	String Substring(int nFirst, int nCount = INT_MAX) const { return Mid(nFirst, nCount); }
	String Right(ssize_t nCount) const;
	String Left(ssize_t nCount) const;
	String TrimStart(RCString trimChars = nullptr) const;
	String TrimEnd(RCString trimChars = nullptr) const;
	String& TrimLeft() { return (*this) = TrimStart(); }
	String& TrimRight() { return (*this) = TrimEnd(); }
	String Trim() const { return TrimStart().TrimEnd(); }
	EXT_API std::vector<String> Split(RCString separator = "", size_t count = INT_MAX) const;
	EXT_API static String AFXAPI Join(RCString separator, const std::vector<String>& value);
	void MakeUpper();
	void MakeLower();

	String ToUpper() const {
		String r(*this);
		r.MakeUpper();
		return r;
	}

	String ToLower() const {
		String r(*this);
		r.MakeLower();
		return r;
	}

	void Replace(int offset, int size, const String& s);
	int Replace(RCString sOld, RCString sNew);

	//!!!D  static String AFXAPI FromUTF8(const char *p, int len = -1);

	size_t GetLength() const;
	size_t get_Length() const { return GetLength(); }
	DEFPROP_GET_CONST(size_t, Length);

	size_t size() const { return Length; }

#if UCFG_USE_POSIX
	std::string ToOsString() const {
		return operator explicit_cast<std::string>();
	}
#elif UCFG_STDSTL 
	std::wstring ToOsString() const {
		return operator explicit_cast<std::wstring>();
	}
#else
	String ToOsString() const {
		return *this;
	}
#endif

#if UCFG_COM
	static String AFXAPI FromGlobalAtom(ATOM a);
	BSTR AllocSysString() const;
	LPOLESTR AllocOleString() const;
	void Load(UINT nID);
	bool Load(UINT nID, WORD wLanguage);
//!!!R	bool LoadString(UINT nID); //!!!comp
#endif

private:
	Blob m_blob;

	void Init(Encoding *enc, const char *lpch, ssize_t nLength);
	void Init(const Char *lpch, ssize_t nLength);
	void MakeDirty();

	friend AFX_API String AFXAPI operator+(const String& string1, const String& string2);
	friend AFX_API String AFXAPI operator+(const String& string, const char * lpsz);
	friend AFX_API String AFXAPI operator+(const String& string, const Char * lpsz);
	//!!!friend AFX_API String AFXAPI operator+(const String& string, TCHAR ch);
	friend inline bool operator<(const String& s1, const String& s2);
	friend inline bool AFXAPI operator==(const String& s1, const String& s2);
};

inline void swap(String& x, String& y) {
	x.swap(y);
}


#if UCFG_STLSOFT
} namespace stlsoft {
	inline const char *c_str_ptr(Ext::RCString s) { return s.c_str(); }
} namespace Ext {
#endif

#if UCFG_COM
	inline BSTR Bstr(RCString s) { return s.Bstr; }		// Shim
#endif

typedef std::vector<String> CStringVector;

inline bool operator<(const String& s1, const String& s2)  { //!!! can be intrinsic
	//!!!  size_t count = (s1.m_blob.Size>>1)+1;
	String::Char *p1 = (String::Char*)s1.m_blob.constData(),
		*p2 = (String::Char*)s2.m_blob.constData();
	//!!!  MacFastWcscmp(count, p1, p2);
	//!!!  return FastWcscmp((s1.m_blob.Size>>1)+1, (wchar_t*)s1.m_blob.Data, (wchar_t*)s2.m_blob.Data);
	return StrCmp(p1, p2) < 0;
}


inline bool AFXAPI operator==(const String& s1, const String& s2) { return s1.m_blob == s2.m_blob; }
AFX_API bool AFXAPI operator!=(const String& s1, const String& s2);
AFX_API bool AFXAPI operator<=(const String& s1, const String& s2);
AFX_API bool AFXAPI operator>=(const String& s1, const String& s2);

AFX_API bool AFXAPI operator==(const String& s1, const char * s2);
AFX_API bool AFXAPI operator==(const char * s1, const String& s2);
AFX_API bool AFXAPI operator!=(const String& s1, const char * s2);
AFX_API bool AFXAPI operator!=(const char * s1, const String& s2);
AFX_API bool AFXAPI operator<(const char * s1, const String& s2);
AFX_API bool AFXAPI operator<(const String& s1, const char * s2);
AFX_API bool AFXAPI operator>(const String& s1, const char * s2);
AFX_API bool AFXAPI operator>(const char * s1, const String& s2);
AFX_API bool AFXAPI operator<=(const String& s1, const char * s2);
AFX_API bool AFXAPI operator<=(const char * s1, const String& s2);
AFX_API bool AFXAPI operator>=(const String& s1, const char * s2);
AFX_API bool AFXAPI operator>=(const char * s1, const String& s2);

AFX_API bool AFXAPI operator==(const String& s1, const String::Char * s2);
AFX_API bool AFXAPI operator==(const String::Char * s1, const String& s2);
AFX_API bool AFXAPI operator!=(const String& s1, const String::Char * s2);
AFX_API bool AFXAPI operator!=(const String::Char * s1, const String& s2);
AFX_API bool AFXAPI operator<(const String::Char * s1, const String& s2);
AFX_API bool AFXAPI operator<(const String& s1, const String::Char * s2);
AFX_API bool AFXAPI operator>(const String& s1, const String::Char * s2);
AFX_API bool AFXAPI operator>(const String::Char * s1, const String& s2);
AFX_API bool AFXAPI operator<=(const String& s1, const String::Char * s2);
AFX_API bool AFXAPI operator<=(const String::Char * s1, const String& s2);
AFX_API bool AFXAPI operator>=(const String& s1, const String::Char * s2);
AFX_API bool AFXAPI operator>=(const String::Char * s1, const String& s2);

inline bool AFXAPI operator!=(const String& s1, std::nullptr_t) {
	return s1 != (const String::Char*)0;
}

inline bool AFXAPI operator==(const String& s1, std::nullptr_t) {
	return s1 == (const String::Char*)0;
}


template <class A, class B> String Concat(const A& a, const B& b) {
	std::ostringstream os;
	os << a << b;
	return os.str().c_str();
}

template <class A, class B, class C> String Concat(const A& a, const B& b, const C& c) {
	std::ostringstream os;
	os << a << b << c;
	return os.str().c_str();
}



AFX_API String AFXAPI AfxLoadString(UINT nIDS);

struct CStringResEntry {
	UINT ID;
	const char *Ptr;
};




//EXPIMP_TEMPLATE template class AFX_TEMPL_CLASS /*!!!EXPIMP_CLASS*/ vector<String>;


} // Ext::

namespace EXT_HASH_VALUE_NS {
inline size_t hash_value(const Ext::String& s) {
	return Ext::hash_value((const Ext::String::Char*)s, (s.Length*sizeof(Ext::String::Char)));
}
}

EXT_DEF_HASH(Ext::String)

namespace std {
	inline int AFXAPI stoi(Ext::RCString s, size_t *idx = 0, int base = 10) {
		return stoi(Ext::explicit_cast<string>(s), idx, base);
	}

	inline long AFXAPI stol(Ext::RCString s, size_t *idx = 0, int base = 10) {
		return stol(Ext::explicit_cast<string>(s), idx, base);
	}

	inline long long AFXAPI stoll(Ext::RCString s, size_t *idx = 0, int base = 10) {
		return stoll(Ext::explicit_cast<string>(s), idx, base);
	}

	inline double AFXAPI stod(Ext::RCString s, size_t *idx = 0) {
		return stod(Ext::explicit_cast<string>(s), idx);
	}
} // std::


