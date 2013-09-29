/*######     Copyright (c) 1997-2013 Ufasoft  http://ufasoft.com  mailto:support@ufasoft.com,  Sergey Pavlov  mailto:dev@ufasoft.com #######################################
#                                                                                                                                                                          #
# This program is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation;  #
# either version 3, or (at your option) any later version. This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the      #
# implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details. You should have received a copy of the GNU #
# General Public License along with this program; If not, see <http://www.gnu.org/licenses/>                                                                               #
##########################################################################################################################################################################*/

#include <el/ext.h>

#define PCRE_STATIC 1
#include <pcre.h>


#if UCFG_STDSTL && defined(_MSC_VER)
#	include "extstl.h"
#endif

using namespace std;
#include "regex"
using ExtSTL::regex_constants::syntax_option_type;

ASSERT_CDECL

namespace ExtSTL  {
using namespace Ext;

class StdRegexObj : public Object {
public:
	String m_pattern;
	int m_pcreOpts;
	pcre *m_re;

	StdRegexObj(RCString pattern, int options, bool bBinary);
	~StdRegexObj();
	pcre *FullRe();
private:
	pcre *m_reFull;
};


const int MAX_MATCHES = 20; //!!!

StdRegexObj::StdRegexObj(RCString pattern, int options, bool bBinary)
	:	m_pattern(pattern)
	,	m_reFull(0)
{
	m_pcreOpts = PCRE_JAVASCRIPT_COMPAT;
	if (!bBinary)
		m_pcreOpts |= PCRE_UTF8;
	if (options & regex_constants::icase)
		m_pcreOpts |= PCRE_CASELESS;
	if (options & regex_constants::nosubs)		//!!!?
		m_pcreOpts |= PCRE_NO_AUTO_CAPTURE;

	Blob utf8 = Encoding::UTF8.GetBytes(pattern);
	const char *error;
	int error_offset;
	unsigned char *table = 0;
	int errcode;
	if (!(m_re = pcre_compile2((const char*)utf8.constData(), m_pcreOpts, &errcode, &error, &error_offset, table)))
		throw RegexExc(MAKE_HRESULT(SEVERITY_ERROR, FACILITY_PCRE, errcode), EXT_STR(error << " at offset " << error_offset));
}

StdRegexObj::~StdRegexObj() {
	if (m_reFull)
		pcre_free(m_reFull);		
	pcre_free(m_re);
}

pcre *StdRegexObj::FullRe() {
	if (!m_reFull) {
		Blob utf8 = Encoding::UTF8.GetBytes("(?:"+m_pattern+")\\z");
		const char *error;
		int error_offset;
		unsigned char *table = 0;
		int errcode;
		if (!(m_reFull = pcre_compile2((const char*)utf8.constData(), m_pcreOpts | PCRE_ANCHORED, &errcode, &error, &error_offset, table)))
			throw RegexExc(MAKE_HRESULT(SEVERITY_ERROR, FACILITY_PCRE, errcode), EXT_STR(error << " at offset " << error_offset));
	}
	return m_reFull;
}

basic_regexBase::~basic_regexBase() {
}

void *basic_regexBase::Re() const {
	return m_pimpl->m_re;
}

void *basic_regexBase::FullRe() const {
	return m_pimpl->FullRe();
}

int PcreCheck(int r) {
	if (r < 0)
		throw RegexExc(MAKE_HRESULT(SEVERITY_ERROR, FACILITY_PCRE, -r));
	return r;
}

basic_regex<char>::basic_regex(RCString pattern, flag_type flags) {
	m_pimpl = new StdRegexObj(pattern, flags, true);
}

basic_regex<wchar_t>::basic_regex(RCString pattern, flag_type flags) {
	m_pimpl = new StdRegexObj(pattern, flags, false);
}

bool AFXAPI regex_searchImpl(const char *b, const char *e, match_results<const char*> *m, const basic_regexBase& re, bool bMatch, regex_constants::match_flag_type flags, const char *org) {
	void *pre = bMatch ? re.FullRe() : re.Re();

	int matches[MAX_MATCHES*3];
	int options = 0;	
	if (flags & regex_constants::match_not_bol)
		options |= PCRE_NOTBOL;
	if (flags & regex_constants::match_not_eol)
		options |= PCRE_NOTEOL;

	int rc = ::pcre_exec((pcre*)pre, NULL, b, int(e-b), 0, options, matches, MAX_MATCHES*3);

	if (rc < -1)
		PcreCheck(rc);
	if (rc < 1)
		return false;
	int count;
	PcreCheck(::pcre_fullinfo((pcre*)pre, 0, PCRE_INFO_CAPTURECOUNT, &count));
	if (m) {
		m->m_ready = true;
		m->m_org = b;
		m->Resize(count+1);
		for (int i=0; i<rc; i++) {
			sub_match<const char*>& sm = m->GetSubMatch(i);
			sm.first = b+matches[2*i];
			sm.second = b+matches[2*i+1];
			sm.matched = matches[2*i] >= 0;
		}
		m->m_prefix.first = b;
		m->m_prefix.second = (*m)[0].first;
		m->m_prefix.matched = true;

		m->m_suffix.first = (*m)[0].second;
		m->m_suffix.second = e;
		m->m_suffix.matched = true;
	}
	return true;
}

bool AFXAPI regex_searchImpl(const wchar_t *bs, const wchar_t *es, match_results<const wchar_t*> *m, const basic_regexBase& re, bool bMatch, regex_constants::match_flag_type flags, const wchar_t *org) {
	Blob utf8 = Encoding::UTF8.GetBytes(String(wstring(bs, es-bs)));
	const char *b = (const char *)utf8.constData(),
	           *e = (const char *)utf8.constData()+utf8.Size;
	bool r;
	if (m) {
		match_results<const char*> m8;
		if (r = regex_searchImpl(b, e, &m8, re, bMatch, flags, nullptr)) {
			m->m_ready = true;
			m->m_org = org;
			m->Resize(m8.size());
			for (int i=0; i<m8.size(); ++i) {
				const sub_match<const char*>& sm = m8[i];
				match_results<const wchar_t*>::value_type& dm = m->GetSubMatch(i);
				if (dm.matched = sm.matched) {
					dm.first = bs+Encoding::UTF8.GetCharCount(ConstBuf(b, sm.first-b));
					dm.second = bs+Encoding::UTF8.GetCharCount(ConstBuf(b, sm.second-b));
				}
			}
			m->m_prefix.first = bs;
			m->m_prefix.second = (*m)[0].first;
			m->m_prefix.matched = true;

			m->m_suffix.first = (*m)[0].second;
			m->m_suffix.second = es;
			m->m_suffix.matched = true;
		}
	} else
		r = regex_searchImpl(b, e, 0, re, bMatch, flags, nullptr);
	return r;
}

bool AFXAPI regex_searchImpl(string::const_iterator bi,  string::const_iterator ei, smatch *m, const basic_regexBase& re, bool bMatch, regex_constants::match_flag_type flags, string::const_iterator orgi) {
	const char *b = (const char *)&*bi,
	           *e = (const char *)&*ei;
	bool r;
	if (m) {
		match_results<const char*> m8;
		if (r = regex_searchImpl(b, e, &m8, re, bMatch, flags, nullptr)) {
			m->m_ready = true;
			m->m_org = orgi;
			m->Resize(m8.size());
			for (int i=0; i<m8.size(); ++i) {
				const sub_match<const char*>& sm = m8[i];
				smatch::value_type& dm = m->GetSubMatch(i);
				if (dm.matched = sm.matched) {
					dm.first = bi+(sm.first-b);
					dm.second = bi+(sm.second-b);
				}
			}
			m->m_prefix.first = bi;
			m->m_prefix.second = (*m)[0].first;
			m->m_prefix.matched = true;

			m->m_suffix.first = (*m)[0].second;
			m->m_suffix.second = ei;
			m->m_suffix.matched = true;
		}
	} else
		r = regex_searchImpl(b, e, 0, re, bMatch, flags, nullptr);
	return r;
}

struct ExtSTL_RegexTraits {
	EXT_DATA static const syntax_option_type DefaultB;
};

typedef DelayedStatic2<regex, ExtSTL_RegexTraits, String, syntax_option_type> ExtSTL_StaticRegex;
static ExtSTL_StaticRegex s_reRepl("\\${(\\w+)}");

const syntax_option_type ExtSTL_RegexTraits::DefaultB = regex_constants::ECMAScript;

struct IdxLenName {					// can't be local in old GCC
	sub_match<const char *> sm;
	string Name;
};

string AFXAPI regex_replace(const string& s, const regex& re, const string& fmt, regex_constants::match_flag_type flags) {
	const char *bs = s.c_str(), *es = bs+s.size(),
		*bfmt = fmt.c_str(), *efmt = bfmt+fmt.size();

	vector<IdxLenName> vRepl;
	for (cregex_iterator it(bfmt, efmt, s_reRepl), e; it!=e; ++it) {
		IdxLenName iln;
		iln.sm = (*it)[0];
		iln.Name = (*it)[1];
		vRepl.push_back(iln);
	}

	int last = 0;
	string r;
	const char *prev = bs;
	for (cregex_iterator it(bs, es, re, flags), e; it!=e; ++it) {
		r += string(prev, (*it)[0].first);
		const char *prevRepl = bfmt;
		for (int j=0; j<vRepl.size(); ++j) {
			r += string(prevRepl, vRepl[j].sm.first);
			String name = vRepl[j].Name;
			int idx = atoi(name);
			if (!idx && name!="0")
				Throw(E_FAIL);
			r += (*it)[idx];
			prevRepl = vRepl[j].sm.second;
		}
		r += string(prevRepl, efmt);
		prev = (*it)[0].second;
	}
	r += string(prev, es);
	return r;
}

}  // ExtSTL::


