/*######     Copyright (c) 1997-2013 Ufasoft  http://ufasoft.com  mailto:support@ufasoft.com,  Sergey Pavlov  mailto:dev@ufasoft.com #######################################
#                                                                                                                                                                          #
# This program is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation;  #
# either version 3, or (at your option) any later version. This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the      #
# implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details. You should have received a copy of the GNU #
# General Public License along with this program; If not, see <http://www.gnu.org/licenses/>                                                                               #
##########################################################################################################################################################################*/

#include <el/ext.h>

#if UCFG_WIN32
#	include <comutil.h>
#endif

#if UCFG_STDSTL && !UCFG_WCE
#	include <locale>
#endif

namespace Ext {
using namespace std;

String::String(char ch, ssize_t nRepeat) {
	Char wch;
#ifdef WDM_DRIVER
	wch = ch;
#else
	Encoding& enc = Encoding::Default();
	enc.GetChars(ConstBuf(&ch, 1), &wch, 1);
#endif
	m_blob.Size = nRepeat*sizeof(String::Char);
	fill_n((Char*)m_blob.data(), nRepeat, wch);
}

void String::SetAt(size_t nIndex, char ch) {
	Char wch;
#ifdef WDM_DRIVER
	wch = ch;
#else
	Encoding& enc = Encoding::Default();
	enc.GetChars(ConstBuf(&ch, 1), &wch, 1);
#endif
	SetAt(nIndex, wch);
}

String::String(const char *lpch, ssize_t nLength)
	:	m_blob(nullptr)
{
#ifdef WDM_DRIVER
	Init(nullptr, lpch, nLength);
#else
	Init(&Encoding::Default(), lpch, nLength);
#endif
}

String::String(const char *lpsz)
	:	m_blob(nullptr)
{
#ifdef WDM_DRIVER
	size_t len = lpsz ? strlen(lpsz) : -1;
	Init((const wchar_t*)nullptr, len);
	if (lpsz)
		std::copy(lpsz, lpsz+len, (wchar_t*)m_blob.data());
#else
	Init(&Encoding::Default(), lpsz, lpsz ? strlen(lpsz) : -1);  //!!! May be multibyte
#endif
}

#if defined(_NATIVE_WCHAR_T_DEFINED) && UCFG_STRING_CHAR/8 == __SIZEOF_WCHAR_T__

String::String(const UInt16 *lpch, ssize_t nLength)
	:	m_blob(nullptr)
{
	Init(lpch, nLength);
}

String::String(const UInt16 *lpsz)
	:	m_blob(nullptr)
{
	int len = -1;
	if (lpsz) {
		len = 0;
		for (const UInt16 *p=lpsz; *p; ++p)
			++len;
	}
	Init((const Char*)nullptr, len);
	if (lpsz)
		std::copy(lpsz, lpsz+len, (Char*)m_blob.data());
}

void String::Init(unsigned short const *lpch, ssize_t nLength) {
	Init((const Char*)nullptr, nLength);
	std::copy(lpch, lpch+nLength, (Char*)m_blob.data());
}
#endif

void String::Init(const Char *lpch, ssize_t nLength) {
	if (!lpch && nLength == -1)
		m_blob.m_pData = 0; //!!! = new CStringBlobBuf;
	else {
		size_t bytes = nLength*sizeof(Char);
		m_blob.m_pData = new(bytes) CStringBlobBuf(bytes);
		if (lpch)
			memcpy(m_blob.data(), lpch, bytes);				//!!! was wcsncpy((BSTR)m_blob.get_Data(), lpch, nLength);
	}
}

#if UCFG_STRING_CHAR/8 != __SIZEOF_WCHAR_T__
String::String(const wchar_t *lpsz) {
	size_t len = wcslen(lpsz);
	Init((const Char*)0, len);				//!!! not correct for UTF-32
	Char *p = (Char*)m_blob.data();
	for (int i=0; i<len; ++i)
		p[i] = Char(lpsz[i]);
}
#endif

String::String(const std::string& s)
	:	m_blob(nullptr)
{
#ifdef WDM_DRIVER
	Init(nullptr, s.c_str(), s.size());
#else
	Init(&Encoding::Default(), s.c_str(), s.size());
#endif
}

String::String(const std::wstring& s)
	:	m_blob(nullptr)
{
	Init((const Char*)0, s.size());				//!!! not correct for UTF-32
	Char *p = (Char*)m_blob.data();
	for (int i=0; i<s.size(); ++i)
		p[i] = Char(s[i]);
}

#if UCFG_COM

String::String(const _bstr_t& bstr)
	:	m_blob(nullptr)
{
	const wchar_t *p = bstr;
	Init(p, p ? wcslen(p) : -1);
}

#endif // UCFG_COM

void String::Init(Encoding *enc, const char *lpch, ssize_t nLength) {
	if (!lpch)
		m_blob.m_pData = 0; //!!!new CStringBlobBuf;
	else {
		int len = 0;
		if (nLength) {
			if (enc) {
				ConstBuf mb(lpch, nLength);
				len = enc->GetCharCount(mb);
				size_t bytes = len*sizeof(Char);
				m_blob.m_pData = new(bytes) CStringBlobBuf(bytes);
				enc->GetChars(mb, (Char*)m_blob.data(), len);
			} else {
				Init((const Char *)nullptr, nLength);
				std::copy(lpch, lpch+nLength, (Char*)m_blob.data());
			}
		} else {
			size_t bytes = len*sizeof(Char);
			m_blob.m_pData = new(bytes) CStringBlobBuf(bytes);
		}
	}
}

const char *String::c_str() const {  //!!! optimize
	if (!m_blob.m_pData)
		return 0;
	char * volatile &pChar = m_blob.m_pData->AsStringBlobBuf()->m_pChar;
	if (!pChar) {
		Encoding& enc = Encoding::Default();
		for (size_t n=(m_blob.Size/sizeof(Char))+1, len=n+1;; len<<=1) {
			Array<char> p(len);
			size_t r;
			if ((r=enc.GetBytes((const Char*)m_blob.m_pData->GetBSTR(), n, (byte*)p.get(), len)) < len) {
				p.get()[r] = 0; //!!!R
				char *pch = p.release();
				if (Interlocked::CompareExchange(pChar, pch, (char*)nullptr))
					Free(pch);
				break;
			}
		}
	}
	return pChar;
}

String::operator explicit_cast<std::string>() const {
	Blob bytes = Encoding::Default().GetBytes(_self);
	return std::string((const char*)bytes.constData(), bytes.Size);
}

void String::CopyTo(char *ar, size_t size) const {
#ifdef WDM_DRIVER
	Throw(E_NOTIMPL);
#else
	const char *p = _self;
	size_t len = std::min(size-1, strlen(p)-1);
	memcpy(ar, p, len);
	ar[len] = 0;
#endif
}

int String::FindOneOf(RCString sCharSet) const {
	Char ch;
	for (const Char *pbeg=_self, *p=pbeg, *c=sCharSet; (ch=*p); ++p)
		if (StrChr(c, ch))
			return int(p-pbeg);  //!!! shoul be ssize_t
	return -1;
}

int String::Replace(RCString sOld, RCString sNew) {
	const Char *p = _self,
		*q = p+Length,
		*r;
	int nCount = 0,
		nOldLen = (int)sOld.Length,
		nReplLen = (int)sNew.Length;
	for (; p<q; p += traits_type::length(p)+1) {
		while (r = StrStr<Char>(p, sOld)) {
			nCount++;
			p = r+nOldLen;
		}
	}
	if (nCount) {
		int nLen = int(Length+(nReplLen-nOldLen)*nCount);
		Char *pTarg = (Char*)alloca(nLen*sizeof(Char)),
			*z = pTarg;
		for (p = _self; p<q;) {
			if (StrNCmp<Char>(p, sOld, nOldLen))
				*z++ = *p++;
			else {
				memcpy(z, (const Char*)sNew, nReplLen*sizeof(Char));
				p += nOldLen;
				z += nReplLen;
			}
		}
		_self = String(pTarg, nLen);
	}
	return nCount;
}


String::String(Char ch, ssize_t nRepeat) {
	m_blob.Size = nRepeat*sizeof(Char);
	fill_n((Char*)m_blob.data(), nRepeat, ch);
}

String::String(const Char *lpsz)
	:	m_blob(nullptr)
{
	Init(lpsz, lpsz ? traits_type::length(lpsz) : -1);
}

/*!!!
String::String(const string& s)
	: m_blob(nullptr)
{
const char *p = s.c_str();
Init(p, strlen(p));  //!!! May be multibyte
}*/

String::String(const std::vector<Char>& vec)
	:	m_blob(nullptr)
{
	Init(vec.empty() ? 0 : &vec[0], vec.size());
}

void String::MakeDirty() {
	if (m_blob.m_pData)
		if (char * volatile &p = m_blob.m_pData->AsStringBlobBuf()->m_pChar)
			Free(exchange(p, (char*)0));
}

#if UCFG_WDM
String::String(UNICODE_STRING *pus)
	:	m_blob(nullptr)
{
	if (pus)
		Init(pus->Buffer, pus->Length/sizeof(wchar_t));
	else
		Init((const wchar_t*)NULL, -1);
}

String::operator UNICODE_STRING*() const {
	if (CStringBlobBuf *pBuf = (CStringBlobBuf*)m_blob.m_pData) {
		UNICODE_STRING *us = &pBuf->m_us;
		us->Length = us->MaximumLength = (USHORT)m_blob.Size;
		us->Buffer = (WCHAR*)(const wchar_t*)_self;
		return us;
	} else
		return nullptr;
}
#endif

String::Char String::operator[](int nIndex) const {
		return (operator const Char*())[nIndex];
}



/*!!!D
String String::FromUTF8(const char *p, int len)
{
vector<wchar_t> ar;
if (len < 0)
len = strlen(p);
for (char c; len-->0 && (c = *p++);)
{
BYTE b = c;
wchar_t wc;
if (b < 0x80)
wc = b;
else if (b < 0xE0)
{
BYTE b2 = *p++;
len--;
if (!(b2 & 0x80))
Throw(E_EXT_InvalidUTF8String);
wc = ((b & 0x1F) << 6) | b2 & 0x3F;
}
else
{
BYTE b2 = *p++;
len--;
if (!(b2 & 0x80))
Throw(E_EXT_InvalidUTF8String);
BYTE b3 = *p++;
len--;
if (!(b3 & 0x80))
Throw(E_EXT_InvalidUTF8String);
wc = ((b & 0xF) << 12) | ((b2 & 0x3F) << 6) | b3 & 0x3F;
}
ar.push_back(wc);
}
return String(&ar[0], ar.size());
}*/

/*!!!D
String String::ToOem() const
{
char *p = (char*)alloca(Length+1);
Win32Check(::CharToOem(_self, p));
return p;
}
*/

void String::SetAt(size_t nIndex, Char ch) {
	MakeDirty();
	m_blob.m_pData->GetBSTR()[nIndex] = ch;
}

String& String::operator=(const char *lpsz) {
	return operator=(String(lpsz));
}

String& String::operator=(const Char *lpsz) {
	MakeDirty();
	if (lpsz) {
		size_t len = traits_type::length(lpsz)*sizeof(Char);
		if (!m_blob.m_pData)
			m_blob.m_pData = new(len) CStringBlobBuf(len);
		else
			m_blob.Size = len;
		memcpy(m_blob.data(), lpsz, len);
	} else {
		if (m_blob.m_pData)
			m_blob.m_pData->Release();
		m_blob.m_pData = 0;
	}
	return _self;
}

String& String::operator+=(const String& s) {
	MakeDirty();
	m_blob += s.m_blob;
	return _self;
}

void String::CopyTo(Char *ar, size_t size) const {
	size_t len = std::min(size-1, (size_t)Length-1);
	memcpy(ar, m_blob.constData(), len*sizeof(Char));
	ar[len] = 0;
}

int String::Compare(const String& s) const {
	Char *s1 = (Char*)m_blob.constData(),
		*s2 = (Char*)s.m_blob.constData();
	for (int len1=Length, len2=s.Length;; --len1, --len2) {
		Char ch1=*s1++, ch2=*s2++;
		if (ch1 < ch2)
			return -1;
		if (ch1 > ch2)
			return 1;
		if (!len1)
			return len2 ? -1 : 0;
		else if (!len2)
			return 1;
	}
}

static locale& UserLocale() {
	static locale s_locale("");
	return s_locale; 
}

static locale& s_locale_not_used = UserLocale();	// initialize while one thread, don't use

int String::CompareNoCase(const String& s) const {
	Char *s1 = (Char*)m_blob.constData(),
		*s2 = (Char*)s.m_blob.constData();
	for (int len1=Length, len2=s.Length;; --len1, --len2) {
		Char ch1=*s1++, ch2=*s2++;
#if UCFG_USE_POSIX
		ch1 = (Char)tolower<wchar_t>(ch1, UserLocale());
		ch2 = (Char)tolower<wchar_t>(ch2, UserLocale());
#else
		ch1 = (Char)tolower(ch1, UserLocale());
		ch2 = (Char)tolower(ch2, UserLocale());
#endif
		if (ch1 < ch2)
			return -1;
		if (ch1 > ch2)
			return 1;
		if (!len1)
			return len2 ? -1 : 0;
		else if (!len2)
			return 1;
	}
}

bool String::IsEmpty() const {
	return !m_blob.m_pData || m_blob.Size==0;
}

void String::Empty() {
	m_blob.Size = 0;
	MakeDirty();
}

int String::Find(Char c) const {
	for (size_t i=0,e=Length; i<e; i++)
		if (_self[i] == c)
			return (int)i;
	return -1;
}

int String::LastIndexOf(Char c) const {
	for (size_t i=Length; i--;)
		if (_self[i] == c)
			return (int)i;
	return -1;
}

int String::Find(RCString s, int nStart) const {
	if (nStart > Length)
		return -1;
	const Char *p = _self;
	const Char *lpsz = StrStr<Char>(p+nStart, s);
	return int(lpsz ? lpsz - p : -1);
}

String String::Mid(int nFirst, int nCount) const {
	// out-of-bounds requests return sensible things
	if (nFirst < 0)
		nFirst = 0;
	if (nCount < 0)
		nCount = 0;

	if ((DWORD)nFirst + (DWORD)nCount > (DWORD)Length)
		nCount = int(Length - nFirst);
	if (nFirst > Length)
		nCount = 0;

	// optimize case of returning entire string
	if (nFirst == 0 && nFirst + nCount == Length)
		return _self;

	String dest;
	dest.m_blob.Size = nCount*sizeof(Char);
	memcpy(dest.m_blob.data(), m_blob.constData()+nFirst*sizeof(Char), nCount*sizeof(Char));
	return dest;
}

String String::Right(ssize_t nCount) const {
	if (nCount < 0)
		nCount = 0;
	if (nCount >= Length)
		return _self;
	String dest;
	dest.m_blob.Size = nCount*sizeof(Char);
	memcpy(dest.m_blob.data(), m_blob.constData()+(Length-nCount)*sizeof(Char), nCount*sizeof(Char));
	return dest;
}

String String::Left(ssize_t nCount) const {
	if (nCount < 0)
		nCount = 0;
	if (nCount >= Length)
		return _self;
	String dest;
	dest.m_blob.Size = nCount*sizeof(Char);
	memcpy(dest.m_blob.data(), m_blob.constData(), nCount*sizeof(Char));
	return dest;
}

String String::TrimStart(RCString trimChars) const {
	size_t i;
	for (i=0; i<Length; i++) {
		Char ch = _self[i];
		if (!trimChars) {
			if (!iswspace(ch)) //!!! must test wchar_t
				break;
		} else if (trimChars.Find(ch) == -1)
			break;
	}
	return Right(int(Length-i));
}

String String::TrimEnd(RCString trimChars) const {
	ssize_t i;
	for (i=Length; i--;) {
		Char ch = _self[(size_t)i];
		if (!trimChars) {
			if (!iswspace(ch)) //!!! must test wchar_t
				break;
		} else if (trimChars.Find(ch) == -1)
			break;
	}
	return Left(int(i+1));
}

vector<String> String::Split(RCString separator, size_t count) const {
	String sep = separator.IsEmpty() ? " \t\n\r\v\f\b" : separator;
	vector<String> ar;
	for (const Char *p = _self; count-- && *p;) {
		if (count) {
			size_t n = StrCSpn<Char>(p, sep);
			ar.push_back(String(p, n));
			if (*(p += n) && !*++p)
				ar.push_back("");
		} else
			ar.push_back(p);
	}
	return ar;
}

String String::Join(RCString separator, const std::vector<String>& value) {
	String r;
	for (int i=0; i<value.size(); ++i)
		r += (i ? separator : "") + value[i];
	return r;
}

void String::MakeUpper() {
	MakeDirty();
	for (Char *p=(Char*)m_blob.data(); *p; ++p) {
#if UCFG_USE_POSIX
		*p = (Char)toupper<wchar_t>(*p, UserLocale());
#else
		*p = (Char)toupper(*p, UserLocale());
#endif
	}
}

void String::MakeLower() {
	MakeDirty();
	for (Char *p=(Char*)m_blob.data(); *p; ++p) {
#if UCFG_USE_POSIX
		*p = (Char)tolower<wchar_t>(*p, UserLocale());
#else
		*p = (Char)tolower(*p, UserLocale());
#endif
	}
}

void String::Replace(int offset, int size, const String& s) {
	MakeDirty();
	m_blob.Replace(offset*sizeof(Char), size*sizeof(Char), s.m_blob);
}

size_t String::GetLength() const {
	return m_blob.Size/sizeof(Char);
}

#if UCFG_COM
BSTR String::AllocSysString() const {
	return ::SysAllocString(Bstr);
}

LPOLESTR String::AllocOleString() const {
	LPOLESTR p = (LPOLESTR)CoTaskMemAlloc(m_blob.Size+sizeof(wchar_t));
	memcpy(p, Bstr, m_blob.Size+sizeof(wchar_t));
	return p;
}
#endif

String AFXAPI operator+(const String& string1, const String& string2) {
	size_t len1 = string1.Length*sizeof(String::Char),
		len2 = string2.Length*sizeof(String::Char);
	String s;
	s.m_blob.Size = len1+len2;
	memcpy(s.m_blob.data(), string1.m_blob.constData(), len1);
	memcpy(s.m_blob.data()+len1, string2.m_blob.constData(), len2);
	return s;
}

String AFXAPI operator+(const String& string, const char *lpsz) {
	return operator+(string, String(lpsz));
}

String AFXAPI operator+(const String& string, const String::Char *lpsz) {
	return operator+(string, String(lpsz));
}

String AFXAPI operator+(const String& string, char ch) {
	return operator+(string, String(ch));
}

String AFXAPI operator+(const String& string, String::Char ch) {
	return operator+(string, String(ch));
}

/*!!!
bool __fastcall operator<(const String& s1, const String& s2)
{
return wcscmp((wchar_t*)s1.m_blob.Data, (wchar_t*)s2.m_blob.Data) < 0;
}*/

bool AFXAPI operator==(const String& s1, const char * s2)
{ return s1==String(s2); }
bool AFXAPI operator!=(const String& s1, const String& s2)
{ return !(s1==s2); }
bool AFXAPI operator>(const String& s1, const String& s2)
{ return s1.Compare(s2) > 0; }
bool AFXAPI operator<=(const String& s1, const String& s2)
{ return s1.Compare(s2) <= 0; }
bool AFXAPI operator>=(const String& s1, const String& s2)
{ return s1.Compare(s2) >= 0; }

bool AFXAPI operator==(const char * s1, const String& s2)
{ return String(s1)==s2; }
bool AFXAPI operator!=(const String& s1, const char * s2)
{ return !(s1==s2); }
bool AFXAPI operator!=(const char * s1, const String& s2)
{ return !(s1==s2); }
bool AFXAPI operator<(const String& s1, const char *s2)
{ return s1.Compare(s2) < 0; }
bool AFXAPI operator<(const char * s1, const String& s2)
{ return s2.Compare(s1) > 0; }
bool AFXAPI operator>(const String& s1, const char * s2)
{ return s1.Compare(s2) > 0; }
bool AFXAPI operator>(const char * s1, const String& s2)
{ return s2.Compare(s1) < 0; }
bool AFXAPI operator<=(const String& s1, const char * s2)
{ return s1.Compare(s2) <= 0; }
bool AFXAPI operator<=(const char * s1, const String& s2)
{ return s2.Compare(s1) >= 0; }
bool AFXAPI operator>=(const String& s1, const char * s2)
{ return s1.Compare(s2) >= 0; }
bool AFXAPI operator>=(const char * s1, const String& s2)
{ return s2.Compare(s1) <= 0; }

bool AFXAPI operator==(const String& s1, const String::Char *s2)
{ return s1==String(s2); }
bool AFXAPI operator==(const String::Char * s1, const String& s2)
{ return String(s1)==s2; }
bool AFXAPI operator!=(const String& s1, const String::Char *s2)
{ return !(s1==s2); }
bool AFXAPI operator!=(const String::Char * s1, const String& s2)
{ return !(s1==s2); }
bool AFXAPI operator<(const String& s1, const String::Char *s2)
{ return s1.Compare(s2) < 0; }
bool AFXAPI operator<(const String::Char * s1, const String& s2)
{ return s2.Compare(s1) > 0; }
bool AFXAPI operator>(const String& s1, const String::Char *s2)
{ return s1.Compare(s2) > 0; }
bool AFXAPI operator>(const String::Char * s1, const String& s2)
{ return s2.Compare(s1) < 0; }
bool AFXAPI operator<=(const String& s1, const String::Char *s2)
{ return s1.Compare(s2) <= 0; }
bool AFXAPI operator<=(const String::Char * s1, const String& s2)
{ return s2.Compare(s1) >= 0; }
bool AFXAPI operator>=(const String& s1, const String::Char *s2)
{ return s1.Compare(s2) >= 0; }
bool AFXAPI operator>=(const String::Char * s1, const String& s2)
{ return s2.Compare(s1) <= 0; }



} // Ext::

