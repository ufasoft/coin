#pragma once


#if UCFG_USE_POSIX
#	ifdef __FreeBSD__
#		include <sys/endian.h>
#	else
#		include <byteswap.h>
#	endif

#	include <unistd.h>
#	include <iconv.h>
#	include <dlfcn.h>
#endif

#if UCFG_USE_PTHREADS

#	if WIN32
#		define HAVE_CONFIG_H 0
#		define  HAVE_SIGNAL_H 1
#		define  pthread_create STDCALL_pthread_create
#	endif

#	include <sched.h>
#	include <pthread.h>


#	if WIN32
#		undef  pthread_create

PTW32_DLLPORT int PTW32_CDECL pthread_create (pthread_t * tid,
											  const pthread_attr_t * attr,
											  void *(__cdecl *start) (void *),
											  void *arg);


#	endif

#endif


namespace Ext {

ENUM_CLASS(Endian) {
	Big,
	Little
} END_ENUM_CLASS(Endian);

__forceinline UInt16	htole(UInt16 v) { return htole16(v); }
__forceinline Int16		htole(Int16 v) { return Int16(htole16(UInt16(v))); }
__forceinline UInt32	htole(UInt32 v) { return htole32(v); }
__forceinline Int32		htole(Int32 v) { return Int32(htole32(UInt32(v))); }
__forceinline UInt64	htole(UInt64 v) { return htole64(v); }
__forceinline Int64		htole(Int64 v) { return Int64(htole64(UInt64(v))); }
__forceinline UInt16	letoh(UInt16 v) { return le16toh(v); }
__forceinline Int16		letoh(Int16 v) { return Int16(le16toh(UInt16(v))); }
__forceinline UInt32	letoh(UInt32 v) { return le32toh(v); }
__forceinline Int32		letoh(Int32 v) { return Int32(le32toh(UInt32(v))); }
__forceinline UInt64	letoh(UInt64 v) { return le64toh(v); }
__forceinline Int64		letoh(Int64 v) { return Int64(le64toh(UInt64(v))); }

#if UCFG_SEPARATE_LONG_TYPE
__forceinline long			letoh(long v) { return letoh(int_presentation<sizeof(long)>::type(v)); }
__forceinline unsigned long letoh(unsigned long v) { return letoh(int_presentation<sizeof(unsigned long)>::type(v)); }
__forceinline long			htole(long v) { return htole(int_presentation<sizeof(long)>::type(v)); }
__forceinline unsigned long htole(unsigned long v) { return htole(int_presentation<sizeof(unsigned long)>::type(v)); }
#endif

inline float htole(float v) {
	int_presentation<sizeof(float)>::type vp = htole((int_presentation<sizeof(float)>::type&)(v));
	return (float&)(vp);
}

inline double htole(double v) {
	int_presentation<sizeof(double)>::type vp = htole((int_presentation<sizeof(double)>::type&)(v));
	return (double&)(vp);
}

inline float letoh(float v) {
	int_presentation<sizeof(float)>::type vp = letoh((int_presentation<sizeof(float)>::type&)(v));
	return (float&)(vp);
}

inline double letoh(double v) {
	int_presentation<sizeof(double)>::type vp = letoh((int_presentation<sizeof(double)>::type&)(v));
	return (double&)(vp);
}

inline UInt32 htobe(UInt32 v) { return htobe32(v); }
inline UInt16 htobe(UInt16 v) { return htobe16(v); }
inline UInt32 betoh(UInt32 v) { return be32toh(v); }
inline UInt16 betoh(UInt16 v) { return be16toh(v); }
inline UInt64 htobe(UInt64 v) { return htobe64(v); }
inline UInt64 betoh(UInt64 v) { return be64toh(v); }

template <typename T>
class BeInt {
public:
	BeInt(T v = 0)
		:	m_val(htobe(v))
	{}

	operator T() const { return betoh(m_val); }

