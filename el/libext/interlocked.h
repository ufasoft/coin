/*######     Copyright (c) 1997-2013 Ufasoft  http://ufasoft.com  mailto:support@ufasoft.com,  Sergey Pavlov  mailto:dev@ufasoft.com #######################################
#                                                                                                                                                                          #
# This program is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation;  #
# either version 3, or (at your option) any later version. This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the      #
# implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details. You should have received a copy of the GNU #
# General Public License along with this program; If not, see <http://www.gnu.org/licenses/>                                                                               #
##########################################################################################################################################################################*/

#pragma once

/*!!!
typedef Ext::byte u8;

inline void barrier() {}

#define __KERNEL__
#define CONFIG_X86_32
#define CONFIG_M686
#define HZ 100

#define new _new
#define false _false
#define true _true
#define _I386_PAGE_H
#define _I386_BITOPS_H
#define _LINUX_BITOPS_H
#define __ASM_I386_PROCESSOR_H
#include <asm/alternative.h>
#include <asm/atomic.h>
#undef new
#undef false
#undef true
#undef __KERNEL__
*/



#ifdef _MSC_VER

#if defined(WIN32) && !UCFG_WCE
//!!!	#include <intrin.h>
/*!!!
#ifndef InterlockedIncrement
extern "C"
{
long  __cdecl _InterlockedIncrement(long volatile *Addend);
long  __cdecl _InterlockedDecrement(long volatile *Addend);
long  __cdecl _InterlockedCompareExchange(long * volatile Dest, long Exchange, long Comp);
long  __cdecl _InterlockedExchange(long * volatile Target, long Value);
long  __cdecl _InterlockedExchangeAdd(long * volatile Addend, long Value);
}

//#include <winbase.h>



#pragma intrinsic (_InterlockedCompareExchange)
#pragma intrinsic (_InterlockedExchange)
#pragma intrinsic (_InterlockedExchangeAdd)
#pragma intrinsic (_InterlockedIncrement)
#pragma intrinsic (_InterlockedDecrement)
#endif*/
#endif



/*!!!?
#	if _MSC_VER>=1600
#		include <intrin.h>
#	endif
*/


#if UCFG_WCE
	typedef LONG* PVLONG;
	#define INTERLOCKED_FUN(fun) fun
#elif defined(WIN32)
//	typedef volatile LONG* PVLONG;
	#define INTERLOCKED_FUN(fun) fun
#elif defined(WDM_DRIVER)
	typedef volatile LONG* PVLONG;
	#define INTERLOCKED_FUN(fun) fun
#endif

#if UCFG_WCE
#	pragma warning (disable: 4732)	// intrinsic '_Interlocked...' is not supported in this architectur
#endif


#if defined(_MSC_VER) && !defined(__INTRIN_H_)
	extern "C" long __cdecl _InterlockedCompareExchange (long volatile *, long, long);
#	ifdef _M_ARM
#	define _InterlockedCompareExchange InterlockedCompareExchange
#else
#	pragma intrinsic(_InterlockedCompareExchange)
#endif

	extern "C" long __cdecl _InterlockedIncrement (long volatile *Destination);
#	ifdef _M_ARM
#		define _InterlockedIncrement InterlockedIncrement
#	else
#		pragma intrinsic(_InterlockedIncrement)
#	endif

	extern "C" long __cdecl _InterlockedDecrement (long volatile *Destination);
#	ifdef _M_ARM
#		define _InterlockedDecrement InterlockedDecrement
#	else
#		pragma intrinsic(_InterlockedDecrement)
#	endif


	extern "C" long _InterlockedAnd (long volatile *Destination, long Value );
#	pragma intrinsic(_InterlockedAnd)

	extern "C" long _InterlockedOr (long volatile *Destination, long Value );
#	pragma intrinsic(_InterlockedOr)

	extern "C" long _InterlockedXor (long volatile *Destination, long Value );
#	pragma intrinsic(_InterlockedXor)

	extern "C" long __cdecl _InterlockedExchange (long volatile *Destination, long Value );
#	pragma intrinsic(_InterlockedExchange)

	extern "C" long __cdecl _InterlockedExchangeAdd(long volatile *, long);
#	pragma intrinsic(_InterlockedExchangeAdd)


//	extern "C" long _cdecl InterlockedAdd(long volatile *Destination, long Valu );
//!!! IPF only #	pragma intrinsic(_InterlockedAdd)

#	if _MSC_VER >= 1700 || defined(_M_X64)
	extern "C" void * __cdecl _InterlockedCompareExchangePointer(void * volatile * _Destination, void * _Exchange, void * _Comparand);
#		pragma intrinsic(_InterlockedCompareExchangePointer)
#	endif

#endif

#endif // _MSC_VER



namespace Ext {

class NonInterlocked {
public:
	template <class T> static T Increment(T& v) { return ++v; }
	template <class T> static T Decrement(T& v) { return --v; }
};




#ifdef __GNUC__


class Interlocked {
public:
	static Int32 Add(volatile Int32& v, Int32 add) {
		return __sync_add_and_fetch(&v, add);

/*!!!
		Int32 r = add;
		asm volatile("lock; xaddl %0, %1" : "=r"(r), "+m"(v) : "0"(r) : "memory");
		return r+add; */
	}

