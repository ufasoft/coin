/*######     Copyright (c) 1997-2013 Ufasoft  http://ufasoft.com  mailto:support@ufasoft.com,  Sergey Pavlov  mailto:dev@ufasoft.com #######################################
#                                                                                                                                                                          #
# This program is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation;  #
# either version 3, or (at your option) any later version. This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the      #
# implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details. You should have received a copy of the GNU #
# General Public License along with this program; If not, see <http://www.gnu.org/licenses/>                                                                               #
##########################################################################################################################################################################*/

#pragma once



//!!! #if UCFG_EXTENDED
#include <el/inc/inc_configs.h>
//!!! #endif

#ifdef HAVE_CONFIG_H
#	include <config.h>
#endif

#ifdef _WIN32
#	include <u-config.h>
#endif

#include "default-defs.h"

//!!!R #ifndef VER_EXT_URL
//!!! #	define VER_EXT_URL ""
//!!! #endif

#ifndef UCFG_MINISTL
#	define UCFG_MINISTL 0
#endif

#ifndef UCFG_FRAMEWORK
#	define UCFG_FRAMEWORK (!UCFG_MINISTL)
#endif

#ifndef UCFG_STL
#	define UCFG_STL (!UCFG_MINISTL)
#endif

#ifndef UCFG_STDSTL
#	if !defined(WDM_DRIVER) && (!defined(_MSC_VER) || UCFG_WCE || UCFG_MINISTL)
#		define UCFG_STDSTL 1
#else
#		define UCFG_STDSTL 0		
#	endif
#endif

#ifndef UCFG_USE_STDEXCEPT
#	define UCFG_USE_STDEXCEPT (UCFG_FRAMEWORK && !UCFG_WDM)
#endif

#ifndef _M_IX86
#	define UCFG_OS_IMPTLS 0
#endif

#ifndef UCFG_OS_IMPTLS
#	define UCFG_OS_IMPTLS (!UCFG_STDSTL && UCFG_EXTENDED)
#endif

#ifndef UCFG_USE_DECLSPEC_THREAD
#	define UCFG_USE_DECLSPEC_THREAD UCFG_OS_IMPTLS
#endif

#if !defined(__STDC_WANT_SECURE_LIB__) && !UCFG_STDSTL
#	define __STDC_WANT_SECURE_LIB__ 0
#endif



#ifndef UCFG_USE_OLD_MSVCRTDLL
#	if !UCFG_STDSTL && defined(WIN32) && defined(_MSC_VER) && (_MSC_VER >= 1400) && !defined(_WIN64) && !UCFG_MINISTL && !UCFG_WCE
#		define UCFG_USE_OLD_MSVCRTDLL 1
#	else
#		define UCFG_USE_OLD_MSVCRTDLL 0
#	endif
#endif

#ifndef UCFG_USE_DRIVER_WORKS
#	define UCFG_USE_DRIVER_WORKS 0
#endif


#ifndef UCFG_COPY_PROT
#	define UCFG_COPY_PROT 1
#endif

#if !UCFG_STDSTL
#	define _CRT_NOFORCE_MANIFEST
#	define _STL_NOFORCE_MANIFEST
#	define _CRTIMP2_PURE
#endif



#define _SECURE_COMPILER_COM 0

#define _CRT_INSECURE_NO_DEPRECATE  //VS8

#ifndef _CRT_NONSTDC_NO_WARNINGS
#	define _CRT_NONSTDC_NO_WARNINGS
#endif

#define _HAS_STRICT_CONFORMANCE 1

#ifndef _HAS_ITERATOR_DEBUGGING
#	define _HAS_ITERATOR_DEBUGGING	0
#endif

#define _WINSOCKAPI_

#define _CRT_WCTYPE_NOINLINE

#ifndef UCFG_SECURE_SCL
#	define UCFG_SECURE_SCL 0
#endif

#if UCFG_SECURE_SCL != 2
#	define _SECURE_SCL UCFG_SECURE_SCL
#endif

