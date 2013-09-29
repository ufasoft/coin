/*######     Copyright (c) 1997-2013 Ufasoft  http://ufasoft.com  mailto:support@ufasoft.com,  Sergey Pavlov  mailto:dev@ufasoft.com #######################################
#                                                                                                                                                                          #
# This program is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation;  #
# either version 3, or (at your option) any later version. This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the      #
# implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details. You should have received a copy of the GNU #
# General Public License along with this program; If not, see <http://www.gnu.org/licenses/>                                                                               #
##########################################################################################################################################################################*/

#include <el/ext.h>

#if UCFG_WIN32
#	include <winuser.h>

#	include <el/libext/win32/ext-win.h>
#endif


#if UCFG_EXTENDED && UCFG_GUI
#	include <el/gui/controls.h>
#endif

//!!!#include <exception>//!!!

#if !UCFG_STDSTL || !UCFG_CPP11_HAVE_MUTEX || UCFG_SPECIAL_CRT
#	include <el/stl/mutex>
#endif

namespace ExtSTL {
#if !UCFG_STDSTL || !UCFG_CPP11_HAVE_THREAD
	ostream& AFXAPI operator<<(ostream& os, const thread::id& v);
#endif	// !UCFG_STDSTL || !UCFG_CPP11_HAVE_THREAD

#if !UCFG_STDSTL || !UCFG_CPP11_HAVE_MUTEX || UCFG_SPECIAL_CRT
	const adopt_lock_t adopt_lock = {};
	const defer_lock_t defer_lock = {};
	const try_to_lock_t try_to_lock = {};
#endif // !UCFG_STDSTL || !UCFG_CPP11_HAVE_MUTEX

#if UCFG_WCE

ostream& operator<<(ostream& os, const thread::id& v) {
	return os << v.m_tid;
}

#endif

} // ExtSTL::