	static Int32 Increment(volatile Int32& v) {
		return Add(v, 1);
	}

	static Int32 Decrement(volatile Int32& v) {
		return Add(v, -1);
	}


	static Int32 Exchange(volatile Int32& v, Int32 n) {
		return __sync_lock_test_and_set(&v, n);
/*		Int32 r = n;
		asm volatile("xchg %0, %1" : "=r"(r), "+m"(v) : "0"(r) : "memory");
		return r;*/
	}

	static Int32 CompareExchange(volatile Int32& d, Int32 e, Int32 c) {
		return __sync_val_compare_and_swap(&d, c, e);
/*!!!		Int32 prev = c;
		asm volatile( "lock; cmpxchg %3,%1"
			: "=a" (prev), "=m" (d)
			: "0" (prev), "r" (e)
			: "memory", "cc");
		return prev; */
	}

	template <typename T>
	static void *CompareExchange(T * volatile & d, T *e, T *c) {
		return __sync_val_compare_and_swap(&d, c, e);
/*!!!		T *prev = c;
		asm volatile( "lock; cmpxchg %3,%1"
			: "=a" (prev), "=m" (d)
			: "0" (prev), "r" (e)
			: "memory", "cc");
		return prev;
*/
	}               
};

#else

class Interlocked {
public:
#	if UCFG_WCE
	template <class T> static T Increment(volatile T& v) { return T(_InterlockedIncrement((LONG*)(&v))); }
	template <class T> static T Decrement(volatile T& v) { return T(_InterlockedDecrement((LONG*)(&v))); }
#	else
	template <class T> static T Increment(volatile T& v) { return T(_InterlockedIncrement((volatile LONG*)(&v))); }
	template <class T> static T Decrement(volatile T& v) { return T(_InterlockedDecrement((volatile LONG*)(&v))); }
#	endif

/*!!!R	template <class T> static T Exchange(T &d, T e)
	{
		return (T)INTERLOCKED_FUN(InterlockedExchange)((PVLONG)(&d),e);
	}*/

	template <class T> static T *Exchange(T * volatile &d, void *e) {
#ifdef WDM_DRIVER
		return (T*)InterlockedExchangePointer((void * volatile *)(&d), e);
#elif defined(_M_IX86)
		return (T*)_InterlockedExchange((volatile long *)(&d), (long)e);
#else
		return (T*)_InterlockedExchangePointer((void * volatile *)(&d), e);
#endif
	}

	static Int32 Exchange(volatile Int32& d, Int32 e) {
		return _InterlockedExchange(reinterpret_cast<long*>(const_cast<Int32*>(&d)), e);
	}
	
	static Int32 CompareExchange(volatile Int32& d, Int32 e, Int32 c) {
		return _InterlockedCompareExchange(reinterpret_cast<long*>(const_cast<Int32*>(&d)), e, c);
	}

	template <typename T>
	static T *CompareExchange(T *volatile & d, T *e, T *c) {
#	if _MSC_VER >= 1700 || defined(_M_X64)
		return static_cast<T*>(_InterlockedCompareExchangePointer((void* volatile *)&d, e, c));
#else
		return (T*)_InterlockedCompareExchange(reinterpret_cast<long volatile*>(&d), (long)e, (long)c);
#endif
	}

	static Int32 ExchangeAdd(volatile Int32& d, Int32 v) {
		return _InterlockedExchangeAdd(reinterpret_cast<long*>(const_cast<Int32*>(&d)), v);
	}

#if LONG_MAX == 2147483647
	static Int32 And(volatile Int32& d, Int32 v) {
		return _InterlockedAnd(reinterpret_cast<volatile long*>(&d), v);
	}

	static Int32 Or(volatile Int32& d, Int32 v) {
		return _InterlockedOr(reinterpret_cast<volatile long*>(&d), v);
	}

	static Int32 Xor(volatile Int32& d, Int32 v) {
		return _InterlockedXor(reinterpret_cast<volatile long*>(&d), v);
	}

	static Int32 Add(volatile Int32& d, Int32 v) {
		return _InterlockedExchangeAdd(reinterpret_cast<volatile long*>(&d), v) + v;
	}
#endif
};

#ifdef WIN32

	inline void atomic_add_int(volatile int *dst, int n) {
		_InterlockedExchangeAdd((long*)dst, n);
	}

	inline void atomic_add_int(volatile unsigned int *dst, int n) {
		_InterlockedExchangeAdd((long*)dst, n);
	}

	inline int atomic_cmpset_int(volatile int *dst, int old, int n) {
		return Interlocked::CompareExchange(*(volatile Int32 *)dst,n,old) == old;
	}

	inline int atomic_cmpset_int(volatile unsigned int *dst, int old, int n) {
		return Interlocked::CompareExchange(*(volatile Int32*)dst,n,old) == old;
	}

#endif


#endif // __GNUC__

} // Ext::





