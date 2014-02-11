/*######     Copyright (c) 1997-2013 Ufasoft  http://ufasoft.com  mailto:support@ufasoft.com,  Sergey Pavlov  mailto:dev@ufasoft.com #######################################
#                                                                                                                                                                          #
# This program is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation;  #
# either version 3, or (at your option) any later version. This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the      #
# implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details. You should have received a copy of the GNU #
# General Public License along with this program; If not, see <http://www.gnu.org/licenses/>                                                                               #
##########################################################################################################################################################################*/

#pragma once

#if UCFG_STDSTL
#	define EXT_HEADER(hname) <hname>
#else
#	define EXT_HEADER(hname) <el/stl/hname>
#endif

#if UCFG_CPP11_HAVE_DECIMAL
#	define EXT_HEADER_DECIMAL EXT_HEADER(decimal/decimal)			//!!!?
#else
#	define EXT_HEADER_DECIMAL <el/stl/decimal>
#endif

#define EXT_HEADER_CONDITION_VARIABLE EXT_HEADER(condition_variable)
#define EXT_HEADER_FUTURE EXT_HEADER(future)

#if UCFG_STD_SHARED_MUTEX
#	define EXT_HEADER_SHARED_MUTEX EXT_HEADER(shared_mutex)
#else
#	define EXT_HEADER_SHARED_MUTEX <el/stl/shared_mutex>
#endif

#if UCFG_STD_DYNAMIC_BITSET
#	define EXT_HEADER_DYNAMIC_BITSET EXT_HEADER(dynamic_bitset)
#else
#	define EXT_HEADER_DYNAMIC_BITSET <el/stl/dynamic_bitset>
#endif

#if UCFG_STD_FILESYSTEM
#	define EXT_HEADER_FILESYSTEM EXT_HEADER(filesystem)
#else
#	define EXT_HEADER_FILESYSTEM <el/stl/filesystem>
#endif

#define EXT_HASH_VALUE_NS Ext

#ifdef _M_CEE

using namespace System;

#	define M_INTERFACE interface class
#	define M_REF_CLASS ref class
#	define M_VALUE_STRUCT value struct
#	define M_VIRTUAL 
#	define M_DEF_POINTER(typ) typedef typ ^P##typ;
#	define M_REF(typ) typ%
#	define M_THROW(x) throw gcnew x;
#	define M_NEW gcnew
#	define M_OVERRIDE override
#	define M_ABSTRACT abstract
#	define M_ENUM enum class

typedef Byte byte;


template <class T> void swap(M_REF(T) a, M_REF(T) b) {
	T c = a;
	a = b;
	b = c;
}

#else		// _M_CEE


#	define M_INTERFACE struct
#	define M_REF_CLASS class
#	define M_VALUE_STRUCT struct
#	define M_VIRTUAL  virtual
#	define M_DEF_POINTER(typ) typedef typ *P##typ;
#	define M_REF(typ) typ&
#	define M_THROW(x) throw x;
#	define M_NEW new
#	define M_OVERRIDE
#	define M_ABSTRACT
#	define M_ENUM enum


#if !UCFG_STDSTL && (!defined(_MSC_VER) || _MSC_VER>=1600)
	namespace ExtSTL {
		typedef decltype(__nullptr) nullptr_t;
		using std::nullptr_t;
	}
#endif



//!!!R #ifndef _CRTBLD
#	include "cpp-old.h"
//!!! #endif

#if UCFG_STDSTL
#	include <cstddef>			// to define std::nullptr_t in GCC
#endif

namespace Ext {
AFX_API bool AFXAPI AfxAssertFailedLine(const char* sexp, const char*fileName, int nLine);
}

//!!!R#	if UCFG_EXTENDED
#		if !defined(ASSERT) && !defined(NDEBUG)
#			define ASSERT(f) ((void) (f || Ext::AfxAssertFailedLine(#f, __FILE__, __LINE__)))
#		endif
//!!!R #	endif

#	ifndef ASSERT
#		define ASSERT assert
#	endif


