/*######     Copyright (c) 1997-2013 Ufasoft  http://ufasoft.com  mailto:support@ufasoft.com,  Sergey Pavlov  mailto:dev@ufasoft.com #######################################
#                                                                                                                                                                          #
# This program is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation;  #
# either version 3, or (at your option) any later version. This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the      #
# implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details. You should have received a copy of the GNU #
# General Public License along with this program; If not, see <http://www.gnu.org/licenses/>                                                                               #
##########################################################################################################################################################################*/

#include <el/ext.h>

#include EXT_HEADER(mutex)
#include "shared_mutex"

#if UCFG_WIN32
#	include <windows.h>
#endif

namespace ExtSTL  {
using namespace Ext;

#if UCFG_WIN32_FULL

typedef VOID (WINAPI *PFN_VoidSRWLock)(PSRWLOCK);
typedef BOOLEAN (WINAPI *PFN_BooleanSRWLock)(PSRWLOCK);

static PFN_VoidSRWLock s_pfnInitializeSRWLock, s_pfnReleaseSRWLockExclusive, s_pfnReleaseSRWLockShared, s_pfnAcquireSRWLockExclusive, s_pfnAcquireSRWLockShared;
static PFN_BooleanSRWLock s_pfnTryAcquireSRWLockExclusive, s_pfnTryAcquireSRWLockShared;
    
class SharedMutexImp {
public:
	mutex m_mtxWriters;
	ManualResetEvent m_noReadsEvent;
	volatile Int32 m_nActiveReaders;

	SharedMutexImp()
		:	m_nActiveReaders(0)
		,	m_noReadsEvent(true)	// Initially Set
	{}

	void lock() {
		m_mtxWriters.lock();
		m_noReadsEvent.Lock();
	}

	bool try_lock() {
		if (!m_mtxWriters.try_lock())
			return false;
		m_noReadsEvent.Lock();
		return true;
	}

	void unlock() {	
		m_mtxWriters.unlock();
	}

	void lock_shared() {
		EXT_LOCK (m_mtxWriters) {
			if (Interlocked::Increment(m_nActiveReaders) == 1)
				m_noReadsEvent.Reset();
		}
	}

	bool try_lock_shared() {
		unique_lock<mutex> lk(m_mtxWriters, try_to_lock);
		if (!lk)
			return false;
		if (Interlocked::Increment(m_nActiveReaders) == 1)
			m_noReadsEvent.Reset();
		return true;
	}

	void unlock_shared() {
		if (!Interlocked::Decrement(m_nActiveReaders))
			m_noReadsEvent.Set();
	}
};

class RecursiveSharedMutexImp : public SharedMutexImp {
	typedef SharedMutexImp base;
public:
	DWORD m_tidWriter;
	CInt<Int32> m_nWriterDeep;
	CTls m_tls;

	RecursiveSharedMutexImp()
		:	m_tidWriter(0)
	{}

	DWORD_PTR get_TlsValue() { return (DWORD_PTR)(void*)m_tls.Value; }
	void put_TlsValue(DWORD_PTR v) { m_tls.Value = (void*)v; }
	DEFPROP(DWORD_PTR, TlsValue);

	void lock() {
		DWORD tid = ::GetCurrentThreadId();
		if (tid == m_tidWriter) {
			m_nWriterDeep++;
			return;
		}
		base::lock();
		m_nWriterDeep++;
		m_tidWriter = tid;
	}

	bool try_lock() {
		DWORD tid = ::GetCurrentThreadId();
		if (tid == m_tidWriter) {
			m_nWriterDeep++;
			return true;
		}
		if (!base::try_lock())
			return false;
		m_nWriterDeep++;
		m_tidWriter = tid;
		return true;
	}

	void unlock() {	
		if (--m_nWriterDeep)
			return;
		m_tidWriter = 0;
		base::unlock();
	}

	void lock_shared() {
		DWORD tid = ::GetCurrentThreadId();
		if (tid == m_tidWriter)
			return;
		DWORD_PTR tlsv = TlsValue+1;
		if (1 == tlsv)
			base::lock_shared();
		TlsValue = tlsv;
	}

	bool try_lock_shared() {
		DWORD tid = ::GetCurrentThreadId();
		if (tid == m_tidWriter)
			return true;
		DWORD_PTR tlsv = TlsValue+1;
		if (1 == tlsv && !base::try_lock_shared())
			return false;
		TlsValue = tlsv;
		return true;
	}

