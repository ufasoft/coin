/*######     Copyright (c) 1997-2013 Ufasoft  http://ufasoft.com  mailto:support@ufasoft.com,  Sergey Pavlov  mailto:dev@ufasoft.com #######################################
#                                                                                                                                                                          #
# This program is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation;  #
# either version 3, or (at your option) any later version. This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the      #
# implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details. You should have received a copy of the GNU #
# General Public License along with this program; If not, see <http://www.gnu.org/licenses/>                                                                               #
##########################################################################################################################################################################*/

#include <el/ext.h>


#pragma warning(disable: 4073)
#pragma init_seg(lib)				// to initialize DateTime::MaxValue early

#if UCFG_WIN32
#	include <winsock2.h>		// for timeval
#	include <unknwn.h>

#	include <el/libext/win32/ext-win.h>

#	if UCFG_COM
#		include <el/libext/win32/ext-com.h>
#	endif
#endif

#if UCFG_WDM
struct timeval {
	long    tv_sec;
	long    tv_usec;
};
#endif

#ifdef HAVE_SYS_TIME_H
#	include <sys/time.h>
#endif

#if UCFG_WCE
#	define mktime _mktime64
#endif

using namespace Ext;

namespace Ext {
using namespace std;

#if UCFG_WCE
	const Int64 TimeSpan::TicksPerMillisecond = 10000;
	const Int64 TimeSpan::TicksPerSecond = TimeSpan::TicksPerMillisecond * 1000;
	const Int64 TimeSpan::TicksPerMinute = TimeSpan::TicksPerSecond * 60;
	const Int64 TimeSpan::TicksPerHour = TimeSpan::TicksPerMinute * 60;
	const Int64 TimeSpan::TicksPerDay = TimeSpan::TicksPerHour * 24;

