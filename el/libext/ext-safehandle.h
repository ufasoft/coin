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
#		include <processthreadsapi.h>
#	else
#		include <windows.h>
#	endif
#endif

namespace Ext {

class Exc;
typedef const Exc& RCExc;

class CHandleBaseBase {
public:
	mutable volatile Int32 m_nInUse;

	CHandleBaseBase()
		:	m_bClosed(true)
		,	m_nInUse(0)
	{
	}

	virtual bool Release() const =0;
	bool Close(bool bFromDtor = false);
protected:
	volatile Int32 m_bClosed;					// Int32 because we need Interlocked operations, but sizeof(bool)==1
};

template <class T>
class CHandleBase : public CHandleBaseBase {
public:
	bool Release() const override {
		Interlocked::Decrement(m_nInUse);
		if (!Interlocked::CompareExchange(m_nInUse, 0x80000000L, 0L)) {
			static_cast<const T*>(this)->InternalReleaseHandle();
			return true;
		}
		return false;
	}

template <class U> friend class CHandleKeeper;
};

#ifndef WDM_DRIVER

class EXTAPI CTls {	
public:
	typedef CTls class_type;

	CTls();
	~CTls();

	__forceinline void *get_Value() const {
#if UCFG_USE_PTHREADS
		return ::pthread_getspecific(m_key);
#else
		return ::TlsGetValue(m_key);
#endif
	}

	void put_Value(const void *p);
	DEFPROP(void*, Value);

private:

#if UCFG_USE_PTHREADS
	pthread_key_t m_key;
#else
	unsigned long m_key; //!!! was ULONG
#endif
};

template <class T>
class single_tls_ptr : protected CTls {
	typedef CTls base;
public:
	single_tls_ptr(T *p = 0) {
		base::put_Value(p);
	}

	single_tls_ptr& operator=(const T* p) {
		base::put_Value(p);
		return _self;
	}

	operator T*() const { return (T*)get_Value(); }
	T* operator->() const { return operator T*(); }
};

#if UCFG_USE_DECLSPEC_THREAD
#	define EXT_THREAD_PTR(typ, name) __declspec(thread) typ *name
#else
#	define EXT_THREAD_PTR(typ, name) Ext::single_tls_ptr<typ> name
#endif


#endif // !WDM_DRIVER

template <class H> class CHandleKeeper {
public:
	CHandleKeeper(const H& hp)
		:	m_hp(&hp)
	{
		AddRef();
	}

	CHandleKeeper()
		:	m_hp(0)
	{
	}

	CHandleKeeper(const CHandleKeeper& k) {
		operator=(k);
	}

	CHandleKeeper& operator=(const CHandleKeeper& k) {
		m_hp = k.m_hp;												// std::exchange() may be not defined yet
		k.m_hp = nullptr;
		m_bIncremented = k.m_bIncremented;
		k.m_bIncremented = false;
		return *this;
	}

	struct _Boolean {
		int i;
	};

	operator int _Boolean::*() const {
    	return m_bIncremented ? &_Boolean::i : 0;
	}

//!!!R	operator typename H::handle_type() { return m_hp->DangerousGetHandle(); }

	~CHandleKeeper() {
		Release();
	}

	void AddRef() {
		if (!m_hp->m_bClosed) {
			Interlocked::Increment(m_hp->m_nInUse);
			if (m_hp->m_bClosed)
				m_hp->Release();
			else
				m_bIncremented = true;
		}
	}

	void Release() {
		if (m_bIncremented) {
			m_bIncremented = false;
			m_hp->Release();
		}
	}
protected:
	mutable const H *m_hp;
	mutable CBool m_bIncremented;
};


class EXTAPI SafeHandle : public Object, public CHandleBase<SafeHandle> {
	EXT_MOVABLE_BUT_NOT_COPYABLE(SafeHandle);
public:
	typedef HANDLE handle_type;

#ifndef WDM_DRIVER
	//!!!R static CTls t_pCurrentHandle;
	static EXT_THREAD_PTR(void, t_pCurrentHandle);
#endif

