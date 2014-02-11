/*######     Copyright (c) 1997-2013 Ufasoft  http://ufasoft.com  mailto:support@ufasoft.com,  Sergey Pavlov  mailto:dev@ufasoft.com #######################################
#                                                                                                                                                                          #
# This program is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation;  #
# either version 3, or (at your option) any later version. This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the      #
# implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details. You should have received a copy of the GNU #
# General Public License along with this program; If not, see <http://www.gnu.org/licenses/>                                                                               #
##########################################################################################################################################################################*/

#pragma once

namespace Ext {

template <typename C>
C *StrCpy(C *d, const C *s) {
	for (C *p=d; *p++ = *s++;)
		;
	return d;
}

template <typename C>
int StrCmp(const C *s1, const C *s2) {
	while (true) {
		C ch1=*s1++, ch2=*s2++;
		if (ch1 < ch2)
			return -1;
		if (ch1 > ch2)
			return 1;
		if (!ch1)
			return 0;
	}
}

template <typename C>
int StrNCmp(const C *s1, const C *s2, size_t count) {
	while (count--) {
		C ch1 = *s1++;
		C ch2 = *s2++;
		if (ch1 < ch2)
			return -1;
		if (ch1 > ch2)
			return 1;
		if (!ch1)
			break;
	}
	return 0;
}

template <typename C>
C *StrStr(const C *ws1, const C *ws2) {
	if (!*ws2)
		return (C*)ws1;
	for (C *cp=(C*)ws1; *cp; ++cp) {
		C *s2 = (C*)ws2;
		for (C *s1=cp; *s1 && *s1==*s2; ++s1, ++s2)
			;
		if (!*s2)
			return cp;
	}
	return 0;
}

template <typename C>
const C *StrChr(const C *s, C ch) {
	C ch2;
	for (;(ch2=*s) && ch2!=ch; ++s)
		;
	return ch2==ch ? s : 0;
}

template <typename C>
size_t StrCSpn(const C *s, const C *c) {
	const C *p=s;
	for (C ch; (ch=*p) && !StrChr(c, ch); ++p)
		;
	return p-s;
}

#ifdef _WSTRING_DEFINED
/*!!!R
template <typename C>
size_t StrLen(const C *s) {
	const C *e = s;
	while (*e++)
		;
	return e-s-1;
}

template<> inline size_t StrLen<char>(const char *s) { return strlen(s); }
template<> inline size_t StrLen<wchar_t>(const wchar_t *s) { return wcslen(s); }
*/

template<> inline char *StrCpy<char>(char *d, const char *s) { return strcpy(d, s); }
template<> inline wchar_t *StrCpy<wchar_t>(wchar_t *d, const wchar_t *s) { return wcscpy(d, s); }

template<> inline const char *StrChr<char>(const char *s, char ch) { return strchr(s, ch); }
template<> inline const wchar_t *StrChr<wchar_t>(const wchar_t *s, wchar_t ch) { return wcschr(s, ch); }

template<> inline char *StrStr<char>(const char *s, const char *ss) { return (char*)strstr(s, ss); }
template<> inline wchar_t *StrStr<wchar_t>(const wchar_t *s, const wchar_t *ss) { return (wchar_t*)wcsstr(s, ss); }
//!!!TODO other funcs


#endif // _WSTRING_DEFINED


} // Ext::