	const int DateTime::DaysPerYear = 365;
	const int DateTime::DaysPer4Years = DateTime::DaysPerYear * 4 + 1;
	const int DateTime::DaysPer100Years = DateTime::DaysPer4Years * 25 - 1;
	const int DateTime::DaysPer400Years = DateTime::DaysPer100Years * 4 + 1;
	const int DateTime::DaysTo1601 = DateTime::DaysPer400Years * 4;
	const Int64 DateTime::FileTimeOffset = DateTime::DaysTo1601 * TimeSpan::TicksPerDay;
#endif

const TimeSpan TimeSpan::MaxValue = numeric_limits<Int64>::max();

const DateTime DateTime::MaxValue = numeric_limits<Int64>::max();

const Int64 DateTime::TimevalOffset = 621355968000000000LL;


#if !UCFG_WDM

const Int64 DateTime::OADateOffset = DateTime(1899, 12, 30).Ticks;

TimeSpan::TimeSpan(const timeval& tv)
	:	DateTimeBase(tv.tv_sec*10000000+tv.tv_usec*10)
{
}

void TimeSpan::ToTimeval(timeval& tv) const {
	tv.tv_sec = long(m_ticks/10000000);
	tv.tv_usec = long((m_ticks % 10000000)/10);
}

static const Int64 Unix_FileTime_Offset = 116444736000000000LL,
	               Unix_DateTime_Offset = DateTime::FileTimeOffset+Unix_FileTime_Offset;

DateTime::DateTime(const timeval& tv) { 
	m_ticks = Int64(tv.tv_sec)*10000000 + Unix_DateTime_Offset+tv.tv_usec*10;
}

DateTime::DateTime(int year, int month, int day, int hour, int minute, int second, int ms) {
	SYSTEMTIME st = { (WORD)year, (WORD)month, 0, (WORD)day, (WORD)hour, (WORD)minute, (WORD)second, (WORD)ms };
	DateTime dt(st);
	_self = dt;
}

DateTime::DateTime(const tm& t) {
	tm _t = t;
	_self = FromUnix(mktime(&_t));
}

DateTime::DateTime(const SYSTEMTIME& st) {
#if UCFG_USE_POSIX
	tm t = { st.wSecond, st.wMinute, st.wHour, st.wDay, st.wMonth-1, st.wYear-1900 };
	_self = t;
#else
	FILETIME ft;
	Win32Check(::SystemTimeToFileTime(&st, &ft));
	_self = ft;
#endif
}

DateTime DateTime::FromUnix(Int64 epoch) {
	return DateTime(epoch*10000000 + Unix_DateTime_Offset);
}

Int64 DateTime::get_UnixEpoch() const {
	return (Ticks-Unix_DateTime_Offset)/10000000;
}

String TimeSpan::ToString(int w) const {
	ostringstream os;
	if (Days)
		os << Days << ".";
	os << setw(2) << setfill('0') << Hours << ":" << setw(2) << setfill('0') << Minutes << ":" << setw(2) << setfill('0') << Seconds;
	int fraction = int(Ticks % 10000000L);
	if (w || fraction) {
		int full = 10000000;
		os << ".";
		if (!w)
			w = 7;
		if (w > 7)
			w = 7;
		for (int i=0; i<w; i++)
			full /= 10;
		os << setw(w) << setfill('0') << fraction/full;
	}
	return os.str();
}

String DateTime::ToString(DWORD dwFlags, LCID lcid) const {
#if UCFG_USE_POSIX || !UCFG_OLE || UCFG_WDM
	tm t = _self;
	char buf[100];
	strftime(buf, sizeof buf, "%x %X", &t);
	return buf;
#else
	CComBSTR bstr;
	OleCheck(::VarBstrFromDate(ToOADate(), lcid, dwFlags, &bstr));
	return bstr;
#endif
}

String DateTime::ToString(Microseconds) const {
	ostringstream os;
#if UCFG_USE_POSIX || UCFG_WDM
	os << ToString();
#else
	os << ToString(VAR_DATEVALUEONLY, LOCALE_NEUTRAL);
#endif
	os << " " << get_TimeOfDay().ToString(6);
	return os.str();
}

#	if !UCFG_WCE
String DateTime::ToString(RCString format) const {
	tm t = _self;
	char buf[100];
	if (format == "u")
		strftime(buf, sizeof buf, "%Y-%m-%d %H:%M:%SZ", &t);
	else
		Throw(E_INVALIDARG);
	return buf;
}
#	endif

#	if UCFG_USE_REGEX

static StaticRegex	s_reDateTimeFormat_u("(\\d\\d\\d\\d)-(\\d\\d)-(\\d\\d) (\\d\\d):(\\d\\d):(\\d\\d)Z"),
					s_reDateTimeFormat_8601("(\\d\\d\\d\\d)-(\\d\\d)-(\\d\\d)T(\\d\\d):(\\d\\d):(\\d\\d)(?:\\.(\\d{1,6}))(?:Z|[+-](\\d\\d)(?::(\\d\\d))?)?");

DateTime AFXAPI DateTime::ParseExact(RCString s, RCString format) {
	cmatch m;
	if (format == nullptr) {
		if (regex_match(s.c_str(), m, *s_reDateTimeFormat_8601)) {
			DateTime dt(Convert::ToInt32(m[1]), Convert::ToInt32(m[2]), Convert::ToInt32(m[3]), Convert::ToInt32(m[4]), Convert::ToInt32(m[5]), Convert::ToInt32(m[6]));
			if (m[7].matched) {
				String s(m[7]);
				int n = atoi(String(m[7]));
				for (ssize_t i=7-m[7].length(); i--;)
					n *= 10;
				dt += TimeSpan(n);
			}
			return dt;						//!!!TODO adjust timezone
		}
	} else if (format == "u") {
		if (regex_match(s.c_str(), m, *s_reDateTimeFormat_u))
			return DateTime(Convert::ToInt32(m[1]), Convert::ToInt32(m[2]), Convert::ToInt32(m[3]), Convert::ToInt32(m[4]), Convert::ToInt32(m[5]), Convert::ToInt32(m[6]));
	}
	Throw(E_INVALIDARG);
}
#	endif // UCFG_USE_REGEX

#endif   // !UCFG_WDM

DateTime DateTime::FromAsctime(RCString s) {
#if UCFG_WDM
	Throw(E_NOTIMPL);
#else
	char month[4];
	int year, day;
	if (sscanf(s, "%3s %d %d", month, &day, &year) == 3) {
		static const char s_months[] = "JanFebMarAprMayJunJulAugSepOctNovDec";
		if (const char *p = strstr(s_months, month))
			return DateTime(year, int(p-s_months)/3+1, day);
	}
	Throw(E_EXT_InvalidInteger);
#endif
}

Int64 DateTime::SimpleUtc() {
#if UCFG_USE_POSIX
	timespec ts;
	CCheck(::clock_gettime(CLOCK_REALTIME, &ts));
	return FromUnix(ts.tv_sec).m_ticks+ts.tv_nsec/100;

//!!!O	timeval tv;
//!!!O	CCheck(::gettimeofday(&tv, 0));
//!!!O	return FromUnix(tv.tv_sec).m_ticks+tv.tv_usec*10;
#elif UCFG_WDM
	LARGE_INTEGER st;
	KeQuerySystemTime(&st);
	return st.QuadPart + s_span1600;
#else

	FILETIME ft;
#	if UCFG_WCE
	SYSTEMTIME st;
	::GetSystemTime(&st);
	Win32Check(::SystemTimeToFileTime(&st, &ft));
#	else
	::GetSystemTimeAsFileTime(&ft);
#	endif

	return (Int64&)ft + s_span1600;
#endif
}

CPreciseTimeBase *CPreciseTimeBase::s_pCurrent;

CPreciseTimeBase::CPreciseTimeBase()
	:	MAX_PERIOD(128*10000000)	// 128 s,  200 s is upper limit
	,	CORRECTION_PRECISION(500000)	// .05 s
	,	MAX_FREQ_INSTABILITY(10)		// 10 %
{
	Reset();
}

void CPreciseTimeBase::AddToList() {
	CPreciseTimeBase **pPlace = &s_pCurrent;
	while (*pPlace)
		pPlace = &(*pPlace)->m_pNext;
	*pPlace = this;
}

Int64 CPreciseTimeBase::GetFrequency(Int64 stPeriod, Int64 tscPeriod) {
	return tscPeriod > numeric_limits<Int64>::max()/10000000 || 0==stPeriod ? 0 : tscPeriod*10000000/stPeriod;
}

bool CPreciseTimeBase::Recalibrate(Int64 st, Int64 tsc, Int64 stPeriod, Int64 tscPeriod, bool bResetBase) {

	InterlockedSemaphore lck(m_mtx32Calibrate);
	if (m_period && lck.TryLock()) {		// Inited
		if (bResetBase)
			ResetBounds();

		Int64 cntPeriod = tscPeriod & m_mask;
		Int64 freq;
		if (stPeriod > 0 && cntPeriod > 0 && (freq = GetFrequency(stPeriod, cntPeriod))) {
			Int64 prevMul = Int64(m_mul) << (32-m_shift);
			
			int shift = std::min(int(m_shift), BitLen((UInt64)freq));
			Int32 mul = Int32((10000000LL << shift)/freq);
			m_shift = shift;
			m_mul = mul;

			m_minFreq = std::min(m_minFreq, freq);
			m_maxFreq = std::max(m_maxFreq, freq);

			TRC(3, "Fq=" << m_minFreq << " < " << freq << "(" << m_mul << ", " << m_shift << ") < " << m_maxFreq << " Hz Spread: " << (m_maxFreq-m_minFreq)*100/m_maxFreq << " %");

			if ((m_maxFreq-m_minFreq)*(100/MAX_FREQ_INSTABILITY) > m_maxFreq) {
				TRC(2, "Switching PreciseTime");
				if (s_pCurrent == this) {
					s_pCurrent = m_pNext;
					return true;
				}
			}

			if (bResetBase)
				m_period = std::min(Int64(m_period)*2, MAX_PERIOD);

		} else if (stPeriod < 0 || cntPeriod < 0) {
			TRC(2, "Some period is Negative");
		}

		if (bResetBase) {
			m_stBase = st;
			m_tscBase = tsc;
		}
	}
	return false;
}

#if UCFG_USE_DECLSPEC_THREAD
__declspec(thread) int s_cntDisablePreciseTime;
#else
static volatile Int32 s_cntDisablePreciseTime;
#endif

class CDisablePreciseTimeKeeper {
public:
	CDisablePreciseTimeKeeper() {
#if UCFG_USE_DECLSPEC_THREAD
		++s_cntDisablePreciseTime;
#else
		Interlocked::Increment(s_cntDisablePreciseTime);
#endif
	}

