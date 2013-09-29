/*######     Copyright (c) 1997-2013 Ufasoft  http://ufasoft.com  mailto:support@ufasoft.com,  Sergey Pavlov  mailto:dev@ufasoft.com #######################################
#                                                                                                                                                                          #
# This program is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation;  #
# either version 3, or (at your option) any later version. This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the      #
# implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details. You should have received a copy of the GNU #
# General Public License along with this program; If not, see <http://www.gnu.org/licenses/>                                                                               #
##########################################################################################################################################################################*/

#include <el/ext.h>

#if UCFG_WIN32
#	include <shlwapi.h>

#	include <el/libext/win32/ext-win.h>
#endif

//#include <el/libext/ext-os-api.h>

#if UCFG_ALLOCATOR == UCFG_ALLOCATOR_TC_MALLOC

#	ifndef _M_ARM
#		define set_new_handler my_set_new_handler
#	endif

#	define open _open
#	define read _read
#	define write _write


#	pragma warning(disable: 4146 4244 4291 4295 4310 4130 4232 4242 4018 4057 4090 4101 4152 4245 4267 4505 4668 4700 4701 4716)
#	include "../el-std/tc_malloc.cpp"

/*!!!
extern "C" {
	void* __cdecl tc_malloc(size_t size);
	void __cdecl tc_free(void *p);
	void* __cdecl tc_realloc(void* old_ptr, size_t new_size);
	size_t __cdecl tc_malloc_size(void* p);
}*/
#endif	// UCFG_ALLOCATOR == UCFG_ALLOCATOR_TC_MALLOC


namespace Ext {
using namespace std;

#if UCFG_INDIRECT_MALLOC
void * (AFXAPI *CAlloc::s_pfnMalloc			)(size_t size)					= &CAlloc::Malloc;
void   (AFXAPI *CAlloc::s_pfnFree			)(void *p)						= &CAlloc::Free;
size_t (AFXAPI *CAlloc::s_pfnMSize			)(void *p)						= &CAlloc::MSize;
void * (AFXAPI *CAlloc::s_pfnRealloc		)(void *p, size_t size) 		= &CAlloc::Realloc;
void * (AFXAPI *CAlloc::s_pfnAlignedMalloc	)(size_t size, size_t align) 	= &CAlloc::AlignedMalloc;
void (AFXAPI *CAlloc::s_pfnAlignedFree	)(void *p)						= &CAlloc::Free;



#	if UCFG_WCE
static int s_bCeAlloc = SetCeAlloc();
#	endif

#endif

/*!!!
CAlloc::~CAlloc() {
}*/

void *CAlloc::Malloc(size_t size) {
#ifdef WDM_DRIVER
	if (void *p = ExAllocatePoolWithTag(NonPagedPool, size, UCFG_POOL_TAG))
		return p;
#elif UCFG_ALLOCATOR == UCFG_ALLOCATOR_TC_MALLOC
#	if UCFG_HEAP_CHECK
	if (void *p = tc_malloc(size)) 
#	else
	if (void *p = do_malloc(size)) 
#	endif
		return p;
#else
	if (void *p = malloc(size))
		return p;
#endif
	Throw(E_OUTOFMEMORY);
}

void CAlloc::Free(void *p) {
#ifdef WDM_DRIVER
	if (!p)
		return;
	ExFreePoolWithTag(p, UCFG_POOL_TAG);
#elif UCFG_ALLOCATOR == UCFG_ALLOCATOR_TC_MALLOC
#	if UCFG_HEAP_CHECK
	tc_free(p);
#	else
	do_free(p);
#	endif
#else
	free(p);
#endif
}

size_t CAlloc::MSize(void *p) {
#if UCFG_WIN32 && (UCFG_ALLOCATOR == UCFG_ALLOCATOR_STD_MALLOC)
	size_t r = _msize(p);
	CCheck(r != size_t(-1));
	return r;
#elif UCFG_ALLOCATOR == UCFG_ALLOCATOR_TC_MALLOC
	return tc_malloc_size(p);
#else
	Throw (E_NOTIMPL);
#endif
}

void *CAlloc::Realloc(void *p, size_t size) {
#ifndef WDM_DRIVER
#if UCFG_ALLOCATOR == UCFG_ALLOCATOR_TC_MALLOC
	if (void *r = tc_realloc(p, size))
		return r;
#else
	if (void *r = realloc(p, size))
		return r;
#endif
	Throw(E_OUTOFMEMORY);
#else
	Throw (E_NOTIMPL);
#endif
}

void *CAlloc::AlignedMalloc(size_t size, size_t align) {
#if UCFG_ALLOCATOR == UCFG_ALLOCATOR_TC_MALLOC
	if (void *p = do_memalign(align, size)) 
		return p;
#elif UCFG_USE_POSIX
	void *r;
	CCheckErrcode(posix_memalign(&r, align, size));
	return r;
#elif UCFG_MSC_VERSION && !UCFG_WDM
	if (void *r = _aligned_malloc(size, align))
		return r;
	else
		CCheck(-1);
#else
	Throw(E_NOTIMPL);
#endif
	Throw(E_OUTOFMEMORY);
}

void CAlloc::AlignedFree(void *p) {
#if UCFG_ALLOCATOR == UCFG_ALLOCATOR_TC_MALLOC
	do_free(p);
#elif UCFG_USE_POSIX
	free(p);
#elif UCFG_MSC_VERSION && !UCFG_WDM
	_aligned_free(p);
#else
	Throw(E_NOTIMPL);
#endif
}

#if !defined(WDM_DRIVER) && !UCFG_MINISTL

String g_ExceptionMessage;

#if UCFG_WIN32
void AFXAPI ProcessExceptionInFilter(EXCEPTION_POINTERS *ep) {
	ostringstream os;
	os << "Code:\t" << hex << ep->ExceptionRecord->ExceptionCode << "\n"
		<< "Address:\t" << ep->ExceptionRecord->ExceptionAddress << "\n";
	g_ExceptionMessage = os.str();
	TRC(0, g_ExceptionMessage);
}

void AFXAPI ProcessExceptionInExcept() {
#if UCFG_GUI
	MessageBox::Show(g_ExceptionMessage);
#endif
	::ExitProcess(ERR_UNHANDLED_EXCEPTION);
}
#endif

void AFXAPI ProcessExceptionInCatch() {
	try {
		throw;
	} catch (RCExc e) {
		TRC(0, e);
		wcerr << e.Message << endl;
#if UCFG_GUI
		if (!IsConsole())
			MessageBox::Show(e.Message);
#endif
#if !UCFG_CATCH_UNHANDLED_EXC
		throw;
#endif
	} catch (std::exception& e) {
		TRC(0, e.what());
		cerr << e.what() << endl;
#if UCFG_GUI
	if (!IsConsole())
		MessageBox::Show(e.what());
#endif
#if !UCFG_CATCH_UNHANDLED_EXC
		throw;
#endif
	} catch (...) {
		TRC(0, "Unknown C++ exception");
		cerr << "Unknown C++ Exception" << endl;
#if UCFG_GUI
	if (!IsConsole())
		MessageBox::Show("Unknown C++ Exception");
#endif
#if !UCFG_CATCH_UNHANDLED_EXC
		throw;
#endif
	}
#if UCFG_USE_POSIX
	_exit(ERR_UNHANDLED_EXCEPTION);
#else
	//!!! Error in DLLS ::ExitProcess(ERR_UNHANDLED_EXCEPTION);	
	Win32Check(::TerminateProcess(::GetCurrentProcess(), ERR_UNHANDLED_EXCEPTION));
#endif
}

#endif

} // Ext::


