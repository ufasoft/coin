/*######     Copyright (c) 1997-2013 Ufasoft  http://ufasoft.com  mailto:support@ufasoft.com,  Sergey Pavlov  mailto:dev@ufasoft.com #######################################
#                                                                                                                                                                          #
# This program is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation;  #
# either version 3, or (at your option) any later version. This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the      #
# implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details. You should have received a copy of the GNU #
# General Public License along with this program; If not, see <http://www.gnu.org/licenses/>                                                                               #
##########################################################################################################################################################################*/

#pragma once


//!!!R#if !UCFG_STDSTL || UCFG_WCE

#ifdef _MSC_VER
#	include <xlocinfo.h>
#else
#	define const _XA = 0;
#endif


//namespace _STL_NAMESPACE {
namespace ExtSTL {

class LocaleObjBase : public Ext::Object {
public:
	typedef Ext::Interlocked interlocked_policy;

};

class locale {
public:
	enum category {
		none = 0,
		collate = _M_COLLATE,
		ctype = _M_CTYPE,
		monetary = _M_MONETARY,
		numeric = _M_NUMERIC,
		time = _M_TIME,
		messages = _M_MESSAGES,
		all = _M_ALL
	};

	class id : Ext::NonCopiable {
		typedef id class_type;
	public:
		id(size_t val = 0);
		operator size_t();
	private:
		volatile Ext::Int32 m_val;
	};

	class facet : public Ext::Object {
	public:
		virtual ~facet() {}

		static size_t GetCat(const locale::facet **pp = 0, const locale *loc = 0) {
			return size_t(-1);
		}
	protected:
		explicit facet(size_t refs = 0) {
			m_dwRef = refs;
		}
	};

#if UCFG_FRAMEWORK
	EXT_DATA static mutex s_cs;
#endif

	locale();
	explicit locale(const char *locname);

	template <class F>
	locale(const locale& loc, const F *fac) {
		Init(loc, fac, F::GetCat(), F::id);
	}

	static const locale& classic() {
		return s_classic;
	}

	const facet *GetFacet(size_t id) const;
private:
	static volatile Ext::Int32 s_id;
	static locale s_classic;

	Ext::ptr<LocaleObjBase> m_pimpl;

	static Ext::ptr<LocaleObjBase> Init();
	void Init(const locale& loc, const facet *fac, int category, size_t id);
};

template <class F>
bool has_facet(const locale& _Loc) {
	EXT_LOCK (locale::s_cs) {
		return F::id || F::GetCat() != size_t(-1);
	}
}

template <class F>
struct FacetPtr {
	static const locale::facet *P;
};

template <class F>
const locale::facet *FacetPtr<F>::P = 0;

#if UCFG_FRAMEWORK

template <class F>
const F& use_facet(const locale& loc) {
	EXT_LOCK (locale::s_cs) {
		const locale::facet *save = FacetPtr<F>::P;
		const locale::facet *pf = loc.GetFacet(F::id);
		if (!pf && !(pf = save)) {
			if (F::GetCat(&pf, &loc) == size_t(-1))
				Ext::ThrowImp(E_EXT_InvalidCast);
			FacetPtr<F>::P = pf;
		}
		return static_cast<const F&>(*pf);
	}
}

#endif // UCFG_FRAMEWORK

class codecvt_base : public locale::facet {
	typedef locale::facet base;
public:
	enum result {
		ok, partial, error, noconv
	};

	explicit codecvt_base(size_t refs = 0)
		:	base(refs)
	{}
	
	bool always_noconv() const { return do_always_noconv(); }
	int max_length() const { return do_max_length(); }
	int encoding() const { return do_encoding(); }
protected:
	virtual bool do_always_noconv() const { return false; }
	virtual int do_max_length() const { return 1; }
	virtual int do_encoding() const { return 1; }
};

template <typename EL, typename B, typename ST>
class codecvt_Base : public codecvt_base {
	typedef codecvt_base base;
public:
	typedef EL intern_type;
	typedef B extern_type;
	typedef ST state_type;

	explicit codecvt_Base(size_t refs = 0)
		:	base(refs)
	{
	}

	result in(state_type& st, const B *f1, const B *l1, const B *& m1, EL *f2, EL *l2, EL *& m2) const {
		return do_in(st, f1, l1, m1, f2, l2, m2);
	}

	result out(state_type& st, const EL *f1, const EL *l1, const EL *& m1, B *f2, B *l2, B *& m2) const {
		return do_out(st, f1, l1, m1, f2, l2, m2);
	}

	result unshift(const state_type& st, extern_type* b2, extern_type* e2, extern_type *& m2) const {
		return do_unshift(st, b2, e2, m2);
	}

	int length(const state_type& st, const extern_type* b1, const extern_type* e1, size_t count) const {
		return do_length(st, b1, e1, count);
	}
protected:
	virtual ~codecvt_Base() {
	}

	virtual result do_in(state_type& st, const B *f1, const B *l1, const B *& m1, EL *f2, EL *l2, EL *& m2) const {
		Ext::ThrowImp(E_NOTIMPL);
	}

