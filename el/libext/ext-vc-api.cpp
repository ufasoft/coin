// #define UCFG_USE_OLD_MSVCRTDLL 0

#include <el/ext.h>

using namespace Ext;

#ifdef _M_IX86 

extern "C" {

#if UCFG_USE_OLD_MSVCRTDLL
#	undef time
#	undef mktime
#	undef localtime
#	undef gmtime
#	undef ctime
#	undef _stati64
#	undef _wstati64
#	undef _fstati64
#	undef _findfirsti64
#	undef _findnexti64
#	undef _utime
#	undef _ftime

	__declspec(dllimport) time_t __cdecl time(time_t *);
	__declspec(dllimport) time_t _cdecl mktime(struct tm * _Tm);
	__declspec(dllimport) struct tm * _cdecl localtime(const time_t * _Time);
	__declspec(dllimport) struct tm * _cdecl gmtime(const time_t * _Time);
	__declspec(dllimport) char * _cdecl ctime(const time_t * _Time);
	__declspec(dllimport) int _cdecl _stati64(const char*, struct _stat32i64 *);
	__declspec(dllimport) int _cdecl _wstati64(const wchar_t*, struct _stat32i64 *);
	__declspec(dllimport) int _cdecl _fstati64(int, struct _stat32i64 *);
	__declspec(dllimport) long _cdecl _findfirsti64(const char *, struct _finddata32i64_t *);
	__declspec(dllimport) int _cdecl _findnexti64(long, struct _finddata32i64_t *);
	__declspec(dllimport) int __cdecl _utime(const char *, struct _utimbuf *);
	__declspec(dllimport) void __cdecl _ftime(struct _timeb *timeptr);
#else
#	include <sys/utime.h>
#	include <sys/timeb.h>
#endif

int _cdecl API_utime(const char *filename, struct _utimbuf *timbuf) {
	return _utime(filename, timbuf);
}

void _cdecl API_ftime(struct _timeb *timeptr) {
	_ftime(timeptr);
}

time_t _cdecl API_time32(time_t *x) {
	return time(x);
}

time_t _cdecl API_mktime(struct tm * _Tm) {
	return mktime(_Tm);
}

struct tm * _cdecl API_localtime32(const time_t * _Time) {
	return localtime(_Time);
}

struct tm * _cdecl API_gmtime(const time_t * _Time) {
	return gmtime(_Time);
}

char * _cdecl API_ctime(const time_t * _Time) {
	return ctime(_Time);
}


#if UCFG_STDSTL
typedef struct _stat64 SBestVCRTStat;

static __time64_t Time32To64(__time64_t t64) {
	return t64;
}

#else
typedef struct _stat32i64 SBestVCRTStat;

static __time64_t Time32To64(__time32_t t32) {
	return t32;									//!!! 64-bit loss
}

#endif

EXT_API int __cdecl API_stat64(const char *path, struct _stat64 *buf) {
	SBestVCRTStat st;
	int r = _stati64(path, &st);

	buf->st_dev = st.st_dev;
	buf->st_ino = st.st_ino;
	buf->st_mode = st.st_mode;
	buf->st_nlink = st.st_nlink;
	buf->st_uid = st.st_uid;
	buf->st_gid = st.st_gid;
	buf->st_rdev = st.st_rdev;
	buf->st_size = st.st_size;
	buf->st_atime = Time32To64(st.st_atime);
	buf->st_mtime = Time32To64(st.st_mtime);
	buf->st_ctime = Time32To64(st.st_ctime);
	return r;
}

//!!!R EXT_API
int __cdecl API_wstat64(const wchar_t *path, struct _stat64 *buf) {
	SBestVCRTStat st;
	int r = _wstati64(path, &st);

	buf->st_dev = st.st_dev;
	buf->st_ino = st.st_ino;
	buf->st_mode = st.st_mode;
	buf->st_nlink = st.st_nlink;
	buf->st_uid = st.st_uid;
	buf->st_gid = st.st_gid;
	buf->st_rdev = st.st_rdev;
	buf->st_size = st.st_size;
	buf->st_atime = Time32To64(st.st_atime);
	buf->st_mtime = Time32To64(st.st_mtime);
	buf->st_ctime = Time32To64(st.st_ctime);
	return r;
}

//!!!R EXT_API
int __cdecl API_fstat64(int fildes, struct _stat64 *buf) {
	SBestVCRTStat st;
	int r = _fstati64(fildes, &st);
	buf->st_dev = st.st_dev;
	buf->st_ino = st.st_ino;
	buf->st_mode = st.st_mode;
	buf->st_nlink = st.st_nlink;
	buf->st_uid = st.st_uid;
	buf->st_gid = st.st_gid;
	buf->st_rdev = st.st_rdev;
	buf->st_size = st.st_size;
	buf->st_atime = Time32To64(st.st_atime);
	buf->st_mtime = Time32To64(st.st_mtime);
	buf->st_ctime = Time32To64(st.st_ctime);
	return r;
}

/*!!!R
int _cdecl API__fstat64i32(int _FileDes,  struct API_stat64i32 * _Stat) {
#if UCFG_USE_OLD_MSVCRTDLL
	struct _stat32i64 st;
	int r = _fstati64(_FileDes, &st);

	_Stat->st_dev = st.st_dev;
	_Stat->st_ino = st.st_ino;
	_Stat->st_mode = st.st_mode;
	_Stat->st_nlink = st.st_nlink;
	_Stat->st_uid = st.st_uid;
	_Stat->st_gid = st.st_gid;
	_Stat->st_rdev = st.st_rdev;
	_Stat->st_size = static_cast<_off_t>(st.st_size);
	_Stat->st_atime = st.st_atime;		//!!! 64-bit loss
	_Stat->st_mtime = st.st_mtime;
	_Stat->st_ctime = st.st_ctime;
	return r;
#else
	return 0;//!!!
#endif
}*/

long _cdecl API__wcstol_l(const wchar_t *nptr, wchar_t ** endptr, int base, _locale_t loc) {
	Throw(E_NOTIMPL);
}

#if UCFG_USE_OLD_MSVCRTDLL


/*!!!R
EXT_API int __cdecl API_stat64i32(const char *path, struct API_stat64i32 *buf) {
	struct _stat32i64 st;
	int r = _stati64(path, &st);

	buf->st_dev = st.st_dev;
	buf->st_ino = st.st_ino;
	buf->st_mode = st.st_mode;
	buf->st_nlink = st.st_nlink;
	buf->st_uid = st.st_uid;
	buf->st_gid = st.st_gid;
	buf->st_rdev = st.st_rdev;
	buf->st_size = static_cast<_off_t>(st.st_size);
	buf->st_atime = Time32To64(st.st_atime);
	buf->st_mtime = Time32To64(st.st_mtime);
	buf->st_ctime = Time32To64(st.st_ctime);
	return r;
}

int __cdecl API__wstat64i32(const wchar_t *filename, struct API_stat64i32 * _Stat) {
	struct _stat32i64 st;
	int r = _wstati64(filename, &st);

	_Stat->st_dev = st.st_dev;
	_Stat->st_ino = st.st_ino;
	_Stat->st_mode = st.st_mode;
	_Stat->st_nlink = st.st_nlink;
	_Stat->st_uid = st.st_uid;
	_Stat->st_gid = st.st_gid;
	_Stat->st_rdev = st.st_rdev;
	_Stat->st_size = static_cast<_off_t>(st.st_size);
	_Stat->st_atime = Time32To64(st.st_atime);
	_Stat->st_mtime = Time32To64(st.st_mtime);
	_Stat->st_ctime = Time32To64(st.st_ctime);
	return r;
}
*/
intptr_t _cdecl API__findfirst64i32(const char * _Filename, struct _finddata64i32_t * _FindData) {
	struct _finddata32i64_t fd;
	intptr_t r = _findfirsti64(_Filename, &fd);
	if (r >= 0) {
		_FindData->attrib = fd.attrib;
		_FindData->time_create = fd.time_create;		//!!! 64-bit loss
		_FindData->time_access = fd.time_access;		//!!! 64-bit loss
		_FindData->time_write = fd.time_write;		//!!! 64-bit loss
		_FindData->size = static_cast<_fsize_t>(fd.size);		//!!! 64-bit loss
		strcpy(_FindData->name, fd.name);
	}
	return r;
}

int _cdecl API__findnext64i32(intptr_t _FindHandle, struct _finddata64i32_t * _FindData) {
	struct _finddata32i64_t fd;
	int r = _findnexti64(_FindHandle, &fd);
	if (r >= 0) {
		_FindData->attrib = fd.attrib;
		_FindData->time_create = Time32To64(fd.time_create);
		_FindData->time_access = Time32To64(fd.time_access);
		_FindData->time_write = Time32To64(fd.time_write);
		_FindData->size = static_cast<_fsize_t>(fd.size);		//!!! 64-bit loss
		strcpy(_FindData->name, fd.name);
	}
	return r;
}
#endif // UCFG_USE_OLD_MSVCRTDLL




#if !UCFG_EXT_C

#	if !UCFG_STDSTL
#		define _lock_file    VS_lock_file
#		define _unlock_file    VS_unlock_file
#		define printf    VS_printf
#		define vsprintf    VS_vsprintf
//	#define _INC_STDIO
#	endif

#define _INC_COMDEF




#	define _CRTBLD
#	if UCFG_STDSTL
#		include <../crt/src/mtdll.h>
#	else
#		define __threadhandle my__threadhandle
#			include </src/foreign/vc6/float.h>
#			include </src/foreign/vc6/mtdll.h>
#		undef __threadhandle

//	#pragma comment(lib, "/src/foreign/lib/vs6_eh.lib")
#	endif

#	if !UCFG_STDSTL
#		undef _lock_file
#		undef _unlock_file
#		undef printf
#		undef vsprintf
#	endif



#endif

/*!!!

#define __threadhandle C__threadhandle
#define _CRTBLD
#include </src/foreign/vc6/float.h>
#include </src/foreign/vc6/mtdll.h>
*/

//!!!#include <windows.h>

_ptiddata __cdecl _getptd_noexit()
{
	char *p = (char*)__fpecode();
	return CONTAINING_RECORD(p, struct _tiddata, _tfpecode);
}

_ptiddata __cdecl _getptd()
{
	_ptiddata ptd = _getptd_noexit();
	//!!!		if (!ptd)
	//!!!			_amsg_exit(_RT_THREAD); /* write message and die */
	return ptd;
}

} // extern "C"

