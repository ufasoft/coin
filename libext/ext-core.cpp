/*######     Copyright (c) 1997-2013 Ufasoft  http://ufasoft.com  mailto:support@ufasoft.com,  Sergey Pavlov  mailto:dev@ufasoft.com #######################################
#                                                                                                                                                                          #
# This program is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation;  #
# either version 3, or (at your option) any later version. This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the      #
# implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details. You should have received a copy of the GNU #
# General Public License along with this program; If not, see <http://www.gnu.org/licenses/>                                                                               #
##########################################################################################################################################################################*/

#include <el/ext.h>

#if UCFG_WIN32
#	include <windows.h>
#endif

using namespace std;
using namespace Ext;

namespace Ext { 

int __cdecl PopCount(unsigned int v) {
	return BitOps::PopCount(v);
}

int __cdecl PopCount(unsigned long long v) {
	return BitOps::PopCount(UInt64(v));
}

const unsigned char *ConstBuf::Find(const ConstBuf& mb) const {
	for (const unsigned char *p=P, *e=p+(Size-mb.Size); p <= e;) {
		if (const unsigned char *q = (const unsigned char*)memchr(p, mb.P[0], e-p+1)) {
			if (mb.Size==1 || !memcmp(q+1, mb.P+1, mb.Size-1))
				return q;
			else
				p = q+1;
		} else
			break;
	}
	return 0;
}


#ifdef WIN32

Blob Convert::ToAnsiBytes(wchar_t ch) {
	int n = WideCharToMultiByte(CP_ACP, 0, &ch, 1, 0, 0, 0, 0);
	Blob blob(0, n);
	WideCharToMultiByte(CP_ACP, 0, &ch, 1, (LPSTR)blob.data(), n, 0, 0);
	return blob;
}

#endif

static void ImpIntToStr(char *buf, UInt64 v, int base) {
	char *q = buf;
	do {
		int d = int(v % base);
		*q++ = char(d<=9 ? d+'0' : d-10+'a');
	}
	while (v /= base);
	*q = 0;
	reverse(buf, q);
}

String Convert::ToString(Int64 v, int base) {
	char buf[66];
	char *p = buf;
	if (v < 0) {
		*p++ = '-';
		v = -v;
	}
	ImpIntToStr(p, v, base);
	return buf;
}

String Convert::ToString(UInt64 v, int base) {
	char buf[66];
	ImpIntToStr(buf, v, base);
	return buf;
}

String Convert::ToString(Int64 v, const char *format) {
	char buf[100];
	char sformat[100];
	char typ = *format;
	switch (typ) {
	case 'D':
		typ = 'd';
	case 'd':
	case 'x':
	case 'X':
		if (format[1])
			sprintf(sformat, "%%%s.%s" EXT_LL_PREFIX "%c", format+1, format+1, typ);
		else
			sprintf(sformat, "%%" EXT_LL_PREFIX "%c", typ);
		sprintf(buf, sformat, v);
		return buf;
	default:
		Throw(E_FAIL);
	}
}

UInt64 Convert::ToUInt64(RCString s, int fromBase) {
	UInt64 r = 0;
	const String::Char *p=s;
	for (String::Char ch; (ch = *p); ++p) {
		int n;
		if (ch>='0' && ch<='9')
			n = ch-'0';
		else if (ch>='A' && ch<='Z')
			n = ch-'A'+10;
		else if (ch>='a' && ch<='z')
			n = ch-'a'+10;
		else
			Throw(E_FAIL);
		if (n >= fromBase)
			Throw(E_FAIL);
		r = (r * fromBase) + n;
	}
	return r;
}

Int64 Convert::ToInt64(RCString s, int fromBase) {
	return s[0] == '-' ? -(Int64)ToUInt64(s.Substring(1), fromBase) : ToUInt64(s, fromBase);
}

template <typename T>
T CheckBounds(Int64 v) {
	if (v > numeric_limits<T>::max() || v < numeric_limits<T>::min())
		Throw(E_EXT_Overflow);
	return (T)v;
}

template <typename T>
T CheckBounds(UInt64 v) {
	if (v > numeric_limits<T>::max())
		Throw(E_EXT_Overflow);
	return (T)v;
}

UInt32 Convert::ToUInt32(RCString s, int fromBase) {
	return CheckBounds<UInt32>(ToUInt64(s, fromBase));
}

UInt16 Convert::ToUInt16(RCString s, int fromBase) {
	return CheckBounds<UInt16>(ToUInt64(s, fromBase));
}

byte Convert::ToByte(RCString s, int fromBase) {
	return CheckBounds<byte>(ToUInt64(s, fromBase));
}

Int32 Convert::ToInt32(RCString s, int fromBase) {
	return CheckBounds<Int32>(ToInt64(s, fromBase));
}

#if !UCFG_WDM
String Convert::ToString(double d) {
	char buf[40];
	sprintf(buf, "%g", d);
	return buf;
}
#endif

MacAddress::MacAddress(RCString s)
	:	m_n64(0)
{
	vector<String> ar = s.Split(":-");
	if (ar.size() != 6)
		Throw(E_FAIL);
	byte *p = (byte*)&m_n64;
	for (size_t i=0; i<ar.size(); i++)
		*p++ = (byte)Convert::ToUInt32(ar[i], 16);
}

String MacAddress::ToString() const {
	ostringstream os;
	os << _self;
	return os.str();
}

ostream& AFXAPI operator<<(ostream& os, const MacAddress& mac) {
	ios::fmtflags flags = os.flags();
	Blob blob(mac);
	for (size_t i=0; i<blob.Size; i++) {
		if (i)
			os << ':';
		os << hex << (int)blob.constData()[i];
	}
	os.flags(flags);
	return os;
}


int StreamReader::ReadChar() {
	if (m_prevChar == -1)
		return BaseStream.ReadByte();
	return exchange(m_prevChar, -1);
}

String StreamReader::ReadToEnd() {
	vector<char> v;
	for (int b; (b = BaseStream.ReadByte()) != -1;)
		v.push_back((char)b);
	if (v.empty())
		return String();
	return String(&v[0], 0, v.size(), &Encoding);
}

pair<String, bool> StreamReader::ReadLineEx() {
	pair<String, bool> r(nullptr, false);
	vector<char> v;
	bool bEmpty = true;
	for (int b; (b=ReadChar())!=-1;) {
		bEmpty = false;
		switch (b) {
		case '\r':
			r.second = true;
			if ((m_prevChar=ReadChar()) == '\n')
				m_prevChar = -1;
		case '\n':
			r.second = true;
			goto LAB_OUT;
		default:
			v.push_back((char)b);
		}
	}
LAB_OUT:
	r.first = bEmpty ? nullptr
		: v.empty() ? String()
		: String(&v[0], 0, v.size(), &Encoding);
	return r;
}

void StreamWriter::WriteLine(RCString line) {
	Blob blob = Encoding.GetBytes(line+NewLine);
	m_stm.WriteBuf(blob);
}

UInt64 ToUInt64AtBytePos(const dynamic_bitset<byte>& bs, size_t pos) {
	ASSERT(!(pos & 7));

	UInt64 r = 0;
#if UCFG_STDSTL
	for (size_t i=0, e=min(size_t(64), size_t(bs.size()-pos)); i<e; ++i)
		r |= UInt64(bs[pos+i]) << i;
#else
	size_t idx = pos/bs.bits_per_block;
	const byte *pb = (const byte*)&bs.m_data[idx];
	ssize_t outRangeBits = max(ssize_t(pos + 64 - bs.size()), (ssize_t)0);
	if (0 == outRangeBits)
		return *(UInt64*)pb;
	size_t n = std::min(size_t(8), (bs.num_blocks()-idx)*bs.bits_per_block/8);
	for (size_t i=0; i<n; ++i)
		((byte*)&r)[i] = pb[i];
	r &= UInt64(-1) >> outRangeBits;
#endif
	return r;
}



} // Ext::