#define _CRT_SECURE_NO_DEPRECATE 1
#define _CRT_NON_CONFORMING_SWPRINTFS


#define _ATL_DLL_IMPL


#ifndef UCFG_INTRINSIC_MEMFUN
#	define UCFG_INTRINSIC_MEMFUN 1
#endif

#ifndef UCFG_ATL_EMULATION
#	define UCFG_ATL_EMULATION 1
#endif

#ifndef UCFG_COMCTL_MANIFEST
#	define UCFG_COMCTL_MANIFEST "/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='x86' publicKeyToken='6595b64144ccf1df' language='*'\"" 
#endif	


#ifndef UCFG_TRC
#	ifdef _DEBUG
#		define UCFG_TRC 1
#	else
#		define UCFG_TRC 0
#	endif	
#endif	


#define _USE_ATTRIBUTES_FOR_SAL 0

#ifndef UCFG_CRASH_DUMP
#	if defined(_AFXDLL) && !UCFG_WCE
#		define UCFG_CRASH_DUMP 1				// 1 - use simple embedded CrashDump, 2 - use CrashRpt
#	else
#		define UCFG_CRASH_DUMP 0
#	endif
#endif

#ifndef UCFG_USE_POSIX
#	ifdef __unix__
#		define UCFG_USE_POSIX 1
#	else
#		define UCFG_USE_POSIX 0
#	endif	
#endif

#ifndef UCFG_COMPLEX_WINAPP
#	define UCFG_COMPLEX_WINAPP (!UCFG_USE_POSIX)
#endif

#ifndef UCFG_GUI
#	define UCFG_GUI (!UCFG_USE_POSIX)
#endif

#ifndef UCFG_UPGRADE
#	define UCFG_UPGRADE UCFG_GUI 
#endif


#if defined(_MSC_VER) && UCFG_USE_POSIX
#	define __GNUC__ 0
#endif

#ifndef UCFG_STLSOFT
#	if !UCFG_WCE && !defined(_CRTBLD) && !UCFG_MINISTL && !(UCFG_MSC_VERSION >= 1600)
#		define UCFG_STLSOFT 0				//!!!
#	else
#		define UCFG_STLSOFT 0
#	endif
#endif


#ifndef UCFG_USE_BOOST
#	if !UCFG_WCE && !defined(_CRTBLD) && !UCFG_MINISTL && !UCFG_WDM && UCFG_STDSTL && !UCFG_USE_POSIX
#		define UCFG_USE_BOOST 0	//!!!? 1
#	else	
#		define UCFG_USE_BOOST 0
#	endif
#endif

#ifndef UCFG_USE_TR1
#	if !UCFG_WCE && !defined(_CRTBLD) && !UCFG_MINISTL
#		define UCFG_USE_TR1 1
#	else	
#		define UCFG_USE_TR1 0
#	endif
#endif

#ifndef UCFG_USE_PTHREADS
#	define UCFG_USE_PTHREADS UCFG_USE_POSIX
#endif

#ifndef UCFG_FULL
#	define UCFG_FULL 1
#endif

#ifndef UCFG_USE_LIBXML
#	define UCFG_USE_LIBXML (!UCFG_WCE && UCFG_FULL)
#endif

#ifndef UCFG_USE_LIBCURL
#	define UCFG_USE_LIBCURL UCFG_USE_POSIX
#endif

#ifndef UCFG_BIGNUM
#	if UCFG_WDM || UCFG_WIN32_FULL || !defined(HAVE_LIBGMP)
#		define UCFG_BIGNUM 'A'
#	else
#		define UCFG_BIGNUM 'G'
#	endif
#endif

#ifndef UCFG_ADDITIONAL_HEAPS
#	define UCFG_ADDITIONAL_HEAPS 0
#endif

#ifndef UCFG_EXTENDED
#	ifdef WDM_DRIVER
#		define UCFG_EXTENDED 0
#	else
#		define UCFG_EXTENDED (!UCFG_USE_POSIX)
#	endif
#endif

