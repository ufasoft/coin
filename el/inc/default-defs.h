/*######     Copyright (c) 1997-2013 Ufasoft  http://ufasoft.com  mailto:support@ufasoft.com,  Sergey Pavlov  mailto:dev@ufasoft.com #######################################
#                                                                                                                                                                          #
# This program is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation;  #
# either version 3, or (at your option) any later version. This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the      #
# implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details. You should have received a copy of the GNU #
# General Public License along with this program; If not, see <http://www.gnu.org/licenses/>                                                                               #
##########################################################################################################################################################################*/

#pragma once

#if !defined(DEBUG) && defined(_DEBUG)
#	define DEBUG 1
#endif

#ifdef _DEBUG
#	define UCFG_DEBUG 1
#else
#	define UCFG_DEBUG 0
#endif

#ifdef _PROFILE
#	define UCFG_PROFILE 1
#else
#	define UCFG_PROFILE 0
#endif

#ifndef _WIN32_IE
#	define _WIN32_IE 0x500
#endif

#ifdef _WIN32
#	define  NTDDI_WINLH NTDDI_VISTA				//!!! not defined in current SDK
#endif

#ifndef _MSC_VER
#	ifdef __x86_64__
#		define _M_X64
#	elif defined(__i786__)
#		define _M_IX86 700
#	elif defined(__i686__)
#		define _M_IX86 600
#	elif defined(__i586__)
#		define _M_IX86 500
#	elif defined(__i486__)
#		define _M_IX86 400
#	elif defined(__i386__)
#		define _M_IX86 300
#	elif defined(__arm__)
#		define _M_ARM
#	endif	

#	ifdef __SSE2__
#		define _M_IX86_FP 2
#	endif
#endif  // ndef _MSC_VER

#if defined(__GNUC__) && __GNUC__ > 0 && !defined(__clang__)
#	define UCFG_GNUC_VERSION (__GNUC__ * 100 + __GNUC_MINOR__)
#else
#	define UCFG_GNUC_VERSION 0
#endif

#ifdef _MSC_VER
#	define UCFG_MSC_VERSION _MSC_VER
#else
#	define UCFG_MSC_VERSION 0
#endif

#if !defined(_MAC) && (defined(_M_M68K) || defined(_M_MPPC))
#	define _MAC
#endif

#ifdef _M_IX86
#	define UCFG_PLATFORM_IX86 1
#else
#	define UCFG_PLATFORM_IX86 0
#endif

#ifdef _M_X64
#	define UCFG_PLATFORM_X64 1
#else
#	define UCFG_PLATFORM_X64 0
#endif

#define UCFG_CPU_X86_X64 (UCFG_PLATFORM_IX86 || UCFG_PLATFORM_X64)

#define UCFG_PLATFORM_64 UCFG_PLATFORM_X64

#ifndef UCFG_USE_MASM
#	define UCFG_USE_MASM UCFG_CPU_X86_X64
#endif


#ifdef _M_ARM
#	ifndef ARM  //!!!
#		define ARM //!!!
#	endif

#	ifndef _ARM_  //!!!
#		define _ARM_  //!!!
#	endif

#	ifdef _MSC_VER
#		if _MSC_VER < 1600
#			ifndef _WIN32_WCE
#				define _WIN32_WCE 0x420
#			endif
#		else
#			define _CRT_BUILD_DESKTOP_APP 0
#		endif
#	endif
#endif


#ifdef _WIN32_WCE
#	ifndef WINCEOSVER
#		define WINCEOSVER _WIN32_WCE
#	endif


//	#ifndef _CE_ALLOW_SINGLE_THREADED_OBJECTS_IN_MTA
//		#define _CE_ALLOW_SINGLE_THREADED_OBJECTS_IN_MTA
//	#endif
#	define UCFG_WCE 1
#else
#	define UCFG_WCE 0
#endif

#if !defined(_WIN32_WINNT) && !UCFG_WCE
#	define _WIN32_WINNT 0x601
#endif

#define NTDDI_WIN7SP1 0x06010001 //!!!?

#ifndef NTDDI_VERSION
#	define NTDDI_VERSION        0x06020000		//  NTDDI_WIN8
#endif

#define _APISET_MINWIN_VERSION 0x200

#define PSAPI_VERSION 1							// to use psapi.dll instead of new kernel32 replacement


#ifndef __SPECSTRINGS_STRICT_LEVEL //!!!
#	define __SPECSTRINGS_STRICT_LEVEL 0
#endif	


#ifdef _M_IX86
#	define _X86_
#endif

#ifdef _M_AMD64
#	define _AMD64_
#endif

#ifdef _UNICODE
#	define UCFG_UNICODE 1
#else
#	define UCFG_UNICODE 0
#endif

#if UCFG_WCE
#	ifndef _UNICODE
#		error ExtLib/CE requires _UNICODE defined
#	endif

