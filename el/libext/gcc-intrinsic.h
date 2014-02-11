/*######     Copyright (c) 1997-2013 Ufasoft  http://ufasoft.com  mailto:support@ufasoft.com,  Sergey Pavlov  mailto:dev@ufasoft.com #######################################
#                                                                                                                                                                          #
# This program is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation;  #
# either version 3, or (at your option) any later version. This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the      #
# implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details. You should have received a copy of the GNU #
# General Public License along with this program; If not, see <http://www.gnu.org/licenses/>                                                                               #
##########################################################################################################################################################################*/

#pragma once


#if defined(_M_IX86) || defined(_M_X64) 
#	include <x86intrin.h>
#	include <cpuid.h>
#endif

__BEGIN_DECLS

#ifndef _rotr
extern __inline unsigned int __attribute__((__gnu_inline__, __always_inline__)) DECLSPEC_ARTIFICIAL __rord (unsigned int __X, int __C) {
	 return (__X >> __C) | (__X << (32 - __C));
}
#	define _rotr(a,b)		__rord((a), (b))
#endif

#ifndef _rotl
extern __inline unsigned int __attribute__((__gnu_inline__, __always_inline__)) DECLSPEC_ARTIFICIAL __rold (unsigned int __X, int __C) {
	 return (__X << __C) | (__X >> (32 - __C));
}
#	define _rotl(a,b)		__rold((a), (b))
#endif

#if defined(_M_IX86) || defined(_M_X64) 


#	ifndef _rdtsc
inline volatile long long __rdtsc() {
   register long long tsc asm("eax");
   asm volatile (".byte 0x0F, 0x31" : : : "eax", "edx");	// RDTSC
   return tsc;
} 
#		define _rdtsc()		__rdtsc()
#	endif

#define swap32 __builtin_bswap32

#endif

/*!!!D

inline void __cpuid(int info[4], int typ) {
	__asm__ __volatile__ ("cpuid":\
                      	"=a" (info[0]), "=b" (info[1]), "=c" (info[2]), "=d" (info[3]) : "a" (typ));
}

*/

inline unsigned long long _rotr64(unsigned long long v, int off) {
#ifdef _lrotr
	return _lrotr(v, off);
#else
	return (v >> off) | (v << 64-off);
#endif
}

inline unsigned char _BitScanReverse(unsigned long * _Index, unsigned long _Mask) {
	if (0 == _Mask)
		return 0;
	*_Index = 31 - __builtin_clz(_Mask);
	return 1;
}

inline unsigned char _BitScanReverse64(unsigned long * _Index, unsigned long long _Mask) {
	if (0 == _Mask)
		return 0;
	*_Index = 63 - __builtin_clzll(_Mask);
	return 1;
}


__END_DECLS


