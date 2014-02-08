/*######     Copyright (c) 1997-2013 Ufasoft  http://ufasoft.com  mailto:support@ufasoft.com,  Sergey Pavlov  mailto:dev@ufasoft.com #######################################
#                                                                                                                                                                          #
# This program is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation;  #
# either version 3, or (at your option) any later version. This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the      #
# implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details. You should have received a copy of the GNU #
# General Public License along with this program; If not, see <http://www.gnu.org/licenses/>                                                                               #
##########################################################################################################################################################################*/

#include <el/ext.h>

#if UCFG_WIN32
#	undef NONLS
#	include <mlang.h>

#	include <el/libext/win32/ext-win.h>
#	if UCFG_COM
#		include <el/libext/win32/ext-com.h>
#	endif
#endif

#pragma warning(disable: 4073)
#pragma init_seg(lib)				// to initialize UTF8 early

/*!!!R
#if UCFG_WIN32
extern "C" {
	const GUID CLSID_CMultiLanguage = __uuidof(CMultiLanguage);
}
#endif */
	
namespace Ext {
using namespace std;



#if UCFG_WDM
class AnsiPageEncoding : public Encoding {
public:
	size_t GetBytes(const wchar_t *chars, size_t charCount, byte *bytes, size_t byteCount) {
		ANSI_STRING as = { (USHORT)byteCount, (USHORT)byteCount, (PCHAR)bytes };
		UNICODE_STRING us = { charCount*sizeof(wchar_t), charCount*sizeof(wchar_t), (PWCH)chars };
		NTSTATUS st = RtlUnicodeStringToAnsiString(&as, &us, FALSE);
		if (NT_SUCCESS(st))
			return as.Length;
		return 0;
	}
};

#else
ASSERT_CDECL;
#endif


Encoding *Encoding::s_Default;
UTF8Encoding Encoding::UTF8;

#if UCFG_WDM
bool Encoding::t_IgnoreIncorrectChars;
#else
EXT_THREAD_PTR(Encoding, Encoding::t_IgnoreIncorrectChars);
#endif

mutex m_csEncodingMap;
Encoding::CEncodingMap Encoding::s_encodingMap;

Encoding& AFXAPI Encoding::Default() {
	if (!s_Default) {
#if UCFG_CODEPAGE_UTF8
		static UTF8Encoding s_defaultEncoding;
#elif UCFG_WDM
		static AnsiPageEncoding s_defaultEncoding;
#else
		static CodePageEncoding s_defaultEncoding(CP_ACP);
#endif
		s_Default = &s_defaultEncoding;
	}
	return *s_Default;
}

Encoding::Encoding(int codePage)
	:	CodePage(codePage)
#if UCFG_USE_POSIX
	,	m_iconvTo((iconv_t)-1)
	,	m_iconvFrom((iconv_t)-1)
#endif
{
#if UCFG_USE_POSIX
	if (CodePage) {
		char name[100];
		switch (CodePage) {
			case CP_UTF8:	strcpy(name, "UTF-8"); break;
			case CP_UTF7:	strcpy(name, "UTF-7"); break;
			default:		sprintf(name, "CP%d", (int)CodePage);
		}
		Name = String(name, 0, strlen(name), nullptr);

		const char *unicodeCP = sizeof(String::Char)==4 ? "UTF-32" : "UTF-16";
		m_iconvTo = ::iconv_open(name, unicodeCP);
		CCheck((LONG_PTR)m_iconvTo == -1 ? -1 : 0);
		m_iconvFrom = ::iconv_open(unicodeCP, name);
		CCheck((LONG_PTR)m_iconvFrom == -1 ? -1 : 0);

		GetCharCount(ConstBuf("ABC", 3));			// to skip encoding prefixes;
	}
#endif
}

bool Encoding::SetThreadIgnoreIncorrectChars(bool v) {
	bool prev = t_IgnoreIncorrectChars;
	t_IgnoreIncorrectChars = v ? &UTF8 : nullptr;
	return prev;
}

Encoding *Encoding::GetEncoding(RCString name) {
	String upper = name.ToUpper();
	ptr<Encoding> r;

	CScopedLock<mutex> lck = ScopedLock(m_csEncodingMap);

	if (!Lookup(s_encodingMap, upper, r)) {		
		if (upper == "UTF-8")
			r = new UTF8Encoding;
		else if (upper == "ASCII")
			r = new ASCIIEncoding;
		else if (upper.Left(2) == "CP") {
			int codePage = atoi(upper.Mid(2));
			r = new CodePageEncoding(codePage);
		} else {
#if UCFG_COM
			CUsingCOM usingCOM;
			MIMECSETINFO mi;
			OleCheck(CComPtr<IMultiLanguage>(CreateComObject(__uuidof(CMultiLanguage)))->GetCharsetInfo(name.Bstr, &mi));
			r = new CodePageEncoding(mi.uiCodePage);
#else
			Throw(E_EXT_EncodingNotSupported);
#endif
		}
		s_encodingMap[upper] = r;
	}
	return r.get();
}

Encoding *Encoding::GetEncoding(int codepage) {
	return GetEncoding("CP" + Convert::ToString(codepage));
}

#ifdef __FreeBSD__
	typedef const char *ICONV_SECOND_TYPE;
#else
	typedef char *ICONV_SECOND_TYPE;
#endif

size_t Encoding::GetCharCount(const ConstBuf& mb) {
	if (0 == mb.Size)
		return 0;
#if UCFG_USE_POSIX
	const char *sp = (char*)mb.P;
	int r = 0;	
	for (size_t len=mb.Size; len;) {
		char buf[40] = { 0 };
		char *dp = buf;
		size_t slen = std::min(len, (size_t)6);
		size_t dlen = sizeof(buf);
		CCheck(::iconv(m_iconvFrom, (ICONV_SECOND_TYPE*)&sp, &slen, &dp, &dlen), E2BIG);
		len = mb.Size-(sp-(const char*)mb.P);
		r += (sizeof(buf)-dlen)/sizeof(String::Char);
	}
	return r;
#elif !UCFG_WDM
	::SetLastError(0);
	int r = ::MultiByteToWideChar(CodePage, 0, (LPCSTR)mb.P, mb.Size, 0, 0);
	Win32Check(r, 0);
	return r;
#else
	return 0; //!!!!
#endif
}

size_t Encoding::GetChars(const ConstBuf& mb, String::Char *chars, size_t charCount) {
#if UCFG_USE_POSIX
	const char *sp = (char*)mb.P;
	size_t len = mb.Size;
	char *dp = (char*)chars;
	size_t dlen = charCount*sizeof(String::Char);
	CCheck(::iconv(m_iconvFrom, (ICONV_SECOND_TYPE*)&sp, &len, &dp, &dlen));
	return (charCount*sizeof(String::Char)-dlen)/sizeof(String::Char);
#elif defined(WIN32)
	::SetLastError(0);
	int r = ::MultiByteToWideChar(CodePage, 0, (LPCSTR)mb.P, mb.Size, chars, charCount);
	Win32Check(r, 0);
	return r;
#else
	return 0; //!!!
#endif
}

vector<String::Char> Encoding::GetChars(const ConstBuf& mb) {
	vector<String::Char> vec(GetCharCount(mb));
	if (vec.size()) {
		size_t n = GetChars(mb, &vec[0], vec.size());
		ASSERT(n == vec.size());
	}
	return vec;
}

size_t Encoding::GetByteCount(const String::Char *chars, size_t charCount) {
	if (0 == charCount)
		return 0;
#if UCFG_USE_POSIX
	const char *sp = (char*)chars;
	int r = 0;	
	for (size_t len=charCount*sizeof(String::Char); len;) {
		char buf[40];
		char *dp = buf;
		size_t dlen = sizeof(buf);
		CCheck(::iconv(m_iconvTo, (ICONV_SECOND_TYPE*)&sp, &len, &dp, &dlen), E2BIG);
		r += (sizeof(buf)-dlen);
	}
	return r;
#elif defined (WIN32)
	::SetLastError(0);
	int r = ::WideCharToMultiByte(CodePage, 0, chars, charCount, 0, 0, 0, 0);
	Win32Check(r, 0);
	return r;
#else
	return 0; //!!!
#endif
}

size_t Encoding::GetBytes(const String::Char *chars, size_t charCount, byte *bytes, size_t byteCount) {
#if UCFG_USE_POSIX
	const char *sp = (char*)chars;
	size_t len = charCount*sizeof(String::Char);
	char *dp = (char*)bytes;
	size_t dlen = byteCount;
	CCheck(::iconv(m_iconvTo, (ICONV_SECOND_TYPE*)&sp, &len, &dp, &dlen));
	return byteCount-dlen;
#elif defined(WIN32)
	::SetLastError(0);
	int r = ::WideCharToMultiByte(CodePage, 0, chars, charCount, (LPSTR)bytes, byteCount, 0, 0);
	Win32Check(r, ERROR_INSUFFICIENT_BUFFER);
	return r;
#else
	return 0;
#endif
}

Blob Encoding::GetBytes(RCString s) {
	Blob blob(0, GetByteCount(s));
	size_t n = GetBytes(s, s.Length, blob.data(), blob.Size);
	ASSERT(n == blob.Size);
	return blob;
}

void UTF8Encoding::Pass(const ConstBuf& mb, UnaryFunction<String::Char, bool>& visitor) {
#ifdef X_DEBUG//!!!D
	cout.write((char*)mb.P, mb.Size);
#endif
	size_t len = mb.Size;
	for (const byte *p=mb.P; len--;) {
		byte b = *p++;
		int n = 0;
		if (b >= 0xFE) {
			if (!t_IgnoreIncorrectChars)
				Throw(E_EXT_InvalidUTF8String);
			n = 1;
			b = '?';			
		}
		else if (b >= 0xFC)
			n = 5;
		else if (b >= 0xF8)
			n = 4;
		else if (b >= 0xF0)
			n = 3;
		else if (b >= 0xE0)
			n = 2;
		else if (b >= 0xC0)
			n = 1;
		else if (b >= 0x80) {
			if (!t_IgnoreIncorrectChars)
				Throw(E_EXT_InvalidUTF8String);
			n = 1;
			b = '?';			
		}
		String::Char wc = String::Char(b & ((1<<(7-n))-1));
		while (n--) {
			if (!len--) {
				if (!t_IgnoreIncorrectChars)
					Throw(E_EXT_InvalidUTF8String);
				++len;
				wc = '?';
				break;
			}
			b = *p++;
			if ((b & 0xC0) != 0x80) {
				if (!t_IgnoreIncorrectChars)
					Throw(E_EXT_InvalidUTF8String);
				wc = '?';
				break;
			} else
				wc = (wc<<6) | (b&0x3F);
		}
		if (!visitor(wc))
			break;
	}
}

void UTF8Encoding::PassToBytes(const String::Char* pch, size_t nCh, UnaryFunction<byte, bool>& visitor) {
	for (size_t i=0; i<nCh; i++) {
		String::Char wch = pch[i];
		UInt32 ch = wch; 					// may be 32-bit chars in the future
		if (ch < 0x80) {
			if (!visitor(byte(ch)))
				break;
		} else {
			int n = 5;
			if (ch >= 0x4000000) {
				if (!visitor(byte(0xFC | (ch>>30))))
					break;
			} else if (ch >= 0x200000) {
				if (!visitor(byte(0xF8 | (ch>>24))))
					break;
				n = 4;
			} else if (ch >= 0x10000) {
				if (!visitor(byte(0xF0 | (ch>>18))))
					break;
				n = 3;
			} else if (ch >= 0x800) {
				if (!visitor(byte(0xE0 | (ch>>12))))
					break;
				n = 2;
			} else {
				if (!visitor(byte(0xC0 | (ch>>6))))
					break;
				n = 1;
			}
			while (n--)
				if (!visitor(byte(0x80 | ((ch>>(n*6))&0x3F))))
					return;
		}
	}
}

Blob UTF8Encoding::GetBytes(RCString s) {
	struct Visitor : UnaryFunction<byte, bool> {
		MemoryStream ms;
		BinaryWriter wr;
		
		Visitor()
			:	wr(ms)
		{}

		bool operator()(byte b) {
			wr << b;
			return true;
		}
	} v;
	PassToBytes(s, s.Length, v);
	return v.ms.Blob;
}

size_t UTF8Encoding::GetBytes(const String::Char *chars, size_t charCount, byte *bytes, size_t byteCount) {
	struct Visitor : UnaryFunction<byte, bool> {
		size_t m_count;
		byte *m_p;

		bool operator()(byte ch) {
			if (m_count <= 0)
				return false;
			*m_p++ = ch;
			--m_count;
			return true;			
		}
	} v;
	v.m_count = byteCount;
	v.m_p = bytes;
	PassToBytes(chars, charCount, v);
	return v.m_p-bytes;
}

size_t UTF8Encoding::GetCharCount(const ConstBuf& mb) {
	struct Visitor : UnaryFunction<String::Char, bool> {
		size_t count;

		Visitor()
			:	count(0)
		{}

		bool operator()(String::Char ch) {
			++count;
			return true;
		}
	} v;
	Pass(mb, v);
	return v.count;
}

std::vector<String::Char> UTF8Encoding::GetChars(const ConstBuf& mb) {
	struct Visitor : UnaryFunction<String::Char, bool> {
		vector<String::Char> ar;

		bool operator()(String::Char ch) {
			ar.push_back(ch);
			return true;
		}
	} v;
	Pass(mb, v);
	return v.ar;
}

size_t UTF8Encoding::GetChars(const ConstBuf& mb, String::Char *chars, size_t charCount) {
	struct Visitor : UnaryFunction<String::Char, bool> {
		size_t m_count;
		String::Char *m_p;

		bool operator()(String::Char ch) {
			if (m_count <= 0)
				return false;
			*m_p++ = ch;
			--m_count;
			return true;			
		}
	} v;
	v.m_count = charCount;
	v.m_p = chars;
	Pass(mb, v);
	return v.m_p-chars;
}


Blob ASCIIEncoding::GetBytes(RCString s) {
	Blob blob(nullptr, s.Length);
	for (size_t i=0; i<s.Length; ++i)
		blob.data()[i] = (byte)s[i];
	return blob;
}

size_t ASCIIEncoding::GetBytes(const String::Char *chars, size_t charCount, byte *bytes, size_t byteCount) {
	size_t r = std::min(charCount, byteCount);
	for (size_t i=0; i<r; ++i)
		bytes[i] = (byte)chars[i];
	return r;
}

size_t ASCIIEncoding::GetCharCount(const ConstBuf& mb) {
	return mb.Size;
}

std::vector<String::Char> ASCIIEncoding::GetChars(const ConstBuf& mb) {
	vector<String::Char> r(mb.Size);
	for (size_t i=0; i<mb.Size; ++i)
		r[i] = mb.P[i];
	return r;
}

size_t ASCIIEncoding::GetChars(const ConstBuf& mb, String::Char *chars, size_t charCount) {
	size_t r = std::min(charCount, mb.Size);
	for (size_t i=0; i<r; ++i)
		chars[i] = mb.P[i];
	return r;
}


CodePageEncoding::CodePageEncoding(int codePage)
	:	Encoding(codePage)
{
}



} // Ext::