#ifndef UCFG_WIN_MSG
#	define UCFG_WIN_MSG (UCFG_EXTENDED && UCFG_WIN32)
#endif

#ifndef UCFG_USE_RESOURCES
#	define UCFG_USE_RESOURCES (UCFG_EXTENDED && UCFG_WIN32)
#endif

#ifndef UCFG_USE_RTTI
#	define UCFG_USE_RTTI (!UCFG_EXTENDED)
#endif

//#if !UCFG_USE_RTTI && defined _CPPRTTI
//#	error ExtLib requires /GR- compiler option
//#endif


#ifndef UCFG_DELAYLOAD_THROW
#	define UCFG_DELAYLOAD_THROW (UCFG_EXTENDED && !UCFG_WCE)
#endif

#ifndef UCFG_EHS_EHC_MINUS
#	define UCFG_EHS_EHC_MINUS UCFG_DELAYLOAD_THROW
#endif

#ifndef UCFG_WIN_HEADERS
#	define UCFG_WIN_HEADERS 0
#endif

#ifndef UCFG_COM
#	define UCFG_COM (UCFG_WIN32 && UCFG_FULL)
#endif

#ifndef UCFG_OLE
#	define UCFG_OLE (UCFG_COM && UCFG_EXTENDED)
#endif

#ifndef UCFG_OCC
#	define UCFG_OCC (!UCFG_WCE && UCFG_OLE)
#endif	

#ifndef UCFG_COM_IMPLOBJ
#	define UCFG_COM_IMPLOBJ (!UCFG_WCE && UCFG_OLE)
#endif	

#ifndef UCFG_XML
#	define UCFG_XML UCFG_FULL
#endif

#ifndef UCFG_CODEPAGE_UTF8
#	define UCFG_CODEPAGE_UTF8 (!UCFG_WDM && (UCFG_UNICODE || !UCFG_WIN32))
#endif


#ifndef UCFG_USE_REGEX
#	define UCFG_USE_REGEX (UCFG_FULL && !UCFG_WDM)
#endif

#ifndef UCFG_USE_PCRE
#	define UCFG_USE_PCRE (UCFG_USE_REGEX && (!UCFG_STDSTL || !UCFG_CPP11))
#endif

#ifndef UCFG_USELISP
#	define UCFG_USELISP (!UCFG_USE_POSIX)
#endif

#ifndef UCFG_LIB_DECLS
#	ifdef _WIN32
#		define UCFG_LIB_DECLS 1
#	else
#		define UCFG_LIB_DECLS 0
#	endif
#endif


#ifndef UCFG_POOL_TAG
#	define UCFG_POOL_TAG ' txE'
#endif


#if UCFG_GNUC
#	define _HAS_TR1
#endif

#if defined(_HAS_TR1) && defined(_MSC_VER)
#	define _HAS_TR1_NS
#endif


//#ifndef __cplusplus
#ifndef __STDC__
#	define __STDC__ 0
#endif
//#endif

#define _BSD_SOURCE 1

#ifdef _WIN32
//!!!#	define ZLIB_WINAPI
#endif

#ifndef UCFG_USING_DEFAULT_NS
#	if defined (_EXT)
#		define UCFG_USING_DEFAULT_NS 0
#	else
#		define UCFG_USING_DEFAULT_NS 1
#	endif
#endif

#ifndef UCFG_EH_SUPPORT_IGNORE
#	if defined(_WIN32) && UCFG_DEBUG && !UCFG_WDM && !defined(_WIN64)
#		define UCFG_EH_SUPPORT_IGNORE 1
#	else
#		define UCFG_EH_SUPPORT_IGNORE 0
#	endif
#endif

#ifndef UCFG_FORCE_LINK
#	define UCFG_FORCE_LINK 1
#endif

#ifndef UCFG_CATCH_UNHANDLED_EXC
#	define UCFG_CATCH_UNHANDLED_EXC 1
#endif


#ifndef UCFG_WND
#	define UCFG_WND UCFG_WIN32
#endif

#ifndef UCFG_USE_SIMPLE_MAPI
#	define UCFG_USE_SIMPLE_MAPI !UCFG_USE_POSIX
#endif