	BeInt& operator=(T v) {
		m_val = htobe(v);
		return *this;
	}
private:
	T m_val;
};

typedef BeInt<UInt16> BeUInt16;
typedef BeInt<UInt32> BeUInt32;
typedef BeInt<UInt64> BeUInt64;


inline std::ostream& AFXAPI operator<<(std::ostream& os, const String& s) {
	return os << (const char*)s;
}

inline std::wostream& AFXAPI operator<<(std::wostream& os, const String& s) {
	return os << (std::wstring)explicit_cast<std::wstring>(s);
}

/*!!!
inline std::istream& AFXAPI operator>>(std::istream& is, String& s) {
	std::string bs;
	is >> bs;
	s = bs;
	return is;
}*/

inline std::istream& AFXAPI getline(std::istream& is, String& s, char delim = '\n') {
	std::string bs;
	getline(is, bs, delim);
	s = bs;
	return is;
}

} // Ext::


__BEGIN_DECLS

inline uint16_t AFXAPI GetLeUInt16(const void *p) { return le16toh(*(const uint16_t UNALIGNED *)p); }
inline uint32_t AFXAPI GetLeUInt32(const void *p) { return le32toh(*(const uint32_t UNALIGNED *)p); }
inline uint64_t AFXAPI GetLeUInt64(const void *p) { return le64toh(*(const uint64_t UNALIGNED *)p); }
inline uint64_t AFXAPI GetBeUInt64(const void *p) { return be64toh(*(const uint64_t UNALIGNED *)p); }

inline void AFXAPI PutLeUInt16(void *p, uint16_t v) { *(uint16_t UNALIGNED *)p = htole16(v); }
inline void AFXAPI PutLeUInt32(void *p, uint32_t v) { *(uint32_t UNALIGNED *)p = htole32(v); }
inline void AFXAPI PutLeUInt64(void *p, uint64_t v) { *(uint64_t UNALIGNED *)p = htole64(v); }

__END_DECLS


namespace Ext {

UInt64 AFXAPI Read7BitEncoded(const byte *&p);
void AFXAPI Write7BitEncoded(byte *&p, UInt64 v);

class Convert {
public:
	static AFX_API Blob AFXAPI FromBase64String(RCString s);
	static AFX_API String AFXAPI ToBase64String(const ConstBuf& mb);
	static inline Int32 AFXAPI ToInt32(bool b) { return Int32(b); }
	static AFX_API UInt32 AFXAPI ToUInt32(RCString s, int fromBase = 10);
	static AFX_API UInt64 AFXAPI ToUInt64(RCString s, int fromBase = 10);
	static AFX_API Int64 AFXAPI ToInt64(RCString s, int fromBase = 10);
	static AFX_API UInt16 AFXAPI ToUInt16(RCString s, int fromBase = 10);
	static AFX_API byte AFXAPI ToByte(RCString s, int fromBase = 10);
	static AFX_API Int32 AFXAPI ToInt32(RCString s, int fromBase = 10);
	static AFX_API String AFXAPI ToString(Int64 v, int base = 10);
	static AFX_API String AFXAPI ToString(UInt64 v, int base = 10);
	static AFX_API String AFXAPI ToString(Int64 v, const char *format);
	//!!!	static String AFXAPI ToString(size_t v, int base = 10) { return ToString(UInt64(v), base); }
	static String AFXAPI ToString(Int32 v, int base = 10) { return ToString(Int64(v), base); }
	static String AFXAPI ToString(UInt32 v, int base = 10) { return ToString(UInt64(v), base); }
#if	UCFG_SEPARATE_INT_TYPE
	static String AFXAPI ToString(int v, int base = 10) { return ToString(Int64(v), base); }
	static String AFXAPI ToString(unsigned int v, int base = 10) { return ToString(UInt64(v), base); }
#endif
#if	UCFG_SEPARATE_LONG_TYPE
	static String AFXAPI ToString(long v, int base = 10) { return ToString(Int64(v), base); }
	static String AFXAPI ToString(unsigned long v, int base = 10) { return ToString(UInt64(v), base); }
#endif
	static String AFXAPI ToString(Int16 v, int base = 10) { return ToString(Int32(v), base); }
	static String AFXAPI ToString(UInt16 v, int base = 10) { return ToString(UInt32(v), base); }
	static String AFXAPI ToString(double d);
#ifdef WIN32
	static AFX_API Blob AFXAPI ToAnsiBytes(wchar_t ch);	
#endif

#if UCFG_COM
	static AFX_API Int32 AFXAPI ToInt32(const VARIANT& v);
	static AFX_API Int64 AFXAPI ToInt64(const VARIANT& v);
	static AFX_API double AFXAPI ToDouble(const VARIANT& v);
	static AFX_API String AFXAPI ToString(const VARIANT& v);
	static AFX_API bool AFXAPI ToBoolean(const VARIANT& v);
#endif
};

class MemStreamWithPosition : public Stream {
public:
	MemStreamWithPosition()
		:	m_pos(0)
	{}