#	ifndef UNDER_CE
#		define UNDER_CE _WIN32_WCE
#	endif

#	ifndef WIN32
#		define WIN32
#	endif
	
#	define _CRTAPI1
#	define _CRTAPI2

#endif

#ifndef WDM_DRIVER
#	ifdef NDIS_WDM
#		define WDM_DRIVER
#	endif                    
#endif

#ifdef WDM_DRIVER
#	define UCFG_WDM 1
#else
#	define UCFG_WDM 0
#endif

#ifndef UCFG_EXT_C
#	define UCFG_EXT_C UCFG_WDM
#endif

#if !defined(WIN32) && defined(_WIN32) && !UCFG_WDM && !UCFG_EXT_C
#	define WIN32
#endif

#ifndef UCFG_WIN32
#	ifdef WIN32
#		define UCFG_WIN32 1
#	else
#		define UCFG_WIN32 0
#	endif
#endif

#define UCFG_WIN32_FULL (UCFG_WIN32 && !UCFG_WCE)

#ifndef WINVER
#	define WINVER 0x0600 //!!! 0x400
#endif

#ifndef UCFG_USE_WINDEFS
#	define UCFG_USE_WINDEFS UCFG_WIN32
#endif

#ifndef UCFG_CPP17
#	if defined(__cplusplus) && __cplusplus >= 201700 || (UCFG_MSC_VERSION >= 1900) || (UCFG_GNUC_VERSION >= 411)	//!!!?
#		define UCFG_CPP17 1
#	else
#		define UCFG_CPP17 0
#	endif
#endif

#ifndef UCFG_CPP14
#	if defined(__cplusplus) && __cplusplus >= 201400 || (UCFG_MSC_VERSION >= 1800) || (UCFG_GNUC_VERSION >= 410)	//!!!?
#		define UCFG_CPP14 1
#	else
#		define UCFG_CPP14 UCFG_CPP17
#	endif
#endif

#ifndef UCFG_CPP11
#	if defined(__cplusplus) && __cplusplus > 199711 || defined(__GXX_EXPERIMENTAL_CXX0X__) || (UCFG_MSC_VERSION >= 1600) || UCFG_WCE
#		define UCFG_CPP11 1
#	else
#		define UCFG_CPP11 UCFG_CPP14
#	endif
#endif

#ifndef UCFG_CPP11_ENUM
#	if defined(_MSC_VER) && _MSC_VER<1700
#		define UCFG_CPP11_ENUM 0
#	else
#		define UCFG_CPP11_ENUM UCFG_CPP11
#	endif
#endif

#ifndef UCFG_CPP11_EXPLICIT_CAST
#	if defined(_MSC_VER) && _MSC_VER<=1700 || defined(__GXX_EXPERIMENTAL_CXX0X__) && __GNUC__ == 4 && __GNUC_MINOR__ < 5
#		define UCFG_CPP11_EXPLICIT_CAST 0
#	else
#		define UCFG_CPP11_EXPLICIT_CAST UCFG_CPP11
#	endif
#endif

#ifndef UCFG_CPP11_NULLPTR
#	if defined(_NATIVE_NULLPTR_SUPPORTED)
#		define UCFG_CPP11_NULLPTR 1
#	elif defined(_MSC_VER) && _MSC_VER<1600
#		define UCFG_CPP11_NULLPTR 0
#	elif defined(__GXX_EXPERIMENTAL_CXX0X__) && !defined(__clang__)
#		if __GNUC__ == 4 && __GNUC_MINOR__ <= 5
#			define UCFG_CPP11_NULLPTR 0
#		else
#			define UCFG_CPP11_NULLPTR 1
#		endif
#	else
#		define UCFG_CPP11_NULLPTR UCFG_CPP11
#	endif
#endif

#ifndef UCFG_CPP11_FOR_EACH
#	define UCFG_CPP11_FOR_EACH UCFG_CPP11
#endif

#ifndef UCFG_CPP11_RVALUE
#	if	defined(_MSC_VER) && _MSC_VER<1600
#		define UCFG_CPP11_RVALUE 0
#	else
#		define UCFG_CPP11_RVALUE UCFG_CPP11
#	endif
#endif

#ifndef UCFG_CPP11_OVERRIDE
#	define UCFG_CPP11_OVERRIDE (UCFG_GNUC_VERSION >= 407 || UCFG_MSC_VERSION >= 1600 || (!UCFG_GNUC_VERSION && !UCFG_MSC_VERSION && UCFG_CPP11))
#endif

#ifndef UCFG_CPP11_THREAD_LOCAL
#	if (defined(_MSC_VER) && _MSC_VER<=1800) || (defined(__GXX_EXPERIMENTAL_CXX0X__) && __GNUC__ == 4 && __GNUC_MINOR__ <= 6)
#		define UCFG_CPP11_THREAD_LOCAL 0
#	else
#		define UCFG_CPP11_THREAD_LOCAL UCFG_CPP11
#	endif
#endif

