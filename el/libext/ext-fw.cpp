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
#	include <wininet.h>
#	include <shlobj.h>

#	include <el/libext/win32/ext-win.h>
#endif


#if UCFG_USE_POSIX
#	include <pwd.h>
#	include <sys/utsname.h>
#endif

#pragma warning(disable: 4073)
#pragma init_seg(lib)

#if UCFG_WIN32_FULL
#	pragma comment(lib, "shell32")
#endif

#if UCFG_WCE
#	pragma comment(lib, "ceshell")
#endif

#ifndef WIN32
	const GUID GUID_NULL = { 0 };
#endif

namespace Ext { 
using namespace std;

mutex g_mfcCS;

void AFXAPI CCheckErrcode(int en) {
	if (en) {
#if UCFG_HAVE_STRERROR
		ThrowS(HRESULT_FROM_C(en), strerror(en));
#else
		Throw(E_NOTIMPL); //!!!
#endif
	}
}

int AFXAPI CCheck(int i, int allowableError) {
	if (i >= 0)
		return i;
	ASSERT(i == -1);
	int en = errno;
	if (en == allowableError)
		return i;
#if UCFG_HAVE_STRERROR
	ThrowS(HRESULT_FROM_C(en), strerror(en));
#else
	Throw(E_NOTIMPL); //!!!
#endif
}

int AFXAPI NegCCheck(int rc) {
	if (rc >= 0)
		return rc;
#if UCFG_HAVE_STRERROR
	ThrowS(HRESULT_FROM_C(-rc), strerror(-rc));
#else
	Throw(HRESULT_FROM_C(-rc));
#endif
}

EXTAPI void AFXAPI CFileCheck(int i) {
	if (i) {
#if UCFG_HAVE_STRERROR
		ThrowS(HRESULT_FROM_C(i), strerror(i));
#else
		Throw(HRESULT_FROM_C(i));
#endif
	}
}


#if UCFG_USE_POSIX
CMapStringRes& MapStringRes() {
	static CMapStringRes s_mapStringRes;
	return s_mapStringRes;
}

static CMapStringRes& s_mapStringRes_not_used = MapStringRes();	// initialize while one thread, don't use

String AFXAPI AfxLoadString(UINT nIDS) {
	CMapStringRes::iterator it = MapStringRes().find(nIDS);
	if (it == MapStringRes().end())
		Throw(E_FAIL);	//!!!
	return it->second;
}
#endif



#if UCFG_WIN32_FULL
CStringVector AFXAPI COperatingSystem::QueryDosDevice(RCString dev) {
	for(int size=_MAX_PATH;; size*=2) {
		TCHAR *buf = (TCHAR*)alloca(sizeof(TCHAR)*size);
		DWORD dw = ::QueryDosDevice(dev, buf, size);
		if (dw && dw < size)
			return AsciizArrayToStringArray(buf);
		Win32Check(GetLastError() != ERROR_INSUFFICIENT_BUFFER);
	}
}
#endif

#ifdef _WIN32
COperatingSystem::COsVerInfo COperatingSystem::get_Version() {
#	if UCFG_WDM
	RTL_OSVERSIONINFOEXW r = { sizeof r };
	NtCheck(::RtlGetVersion((RTL_OSVERSIONINFOW*)&r));
#	elif UCFG_WCE
	OSVERSIONINFO r = { sizeof r };
	Win32Check(::GetVersionEx(&r));
#	else
	OSVERSIONINFOEX r = { sizeof r };
	Win32Check(::GetVersionEx((OSVERSIONINFO*)&r));
#endif
	return r;
}
#endif

String Environment::GetMachineType() {
#if UCFG_USE_POSIX
	utsname u;
	CCheck(::uname(&u));
	return u.machine;
#elif defined(WIN32)
	SYSTEM_INFO si;
	GetSystemInfo(&si);
	switch (si.wProcessorArchitecture) {
	case PROCESSOR_ARCHITECTURE_INTEL: 	return "x86";
	case PROCESSOR_ARCHITECTURE_MIPS:	return "MIPS";
	case PROCESSOR_ARCHITECTURE_SHX:	return "SHX";
	case PROCESSOR_ARCHITECTURE_ARM:	return "ARM";
	case PROCESSOR_ARCHITECTURE_IA64:	return "IA-64";
#	if UCFG_WIN32_FULL
	case PROCESSOR_ARCHITECTURE_IA32_ON_WIN64:	return "IA-32 on Win64";
	case PROCESSOR_ARCHITECTURE_AMD64:	return "x64";
#	endif
	case PROCESSOR_ARCHITECTURE_UNKNOWN:
	default:
		return "Unknown";
	}
#else
	Throw(E_NOTIMPL);
#endif
}

String Environment::GetMachineVersion() {
#if UCFG_USE_POSIX
	utsname u;
	CCheck(::uname(&u));
	return u.machine;
#elif defined(WIN32)
	String s;	
	SYSTEM_INFO si;
	GetSystemInfo(&si);
	switch (si.wProcessorArchitecture) {
	case PROCESSOR_ARCHITECTURE_INTEL:
		switch (si.dwProcessorType)
		{
		case 586:
			switch (si.wProcessorRevision)
			{
			case 5895:
				s = "Core 2 Quad";
				break;
			default:
				s = "586";
			}
			break;
		default:
			s = "Intel";
		};
		break;
#if UCFG_WIN32_FULL
	case PROCESSOR_ARCHITECTURE_AMD64:
		s = "AMD64";
		break;
#endif
	case PROCESSOR_ARCHITECTURE_ARM:
		s = "ARM";
		break;
	default:
		s = "Unknown CPU";
		break;
	}
	return s;
#else
	Throw(E_NOTIMPL);
#endif
}

/*!!!

ULONGLONG AFXAPI StrToVersion(RCString s) {
	LONGLONG r = 0;
	istringstream is(s.c_str());
	for (int i=4; i--;) {
		WORD w;
		if (is >> w)
			((WORD*)&r)[i] = w;
		else
			break;
		if (is.get() != '.')
			break;
	}
	return r;
}

String AFXAPI VersionToStr(const LONGLONG& v, int n) {
	WORD ar[4];
	(LONGLONG&)ar[0] = v;
	ostringstream os;
	for (int i=3; i>0 && 3-i < n; i--) {
		if (i != 3)
			os << '.';
		os << ar[i];
	}
	return os.str();
}*/

#if UCFG_USE_REGEX
static StaticRegex s_reVersion("^(\\d+)\\.(\\d+)(\\.(\\d+)(\\.(\\d+))?)?$");

Version::Version(RCString s) {
	cmatch m;
	if (regex_search(s.c_str(), m, *s_reVersion)) {
		Major = atoi(String(m[1]));
		Minor = atoi(String(m[2]));
		Build = m[4].matched ? atoi(String(m[4])) : -1;
		Revision = m[6].matched ? atoi(String(m[6])) : -1;
	} else
		Throw(E_INVALIDARG);
}
#endif

#if UCFG_WIN32

Version Version::FromFileInfo(int ms, int ls, int fieldCount) {
	int ar[4] = { UInt16(ms>>16), UInt16(ms), UInt16(ls>>16), UInt16(ls) };
	for (int i=4; i-- > fieldCount;)
		ar[i] = -1;
	return Version(ar[0], ar[1], ar[2], ar[3]);
}

#endif

String Version::ToString(int fieldCount) const {
	if (fieldCount<0 || fieldCount>4)
		Throw(E_INVALIDARG);
	int ar[4] = { Major, Minor, Build, Revision };
	ostringstream os;
	for (int i=0; i<fieldCount; ++i) {
		if (ar[i] == -1)
			Throw(E_INVALIDARG);
		if (i)
			os << ".";
		os << ar[i];
	}
	return os.str();
}

String Version::ToString() const {
	return ToString(Revision!=-1 ? 4 : Build!=-1 ? 3 : 2);
}

OperatingSystem::OperatingSystem() {
	Platform = PlatformID::Unix;
#ifdef _WIN32
	COperatingSystem::COsVerInfo vi = System.Version;
	switch (vi.dwPlatformId) {
	case VER_PLATFORM_WIN32s: Platform = PlatformID::Win32S; break;
	case VER_PLATFORM_WIN32_WINDOWS: Platform = PlatformID::Win32Windows; break;
	case VER_PLATFORM_WIN32_NT: Platform = PlatformID::Win32NT; break;
#	if UCFG_WCE
	case VER_PLATFORM_WIN32_CE: Platform = PlatformID::WinCE; break;
#	endif
	}		
	Version = Ext::Version(vi.dwMajorVersion, vi.dwMinorVersion, vi.dwBuildNumber);
	ServicePack = vi.szCSDVersion;
#endif
}

String OperatingSystem::get_PlatformName() const {
#ifdef _WIN32
#	ifdef UCFG_WIN32
	switch (Platform) {
	case PlatformID::Win32S: return "Win32s";
	case PlatformID::Win32Windows: return "Windows 9x";
	case PlatformID::Win32NT: return "Windows NT";
	case PlatformID::WinCE: return "Windows CE";
	}
#	endif
	return "Windows Native NT";
#else
	utsname u;
	CCheck(::uname(&u));
	return u.sysname;
#endif
}

String OperatingSystem::get_VersionString() const {
	String r = get_PlatformName()+" "+get_VersionName();
	r += " "+Version.ToString();
	if (!ServicePack.IsEmpty())
		r += " "+ServicePack;
	return r;
}

String OperatingSystem::get_VersionName() const {
#ifdef _WIN32
	String s;
	switch (int osver = GetOsVersion()) {
#if UCFG_WIN32_FULL
	case OSVER_95:
		s = "95";
		break;
	case OSVER_98:
		s = "98";
		break;
	case OSVER_ME:
		s = "Millenium";
		break;
	case OSVER_NT4:
		s = "NT 4";
		break;
	case OSVER_2000:
		s = "2000";
		break;
	case OSVER_XP:
		s = "XP";
		break;
	case OSVER_SERVER_2003:
		s = "Server 2003";
		break;
	case OSVER_VISTA:
		s = "Vista";
		break;
	case OSVER_2008:
		s = "Server 2008";
		break;
	case OSVER_7:
		s = "7";
		break;
	case OSVER_2008_R2:
		s = "Server 2008 R2";
		break;
	case OSVER_8:
		s = "8";
		break;
	case OSVER_FUTURE:
		s = "Unknown Future Version";
		break;
#endif
	case OSVER_CE_5:
		s = "CE 5";
		break;
	case OSVER_CE_6:
		s = "CE 5";
		break;
	case OSVER_CE_FUTURE:
		s = "CE Future";
		break;
	default:
		s = "Unknown new";
		break;
	}
	return s;
#else
	utsname u;
	CCheck(::uname(&u));
	return u.release;
#endif
}

#if UCFG_WIN32
Int32 AFXAPI Environment::TickCount() {
	return (Int32)::GetTickCount();
}
#endif // UCFG_WIN32

#if UCFG_WIN32_FULL

Environment::CStringsKeeper::CStringsKeeper()
	:	m_p(0)
{
	if (!(m_p = (LPTSTR)::GetEnvironmentStrings()))
		Throw(E_EXT_UnknownWin32Error);
}

Environment::CStringsKeeper::~CStringsKeeper() {
	Win32Check(::FreeEnvironmentStrings(m_p));
}

UInt32 AFXAPI Environment::GetLastInputInfo() {
	LASTINPUTINFO lii = { sizeof lii };
	Win32Check(::GetLastInputInfo(&lii));
	return lii.dwTime;
}

#endif // UCFG_WIN32_FULL

int Environment::ExitCode;
const OperatingSystem Environment::OSVersion;

class Environment Environment;

String Environment::GetFolderPath(SpecialFolder folder) {
#if UCFG_USE_POSIX
	String homedir = GetEnvironmentVariable("HOME");
	switch (folder)
	{
	case SpecialFolder::Desktop: return Path::Combine(homedir, "Desktop");
	case SpecialFolder::ApplicationData: return Path::Combine(homedir, ".config");
	default:
		Throw(E_NOTIMPL);
	}

#elif UCFG_WIN32
	TCHAR path[_MAX_PATH];
#	if UCFG_OLE
	LPITEMIDLIST pidl;
	OleCheck(SHGetSpecialFolderLocation(0, (int)folder, &pidl));
	Win32Check(SHGetPathFromIDList(pidl, path));
	CComPtr<IMalloc> aMalloc;
	OleCheck(::SHGetMalloc(&aMalloc));
	aMalloc->Free(pidl);
#	else
	if (!SHGetSpecialFolderPath(0, path, (int)folder, false))
		Throw(E_FAIL);
#	endif
	return path;
#else
	Throw(E_NOTIMPL);
#endif
}

int Environment::get_ProcessorCount() {
#if UCFG_WIN32
	SYSTEM_INFO si;
	::GetSystemInfo(&si);
	return si.dwNumberOfProcessors;
#elif UCFG_USE_POSIX
	return std::max((int)sysconf(_SC_NPROCESSORS_ONLN), 1);
#else
	return 1;
#endif
}

String Environment::GetEnvironmentVariable(RCString s) {
#if UCFG_USE_POSIX
	return ::getenv(s);
#elif UCFG_WCE
	return nullptr;
#else
	_TCHAR *p = (_TCHAR*)alloca(256*sizeof(_TCHAR));
	DWORD dw = ::GetEnvironmentVariable(s, p, 256);
	if (dw > 256) {
		p = (_TCHAR*)alloca(dw*sizeof(_TCHAR));
		dw = ::GetEnvironmentVariable(s, p, dw);
	}
	if (dw)
		return p;
	Win32Check(GetLastError() == ERROR_ENVVAR_NOT_FOUND);
	return nullptr;
#endif
}

vector<String> ParseCommandLine(RCString s) {
	vector<String> r;
	const char *p = s;
	const char *q = 0;
	bool bQuoting = false;
	for (; *p; p++) {
		char ch = *p;
		switch (ch) {
		case '\"':
			if (bQuoting) {
				r.push_back(String(q, p-q));
				bQuoting = false;
			}
			else
				bQuoting = true;
			q = 0;
			break;
		case ' ':
		case '\t':
		case '\r':
		case '\n':
			if (!bQuoting && q) {
				r.push_back(String(q, p-q));
				q = 0;
			}
			break;
		default:
			if (!q)
				q = p;
		}
	}
	if (q)
		r.push_back(String(q, p-q));
	return r;
}

#if !UCFG_WCE

String AFXAPI Environment::CommandLine() {
#if UCFG_USE_POSIX
	return File::ReadAllText("/proc/self/cmdline");
#else
	return GetCommandLineW();
#endif
}

vector<String> AFXAPI Environment::GetCommandLineArgs() {
	return ParseCommandLine(CommandLine());
}

String Environment::SystemDirectory() {
#if UCFG_USE_POSIX
	return "";
#else
	TCHAR szPath[_MAX_PATH];
	Win32Check(::GetSystemDirectory(szPath, _countof(szPath)));
	return szPath;
#endif
}

void Environment::SetEnvironmentVariable(RCString name, RCString val) {
#if UCFG_USE_POSIX
	String s = name+"="+val;
	CCheck(::putenv(strdup(s)) != 0 ? -1 : 0);
#else
	Win32Check(::SetEnvironmentVariable(name, val));
#endif
}

String Environment::ExpandEnvironmentVariables(RCString name) {
#if UCFG_USE_POSIX
	Throw(E_NOTIMPL);
#else
	vector<TCHAR> buf(32768);																	// in Heap to save Stack
	DWORD r = Win32Check(::ExpandEnvironmentStrings(name, &buf[0], buf.size()));
	if (r > buf.size())
		Throw(E_FAIL);
	return buf;
#endif
}

map<String, String> Environment::GetEnvironmentVariables() {
#if UCFG_USE_POSIX
	Throw(E_NOTIMPL);
#else
	map<String, String> m;
	CStringsKeeper sk;
	for (LPTSTR p=sk.m_p; *p; p+=_tcslen(p)+1) {
		if (TCHAR *q = _tcschr(p, '='))
			m[String(p, q-p)] = q+1;
		else
			m[p] = "";			
	}
	return m;
#endif
}


int Environment::get_ProcessorCoreCount() {
#if UCFG_WIN32_FULL
	typedef BOOL (WINAPI *PFN_GetLogicalProcessorInformationEx)(LOGICAL_PROCESSOR_RELATIONSHIP RelationshipType, PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX Buffer, PDWORD ReturnedLength);

	DlProcWrap<PFN_GetLogicalProcessorInformationEx> pfn("KERNEL32.DLL", "GetLogicalProcessorInformationEx");
	if (pfn) {
		DWORD nBytes = 0;
		Win32Check(pfn(RelationProcessorCore, 0, &nBytes), ERROR_INSUFFICIENT_BUFFER);
		SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX  *buf = (SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX*)alloca(nBytes);
		Win32Check(pfn(RelationProcessorCore, buf, &nBytes));
		return nBytes/buf->Size;
	}
#endif
#if UCFG_CPU_X86_X64
	int a[4];
	Cpuid(a, 0);
	if (a[0] >= 4) {
		Cpuid(a, 4);
		return ((a[0] >> 26) & 0x3F) + 1;
	}
#endif
	return ProcessorCount;
}

String COperatingSystem::get_UserName() {
#if UCFG_USE_POSIX
	return getpwuid(getuid())->pw_name;
#else
	TCHAR buf[255];
	DWORD len = _countof(buf);
	Win32Check(::GetUserName(buf, &len));
	return buf;
#endif
}

#endif  // !UCFG_WCE

String NameValueCollection::ToString() const {
	ostringstream os;
	for (const_iterator i=begin(); i!=end(); ++i) {
		os << i->first << ": " << String::Join(",", i->second) << "\r\n";
	}
	return os.str();
}

String HttpUtility::UrlEncode(RCString s, Encoding& enc) {
	Blob blob = enc.GetBytes(s);
	ostringstream os;
	for (int i=0; i<blob.Size; ++i)	{
		char ch = blob.constData()[i];
		if (ch == ' ')
			os << '+';
		else if (isalnum(ch) || strchr("!'()*-._", ch))
			os << ch;
		else
			os << '%' << setw(2) << setfill('0') << hex << (int)(BYTE)ch;
	}		
	return os.str();
}

String HttpUtility::UrlDecode(RCString s, Encoding& enc) {
	Blob blob = enc.GetBytes(s);
	ostringstream os;
	for (int i=0; i<blob.Size; ++i)	
	{
		char ch = blob.constData()[i];
		switch (ch)
		{
		case '+':
			os << ' ';
			break;
		case '%':
			{
				if (i+2 > blob.Size)
					Throw(E_FAIL);
				String c1((char)blob.constData()[++i]),
					c2((char)blob.constData()[++i]);
				os << (char)Convert::ToUInt32(c1+c2, 16);
			}
			break;
		default:
			os << ch;
		}
	}		
	return os.str();
}

#if UCFG_USE_REGEX
static StaticRegex s_reNameValue("([^=]+)=(.*)");

NameValueCollection HttpUtility::ParseQueryString(RCString query) {
	vector<String> params = query.Split("&");
	NameValueCollection r;
	for (int i=0; i<params.size(); ++i) {
		cmatch m;
		if (!regex_search(params[i].c_str(), m, *s_reNameValue))
			Throw(E_FAIL);
		String name = UrlDecode(m[1]),
			value = UrlDecode(m[2]);
		r[name.ToUpper()].push_back(value);
	}
	return r;
}
#endif

class CBase64Table : public vector<int> {
public:
	static const char s_toBase64[];