	UInt64 get_Position() const override {
		return m_pos;
	}
	
	void put_Position(UInt64 pos) const override {
		m_pos = (size_t)pos;
	}

	Int64 Seek(Int64 offset, SeekOrigin origin) const override {
		switch (origin) {
		case SeekOrigin::Begin: put_Position(offset); break;
		case SeekOrigin::Current: put_Position(m_pos + offset); break;
		case SeekOrigin::End: put_Position(Length+offset); break;
		}
		return m_pos;
	}

protected:
	mutable size_t m_pos;
};

class CMemReadStream : public MemStreamWithPosition {
public:
	ConstBuf m_mb;

	CMemReadStream(const ConstBuf& mb)
		:	m_mb(mb)
	{
	}

	UInt64 get_Length() const override { return m_mb.Size; }
	bool Eof() const override { return m_pos == m_mb.Size; }

	void put_Position(UInt64 pos) const override {
		if (pos > m_mb.Size)
			Throw(E_FAIL);
		m_pos = (size_t)pos;
	}

	void ReadBufferAhead(void *buf, size_t count) const override;
	size_t Read(void *buf, size_t count) const override;
	void ReadBuffer(void *buf, size_t count) const override;
	int ReadByte() const override;
};

class CBlobReadStream : public CMemReadStream {
	typedef CMemReadStream base;
public:
	CBlobReadStream(const Blob& blob)
		:	base(blob)
		,	m_blob(blob)						// m_mb now points to the same buffer as m_blob
	{}
private:
	const Blob m_blob;
};

class CMemWriteStream : public MemStreamWithPosition {
public:
	Buf m_mb;

	CMemWriteStream(const Buf& mb)
		:	m_mb(mb)
	{
	}

	UInt64 get_Length() const override { return m_mb.Size; }
	bool Eof() const override { return m_pos == m_mb.Size; }

	void put_Position(UInt64 pos) const override {
		if (pos > m_mb.Size)
			Throw(E_FAIL);
		m_pos = (size_t)pos;
	}

	void WriteBuffer(const void *buf, size_t count) override {
		if (count >m_mb.Size-m_pos)
			Throw(E_EXT_EndOfStream);
		memcpy(m_mb.P+m_pos, buf, count);
		m_pos += count;
	}
};

class EXTAPI MemoryStream : public MemStreamWithPosition {
public:
	typedef MemoryStream class_type;

	MemoryStream(size_t capacity = 0)
		:	m_blob(0, capacity)
	{}

	void WriteBuffer(const void *buf, size_t count) override;
	bool Eof() const override;
	void Reset(size_t cap = 256);

	operator ConstBuf() const { return ConstBuf(m_blob.constData(), (size_t)m_pos); }
	operator Ext::Blob() const { return Ext::Blob(m_blob.constData(), (size_t)m_pos); }

	class Blob get_Blob();
	DEFPROP_GET(class Blob, Blob);

	size_t get_Capacity() const { return m_blob.Size; }
	DEFPROP_GET(size_t, Capacity);
private:
	Ext::Blob m_blob;
};

AFX_API String AFXAPI operator+(const String& s, const char *lpsz);		// friend declaration in the "class String" is not enough

inline String AFXAPI operator+(const String& s, unsigned char ch) {
	return s+String((char)ch);
}

inline std::ostream& operator<<(std::ostream& os, const CStringVector& ar) {
	for (size_t i=0; i<ar.size(); i++)
		os << ar[i] << '\n';
	return os;
}

inline String AFXAPI operator+(const char *p, const String& s) {
	return String(p)+s;
}

inline String AFXAPI operator+(const String::Char *p, const String& s) {
	return String(p)+s;
}


class CIosStream : public Stream {
	typedef CIosStream class_type;
public:
	CIosStream(std::istream& ifs)
		:	m_pis(&ifs)
		,	m_pos(0)
	{}

