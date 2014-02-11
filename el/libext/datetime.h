/*######     Copyright (c) 1997-2013 Ufasoft  http://ufasoft.com  mailto:support@ufasoft.com,  Sergey Pavlov  mailto:dev@ufasoft.com #######################################
#                                                                                                                                                                          #
# This program is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation;  #
# either version 3, or (at your option) any later version. This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the      #
# implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details. You should have received a copy of the GNU #
# General Public License along with this program; If not, see <http://www.gnu.org/licenses/>                                                                               #
##########################################################################################################################################################################*/

#pragma once

struct timeval;

namespace Ext {

enum Microseconds {
};

class DateTimeBase {
	typedef DateTimeBase class_type;
public:
	Int64 get_Ticks() const { return m_ticks; }
	DEFPROP_GET_CONST(Int64, Ticks);

	void Write(BinaryWriter& wr) const {
		wr << m_ticks;
	}

	void Read(const BinaryReader& rd) {
		m_ticks = rd.ReadInt64();
	}
protected:
	Int64 m_ticks;	

	DateTimeBase(Int64 ticks = 0)
		:	m_ticks(ticks)
	{}

	friend BinaryReader& AFXAPI operator>>(BinaryReader& rd, DateTimeBase& dt);
};

class TimeSpan : public DateTimeBase {
	typedef DateTimeBase base;	
public:
	typedef TimeSpan class_type;

#if UCFG_WCE
	static const EXT_DATA Int64 TicksPerMillisecond;
	static const EXT_DATA Int64 TicksPerSecond;
	static const EXT_DATA Int64 TicksPerMinute;
	static const EXT_DATA Int64 TicksPerHour;
	static const EXT_DATA Int64 TicksPerDay;
#else
	static const Int64 TicksPerMillisecond = 10000;
	static const Int64 TicksPerSecond = TicksPerMillisecond * 1000;
	static const Int64 TicksPerMinute = TicksPerSecond * 60;
	static const Int64 TicksPerHour = TicksPerMinute * 60;
	static const Int64 TicksPerDay = TicksPerHour * 24;
#endif

	static const EXT_DATA TimeSpan MaxValue;

	TimeSpan(Int64 ticks = 0)
		:	base(ticks)
	{}

	TimeSpan(const TimeSpan& span)
		:	base(span.Ticks)
	{}

	TimeSpan(int days, int hours, int minutes, int seconds)
		:	base((((days*24+hours)*60+minutes)*60+seconds)*10000000LL)
	{}

#if UCFG_WIN32
	TimeSpan(const FILETIME& ft)
		:	base((Int64&)ft)
	{}
#endif

#ifndef WDM_DRIVER
	TimeSpan(const timeval& tv);
	void ToTimeval(timeval& tv) const;
#endif

#if UCFG_USE_POSIX
	TimeSpan(const timespec& ts)
		:	base(Int64(ts.tv_sec)*10000000+ts.tv_nsec/100)
	{
	}
#endif

	TimeSpan operator+(const TimeSpan& span) const { return m_ticks+span.Ticks; }
	TimeSpan& operator+=(const TimeSpan& span) { m_ticks+=span.Ticks; return *this;}

	TimeSpan operator-(const TimeSpan& span) const {return m_ticks-span.m_ticks; }
	TimeSpan& operator-=(const TimeSpan& span) { m_ticks-=span.Ticks; return *this;}

	TimeSpan operator*(double v) const {
		return Int64(m_ticks*v);
	}

	TimeSpan operator/(double v) const {
		return Int64(m_ticks/v);
	}

	double get_TotalMilliseconds() const { return double(m_ticks)/10000; }
	DEFPROP_GET_CONST(double, TotalMilliseconds);

	double get_TotalSeconds() const { return double(m_ticks)/10000000; }
	DEFPROP_GET_CONST(double, TotalSeconds);

	double get_TotalMinutes() const { return double(m_ticks)/(60LL*10000000); }
	DEFPROP_GET_CONST(double, TotalMinutes);

	double get_TotalHours() const { return double(m_ticks)/(60LL*60*10000000); }
	DEFPROP_GET_CONST(double, TotalHours);

	int get_Days() const { return int(TotalSeconds/(3600*24)); }
	DEFPROP_GET_CONST(int, Days);

	int get_Hours() const { return int(Ticks/(3600LL*10000000)%24); }
	DEFPROP_GET(int, Hours);

	int get_Minutes() const { return int(Ticks/(60LL*10000000)%60); }
	DEFPROP_GET(int, Minutes);

