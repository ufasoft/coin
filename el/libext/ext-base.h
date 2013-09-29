/*######     Copyright (c) 1997-2013 Ufasoft  http://ufasoft.com  mailto:support@ufasoft.com,  Sergey Pavlov  mailto:dev@ufasoft.com #######################################
#                                                                                                                                                                          #
# This program is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation;  #
# either version 3, or (at your option) any later version. This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the      #
# implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details. You should have received a copy of the GNU #
# General Public License along with this program; If not, see <http://www.gnu.org/licenses/>                                                                               #
##########################################################################################################################################################################*/

#pragma once

#include EXT_HEADER(map)

namespace Ext {

class CCriticalSection;
class Exc;

class CPrintable {
public:
	virtual ~CPrintable() {}
	virtual String ToString() const =0;
};


#if !UCFG_WCE
class CStackTrace : public CPrintable {
public:
	EXT_DATA static bool Use;

	int AddrSize;
	std::vector<UInt64> Frames;

	CStackTrace()
		:	AddrSize(sizeof(void*))
	{}

	static CStackTrace AFXAPI FromCurrentThread();
	String ToString() const;
};
#endif

class Exc : public CPrintable {
public:
	typedef Exc class_type;

	static tls_ptr<String> t_LastStringArg;

	HRESULT HResult;
	String m_message;

	typedef std::map<String, String> CDataMap;
	CDataMap Data;

	explicit Exc(HRESULT hr = 0, RCString message = "");
	String ToString() const;

	virtual String get_Message() const;
	DEFPROP_VIRTUAL_GET_CONST(String, Message);
private:

#ifdef _WIN64
	LONG dummy;
#endif

public:
#if !UCFG_WCE
	CStackTrace StackTrace;
#endif
};

class ExcLastStringArgKeeper {
public:
	String m_prev;

	ExcLastStringArgKeeper(RCString s)
#if !UCFG_WDM
		:	m_prev(*Exc::t_LastStringArg)
#endif
	{
#if !UCFG_WDM
		*Exc::t_LastStringArg = s;
#endif
	}

	~ExcLastStringArgKeeper() {
#if !UCFG_WDM
		*Exc::t_LastStringArg = m_prev;
#endif
	}

	operator const String&() const {
#if !UCFG_WDM
		return *Exc::t_LastStringArg;
#endif
	}

	operator const char *() const {
#if !UCFG_WDM
		return *Exc::t_LastStringArg;
#endif
	}

	operator const String::Char *() const {
#if !UCFG_WDM
		return *Exc::t_LastStringArg;
#endif
	}
};

class OutOfMemoryExc : public Exc {
public:
	OutOfMemoryExc()
		:	Exc(E_OUTOFMEMORY)
	{
	}
};

class SystemException : public Exc {
public:
	SystemException(HRESULT hr = 0, RCString message = "")
		:	Exc(hr, message)
	{
	}
};

class ArithmeticExc : public Exc {
public:
	ArithmeticExc(HRESULT hr = E_EXT_Arithmetic)
		:	Exc(hr)
	{}
};

class OverflowExc : public ArithmeticExc {
public:
	OverflowExc(HRESULT hr = E_EXT_Overflow)
		:	ArithmeticExc(hr)
	{}
};

class CryptoExc : public Exc {
	typedef Exc base;
public:
	explicit CryptoExc(HRESULT hr, RCString message)
		:	base(hr, message)
	{}
};

class ArgumentExc : public Exc {
public:
	ArgumentExc()
		:	Exc(E_INVALIDARG)
	{}
};

class EndOfStreamException : public Exc {
public:
	EndOfStreamException()
		:	Exc(E_EXT_EndOfStream)
	{}
};

class FileFormatException : public Exc {
public:
	FileFormatException()
		:	Exc(E_EXT_FileFormat)
	{}
};

class NotImplementedExc : public Exc {
public:
	NotImplementedExc()
		:	Exc(E_NOTIMPL)
	{}
};

class UnspecifiedException : public Exc {
public:
	UnspecifiedException()
		:	Exc(E_FAIL)
	{}
};

class ThreadAbortException : public Exc {
public:
	ThreadAbortException()
		:	Exc(E_EXT_ThreadStopped)
	{}
};

class StackOverflowExc : public Exc {
	typedef Exc base;
public:
	StackOverflowExc()
		:	Exc(HRESULT_FROM_WIN32(ERROR_STACK_OVERFLOW))
	{}

	CInt<void *> StackAddress;
};

class AccessDeniedExc : public Exc {
public:
	AccessDeniedExc()
		:	Exc(E_ACCESSDENIED)
	{
	}
};

class ProcNotFoundExc : public Exc {
	typedef Exc base;
public:
	String ProcName;