#if !UCFG_STDSTL || UCFG_WCE

namespace std {

wchar_t AFXAPI toupper(wchar_t ch, const locale& loc) {
#if UCFG_WDM
	return RtlUpcaseUnicodeChar(ch);
#else
	return (wchar_t)(DWORD_PTR)::CharUpperW(LPWSTR(ch));
#endif
}

wchar_t AFXAPI tolower(wchar_t ch, const locale& loc) {
#if UCFG_WDM
	WCHAR dch;
	UNICODE_STRING dest = { 2, 2, &dch },
		src = { 2, 2, &ch };
	NTSTATUS st = RtlDowncaseUnicodeString(&dest, &src, FALSE);
	if (!NT_SUCCESS(st))
		Throw(st);
	return dch;
	//!!!	return RtlDowncaseUnicodeChar(ch);  // not supported in XP ntoskrnl.exe
#else
	return (wchar_t)(DWORD_PTR)::CharLowerW(LPWSTR(ch));
#endif
}

#ifdef WIN32
bool AFXAPI islower(wchar_t ch, const locale& loc) {
	return IsCharLowerW(ch);
}

bool AFXAPI isupper(wchar_t ch, const locale& loc) {
	return IsCharUpperW(ch);
}

bool AFXAPI isalpha(wchar_t ch, const locale& loc) {
	return IsCharAlphaW(ch);
}
#endif // WIN32

} // std::