	CIosStream(std::ostream& ofs)
		:	m_pos(&ofs)
		,	m_pis(0)
	{}

	size_t Read(void *buf, size_t count) const override {
		m_pis->read((char*)buf, (std::streamsize)count);
		if (!*m_pis)
			Throw(E_EXT_NoInputStream);
		return (size_t)m_pis->gcount();
	}

	void ReadBuffer(void *buf, size_t count) const override {
		m_pis->read((char*)buf, (std::streamsize)count);
		if (!*m_pis)
			Throw(E_EXT_NoInputStream);
	}

	void WriteBuffer(const void *buf, size_t count) override {
		m_pos->write((const char*)buf, (std::streamsize)count);
		if (!*m_pos)
			Throw(E_EXT_NoOutputStream);
	}

	UInt64 get_Position() const override {
		return m_pis ? m_pis->tellg() : m_pos->tellp();
	}

	void put_Position(UInt64 pos) const override {
		if (m_pis) {
			m_pis->seekg((long)pos);
			if (!*m_pis)
				Throw(E_EXT_NoInputStream);
		}
		if (m_pos) {
			m_pos->seekp((long)pos);
			if (!*m_pos)
				Throw(E_EXT_NoOutputStream);
		}
	}

	bool Eof() const override {
		return m_pis->eof();
	}

	void Flush() override {
		if (m_pos)
			m_pos->flush();
	}
protected:
	std::istream *m_pis;
	std::ostream *m_pos;

	CIosStream()
		:	m_pis(0)
		,	m_pos(0)
	{}
};

class StringInputStream : public CIosStream {
public:
	StringInputStream(RCString s)
		:	m_s(s)
		,	m_is(m_s.c_str())
	{
		m_pis = &m_is;
	}
private:
	String m_s;
	std::istringstream m_is;

};

template <typename EL, typename TR>
inline std::basic_ostream<EL, TR>& operator<<(std::basic_ostream<EL, TR>& os, const CPrintable& ob) { return os << ob.ToString(); }


class MacAddress : totally_ordered<MacAddress> {
public:
	UInt64 m_n64;

	MacAddress(const MacAddress& mac)
		:	m_n64(mac.m_n64)
	{}

	explicit MacAddress(Int64 n64 = 0)
		:	m_n64(n64)
	{}

	explicit MacAddress(const ConstBuf& mb) {
		if (mb.Size != 6)
			Throw(E_FAIL);
		m_n64 = *(DWORD*)mb.P | (UInt64(*((UInt16*)mb.P+2)) << 32);
	}

	explicit MacAddress(RCString s);

	operator Blob() const {
		return Blob(&m_n64, 6);
	}

	void CopyTo(void *p) const {
		*(DWORD*)p = (DWORD)m_n64;
		*((UInt16*)p+2) = UInt16(m_n64 >> 32);
	}

	bool operator<(MacAddress mac) const { return m_n64<mac.m_n64; }
	bool operator==(MacAddress mac) const { return m_n64==mac.m_n64; }

	static MacAddress __stdcall Null() { return MacAddress(); }
	static MacAddress __stdcall Broadcast() { return MacAddress(0xFFFFFFFFFFFFLL); }

	String ToString() const;
};

EXT_API std::ostream& __stdcall operator<<(std::ostream& os, const MacAddress& mac);

#if !UCFG_WCE
} // Ext::

namespace EXT_HASH_VALUE_NS {
inline size_t hash_value(const Ext::MacAddress& mac) {
	return std::hash<Ext::UInt64>()(mac.m_n64);
}
}