	SafeHandle()
		:	m_handle(-1)
		,	m_invalidHandleValue(-1)
		,	m_bOwn(true)
	{}

	SafeHandle(HANDLE invalidHandle, bool)
		:	m_handle((intptr_t)invalidHandle)
		,	m_invalidHandleValue((intptr_t)invalidHandle)
		,	m_bOwn(true)
#if UCFG_WDM
		,	m_pObject(nullptr)
#endif
	{}

	SafeHandle(HANDLE handle);
	virtual ~SafeHandle();

	SafeHandle(EXT_RV_REF(SafeHandle) rv)
		:	m_handle(rv.m_handle)
		,	m_invalidHandleValue(rv.m_invalidHandleValue)
		,	m_bOwn(rv.m_bOwn)
	{
		m_nInUse = rv.m_nInUse;
		m_bClosed = rv.m_bClosed;

		rv.m_handle = rv.m_invalidHandleValue;
		rv.m_bOwn = true;
		rv.m_nInUse = 0;
		rv.m_bClosed = true;
	}

	SafeHandle& operator=(EXT_RV_REF(SafeHandle) rv) {
		InternalReleaseHandle();

		m_handle = rv.m_handle;
		m_bOwn = rv.m_bOwn;
		m_nInUse = rv.m_nInUse;
		m_bClosed = rv.m_bClosed;

		rv.m_handle = rv.m_invalidHandleValue;
		rv.m_bOwn = true;
		rv.m_nInUse = 0;
		rv.m_bClosed = true;

		return *this;
	}

//!!!	void Release() const;
	//!!!  void CloseHandle();
	HANDLE DangerousGetHandle() const;
	void Attach(HANDLE handle, bool bOwn = true);

	EXPLICIT_OPERATOR_BOOL() const {
		return Valid() ? EXT_CONVERTIBLE_TO_TRUE : 0;
	}

#if UCFG_WDM
protected:
	mutable void *m_pObject;
	mutable CBool m_CreatedReference;
	void AttachKernelObject(void *pObj, bool bKeepRef);
public:
	void Attach(PKEVENT pObj) { AttachKernelObject(pObj, false); }
	void Attach(PKMUTEX pObj) { AttachKernelObject(pObj, false); }

	NTSTATUS InitFromHandle(HANDLE h, ACCESS_MASK DesiredAccess, POBJECT_TYPE ObjectType, KPROCESSOR_MODE  AccessMode);
#endif


	HANDLE Detach();
	void Duplicate(HANDLE h, DWORD dwOptions = 0);
	virtual bool Valid() const;

	class HandleAccess : public CHandleKeeper<SafeHandle> {
		typedef CHandleKeeper<SafeHandle> base;
	public:
		HandleAccess()
		{}

		HandleAccess(const HandleAccess& ha)
			:	base(ha)
		{}

		HandleAccess(const SafeHandle& h)
			:	base(h)
		{}

		~HandleAccess();

		operator SafeHandle::handle_type() {
			if (!m_bIncremented)
				return 0;
			return m_hp->DangerousGetHandle();
		}
	};

	class BlockingHandleAccess : public HandleAccess {
	public:
		BlockingHandleAccess(const SafeHandle& h);
		~BlockingHandleAccess();
	private:
		HandleAccess *m_pPrev;
	};

	void InternalReleaseHandle() const;
protected:
	HANDLE DangerousGetHandleEx() const { return (HANDLE)m_handle; }
	virtual void ReleaseHandle(HANDLE h) const;	
private:
	mutable intptr_t m_handle;
	const intptr_t m_invalidHandleValue;
	CBool m_bOwn;	

	template <class T> friend class CHandleKeeper;
};

template <typename T>
typename T::handle_type Handle(T& x) {
	return T::HandleAccess(x);
}

template <typename T>
typename T::handle_type BlockingHandle(T& x) {
	return T::BlockingHandleAccess(x);
}

ENUM_CLASS(HandleInheritability) {
	None,
	Inheritable
} END_ENUM_CLASS(HandleInheritability);

} // Ext::

