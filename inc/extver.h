/*######     Copyright (c) 1997-2013 Ufasoft  http://ufasoft.com  mailto:support@ufasoft.com,  Sergey Pavlov  mailto:dev@ufasoft.com #######################################
#                                                                                                                                                                          #
# This program is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation;  #
# either version 3, or (at your option) any later version. This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the      #
# implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details. You should have received a copy of the GNU #
# General Public License along with this program; If not, see <http://www.gnu.org/licenses/>                                                                               #
##########################################################################################################################################################################*/

#if !defined(VER_H) && UCFG_WIN32
#	ifdef RC_INVOKED
#		include <winver.h>
#	else
#		define RC_INVOKED
#			include <winver.h>
#		undef RC_INVOKED	
#	endif	
#	undef VER_H
#endif	



//!!!R #define NONLS

//#define _SLIST_HEADER_

//!!!typedef void *PSLIST_ENTRY;
//!!!typedef void *PSLIST_HEADER;
//!!!typedef void *SLIST_HEADER;


#include <el/inc/inc_configs.h>

#include <manufacturer.h>



#ifdef _POST_CONFIG
	__BEGIN_DECLS
			WINBASEAPI LCID WINAPI GetUserDefaultLCID(void);
	__END_DECLS
#endif


#define VER_FILEFLAGSMASK			VS_FFI_FILEFLAGSMASK
#define VER_FILEFLAGS				0

#ifdef WIN32
	#define VER_FILEOS          		VOS__WINDOWS32
	#ifdef _USRDLL
		#define VER_FILETYPE VFT_DLL
	#else
		#define VER_FILETYPE VFT_APP
	#endif
	#define VER_FILESUBTYPE	VFT2_UNKNOWN
#else
	#define VER_FILEOS          		VOS_NT_WINDOWS32
	#define VER_FILETYPE				VFT_DRV
	#define VER_FILESUBTYPE	VFT2_DRV_SYSTEM
#endif

#ifndef VER_COMPANYNAME_STR
	#define VER_COMPANYNAME_STR MANUFACTURER
#endif	

#ifndef VER_LEGALCOPYRIGHT_YEARS
#	define VER_LEGALCOPYRIGHT_YEARS "1997-2013"
#endif

#ifndef VER_LEGALCOPYRIGHT_STR
#	define VER_LEGALCOPYRIGHT_STR "Copyright (c) " VER_LEGALCOPYRIGHT_YEARS " " VER_COMPANYNAME_STR
#endif


#ifndef VER_PRODUCTNAME_STR
	#define VER_PRODUCTNAME_STR "ExtLIBs"
#endif