#ifndef UCFG_CPP11_HAVE_MUTEX
#	if (!defined(_MSC_VER) || _MSC_VER>=1700) && (!defined(__clang_major__) || __clang_major__>3 || __clang_major__==3 && __clang_minor__>0)
#		define UCFG_CPP11_HAVE_MUTEX 1
#	else
#		define UCFG_CPP11_HAVE_MUTEX 0
#	endif
#endif

#ifndef UCFG_STD_SHARED_MUTEX
#	define UCFG_STD_SHARED_MUTEX UCFG_CPP14
#endif

#ifndef UCFG_STD_DYNAMIC_BITSET
#	define UCFG_STD_DYNAMIC_BITSET UCFG_CPP14
#endif

#ifndef UCFG_CPP11_HAVE_REGEX
#	if defined(__GXX_EXPERIMENTAL_CXX0X__) && __GNUC__ == 4 && __GNUC_MINOR__ <= 8
#		define UCFG_CPP11_HAVE_REGEX 0
#	else
#		define UCFG_CPP11_HAVE_REGEX 1
#	endif
#endif

#ifndef UCFG_CPP11_HAVE_THREAD
#	if (!defined(_MSC_VER) || _MSC_VER>=1700) && (!defined(__clang_major__) || __clang_major__>3 || __clang_major__==3 && __clang_minor__>0)
#		define UCFG_CPP11_HAVE_THREAD 1
#	else
#		define UCFG_CPP11_HAVE_THREAD 0
#	endif
#endif

#ifndef UCFG_CPP11_HAVE_DECIMAL
#	define UCFG_CPP11_HAVE_DECIMAL (UCFG_GNUC_VERSION >= 409 || UCFG_MSC_VERSION >= 1900)
#endif

#ifndef UCFG_CPP11_BEGIN
#	define UCFG_CPP11_BEGIN (UCFG_GNUC_VERSION >= 406 || UCFG_MSC_VERSION >= 1600)
#endif

#ifndef UCFG_STD_EXCHANGE
#	define UCFG_STD_EXCHANGE ((UCFG_MSC_VERSION && UCFG_MSC_VERSION > 1800) || (!UCFG_MSC_VERSION && UCFG_CPP14))
#endif

#define WIN9X_COMPAT_SPINLOCK

#ifdef _CPPUNWIND
#	define _HAS_EXCEPTIONS 1
#else
#	define _HAS_EXCEPTIONS 0
#endif

#define _ATL_NO_AUTOMATIC_NAMESPACE

#define _ATL_ALLOW_CHAR_UNSIGNED  // to prevent ATL error in VS11

/*!!!R
#ifndef _MSC_VER

#	ifdef __CHAR_UNSIGNED__
#		define _CHAR_UNSIGNED
#	endif

#endif
*/

#define _HAS_TRADITIONAL_STL 1


#ifndef _FILE_OFFSET_BITS
#	define _FILE_OFFSET_BITS 64
#endif


#ifndef _STRALIGN_USE_SECURE_CRT
#	define _STRALIGN_USE_SECURE_CRT 0
#endif

#define UCFG_ALLOCATOR_STD_MALLOC 1
#define UCFG_ALLOCATOR_TC_MALLOC 2

#ifndef UCFG_POOL_THREADS
#	define UCFG_POOL_THREADS 25
#endif


#ifndef UCFG_EH_OS_UNWIND
#	define UCFG_EH_OS_UNWIND (defined _M_X64)
#endif

#define UCFG_EH_PDATA (!defined _M_IX86)

#ifndef UCFG_EH_DIRECT_CATCH
#	define UCFG_EH_DIRECT_CATCH (!UCFG_WCE)
#endif

#if !defined(_MSC_VER) && defined(__GNUC__)
#	define UCFG_GNUC 1
#else
#	define UCFG_GNUC 0
#endif

#if UCFG_WDM && (UCFG_CPU_X86_X64 || defined(_M_ARM))	//!!!?
#	define UCFG_FPU 0
#else
#	define UCFG_FPU 1
#endif

#ifdef _DEBUG
#	define DBG 1
#	define DEBUG 1
#else
#	define DBG 0
#endif

#define _COMPLEX_DEFINED

#ifdef _MSC_VER
#	define HAVE_SNPRINTF 1
#	define HAVE_STRLCPY
#	define HAVE_STRDUP 1
#	define HAVE_VSNPRINTF 1
#	define HAVE_MEMMOVE  1
#	define HAVE_FCNTL_H 1
#	define HAVE_STRSEP
#	define HAVE_SOCKADDR_STORAGE
#	define HAVE_SQLITE3 1
#	define HAVE_LIBGMP 1
#	define HAVE_LIBNTL 1
#	define HAVE_BERKELEY_DB 1
#	define HAVE_JANSSON 1
#endif	// _MSC_VER