#if (!UCFG_CPP11 /*!!! || defined(_MSC_VER) && _MSC_VER >= 1600 */) && !defined(__GXX_EXPERIMENTAL_CXX0X__) && UCFG_STDSTL
#	define BEGIN_STD_TR1 namespace std { namespace tr1 {
#	define END_STD_TR1 }}
#	ifdef _MSC_VER
#		define TR1_HEADER(name)  <name>
#	else
#		define TR1_HEADER(name)  <tr1/name>
#	endif
#	define EXT_TR1_IS_NS 1
#	define _TR1_NAME(n) std::tr1::##n
#else
#	define TR1_HEADER(name)  <name>
#	define BEGIN_STD_TR1 namespace std {
#	define END_STD_TR1 }
#	define EXT_TR1_IS_NS 0
#	define _TR1_NAME(n) std:: n
#endif

#if !UCFG_CPP11
#	ifndef static_assert
#		define static_assert(e, Msg) STATIC_ASSERT(e)
#	endif
#endif

#if !UCFG_CPP14_NOEXCEPT
#	define noexcept throw()
#endif
#	include "interlocked.h"
#undef noexcept

#if UCFG_STL


#	if UCFG_STDSTL

#		if !UCFG_CPP11_HAVE_REGEX
#			define _GLIBCXX_REGEX
#		endif


//!!!	#ifdef  __cplusplus
//!!!		#include <extstl.h>
//!!!	#endif




#		include <algorithm>
#		include <deque>
//!!!R#		include <tr1/functional>

#		if !UCFG_WDM
#			include <functional>
#			include <limits>

#			if !UCFG_WCE
#				include <unordered_map>
#				include <unordered_set>
#			endif

#		endif

#		if UCFG_WCE
#			include <typeinfo>
#		endif

#		include <list>
#		include <map>
#		include <memory>
#		include <queue>
#		include <set>
#		include <stack>
#		include <utility>
#		include <vector>

#		if UCFG_USE_REGEX && UCFG_CPP11_HAVE_REGEX
#			include <regex>
#		endif

#		include <fstream>
#		include <iomanip>
#		include <iostream>
#		include <sstream>

#		include <stdexcept>


#		if UCFG_CPP11_HAVE_MUTEX
#			include <mutex>
#		endif

#		if UCFG_CPP11_HAVE_THREAD
#			include <thread>
#		endif

//!!!using namespace std;


	namespace ExtSTL {
		using std::pair;
	}

#	else
#		include <el/stl/ext-stl-full.h>

#	endif

#endif //  UCFG_STL

#if !UCFG_CPP14_NOEXCEPT
#	define noexcept throw()
#endif

#include "exthelpers.h"		// uses std::swap() from <utility>


namespace Ext {
	class CCriticalSection;
	class CNonRecursiveCriticalSection;

	template <typename P1, typename R> struct UnaryFunction {
		virtual R operator()(P1 p1) =0;
	};

} // Ext::

#if UCFG_CPP11 && UCFG_WCE
#	include <el/stl/type_traits>
#endif


#if UCFG_CPP11_RVALUE
#	define EXT_MOVABLE_BUT_NOT_COPYABLE(TYPE)	\
   private:										\
   TYPE(const TYPE &);							\
   TYPE& operator=(const TYPE &);				\

#	define EXT_RV_REF(typ) typ &&
#else

namespace Ext {

template <class T>
class rv : public T {
   rv();
   ~rv();
   rv(rv const&);
   void operator=(rv const&);
};

template<class T>
struct is_movable : public std::is_convertible<T, rv<T>&> {
};

} // Ext::

#	define EXT_MOVABLE_BUT_NOT_COPYABLE(TYPE)\
   private:\
   TYPE(TYPE &);\
   TYPE& operator=(TYPE &);\
   public:\
   operator ::Ext::rv<TYPE>&() \
   {  return *static_cast< ::Ext::rv<TYPE>* >(this);  }\
   operator const ::Ext::rv<TYPE>&() const \
   {  return *static_cast<const ::Ext::rv<TYPE>* >(this);  }\
   private:

#	define EXT_RV_REF(typ) ::Ext::rv<typ>&

namespace std {

template <class T>
typename enable_if<Ext::is_movable<T>::value, Ext::rv<T>&>::type move(T& x) {
	return *static_cast<Ext::rv<T>* >(&x);
} // std::


}





#endif // UCFG_CPP11_RVALUE

#if !UCFG_MINISTL
#	include "ext-meta.h"
#endif

#if UCFG_FRAMEWORK && !defined(_CRTBLD)
#	include "ext-safehandle.h"
#	include "ext-sync.h"
#endif

#define AFX_STATIC_DATA extern __declspec(selectany)

