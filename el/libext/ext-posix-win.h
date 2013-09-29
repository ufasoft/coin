/*######     Copyright (c) 1997-2013 Ufasoft  http://ufasoft.com  mailto:support@ufasoft.com,  Sergey Pavlov  mailto:dev@ufasoft.com #######################################
#                                                                                                                                                                          #
# This program is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation;  #
# either version 3, or (at your option) any later version. This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the      #
# implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details. You should have received a copy of the GNU #
# General Public License along with this program; If not, see <http://www.gnu.org/licenses/>                                                                               #
##########################################################################################################################################################################*/

#pragma once

// Windows definitions for non-Windows environment


typedef struct sockaddr_in SOCKADDR_IN;

#define FAR

#ifndef EXTERN_C
#	ifdef __cplusplus
#		define EXTERN_C    extern "C"
#	else
#		define EXTERN_C    extern
#	endif
#endif

#ifndef GUID_DEFINED
#	define GUID_DEFINED
typedef struct _GUID {
    unsigned long  Data1;
    unsigned short Data2;
    unsigned short Data3;
    unsigned char  Data4[ 8 ];
} GUID;
#endif



#ifdef DEFINE_GUID
#	undef DEFINE_GUID
#endif

#ifdef INITGUID
#	define DEFINE_GUID(name, l, w1, w2, b1, b2, b3, b4, b5, b6, b7, b8) \
        EXTERN_C const GUID DECLSPEC_SELECTANY name \
                = { l, w1, w2, { b1, b2,  b3,  b4,  b5,  b6,  b7,  b8 } }
#else
#	define DEFINE_GUID(name, l, w1, w2, b1, b2, b3, b4, b5, b6, b7, b8) \
    EXTERN_C const GUID FAR name
#endif // INITGUID




#define SPECSTRINGS_H

#define FACILITY_WIN32                   7
#define FACILITY_URT                     19


#define _WINERROR_

typedef __success(return >= 0) long HRESULT;

#define _HRESULT_TYPEDEF_(_sc) ((HRESULT)_sc)

#define HRESULT_FROM_WIN32(x) ((HRESULT)(x) <= 0 ? ((HRESULT)(x)) : ((HRESULT) (((x) & 0x0000FFFF) | (FACILITY_WIN32 << 16) | 0x80000000)))

#define SEVERITY_SUCCESS    0
#define SEVERITY_ERROR      1

#define MAKE_HRESULT(sev,fac,code) \
    ((HRESULT) (((unsigned long)(sev)<<31) | ((unsigned long)(fac)<<16) | ((unsigned long)(code))) )
#define MAKE_SCODE(sev,fac,code) \
    ((SCODE) (((unsigned long)(sev)<<31) | ((unsigned long)(fac)<<16) | ((unsigned long)(code))) )


#define HRESULT_CODE(hr)    ((hr) & 0xFFFF)
#define SCODE_CODE(sc)      ((sc) & 0xFFFF)

#define HRESULT_FACILITY(hr)  (((hr) >> 16) & 0x1fff)
#define SCODE_FACILITY(sc)    (((sc) >> 16) & 0x1fff)

#define HRESULT_SEVERITY(hr)  (((hr) >> 31) & 0x1)
#define SCODE_SEVERITY(sc)    (((sc) >> 31) & 0x1)


#define E_NOTIMPL                        _HRESULT_TYPEDEF_(0x80004001L)
#define E_OUTOFMEMORY                    _HRESULT_TYPEDEF_(0x8007000EL)
#define E_INVALIDARG                     _HRESULT_TYPEDEF_(0x80070057L)
#define E_FAIL                           _HRESULT_TYPEDEF_(0x80004005L)
#define E_ACCESSDENIED                   _HRESULT_TYPEDEF_(0x80070005L)
#define E_HANDLE                         _HRESULT_TYPEDEF_(0x80070006L)