#ifndef UCFG_DEFINE_NDEBUG
#	define UCFG_DEFINE_NDEBUG (!UCFG_DEBUG)
#endif

#if !defined(NDEBUG) && UCFG_DEFINE_NDEBUG
#	define NDEBUG
#endif

#ifndef _AFXDLL
#	define UCFG_ALLOCATOR UCFG_ALLOCATOR_STD_MALLOC
#endif

#ifndef UCFG_ALLOCATOR
#	if UCFG_STDSTL || defined(_WIN64) || UCFG_WCE
#		define UCFG_ALLOCATOR UCFG_ALLOCATOR_STD_MALLOC
#	else
#		define UCFG_ALLOCATOR UCFG_ALLOCATOR_TC_MALLOC
#	endif
#endif

#ifndef UCFG_ARGV_UNICODE
#	define UCFG_ARGV_UNICODE 0
#endif

#ifndef UCFG_REDEFINE_MAIN
#	define UCFG_REDEFINE_MAIN UCFG_WIN32
#endif

#ifndef UCFG_HASH_USE_DIV
#	if defined _M_ARM
#		define UCFG_HASH_USE_DIV 0
#	else
#		define UCFG_HASH_USE_DIV 1
#	endif
#endif

#ifndef UCFG_BLOB_POLYMORPHIC
#	define UCFG_BLOB_POLYMORPHIC 0
#endif

#ifndef UCFG_STRING_CHAR
#	define UCFG_STRING_CHAR 16
#endif

#ifndef UCFG_TRACE_EH
#	define UCFG_TRACE_EH 0
#endif

#ifndef UCFG_STAT64
#	define UCFG_STAT64 (!UCFG_STDSTL)
#endif

#ifndef UCFG_DEFINE_NEW
#	define UCFG_DEFINE_NEW !(UCFG_STDSTL)
#endif

#define UCFG_HAVE_STRERROR (!UCFG_WCE)

#if UCFG_HAVE_STRERROR
#	define HAVE_STRERROR 1
#endif

#ifndef UCFG_INDIRECT_MALLOC
#	define UCFG_INDIRECT_MALLOC UCFG_WCE
#endif

#ifndef UCFG_SIMPLE_MACROS
#	define UCFG_SIMPLE_MACROS 0
#endif

#ifndef UCFG_SIMPLE_MACROS_SELF
#	define UCFG_SIMPLE_MACROS_SELF UCFG_FULL
#endif

#if !defined(_AFXDLL) && !UCFG_STDSTL
#	define _CRTIMP_PURE			//!!!
#endif

#ifndef UCFG_DETECT_MISMATCH
#	if defined(_MSC_VER) && _MSC_VER>=1600
#		define UCFG_DETECT_MISMATCH 1
#	else
#		define UCFG_DETECT_MISMATCH 0
#	endif
#endif

#if defined(_MSC_VER) && !UCFG_WDM && (UCFG_USE_POSIX || UCFG_EXTENDED) && !UCFG_STDSTL
#	define HAVE_SYS_TIME_H 1
#endif

#ifndef UCFG_HEAP_CHECK
#	define UCFG_HEAP_CHECK 0
#endif

#if UCFG_WDM
#	define	UCFG_HAS_REALLOC 0
#else
#	define	UCFG_HAS_REALLOC 1
#endif

#ifndef UCFG_THREAD_STACK_SIZE
#	define UCFG_THREAD_STACK_SIZE 65536				// not enough under profiler, can be changed by UCFG_THREAD_STACK_SIZE environment variable
#endif

#define UCFG_JSON_JANSSON 1
#define UCFG_JSON_JSONCPP 2

#ifndef UCFG_JSON
#	ifdef HAVE_JANSSON
#		define UCFG_JSON UCFG_JSON_JANSSON
#	else
#		define UCFG_JSON 0
#	endif
#endif

#ifndef UCFG_SPECIAL_CRT
#	define UCFG_SPECIAL_CRT 0
#endif


