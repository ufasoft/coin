/*######     Copyright (c) 1997-2013 Ufasoft  http://ufasoft.com  mailto:support@ufasoft.com,  Sergey Pavlov  mailto:dev@ufasoft.com #######################################
#                                                                                                                                                                          #
# This program is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation;  #
# either version 3, or (at your option) any later version. This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the      #
# implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details. You should have received a copy of the GNU #
# General Public License along with this program; If not, see <http://www.gnu.org/licenses/>                                                                               #
##########################################################################################################################################################################*/

#pragma once

#if UCFG_WIN32
#	ifdef NTDDI_WIN8
#		include <synchapi.h >
#	else
#		include <winbase.h>
#	endif

#	if !UCFG_WCE
#		include <winternl.h>
#	endif
#endif


#if UCFG_USE_PTHREADS
#	include <semaphore.h>
#endif

#if !UCFG_STDSTL || !UCFG_CPP11_HAVE_MUTEX
#	include <el/stl/mutex>

namespace std {
	using ExtSTL::mutex;
	using ExtSTL::recursive_mutex;
	using ExtSTL::lock_guard;
	using ExtSTL::once_flag;
	using ExtSTL::call_once;
}

#endif

/*!!!R
#if !UCFG_STDSTL || !UCFG_CPP11_HAVE_THREAD
#	include <el/stl/thread>

namespace std {
	using ExtSTL::thread;
}

#endif
*/


namespace Ext {

#if UCFG_USE_PTHREADS
int PthreadCheck(int code, int allowableError = 0);
#endif

#ifdef _WIN32
NTSTATUS AFXAPI NtCheck(NTSTATUS status, NTSTATUS allowableError = 0);		// 0 == STATUS_SUCCESS
#endif



class AFX_CLASS CSyncObject : public SafeHandle {
	typedef CSyncObject class_type;
public:
	CBool m_bAlreadyExists;

	CSyncObject() {}
	CSyncObject(RCString pstrName);
	void AttachCreated(HANDLE h);
	virtual void Unlock() =0;

	virtual bool Lock(DWORD dwTimeout = INFINITE);
	void Unlock(Int32 lCount, Int32 *lprevCount = 0);
#ifdef WDM_DRIVER
	bool LockEx(DWORD dwTimeout, KPROCESSOR_MODE mode, bool bAlertable);
#endif
private:
	CSyncObject(const class_type&);
};

class AFX_CLASS CCriticalSection : public CSyncObject {
public:
#if UCFG_USE_POSIX
	typedef pthread_mutex_t *native_handle_type;
#elif UCFG_WIN32
	typedef CRITICAL_SECTION *native_handle_type;
#elif UCFG_WDM
	typedef KSPIN_LOCK *native_handle_type;
#endif

	CCriticalSection();
	//  CCriticalSection(DWORD dwSpinCount);
	~CCriticalSection();

	void lock() {
#if UCFG_USE_PTHREADS
		PthreadCheck(::pthread_mutex_lock(&m_mutex));
#elif defined(WDM_DRIVER)
		KeAcquireSpinLock(&m_spinLock, &m_oldIrql);
#else
		::EnterCriticalSection(&m_sect);				// don't handle EXCEPTION_POSSIBLE_DEADLOCK and "OutOfMemory on Win2K"
#endif
	}

	bool try_lock() {
#if UCFG_USE_PTHREADS
		return !PthreadCheck(::pthread_mutex_trylock(&m_mutex), EBUSY);
#elif UCFG_WDM
		m_oldIrql = DISPATCH_LEVEL;					//!!!
		return KeTryToAcquireSpinLockAtDpcLevel(&m_spinLock);
#else
		return ::TryEnterCriticalSection(&m_sect);				// don't handle EXCEPTION_POSSIBLE_DEADLOCK and "OutOfMemory on Win2K"
#endif
	}

	void unlock() {
#if UCFG_USE_PTHREADS
		PthreadCheck(::pthread_mutex_unlock(&m_mutex));
#elif defined(WDM_DRIVER)
		KeReleaseSpinLock(&m_spinLock, m_oldIrql);
#else
		::LeaveCriticalSection(&m_sect);
#endif
	}