	~CDisablePreciseTimeKeeper() {
#if UCFG_USE_DECLSPEC_THREAD
		--s_cntDisablePreciseTime;
#else
		Interlocked::Decrement(s_cntDisablePreciseTime);
#endif
	}
};


Int64 CPreciseTimeBase::GetTime(PFNSimpleUtc pfnSimpleUtc) {
	Int64 tsc = GetTicks(),
			st = pfnSimpleUtc();
#if UCFG_TRC
	Int64 tscAfter = GetTicks();
#endif

	CDisablePreciseTimeKeeper disablePreciseTimeKeeper;

	Int64 r = st;
	bool bSwitch = false;
	if (m_stBase) {
		Int64 stPeriod = st-m_stBase,
			tscPeriod = tsc-m_tscBase;
		Int64 ct = m_stBase+(((tscPeriod & m_mask)*m_mul)>>m_shift);
		bool bResetBase = MyAbs(stPeriod) > m_period;
		if (!bResetBase && MyAbs(st-ct) < CORRECTION_PRECISION)
			r = ct;		
		else {
			TRC(3, "ST-diff " << stPeriod << "  CT-diff " << (st-(m_stBase+(((tscPeriod & m_mask)*m_mul)>>m_shift)))/10000 << "ms TSC-diff = " << tscAfter-tsc << "   Prev Recalibration: " << stPeriod/10000000 << " s ago");
			bSwitch = Recalibrate(st, tsc, stPeriod, tscPeriod, bResetBase);
		}
	} else {
		m_tscBase = tsc;
		m_stBase = st;  			// Race: this assignment should be last
	}
	if (bSwitch) {
		if (s_pCurrent)
			r = s_pCurrent->GetTime(pfnSimpleUtc);
	} else if (r < m_stPrev) {
		Int64 stPrev = m_stPrev;
		if (stPrev-r > 10*10000000) {
			TRC(3, "Time Anomaly: " << r-m_stPrev << "  stPrev = " << hex << stPrev << " r=" << hex << r);

			m_stPrev = r;
		} else {
			r = m_stPrev = m_stPrev+1;
		}
	} else {
		m_stPrev = r;
	}
	return r;
}

#	if UCFG_CPU_X86_X64
static class CTscPreciseTime : public CPreciseTimeBase {
public:
	CTscPreciseTime() {
		if (CpuInfo().ConstantTsc)
			AddToList();
	}