#endif


#if UCFG_WIN32

class ThreadStrings {
public:
	std::mutex Mtx;

	typedef std::unordered_map<DWORD, Ext::String> CThreadMap;
	CThreadMap Map;

	ThreadStrings() {

	}
};

static ThreadStrings *s_threadStrings;

static bool InitTreadStrings() {
	s_threadStrings = new ThreadStrings;
	return true;
}

extern "C" const char16_t * AFXAPI Utf8ToUtf16String(const char *utf8) {
	static std::once_flag once;
	std::call_once(once, &InitTreadStrings);

	const char16_t *r = 0;
	EXT_LOCK (s_threadStrings->Mtx) {
		if (s_threadStrings->Map.size() > 256) {
			for (ThreadStrings::CThreadMap::iterator it=s_threadStrings->Map.begin(), e=s_threadStrings->Map.end(); it!=e;) {
				if (HANDLE h = ::OpenThread(0, FALSE, it->first)) {
					::CloseHandle(h);
					++it;
				} else {
					s_threadStrings->Map.erase(it++);
				}
			}
		}
		r = (const char16_t*)(const wchar_t*)(s_threadStrings->Map[::GetCurrentThreadId()] = Ext::String(utf8));	
	}
	return r;
}



#endif // UCFG_WIN32

 