	bool Lock(DWORD dwTimeout = INFINITE);
	bool TryLock();
	void Unlock();
	
	//!!!  DWORD SetSpinCount(DWORD dwSpinCount);

#if UCFG_USE_POSIX
	native_handle_type native_handle() { return &m_mutex; }
#elif UCFG_WIN32
	native_handle_type native_handle() { return &m_sect; }
#elif UCFG_WDM
	native_handle_type native_handle() { return &m_spinLock; }
#endif

protected:

#if UCFG_USE_PTHREADS
	pthread_mutex_t m_mutex;
#elif defined WDM_DRIVER
    KSPIN_LOCK m_spinLock;
	KIRQL m_oldIrql;
#else
	CRITICAL_SECTION m_sect;
#endif
	void Init();

friend class CEvent;
};

#if UCFG_WIN32
class AFX_CLASS CNonRecursiveCriticalSection : public CCriticalSection {
public:
	void lock() {
		::EnterCriticalSection(&m_sect);				// don't handle EXCEPTION_POSSIBLE_DEADLOCK and "OutOfMemory on Win2K"
#if UCFG_DEBUG
		ASSERT(m_sect.RecursionCount == 1);
#endif
	}

	bool try_lock() {
		if (m_sect.RecursionCount > 0)
			return false;
		return ::TryEnterCriticalSection(&m_sect);				// don't handle EXCEPTION_POSSIBLE_DEADLOCK and "OutOfMemory on Win2K"
	}
};
#endif // UCFG_WIN32

template <class T> void Lock(T& sync) {
	sync.Lock();
}

template <class T> void Unlock(T& sync) {
	sync.Unlock();
}

inline void Lock(CCriticalSection& cs) {
	cs.lock();
}

inline void Unlock(CCriticalSection& cs) {
	cs.unlock();
}

#if UCFG_WIN32
inline void Lock(CNonRecursiveCriticalSection& cs) {
	cs.lock();
}

inline void Unlock(CNonRecursiveCriticalSection& cs) {
	cs.unlock();
}
#endif

#if UCFG_STDSTL && UCFG_CPP11_HAVE_MUTEX

inline void Lock(std::mutex& mtx) {
	mtx.lock();
}

inline void Unlock(std::mutex& mtx) {
	mtx.unlock();
}

inline void Lock(std::recursive_mutex& mtx) {
	mtx.lock();
}

inline void Unlock(std::recursive_mutex& mtx) {
	mtx.unlock();
}

#endif // UCFG_STDSTL && UCFG_CPP11_HAVE_MUTEX


class CScopedLockBase {
public:
	operator bool() const {
		return false;
	}
};

template <class T>
class CScopedLock : public CScopedLockBase {
public:
	mutable T *m_sync;
	
	CScopedLock(const CScopedLock& sl)
		:	m_sync(exchange(sl.m_sync, nullptr))
	{}

	CScopedLock(T& sync)
		:	m_sync(&sync)
	{
		Lock(*m_sync);
	}
	
	~CScopedLock() {
		if (m_sync)
			Ext::Unlock(*m_sync);
	}


/*!!!	void Unlock() const {
		Ext::Unlock(*m_sync);
		m_sync = 0;
	}*/
};


template <class T> CScopedLock<T> ScopedLock(T& sync) { return CScopedLock<T>(sync); }

#define EXT_LOCK(m) if (const Ext::CScopedLockBase& lck = Ext::ScopedLock(m)) Ext::ThrowImp(E_EXT_CodeNotReachable); else
//#define EXT_LOCK(m) if (const Ext::CScopedLockBase& lck = Ext::ScopedLock(m)) ; else

#if defined(_MSC_VER) && _MSC_VER>=1600
#	define EXT_LOCKED(mtx, expr) (std::lock_guard<decltype(mtx)>(mtx), (expr))			//!!!
#else
#	define EXT_LOCKED(mtx, expr) (Ext::ScopedLock(mtx), (expr))			//!!!
#endif

#if UCFG_SIMPLE_MACROS
#	define lock EXT_LOCK
#endif

} // Ext::





