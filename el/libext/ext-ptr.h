#pragma once

// Smart Pointers

namespace Ext {


template <typename T> class SelfCountPtr {
	T *m_p;
	RefCounter *m_pRef;

	void Destroy() {
		if (m_pRef && !--m_pRef) {
			delete m_pRef;
			delete m_p;
		}
	}
public:
	SelfCountPtr()
		:	m_p(0)
		,	m_pRef(0)
	{}

	SelfCountPtr(T *p)
		:	m_p(p)
		,	m_pRef(new RefCounter(1))
	{}

	SelfCountPtr(const SelfCountPtr& p)
		:	m_p(p.m_p)
		,	m_pRef(p.m_pRef)
	{
		m_pRef++;
	}

	~SelfCountPtr() {
		Destroy();
	}

	bool operator<(const SelfCountPtr& p) const { return m_p < p.m_p; }
	bool operator==(const SelfCountPtr& p) const { return m_p == p.m_p; }
	bool operator==(T *p) const { return m_p == p; }

	SelfCountPtr& operator=(const SelfCountPtr& p) {
		if (&p != this) {
			Destroy();
			m_p = p.m_p;
			++*(m_pRef = p.m_pRef);
		}
		return *this;
	}

	operator T*() const { return m_p; }
	T *operator->() const { return m_p; }
};

template <class T, class L> class CCounterIncDec {
public:
	static RefCounter __fastcall AddRef(T *p) {
		return L::Increment(p->m_dwRef);
	}

	static RefCounter __fastcall Release(T *p) {
		RefCounter r = L::Decrement(p->m_dwRef);
		if (!r)
			delete p;
		return r;
	}
};

template <class T> class CAddRefIncDec {
public:
	static RefCounter AddRef(T *p) {
		return p->AddRef();
	}

	static RefCounter Release(T *p) {
		return p->Release();
	}
};

template <typename T>
class PtrBase {
public:
	typedef T element_type;
	PtrBase(T *p = 0)
		:	m_p(p)
	{}

	T *get() const noexcept { return m_p; }
	T *operator->() const noexcept { return m_p; }

	void swap(PtrBase& p) noexcept {
		std::swap(m_p, p.m_p);
	}
protected:
	T *m_p;
};

template <typename T, typename L>
class RefPtr : public PtrBase<T> {
	typedef PtrBase<T> base;
public:
	using base::get;

	RefPtr(T *p = 0)
		:	base(p)
	{
		AddRef();
	}

	RefPtr(const RefPtr& p)
		:	base(p)
	{
		AddRef();
	}

	RefPtr(EXT_RV_REF(RefPtr) rv)
		:	base(rv.m_p)
	{
		rv.m_p = nullptr;
	}

	~RefPtr() {
		Destroy();
	}

	bool operator<(const RefPtr& p) const { return m_p < p.m_p; }
	bool operator==(const RefPtr& p) const { return m_p == p.m_p; }
	bool operator==(T *p) const { return m_p == p; }

	RefPtr& operator=(const RefPtr& p) {
		if (&p != this) {
			Destroy();
			m_p = p.m_p;
			AddRef();
		}
		return *this;
	}

	void Attach(T *p) {
		if (m_p)
			Throw(E_EXT_NonEmptyPointer);
		m_p = p;
	}

	operator T*() const { return get(); }

	T** AddressOf() {
		if (m_p)
			Throw(E_EXT_NonEmptyPointer);
		return &m_p;
	}

	void reset() {
		if (m_p)
			L::Release(exchange(m_p, (T*)0));
	}
protected:
	using base::m_p;

	void Destroy() {
		if (m_p)
			L::Release(m_p);
	}

	void AddRef() {
		if (m_p)
			L::AddRef(m_p);
	}
};


template <typename T, typename I = typename ptr_traits<T>::interlocked_policy>
class ptr : public RefPtr<T, CCounterIncDec<T, I> > {
public:
	typedef ptr class_type;
	typedef RefPtr<T, CCounterIncDec<T, I> > base;

	ptr(T *p = 0)
		:	base(p)
	{}

	ptr(const std::nullptr_t&)
		:	base(0)
	{}

	ptr(const ptr& p)
		:	base(p)
	{}

	ptr(EXT_RV_REF(ptr) rv)
		:	base(static_cast<EXT_RV_REF(base)>(rv))
	{}