	CBase64Table()
		:	vector<int>((size_t)256, EOF)
	{
		for (byte i=(byte)strlen(s_toBase64); i--;) // to eliminate trailing zero
			_self[s_toBase64[i]] = i;
	}
};

static InterlockedSingleton<CBase64Table> s_pBase64Table;

const char CBase64Table::s_toBase64[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

static int GetBase64Val(istream& is) {
	for (char ch; is.get(ch);)
		if (!isspace(ch))
			return (*s_pBase64Table)[ch];
	return EOF;
}

Blob Convert::FromBase64String(RCString s) {  
	istringstream is(s.c_str());
	MemoryStream ms;
	BinaryWriter wr(ms);
	while (true) {
		int ch0 = GetBase64Val(is),
			ch1 = GetBase64Val(is);
		if (ch0 == EOF || ch1 == EOF)
			break;
		wr << byte(ch0 << 2 | ch1 >> 4);
		int ch2 = GetBase64Val(is);
		if (ch2 == EOF)
			break;
		wr << byte(ch1 << 4 | ch2 >> 2);
		int ch3 = GetBase64Val(is);
		if (ch3 == EOF)
			break;
		wr << byte(ch2 << 6 | ch3);
	}
	return ms.Blob;
}

String Convert::ToBase64String(const ConstBuf& mb) {
	//!!!R	CBase64Table::s_toBase64;
	vector<String::Char> v;
	const byte *p = mb.P;
	for (size_t i=mb.Size/3; i--; p+=3) {
		DWORD dw = (p[0]<<16) | (p[1]<<8) | p[2];
		v.push_back(CBase64Table::s_toBase64[(dw>>18) & 0x3F]);
		v.push_back(CBase64Table::s_toBase64[(dw>>12) & 0x3F]);
		v.push_back(CBase64Table::s_toBase64[(dw>>6) & 0x3F]);
		v.push_back(CBase64Table::s_toBase64[dw & 0x3F]);
	}
	if (size_t rem = mb.Size % 3) {
		v.push_back(CBase64Table::s_toBase64[(p[0]>>2) & 0x3F]);
		switch (rem)
		{
		case 1:
			v.push_back(CBase64Table::s_toBase64[(p[0]<<4) & 0x3F]);
			v.push_back('=');
			break;
		case 2:
			v.push_back(CBase64Table::s_toBase64[((p[0]<<4) | (p[1]>>4)) & 0x3F]);
			v.push_back(CBase64Table::s_toBase64[(p[1]<<2) & 0x3F]);
		}
		v.push_back('=');
	}
	return String(&v[0], v.size());
}

#if UCFG_USE_REGEX
static StaticRegex s_reGuid("\\{([0-9A-Fa-f]{8,8})-([0-9A-Fa-f]{4,4})-([0-9A-Fa-f]{4,4})-([0-9A-Fa-f]{2,2})([0-9A-Fa-f]{2,2})-([0-9A-Fa-f]{2,2})([0-9A-Fa-f]{2,2})([0-9A-Fa-f]{2,2})([0-9A-Fa-f]{2,2})([0-9A-Fa-f]{2,2})([0-9A-Fa-f]{2,2})\\}");

Guid::Guid(RCString s) {
#if UCFG_COM
	OleCheck(::CLSIDFromString(Bstr(s), this));	
#else
	cmatch m;
	if (regex_search(s.c_str(), m, *s_reGuid)) {
		Data1 = Convert::ToUInt32(m[1], 16);
		Data2 = Convert::ToUInt16(m[2], 16);
		Data3 = Convert::ToUInt16(m[3], 16);
		for (int i=0; i<8; ++i)
			Data4[i] = (byte)Convert::ToUInt16(m[4+i], 16);
	} else
		Throw(E_FAIL);
#endif
}
#endif

Guid Guid::NewGuid() {
	Guid guid;
#if UCFG_COM
	OleCheck(::CoCreateGuid(&guid));
#else
	Random rng;
	rng.NextBytes(Buf((byte*)&guid, sizeof(GUID)));
#endif
	return guid;	
}

String Guid::ToString(RCString format) const {
	String s;
#ifdef WIN32
	wchar_t buf[40];
	StringFromGUID2(_self, buf, _countof(buf));
	s = buf;
#else
	s = "{"+Convert::ToString(Data1, "X8")+"-"+Convert::ToString(Data2, "X4")+"-"+Convert::ToString(Data3, "X4")+"-";			//!!!A
	for (int i=0; i<2; ++i)
		s += Convert::ToString(Data4[i], "X2");
	s += "-";
	for (int i=2; i<8; ++i)
		s += Convert::ToString(Data4[i], "X2");
	s += "}";
#endif
	if ("B" == format)
		return s;
	s = s.Mid(1, s.Length-2);
	if (format.IsEmpty() || "D"==format)
		return s;
	if ("N" == format)
		return s.Mid(0, 8)+s.Mid(9, 4)+s.Mid(14, 4)+s.Mid(19, 4)+s.Mid(24);
	if ("P" == format)
		return "("+s+")";
	Throw(E_FAIL);
}


CResID::CResID(UINT nResId)
:	m_resId(nResId)
{
}

CResID::CResID(const char *lpName)
    :	m_resId(0)
{
	_self = lpName;
}

CResID::CResID(const String::Char *lpName)
	:	m_resId(0)
{
	_self = lpName;
}

CResID& CResID::operator=(const char *resId) {
	m_name = "";
	m_resId = 0;
	if (HIWORD(resId))
		m_name = resId;
	else
		m_resId = (DWORD)(DWORD_PTR)resId;
	return _self;
}

CResID& CResID::operator=(const String::Char *resId) {
	m_name = "";
	m_resId = 0;
	if (HIWORD(resId))
		m_name = resId;
	else
		m_resId = (DWORD)(DWORD_PTR)resId;
	return _self;
}

AFX_API CResID& CResID::operator=(RCString resId) {
	m_resId = 0;
	m_name = resId;
	return _self;
}

CResID::operator const char *() const {
	if (m_name.IsEmpty())
		return (const char*)m_resId;
	else
		return m_name;
}

CResID::operator const String::Char *() const {
	if (m_name.IsEmpty())
		return (const String::Char *)m_resId;
	else
		return m_name;
}

#ifdef WIN32
CResID::operator UINT() const {
	return (UINT)(DWORD_PTR)(operator LPCTSTR());
}
#endif

String CResID::ToString() const {
	return m_name.IsEmpty() ? Convert::ToString((DWORD)m_resId) : m_name;
}

void CResID::Read(const BinaryReader& rd) {
	rd >> m_resId >> m_name;
}

void CResID::Write(BinaryWriter& wr) const {
	wr << m_resId << m_name;
}

CDynamicLibrary::~CDynamicLibrary() {
	Free();
}

void CDynamicLibrary::Load(RCString path) const {
#if UCFG_USE_POSIX
	m_hModule = ::dlopen(path, 0);
	DlCheck(m_hModule == 0);
#else
	Win32Check((m_hModule = LoadLibrary(path)) != 0);
#endif
}

void CDynamicLibrary::Free() {
	if (m_hModule) {
		HMODULE h = m_hModule;
		m_hModule = 0;
#if UCFG_USE_POSIX
		DlCheck(::dlclose(h));
#else
		Win32Check(::FreeLibrary(h));
#endif
	}
}

FARPROC CDynamicLibrary::GetProcAddress(const CResID& resID) {
	ExcLastStringArgKeeper argKeeper(resID.ToString());

	FARPROC proc;
#if UCFG_USE_POSIX
	proc = (FARPROC)::dlsym(_self, resID);
	DlCheck(proc == 0);
#else
	proc = ::GetProcAddress(_self, resID);
	Win32Check(proc != 0);
#endif
	return proc;
}

#if UCFG_WIN32

DlProcWrapBase::DlProcWrapBase(RCString dll, RCString funname) {
	Init(::GetModuleHandle(dll), funname);
}

void DlProcWrapBase::Init(HMODULE hModule, RCString funname) {
	m_p = ::GetProcAddress(hModule, funname);
}
#endif

int AFXAPI Rand() {
#if UCFG_USE_POSIX
	return (unsigned int)time(0) % ((unsigned int)RAND_MAX+1);
#else
	return int (System.PerformanceCounter % (RAND_MAX+1));
#endif
}


UInt16 Random::NextWord() {
	return UInt16((m_seed = m_seed * 214013L + 2531011L) >> 16);
}

void Random::NextBytes(const Buf& mb) {
	for (int i=0; i<mb.Size; i++)
		mb.P[i] = (byte)NextWord();
}

int Random::Next() {
	int v;
	NextBytes(Buf(&v, sizeof v));
	return abs(v);
}

int Random::Next(int maxValue) {
	return Next() % maxValue; //!!!
}

double Random::NextDouble() {
	STATIC_ASSERT(DBL_MANT_DIG < 64);

	UInt64 n;
	NextBytes(Buf((byte*)&n, sizeof n));

	n = (n >> (64 - (DBL_MANT_DIG-1))) | (UInt64(1) << (DBL_MANT_DIG-1));
	return ldexp(double(n), -(DBL_MANT_DIG-1)) - 1.0;
}

void CAnnoyer::OnAnnoy() {
	if (m_iAnnoy)
		m_iAnnoy->OnAnnoy();
}

void CAnnoyer::Request() {
	DateTime now = DateTime::UtcNow();
	if (now-m_prev > m_period) {
		OnAnnoy();
		m_prev = now;
		m_period += m_period;
	}
}

template <typename W>
hashval ComputeHashImp(HashAlgorithm& algo, Stream& stm) {
	W hash[8];
	algo.InitHash(hash);
	byte buf[16*sizeof(W)];
	UInt64 len = 0, counter;
	bool bLast = false;
	while (true) {
		ZeroStruct(buf);
		if (bLast) {
			counter = 0;
			break;
		}
		for (int i=0; i<_countof(buf); ++i) {
			int v = stm.ReadByte();
			if (bLast = (-1 == v)) {
				buf[i] = 0x80;
				break;
			}
			buf[i] = (byte)v;
			++len;
		}
		counter = len << 3;
		if (bLast && (len & (sizeof buf - 1)) < sizeof buf - int(bool(algo.IsHaifa)) - sizeof(W)*2) {
			if (!(len & (sizeof buf - 1)))
				counter = 0;
			break;
		}
		for (int j=0; j<16; ++j)
			((W*)buf)[j] = betoh(((W*)buf)[j]);
		algo.HashBlock(hash, buf, counter);
	}
	if (algo.IsHaifa)
		buf[sizeof buf - sizeof(W)*2- 1] = 1;
	len <<= 3;
	for (int i=0; i<8; ++i)
		buf[sizeof buf - 1 -i] = byte(len>>(i*8));
	for (int j=0; j<16; ++j)
		((W*)buf)[j] = betoh(((W*)buf)[j]);
	algo.HashBlock(hash, buf, counter);
	for (int j=0; j<8; ++j)
		hash[j] = htobe(hash[j]);
	return hashval((const byte*)hash, sizeof hash);
}

hashval HashAlgorithm::ComputeHash(Stream& stm) {
	return Is64Bit ? ComputeHashImp<UInt64>(_self, stm) : ComputeHashImp<UInt32>(_self, stm);
}

hashval HashAlgorithm::ComputeHash(const ConstBuf& mb) {
	CMemReadStream stm(mb);
	return ComputeHash(stm);
}


static UInt32 s_crcTable[256];

static bool Crc32GenerateTable() {
	UInt32 poly = 0xEDB88320;
	for (UInt32 i = 0; i < 256; i++) {
		UInt32 r = i;
		for (int j = 0; j < 8; j++)
			r = (r >> 1) ^ (poly & ~((r & 1) - 1));
		s_crcTable[i] = r;
	}
	return true;
}

hashval Crc32::ComputeHash(Stream& stm) {
	static once_flag once;
	call_once(once, &Crc32GenerateTable);

	UInt32 val = 0xFFFFFFFF;
	for (int v; (v=stm.ReadByte())!=-1;)
		val = s_crcTable[(val ^ v) & 0xFF] ^ (val >> 8);
	val = ~val;
	return hashval((const byte*)&val, sizeof val);
}

CMessageProcessor g_messageProcessor;


CMessageProcessor::CMessageProcessor() {
	m_default.Init(0, UInt32(-1), Path::GetFileNameWithoutExtension(System.ExeFilePath));
	///!!!  RegisterModule(E_EXT_BASE, E_EXT_UPPER, EXT_MODULE);
}

CMessageRange::CMessageRange() {
	g_messageProcessor.m_ranges.push_back(this);  
}

CMessageRange::~CMessageRange() {
	g_messageProcessor.m_ranges.erase(std::remove(g_messageProcessor.m_ranges.begin(), g_messageProcessor.m_ranges.end(), this), g_messageProcessor.m_ranges.end());
}

void CMessageProcessor::CModuleInfo::Init(UInt32 lowerCode, UInt32 upperCode, RCString moduleName) {
	m_lowerCode = lowerCode;
	m_upperCode = upperCode;
	m_moduleName = moduleName;
	if (!Path::HasExtension(m_moduleName)) {
#if UCFG_USE_POSIX
		m_moduleName += ".cat";
#elif UCFG_WIN32
		m_moduleName += ".dll";
#endif
	}
}

String CMessageProcessor::CModuleInfo::GetMessage(HRESULT hr) {
	if (UInt32(hr) < m_lowerCode || UInt32(hr) >= m_upperCode)
		return nullptr;
#if UCFG_USE_POSIX
	if (!m_mcat) {
		if (exchange(m_bCheckedOpen, true))
			return nullptr;
		try {
			m_mcat.Open(m_moduleName);
		} catch (RCExc) {
			if (Path::GetDirectoryName(m_moduleName) != "")
				return nullptr;
			try {
				m_mcat.Open(Path::Combine(Path::GetDirectoryName(System.ExeFilePath), m_moduleName));
			} catch (RCExc) {
				return nullptr;
			}
		}		
	}
	return m_mcat.GetMessage((hr>>16) & 0xFF, hr & 0xFFFF);
#elif UCFG_WIN32
	TCHAR buf[256];
	if (::FormatMessage(FORMAT_MESSAGE_FROM_HMODULE|FORMAT_MESSAGE_ARGUMENT_ARRAY, LPCVOID(GetModuleHandle(m_moduleName)), hr, 0, buf, sizeof buf, 0))
		return buf;
	else
		return nullptr;
#else
	return nullptr;
#endif
}

void CMessageProcessor::SetParam(RCString s) {
	g_messageProcessor.m_param.GetData()->m_string = s;
}

String AFXAPI AfxProcessError(HRESULT hr) {
	return CMessageProcessor::Process(hr);
}

#if UCFG_COM

HRESULT AFXAPI AfxProcessError(HRESULT hr, EXCEPINFO *pexcepinfo) {
	return CMessageProcessor::Process(hr, pexcepinfo);
}

HRESULT CMessageProcessor::Process(HRESULT hr, EXCEPINFO *pexcepinfo) {
	//!!!  HRESULT hr = ProcessOleException(e);
	if (pexcepinfo) {
		pexcepinfo->bstrDescription = Process(hr).AllocSysString();
		pexcepinfo->wCode = 0;
		pexcepinfo->scode = hr;
		return DISP_E_EXCEPTION;
	} else
		return hr;
}
#endif

void CMessageProcessor::RegisterModule(DWORD lowerCode, DWORD upperCode, RCString moduleName) {
	g_messageProcessor.CheckStandard();
	CModuleInfo info;
	info.Init(lowerCode, upperCode, moduleName);
	g_messageProcessor.m_vec.push_back(info);
}

String CMessageProcessor::ProcessInst(HRESULT hr) {
	CheckStandard();
	char hex[30];
	sprintf(hex, "Error %8.8lX", hr);
	TCHAR buf[256];
	char *ar[5] = {0, 0, 0, 0, 0};
	char **p = 0;
	String s = m_param->m_string;
	if (!s.IsEmpty()) {
		ar[0] = (char*)(const char*)s;
		p = ar;
	}
#if UCFG_WIN32
	if (::FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM|FORMAT_MESSAGE_ARGUMENT_ARRAY, 0, hr, 0, buf, sizeof buf, p))
		return String(hex)+":  "+buf;
	for (int i=0; i<m_ranges.size(); i++) {
		String msg = m_ranges[i]->CheckMessage(hr);
		if (!msg.IsEmpty())
			return String(hex)+L":  "+msg;
	}
	switch (HRESULT_FACILITY(hr)) {
	case FACILITY_INTERNET:
		if (FormatMessage(FORMAT_MESSAGE_FROM_HMODULE|FORMAT_MESSAGE_ARGUMENT_ARRAY, LPCVOID(GetModuleHandle(_T("urlmon.dll"))),
			hr, 0, buf, sizeof buf, 0))
			return String(hex)+":  "+buf;
		break;
	case FACILITY_WIN32:
		{
			WORD code = WORD(HRESULT_CODE(hr)); //!!!
/*!!!R
#	if !UCFG_WCE && UCFG_GUI
			if (code >= RASBASE && code <= RASBASEEND) {
				RasGetErrorString(code, buf, 256);
				return buf;
			} else
#	endif
			*/
			if (code >= INTERNET_ERROR_BASE && code <= INTERNET_ERROR_LAST) {
				if (FormatMessage(FORMAT_MESSAGE_FROM_HMODULE|FORMAT_MESSAGE_ARGUMENT_ARRAY, LPCVOID(GetModuleHandle(_T("wininet.dll"))),
					code, 0, buf, sizeof buf, 0))
					return String(hex)+":  "+buf;
			} else if (code>=WSABASEERR && code<WSABASEERR+1024) {
				if (FormatMessage(FORMAT_MESSAGE_FROM_HMODULE|FORMAT_MESSAGE_ARGUMENT_ARRAY,
					LPCVOID(_afxBaseModuleState.m_hCurrentResourceHandle),
					hr, 0, buf, sizeof buf, 0))
					return String(hex)+":  "+buf;
			}
			if (FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM|FORMAT_MESSAGE_ARGUMENT_ARRAY, 0, code, 0, buf, sizeof buf, 0))
				return String(hex)+L":  "+buf;
			return hex;
		}
		break;
	}
#endif // UCFG_WIN32