EXT_DEF_HASH(Ext::MacAddress) 
	
	namespace Ext {
#endif


class UTF8Encoding;

class Encoding : public Object {
public:
	EXT_DATA static Encoding *s_Default;
	EXT_DATA static UTF8Encoding UTF8;	

	static Encoding& AFXAPI Default();

	int CodePage;
	String Name;

	Encoding(int codePage = 0);

	virtual ~Encoding() {
#if UCFG_USE_POSIX
		if ((LONG_PTR)m_iconvTo != -1)
			CCheck(::iconv_close(m_iconvTo));
		if ((LONG_PTR)m_iconvFrom != -1)
			CCheck(::iconv_close(m_iconvFrom));
#endif
	}
	static bool AFXAPI SetThreadIgnoreIncorrectChars(bool v);
	static Encoding* AFXAPI GetEncoding(RCString name);
	static Encoding* AFXAPI GetEncoding(int codepage);
	virtual size_t GetCharCount(const ConstBuf& mb);
	virtual size_t GetChars(const ConstBuf& mb, String::Char *chars, size_t charCount);
	EXT_API virtual std::vector<String::Char> GetChars(const ConstBuf& mb);
	virtual size_t GetByteCount(const String::Char *chars, size_t charCount);
	virtual size_t GetByteCount(RCString s) { return GetByteCount(s, s.Length); }
	virtual size_t GetBytes(const String::Char *chars, size_t charCount, byte *bytes, size_t byteCount);
	virtual Blob GetBytes(RCString s);

	class CIgnoreIncorrectChars {
	public:
		CIgnoreIncorrectChars(bool v = true)
			:	m_prev(SetThreadIgnoreIncorrectChars(v))
		{}

		~CIgnoreIncorrectChars() {
			SetThreadIgnoreIncorrectChars(m_prev);			
		}
	private:
		bool m_prev;
	};
protected:
#if UCFG_WDM
	static bool t_IgnoreIncorrectChars;
#else
	static EXT_THREAD_PTR(Encoding) t_IgnoreIncorrectChars;
#endif

	typedef std::unordered_map<String, ptr<Encoding>> CEncodingMap;
	static CEncodingMap s_encodingMap;

#if UCFG_USE_POSIX
	iconv_t m_iconvTo;
	iconv_t m_iconvFrom;
#endif
};

class UTF8Encoding : public Encoding {
	typedef Encoding base;
public:
	UTF8Encoding()
		:	base(CP_UTF8)
	{}

	Blob GetBytes(RCString s);
	size_t GetBytes(const String::Char *chars, size_t charCount, byte *bytes, size_t byteCount);
	size_t GetCharCount(const ConstBuf& mb);
	EXT_API std::vector<String::Char> GetChars(const ConstBuf& mb);
	size_t GetChars(const ConstBuf& mb, String::Char *chars, size_t charCount);
protected:
	void Pass(const ConstBuf& mb, UnaryFunction<String::Char, bool>& visitor);
	void PassToBytes(const String::Char* pch, size_t nCh, UnaryFunction<byte, bool>& visitor);
};

class ASCIIEncoding : public Encoding {
public:
	Blob GetBytes(RCString s);
	size_t GetBytes(const String::Char *chars, size_t charCount, byte *bytes, size_t byteCount);
	size_t GetCharCount(const ConstBuf& mb);
	EXT_API std::vector<String::Char> GetChars(const ConstBuf& mb);
	size_t GetChars(const ConstBuf& mb, String::Char *chars, size_t charCount);
};

class CodePageEncoding : public Encoding {
public:
	CodePageEncoding(int codePage);
};


#ifndef _MSC_VER
__forceinline UInt32 _byteswap_ulong(UInt32 v) {
#ifdef __FreeBSD__
	return __bswap32(v);
#else
	return __bswap_32(v);
#endif
}

__forceinline UInt16 _byteswap_ushort(UInt16 v) {
#ifdef __FreeBSD__
	return __bswap16(v);
#else
	return __bswap_16(v);
#endif
}
#endif

inline size_t RotlSizeT(size_t v, int shift) {
#if defined(_MSC_VER) && !UCFG_WCE
#	ifdef _WIN64
	return _rotl64(v, shift);
#	else
	return _rotl((UInt32)v, shift);
#	endif
#else
	return shift==0 ? v
					: (v << shift) | (v >> (sizeof(v)*8-shift));
#endif
}

int __cdecl PopCount(unsigned int v);
int __cdecl PopCount(unsigned long long v);

class BitOps {
public:

	static inline bool BitTest(const void *p, int idx) {
#if defined(_MSC_VER) && !UCFG_WCE
#	ifdef _WIN64
		return _bittest64((Int64*)p, idx);
#	else
		return ::_bittest((long*)p, idx);
#	endif
#else
		return ((const byte*)p)[idx >> 3] & (1 << (idx & 7));
#endif
	}

	static inline bool BitTestAndSet(void *p, int idx) {
#if defined(_MSC_VER) && !UCFG_WCE
#	ifdef _WIN64
		return _bittestandset64((Int64*)p, idx);
#	else
		return ::_bittestandset((long*)p, idx);
#	endif
#else
		byte *pb = (byte*)p + (idx >> 3);
		byte mask = byte(1 << (idx & 7));
		byte v = *pb;
		*pb = v | mask;
		return v & mask;
#endif
	}

#if !UCFG_WCE
	static inline int PopCount(UInt32 v) {
#	ifdef _MSC_VER
		int r = 0;								// 	__popcnt() is AMD-specific
		for (int i=0; i<32; ++i)
			r += (v >> i) & 1;
		return r;
#	else
		return __builtin_popcount(v);
#	endif
	}

	static inline int PopCount(UInt64 v) {
#	ifdef _MSC_VER
		int r = 0;								// 	__popcnt() is AMD-specific
		for (int i=0; i<64; ++i)
			r += (v >> i) & 1;
		return r;
#	else
		return __builtin_popcountll(v);
#	endif
	}

	static inline int Scan(UInt32 mask) {
#	ifdef _MSC_VER
		unsigned long index;
		return _BitScanForward(&index, mask) ? index+1 : 0;
#	else
		return __builtin_ffs(mask);
#	endif
	}

	static inline int Scan(UInt64 mask) {
#	ifdef _MSC_VER
#		ifdef _M_X64
			unsigned long index;
			return _BitScanForward64(&index, mask) ? index+1 : 0;
#		else
			int r;
			return !mask ? 0
				: (r = Scan(UInt32(mask))) ? r
				: 32+Scan(UInt32(mask >> 32));
#		endif
#	else
		return __builtin_ffsll(mask);
#	endif
	}
#endif // !UCFG_WCE

	static inline int ScanReverse(UInt32 mask) {
#	ifdef _MSC_VER
		unsigned long index;
		return _BitScanReverse(&index, mask) ? index+1 : 0;
#	else
		return mask==0 ? 0 : 32-__builtin_clz(mask);
#	endif
	}

	static inline int ScanReverse(UInt64 mask) {
#	ifdef _MSC_VER
#		ifdef _M_X64
			unsigned long index;
			return _BitScanReverse64(&index, mask) ? index+1 : 0;
#		else
			int r;
			return !mask ? 0
				: (r = ScanReverse(UInt32(mask >> 32))) ? r+32
				: ScanReverse(UInt32(mask));
#		endif
#	else
		return mask==0 ? 0 : 64-__builtin_clzll(mask);
#	endif
	}

};

typedef void (_cdecl *PFNAtExit)();

class AtExitRegistration {
public:
	AtExitRegistration(PFNAtExit pfn)
		:	m_pfn(pfn)
	{
		RegisterAtExit(m_pfn);
	}
	
	~AtExitRegistration() {
		UnregisterAtExit(m_pfn);
	}
private:
	PFNAtExit m_pfn;
};

class StreamReader {
public:
	Stream& BaseStream;
	Ext::Encoding& Encoding;

	StreamReader(Stream& stm, Ext::Encoding& enc = Ext::Encoding::Default())
		:	BaseStream(stm)
		,	Encoding(enc)
		,	m_prevChar(-1)
	{
	}

	String ReadToEnd();
	EXT_API std::pair<String, bool> ReadLineEx();
	
	String ReadLine() { return ReadLineEx().first; }
private:
	int m_prevChar;

	int ReadChar();
};

class StreamWriter {
public:
	Ext::Encoding& Encoding;
	String NewLine;

	StreamWriter(Stream& stm, Ext::Encoding& enc = Ext::Encoding::Default())
		:	Encoding(enc)
		,	m_stm(stm)
		,	NewLine("\r\n")
	{
	}

	void WriteLine(RCString line);
private:
	Stream& m_stm;
};

unsigned int MurmurHashAligned2(const ConstBuf& cbuf, UInt32 seed);
UInt32 MurmurHash3_32(const ConstBuf& cbuf, UInt32 seed);

} // Ext::