#endif // _M_IX86 

#include "ext-os-api.h"

#include <../crt/src/mtdll.h>

namespace Ext {

#if UCFG_USE_OLD_MSVCRTDLL					// VC don't support this field

struct PtdData {
	int _ProcessingThrow;
	int _cxxReThrow;
	EHExceptionRecord *_pForeignException;
	void *_pFrameInfoChain;
	void *_curexcspec;
};

#	if UCFG_USE_DECLSPEC_THREAD
		static __declspec(thread) PtdData t_ptdData;
#		define PTDDATA t_ptdData
#	else
		static tls_ptr<PtdData> t_pPtdData;
#		define PTDDATA (*t_pPtdData)
#	endif
#else
#		define PTDDATA (*ptid)
#endif

ExcRefs ExcRefs::GetForCurThread() {
#if UCFG_USE_OLD_MSVCRTDLL
	_ptiddata ptid = _getptd_noexit();
	if (!ptid)
		Throw(E_OUTOFMEMORY);
	auto& ptdata = PTDDATA;
	return ExcRefs((EHExceptionRecord*&)ptid->_curexception, ptid->_curcontext, ptdata._pFrameInfoChain
#	if UCFG_PLATFORM_IX86
		, ptdata._curexcspec
#else
		, ptid->_curexcspec
#	endif
		, ptdata._cxxReThrow
		, (_se_translator_function&)ptid->_translator		
#	ifdef _M_X64
		, ptid->_curexception
		, ptid->_curcontext
		, ptid->_ImageBase
		, ptid->_ThrowImageBase
#	endif				
#	if defined (_M_X64) || defined (_M_ARM)
		, (EHExceptionRecord*&)ptdata._pForeignException
#	endif
		);
#else
	_ptiddata ptid = _getptd();
	return ExcRefs((EHExceptionRecord*&)ptid->_curexception, ptid->_curcontext, PTDDATA._pFrameInfoChain, ptid->_curexcspec, ptid->_cxxReThrow, (_se_translator_function&)ptid->_translator		
#	ifdef _M_X64
		, ptid->_curexception
		, ptid->_curcontext
		, ptid->_ImageBase
		, ptid->_ThrowImageBase
#	endif				
#	if defined (_M_X64) || defined (_M_ARM)
		, (EHExceptionRecord*&)ptid->._pForeignException
#	endif
		);
#endif
}

int& ExcRefs::ProcessingThrowRef() {
#if UCFG_USE_OLD_MSVCRTDLL
	_ptiddata ptid = _getptd_noexit();
	if (!ptid)
		Throw(E_OUTOFMEMORY);
#else
	_ptiddata ptid = _getptd();
#endif
	return PTDDATA._ProcessingThrow;
}


} // Ext::