	for (vector<CModuleInfo>::iterator i(m_vec.begin()); i!=m_vec.end(); ++i) {
		String msg = i->GetMessage(hr);
		if (!!msg)
			return String(hex)+":  "+msg;
	}
	String msg = m_default.GetMessage(hr);
	if (!!msg)
		return msg;
#if UCFG_HAVE_STRERROR
	if (HRESULT_FACILITY(hr) == FACILITY_C) {
		if (const char *s = strerror(HRESULT_CODE(hr)))
			return String(hex)+":  "+s;
	}
#endif
#if UCFG_WIN32
	if (::FormatMessage(FORMAT_MESSAGE_FROM_HMODULE|FORMAT_MESSAGE_ARGUMENT_ARRAY, LPCVOID(_afxBaseModuleState.m_hCurrentResourceHandle), hr, 0, buf, sizeof buf, 0))
		return String(hex)+":  "+buf;
#endif
	return hex;
}

String CMessageProcessor::Process(HRESULT hr) {
	return g_messageProcessor.ProcessInst(hr);
}

const int //!!!E_EXT_BASE = 0x80040000 | 10000,
	E_EXT_UPPER = E_EXT_BASE+0xFFFF;

void CMessageProcessor::CheckStandard() {
#if UCFG_EXTENDED
	if (m_vec.size() == 0) {
		CModuleInfo info;
		info.m_lowerCode = E_EXT_BASE;
		info.m_upperCode = (DWORD)E_EXT_UPPER;

#ifdef _AFXDLL
		AFX_MODULE_STATE *pMS = AfxGetStaticModuleState();
#else
		AFX_MODULE_STATE *pMS = &_afxBaseModuleState;
#endif
		info.m_moduleName = Path::GetFileName(pMS->FileName);
		m_vec.push_back(info);
	}
#endif
}

