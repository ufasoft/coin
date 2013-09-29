/*######     Copyright (c) 1997-2013 Ufasoft  http://ufasoft.com  mailto:support@ufasoft.com,  Sergey Pavlov  mailto:dev@ufasoft.com #######################################
#                                                                                                                                                                          #
# This program is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation;  #
# either version 3, or (at your option) any later version. This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the      #
# implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details. You should have received a copy of the GNU #
# General Public License along with this program; If not, see <http://www.gnu.org/licenses/>                                                                               #
##########################################################################################################################################################################*/

#pragma once


#if defined(_MSC_VER)
#	if _MSC_VER < 1400

	#if !defined(UNALIGNED)
	#if defined(_M_IA64) || defined(_M_AMD64)
	#define UNALIGNED __unaligned
	#else
	#define UNALIGNED
	#endif
	#endif




	#ifndef _ERRCODE_DEFINED
	#define _ERRCODE_DEFINED
	/* errcode is deprecated in favor or errno_t, which is part of the standard proposal */
	#if !defined(__midl)
	__declspec(deprecated) typedef int errcode;
	#else
	typedef int errcode;
	#endif

	typedef int errno_t;
	#endif
#	endif
#else
#	define UNALIGNED

#endif  // defined(_MSC_VER) && _MSC_VER < 1400

#if defined(_M_MRX000) || defined(_M_ALPHA) || defined(_M_PPC) || defined(_M_IA64) || defined(_M_AMD64) || defined(_M_ARM)
#	define ALIGNMENT_MACHINE
#	ifdef _MSC_VER
#		define UNALIGNED __unaligned
#	endif
#	if defined(_WIN64)
#		define UNALIGNED64 __unaligned
#	else
#		define UNALIGNED64
#	endif
#else
#	undef ALIGNMENT_MACHINE
#	define UNALIGNED
#	define UNALIGNED64
#endif

	/* _countof helper */
#if !defined(_countof)
#	if !defined(__cplusplus)
#		define _countof(_Array) (sizeof(_Array) / sizeof(_Array[0]))
#	else
		extern "C++" {
			template <typename _CountofType, size_t _SizeOfArray>
			char (*__countof_helper(UNALIGNED _CountofType (&_Array)[_SizeOfArray]))[_SizeOfArray];
#		define _countof(_Array) sizeof(*__countof_helper(_Array))
		}
#	endif
#endif

