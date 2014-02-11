/*######     Copyright (c) 1997-2013 Ufasoft  http://ufasoft.com  mailto:support@ufasoft.com,  Sergey Pavlov  mailto:dev@ufasoft.com #######################################
#                                                                                                                                                                          #
# This program is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation;  #
# either version 3, or (at your option) any later version. This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the      #
# implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details. You should have received a copy of the GNU #
# General Public License along with this program; If not, see <http://www.gnu.org/licenses/>                                                                               #
##########################################################################################################################################################################*/

#pragma once

#if !UCFG_C99_FUNC
#	define __func__ __FUNCTION__
#endif

#ifdef __cplusplus
#	define __BEGIN_DECLS extern "C" {
#	define __END_DECLS }
#else
#	define __BEGIN_DECLS
#	define __END_DECLS
#endif

#define __LONG_MAX__ LONG_MAX

#define LITTLE_ENDIAN 1234
#define _LITTLE_ENDIAN LITTLE_ENDIAN
#define __LITTLE_ENDIAN LITTLE_ENDIAN

#define BIG_ENDIAN 4321
#define _BIG_ENDIAN BIG_ENDIAN
#define __BIG_ENDIAN BIG_ENDIAN

#define BYTE_ORDER LITTLE_ENDIAN
#define _BYTE_ORDER LITTLE_ENDIAN
#define __BYTE_ORDER LITTLE_ENDIAN

#define __SIZEOF_WCHAR_T__ 2

#if UCFG_EXTENDED && (!UCFG_STDSTL || UCFG_SPECIAL_CRT)
#	include <machine/ansi.h>
#endif

#ifdef  _WIN64
	typedef __int64    ssize_t;
#else
	typedef __w64 int   ssize_t;
#endif

typedef unsigned __int64 u_int64_t;
typedef unsigned __int64 uint64_t;
typedef __int64 int64_t;


__BEGIN_DECLS
	unsigned short		__cdecl _byteswap_ushort(unsigned short);
	unsigned long		__cdecl _byteswap_ulong(unsigned long value);
	unsigned __int64	__cdecl _byteswap_uint64(unsigned __int64 value);
__END_DECLS

#pragma intrinsic(_byteswap_ushort, _byteswap_ulong, _byteswap_uint64)


#define _SYS_ENDIAN_H_

#define swap16 _byteswap_ushort
#define swap32 _byteswap_ulong
#define swap64 _byteswap_uint64

#define htobe16 swap16
#define htobe32 swap32
#define htobe64 swap64
#define be16toh swap16
#define be32toh swap32
#define be64toh swap64

#define htole16(x) (x)
#define htole32(x) (x)
#define htole64(x) (x)
#define le16toh(x) (x)
#define le32toh(x) (x)
#define le64toh(x) (x)