namespace Ext {

using namespace std;

size_t ThreadBase::DefaultStackSize;

ThreadBase::ThreadBase(CThreadRef *ownRef)
	:	//!!! CWinThread(ThreaderFunction, this),
		StackSize(0)
	,	StackOffset(0)
	,	m_bAutoDelete(true)
	,	m_owner(ownRef)
	,	m_bStop(false)
	,	m_bSleeping(false)
	,	m_bAPC(false)
#if UCFG_USE_PTHREADS
	,	m_tid()
#elif UCFG_OLE
	,	m_usingCOM(E_NoInit)
	,	m_dwCoInit(COINIT_APARTMENTTHREADED)
#endif
{
	ASSERT(1000 < std::abs(Int32((byte*)&ownRef - (byte*)this)));		// Thread cannot be in stack
}

ThreadBase::~ThreadBase() {
	if (m_threadRef) {
		m_threadRef->StopChilds();
		delete m_threadRef;
	}

#if UCFG_EXTENDED	
	if (AFX_MODULE_THREAD_STATE* pState = AfxGetModuleState()->m_thread.GetDataNA())			// cleanup module state
		if (pState->m_pCurrentThread == this)
			pState->m_pCurrentThread = nullptr;
#endif
}

//!!!DCTls Thread::t_pCurThreader;//!!!

//!!!#if UCFG_EXTENDED

ThreadBase::propclass_CurrentThread ThreadBase::CurrentThread; //!!!

#if !UCFG_EXTENDED
static CTls s_currentThread;
#endif

ThreadBase* ThreadBase::get_CurrentThread() {
#if UCFG_EXTENDED || UCFG_WIN32
	AFX_MODULE_THREAD_STATE& mts = *AfxGetModuleState()->m_thread;
	if (!mts.m_pCurrentThread) { //!!!
		mts.m_pCurrentThread = new Thread;
		mts.m_pCurrentThread->AttachSelf();
	}
	return mts.m_pCurrentThread;
#else
	if (!s_currentThread.get_Value())
		s_currentThread.Value = new Thread; // Leak
	return (ThreadBase*)s_currentThread.get_Value();
#endif
	//!!!D	return *(Thread*)t_pCurThreader.Value;
}

ThreadBase* AFXAPI ThreadBase::TryGetCurrentThread() {
#if UCFG_EXTENDED || UCFG_WIN32
	return AfxGetModuleState()->m_thread->m_pCurrentThread;
#else
	return (ThreadBase*)s_currentThread.get_Value();
#endif
}

//!!!#endif

void ThreadBase::Sleep(DWORD dwMilliseconds) {
#if UCFG_WIN32 || UCFG_USE_PTHREADS
	CurrentThread->SleepImp(dwMilliseconds); //!!!
#else
	::usleep(dwMilliseconds*1000);
#endif
}

#if UCFG_USE_PTHREADS

int PthreadCheck(int code, int allowableError) {
	if (code && (code != allowableError))
		Throw(HRESULT_FROM_C(code));
	return code;
}

void ThreadBase::ReleaseHandle(HANDLE h) const {
	m_tid = thread::id();
	pthread_t ptid = exchange(m_ptid, (pthread_t)0);
	if (!m_bJoined)	
		PthreadCheck(::pthread_detach(ptid));
}

#endif

void ThreadBase::AttachSelf() {
#if UCFG_WIN32
	SafeHandle::Attach(GetCurrentThread(), false);
#	if !UCFG_STDSTL
	m_tid = this_thread::get_id();
#	endif
	m_nThreadID = ::GetCurrentThreadId();
#else
	m_ptid = ::pthread_self();
	m_tid = thread::id(m_ptid);
#endif
}

CThreadRef& ThreadBase::GetThreadRef() {
	if (!m_threadRef)
		m_threadRef = new CThreadRef;
	return *m_threadRef;
}

void ThreadBase::SleepImp(DWORD dwMilliseconds) {
#if UCFG_WIN32_FULL
	if (m_bAPC)
		m_bAPC = false;
	else if (!m_bStop)
		SleepEx(dwMilliseconds, TRUE);
#else
	usleep(dwMilliseconds*1000);
#endif
	if (m_bStop)
		Throw(E_EXT_ThreadStopped);
}


/*!!!
LPTOP_LEVEL_EXCEPTION_FILTER g_pfnUnhadledExceptionFilter;

void __stdcall MyExceptHandler(EXCEPTION_POINTERS *ep)
{
TRC(1, "MyExcept_handler");//!!!T
if (g_pfnUnhadledExceptionFilter)
g_pfnUnhadledExceptionFilter(ep);
else
{
AfxLogErrorCode(0xFFFFFFFF); //!!1
::MessageBox(0, "Fatal Error", "Critical Error!\nReport about it to " MANUFACTURER_EMAIL, MB_ICONEXCLAMATION); //!!!
}
ExitProcess(3);
}

void __cdecl Myterminate_handler()//!!!
{
__try
{
RaiseException(0x02345678, 0, 0, 0);
}
__except(MyExceptHandler(GetExceptionInformation()))
{}
AfxLogErrorCode(0xFFFFFFFF); //!!1
::MessageBox(0, "Fatal Error", "Critical Error!\nReport about it to " MANUFACTURER_EMAIL, MB_ICONEXCLAMATION); //!!!
ExitProcess(3);
}
*/

#if UCFG_OLE

void ThreadBase::InitCOM() {
	m_usingCOM.Initialize(m_dwCoInit);
}

#endif

CThreadRef::~CThreadRef() {
	if (m_bSync)			//!!!?
		StopChilds();
}

void CThreadRef::StopChilds() {
	SignalStop();
	WaitStop();
}

void CThreadRef::SignalStop() {
	EXT_LOCK (m_cs) {
		for (int i=0; i<m_ar.size(); i++) {
			ThreadBase *t = m_ar[i];
			t->m_bAutoDelete = false;
			if (*t)
				t->Stop();
		}
	}
}

void CThreadRef::WaitStop() {
	CThreadColl ar;
	EXT_LOCK (m_cs) {
		ar.swap(m_ar);
	}
	for (CThreadColl::iterator i(ar.begin()), e(ar.end()); i!=e; ++i) {
		if ((*i)->Valid())
			(*i)->Join();

		//!!!    int ec = t->ExitCode;
	}
}

bool CThreadRef::StopChild(ThreadBase *t, int msTimeout) {
	EXT_LOCK (m_cs) {
		t->m_bAutoDelete = false;
		if (*t)
			t->Stop();
	}
	if (*t) {
		if (!t->Join(msTimeout))
			return false;
	}

	EXT_LOCK (m_cs) {
		m_ar.erase(std::remove(m_ar.begin(), m_ar.end(), t), m_ar.end());
	}
	return true;
}

void ThreadBase::Delete() {
	if (m_owner) {
		m_owner->ExternalRelease();
		EXT_LOCK (m_owner->m_cs) {
			if (m_bAutoDelete)
				m_owner->m_ar.erase(std::remove(m_owner->m_ar.begin(), m_owner->m_ar.end(), this), m_owner->m_ar.end());
		}
	}
//!!!R	if (m_bAutoDelete)
//!!!R		delete this;
}

void ThreadBase::Execute() {
}

DWORD ThreadBase::get_ExitCode() {
#if UCFG_USE_PTHREADS
	return m_exitCode;
#else
	DWORD r;
	Win32Check(::GetExitCodeThread(HandleAccess(_self), &r));
	return r;
#endif
}

void ThreadBase::put_Name(RCString name) {
	m_name = name;
#if UCFG_USE_PTHREADS && defined(HAVE_PTHREAD_SETNAME_NP)
	CCheck(pthread_setname_np(m_ptid, name.Left(15)));
#elif UCFG_WIN32
	struct THREADNAME_INFO {
		DWORD dwType; // must be 0x1000
		LPCSTR szName; // pointer to name (in user addr space)
		DWORD dwThreadID; // thread ID (-1=caller thread)
		DWORD dwFlags; // reserved for future use, must be zero
	};

	THREADNAME_INFO info = { 0x1000, m_name, m_nThreadID };
	__try {
		RaiseException(0x406D1388, 0, sizeof(info)/sizeof(DWORD), (ULONG_PTR*)&info);
	} __except(EXCEPTION_CONTINUE_EXECUTION) {
	}
#endif
}

int ThreadBase::get_Priority() {
#if UCFG_USE_PTHREADS
	return 0;
#else
	int p = ::GetThreadPriority(HandleAccess(_self));
	Win32Check(p != THREAD_PRIORITY_ERROR_RETURN);
	return p;
#endif
}

void ThreadBase::put_Priority(int nPriority) {
#if !UCFG_USE_PTHREADS
	Win32Check(::SetThreadPriority(HandleAccess(_self), nPriority));
#endif
}

void ThreadBase::OnEnd() {
#if UCFG_OLE
	m_usingCOM.Uninitialize();
#endif
}

void ThreadBase::OnAPC() {
	m_bAPC = true;
//!!!	if (m_bStop)
//!!!		Socket::ReleaseFromAPC();
}

#if UCFG_USE_PTHREADS
TimeSpan ThreadBase::get_TotalProcessorTime() const {
	timespec ts;
	CCheck(::clock_gettime(CLOCK_THREAD_CPUTIME_ID, &ts));
	return ts;
}

#else // UCFG_USE_PTHREADS

CONTEXT ThreadBase::get_Context(DWORD contextFlags) {
	CONTEXT context;
	context.ContextFlags = contextFlags;
	Win32Check(::GetThreadContext(HandleAccess(_self), &context));
	return context;
}

void ThreadBase::put_Context(const CONTEXT& context) {
	Win32Check(::SetThreadContext(HandleAccess(_self), &context));
}

CTimesInfo ThreadBase::get_Times() const {
	CTimesInfo r;
	Win32Check(::GetThreadTimes(HandleAccess(_self), &r.m_tmCreation, &r.m_tmExit, &r.m_tmKernel, &r.m_tmUser));
	return r;
}

DWORD ThreadBase::Resume() {
	DWORD prevCount = ::ResumeThread(HandleAccess(_self));
	Win32Check(prevCount != 0xFFFFFFFF);
	return prevCount;
}

DWORD ThreadBase::Suspend() {
	DWORD prevCount = ::SuspendThread(HandleAccess(_self));
	Win32Check(prevCount != 0xFFFFFFFF);
	return prevCount;
}

void ThreadBase::PostMessage(UINT message, WPARAM wParam, LPARAM lParam) {
	Win32Check(::PostThreadMessage(m_nThreadID, message, wParam, lParam));
}


#	if !UCFG_WCE

VOID CALLBACK ThreadBase::APCProc(ULONG_PTR dwParam) {
	((ThreadBase*)dwParam)->OnAPC();
}

void ThreadBase::QueueUserAPC(PAPCFUNC pfnAPC, ULONG_PTR dwData) {
	if (::QueueUserAPC(pfnAPC, HandleAccess(_self), dwData))
		return;
	Throw(E_EXT_QueueUserAPC);				// No GetLastError value
}

void ThreadBase::QueueAPC() {
	try {
		QueueUserAPC(&APCProc, (DWORD_PTR)this);
	} catch (RCExc) {
	}
}
#	endif // !UCFG_WCE

#endif // UCFG_USE_PTHREADS

bool ThreadBase::Join(int ms) {
#if UCFG_USE_PTHREADS
	ASSERT(ms == INFINITE);
	void *r;	
	PthreadCheck(::pthread_join(m_ptid, &r));
	m_bJoined = true;
	m_exitCode = (DWORD)(DWORD_PTR)r;
	return true;
#else
	int r = ::WaitForSingleObject(HandleAccess(_self), ms);
	if (r == WAIT_TIMEOUT)
		return false;
	Win32Check(r != WAIT_FAILED);
	return true;
#endif
}

#if UCFG_USE_PTHREADS
static void Sigusr1Handler(int sig) {
}
#endif

void ThreadBase::SignalStop() {
	m_bStop = true;
	if (Valid()) {
		if (m_bSleeping) {

#if UCFG_USE_PTHREADS
			static bool s_bSigusr1HandlerInited;
			if (!s_bSigusr1HandlerInited) {
				struct sigaction act = { 0 };
				::sigemptyset(&act.sa_mask);
				act.sa_handler = &Sigusr1Handler;
				::sigaction(SIGUSR1, &act, nullptr);
				s_bSigusr1HandlerInited = true;
			}
			PthreadCheck(::pthread_kill(m_ptid, SIGUSR1));  //!!!? may be other signal

#elif !UCFG_WCE
			QueueAPC();
#endif
		}
	}
}

void ThreadBase::Stop() {
	SignalStop();
}

void ThreadBase::WaitStop() {
	if (Valid())
		Join();
}

/*
struct _AFX_THREAD_STARTUP {
	ThreadBase *m_pThread;
#ifdef WIN32
	AFX_MODULE_STATE *m_pModuleState;
#endif
};*/

void AFXAPI AfxTermThread(HINSTANCE hInstTerm) {
#if UCFG_EXTENDED
#	if !UCFG_WCE && UCFG_GUI
	if (!hInstTerm)
		if (_AFX_THREAD_STATE* pThreadState = _afxThreadState.GetDataNA())
			pThreadState->m_pToolTip.reset();
#	endif
#endif
	if (_afxThreadData)
		_afxThreadData->DeleteValues(hInstTerm, false);		//!!! 
}

void AFXAPI AfxEndThread(UINT nExitCode, bool bDelete) {
#if UCFG_EXTENDED || UCFG_WIN32
	AFX_MODULE_THREAD_STATE* pState = AfxGetModuleThreadState();
	if (ThreadBase *pThread = pState->m_pCurrentThread) {
		pThread->OnEnd();
		if (bDelete)
			pThread->Delete();
		pState->m_pCurrentThread = 0;
	}
#else
	s_currentThread.Value = 0;
#endif
	AfxTermThread();
#if UCFG_USE_PTHREADS
	::pthread_exit((void*)(uintptr_t)nExitCode);
#elif UCFG_WCE
	::ExitThread(nExitCode);
#else
	_endthreadex(nExitCode);
#endif
}

void ThreadBase::OnEndThread(bool bDelete) {
#if UCFG_WIN32
	TRC(4, hex << Name << " " << m_nThreadID.Value());
#endif
	
	OnEnd();
	if (bDelete)
		Delete();
	AfxTermThread();
}

UInt32 ThreadBase::CppThreadThunk() {
	UInt32 exitCode = (UINT)E_FAIL; //!!!
	alloca(StackOffset);							// to prevent cache line aliasing 
	try {
#ifdef X_CPPRTTI
		const type_info& ti = typeid(*this);
		Name = ti.name();
#endif
		{
			struct ActiveThreadKeeper {
				CThreadRef *m_tr;

				ActiveThreadKeeper(CThreadRef *tr)
					:	m_tr(tr)
				{
					if (m_tr)
						Interlocked::Increment(m_tr->RefCountActiveThreads);
				}

				~ActiveThreadKeeper() {
					if (m_tr)
						Interlocked::Decrement(m_tr->RefCountActiveThreads);
				}
			} actveThreadKeeper(m_owner);

			DBG_LOCAL_IGNORE(E_EXT_ThreadStopped);

			Execute();
			exitCode = m_exitCode;
		}
		OnEndThread(true);
	} catch (ThreadAbortException& ex) {
		exitCode = m_exitCode = ex.HResult;
		TRC(1, ex.Message << " in Thread: " << get_id() << "\t" << Name);
		OnEndThread(true);
	} catch (...) {
		TRC(0, "Unhandled exception in Thread: " << hex << get_id() << "\t" << Name);
		ProcessExceptionInCatch();
		OnEndThread(true);
	}
	CCounterIncDec<ThreadBase, Interlocked>::Release(this);
	return exitCode;
}

#if UCFG_WCE

UINT ThreadBase::ThreaderFunction(LPVOID pParam) {
	__try {
//!!!R		_AFX_THREAD_STARTUP *pStartup = (_AFX_THREAD_STARTUP*)pParam;
		ThreadBase *pT = (ThreadBase*)pParam;
		return pT->CppThreadThunk();
	} __except (ProcessExceptionInFilter(GetExceptionInformation()), EXCEPTION_EXECUTE_HANDLER) {
		ProcessExceptionInExcept();
		return (UINT)E_FAIL;
	}	
}


#else

#if UCFG_USE_PTHREADS
	void * __cdecl ThreadBase::ThreaderFunction(void *pParam)
#else
	UINT ThreadBase::ThreaderFunction(LPVOID pParam)
#endif
{
	//!!!  set_terminate(Myterminate_handler);//!!!

	//!!!	__try
	//!!!	{
	//!!!  pThread->InitCOM();
	//!!!		bool bDelete = true;
	ThreadBase *pT = (ThreadBase*)pParam;;
#if UCFG_WIN32

	_AFX_THREAD_STATE* pThreadState = AfxGetThreadState();
	pThreadState->m_pModuleState = pT->m_pModuleStateForThread; //!!!pStartup->m_pModuleState;
	AFX_MODULE_STATE *pModuleState = AfxGetModuleState();
#ifdef _AFXDLL
	pT->m_pModuleState = pModuleState;
#endif
	pModuleState->m_thread->m_pCurrentThread = pT;
#else
	s_currentThread.Value = pT;
#endif
//!!!R	delete pStartup;

	DWORD r = pT->CppThreadThunk();

#if UCFG_USE_PTHREADS
	return (void*)(uintptr_t)r;
#else
	return r;
#endif

	/*!!!
	pT->TryExecute();
	DWORD exitCode = pT->ExitCode;
	AfxEndThread(exitCode, bDelete);
	return exitCode;*/
	/*!!!	}
	__except (MyExceptHandler(GetExceptionInformation()))
	{} */
	return 0;
}

#endif

void ThreadBase::Create(DWORD dwCreateFlags, size_t nStackSize
#ifdef WIN32
						, LPSECURITY_ATTRIBUTES lpSecurityAttrs
#endif
						)
{

	ASSERT(!Valid());

#ifdef WIN32
	m_pModuleStateForThread = AfxGetThreadState()->m_pModuleState;
#endif
	Interlocked::Increment(base::m_dwRef);
#if UCFG_USE_PTHREADS
	CAttr attr;
	if (nStackSize)
		attr.StackSize = nStackSize;
	int rc = ::pthread_create(&m_ptid, attr, &ThreaderFunction, this);
	if (rc) {
		CCounterIncDec<ThreadBase, Interlocked>::Release(this);
		PthreadCheck(rc);
	}
	m_tid = thread::id(m_ptid);
#else
	HANDLE h;
	DWORD threadID;

#	if UCFG_WCE
		h = ::CreateThread(lpSecurityAttrs, nStackSize, (LPTHREAD_START_ROUTINE)&ThreaderFunction, this, dwCreateFlags, &threadID);
#	else
		h = (HANDLE)_beginthreadex(lpSecurityAttrs, (UINT)nStackSize, &ThreaderFunction, this, dwCreateFlags, (UINT*)&threadID); //!!!
#	endif
	if (!h || h==(HANDLE)-1) {
		CCounterIncDec<ThreadBase, Interlocked>::Release(this);
	}
	Attach(h, threadID);
#endif
}

void ThreadBase::Start(DWORD flags) {
	try {
		BeforeStart();
	} catch (RCExc) {
//!!!R		if (m_bAutoDelete)
//!!!R			delete this;
		throw;
	}
	if (m_owner) {
		EXT_LOCK (m_owner->m_cs) {
			m_owner->m_ar.push_back(this);
			m_owner->ExternalAddRef();
		}
	}
	if (!StackSize) {
		if (!ThreadBase::DefaultStackSize) {
			String sStackSize = Environment::GetEnvironmentVariable("UCFG_THREAD_STACK_SIZE");
			ThreadBase::DefaultStackSize = !!sStackSize ? Convert::ToUInt32(sStackSize) : UCFG_THREAD_STACK_SIZE;
		}
		StackSize = ThreadBase::DefaultStackSize;
	}
#ifdef _WIN32
	if (StackSize)
		flags |= STACK_SIZE_PARAM_IS_A_RESERVATION;
#endif
	Create(flags, StackSize);
}

CSeparateThreadThreader::CSeparateThreadThreader(CSeparateThread& st)
	:	ThreadBase(&st)
	,	m_st(st)
{
	m_bSleeping = true;
}

CSeparateThreadThreader::~CSeparateThreadThreader() {
	EXT_LOCK (m_st.m_cs) {
		m_st.m_pT = nullptr;
	}
}

void CSeparateThreadThreader::BeforeStart() {
	m_st.BeforeStart();
}

void CSeparateThreadThreader::Execute() {
	m_st.Execute();
}

void CSeparateThreadThreader::Stop() {
	ThreadBase::Stop();
	m_st.Stop();
}

CSeparateThread::~CSeparateThread() {
	if (m_bSync)			//!!!?
		StopChilds();
}

void CSeparateThread::Stop() {
//!!!R		if (m_pT)							// Endless recursion
//!!!R			m_pT->Stop();
}

void CSeparateThread::Create(DWORD flags) {
	(m_pT = new CSeparateThreadThreader(_self))->Start(flags);
}

#ifdef WIN32
CWinThread::CWinThread(CThreadRef *ownRef)
	:	ThreadBase(ownRef)
{
	m_pThreadParams = 0;
	m_pfnThreadProc = 0;
	CommonConstruct();
}

CWinThread::CWinThread(AFX_THREADPROC pfnThreadProc, LPVOID pParam) {
	m_pfnThreadProc = pfnThreadProc;
	m_pThreadParams = pParam;
	CommonConstruct();
}

void CWinThread::CommonConstruct() {
	m_hr = 0;
	m_lpfnOleTermOrFreeLib = 0;
}

CWinThread* AFXAPI AfxBeginThread(AFX_THREADPROC pfnThreadProc, LPVOID pParam, int nPriority, UINT nStackSize, DWORD dwCreateFlags, LPSECURITY_ATTRIBUTES lpSecurityAttrs) {
	CWinThread* pThread = new CWinThread(pfnThreadProc, pParam);
	pThread->Create(dwCreateFlags|CREATE_SUSPENDED, nStackSize, lpSecurityAttrs);
	pThread->Priority = nPriority;
#if !UCFG_USE_PTHREADS
	if (!(dwCreateFlags & CREATE_SUSPENDED))
		pThread->Resume();
#endif
	return pThread;
}


#endif



//!!!D Need for Win32s static long _afxTlsRef;

void AFXAPI AfxTlsAddRef() {
	//!!!D 	++_afxTlsRef;
}

void AFXAPI AfxTlsRelease() {
	//!!!D	if (_afxTlsRef == 0 || --_afxTlsRef == 0)
	{
		if (_afxThreadData) {
			_afxThreadData->~CThreadSlotData();
			_afxThreadData = NULL;
		}
	}
}

void CThreadSlotData::DeleteValues(CThreadData *pData, HINSTANCE hInst) {
	bool bDelete = true;
	for (int i = 1; i < pData->size(); i++) {
		if (!hInst || m_arSlotData[i].hInst == hInst) {
			// delete the data since hInst matches (or is NULL)
			delete (CNoTrackObject*)(*pData)[i];
			(*pData)[i] = 0;
		} else if ((*pData)[i])
			bDelete = false;
	}
	if (bDelete) {
		EXT_LOCK (m_criticalSection) {
			Remove(m_list, pData);
			m_tls.Value = 0;
		}
	}
}

#define SLOT_USED   0x01    // slot is allocated


void *CThreadSlotData::GetThreadValue(int nSlot) {
	ASSERT(nSlot && nSlot < m_nMax);
	ASSERT(m_arSlotData[nSlot].dwFlags & SLOT_USED);
	CThreadData *pData = (CThreadData*)(void*)m_tls.Value;
	if (!pData || nSlot >= pData->size())
		return 0;
	return (*pData)[nSlot];
}

CThreadLocalObject::~CThreadLocalObject() {
	if (m_nSlot != 0 && _afxThreadData)
		_afxThreadData->FreeSlot(m_nSlot);
	m_nSlot = 0;
}

void CThreadSlotData::FreeSlot(int nSlot) {
	EXT_LOCK (m_criticalSection) {
		ASSERT(nSlot && nSlot < m_nMax);
		ASSERT(m_arSlotData[nSlot].dwFlags & SLOT_USED);
		for (int i=0; i<m_list.size(); i++) {
			if (nSlot < m_list[i]->size()) {
				delete (CNoTrackObject*)(*m_list[i])[nSlot];
				(*m_list[i])[nSlot] = 0;
			}
		}
		m_arSlotData[nSlot].dwFlags &= ~SLOT_USED;
	}
}


int CThreadSlotData::AllocSlot() {
	EXT_LOCK (m_criticalSection) {
		int nAlloc = (int)m_arSlotData.size();
		int nSlot = m_nRover;
		if (nSlot >= nAlloc || (m_arSlotData[nSlot].dwFlags & SLOT_USED)) {
			// search for first free slot, starting at beginning
			for (nSlot = 1; nSlot < nAlloc && (m_arSlotData[nSlot].dwFlags & SLOT_USED); nSlot++)
				;

			// if none found, need to allocate more space
			if (nSlot >= nAlloc)
				m_arSlotData.resize(nSlot+1);
		}
		// adjust m_nMax to largest slot ever allocated
		if (nSlot >= m_nMax)
			m_nMax = nSlot+1;

		ASSERT(!(m_arSlotData[nSlot].dwFlags & SLOT_USED));
		m_arSlotData[nSlot].dwFlags |= SLOT_USED;
		// update m_nRover (likely place to find a free slot is next one)
		m_nRover = nSlot+1;

		return nSlot;   // slot can be used for FreeSlot, GetValue, SetValue
	}
}

void CThreadSlotData::SetValue(int nSlot, void *pValue) {
	ASSERT(nSlot && nSlot < m_nMax);
	ASSERT(m_arSlotData[nSlot].dwFlags & SLOT_USED);
	CThreadData *pData = (CThreadData*)(void*)m_tls.Value;
	if (!pData || nSlot >= pData->size() && pValue) {
		if (!pData) {
			pData = new CThreadData;
			EXT_LOCK (m_criticalSection) {
				m_list.push_back(pData);
			}
		}
		pData->resize(m_nMax);
		m_tls.Value = pData;
	}
	(*pData)[nSlot] = pValue;
}

CNoTrackObject* CThreadLocalObject::GetDataNA() {
	if (m_nSlot == 0 || _afxThreadData == NULL)
		return NULL;

	CNoTrackObject* pValue =
		(CNoTrackObject*)_afxThreadData->GetThreadValue(m_nSlot);
	return pValue;
}

void *CNoTrackObject::operator new(size_t nSize) {
#ifdef WIN32
	void* p = ::LocalAlloc(LPTR, nSize);
#else
	void* p = malloc(nSize);
#endif
	if (!p)
		Throw(E_OUTOFMEMORY);
	return p;
}

void CNoTrackObject::operator delete(void* p) {
	if (p)
#ifdef WIN32
		::LocalFree(p);
#else
		free(p);
#endif
}

CTls::CTls() {
#if UCFG_USE_PTHREADS
	PthreadCheck(::pthread_key_create(&m_key, 0));
#else
	Win32Check((m_key = ::TlsAlloc()) != TLS_OUT_OF_INDEXES);
#endif
}

CTls::~CTls() {
#if UCFG_USE_PTHREADS
	PthreadCheck(::pthread_key_delete(m_key));
#else
	Win32Check(::TlsFree(m_key));
#endif
}

void CTls::put_Value(const void *p) {
#if UCFG_USE_PTHREADS
	PthreadCheck(::pthread_setspecific(m_key, p));
#else
	Win32Check(::TlsSetValue(m_key, (void*)p));
#endif
}



BYTE __afxThreadData[sizeof(CThreadSlotData)];

CThreadSlotData *_afxThreadData;


//!!!__declspec(nothrow)
CThreadSlotData::CThreadSlotData()
	:	m_nRover(1)
	,	m_nMax(0)
{
}

void CThreadSlotData::DeleteValues(HINSTANCE hInst, bool bAll) {
	EXT_LOCK (m_criticalSection) {
		if (!bAll) {
			CThreadData* pData = (CThreadData*)(void*)m_tls.Value;
			if (pData)
				DeleteValues(pData, hInst);
		} else
			for (int i=0; i<m_list.size(); i++) 
				DeleteValues(m_list[i].get(), hInst);
	}
}

CThreadSlotData::~CThreadSlotData() {
	DeleteValues(0, true);
}

void CThreadSlotData::AssignInstance(HINSTANCE hInst) {
	EXT_LOCK (m_criticalSection) {
		ASSERT(hInst != NULL);

		for (int i = 1; i < m_arSlotData.size(); i++) {
			if (m_arSlotData[i].hInst == NULL && (m_arSlotData[i].dwFlags & SLOT_USED))
				m_arSlotData[i].hInst = hInst;
		}
	}
}

void AFXAPI AfxInitLocalData(HINSTANCE hInst) {
	if (_afxThreadData)
		_afxThreadData->AssignInstance(hInst);
}

void AFXAPI AfxTermLocalData(HINSTANCE hInst, bool bAll) {
	if (_afxThreadData)
		_afxThreadData->DeleteValues(hInst, bAll);
}

void CThreadLocalObject::AllocSlot() {
	if (m_nSlot == 0) {
		if (_afxThreadData == NULL) { //!!!
			_afxThreadData = new(__afxThreadData) CThreadSlotData;
			ASSERT(_afxThreadData != NULL);
		}
		m_nSlot = _afxThreadData->AllocSlot();
		ASSERT(m_nSlot != 0);
	}
}

CNoTrackObject* CThreadLocalObject::GetData(CNoTrackObject* (AFXAPI * pfnCreateObject)()) {
	AllocSlot();			//!!! Normally Already Alloced
	CNoTrackObject* pValue = (CNoTrackObject*)_afxThreadData->GetThreadValue(m_nSlot);
	if (!pValue) {
		// allocate zero-init object
		pValue = (*pfnCreateObject)();

		// set tls data to newly created object
		_afxThreadData->SetValue(m_nSlot, pValue);
		ASSERT(_afxThreadData->GetThreadValue(m_nSlot) == pValue);
	}
	return pValue;
}

#ifdef WIN32
DWORD AFXAPI WaitWithMS(DWORD dwMS, HANDLE h0, HANDLE h1, HANDLE h2, HANDLE h3, HANDLE h4, HANDLE h5) {
	DWORD r = WAIT_FAILED;
	if (!h1)
		r = WaitForSingleObject(h0, dwMS);
	else {
		HANDLE ar[6] = {h0, h1, h2, h3, h4, h5};
		int i;
		for (i=0; i<6; i++)
			if (!ar[i])
				break;
		r = WaitForMultipleObjects(i, ar, FALSE, dwMS);
	}
	Win32Check(r != WAIT_FAILED);
	return r;
}

DWORD AFXAPI Wait(HANDLE h0, HANDLE h1, HANDLE h2, HANDLE h3, HANDLE h4, HANDLE h5) {
	return WaitWithMS(INFINITE, h0, h1, h2, h3, h4, h5);
}
#endif




#ifdef _WIN32

#if UCFG_WIN32_FULL
void WorkItem::QueueAPC() {
	if (ExecutingThread)
		ExecutingThread->QueueAPC();
}
#endif

void WorkItem::Cancel() {
	if (!Finished) {
		Stop();
		if (ExecutingThread) {
			{
				EXT_LOCK (ExecutingThread->m_cs) {
					ExecutingThread->m_wiToCancel = this;
				}
			}
#if UCFG_WIN32_FULL
			ExecutingThread->QueueAPC();

#endif
		}
	}
}

PoolThread::PoolThread(ThreadPool& pool)
	:	base(&pool.m_tr)
	,	m_pool(pool)
{
}

void PoolThread::Execute() {
	Name = "PoolThread";

	while (true) {
		while (true) {
			if (m_bStop)
				return;
			ptr<WorkItem> wi;
			EXT_LOCK (ThreadPool::I->m_cs) {
				if (!ThreadPool::I->m_queue.empty()) {
					wi = ThreadPool::I->m_queue.front();
					ThreadPool::I->m_queue.pop();
				}
			}
			if (!wi)
				break;
			m_curWi = wi;
			wi->ExecutingThread = this;
			if (!wi->m_bStop) {
				try {
					wi->Execute();
				} catch (RCExc e) {
					wi->HResult = e.HResult;
				}
				wi->Finished = true;
				m_curWi = nullptr;
			}
		}
		EXT_LOCK (m_cs) {
			m_wiToCancel = nullptr;
		}
		int r = Wait(Handle(m_pool.m_evStop), Handle(ThreadPool::I->m_sem));
	}
}

void PoolThread::OnAPC() {
	if (m_wiToCancel == m_curWi)
		m_wiToCancel->OnAPC();
}


ThreadPool *ThreadPool::I;

ThreadPool::ThreadPool()
	:	m_evStop(FALSE, TRUE)
	,	m_sem(0, UCFG_POOL_THREADS)
{
	ASSERT(I == nullptr);
	I = this;

	for (int i=0; i<UCFG_POOL_THREADS; ++i)
		(new PoolThread(_self))->Start();
}

void ThreadPool::Ensure() {
	if (!I)
		AfxGetApp()->m_pThreadPool = new ThreadPool;
}



ptr<WorkItem> ThreadPool::QueueUserWorkItem(WorkItem *wi) {
	Ensure();
	EXT_LOCK (ThreadPool::I->m_cs) {
		ThreadPool::I->m_queue.push(wi);
		if (ThreadPool::I->m_queue.size() < UCFG_POOL_THREADS)
			ThreadPool::I->m_sem.Unlock();
	}
	return wi;
}
#endif  // _WIN32

bool g_bProcessDetached;

} // Ext::