#define ERROR_FILE_NOT_FOUND             2L
#define ERROR_PATH_NOT_FOUND             3L
#define ERROR_CRC                        23L
#define ERROR_FILE_EXISTS                80L
#define ERROR_INVALID_PASSWORD           86L
#define ERROR_INVALID_PARAMETER          87L
#define ERROR_PROC_NOT_FOUND             127L
#define ERROR_ARITHMETIC_OVERFLOW        534L
#define ERROR_PWD_TOO_SHORT              615L
#define ERROR_STACK_OVERFLOW             1001L
#define ERROR_CANCELLED                  1223L
#define ERROR_WRONG_PASSWORD             1323L
#define ERROR_PASSWORD_RESTRICTION       1325L
#define ERROR_LOGON_FAILURE              1326L
#define ERROR_INVALID_COMMAND_LINE       1639L

#define DNS_ERROR_INVALID_IP_ADDRESS     9552L

#define WSAENOTSOCK                      10038L
#define WSAEADDRINUSE           		 10048L


//!!!R #include <winerror.h>


#define INFINITE            0xFFFFFFFF  // Infinite timeout

#define FILE_BEGIN           0
#define FILE_CURRENT         1
#define FILE_END             2

#define INVALID_SOCKET  (SOCKET)(~0)
#define SOCKET_ERROR            (-1)

#define HTTP_ADDREQ_FLAG_ADD        0x20000000

#define CSIDL_DESKTOP                   0x0000        // <desktop>
#define CSIDL_INTERNET                  0x0001        // Internet Explorer (icon on desktop)
#define CSIDL_PROGRAMS                  0x0002        // Start Menu\Programs
#define CSIDL_CONTROLS                  0x0003        // My Computer\Control Panel
#define CSIDL_PRINTERS                  0x0004        // My Computer\Printers
#define CSIDL_PERSONAL                  0x0005        // My Documents
#define CSIDL_FAVORITES                 0x0006        // <user name>\Favorites
#define CSIDL_STARTUP                   0x0007        // Start Menu\Programs\Startup
#define CSIDL_RECENT                    0x0008        // <user name>\Recent
#define CSIDL_SENDTO                    0x0009        // <user name>\SendTo
#define CSIDL_BITBUCKET                 0x000a        // <desktop>\Recycle Bin
#define CSIDL_STARTMENU                 0x000b        // <user name>\Start Menu
#define CSIDL_MYDOCUMENTS               CSIDL_PERSONAL //  Personal was just a silly name for My Documents
#define CSIDL_MYMUSIC                   0x000d        // "My Music" folder
#define CSIDL_MYVIDEO                   0x000e        // "My Videos" folder
#define CSIDL_DESKTOPDIRECTORY          0x0010        // <user name>\Desktop
#define CSIDL_DRIVES                    0x0011        // My Computer
#define CSIDL_NETWORK                   0x0012        // Network Neighborhood (My Network Places)
#define CSIDL_NETHOOD                   0x0013        // <user name>\nethood
#define CSIDL_FONTS                     0x0014        // windows\fonts
#define CSIDL_TEMPLATES                 0x0015
#define CSIDL_COMMON_STARTMENU          0x0016        // All Users\Start Menu
#define CSIDL_COMMON_PROGRAMS           0X0017        // All Users\Start Menu\Programs
#define CSIDL_COMMON_STARTUP            0x0018        // All Users\Startup
#define CSIDL_COMMON_DESKTOPDIRECTORY   0x0019        // All Users\Desktop
#define CSIDL_APPDATA                   0x001a        // <user name>\Application Data
#define CSIDL_PRINTHOOD                 0x001b        // <user name>\PrintHood

#ifndef CSIDL_LOCAL_APPDATA
#define CSIDL_LOCAL_APPDATA             0x001c        // <user name>\Local Settings\Applicaiton Data (non roaming)
#endif // CSIDL_LOCAL_APPDATA