#if UCFG_STL && !defined(_CRTBLD)
//#	define NS std
#	if !UCFG_STDSTL || UCFG_WCE //!!!
#		include <el/stl/exttemplates.h>
#		include <el/stl/clocale>
#		include <el/stl/ext-locale.h>
#	endif
#	include "ext-hash.h"
#	if !UCFG_STDSTL
#		include <el/stl/extstl.h>
#	endif
//#	undef NS
#endif

#if UCFG_STDSTL && UCFG_USE_REGEX && !UCFG_CPP11_HAVE_REGEX
#	include <el/stl/regex>

namespace std {
	using ExtSTL::sub_match;
	using ExtSTL::cmatch;
	using ExtSTL::wcmatch;
	using ExtSTL::wcsub_match;
	using ExtSTL::smatch;
	using ExtSTL::wsmatch;
	using ExtSTL::match_results;
	using ExtSTL::basic_regex;
	using ExtSTL::regex;
	using ExtSTL::wregex;
	using ExtSTL::regex_iterator;
	using ExtSTL::cregex_iterator;
	using ExtSTL::regex_search;
	using ExtSTL::regex_match;
	using ExtSTL::regex_error;

	namespace regex_constants {
		using ExtSTL::regex_constants::ECMAScript;
		using ExtSTL::regex_constants::error_escape;
		using ExtSTL::regex_constants::icase;
		using ExtSTL::regex_constants::match_flag_type;
		using ExtSTL::regex_constants::match_default;
		using ExtSTL::regex_constants::match_any;
		using ExtSTL::regex_constants::syntax_option_type;
	}

} // std::

#endif


namespace Ext {

class AFX_CLASS CNoTrackObject {
public:
	void* __stdcall operator new(size_t nSize);
	void __stdcall operator delete(void*);

	CNoTrackObject();
	virtual ~CNoTrackObject();
};

#if UCFG_FRAMEWORK && !defined(_CRTBLD)

struct HResultItem {
	HResultItem *Next, *Prev;
	HRESULT HResult;

	explicit HResultItem(HRESULT hr)
		:	HResult(hr)
	{}
		
	operator HRESULT() const { return HResult; }
};

class EXT_CLASS CLocalIgnore : public HResultItem {
	typedef HResultItem base;
public:
	CLocalIgnore(HRESULT hr);
	~CLocalIgnore();
};

#endif	// UCFG_FRAMEWORK && !defined(_CRTBLD)

//!!!R AFX_API void AFXAPI DbgAddIgnoredHResult(HRESULT hr);

class AFX_CLASS CThreadLocalObject {
public:
	CThreadLocalObject() {
		AllocSlot();
	}

	CNoTrackObject* GetData(CNoTrackObject* (AFXAPI * pfnCreateObject)());
	CNoTrackObject* GetDataNA();

	int m_nSlot;
	~CThreadLocalObject();
private:
	void AllocSlot();
};

template <class TYPE> class AFX_CLASS CThreadLocal : public CThreadLocalObject {
public:
	TYPE* GetData()
	{
		TYPE* pData = (TYPE*)CThreadLocalObject::GetData(&CreateObject);
		//!!!CE		ASSERT(pData != NULL);
		return pData;
	}
	TYPE* GetDataNA()
	{
		TYPE* pData = (TYPE*)CThreadLocalObject::GetDataNA();
		return pData;
	}
	operator TYPE*()
	{ return GetData(); }
	TYPE* operator->()
	{ return GetData(); }

	//!!!	static CNoTrackObject* CreateObject()
	static CNoTrackObject * AFXAPI CreateObject()
	{ return new TYPE; }
};

template <class T>
class tls_ptr {
	struct CWrap : public CNoTrackObject {
		T m_val;
	};

	CThreadLocal<CWrap> m_wrap;
public:
	operator T*() { return &m_wrap.GetData()->m_val; }
	T* operator->() { return operator T*(); }
};


	//!!!extern vector<HRESULT> g_arIgnoreExceptions;

} // Ext::


inline Ext::Int64 abs(const Ext::Int64& v) {
	return v>=0 ? v : -v;
}



//!!!R #include "ext-meta.h"

#define HRESULT_OF_WIN32(x) ((HRESULT) ((x) & 0x0000FFFF | (FACILITY_WIN32 << 16) | 0x80000000))			// variant of HRESULT_FROM_WIN32 without condition


#ifndef _CRTBLD