bool PersistentCache::Lookup(RCString key, Blob& blob) {
	String fn = AfxGetCApp()->get_AppDataDir()+"/.cache";
	Directory::CreateDirectory(fn);
	fn = Path::Combine(fn, key);
	if (!File::Exists(fn))
		return false;
	FileStream fs(fn, FileMode::Open, FileAccess::Read);
	blob.Size = (size_t)fs.Length;
	fs.ReadBuffer(blob.data(), blob.Size);
	return true;
}

void PersistentCache::Set(RCString key, const ConstBuf& mb) {
	String fn = AfxGetCApp()->get_AppDataDir()+"/.cache";
	Directory::CreateDirectory(fn);
	fn = Path::Combine(fn, key);
	FileStream fs(fn, FileMode::Create, FileAccess::Write);
	fs.WriteBuffer(mb.P, mb.Size);

	/*!!!
#if UCFG_WIN32
	RegistryKey rk(AfxGetCApp()->KeyCU, "cache");
	rk.SetValue(key, mb);
#endif*/
}

#if UCFG_WIN32

ResourceObj::ResourceObj(HMODULE hModule, HRSRC hRsrc)
	:	m_hModule(hModule)
	,	m_hRsrc(hRsrc)
	,	m_p(0)
{
	Win32Check(bool(m_hglbResource = ::LoadResource(m_hModule, m_hRsrc)));
}

