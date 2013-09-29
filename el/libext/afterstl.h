/*######     Copyright (c) 1997-2013 Ufasoft  http://ufasoft.com  mailto:support@ufasoft.com,  Sergey Pavlov  mailto:dev@ufasoft.com #######################################
#                                                                                                                                                                          #
# This program is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation;  #
# either version 3, or (at your option) any later version. This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the      #
# implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details. You should have received a copy of the GNU #
# General Public License along with this program; If not, see <http://www.gnu.org/licenses/>                                                                               #
##########################################################################################################################################################################*/

#pragma once

#include EXT_HEADER(list)
#include EXT_HEADER_DYNAMIC_BITSET


namespace Ext {


template <class T> class Array {
public:
	Array(size_t size)
		:	m_p(new T[size])
	{
	}

	~Array() {
		delete[] m_p;
	}

	T *get() const { return m_p; }
	T *release() { return exchange(m_p, nullptr); }
private:
	T *m_p;
};

template<> class Array<char> {
public:
	Array(size_t size)
		:	m_p((char*)Malloc(size))
	{
	}

	~Array() {
		if (m_p)
			Free(m_p);
	}

	char *get() const { return m_p; }
	char *release() { return std::exchange(m_p, nullptr); }
private:
	char *m_p;
};



} // Ext::



namespace Ext {


/*!!!
class Istream {
public:
	std::istream& m_is;

	Istream(std::istream& is)
		:	m_is(is)
	{}

	void ReadString(String& s);
	void GetLine(String& s, char delim);
};


class Ostream : public ostream
{
public:
Ostream(streambuf *sb)
: ostream(sb)
{}
};*/

#if !defined(_MSC_VER) || _MSC_VER > 1400 

/*!!!R
template <class T>
class AutoPtr : public std::auto_ptr<T> {
	typedef std::auto_ptr<T> base;
public:
	AutoPtr()  // only this non-explicit constructor different from auto_ptr
		:	base()
	{}

	AutoPtr(const AutoPtr& ap) //!!!
		:	base((AutoPtr&)ap)
	{
	}

	AutoPtr(T *p)
		:	base(p)
	{}

	AutoPtr& operator=(const AutoPtr& ap) {
		base::operator=((AutoPtr&)ap);
		return _self;
	}

	AutoPtr& operator=(T *p) {
		base::reset(p);
		return _self;
	}

private:
	operator T*() const;		// dangerous, set<AutoPtr> is very undetectable bug
};  */

//!!!R #define AutoPtr std::unique_ptr
/*!!!R
template <class T> class COwnerArray : public std::vector<AutoPtr<T> > {
	typedef std::vector<AutoPtr<T> > base;
public:
	typedef typename base::iterator iterator;

	void Add(T *p) {
		base::push_back(AutoPtr<T>(p));
	}

	void Unlink(const T *p) {
		for (iterator i(this->begin()); i!=this->end(); ++i)
			if (i->get() == p) {
				i->release();
				erase(i);
				return;
			}
	}

	void Remove(const T *p) {
		for (iterator i(this->begin()); i!=this->end(); ++i)
			if (i->get() == p) {
				this->erase(i);
				return;
			}
	}
};
*/

template <class T> class CMTQueue {
	size_t m_cap;
	size_t m_mask;
	std::unique_ptr<byte> m_buf;
	T * volatile m_r, * volatile m_w;
public:
	explicit CMTQueue(size_t cap)
		:	m_cap(cap+1)
		,	m_mask(cap)
	{
		for (size_t n=cap; n; n>>=1)
			if (!(n & 1))
				Throw(E_FAIL);
		m_buf.reset(new byte[sizeof(T)*m_cap]);
		m_r = m_w = (T*)m_buf.get();
	}

	~CMTQueue() {
		clear();
	}

	size_t capacity() const { return m_cap-1; }

	size_t size() const { return (m_w-m_r) & m_mask; }

	bool empty() const { return m_r == m_w; }

	const T& front() const {
		if (m_r == m_w)
			Throw(E_EXT_IndexOutOfRange);
		return *m_r;
	}

	T& front() { return (T&)((const CMTQueue*)this)->front(); }

	T *Inc(T *p) {
		T *r = ++p;
		if (r == (T*)m_buf.get()+m_cap)
			r = (T*)m_buf.get();
		return r;
	}

	void push_back(const T& v) {
		T *n = Inc(m_w);
		if (n == m_r)
			Throw(E_EXT_QueueOverflow);
		new(m_w) T(v);
		m_w = n;
	}

	void pop_front() {
		if (empty())
			Throw(E_EXT_IndexOutOfRange);
		m_r->~T();
		m_r = Inc(m_r);
	}

	void clear() {
		while (!empty())
			pop_front();
	}
};

#endif

template <typename C, typename K>
bool Contains(const C& c, const K& key) {
	return c.find(key) != c.end();
}

template <typename C, class T>
bool ContainsInLinear(const C& c, const T& key) {
	return find(c.begin(), c.end(), key) != c.end();
}

template <class M, class K, class T>
bool Lookup(const M& m, const K& key, T& val) {
	typename M::const_iterator i = m.find(key);
	if (i == m.end())
		return false;
	val = i->second;
	return true;
}


template <typename S1, typename S2>
typename S1::const_iterator Search(const S1& s1, const S2& s2) {
	return std::search(std::begin(s1), std::end(s1), std::begin(s2), std::end(s2));
}



template <class Q, class T>
bool Dequeue(Q& q, T& val) {
	if (q.empty())
		return false;
	val = q.front();
	q.pop();
	return true;
}

template <class T> class MyList : public std::list<T>
{
	typedef std::list<T> base;
public:
	typedef typename base::const_iterator const_iterator;
	typedef typename base::iterator iterator;
	//!!!	typedef _Nodeptr MyNodeptr;
	typedef typename base::_Node *MyNodeptr;

#if UCFG_STDSTL
#	if defined(__GNUC__)
	static const_iterator ToConstIterator(void *pNode)
	{
		return const_iterator((MyNodeptr)pNode);
	}
#	elif defined(_MSC_VER) && _MSC_VER > 1400 
	static const_iterator ToConstIterator(void *pNode)
	{
		return _Const_iterator<false>((MyNodeptr)pNode);
	}
#	else
	static iterator ToConstIterator(void *pNode)
	{
		return ((MyNodeptr)pNode);
	}
#	endif
#else
	static const_iterator ToConstIterator(void *pNode)
	{
		return (MyNodeptr)pNode;
	}
#endif
	static iterator ToIterator(void *pNode) {
		return iterator((MyNodeptr)pNode);
	}
};

template <typename LI>
void *ListIteratorToPtr(LI li) {
#if defined(_STLP_LIST) || defined(__GNUC__)
    return li._M_node;
#else
    return li._Mynode();
#endif
}

/*!!!R
#ifdef _DEBUG//!!!D
#	define ASSERT_INTRUSIVE assert(empty() || (getP()->Prev != getP() && getP()->Prev != getP())) 
#else */
#	define ASSERT_INTRUSIVE
//!!!R #endif

template <typename T>
class IntrusiveList : NonCopiable {
	typedef IntrusiveList class_type;
public:
	typedef T value_type;

