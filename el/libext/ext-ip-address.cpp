/*######     Copyright (c) 1997-2013 Ufasoft  http://ufasoft.com  mailto:support@ufasoft.com,  Sergey Pavlov  mailto:dev@ufasoft.com #######################################
#                                                                                                                                                                          #
# This program is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation;  #
# either version 3, or (at your option) any later version. This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the      #
# implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details. You should have received a copy of the GNU #
# General Public License along with this program; If not, see <http://www.gnu.org/licenses/>                                                                               #
##########################################################################################################################################################################*/

#include <el/ext.h>

#if UCFG_WIN32
#	if UCFG_WCE
#		pragma comment(lib, "ws2")
#	else
#		pragma comment(lib, "ws2_32")
#	endif
#endif

#include "ext-net.h"


namespace Ext { 
using namespace std;


void SocketStartupCheck(int code) {
	if (code)
		ThrowWSALastError();
}

CUsingSockets::~CUsingSockets() {
#if UCFG_WIN32
	try {
		DBG_LOCAL_IGNORE_NAME(HRESULT_FROM_WIN32(WSASYSNOTREADY), ignWSASYSNOTREADY);	
		Close();
	} catch (RCExc ex) {
		if (HResultInCatch(ex) != HRESULT_FROM_WIN32(WSASYSNOTREADY))
			throw;
	}
#endif
}

void CUsingSockets::EnsureInit() {
#if UCFG_WIN32
	if (!m_bInited) {
		SocketStartupCheck(::WSAStartup(MAKEWORD(2, 2), &m_data));
		m_bInited = true;
	}
#endif
}

void CUsingSockets::Close() {
#if UCFG_WIN32
	if (m_bInited) {
		m_bInited = false;
		SocketCheck(::WSACleanup());
	}
#endif
}


static CUsingSockets s_usingSockets(false);

void _cdecl SocketsCleanup() {
	s_usingSockets.Close();
}
static AtExitRegistration s_atexitSocketsCleanup(&SocketsCleanup);

#if UCFG_WIN32

extern "C" const char * __cdecl API_inet_ntop(int af, const void *src, char *dst, socklen_t size) {	
	s_usingSockets.EnsureInit();

	TCHAR buf[256];
	DWORD len = _countof(buf);
	int rc;
	switch (af) {
	case AF_INET:
		{
			sockaddr_in sa = { (short)af };
			memcpy(&sa.sin_addr, src, 4);
			rc = ::WSAAddressToString((LPSOCKADDR)&sa, sizeof sa, 0, buf, &len);
		}
		break;
	case AF_INET6:
		{
			sockaddr_in6 sa = { (short)af };
			memcpy(&sa.sin6_addr, src, 16);
			rc = ::WSAAddressToString((LPSOCKADDR)&sa, sizeof sa, 0, buf, &len);
		}
		break;
	default:
		errno = EAFNOSUPPORT;
		return 0;

	}
	if (rc) {
		errno = EFAULT;
		return 0;
	}
	if (len > size) {
		errno  = ENOSPC;
		return 0;
	}
	return strncpy(dst, String(buf), size);
}

extern "C" int __cdecl API_inet_pton(int af, const char *src, void *dst) {
	s_usingSockets.EnsureInit();

	union {
		sockaddr_in sa4;
		sockaddr_in6 sa6;
	} sa;
	INT len; 
	int rc;
	String s(src);
	switch (af) {
	case AF_INET:
		len = sizeof(sockaddr_in);
		break;
	case AF_INET6:
		len = sizeof(sockaddr_in6);
		break;
	default:
		errno = EAFNOSUPPORT;
		return -1;
	}
	rc = ::WSAStringToAddress((LPTSTR)(LPCTSTR)s, af, 0, (LPSOCKADDR)&sa, &len);
	if (rc)
		return 0;
	switch (af) {
	case AF_INET:
		memcpy(dst, &sa.sa4.sin_addr, 4);
		break;
	case AF_INET6:
		memcpy(dst, &sa.sa6.sin6_addr, 16);
		break;
	}
	return 1;
}

#endif // UCFG_WIN32

String AFXAPI HostToStr(DWORD host) {
	char buf[20];
	const byte *ar = (const byte*)&host;
	sprintf(buf, "%d.%d.%d.%d", ar[3], ar[2], ar[1], ar[0]);
	return buf;
}

static const in6_addr s_loopback6 = IN6ADDR_LOOPBACK_INIT,
						s_any6 = IN6ADDR_ANY_INIT,
							s_none6 = { 0 };

const IPAddress IPAddress::Any(Fast_htonl(INADDR_ANY)),			// [Win] don't use standard ntohl to prevent loading WS2_32.dll
	IPAddress::Broadcast(Fast_htonl(INADDR_BROADCAST)),
	IPAddress::Loopback(Fast_htonl(INADDR_LOOPBACK)),
	IPAddress::None(Fast_htonl(INADDR_NONE)),
	IPAddress::IPv6Any(ConstBuf(&s_any6, 16)),
	IPAddress::IPv6Loopback(ConstBuf(&s_loopback6, 16)),
	IPAddress::IPv6None(ConstBuf(&s_none6, 16));


IPAddress::IPAddress()
	:	m_domainname(nullptr)
{
	CreateCommon();
}

IPAddress::IPAddress(UInt32 nboIp4)
	:	m_domainname(nullptr)
{
	CreateCommon();
	m_sin.sin_addr.s_addr = nboIp4;
}

IPAddress::IPAddress(const ConstBuf& mb)
	:	m_domainname(nullptr)
{
	CreateCommon();
	switch (mb.Size) {
	case 4:
		AddressFamily = Ext::AddressFamily::InterNetwork;
		memcpy(&m_sin.sin_addr, mb.P, 4);
		break;
	case 16:
		AddressFamily = Ext::AddressFamily::InterNetworkV6;
		memcpy(&m_sin6.sin6_addr, mb.P, 16);
		break;
	default:
		Throw(E_EXT_UnknownHostAddressType);
	}
}
void IPAddress::CreateCommon() {
	ZeroStruct(m_sin6);
	AddressFamily = Ext::AddressFamily::InterNetwork;
}

IPAddress::IPAddress(const sockaddr& sa)
	:	m_domainname(nullptr)
{
	switch (sa.sa_family)
	{
	case AF_INET:
		memcpy(&m_sin, &sa, sizeof(sockaddr_in));
		break;
	case AF_INET6:
		memcpy(&m_sin6, &sa, sizeof(sockaddr_in6));
		break;
	default:
		m_sockaddr = sa;
	}
}

size_t IPAddress::GetHashCode() const {
	size_t r = std::hash<UInt16>()((UInt16)get_AddressFamily());
	switch ((int)get_AddressFamily()) {
	case AF_DOMAIN_NAME:
		r += std::hash<String>()(m_domainname);
		break;
	case AF_INET:
		r += std::hash<UInt32>()(*(UInt32*)&m_sin.sin_addr);
		break;
	case AF_INET6:
		r += hash_value(ConstBuf(&m_sin6.sin6_addr, 16));		
		break;
	default:
		Throw(E_EXT_UnknownHostAddressType);
	}
	return r;
}

#if UCFG_USE_REGEX
//!!!static Regex s_reDomainName("^[\\p{L}0-9-]+(\\.[\\p{L}0-9-]+)*\\.?$");
#	if UCFG_WIN32
static StaticWRegex
#	else
static StaticRegex
#	endif
	s_reDomainName("^[[:alpha:]0-9-]+(\\.[[:alpha:]0-9-]+)*\\.?$");
#endif // UCFG_USE_REGEX

IPAddress IPAddress::Parse(RCString ipString) {
	byte buf[16];
	if (CCheck(::inet_pton(AF_INET, ipString, buf)))
		return IPAddress(ConstBuf(buf, 4));
	if (CCheck(::inet_pton(AF_INET6, ipString, buf)))
		return IPAddress(ConstBuf(buf, 16));
#if UCFG_USE_REGEX
#	if UCFG_WIN32
	if (regex_search(ipString, *s_reDomainName)) {
#	else
	if (regex_search(ipString.c_str(), s_reDomainName)) {
#	endif
		IPAddress r;
		r.AddressFamily = Ext::AddressFamily(AF_DOMAIN_NAME);
		r.m_domainname = ipString;
		return r;
	}
#endif
	Throw(HRESULT_FROM_WIN32(DNS_ERROR_INVALID_IP_ADDRESS));
}

bool IPAddress::TryParse(RCString s, IPAddress& ip) {
	byte buf[16];
	if (CCheck(::inet_pton(AF_INET, s, buf))) {
		ip = IPAddress(ConstBuf(buf, 4));
		return true;
	}
	if (CCheck(::inet_pton(AF_INET6, s, buf))) {
		ip = IPAddress(ConstBuf(buf, 16));
		return true;
	}
	return false;
}

String IPAddress::ToString() const {
	switch ((int)get_AddressFamily()) {
	case AF_INET:
		return HostToStr(ntohl(m_sin.sin_addr.s_addr));
	case AF_INET6:
		{
			char buf[INET6_ADDRSTRLEN];
			if (!::inet_ntop((int)get_AddressFamily(), (void*)&m_sin6.sin6_addr, buf, sizeof buf))
				CCheck(-1);
			return buf;			
		}
	case AF_DOMAIN_NAME:
		return m_domainname;
#if UCFG_USE_POSIX && !defined(__FreeBSD__)
	case AF_PACKET:
		return ":AF_PACKET:";
#endif
	default:
		return "Address Family "+Convert::ToString((int)get_AddressFamily());
	}
}

IPAddress& IPAddress::operator=(const IPAddress& ha) {
	AddressFamily = ha.AddressFamily;
	m_domainname = ha.m_domainname;
	UInt16 port = m_sin.sin_port;
	memcpy(&m_sin6, &ha.m_sin6, sizeof m_sin6);
	switch ((int)get_AddressFamily()) {
	case AF_DOMAIN_NAME:
	case AF_INET:
	case AF_INET6:
		m_sin.sin_port = port;
	}
	return _self;
}

bool IPAddress::operator<(const IPAddress& ha) const {
	if (AddressFamily != ha.AddressFamily)
		return (int)get_AddressFamily() < (int)ha.get_AddressFamily();
	switch ((int)get_AddressFamily()) {
	case AF_DOMAIN_NAME:
		return m_domainname < ha.m_domainname;
	case AF_INET:
		return memcmp(&m_sin.sin_addr, &ha.m_sin.sin_addr, 4) < 0;
	case AF_INET6:
		return memcmp(&m_sin6.sin6_addr, &ha.m_sin6.sin6_addr, 16) < 0;
	default:
		Throw(E_EXT_UnknownHostAddressType);
	}
}

bool IPAddress::operator==(const IPAddress& ha) const {
	if (AddressFamily != ha.AddressFamily)
		return false;
	switch ((int)get_AddressFamily())
	{
	case AF_DOMAIN_NAME:
		return m_domainname == ha.m_domainname;
	case AF_INET:
		return *(Int32*)&m_sin.sin_addr == *(Int32*)&ha.m_sin.sin_addr;
	case AF_INET6:
		return !memcmp(&m_sin6.sin6_addr, &ha.m_sin6.sin6_addr, 16);
	default:
		Throw(E_EXT_UnknownHostAddressType);
	}
}

Blob IPAddress::GetAddressBytes() const {
	int size = FamilySize(AddressFamily);
	if (!size)
		Throw(E_EXT_UnknownHostAddressType);
	switch ((int)get_AddressFamily()) {
	case AF_INET:
		return Blob(&m_sin.sin_addr, 4);
	case AF_INET6:
		return Blob(&m_sin6.sin6_addr, 16);
	default:
		Throw(E_EXT_UnknownHostAddressType);
	}
}

UInt32 IPAddress::GetIP() const {
	UInt32 nhost;
	switch ((int)get_AddressFamily()) {
	case AF_DOMAIN_NAME:
		if ((nhost = inet_addr(m_domainname)) != INADDR_NONE)
			return ntohl(nhost);
		return Dns::GetHostEntry(m_domainname).AddressList[0].GetIP();
	case AF_INET:
		nhost = ntohl(m_sin.sin_addr.s_addr);
		break;
	default:
		Throw(E_EXT_UnknownHostAddressType);
	}
	return nhost;
}

void IPAddress::Normalize() {			//!!!TODO for IPv6
	if ((int)get_AddressFamily() == AF_DOMAIN_NAME)
		operator=(IPAddress(htonl(GetIP())));
}

bool IPAddress::IsGlobal() const {
	switch ((int)get_AddressFamily()) {
	case AF_INET:
		{
			UInt32 hip = ntohl(m_sin.sin_addr.s_addr);
			return hip != 0
				&& (hip & 0xFF000000) != 0x7F000000		// 127.x.x.x
				&& (hip & 0xFFFF0000) != 0xC0A80000		// 192.168.x.x
				&& (hip & 0xFFF00000) != 0xAC100000		// 172.16.x.x
				&& (hip & 0xFF000000) != 0x0A000000		// 10.x.x.x
				&& (hip & 0xFFFF0000) != 0xA9FE0000;	// 169.254.x.x (RFC 3927)
		}
		break;
	case AF_INET6:
		{
			UInt16 fam = *(byte*)&m_sin6.sin6_addr;
			return fam > 0 && fam<0xFC;
		}
	case IPAddress::AF_DOMAIN_NAME:
		return true;
	default:
		Throw(E_FAIL);
	}
}

bool IPAddress::get_IsIPv4MappedToIPv6() const {
	return (int)get_AddressFamily() == AF_INET6 &&
		!memcmp(&m_sin6.sin6_addr, "\xFF\xFF" "0000000000", 12);
}

bool IPAddress::get_IsIPv6Teredo() const {
	return (int)get_AddressFamily() == AF_INET6 &&
		*(UInt32*)&m_sin6.sin6_addr == htonl(0x20010000);
}

BinaryWriter& AFXAPI operator<<(BinaryWriter& wr, const IPAddress& ha) {
	wr << (short)ha.get_AddressFamily();
	switch ((int)ha.get_AddressFamily()) {
	case IPAddress::AF_DOMAIN_NAME:
		wr << ha.m_domainname;
		break;
	case AF_INET:
		wr.Write(&ha.m_sin.sin_addr, sizeof(ha.m_sin.sin_addr));
		break;
	case AF_INET6:
		wr.Write(&ha.m_sin6.sin6_addr, sizeof(ha.m_sin6.sin6_addr));
		break;
	default:
		wr.Write(&ha.m_sockaddr, sizeof(ha.m_sockaddr));		//!!! unknown size
		break;
	}
	return wr;
}

const BinaryReader& AFXAPI operator>>(const BinaryReader& rd, IPAddress& ha) {
	IPAddress h;
	short fam = rd.ReadInt16();
	h.AddressFamily = Ext::AddressFamily(fam);
	switch (fam) {
	case IPAddress::AF_DOMAIN_NAME:
		{
			String s;
			rd >> s;
			h.m_domainname = s;
		}
		break;
	case AF_INET:
		rd.Read(&h.m_sin.sin_addr, sizeof(h.m_sin.sin_addr));
		break;
	case AF_INET6:
		rd.Read(&h.m_sin6.sin6_addr, sizeof(h.m_sin6.sin6_addr));
		break;
	default:
		rd.Read(&h.m_sockaddr, sizeof(h.m_sockaddr));		//!!! unknown size
		break;
	}
	ha = h;
	return rd;
}

IPEndPoint::IPEndPoint(RCString s, UInt16 port) {
	const char *p = s;
	const char *q = strchr(p, ':');
	if (q) {
		Address = IPAddress::Parse(String(p, q-p));
		Port = Convert::ToUInt16(q+1);
	} else
		Address = IPAddress::Parse(p);
	Port = port;
}

const sockaddr *IPEndPoint::c_sockaddr() const {
	return &Address.m_sockaddr;
}

size_t IPEndPoint::sockaddr_len() const {
	switch ((int)get_AddressFamily()) {
	case AF_INET: return sizeof(sockaddr_in);
	case AF_INET6: return sizeof(sockaddr_in6);
	default:
		Throw(E_EXT_UnknownHostAddressType);
	}
}

IPEndPoint IPEndPoint::Parse(RCString s) {
	vector<String> ar = s.Split(":");
	return IPEndPoint(IPAddress::Parse(ar[0]), Convert::ToUInt16(ar[1]));
}

IPHostEntry::IPHostEntry(hostent *phost) {
	if (phost) {
		HostName = phost->h_name;
		for (const char * const *p = phost->h_addr_list; p && *p; ++p) {
			IPAddress ip;
			switch (phost->h_addrtype) {
			case AF_INET:
				ip = IPAddress(*(UInt32*)*p);
				break;
			case AF_INET6:
				ip = IPAddress(ConstBuf(*p, 16));
				break;
			default:
				Throw(E_EXT_UnknownHostAddressType);
			}
			AddressList.push_back(ip);
		}
		for (const char * const *q = phost->h_aliases; q && *q; ++q)
			Aliases.push_back(*q);
	}
}

IPHostEntry Dns::GetHostEntry(const IPAddress& address) {
	Blob blob = address.GetAddressBytes();
	if (hostent *phost = gethostbyaddr((const char*)blob.constData(), blob.Size, (int)address.get_AddressFamily()))
		return IPHostEntry(phost);
	else
		ThrowWSALastError();
}

IPHostEntry Dns::GetHostEntry(RCString hostNameOrAddress) {
	UInt32 nhost = inet_addr(hostNameOrAddress);
	if (nhost != INADDR_NONE)
		return GetHostEntry(IPAddress(nhost));
	if (hostent *phost = gethostbyname(hostNameOrAddress))
		return IPHostEntry(phost);
	else
		ThrowWSALastError();
}

String Dns::GetHostName() {
	char buf[256] = "";
	SocketCheck(gethostname(buf, _countof(buf)));
	return buf;
}

String COperatingSystem::get_ComputerName() {	
#if UCFG_USE_POSIX || UCFG_WCE
	return Dns::GetHostName();
#else
	TCHAR buf[MAX_COMPUTERNAME_LENGTH+1];
	DWORD len = _countof(buf);
	Win32Check(::GetComputerName(buf, &len));
	return String(buf, len);
#endif
}




} // Ext::

/*!!!R

#ifndef INET_ADDRSTRLEN
#	define INET_ADDRSTRLEN    16
#endif


#if !UCFG_WCE

static const char *inet_ntop_v4 (const void *src, char *dst, size_t size) {
	const char digits[] = "0123456789";
	int i;
	struct in_addr *addr = (struct in_addr *)src;
	u_long a = ntohl(addr->s_addr);
	const char *orig_dst = dst;
	if (size < INET_ADDRSTRLEN) {
		errno = ENOSPC;
		return NULL;
	}
	for (i = 0; i < 4; ++i) {
		int n = (a >> (24 - i * 8)) & 0xFF;
		int non_zerop = 0;
		if (non_zerop || n / 100 > 0) {
			*dst++ = digits[n / 100];
			n %= 100;
			non_zerop = 1;
		}
		if (non_zerop || n / 10 > 0)
		{
			*dst++ = digits[n / 10];
			n %= 10;
			non_zerop = 1;
		}
		*dst++ = digits[n];
		if (i != 3)
			*dst++ = '.';
	}
	*dst++ = '\0';
	return orig_dst;
}

extern "C" const char *inet_ntop(int af, const void *src, char *dst, size_t size) {
	switch (af)
	{
	case AF_INET :
		return inet_ntop_v4 (src, dst, size);
	default :
		errno = EAFNOSUPPORT;	
		return NULL;
	}
}

#endif
*/