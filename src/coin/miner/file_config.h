/*######     Copyright (c) 1997-2013 Ufasoft  http://ufasoft.com  mailto:support@ufasoft.com,  Sergey Pavlov  mailto:dev@ufasoft.com #######################################
#                                                                                                                                                                          #
# This program is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation;  #
# either version 3, or (at your option) any later version. This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the      #
# implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details. You should have received a copy of the GNU #
# General Public License along with this program; If not, see <http://www.gnu.org/licenses/>                                                                               #
##########################################################################################################################################################################*/

#ifndef _AFXDLL
#	ifdef _WIN32
#		define UCFG_COM 1
#		define UCFG_OLE 1
#	endif

#	define UCFG_GUI 0
#	define UCFG_EXTENDED 0
#	define UCFG_XML 0
#	define UCFG_WND 0
#	define UCFG_STLSOFT 0
#	define UCFG_USE_PCRE 0
#	define UCFG_LIB_DECLS 0
#endif

#if !defined(UCFG_CUSTOM) || !UCFG_CUSTOM
#	define CUDA_FORCE_API_VERSION 3010		//!!!
#endif

#define UCFG_COIN_LIB_DECLS 0

#ifndef _WIN32
#	define UCFG_BITCOIN_THERMAL_CONTROL 0
#	define UCFG_BITCOIN_SOLO_MINING 0
#	define UCFG_BITCOIN_SOLIDCOIN 0
#endif

#define UCFG_BITCOIN_TRACE 1

#ifdef _DEBUG
#	define UCFG_BITCOIN_USERAGENT_INFO 1 //!!!D
#else
#	define UCFG_BITCOIN_USERAGENT_INFO 0
#endif

#define VER_FILEDESCRIPTION_STR "coin-miner"
#define VER_INTERNALNAME_STR "coin-miner"
#define VER_ORIGINALFILENAME_STR "coin-miner.exe"

#define VER_PRODUCTNAME_STR "xCoin Miner"

#define UCFG_CRASHDUMP_COMPRESS 0

#if defined(_AFXDLL) || !defined(_WIN32)
#	define UCFG_BITCOIN_USE_RESOURCE 0
#else
#	define UCFG_BITCOIN_USE_RESOURCE 1
#endif