	class const_iterator {
	public:
		typedef std::bidirectional_iterator_tag iterator_category;
		typedef T value_type;
		typedef ssize_t difference_type;
		typedef T* pointer;
		typedef T& reference;

		explicit const_iterator(const value_type *p = nullptr)
			:	m_p(p) {
		}

		const_iterator& operator=(const const_iterator& it) {
			m_p = it.m_p;
			return _self;
		}

		const value_type& operator*() const { return *m_p; };
		const value_type *operator->() const { return &operator*(); };

		const_iterator& operator++() {
			m_p = m_p->Next;
			return *this;
		}

		const_iterator operator++(int) {
			const_iterator r(_self);
			operator++();
			return r;
		}

		const_iterator& operator--() {
			m_p = m_p->Prev;
			return *this;
		}

		const_iterator operator--(int) {
			const_iterator r(_self);
			operator--();
			return r;
		}

		bool operator==(const const_iterator& it) const { return m_p == it.m_p; }
		bool operator!=(const const_iterator& it) const { return !operator==(it); }

	protected:
		const value_type *m_p;

	};

	class iterator : public const_iterator {
		typedef const_iterator base;
	public:
		explicit iterator(value_type *p = nullptr)
			:	const_iterator(p) {
		}

		explicit iterator(const const_iterator& ci)
			:	const_iterator(ci) {
		}

		iterator& operator=(const iterator& it) {
			base::m_p = it.m_p;
			return _self;
		}

		value_type& operator*() const { return *const_cast<value_type*>(base::m_p); };
		value_type *operator->() const { return &operator*(); };

		iterator& operator++() {
			base::m_p = base::m_p->Next;
			return *this;
		}

		iterator operator++(int) {
			iterator r(_self);
			operator++();
			return r;
		}

		iterator& operator--() {
			base::m_p = base::m_p->Prev;
			return *this;
		}

		iterator operator--(int) {
			iterator r(_self);
			operator--();
			return r;
		}
};


	IntrusiveList()
		:	m_size(0)
	{
		getP()->Prev = getP()->Next = getP();
	}

//!!!R	size_t size() const { return m_size; }
	bool empty() const { return getP()->Next == getP(); }

	const_iterator begin() const {
		return const_iterator(getP()->Next);
	}

	const_iterator end() const {
		return const_iterator(getP());
	}

	iterator begin() {
		return iterator(getP()->Next);
	}

	iterator end() {
		return iterator(getP());
	}

	const value_type& front() const { return *begin(); }
	const value_type& back() const {
		const_iterator it = end();
		--it;
		return *it;
	}

	value_type& front() { return *begin(); }
	value_type& back() {
		iterator it = end();
		--it;
		return *it;
	}

