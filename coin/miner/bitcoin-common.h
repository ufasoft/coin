/*######     Copyright (c) 1997-2013 Ufasoft  http://ufasoft.com  mailto:support@ufasoft.com,  Sergey Pavlov  mailto:dev@ufasoft.com #######################################
#                                                                                                                                                                          #
# This program is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation;  #
# either version 3, or (at your option) any later version. This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the      #
# implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details. You should have received a copy of the GNU #
# General Public License along with this program; If not, see <http://www.gnu.org/licenses/>                                                                               #
##########################################################################################################################################################################*/

#pragma once

#ifndef UCFG_BITCOIN_ASM
#	define UCFG_BITCOIN_ASM UCFG_USE_MASM
#endif

#ifndef UCFG_BITCOIN_ASM_X86
#	ifdef _M_IX86
#		define UCFG_BITCOIN_ASM_X86 UCFG_BITCOIN_ASM
#	else
#		define UCFG_BITCOIN_ASM_X86 0
#	endif
#endif

#ifndef UCFG_BITCOIN_TRACE
#	define UCFG_BITCOIN_TRACE 0
#endif

#ifndef UCFG_BITCOIN_USE_CAL
#	define UCFG_BITCOIN_USE_CAL 0
#endif

#ifndef UCFG_BITCOIN_USE_CUDA
#	define UCFG_BITCOIN_USE_CUDA 0
#endif

#ifndef UCFG_BITCOIN_THERMAL_CONTROL
#	define UCFG_BITCOIN_THERMAL_CONTROL 1
#endif

#ifndef UCFG_BITCOIN_USERAGENT_INFO
#	define UCFG_BITCOIN_USERAGENT_INFO 1
#endif

#ifndef UCFG_BITCOIN_LONG_POLLING 
#	define UCFG_BITCOIN_LONG_POLLING 1
#endif

#ifndef UCFG_BITCOIN_GPU_VECTOR
#	define UCFG_BITCOIN_GPU_VECTOR 1
#endif

#ifndef UCFG_BITCOIN_WAY
#	define UCFG_BITCOIN_WAY 4
#endif

#ifndef UCFG_BITCOIN_NPAR
#	define UCFG_BITCOIN_NPAR (32*UCFG_BITCOIN_WAY)
#endif


#if UCFG_LIB_DECLS
#	ifdef _MINER
#		define MINER_CLASS       AFX_CLASS_EXPORT
#	else
#		define MINER_CLASS       AFX_CLASS_IMPORT
#	endif
#else
#	define MINER_CLASS
#endif

#ifndef UCFG_COIN_PRIME
#	define UCFG_COIN_PRIME 0
#endif

#ifndef UCFG_BITCOIN_SOLIDCOIN
#	define UCFG_BITCOIN_SOLIDCOIN 1
#endif

#ifndef UCFG_BITCOIN_USE_RESOURCE
#	define UCFG_BITCOIN_USE_RESOURCE 0
#endif

#define UCFG_BITCOIN_USE_CRASHDUMP 0

#ifndef UCFG_BITCOIN_USE_CRASHDUMP
#	if UCFG_WIN32 && UCFG_DEBUG //&& !defined(_AFXDLL) && !defined(_M_X64)
#		define UCFG_BITCOIN_USE_CRASHDUMP 1
#	else
#		define UCFG_BITCOIN_USE_CRASHDUMP 0
#	endif
#endif

#ifndef UCFG_COIN_MINER_SERVICE
#	define UCFG_COIN_MINER_SERVICE 0
#endif

