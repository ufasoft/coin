/*######     Copyright (c) 1997-2013 Ufasoft  http://ufasoft.com  mailto:support@ufasoft.com,  Sergey Pavlov  mailto:dev@ufasoft.com #######################################
#                                                                                                                                                                          #
# This program is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation;  #
# either version 3, or (at your option) any later version. This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the      #
# implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details. You should have received a copy of the GNU #
# General Public License along with this program; If not, see <http://www.gnu.org/licenses/>                                                                               #
##########################################################################################################################################################################*/

#pragma once


//!!! #include "../eh/ext_eh.h"

#if defined(__cplusplus)
#	include <eh.h>

#	if UCFG_WDM

typedef LONG (__stdcall *PTOP_LEVEL_EXCEPTION_FILTER)(
	__in struct _EXCEPTION_POINTERS *ExceptionInfo
	);
typedef PTOP_LEVEL_EXCEPTION_FILTER LPTOP_LEVEL_EXCEPTION_FILTER;

typedef int                 BOOL;

#	endif

struct EHExceptionRecord;

namespace Ext {

struct ExcRefs {
	EHExceptionRecord*& ExceptionRecord;
	void*& Context;
	void*& FrameInfoChain;
	void *& CurExcSpec;
#if !UCFG_WCE
	int& CxxReThrow;
	_se_translator_function& Translator; 
#endif
#ifdef _M_X64
	void *&      _curexception;  /* current exception */
	void *&      _curcontext;    /* current exception context */
	UInt64&		ImageBase;
	UInt64&		ThrowImageBase;
#endif
#if defined (_M_X64) || defined (_M_ARM)
	EHExceptionRecord*& ForeignException;		
#endif

	ExcRefs(EHExceptionRecord*& er, void*& ctx, void*& frameInfoChain, void *&curexcspec
#if !UCFG_WCE
		, int& cxxReThrow
		, _se_translator_function& tr
#endif
#ifdef _M_X64
		, void *&      curexception
		, void *&      curcontext
		, UInt64&      imageBase
		, UInt64&      throwImageBase
#endif		
#if defined (_M_X64) || defined (_M_ARM)
		,	EHExceptionRecord*& foreignException
#endif
		)
		:	ExceptionRecord(er)
		,	Context(ctx)
		,	FrameInfoChain(frameInfoChain)
		,	CurExcSpec(curexcspec)
#if !UCFG_WCE
		,	CxxReThrow(cxxReThrow)
		,	Translator(tr)
#endif
#ifdef _M_X64
		,	_curexception(curexception)
		,	_curcontext(curcontext)
		,	ImageBase(imageBase)
		,	ThrowImageBase(throwImageBase)
#endif		
#if defined (_M_X64) || defined (_M_ARM)
		,	ForeignException(foreignException)
#endif
	{
	}

	static ExcRefs __stdcall GetForCurThread();
	static int& __stdcall ProcessingThrowRef();			// for uncaught_exception
};


bool __stdcall InitTls(void *hInst);

} // Ext::
#endif // defined(__cplusplus) && !UCFG_WCE

#if defined(__cplusplus)

namespace Ext {
	bool __stdcall HOOK_SelectFastRaiseException(void* pExceptionObject, void  *_pThrowInfo, void *imageBase);

#ifdef _WIN32
	typedef BOOLEAN (NTAPI *PFN_RtlDispatchException)(IN PEXCEPTION_RECORD ExceptionRecord, IN PCONTEXT ContextRecord);
	PFN_RtlDispatchException __stdcall GetProcAddress_RtlDispatchException();
#endif

} // Ext::

#endif


__BEGIN_DECLS

void _stdcall API_RtlRaiseException(EXCEPTION_RECORD *e);
void _stdcall API_RaiseException(DWORD dwExceptionCode, DWORD dwExceptionFlags, DWORD nNumberOfArguments, const ULONG_PTR *lpArguments);


void _stdcall IMP_RtlUnwind (void * TargetFrame, void *TargetIp, PEXCEPTION_RECORD ExceptionRecord, void * ReturnValue);
void _stdcall API_RtlUnwind (void * TargetFrame, void * TargetIp, PEXCEPTION_RECORD ExceptionRecord, void * ReturnValue);
//!!!LPTOP_LEVEL_EXCEPTION_FILTER 
void *_stdcall API_SetUnhandledExceptionFilter(void * /*!!!	LPTOP_LEVEL_EXCEPTION_FILTER*/ lpTopLevelExceptionFilter);

#ifdef _M_X64
void * _stdcall API_RtlPcToFileHeader(const void *PcValue, void **pBaseOfImage);
void _stdcall API_RtlUnwindEx(void *TargetFrame, PVOID TargetIp, PEXCEPTION_RECORD ExceptionRecord, PVOID ReturnValue, PCONTEXT OriginalContext, void *HistoryTable);
void * _stdcall API_RtlLookupFunctionEntry(DWORDLONG ControlPC, DWORDLONG *ImageBase, DWORDLONG *TargetGp);

#endif


BOOL _stdcall API_IsBadReadPtr(CONST VOID *lp, UINT_PTR ucb);
BOOL _stdcall API_IsBadWritePtr(VOID *lp, UINT_PTR ucb);
BOOL _stdcall API_IsBadCodePtr(void *lpfn);
BOOLEAN _stdcall API_RtlpGetStackLimits(void **stackLimit, void **stackBase);

#if UCFG_WCE

struct TEB {
	void *DriverDirectCaller;
	DWORD OwnerProcessVersion;
	void *ThreadInfo;
	size_t ProcStackSize;
	int CurFiber;
	void *StackBound;
	void *StackBase;
};

HRESULT API_SafeArrayGetVartype(SAFEARRAY * psa, VARTYPE * pvt);

#endif // UCFG_WCE

typedef ULONG (_cdecl *PFN_printf)(const char *format, ...);
extern PFN_printf g_API_printf;


#if UCFG_EXT_C && defined(_M_ARM)

#	define _JBLEN    11
typedef int jmp_buf[_JBLEN];

#endif


__declspec(noreturn) void __cdecl API_longjmp(jmp_buf env, int val);
void _cdecl API_terminate();
void _cdecl API_unexpected();

#ifdef __cplusplus
bool _cdecl API_uncaught_exception();
#endif

__END_DECLS