#if UCFG_FRAMEWORK && UCFG_WCE && UCFG_OLE
#	ifdef _DEBUG
#		pragma comment(lib, "comsuppwd")
#	else
#		pragma comment(lib, "comsuppw")
#	endif
#endif


#if UCFG_CPP11 && UCFG_STDSTL && !UCFG_MINISTL
#	include <array>
#	include <type_traits>

#	ifdef _HAS_TR1_NS
		namespace std {
			using tr1::is_pod;
		}
#	endif
#endif

#if UCFG_MSC_VERSION <= 1700
namespace std {
template <> struct is_scalar<std::nullptr_t> { //!!! bug in VC
	typedef true_type type;
	static const bool value = true;
};
} // std::
#endif // UCFG_MSC_VERSION <= 1800

#if !UCFG_STD_EXCHANGE && !UCFG_MINISTL
namespace std {
template <typename T, typename U>
inline T Do_exchange(T& obj, const U EXT_REF new_val, false_type) {
#	if UCFG_CPP11_RVALUE
	T old_val = std::move(obj);
	obj = std::forward<U>(new_val);
#else
	T old_val = obj;
	obj = new_val;
#endif
  	return old_val;
}

template <typename T, typename U>
inline T Do_exchange(T& obj, const U& new_val, false_type) {
#	if UCFG_CPP11_RVALUE
	T old_val = std::move(obj);
#else
	T old_val = obj;
#endif
	obj = new_val;
  	return old_val;
}

template <typename T, typename U>
inline T Do_exchange(T& obj, const U new_val, true_type) {
#	if UCFG_CPP11_RVALUE
	T old_val = std::move(obj);
#else
	T old_val = obj;
#endif
	obj = new_val;
  	return old_val;
}

template <typename T, typename U>
inline T exchange(T& obj, const U EXT_REF new_val) {
	return Do_exchange(obj, new_val, typename is_scalar<U>::type());
}

#	if UCFG_CPP11_RVALUE

	template <typename T, typename U>
	inline T exchange(T& obj, const U& new_val) {
		return Do_exchange(obj, new_val, typename is_scalar<U>::type());
	}
#	endif

} // std::
#endif // !UCFG_STD_EXCHANGE


namespace Ext {

using std::exchange;

#if UCFG_FULL && !UCFG_MINISTL
template <class T, size_t MAXSIZE>
class vararray : public std::array<T, MAXSIZE> {
	typedef std::array<T, MAXSIZE> base;
public:
	using base::begin;
	using base::data;

	vararray(size_t size = 0)
		:	m_size(size)
	{}    

	vararray(const T *b, const T *e)
		:	m_size(e-b)
	{
		std::copy_n(b, m_size, begin());
	}

	vararray(const T *b, size_t size)
		:	m_size(size)
	{
		std::copy_n(b, m_size, begin());
	}

	size_t max_size() const { return MAXSIZE; }

	size_t size() const { return m_size; }
	void resize(size_t n) { m_size = n; }

	void push_back(const T& v) {
		(*this)[m_size++] = v;
	}
	
	const T *constData() const { return data(); }

	typename base::const_iterator end() const { return begin() + size(); }
	typename base::iterator end() { return begin() + size(); }
private:
	size_t m_size;
};
#endif // UCFG_FULL

struct ConstBuf {
	typedef const unsigned char *const_iterator;

	const unsigned char *P;
	size_t Size;

	ConstBuf(const Buf& mb)
		:	P(mb.P)
		,	Size(mb.Size)
	{
	}

	ConstBuf()
		:	P(0)
		,	Size(0)
	{}
	
	ConstBuf(const void *p, size_t len)
		:	P((const unsigned char*)p)
		,	Size(len)
	{}

#if UCFG_STL && !defined(_CRTBLD)
	ConstBuf(const std::vector<byte>& v)
		:	P(&v[0])
		,	Size(v.size())
	{}

#	if UCFG_FULL
	template <size_t N>
	ConstBuf(const std::array<byte, N>& ar)
		:	P(ar.data())
		,	Size(ar.size())
	{}

	template <size_t N>
	ConstBuf(const vararray<byte, N>& ar)
		:	P(ar.data())
		,	Size(ar.size())
	{}
#	endif
#endif

	const unsigned char *begin() const { return P; }
	const unsigned char *end() const { return P+Size; }