	void unlock_shared() {
		DWORD tid = ::GetCurrentThreadId();
		if (tid == m_tidWriter)
			return;
		DWORD_PTR tlsv = TlsValue-1;
		TlsValue = tlsv;
		if (!tlsv)
			base::unlock_shared();
	}
};

void shared_mutex::InitSharedMutex() {
	m_pimpl = new SharedMutexImp;
}

void shared_mutex::InitSRWSharedMutex() {
	s_pfnInitializeSRWLock(&m_srwlock);
}

static void (shared_mutex::*s_pfnInitSharedMutex)();


#endif // UCFG_WIN32_FULL

shared_mutex::shared_mutex()
#if UCFG_WIN32_FULL
	:	m_pimpl(0)
#endif
{
#if UCFG_WIN32
	if (!s_pfnInitSharedMutex) {
		HMODULE hModule = GetModuleHandleW(L"kernel32.dll");
		if (!hModule || !Ext::GetProcAddress(s_pfnInitializeSRWLock, hModule, "InitializeSRWLock"))
			s_pfnInitSharedMutex = &shared_mutex::InitSharedMutex;
		else {
			Ext::GetProcAddress(s_pfnReleaseSRWLockExclusive, hModule, "ReleaseSRWLockExclusive");
			Ext::GetProcAddress(s_pfnReleaseSRWLockShared, hModule, "ReleaseSRWLockShared");		
			Ext::GetProcAddress(s_pfnAcquireSRWLockExclusive, hModule, "AcquireSRWLockExclusive");
			Ext::GetProcAddress(s_pfnAcquireSRWLockShared, hModule, "AcquireSRWLockShared");
			Ext::GetProcAddress(s_pfnTryAcquireSRWLockExclusive, hModule, "TryAcquireSRWLockExclusive");
			Ext::GetProcAddress(s_pfnTryAcquireSRWLockShared, hModule, "TryAcquireSRWLockShared");
			s_pfnInitSharedMutex = &shared_mutex::InitSRWSharedMutex;
		}			
	}
	(this->*s_pfnInitSharedMutex)();
#elif UCFG_USE_PTHREADS
	PthreadCheck(::pthread_rwlock_init(&m_rwlock, NULL));
#endif
}

shared_mutex::~shared_mutex() {
#if UCFG_USE_PTHREADS
	PthreadCheck(::pthread_rwlock_destroy(&m_rwlock));
#else
	delete m_pimpl;
#endif
}

void shared_mutex::lock() {
#if UCFG_USE_PTHREADS
	PthreadCheck(::pthread_rwlock_wrlock(&m_rwlock));
#else
	m_pimpl ? m_pimpl->lock() : s_pfnAcquireSRWLockExclusive(&m_srwlock);
#endif
}

bool shared_mutex::try_lock() {
#if UCFG_USE_PTHREADS
	return !PthreadCheck(::pthread_rwlock_trywrlock(&m_rwlock), EBUSY);
#else
	return m_pimpl ? m_pimpl->try_lock() : s_pfnTryAcquireSRWLockExclusive(&m_srwlock);
#endif
}

void shared_mutex::unlock() {
#if UCFG_USE_PTHREADS
	PthreadCheck(::pthread_rwlock_unlock(&m_rwlock));
#else
	m_pimpl ? m_pimpl->unlock() : s_pfnReleaseSRWLockExclusive(&m_srwlock);
#endif
}

void shared_mutex::lock_shared() {
#if UCFG_USE_PTHREADS
	PthreadCheck(::pthread_rwlock_rdlock(&m_rwlock));
#else
	m_pimpl ? m_pimpl->lock_shared() : s_pfnAcquireSRWLockShared(&m_srwlock);
#endif
}

bool shared_mutex::try_lock_shared() {
#if UCFG_USE_PTHREADS
	return !PthreadCheck(::pthread_rwlock_tryrdlock(&m_rwlock), EBUSY);
#else
	return m_pimpl ? m_pimpl->try_lock_shared() : s_pfnTryAcquireSRWLockShared(&m_srwlock);
#endif
}

void shared_mutex::unlock_shared() {
#if UCFG_USE_PTHREADS
	PthreadCheck(::pthread_rwlock_unlock(&m_rwlock));
#else
	m_pimpl ? m_pimpl->unlock_shared() : s_pfnReleaseSRWLockShared(&m_srwlock);
#endif
}



}  // ExtSTL::


