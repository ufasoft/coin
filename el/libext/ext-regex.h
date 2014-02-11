/*######     Copyright (c) 1997-2013 Ufasoft  http://ufasoft.com  mailto:support@ufasoft.com,  Sergey Pavlov  mailto:dev@ufasoft.com #######################################
#                                                                                                                                                                          #
# This program is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation;  #
# either version 3, or (at your option) any later version. This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the      #
# implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details. You should have received a copy of the GNU #
# General Public License along with this program; If not, see <http://www.gnu.org/licenses/>                                                                               #
##########################################################################################################################################################################*/

#pragma once

namespace Ext {

struct RegexTraits {
	EXT_API static const std::regex_constants::syntax_option_type DefaultB;
};

typedef DelayedStatic2<std::regex, RegexTraits, String , std::regex_constants::syntax_option_type> StaticRegex;
typedef DelayedStatic2<std::wregex, RegexTraits, String , std::regex_constants::syntax_option_type> StaticWRegex;


class RegexExc : public std::regex_error {
public:
	String Message;
	HRESULT HResult;

	RegexExc(HRESULT hr, RCString msg = nullptr)
		:	HResult(hr)
		,	Message(msg)
		,	std::regex_error(std::regex_constants::error_escape)				//!!! error_escape is just a stub
	{}

	/*!!!R
	RegexExc(const RegexExc& ex)
		:	base(ex.HResult, ex.m_message)
		,	std::regex_error(std::regex_constants::error_escape)				//!!! error_escape is just a stub	//!!!
	{}*/

	~RegexExc() throw() {
	}
};

} // Ext::