	template <class U>
	ptr(const ptr<U>& pu)
		:	base(pu.get())
	{}

	ptr& operator=(const ptr& p) {
		base::operator=(p);
		return *this;
	}

	ptr& operator=(T *p) {
		base::Destroy();
		m_p = p;
		base::AddRef();
		return *this;
	}

	template <class U>
	ptr& operator=(const ptr<U>& pu) {
		base::operator=(pu.get());
		return *this;
	}

	ptr& operator=(const std::nullptr_t& np) {
		return operator=(class_type(np));
	}

/*!!!R
	T *get_P() const { return (T*)*this; }
	DEFPROP_GET(T*, P);
	*/

	ptr *This() { return this; }

	RefCounter use_count() const { return m_p ? m_p->m_dwRef : 0; }
protected:
	using base::m_p;
};

template <typename U, typename T, typename L> U *StaticCast(const ptr<T, L>& p) 	{
	return static_cast<U*>(p.get());
}

template <class T>
class Pimpl {
public:
	ptr<T> m_pimpl;

	Pimpl() {}

	Pimpl(const Pimpl& x)
		:	m_pimpl(x.m_pimpl)
	{}

	Pimpl(EXT_RV_REF(Pimpl) rv)
		:	m_pimpl(static_cast<EXT_RV_REF(ptr<T>)>(rv.m_pimpl))
	{}

	EXPLICIT_OPERATOR_BOOL() const {
		return m_pimpl ? EXT_CONVERTIBLE_TO_TRUE : 0;
	}

	Pimpl& operator=(const Pimpl& v) {
		m_pimpl = v.m_pimpl;
		return *this;
	}

	Pimpl& operator=(EXT_RV_REF(Pimpl) rv) {
		m_pimpl.operator=(static_cast<EXT_RV_REF(ptr<T>)>(rv.m_pimpl));
		return *this;
	}

	bool operator<(const Pimpl& tn) const {
		return m_pimpl < tn.m_pimpl;
	}

	bool operator==(const Pimpl& tn) const {
		return m_pimpl == tn.m_pimpl;
	}

	bool operator!=(const Pimpl& tn) const {
		return !operator==(tn);
	}
protected:
};

template <typename T>
class addref_ptr : public RefPtr<T, CAddRefIncDec<T> > {
public:
	typedef addref_ptr class_type;
	typedef RefPtr<T, CAddRefIncDec<T> > base;

	addref_ptr(T *p = 0)
		:	base(p)
	{}

	addref_ptr(const base& p) //!!! was ptr
		:	base(p)
	{}

	addref_ptr& operator=(const addref_ptr& p)
	{
		base::operator=(p);
		return *this;
	}
/*!!!R
	T *get_P() const { return (T*)*this; }
	DEFPROP_GET(T*, P);
	*/
};

template <class T>
class AFX_CLASS CRefCounted {
public:
	class AFX_CLASS CRefCountedData : public T {
	public:
		RefCounter m_dwRef;

		void AddRef() {
			Interlocked::Increment(m_dwRef);
		}

		void Release() {
			if (!Interlocked::Decrement(m_dwRef))
				delete this;
		}
	};

	CRefCountedData *m_pData;

	CRefCounted()
		:	m_pData(new CRefCountedData)
	{
		m_pData->m_dwRef = 1;
	}

	CRefCounted(const CRefCounted& rc)
		:	m_pData(rc.m_pData)
	{
		m_pData->AddRef();
	}

	CRefCounted(CRefCountedData *p)
		:	m_pData(p)
	{
		m_pData->m_dwRef = 1;
	}

	~CRefCounted() {
		m_pData->Release();
	}

	void Init(CRefCountedData *p) {
		m_pData->Release();
		m_pData = p;
		m_pData->m_dwRef = 1;
	}

	CRefCounted<T>& operator=(const CRefCounted<T>& val) {
		if (m_pData != val.m_pData) {
			m_pData->Release();
			m_pData = val.m_pData;
			m_pData->AddRef();
		}
		return *this;
	}
};

} // Ext::


namespace EXT_HASH_VALUE_NS {
	template <typename T> size_t hash_value(const ptr<T>& v) {
	    return reinterpret_cast<size_t>(v.get());
	}
}