	virtual result do_out(state_type&, const EL *f1, const EL *, const EL *& m1, B *f2, B *, B *&m2) const {
		Ext::ThrowImp(E_NOTIMPL);
	}

	virtual result do_unshift(state_type& st, B *f2, B*, B *&m2) const {
		m2 = f2;
		return noconv;
	}

	virtual int do_length(const state_type& st, const extern_type* b1, const extern_type* e1, size_t count) const {
		Ext::ThrowImp(E_NOTIMPL);
	}
};

template <typename EL, typename B, typename ST>
class codecvt : public codecvt_Base<EL, B, ST> {
	typedef codecvt_Base<EL, B, ST> base;
public:
	static locale::id id;

	explicit codecvt(size_t refs = 0)
		:	base(refs)
	{}

	static size_t GetCat(const locale::facet **pp = 0, const locale *loc = 0) {
		if (pp && !*pp)
			*pp = new codecvt;
		return _X_CTYPE;
	}

};

template<> AFX_CLASS class codecvt<wchar_t, char, _Mbstatet> : public codecvt_Base<wchar_t, char, mbstate_t> {
	typedef codecvt_Base<wchar_t, char, mbstate_t> base;
public:
	static EXT_DATA locale::id id;

	explicit codecvt(size_t refs = 0)
		:	base(refs)
	{}

	static size_t GetCat(const locale::facet **pp = 0, const locale *loc = 0) {
		if (pp && !*pp)
			*pp = new codecvt;
		return _X_CTYPE;
	}
};

template <typename EL, typename B, typename ST>
locale::id codecvt<EL, B, ST>::id;

template <typename EL>
EL tolower(EL ch, const locale& loc) {
	return use_facet<ctype<EL>>(loc).tolower(ch);
}

template <typename EL>
EL toupper(EL ch, const locale& loc) {
	return use_facet<ctype<EL>>(loc).toupper(ch);
}

EXT_API bool AFXAPI islower(wchar_t ch, const locale& loc = locale(0));
EXT_API bool AFXAPI isupper(wchar_t ch, const locale& loc = locale(0));
EXT_API bool AFXAPI isalpha(wchar_t ch, const locale& loc = locale(0));
EXT_API wchar_t AFXAPI toupper(wchar_t ch, const locale& loc = locale(0));
EXT_API wchar_t AFXAPI tolower(wchar_t ch, const locale& loc = locale(0));

struct ctype_base : public locale::facet {
	typedef short mask;

	enum {
		lower = _LOWER,
		upper = _UPPER,
		alpha = lower | upper | _XA,
		digit = _DIGIT,
		xdigit = _HEX,
		alnum = alpha | digit,
		punct = _PUNCT,
		graph = alnum | punct,
		print = graph | xdigit,
		cntrl = _CONTROL,
		space = _SPACE | _BLANK
	};
};

template <typename EL>
class ctypeBase : public ctype_base {
public:
	typedef EL char_type;

	static locale::id id;

	bool is(mask m, char_type ch) const {
		return do_is(m, ch);
	}

	char narrow(char_type ch, char dflt = '\0') const {
		Ext::ThrowImp(E_NOTIMPL);
	}

	char_type tolower(char_type ch) const { return do_tolower(ch); }
	char_type toupper(char_type ch) const { return do_toupper(ch); }
	char_type widen(char ch) const { return do_widen(ch); }
protected:
	virtual bool do_is(mask m, char_type ch) const {
		Ext::ThrowImp(E_NOTIMPL);
	}

	virtual char_type do_tolower(char_type ch) const {
		Ext::ThrowImp(E_NOTIMPL);
	}

	virtual char_type do_toupper(char_type ch) const {
		Ext::ThrowImp(E_NOTIMPL);
	}

	virtual char_type do_widen(char_type ch) const {
		Ext::ThrowImp(E_NOTIMPL);
	}
};

template <typename EL>
class ctype : public ctypeBase<EL> {
};

template <>
class ctype<char> : public ctypeBase<char> {
public:
	static size_t GetCat(const locale::facet **pp = 0, const locale *loc = 0) {
		if (pp && !*pp)
			*pp = new ctype;
		return _X_CTYPE;
	}
protected:
	bool do_is(mask m, char_type ch) const override {
		return _isctype(ch, m);
	}

	char_type do_tolower(char_type ch) const override {
		return (char_type)::tolower(ch);
	}

	char_type do_widen(char_type ch) const override {
		return ch;
	}
};

template <>
class ctype<wchar_t> : public ctypeBase<wchar_t> {
public:
	static size_t GetCat(const locale::facet **pp = 0, const locale *loc = 0) {
		if (pp && !*pp)
			*pp = new ctype;
		return _X_CTYPE;
	}
protected:
	bool do_is(mask m, char_type ch) const override {
		return iswctype(ch, m);
	}

	char_type do_tolower(char_type ch) const override {
		return ::towlower(ch);
	}

	char_type do_widen(char_type ch) const override {
		return (unsigned char)ch;	//!!!TODO
	}
};


template <class EL>
locale::id ctype<EL>::id;

template <class EL>
class collate : public locale::facet {
public:
	typedef EL char_type;

	static locale::id id;
};





} // ExtSTL::


//#endif // UCFG_STDSTL || UCFG_WCE