	ProcNotFoundExc()
		:	Exc(HRESULT_OF_WIN32(ERROR_PROC_NOT_FOUND))
	{
	}

	String get_Message() const override {
		String r = base::get_Message();
		if (!ProcName.IsEmpty())
			r += " "+ProcName;
		return r;
	}
};

INT_PTR WINAPI AfxApiNotFound();
#define DEF_DELAYLOAD_THROW static FARPROC WINAPI DliFailedHook(unsigned dliNotify, PDelayLoadInfo  pdli) { return &AfxApiNotFound; } static struct InitDliFailureHook {  InitDliFailureHook() { __pfnDliFailureHook2 = &DliFailedHook; } } s_initDliFailedHook;

extern "C" AFX_API void _cdecl AfxTestEHsStub(void *prevFrame);

//!!! DBG_LOCAL_IGNORE_NAME(1, ignOne);	

#define DEF_TEST_EHS							\
	static DECLSPEC_NOINLINE int AfxTestEHs(void *p) {			\
		try {								\
			AfxTestEHsStub(&p);					\
		} catch (...) {							\
		}										\
		return 0;								\
	}											\
	static int s_testEHs  = AfxTestEHs(&AfxTestEHs);									\


class AssertFailedExc : public Exc {
public:
	String FileName;
	int LineNumber;

	AssertFailedExc(RCString exp, RCString fileName, int line)
		:	Exc(HRESULT_FROM_WIN32(ERROR_ASSERTION_FAILURE), "Assertion Failed: "+exp)
		,	FileName(fileName)
		,	LineNumber(line)
	{
	}
};




template <typename H>
struct HandleTraitsBase {
	typedef H handle_type;

	static bool ResourceValid(const handle_type& h) {
		return h != 0;
	}

	static handle_type ResourseNull() {
		return 0;
	}
};

template <typename H>
struct HandleTraits : HandleTraitsBase<H> {
};

/*!!!
template <typename H>
inline void ResourceRelease(const H& h) {
	Throw(E_NOTIMPL);
}*/

template <typename H>
class ResourceWrapper {
public:
	typedef HandleTraits<H> traits_type;
	typedef typename HandleTraits<H>::handle_type handle_type;

	handle_type m_h;		//!!! should be private
    
	ResourceWrapper()
		:	m_h(traits_type::ResourseNull())
	{}

	ResourceWrapper(const ResourceWrapper& res)
		:	m_h(traits_type::ResourseNull())
	{
		operator=(res);
	}

	~ResourceWrapper() {
		if (Valid()) {
			if (!std::uncaught_exception())
				traits_type::ResourceRelease(m_h);
			else {
				try {
					traits_type::ResourceRelease(m_h);
				} catch (RCExc&) {
				//	TRC(0, e);
				}
			}
		}
	}

	bool Valid() const {
		return traits_type::ResourceValid(m_h);
	}

	void Close() {
		if (Valid())
			traits_type::ResourceRelease(exchange(m_h, traits_type::ResourseNull()));
	}

	handle_type& OutRef() {
		if (Valid())
			Throw(E_EXT_AlreadyOpened);
		return m_h;
	}

	handle_type Handle() const {
		if (!Valid())
			Throw(E_HANDLE);
		return m_h;
	}

	handle_type operator()() const { return Handle(); }

	handle_type& operator()() {
		if (!Valid())
			Throw(E_HANDLE);
		return m_h;		
	}

	ResourceWrapper& operator=(const ResourceWrapper& res) {
		handle_type tmp = exchange(m_h, traits_type::ResourseNull());
		if (traits_type::ResourceValid(tmp))
			traits_type::ResourceRelease(tmp);
		if (traits_type::ResourceValid(res.m_h))
			m_h = traits_type::Retain(res.m_h);
		return *this;
	}

};


class CEscape {
public:
	virtual ~CEscape() {}

	virtual void EscapeChar(std::ostream& os, char ch) {
		os.put(ch);
	}

	virtual int UnescapeChar(std::istream& is) {
		return is.get();
	}

	static AFX_API String AFXAPI Escape(CEscape& esc, RCString s);
	static AFX_API String AFXAPI Unescape(CEscape& esc, RCString s);
};

struct CExceptionFabric {
	CExceptionFabric(int facility);
	virtual DECLSPEC_NORETURN void ThrowException(HRESULT hr, RCString msg) =0;
};

DECLSPEC_NORETURN void AFXAPI ThrowS(HRESULT hr, RCString msg);



} // Ext::