ResourceObj::~ResourceObj() {
#	if UCFG_WIN32_FULL
	Win32Check(! ::FreeResource(m_hglbResource));
#	endif
}
#endif // UCFG_WIN32

Resource::Resource(const CResID& resID, const CResID& resType, HMODULE hModule) {
#if UCFG_WIN32
	if (!hModule)
		hModule = AfxFindResourceHandle(resID, resType);
	HRSRC hRsrc = ::FindResource(hModule, resID, resType);
	Win32Check(hRsrc != 0);
	m_pimpl = new ResourceObj(hModule, hRsrc);
#else
	m_blob = File::ReadAllBytes(Path::Combine(Path::GetDirectoryName(System.ExeFilePath), resID.ToString()));
#endif
}

const void *Resource::get_Data() {
#if UCFG_WIN32
	if (!m_pimpl->m_p)
		m_pimpl->m_p = ::LockResource(m_pimpl->m_hglbResource);
	return m_pimpl->m_p;
#else
	return m_blob.constData();
#endif
}

size_t Resource::get_Size() {
#if UCFG_WIN32
	return Win32Check(::SizeofResource(m_pimpl->m_hModule, m_pimpl->m_hRsrc));
#else
	return m_blob.Size;
#endif
}

EXT_DATA const Temperature Temperature::Zero = Temperature::FromKelvin(0);