#if UCFG_STDSTL && UCFG_USE_REGEX && !UCFG_CPP11_HAVE_REGEX
namespace ExtSTL {
#else
namespace std {
#endif

template<> class sub_match<Ext::String::const_iterator> : public std::pair<Ext::String::const_iterator, Ext::String::const_iterator> {
public:
	typedef Ext::String::Char value_type;
	typedef Ext::String::difference_type difference_type;
	typedef Ext::String::const_iterator iterator;
	typedef Ext::String string_type;
	
	Ext::CBool matched;

	difference_type length() const {
		return matched ? std::distance(this->first, this->second) : 0;
	}

	string_type str() const {
		return matched ? string_type(first, second) : string_type();
	}

	operator string_type() const { return str(); }

	int compare(const value_type *p) const { return str().Compare(p); }
	int compare(const string_type& x) const { return str().Compare(x); }
	int compare(const sub_match& x) const { return compare(x.str()); }

	bool operator==(const sub_match& sm) const { return !compare(sm); }
	bool operator!=(const sub_match& sm) const { return !operator==(sm); }

	void Init(const std::wcsub_match& sm, Ext::String::const_iterator mb, const wchar_t *org) {
		matched = sm.matched;
		first = mb+(sm.first-org);
		second = mb+(sm.second-org);
	}
};

typedef sub_match<Ext::String::const_iterator> Ssub_match;

template<> class match_results<Ext::String::const_iterator> : protected vector<Ssub_match> {
	typedef vector<Ssub_match> base;
public:
	typedef Ssub_match value_type;
	typedef const value_type& const_reference;
	typedef Ext::String::Char char_type;
	typedef Ext::String string_type;

	using base::allocator_type;
	using base::empty;
	using base::size;
	using base::begin;
	using base::end;

	match_results()
		:	m_null()
	{
	}

	bool ready() const { return bool(m_ready); }

	const_reference operator[](size_t idx) const {
		return idx < size() ? base::operator[](idx) : m_null;
	}

	const_reference prefix() const { return m_prefix; }
	const_reference suffix() const { return m_suffix; }

	size_t length(int idx = 0) const { return operator[](idx).length(); }	
	size_t position(int idx = 0) const { return distance(m_org, operator[](idx).first); }
	string_type str(int idx = 0) const { return operator[](idx).str(); }

	const value_type m_null;
	value_type m_prefix;
	value_type m_suffix;
	Ext::String::const_iterator m_org;
	Ext::CBool m_ready;

	//!!! Should be Private
	value_type& GetSubMatch(int idx) {
		return base::operator[](idx);
	}

	void Resize(size_t size) {
		base::resize(size);
	}
};

} // std::
namespace Ext {
	typedef std::match_results<String::const_iterator> Smatch;
	typedef std::regex_iterator<String::const_iterator> Sregex_iterator;
} namespace std {


inline bool regex_search(const TCHAR *b, match_results<const TCHAR*>& m, const basic_regex<TCHAR>& re, regex_constants::match_flag_type flags = regex_constants::match_default) {
	return regex_search(b, b+char_traits<TCHAR>::length(b), m, re, flags);
}

inline bool regex_search(const TCHAR *b, const basic_regex<TCHAR>& re, regex_constants::match_flag_type flags = regex_constants::match_default) {
	return regex_search(b, b+char_traits<TCHAR>::length(b), re, flags);
}

inline bool regex_match(const TCHAR *b, match_results<const TCHAR*>& m, const basic_regex<TCHAR>& re, regex_constants::match_flag_type flags = regex_constants::match_default) {
	return regex_match(b, b+char_traits<TCHAR>::length(b), m, re, flags);
}

inline bool regex_match(const TCHAR *b, const basic_regex<TCHAR>& re, regex_constants::match_flag_type flags = regex_constants::match_default) {
	return regex_match(b, b+char_traits<TCHAR>::length(b), re, flags);
}

EXT_API bool AFXAPI regex_searchImpl(Ext::String::const_iterator bs,  Ext::String::const_iterator es, Ext::Smatch *m, const wregex& re, bool bMatch, regex_constants::match_flag_type flags, Ext::String::const_iterator org);

inline bool regex_search(Ext::String::const_iterator b,  Ext::String::const_iterator e, Ext::Smatch& m, const wregex& re, regex_constants::match_flag_type flags = regex_constants::match_default) {
	return regex_searchImpl(b, e, &m, re, false, flags,	b);
}

inline bool regex_search(Ext::String::const_iterator b,  Ext::String::const_iterator e, const wregex& re, regex_constants::match_flag_type flags = regex_constants::match_default) {
	return regex_searchImpl(b, e, 0, re, false, flags | regex_constants::match_any,	b);
}

inline bool regex_search(Ext::RCString s, Ext::Smatch& m, const wregex& re, regex_constants::match_flag_type flags = regex_constants::match_default) {
	return regex_search(s.begin(), s.end(), m, re, flags);
}

inline bool regex_search(Ext::RCString s, const wregex& re, regex_constants::match_flag_type flags = regex_constants::match_default) {
	return regex_search(s.begin(), s.end(), re, flags);
}

inline bool regex_match(Ext::String::const_iterator b,  Ext::String::const_iterator e, Ext::Smatch& m, const wregex& re, regex_constants::match_flag_type flags = regex_constants::match_default) {
	return regex_searchImpl(b, e, &m, re, true, flags,	b);
}

inline bool regex_match(Ext::String::const_iterator b,  Ext::String::const_iterator e, const wregex& re, regex_constants::match_flag_type flags = regex_constants::match_default) {
	return regex_searchImpl(b, e, 0, re, true, flags | regex_constants::match_any,	b);
}

inline bool regex_match(Ext::RCString s, Ext::Smatch& m, const wregex& re, regex_constants::match_flag_type flags = regex_constants::match_default) {
	return regex_match(s.begin(), s.end(), m, re, flags);
}

inline bool regex_match(Ext::RCString s, const regex& re, regex_constants::match_flag_type flags = regex_constants::match_default) {
	return regex_match(s.c_str(), re, flags);
}

inline bool regex_match(Ext::RCString s, const wregex& re, regex_constants::match_flag_type flags = regex_constants::match_default) {
	return regex_match(s.begin(), s.end(), re, flags);
}


} // std::




