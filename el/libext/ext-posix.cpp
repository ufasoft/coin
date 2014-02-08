/*######     Copyright (c) 1997-2013 Ufasoft  http://ufasoft.com  mailto:support@ufasoft.com,  Sergey Pavlov  mailto:dev@ufasoft.com #######################################
#                                                                                                                                                                          #
# This program is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation;  #
# either version 3, or (at your option) any later version. This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the      #
# implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details. You should have received a copy of the GNU #
# General Public License along with this program; If not, see <http://www.gnu.org/licenses/>                                                                               #
##########################################################################################################################################################################*/

#include <el/ext.h>

#include <el/inc/crt/crt.h>

//!!! not only getopt

extern "C" {

#if !UCFG_WDM

long __cdecl API_lround(double d) {
	return (long)d;
}

extern "C" int __cdecl API_dclass(double d) {
	switch (_fpclass(d)) {
	case _FPCLASS_SNAN:
	case _FPCLASS_QNAN:
		return FP_NAN;
	case _FPCLASS_PINF:
	case _FPCLASS_NINF:
		return FP_INFINITE;
	case _FPCLASS_NN:
	case _FPCLASS_PN:
		return FP_NORMAL;
	case _FPCLASS_ND:
	case _FPCLASS_PD:
		return FP_SUBNORMAL;
	case _FPCLASS_NZ:
	case _FPCLASS_PZ:
		return FP_ZERO;
	default:
		Ext::ThrowImp(E_EXT_CodeNotReachable);
	}
}

extern "C" int __cdecl API_fdclass(float d) {
	return API_dclass(d);
}

extern "C" int __cdecl API_ldclass(long double d) {
	return API_dclass((double)d);
}

#if !UCFG_WCE
int __cdecl ftruncate(int fd, off_t length) {
	return _chsize(fd, (long)length); //!!!U
}
#endif

#endif // !UCFG_WDM

#if UCFG_WIN32
void __cdecl usleep(useconds_t useconds) {
	Sleep(useconds/1000);
}
#endif

void __cdecl delay(int us) {
#ifdef WDM_DRIVER
	::KeStallExecutionProcessor(us);
#else
	Sleep(us/1000);
#endif
}


/*
* Get next token from string *stringp, where tokens are possibly-empty
* strings separated by characters from delim.
*
* Writes NULs into the string at *stringp to end tokens.
* delim need not remain constant from call to call.
* On return, *stringp points past the last NUL written (if there might
* be further tokens), or is NULL (if there are definitely no more tokens).
*
* If *stringp is NULL, strsep returns NULL.
*/
char *
	AFXAPI strsep(char **stringp, const char *delim)
{
	register char *s;
	register const char *spanp;
	register int c, sc;
	char *tok;

	if ((s = *stringp) == NULL)
		return (NULL);
	for (tok = s;;) {
		c = *s++;
		spanp = delim;
		do {
			if ((sc = *spanp++) == c) {
				if (c == 0)
					s = NULL;
				else
					s[-1] = 0;
				*stringp = s;
				return (tok);
			}
		} while (sc != 0);
	}
	/* NOTREACHED */
}

/*
 * Copy src to string dst of size siz.  At most siz-1 characters
 * will be copied.  Always NUL terminates (unless siz == 0).
 * Returns strlen(src); if retval >= siz, truncation occurred.
 */
size_t AFXAPI strlcpy(char *d, const char *src, size_t siz)
{
	const char *s = src;
	size_t n = siz;

	/* Copy as many bytes as will fit */
	if (n && --n)
		do
		{
			if ((*d++ = *s++) == 0)
				break;
		}
		while (--n != 0);

	/* Not enough room in dst, add NUL and traverse rest of src */
	if (n == 0)
	{
		if (siz != 0)
			*d = '\0';		/* NUL-terminate dst */
		while (*s++)
			;
	}
	return(s - src - 1);	/* count does not include NUL */
}

size_t __cdecl strlcat(char *dst, const char *src, size_t siz) {
    char *d = dst;
    const char *s = src;
    size_t n = siz;
    while (n-- != 0 && *d)	// Find the end of dst and adjust bytes left but don't go past end 
        d++;
    size_t dlen = d - dst;
    if (0 == (n = siz - dlen))
        return dlen + strlen(s);
    for (; *s; ++s) {
        if (n != 1) {
            *d++ = *s;
            n--;
        }
	}
    *d = 0;
    return dlen + (s - src);  // count does not include NUL
}

unsigned long long __cdecl API_strtoull(const char *str, char **endptr, int base) {
	return _strtoui64(str, endptr, base);
}

} // extern "C"