	int get_Seconds() const { return int(Ticks/10000000%60); }
	DEFPROP_GET(int, Seconds);

	int get_Milliseconds() const { return int(Ticks/10000%1000); }
	DEFPROP_GET(int, Milliseconds);

	String ToString(int w = 0) const;

	static TimeSpan AFXAPI FromMilliseconds(double value) { return Int64(value * 10000); }		//!!! behavior from .NET, where value rounds to milliseconds
	static TimeSpan AFXAPI FromSeconds(double value)	{ return Int64(value * 10000000); }
	static TimeSpan AFXAPI FromMinutes(double value)	{ return FromSeconds(value * 60); }
	static TimeSpan AFXAPI FromHours(double value)		{ return FromSeconds(value * 3600); }
	static TimeSpan AFXAPI FromDays(double value)		{ return FromSeconds(value * 3600*24); }

	friend class DateTime;
};

inline bool AFXAPI operator==(const TimeSpan& span1, const TimeSpan& span2) { return span1.Ticks == span2.Ticks; }
inline bool AFXAPI operator!=(const TimeSpan& span1, const TimeSpan& span2) { return !(span1 == span2); }
inline bool AFXAPI operator<(const TimeSpan& span1, const TimeSpan& span2) { return span1.Ticks < span2.Ticks; }
inline bool AFXAPI operator>(const TimeSpan& span1, const TimeSpan& span2) { return span2 < span1; }
inline bool AFXAPI operator<=(const TimeSpan& span1, const TimeSpan& span2) { return span1.Ticks <= span2.Ticks; }
inline bool AFXAPI operator>=(const TimeSpan& span1, const TimeSpan& span2) { return span2 <= span1; }

class DateTime;

inline bool AFXAPI operator<(const DateTime& dt1, const DateTime& dt2);
inline bool AFXAPI operator>(const DateTime& dt1, const DateTime& dt2);
inline bool AFXAPI operator==(const DateTime& dt1, const DateTime& dt2);

ENUM_CLASS(DayOfWeek) {
	Sunday, Monday, Tuesday, Wednesday, Thursday, Friday, Saturday
} END_ENUM_CLASS(DayOfWeek);

class LocalDateTime;

class DateTime : public DateTimeBase {
	typedef DateTimeBase base;
public:
	typedef DateTime class_type;

	static const Int64 s_span1600 = ((Int64)10000000*3600*24)*(365*1600+388); //!!!+day/4; //!!! between 0 C.E. to 1600 C.E. as MS calculates  

	static const EXT_DATA DateTime MaxValue;

	DateTime() {
	}

	DateTime(const DateTime& dt)
		:	base(dt.m_ticks)
	{}

	DateTime(Int64 ticks)
		:	base(ticks)
	{}

	DateTime(const FILETIME& ft)
		:	base((Int64&)ft + FileTimeOffset)
	{
	}

#if !UCFG_WDM

	DateTime(const SYSTEMTIME& st);
	DateTime(const timeval& tv);

	int get_DayOfYear() const { return tm(*this).tm_yday+1; }
	DEFPROP_GET(int, DayOfYear);

#endif
	void ToTimeval(timeval& tv)  const;
	operator tm() const;

	static Int64 AFXAPI SimpleUtc();
	static DateTime AFXAPI from_time_t(Int64 epoch);
	static DateTime AFXAPI FromAsctime(RCString s);

	int get_Hour() const { 
#ifdef WIN32
		return SYSTEMTIME(*this).wHour;
#else
		return int(m_ticks/(Int64(10000)*1000*3600) % 24);
#endif
	}
	DEFPROP_GET(int, Hour);

	int get_Minute() const { 
#ifdef WIN32
		return SYSTEMTIME(*this).wMinute;
#else
		return int(m_ticks/(10000*1000*60) % 60);
#endif
	}
	DEFPROP_GET(int, Minute);

	int get_Second() const { 
#ifdef WIN32
		return SYSTEMTIME(*this).wSecond;
#else
		return int(m_ticks/(10000*1000) % 60);
#endif
	}
	DEFPROP_GET(int, Second);

	int get_Millisecond() const { 
#ifdef WIN32
		return SYSTEMTIME(*this).wMilliseconds;
#else
		return int(m_ticks/10000 % 1000);
#endif
	}
	DEFPROP_GET(int, Millisecond);


#ifdef WIN32

	DateTime(const COleVariant& v);