#if UCFG_WDM
//!!!POOLTAG DefaultPoolTag = UCFG_POOL_TAG;
#else

/*!!!
void * __cdecl operator new(size_t sz) {
	return Ext::Malloc(sz);
}

void __cdecl operator delete(void *p) {
	Ext::Free(p);
}

void __cdecl operator delete[](void *p) {
	Ext::Free(p);
}

void * __cdecl operator new[](size_t sz) {
	return Ext::Malloc(sz);
}
*/

#	if UCFG_STDSTL
void __cdecl operator delete[](void *p, const std::nothrow_t& ) {
	operator delete(p);//!!!
}	
#else
void __cdecl operator delete[](void *p, const ExtSTL::nothrow_t& ) {
	operator delete(p);//!!!
}	
#	endif // UCFG_STDSTL

#endif // UCFG_WDM

static void * (__cdecl *s_pfnNew)(size_t) = &operator new;
static void * (__cdecl *s_pfnNewArray)(size_t) = &operator new[];
static void (__cdecl *s_pfnDelete)(void*) = &operator delete;
static void (__cdecl *s_pfnDeleteArray)(void*) = &operator delete[];

#if defined(_MSC_VER) && !UCFG_STDSTL

extern "C" {

BOOL __cdecl __crtIsWin8orLater(void) {
	return false;
}

LONG __cdecl __crtUnhandledException
(
    _In_ EXCEPTION_POINTERS *exceptionInfo
) {
	Ext::ThrowImp(E_FAIL);
}

extern "C" void _cdecl API_terminate();

void __cdecl __crtTerminateProcess
(
    _In_ UINT uExitCode
) {
	API_terminate();
}

void __cdecl __crtCaptureCurrentContext
(
    _Out_ CONTEXT *pContextRecord
) {
	Ext::ThrowImp(E_FAIL);

}

void __cdecl __crtCapturePreviousContext
(
    _Out_ CONTEXT *pContextRecord
) {
	Ext::ThrowImp(E_FAIL);

}



} // "C"

#endif // !UCFG_STDSTL