	const unsigned char *Find(const ConstBuf& mb) const;
};

EXTAPI bool AFXAPI operator==(const ConstBuf& x, const ConstBuf& y);
inline bool AFXAPI operator!=(const ConstBuf& x, const ConstBuf& y) { return !(x == y); }

EXTAPI void AFXAPI CCheckErrcode(int en);
EXTAPI int AFXAPI CCheck(int i, int allowableError = INT_MAX);
EXTAPI int AFXAPI NegCCheck(int rc);
EXTAPI void AFXAPI CFileCheck(int i);


class CAlloc {
public:
#if UCFG_INDIRECT_MALLOC
	EXT_DATA static void * (AFXAPI *s_pfnMalloc)(size_t size);
	EXT_DATA static void (AFXAPI *s_pfnFree)(void *p) noexcept ;
	EXT_DATA static size_t (AFXAPI *s_pfnMSize)(void *p);
	EXT_DATA static void * (AFXAPI *s_pfnRealloc)(void *p, size_t size);
	EXT_DATA static void * (AFXAPI *s_pfnAlignedMalloc)(size_t size, size_t align);
	EXT_DATA static void (AFXAPI *s_pfnAlignedFree)(void *p);
#endif
	static void * AFXAPI Malloc(size_t size);
	static void AFXAPI Free(void *p) noexcept ;
	static size_t AFXAPI MSize(void *p);
	static void * AFXAPI Realloc(void *p, size_t size);
	static void * AFXAPI AlignedMalloc(size_t size, size_t align);
	static void AFXAPI AlignedFree(void *p);

	static void AFXAPI DbgIgnoreObject(void *p)
#if UCFG_ALLOCATOR==UCFG_ALLOCATOR_TC_MALLOC && UCFG_HEAP_CHECK
		;
#else
	{}
#endif
};

#if UCFG_WCE
	bool SetCeAlloc();
#endif

	inline void * __fastcall Malloc(size_t size) {
#if UCFG_INDIRECT_MALLOC
		return CAlloc::s_pfnMalloc(size);
#else
		return CAlloc::Malloc(size);
#endif
	}

	inline void __fastcall Free(void *p) noexcept {
#if UCFG_INDIRECT_MALLOC
		CAlloc::s_pfnFree(p);
#else
		CAlloc::Free(p);
#endif
	}


#if UCFG_HAS_REALLOC
	inline void * __fastcall Realloc(void *p, size_t size) {
#	if UCFG_INDIRECT_MALLOC
		return CAlloc::s_pfnRealloc(p, size);
#	else
		return CAlloc::Realloc(p, size);
#	endif
	}
#endif

	inline void * __fastcall AlignedMalloc(size_t size, size_t align) {
#if UCFG_INDIRECT_MALLOC
		return CAlloc::s_pfnAlignedMalloc(size, align);
#else
		return CAlloc::AlignedMalloc(size, align);
#endif
	}

	inline void __fastcall AlignedFree(void *p) {
#if UCFG_INDIRECT_MALLOC
		CAlloc::s_pfnAlignedFree(p);
#else
		CAlloc::AlignedFree(p);
#endif
	}

/*!!!
#if UCFG_USE_POSIX
	inline void * __cdecl _aligned_malloc(size_t size, size_t align) {
		void *r;
		CCheck(posix_memalign(&r, align, size));
		return r;
	}

	inline void __cdecl _aligned_free(void *p) {
		return free(p);
	}
#endif
*/

class AlignedMem {
public:
	AlignedMem(size_t size, size_t align)
		:	m_p(AlignedMalloc(size, align))
	{}
	
	~AlignedMem() {
		AlignedFree(m_p);
	}

	void* get() { return m_p; }
private:
	void *m_p;
};


} // Ext::

namespace EXT_HASH_VALUE_NS {
inline size_t hash_value(const Ext::ConstBuf& mb) {
    return Ext::hash_value(mb.P, mb.Size);
}
}


//!!! #if UCFG_WDM

#if UCFG_DEFINE_NEW

inline void * __cdecl operator new(size_t sz) {
	return Ext::Malloc(sz);
}

inline void __cdecl operator delete(void *p) {
	Ext::Free(p);
}

inline void __cdecl operator delete[](void* p) {
	Ext::Free(p);
}

inline void * __cdecl operator new[](size_t sz) {
	return Ext::Malloc(sz);
}

#else

void * __cdecl operator new(size_t sz);
void __cdecl operator delete(void *p);
void __cdecl operator delete[](void* p);
void * __cdecl operator new[](size_t sz);

#endif // UCFG_DEFINE_NEW