static vector<PFNAtExit> *s_atexits;

static vector<PFNAtExit>& GetAtExit() {
	if (!s_atexits)
		s_atexits = new vector<PFNAtExit>;
	return *s_atexits;
}

extern "C" {

int AFXAPI RegisterAtExit(void (_cdecl*pfn)()) {
	GetAtExit().push_back(pfn);
	return 0;
}

void AFXAPI UnregisterAtExit(void (_cdecl*pfn)()) {
	Remove(GetAtExit(), pfn);
}

void __cdecl MainOnExit() {
	vector<PFNAtExit>& ar = GetAtExit();
	while (!ar.empty()) {
		ar.back()();
		ar.pop_back();
	}
}


#if UCFG_WDM
unsigned int __cdecl bus_space_read_4(int bus, void *addr, int off) {
	if (bus == BUS_SPACE_IO)
		return READ_PORT_ULONG(PULONG((byte*)addr + off));
	else
		return *(volatile ULONG *)((byte*)addr+off);
}

void __cdecl bus_space_write_4(int bus, void *addr, int off, unsigned int val) {
	if (bus == BUS_SPACE_IO)
		WRITE_PORT_ULONG(PULONG((byte*)addr + off), val);
	else
		*(volatile ULONG *)((byte*)addr+off) = val;
}
#endif



} // extern "C"