#define CSIDL_ALTSTARTUP                0x001d        // non localized startup
#define CSIDL_COMMON_ALTSTARTUP         0x001e        // non localized common startup
#define CSIDL_COMMON_FAVORITES          0x001f

#ifndef _SHFOLDER_H_
#define CSIDL_INTERNET_CACHE            0x0020
#define CSIDL_COOKIES                   0x0021
#define CSIDL_HISTORY                   0x0022
#define CSIDL_COMMON_APPDATA            0x0023        // All Users\Application Data
#define CSIDL_WINDOWS                   0x0024        // GetWindowsDirectory()
#define CSIDL_SYSTEM                    0x0025        // GetSystemDirectory()
#define CSIDL_PROGRAM_FILES             0x0026        // C:\Program Files
#define CSIDL_MYPICTURES                0x0027        // C:\Program Files\My Pictures
#endif // _SHFOLDER_H_

#define CSIDL_PROFILE                   0x0028        // USERPROFILE
#define CSIDL_SYSTEMX86                 0x0029        // x86 system directory on RISC
#define CSIDL_PROGRAM_FILESX86          0x002a        // x86 C:\Program Files on RISC

#ifndef _SHFOLDER_H_
#define CSIDL_PROGRAM_FILES_COMMON      0x002b        // C:\Program Files\Common
#endif // _SHFOLDER_H_

#define CSIDL_PROGRAM_FILES_COMMONX86   0x002c        // x86 Program Files\Common on RISC
#define CSIDL_COMMON_TEMPLATES          0x002d        // All Users\Templates

#ifndef _SHFOLDER_H_
#define CSIDL_COMMON_DOCUMENTS          0x002e        // All Users\Documents
#define CSIDL_COMMON_ADMINTOOLS         0x002f        // All Users\Start Menu\Programs\Administrative Tools
#define CSIDL_ADMINTOOLS                0x0030        // <user name>\Start Menu\Programs\Administrative Tools
#endif // _SHFOLDER_H_

#define CSIDL_CONNECTIONS               0x0031        // Network and Dial-up Connections
#define CSIDL_COMMON_MUSIC              0x0035        // All Users\My Music
#define CSIDL_COMMON_PICTURES           0x0036        // All Users\My Pictures
#define CSIDL_COMMON_VIDEO              0x0037        // All Users\My Video
#define CSIDL_RESOURCES                 0x0038        // Resource Direcotry

#ifndef _SHFOLDER_H_
#define CSIDL_RESOURCES_LOCALIZED       0x0039        // Localized Resource Direcotry
#endif // _SHFOLDER_H_

#define CSIDL_COMMON_OEM_LINKS          0x003a        // Links to All Users OEM specific apps
#define CSIDL_CDBURN_AREA               0x003b        // USERPROFILE\Local Settings\Application Data\Microsoft\CD Burning
// unused                               0x003c
#define CSIDL_COMPUTERSNEARME           0x003d        // Computers Near Me (computered from Workgroup membership)

#ifndef _SHFOLDER_H_
#define CSIDL_FLAG_CREATE               0x8000        // combine with CSIDL_ value to force folder creation in SHGetFolderPath()
#endif // _SHFOLDER_H_

#define CSIDL_FLAG_DONT_VERIFY          0x4000        // combine with CSIDL_ value to return an unverified folder path
#define CSIDL_FLAG_DONT_UNEXPAND        0x2000        // combine with CSIDL_ value to avoid unexpanding environment variables
#if (NTDDI_VERSION >= NTDDI_WINXP)
#define CSIDL_FLAG_NO_ALIAS             0x1000        // combine with CSIDL_ value to insure non-alias versions of the pidl
#define CSIDL_FLAG_PER_USER_INIT        0x0800        // combine with CSIDL_ value to indicate per-user init (eg. upgrade)
#endif  // NTDDI_WINXP
#define CSIDL_FLAG_MASK                 0xFF00        // mask for all possible flag values