	Int64 GetTicks() override {
		return __rdtsc();
	}
} s_tscPreciseTime;

#if defined(_DEBUG) && defined(_MSC_VER)
__declspec(dllexport) void __cdecl Debug_ResetTsc() {
	return s_tscPreciseTime.Reset();
}

__declspec(dllexport) Int32 __cdecl Debug_GetTscMul() {
	return s_tscPreciseTime.m_mul;
}
#endif // _DEBUG && defined(_MSC_VER)

#	endif  // UCFG_CPU_X86_X64

#ifdef _WIN32

static class CPerfCounterPreciseTime : public CPreciseTimeBase {
	typedef CPreciseTimeBase base;
public:
	CPerfCounterPreciseTime() {
		LARGE_INTEGER liFreq;
#if UCFG_WIN32
		if (!::QueryPerformanceFrequency(&liFreq))
			return;
#else
		KeQueryPerformanceCounter(&liFreq);
#endif
		if (liFreq.QuadPart)
			AddToList();
	}

	Int64 GetTicks() override {
		return System.PerformanceCounter;
	}

	Int64 GetFrequency(Int64 stPeriod, Int64 tscPeriod) override {
		Int64 freq = System.PerformanceFrequency;
		TRC(3, "Declared Freq: " << freq << " Hz");
//!!!?not accurate		return freq;
		return base::GetFrequency(stPeriod, tscPeriod);
	}
} s_perfCounterPreciseTime;


#endif // _WIN32

/*!!!R
inline DateTime DateTime::PreciseCorrect(Int64 ticks) {
	if (CPreciseTimeBase *preciser = CPreciseTimeBase::s_pCurrent)		// get pointer to avoid Race Condition
		return preciser->GetTime(ticks);
	return ticks;
}*/

DateTime DateTime::UtcNow() {
#if !UCFG_USE_POSIX
	if (!s_cntDisablePreciseTime) {
		if (CPreciseTimeBase *preciser = CPreciseTimeBase::s_pCurrent)		// get pointer to avoid Race Condition
			return preciser->GetTime(&SimpleUtc);
	}
#endif
	return SimpleUtc();
}

LocalDateTime DateTime::ToLocalTime() {
#if UCFG_USE_POSIX
	timeval tv;
	ToTimeval(tv);
	tm g = *gmtime(&tv.tv_sec);
	g.tm_isdst = 0;
	time_t t2 = mktime(&g);
	return LocalDateTime(DateTime(Ticks+Int64(tv.tv_sec-t2)*10000000));
#elif UCFG_WDM
	LARGE_INTEGER st, lt;
	st.QuadPart = Ticks-FileTimeOffset;
	ExSystemTimeToLocalTime(&st, &lt);
	return LocalDateTime(DateTime(lt.QuadPart+FileTimeOffset));
#else
	FILETIME ft = ToFileTime();
	FileTimeToLocalFileTime(&ft, &ft);
	return LocalDateTime(DateTime(ft));
#endif
}


LocalDateTime DateTime::Now() {
	return UtcNow().ToLocalTime();
}


#ifdef WIN32

DateTime::operator SYSTEMTIME() const {
	SYSTEMTIME st;
	Win32Check(::FileTimeToSystemTime(&ToFileTime(), &st));
	return st;
}

#if UCFG_COM
DATE DateTime::ToOADate() const {
	return double(Ticks-OADateOffset)/TimeSpan::TicksPerDay;
}

DateTime DateTime::FromOADate(DATE date) {
	return OADateOffset+Int64(date*TimeSpan::TicksPerDay);
}

DateTime DateTime::Parse(RCString s, DWORD dwFlags, LCID lcid) {
	DATE date;
	const OLECHAR *pS = s;
	OleCheck(::VarDateFromStr((OLECHAR*)pS, lcid, dwFlags, &date));
	return FromOADate(date);
}
#endif

#	if !UCFG_WCE && UCFG_OLE

DateTime::DateTime(const COleVariant& v) {
	if (v.vt != VT_DATE)
		Throw(E_FAIL);
	_self = FromOADate(v.date);
}


#	endif

#endif

/*!!!D
String DateTime::ToString() const
{
  FILETIME ft;
  (Int64&)ft = m_ticks-s_span1600;
	if ((Int64&)ft < 0)
		(Int64&)ft = 0; //!!!
	ATL::COleDateTime odt(ft);
  return odt.Format();
}
*/


/*!!!D
static timeval FileTimeToTimeval(const FILETIME & ft)
{
  timeval tv;
  tv.tv_sec = long(((Int64&)ft - 116444736000000000) / 10000000);
  tv.tv_usec = long(((Int64&)ft % 10000000)/10);
  return tv;
}
*/

#if defined(_WIN32) && defined(_M_IX86)

__declspec(naked) int __fastcall UnsafeDiv64by32(Int64 n, int d, int& r) {
	__asm {
		push	EDX
		mov		EAX, [ESP+8]
		mov		EDX, [ESP+12]
		idiv	ECX
		pop		ECX
		mov		[ECX], EDX
		ret		8
	}
}

#	if !UCFG_WDM
void DateTime::ToTimeval(timeval& tv) const {
	ZeroStruct(tv);
	int r;
	_try {
		tv.tv_sec = UnsafeDiv64by32(Ticks-TimevalOffset, 10000000, r);		//!!! int overflow possible for some dates
		tv.tv_usec = r/10;
	} _except(EXCEPTION_EXECUTE_HANDLER) {
	}
}


#	endif

#	if UCFG_USE_PTHREADS

DateTime::operator timespec() const {
	timespec ts = { 0 };
	int r;
	_try {
		ts.tv_sec = UnsafeDiv64by32(Ticks-TimevalOffset, 10000000, r);		//!!! int overflow possible for some dates
		ts.tv_nsec = r*100;
	} _except(EXCEPTION_EXECUTE_HANDLER) {
	}
	return ts;
}
#	endif

#else // defined(_WIN32) && defined(_M_IX86)

#	if UCFG_USE_PTHREADS

DateTime::operator timespec() const {
	Int64 t = Ticks-TimevalOffset;
	timespec tv;
	tv.tv_sec = long(t/10000000);
	tv.tv_nsec = long((t % 10000000)*100);
	return tv;
}
#	endif

#	if UCFG_WIN32 || UCFG_USE_PTHREADS
void DateTime::ToTimeval(timeval& tv) const {
	Int64 t = Ticks-TimevalOffset;
	tv.tv_sec = long(t/10000000);
	tv.tv_usec = long((t % 10000000)/10);
}
#	endif

#endif // defined(_WIN32) && defined(_M_IX86)

#if !UCFG_WDM
DateTime::operator tm() const {
	timeval tv;
	ToTimeval(tv);
	time_t tim = tv.tv_sec;
	return *gmtime(&tim);
}
#endif

DateTime LocalDateTime::ToUniversalTime() {
#if UCFG_USE_POSIX
	timeval tv;
	ToTimeval(tv);
	tm g = *gmtime(&tv.tv_sec);
	g.tm_isdst = 0;
	time_t t2 = mktime(&g);
	return DateTime(Ticks-Int64(tv.tv_sec-t2)*10000000);
#elif UCFG_WDM
	LARGE_INTEGER st, lt;
	lt.QuadPart = Ticks-FileTimeOffset;
	ExLocalTimeToSystemTime(&lt, &st);
	return DateTime(st.QuadPart+FileTimeOffset);
#else
	FILETIME ft = ToFileTime();
	LocalFileTimeToFileTime(&ft, &ft);
	return ft;
#endif
}


#if !UCFG_WDM

TimeZoneInfo TimeZoneInfo::Local() {
	TimeZoneInfo tzi;
#if UCFG_USE_POSIX
	tzi.BaseUtcOffset = TimeSpan(Int64(timezone)*10000000);
#else
	TIME_ZONE_INFORMATION info;
	Win32Check(TIME_ZONE_ID_INVALID != ::GetTimeZoneInformation(&info));
	tzi.BaseUtcOffset = TimeSpan(-Int64(info.Bias) * 60*10000000);
#endif
	return tzi;
}

#endif // !UCFG_WDM

} // Ext::