	FILETIME ToFileTime() const {
		Int64 n = m_ticks - FileTimeOffset;
		return (FILETIME&)n;
	}

	operator FILETIME() const { return ToFileTime(); }

	operator SYSTEMTIME() const;

	
#	if UCFG_COM
	DATE ToOADate() const;
	static DateTime AFXAPI FromOADate(DATE date);
#	endif

#endif

	DateTime(int year, int month, int day, int hour = 0, int minute = 0, int second = 0, int ms = 0);
	DateTime(const tm& t);

	/*!!!
	DateTime& operator=(const DateTime& dt)
	{
	m_ticks = dt.m_ticks;
	return _self;
	}*/

	DateTime operator+(const TimeSpan& span) const { return m_ticks+span.m_ticks; }
	DateTime& operator+=(const TimeSpan& span) { return *this = *this + span; }

	DateTime operator-(const TimeSpan& span) const { return m_ticks-span.m_ticks; }
	DateTime& operator-=(const TimeSpan& span) { return *this = *this - span; }

	TimeSpan operator-(const DateTime& dt) const { return m_ticks-dt.m_ticks; }

	LocalDateTime ToLocalTime();
	static LocalDateTime AFXAPI Now();
	static DateTime AFXAPI UtcNow() noexcept;

	/*!!!
	static DateTime AFXAPI get_PreciseNow();
	STATIC_PROPERTY_DEF_GET(DateTime, DateTime, PreciseNow);

	static DateTime AFXAPI get_PreciseUtcNow();
	STATIC_PROPERTY_DEF_GET(DateTime, DateTime, PreciseUtcNow);
	*/

#if UCFG_USE_PTHREADS
	operator timespec() const;
#endif

	int get_Day() const {
#if UCFG_USE_POSIX
		return tm(_self).tm_mday;
#elif !defined(WDM_DRIVER)
		return SYSTEMTIME(*this).wDay;
#else
		Throw(E_FAIL);
#endif
	}
	DEFPROP_GET(int, Day);

	TimeSpan get_TimeOfDay() const { return Ticks%(24LL*3600*10000000); }
	DEFPROP_GET_CONST(TimeSpan, TimeOfDay);

	DateTime get_Date() const { return *this - TimeOfDay; }
	DEFPROP_GET_CONST(DateTime, Date);

	Ext::DayOfWeek get_DayOfWeek() const {
#if UCFG_USE_POSIX
		return Ext::DayOfWeek(tm(_self).tm_wday);
#elif !defined(WDM_DRIVER)
		return Ext::DayOfWeek(SYSTEMTIME(*this).wDayOfWeek);
#else
		Throw(E_FAIL);
#endif
	 }
	DEFPROP_GET_CONST(Ext::DayOfWeek, DayOfWeek);

	int get_Month() const {
#if UCFG_USE_POSIX
		return tm(_self).tm_mon+1;
#elif !defined(WDM_DRIVER)
		return SYSTEMTIME(*this).wMonth;
#else
		Throw(E_FAIL);
#endif

	}
	DEFPROP_GET_CONST(int, Month);

	int get_Year() const {
#if UCFG_USE_POSIX
		return tm(_self).tm_year+1900;
#elif !defined(WDM_DRIVER)
		return SYSTEMTIME(*this).wYear;
#else
		Throw(E_FAIL);
#endif
	}
	DEFPROP_GET(int, Year);
	
	String ToString(DWORD dwFlags = 0, LCID lcid = LANG_USER_DEFAULT) const;
	String ToString(Microseconds) const;
	String ToString(RCString format) const;