//!!!#endif


#ifdef WDM_DRIVER
#	define EXT_DEF_HASH(T)
#else
#	define EXT_DEF_HASH(T) BEGIN_STD_TR1																\
		template<> class hash<T>{ public: size_t operator()(const T& v) const { return EXT_HASH_VALUE_NS::hash_value(v); } };						\
		template<> class hash<const T&>{ public: size_t operator()(const T& v) const { return EXT_HASH_VALUE_NS::hash_value(v); } };		\
		END_STD_TR1
#endif

#define EXT_DEF_HASH_NS(NS, T) } BEGIN_STD_TR1																\
	template<> class hash<NS::T>{ public: size_t operator()(const NS::T& v) const { return v.GetHashCode(); } };						\
	END_STD_TR1 namespace NS {


#include "ext-str.h"


#if UCFG_FRAMEWORK

#	include "ext-blob.h"
#	include "ext-string.h"

#	if UCFG_USE_BOOST
#		include <boost/functional/hash.hpp>
#	endif

#		ifdef _MSC_VER
#			define tr1 C_tr1
#				include <functional>
#			undef tr1
#		else
#				include <functional>
#		endif


/*!!!R
#	if !UCFG_WDM
#		include <stdexcept>
#	endif
*/


#	if UCFG_USE_TR1 && defined(_HAS_TR1) && EXT_TR1_IS_NS && !defined(WDM_DRIVER)
	namespace std {
#if		UCFG_STDSTL
			using std::tr1::unordered_map;
			using std::tr1::unordered_set;
#endif
			using std::tr1::array;
			using std::tr1::hash;
	}
#	endif




#endif


#if UCFG_FRAMEWORK && !defined(_CRTBLD)
#	include "afterstl.h"
#	include "ext-lru.h"

namespace Ext {
class CIgnoreList : public CNoTrackObject {
public:
	typedef IntrusiveList<HResultItem> CIgnoredExceptions;
	CIgnoredExceptions IgnoredExceptions;
};
extern CThreadLocal<CIgnoreList> t_ignoreList;
}	// Ext::

#endif	// UCFG_FRAMEWORK && !defined(_CRTBLD)

			

#	if UCFG_FRAMEWORK
		extern std::ostream g_osDebug;
#	endif



namespace ww {
    template<bool> class compile_time_check {
    public:
        compile_time_check(...) {}
    };

    template<> class compile_time_check<false> {
    };
}

#ifndef NDEBUG
#   define StaticAssert(test, errormsg)                         \
    do {                                                        \
        struct ERROR_##errormsg {};                             \
        typedef ww::compile_time_check< (test) != 0 > tmplimpl; \
        tmplimpl aTemp = tmplimpl(ERROR_##errormsg());          \
        sizeof(aTemp);                                          \
    } while (0)
#else
#   define StaticAssert(test, errormsg)                         \
    do {} while (0)
#endif



namespace Ext {


class CDataExchange;
class String;
class CTime; //!!! need by MFC headers
class CWnd;


#if defined(_MSC_VER) && defined (WIN32)


#	if UCFG_EXTENDED
#		include "el/extres.h"
#	endif

#	if	UCFG_GUI
//#		include "win32/ext-afxdd_.h"
#	endif


#	if UCFG_WCE
#		include <atldefce.h>
#		define _WIN32_WINNT 0
#	endif


#	include <afxmsg_.h>

#	if UCFG_WCE
#		undef _WIN32_WINNT
#	endif


  // The WinCE OS headers #defines the following, 
  // but it interferes with MFC member functions.
  #undef TrackPopupMenu
  #undef DrawIcon
//!!!  #undef SendDlgItemMessage

#if !UCFG_WCE
//!!!  #undef SetDlgItemText
//!!!  #undef GetDlgItemText
#endif
  //#undef LoadCursor



  #define SM_REMOTESESSION        0x1000

#endif  
} // Ext::


#if defined(_MSC_VER) && !UCFG_EXT_C && !UCFG_WCE
#	include <xhash>
#endif



#if UCFG_FRAMEWORK

#if defined(_HAS_TR1) || _MSC_VER>=1500

#	ifdef _HAS_TR1
	BEGIN_STD_TR1
#	else
	namespace std {
#	endif
//namespace Ext {

//!!!R #if UCFG_STDSTL

#	ifdef _ARRAY_

template <size_t N>
size_t hash_value(const array<unsigned char, N>& vec) {
	return Ext::hash_value(&vec[0], vec.size());
}

template<size_t N> class hash<array<unsigned char, N> > { public: size_t operator()(array<unsigned char, N> v) const { return hash_value(v); } };
template<size_t N> class hash<const array<unsigned char, N>& > { public: size_t operator()(const array<unsigned char, N>& v) const { return hash_value(v); } };

#	endif // _ARRAY_

//!!!R #endif

template<class U, class V> class hash<pair<U, V> > {
public:
	size_t operator()(const pair<U, V>& v) const {
		return hash<U>()(v.first) ^ hash<V>()(v.second);
	}
};

template<class T> class hash<Ext::ptr<T> >{ public: size_t operator()(const Ext::ptr<T>& v) const { return hash_value(v); } };

//}

#	ifdef _HAS_TR1
	END_STD_TR1
#	else
	}	
#	endif
	
#endif // defined(_HAS_TR1) || _MSC_VER>=1500


#endif

namespace std {

#if 0
#	if defined(_MSC_VER) && _MSC_VER >= 1500
	namespace tr1 {
		template <class T> class hash : public unary_function<T, size_t> {
		public:
			size_t operator()(const T& _Keyval) const { return hash_value(_Keyval); }
		};

	}
	using stdext::hash_value;
#	elif !defined(_MSC_VER)
	namespace tr1 {
		using Ext::hash_value;


		/*!!!
		template<class T> struct hash<const T&> {
			size_t operator()(const T& v) const {
				return hash_value(v);
			}
		};*/
	}
#	endif
#endif



#	ifndef _HASH_SEED
#		define _HASH_SEED	((size_t)0xdeadbeef)
#	endif

}

#if !UCFG_STDSTL || UCFG_WCE//!!!
#	if UCFG_FRAMEWORK
#		include "ext-hashimp.h"
#		if !UCFG_STDSTL || UCFG_WCE //!!! && !UCFG_WDM
#			include <el/stl/unordered_map>
#		endif
#	endif // UCFG_FRAMEWORK

#endif // !UCFG_STDSTL




#if !defined(_CRTBLD)



#include "ext_messages.h"

namespace Ext {

#if !UCFG_MINISTL
class EXT_CLASS CTraceWriter {
public:
	CTraceWriter(int level, const char* funname = 0) noexcept;
	CTraceWriter(std::ostream *pos) noexcept;
	~CTraceWriter() noexcept;
	std::ostream& Stream() noexcept;
	void VPrintf(const char* fmt, va_list args);
	void Printf(const char* fmt, ...);
	static CTraceWriter& AFXAPI CreatePreObject(char *obj, int level, const char* funname);
	static void AFXAPI StaticPrintf(int level, const char* funname, const char* fmt, ...);
private:
	std::ostringstream m_os;
	std::ostream *m_pos;
	bool m_bPrintDate;

	void Init(const char* funname);
};
#endif // !UCFG_MINISTL

#if UCFG_WIN32
	AFX_API int AFXAPI Win32Check(LRESULT i);
	AFX_API bool AFXAPI Win32Check(BOOL b, DWORD allowableError);
#endif

template <size_t n>
struct int_presentation {
};

template <> struct int_presentation<4> {
	typedef Int32 type;
};

template <> struct int_presentation<8> {
	typedef Int64 type;
};

/*!!!
template<> struct type_presentation<int> {
#if INT_MAX == 2147483647
	typedef Int32 type;
#endif
};

template<> struct type_presentation<long> {
#if LONG_MAX == 2147483647
	typedef Int32 type;
#endif
};

template<> struct type_presentation<unsigned long> {
#if ULONG_MAX == 0xffffffffUL
	typedef UInt32 type;
#endif
};
*/

} // Ext::


#if UCFG_FRAMEWORK
#	include "ext-stream.h"

#	if defined(WIN32)		// && UCFG_EXTENDED //!!!
#		include "win32/ext-cmd.h"
#	endif

#	include "ext-base.h"
#	include "ext-os.h"
#	include "ext-core.h"
#	include "binary-reader-writer.h"
#	include "datetime.h"


#	ifdef _WIN32
#		include "win32/ext-registry.h"
#	endif


#	if !UCFG_WDM

#		include "ext-fw.h"
//#		include "ext-net.h"

#		if UCFG_EXTENDED || UCFG_USE_LIBCURL
//#			include "ext-http.h"
#		endif

#		if UCFG_WIN32
//#			include "win32/ext-win.h"	
#			if !UCFG_WCE
//#				include "win32/ext-full-win.h"
#			endif

#			if UCFG_WND
//#				include "win32/extwin32.h"
//#				include "win32/ext-wnd.h"

#				if UCFG_EXTENDED && UCFG_WIN_HEADERS
//#					include "el/gui/ext-image.h"
//#					include "win32/extmfc.h"
#				endif

#			endif

#			if UCFG_EXTENDED
#				if UCFG_COM_IMPLOBJ
//#					include "win32/extctl.h"
#				endif
//!!!R				include "win32/simplemapi.h"
//!!!R#				if !UCFG_WCE
//!!!R#					include "win32/toolbar.h"
//!!!R#				endif
//!!!R#				include "lng.h"
#			endif
#		endif

#		include "ext-app.h"

/*!!!?
#		if UCFG_XML
#			include "xml.h"
#		endif
*/

#		if UCFG_WIN32 && UCFG_EXTENDED && !UCFG_WCE
//!!!R#			include "lng.h"
//!!!R#			include "docview.h"
#		endif


#	include "ext-protocols.h"

#	endif

#endif // UCFG_FRAMEWORK


namespace Ext {

#if UCFG_FRAMEWORK

class StreamToBlob : MemoryStream, public BinaryWriter {
public:
	using MemoryStream::get_Blob;
	using MemoryStream::operator ConstBuf;

	StreamToBlob()
		:	BinaryWriter(static_cast<MemoryStream&>(*this))
	{}

	StreamToBlob& Ref() { return *this; }
};

class CTraceCategory {
public:
	String m_name;
	bool Enabled;

	explicit CTraceCategory(RCString name)
		:	m_name(name)
		,	Enabled(true)
	{}
};

struct TraceThreadContext;

class DbgFun {
public:
	DbgFun(const char *funName)
		:	m_funName(funName)
	{
		OutFormat("%d%% %ds>%%s\n", true);
	}

	~DbgFun() {
		OutFormat("%d%% %ds<%%s\n", false);
	}
private:
	TraceThreadContext *m_ctx;
	const char *m_funName;

	void OutFormat(const char *fs, bool bEnter);
};
#endif

#ifdef _DEBUG
#	define DBG_FUN  DbgFun _dbgFun(__FUNCTION__);
#else
#	define DBG_FUN
#endif


inline int BitScanReverse(UInt32 v) {
	unsigned long idx;
	bool b = ::_BitScanReverse(&idx, v);
	return int(idx | -!b);
}

#ifdef _M_X64
inline int BitScanReverse64(UInt64 v) {
	unsigned long idx;
	bool b = ::_BitScanReverse64(&idx, v);
	return int(idx | -!b);
}
#endif


} // Ext::

#define EXT_STR(expr) (static_cast<std::ostringstream&>(static_cast<std::ostringstream EXT_REF>(std::ostringstream()) << expr)).str()
#define EXT_BIN(expr) ConstBuf(static_cast<Ext::StreamToBlob&>(StreamToBlob().Ref() << expr))


#	if UCFG_ATL_EMULATION && defined(WIN32) && UCFG_FRAMEWORK && !defined(WDM_DRIVER)
//#		include "win32/ext-atl.h"
#	endif

#endif  // _CRTBLD



#endif // _CRTBLD

#endif // _M_CEE

inline void * __cdecl operator new(size_t size, int id, const char *file, int line) {
	return operator new(size);
}

inline void * __cdecl operator new[](size_t size, int id, const char *file, int line) {
	return operator new[](size);
}


#if UCFG_DETECT_MISMATCH
#	if UCFG_COMPLEX_WINAPP
#		pragma detect_mismatch("UCFG_COMPLEX_WINAPP", "1")
#	else
#		pragma detect_mismatch("UCFG_COMPLEX_WINAPP", "0")
#	endif
#	if UCFG_EXTENDED
#		pragma detect_mismatch("UCFG_EXTENDED", "1")
#	else
#		pragma detect_mismatch("UCFG_EXTENDED", "0")
#	endif
#endif // _MSC_VER



#if UCFG_STL && !UCFG_STDSTL && !defined(_CRTBLD) && UCFG_FRAMEWORK
#	include EXT_HEADER_FUTURE			// uses AutoResetEvent
#endif // UCFG_STL && !UCFG_STDSTL