	iterator insert(iterator it, value_type& v) {
		T *p = v.Prev = exchange(it->Prev, &v);
		v.Next = &*it;
		p->Next = &v;
		++m_size;

		ASSERT_INTRUSIVE;

		return iterator(&v);
	}

	void push_back(value_type& v) {
		insert(end(), v);
	}

	void erase(const_iterator it) {
		T *n = it->Next, *p = it->Prev;
		p->Next = n;
		n->Prev = p;
		m_size--;
//		ASSERT((value_type*)(it.operator->()) != getP());
#ifdef _DEBUG
		value_type *v = (value_type*)(it.operator->());
		v->Next = v->Prev = 0;
#endif
	}

	size_t size() const { return m_size; }

	void clear() {
		m_size = 0;
		getP()->Prev = getP()->Next = getP();
		ASSERT_INTRUSIVE;
	}

	void splice(iterator _Where, class_type& _Right, iterator _First) {
		const_iterator _Last = _First;
		++_Last;
		if (this != &_Right || (_Where != _First && _Where != _Last)) {
			value_type& v = *_First;
			_Right.erase(_First);
			insert(_Where, v);
		}
		ASSERT_INTRUSIVE;
	}
protected:
	byte m_base[sizeof(value_type)];
	size_t m_size;
	
	const T *getP() const { return (const T*)m_base; }
	T *getP() { return (T*)m_base; }

};


template <class I>
class Range {
public:
	I begin,
		end;

	//!!!	Range() {}

	Range(I b, I e)
		:	begin(b)
		,	end(e)
	{}
};

template <class C>
class CRange {
public:
	CRange(const C& c)
		:	b(c.begin())
		,	e(c.end())
	{}

	operator bool() const { return b != e; }
	void operator ++() {
		++b;
	}

	typename C::value_type& operator*() const { return *b; }
	typename C::value_type operator->() const { return &*b; }
private:
	typename C::const_iterator b,
		e;
};


template <class I>
void Reverse(const Range<I>& range) { reverse(range.begin, range.end); }

template <class I, class L>
void Sort(Range<I> range, L cmp) { sort(begin(range), end(range), cmp); }

template <class C>
void Sort(C& range) { sort(begin(range), end(range)); }

template <class C, class T>
void AddToUnsortedSet(C& c, const T& v) {
	if (find(c.begin(), c.end(), v) == c.end())
		c.push_back(v);
}

template <class C, class T>
void Remove(C& c, const T& v) {
	c.erase(std::remove(c.begin(), c.end(), v), c.end());
}


/*!!!
#if defined(_MSC_VER) && _MSC_VER >= 1600

template<typename T> inline size_t hash_value(const T& _Keyval) {
	return ((size_t)_Keyval ^ _HASH_SEED);
}

#endif
*/


template <class T>
class StaticList {
public:
	static T *Root;

	T *Next;

	StaticList()
		:	Next(0)
	{}
protected:
	explicit StaticList(bool)
		:	Next(Root)
	{
		Root = static_cast<T*>(this);
	}
};

template <class T>
class InterlockedSingleton {
public:
	InterlockedSingleton() {}

	T *operator->() {
		if (!s_p) {
			T *p = new T;
			if (Interlocked::CompareExchange(s_p, p, (T*)nullptr))
				delete p;
		}
		return s_p;
	}

	T *get() { return operator->(); }
	T& operator*() { return *operator->(); }
private:
	T * volatile s_p;

	InterlockedSingleton(const InterlockedSingleton&);
};

template <class T, class TR, class A, class B>
class DelayedStatic2 {
public:
	 mutable T * volatile m_p;
	 A m_a;
	 B m_b;

	 DelayedStatic2(const A& a, const B& b = TR::DefaultB)
		 :	m_p(0)
		 ,	m_a(a)
		 ,	m_b(b)
	 {}

	~DelayedStatic2() {
		delete exchange(m_p, nullptr);
	}

	const T& operator*() const {
		if (!m_p) {
			T *p = new T(m_a, m_b);
			CAlloc::DbgIgnoreObject(p);
			if (Interlocked::CompareExchange(m_p, p, (T*)0))
				delete p;
		}
		return *m_p;
	}

	operator const T&() const {
		return operator*();
	}
};

UInt64 ToUInt64AtBytePos(const std::dynamic_bitset<byte>& bs, size_t pos);

template <typename T, class L>
T clamp(const T& v, const T& lo, const T& hi, L pred) {
	return pred(v, lo) ? lo : pred(hi, v) ? hi : v;
}

template <typename T>
T clamp(const T& v, const T& lo, const T& hi) {
	return clamp<T, std::less<T>>(v, lo, hi, std::less<T>());
}



} // Ext::

namespace EXT_HASH_VALUE_NS {

template <class T>
size_t AFXAPI hash_value(const Ext::Pimpl<T>& v) { return hash_value(v.m_pimpl); }

} // stdext::