//From January 1, 1601 (UTC). to January 1, 1970

extern "C" {

#if UCFG_WIN32_FULL
/*
* Returns the difference between gmt and local time in seconds.
* Use gmtime() and localtime() to keep things simple.
*/
__int32 _cdecl
	gmt2local(time_t t)
{
	register int dt, dir;
	register struct tm *gmt, *loc;
	struct tm sgmt;

	if (t == 0)
		t = time(NULL);
	gmt = &sgmt;
	*gmt = *gmtime(&t);
	loc = localtime(&t);
	dt = (loc->tm_hour - gmt->tm_hour) * 60 * 60 +
		(loc->tm_min - gmt->tm_min) * 60;

	/*
	* If the year or julian day is different, we span 00:00 GMT
	* and must add or subtract a day. Check the year first to
	* avoid problems when the julian day wraps.
	*/
	dir = loc->tm_year - gmt->tm_year;
	if (dir == 0)
		dir = loc->tm_yday - gmt->tm_yday;
	dt += dir * 24 * 60 * 60;

	return (dt);
}



int __stdcall gettimeofday(struct timeval *tp, void *) {
	Ext::Int64 res = DateTime::UtcNow().Ticks-Unix_DateTime_Offset;	
	tp->tv_sec = (long)(res/10000000);	//!!! can be overflow
	tp->tv_usec = (long)(res % 10000000) / 10; // Micro Seconds
	return 0;

}

#endif // UCFG_WIN32_FULL

} // "C"