	static DateTime AFXAPI Parse(RCString s, DWORD dwFlags = 0, LCID lcid = LANG_USER_DEFAULT);
	static DateTime AFXAPI ParseExact(RCString s, RCString format = nullptr);

#if UCFG_WCE
	static const int DaysPerYear;
	// Number of days in 4 years
	static const int DaysPer4Years;
	// Number of days in 100 years
	static const int DaysPer100Years;
	// Number of days in 400 years
	static const int DaysPer400Years;
	static const int DaysTo1601;
	static const Int64 FileTimeOffset;
#else
	static const int DaysPerYear = 365;
	// Number of days in 4 years
	static const int DaysPer4Years = DaysPerYear * 4 + 1;
	// Number of days in 100 years
	static const int DaysPer100Years = DaysPer4Years * 25 - 1;
	// Number of days in 400 years
	static const int DaysPer400Years = DaysPer100Years * 4 + 1;
	static const int DaysTo1601 = DaysPer400Years * 4;
	static const Int64 FileTimeOffset = DaysTo1601 * TimeSpan::TicksPerDay;
#endif	

private:
	static const Int64 TimevalOffset;
	static const Int64 OADateOffset;

//!!!R	static void PreciseSync();
//!!!R	static DateTime PreciseCorrect(Int64 ticks);
};

inline bool AFXAPI operator<(const DateTime& dt1, const DateTime& dt2) { return dt1.Ticks < dt2.Ticks; }
inline bool AFXAPI operator>(const DateTime& dt1, const DateTime& dt2) { return dt2 < dt1; }
inline bool AFXAPI operator<=(const DateTime& dt1, const DateTime& dt2) { return !(dt2 < dt1); }
inline bool AFXAPI operator>=(const DateTime& dt1, const DateTime& dt2) { return !(dt1 < dt2); }
inline bool AFXAPI operator==(const DateTime& dt1, const DateTime& dt2) { return dt1.Ticks == dt2.Ticks; }
inline bool AFXAPI operator!=(const DateTime& dt1, const DateTime& dt2) { return !(dt1 == dt2); }


inline BinaryWriter& AFXAPI operator<<(BinaryWriter& wr, const DateTimeBase& dt) {
	dt.Write(wr);
	return wr;
}

inline BinaryReader& AFXAPI operator>>(BinaryReader& rd, DateTimeBase& dt) {
	dt.Read(rd);
	return rd;
}

inline std::ostream& AFXAPI operator<<(std::ostream& os, const Ext::TimeSpan& span) {
	return os << span.ToString();
}

inline std::ostream& AFXAPI operator<<(std::ostream& os, const Ext::DateTime& dt) {
	return os << dt.ToString();
}

class LocalDateTime : public DateTime {
	typedef DateTime base;
public:
	int OffsetSeconds;
	
	LocalDateTime()
		:	OffsetSeconds(0)
	{}

	explicit LocalDateTime(const DateTime& dt, int off = 0)
		:	base(dt)
		,	OffsetSeconds(off)
	{}

	DateTime ToUniversalTime();
};

class TimeZoneInfo {
public:
	static TimeZoneInfo AFXAPI Local();

	TimeSpan BaseUtcOffset;
};

typedef Int64 (AFXAPI *PFNSimpleUtc)();

class CPreciseTimeBase {
public:
	static CPreciseTimeBase *s_pCurrent;
	Int64 m_mask;
	volatile Int32 m_mul;
	volatile int m_shift;

	virtual ~CPreciseTimeBase() {}
	Int64 GetTime(PFNSimpleUtc pfnSimpleUtc = &DateTime::SimpleUtc);

	void Reset() {
		m_period = 200000;			// 0.02 s
		m_stBase = 0;
		m_tscBase = 0;
		m_stPrev = 0;
		m_shift = 32;
		m_pNext = 0;
		m_mask = -1;

		ResetBounds();
	}
protected:
	volatile Int32 m_mtx32Calibrate;

	const Int64 MAX_PERIOD,
				CORRECTION_PRECISION,
				MAX_FREQ_INSTABILITY;

	volatile Int64 m_stBase, m_tscBase, m_stPrev;
	volatile Int64 m_period;
	CPreciseTimeBase *m_pNext;
	Int64 m_minFreq, m_maxFreq;

	CPreciseTimeBase();

	void ResetBounds() {
		m_mul = 0;
		m_minFreq = (std::numeric_limits<Int64>::max)();
		m_maxFreq = 0;
	}

	static inline Int64 MyAbs(Int64 v) {
		return v<0 ? -v : v;
	}

	static inline int BitLen(UInt64 n) {
		int r = 0;
		for (; n; ++r)
			n >>= 1;
		return r;
	}

	void AddToList();

	bool Recalibrate(Int64 st, Int64 tsc, Int64 stPeriod, Int64 tscPeriod, bool bResetBase);

	virtual Int64 GetTicks() noexcept =0;
	virtual Int64 GetFrequency(Int64 stPeriod, Int64 tscPeriod);
};

class MeasureTime {
public:
	DateTime Start;
	TimeSpan& Span;

	MeasureTime(TimeSpan& span)
		:	Start(DateTime::UtcNow())
		,	Span(span)
	{}

	TimeSpan End() {
		return Span = DateTime::UtcNow()-Start;
	}

	~MeasureTime() {
		End();
	}
};

Int64 AFXAPI to_time_t(const DateTime& dt);



} // Ext::