String Temperature::ToString() const {
	return Convert::ToString(ToCelsius())+" C";
}

EXT_API mutex g_mtxObjectCounter;
EXT_API CTiToCounter g_tiToCounter;

void AFXAPI LogObjectCounters(bool fFull) {
#if !defined(_DEBUG) || !defined(_M_X64)		//!!!TODO
	static int s_i;
	if (!(++s_i & 0x1FF) || fFull) {
		EXT_LOCK (g_mtxObjectCounter) {
			for (CTiToCounter::iterator it=g_tiToCounter.begin(), e=g_tiToCounter.end(); it!=e; ++it) {
				TRC(2, it->second << "\t" << it->first->name());
			}
		}
	}
#endif
}



} // Ext::

#if UCFG_USE_REGEX

EXT_API const std::regex_constants::syntax_option_type Ext::RegexTraits::DefaultB = std::regex_constants::ECMAScript;


namespace std {

EXT_API bool AFXAPI regex_searchImpl(Ext::String::const_iterator bs,  Ext::String::const_iterator es, Ext::Smatch *m, const wregex& re, bool bMatch, regex_constants::match_flag_type flags, Ext::String::const_iterator org) {
	wcmatch wm;
#if defined(_NATIVE_WCHAR_T_DEFINED) && UCFG_STRING_CHAR/8 == __SIZEOF_WCHAR_T__
	const wchar_t *b = &*bs, *e = &*es;
#else
	wstring ws(bs, es);
	const wchar_t *b = &*ws.begin(),
		          *e = &*ws.end();
#endif
	bool r = bMatch ? regex_match<const wchar_t*, wcmatch::allocator_type, wchar_t>(b, e, wm, re, flags)
					: regex_search<const wchar_t*, wcmatch::allocator_type, wchar_t>(b, e, wm, re, flags);
	if (r && m) {
		m->m_ready = true; //!!!? wm.ready();
		m->m_org = org;
		m->m_prefix.Init(wm.prefix(), m->m_org, b);
		m->m_suffix.Init(wm.suffix(), m->m_org, b);
		m->Resize(wm.size());
		for (int i=0; i<wm.size(); ++i)
			m->GetSubMatch(i).Init(wm[i], bs, b);
	}
	return r;
}

} // std::
#endif // UCFG_USE_REGEX




