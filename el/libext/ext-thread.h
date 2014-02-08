/*######     Copyright (c) 1997-2013 Ufasoft  http://ufasoft.com  mailto:support@ufasoft.com,  Sergey Pavlov  mailto:dev@ufasoft.com #######################################
#                                                                                                                                                                          #
# This program is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation;  #
# either version 3, or (at your option) any later version. This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the      #
# implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details. You should have received a copy of the GNU #
# General Public License along with this program; If not, see <http://www.gnu.org/licenses/>                                                                               #
##########################################################################################################################################################################*/

#pragma once

#include EXT_HEADER(queue)

#define THREAD_PRIORITY_NORMAL          0 //!!! Windows
typedef struct tagMSG MSG;

namespace Ext {

class CWinThread;
class thread_group;

typedef const std::exception& RCExc;

HRESULT AFXAPI HResultInCatch(RCExc);

class PoolThread;
template <> struct ptr_traits<PoolThread> {
	typedef Interlocked interlocked_policy;
};

class AFX_MODULE_STATE;
class _AFX_THREAD_STATE;

class EXTAPI ThreadBase :
#if UCFG_WIN_MSG
	public CCmdTarget,
#endif
	public SafeHandle
{
	typedef ThreadBase class_type;
#if UCFG_WIN_MSG
	typedef CCmdTarget base;
#else
	typedef SafeHandle base;
#endif
public:
	using base::m_dwRef;
	typedef Interlocked interlocked_policy;

#if UCFG_WIN32
	CPointer<_AFX_THREAD_STATE> m_pAfxThreadState;

	_AFX_THREAD_STATE& AfxThreadState();

	class HandleAccess : protected SafeHandle::HandleAccess {
		typedef SafeHandle::HandleAccess base;
	public:
		HANDLE m_handle;

		HandleAccess(const ThreadBase& t) 
			:	m_handle(&t==TryGetCurrentThread() ? ::GetCurrentThread() : 0)
		{
			if (!m_handle) {
				m_hp = &t;
				AddRef();
			}
		}

		operator SafeHandle::handle_type() {
			if (m_handle)
				return m_handle;
			return base::operator SafeHandle::handle_type();
		}
	};
#endif

	EXT_DATA static size_t DefaultStackSize;

	CPointer<thread_group> m_owner;
	size_t StackSize;
	size_t StackOffset;
	String m_name;
#if UCFG_WIN32 && UCFG_OLE
	CUsingCOM m_usingCOM;
	DWORD m_dwCoInit;
#endif
private:
	CPointer<thread_group> m_threadRef;
	CPointer<AFX_MODULE_STATE> m_pModuleStateForThread;
protected:
#if !UCFG_WIN32 || !UCFG_STDSTL //!!!?
	mutable std::thread::id m_tid;
#endif
public:
#if UCFG_WIN32
	volatile UInt32 m_nThreadID;
#endif
	CInt<UInt32> m_exitCode;
#if !UCFG_WIN32 || !UCFG_STDSTL //!!!?
	std::thread::id get_id() const { return m_tid; }
#else
	UInt32 get_id() const { return m_nThreadID; }
#endif

	volatile bool m_bAutoDelete, m_bStop,
		m_bInterruptionEnabled,
		m_bAPC;

	ThreadBase(thread_group *ownRef = 0);
	~ThreadBase();

	virtual CWinThread *AsWinThread() { return 0; }
	virtual void OnEnd();
	virtual void Delete();

	void Attach(HANDLE h, DWORD id, bool bOwn = true);
	void AttachSelf();

//!!!#if UCFG_EXTENDED
	static ThreadBase* AFXAPI get_CurrentThread();
	static ThreadBase* AFXAPI TryGetCurrentThread();
	void SetCurrentThread();
	STATIC_PROPERTY_DEF_GET(ThreadBase, ThreadBase*, CurrentThread);
//!!!#endif

	bool Join(int ms = INFINITE);

	DWORD get_ExitCode();
	DEFPROP_GET(DWORD, ExitCode);

	String get_Name() const { return m_name; }
	void put_Name(RCString name);
	DEFPROP_CONST(String, Name);

	int get_Priority();
	void put_Priority(int nPriority);
	DEFPROP(int, Priority);

#if UCFG_USE_PTHREADS
	class CAttr {
		typedef CAttr class_type;
	public:
		CAttr() {
			PthreadCheck(::pthread_attr_init(&m_attr));
		}

		~CAttr() {
			PthreadCheck(::pthread_attr_destroy(&m_attr));
		}

		operator pthread_attr_t*() { return &m_attr; }
		operator const pthread_attr_t*() const { return &m_attr; }

		size_t get_StackSize() const {
			size_t r;
			PthreadCheck(::pthread_attr_getstacksize(&m_attr, &r));
			return r;
		}
		void put_StackSize(size_t v) {
			PthreadCheck(::pthread_attr_setstacksize(&m_attr, v));
		}
		DEFPROP(size_t, StackSize);
	private:
		pthread_attr_t m_attr;
	};
#else
	CONTEXT get_Context(DWORD contextFlags);
	CONTEXT get_Context() { return get_Context(CONTEXT_FULL); }
	void put_Context(const CONTEXT& context);
	DEFPROP(CONTEXT, Context);

	CTimesInfo get_Times() const;
	DEFPROP_GET_CONST(CTimesInfo, Times);

	TimeSpan get_UserProcessorTime() const {
		return Times.m_tmUser;
	}
	DEFPROP_GET_CONST(TimeSpan, UserProcessorTime);

	TimeSpan get_PrivilegedProcessorTime() const {
		return Times.m_tmKernel;
	}
	DEFPROP_GET_CONST(TimeSpan, PrivilegedProcessorTime);

	DWORD Resume();
	DWORD Suspend();
	void Terminate(DWORD dwExitCode = 1) { Win32Check(::TerminateThread(HandleAccess(*this), dwExitCode)); }
	EXT_API void PostMessage(UINT message, WPARAM wParam = 0, LPARAM lParam = 0);
#endif

	TimeSpan get_TotalProcessorTime() const
#if UCFG_USE_PTHREADS
		;
#else
	{
		return UserProcessorTime + PrivilegedProcessorTime;
	}
#endif
	DEFPROP_GET(TimeSpan, TotalProcessorTime);
	
	void Start(DWORD flags = 0);
	void interrupt();
	void interruption_point();
	virtual void WaitStop();

#if UCFG_OLE
	virtual void InitCOM();
#endif

	static void AFXAPI Sleep(DWORD dwMilliseconds);

#if UCFG_USE_PTHREADS
	bool Valid() const override {
		return m_tid != std::thread::id();
	}

	static void * __cdecl ThreaderFunction(void *pParam);
#else
	static UINT AFXAPI ThreaderFunction(LPVOID pParam);
#endif

#if UCFG_WIN32_FULL
	void QueueUserAPC(PAPCFUNC pfnAPC, ULONG_PTR dwData = 0);
	void QueueAPC();
#endif
	//!!!D	static CTls t_pCurThreader;//!!!
	thread_group& GetThreadRef();
	void SleepImp(DWORD dwMilliseconds);
protected:
#if UCFG_USE_PTHREADS
	mutable pthread_t m_ptid;
	mutable CBool m_bJoined;
	void ReleaseHandle(HANDLE h) const;
#endif

	virtual void BeforeStart() {}
	virtual void Execute();
	virtual void Stop(); //!!! for compatibility
	virtual void OnAPC();
#if UCFG_WIN32_FULL
	static VOID CALLBACK APCProc(ULONG_PTR param);
#endif

	EXT_API void Create(DWORD dwCreateFlags = 0, size_t nStackSize = 0
#ifdef WIN32
		, LPSECURITY_ATTRIBUTES lpSecurityAttrs = 0
#endif

	); //!!!

private:
	void OnEndThread(bool bDelete);
	UInt32 CppThreadThunk();
	friend class Process;
};

namespace this_thread {

bool AFXAPI interruption_enabled() noexcept;
bool AFXAPI interruption_requested() noexcept;
void AFXAPI interruption_point();
	
class disable_interruption : public Ext::CBoolKeeper {
	typedef Ext::CBoolKeeper base;
public:
	disable_interruption();
};

class restore_interruption : public Ext::CBoolKeeper {
	typedef Ext::CBoolKeeper base;
public:
	restore_interruption(disable_interruption& di);
};

void AFXAPI sleep_for(const TimeSpan& span);

} // Ext::this_thread::

class thread_group : noncopyable {
public:
	CEvent m_evFinal;
	//!!!         m_evFinish;
	volatile Int32 m_dwRef;
	volatile Int32 RefCountActiveThreads;

	thread_group(bool bSync = true)
		:	m_dwRef(0)
		,	m_bSync(bSync)
		,	RefCountActiveThreads(0)
	{}

	virtual ~thread_group();	
	int size() const;
	void add_thread(ThreadBase *t);
	void remove_thread(ThreadBase *t);

	void ExternalAddRef() { Interlocked::Increment(m_dwRef);}
	void ExternalRelease() {
		if (!Interlocked::Decrement(m_dwRef))
			OnFinalRelease();      
	}

	void OnFinalRelease() {
		m_evFinal.Set();
	}

	void StopChilds();
	bool StopChild(ThreadBase *t, int msTimeout = INFINITE);
	void interrupt_all();
	void join_all();
protected:
	mutable CCriticalSection m_cs;				// Often locked from static ctr/dtor, which not allowed by VC std::mutex implementation
	typedef std::unordered_set<ptr<ThreadBase> > CThreadColl;
	CThreadColl m_threads;
public:
	bool m_bSync;
};


AFX_API void AFXAPI AfxTermThread(HINSTANCE hInstTerm = 0);

struct CSlotData {
	HINSTANCE hInst;    // module which owns this slot
	DWORD dwFlags;      // slot flags (allocated/not allocated)

	CSlotData()
		:	hInst(0)
		,	dwFlags(0)
	{}
};

class CThreadData : public std::vector<CNoTrackObject*> {
	typedef std::vector<CNoTrackObject*> base;
public:
	std::thread::id ThreadId;

	CThreadData()
		:	base(10)											//!!!?
		,	ThreadId(std::this_thread::get_id())
	{}	
};	

class AFX_CLASS CThreadSlotData {
public:
	CTls m_tls;

	typedef std::unordered_map<std::thread::id, CThreadData> CTDatas;
	CTDatas m_tdatas;  // list of CThreadData structures

	std::vector<CSlotData> m_arSlotData;

	int m_nRover;       // (optimization) for quick finding of free slots
	int m_nMax;         // size of slot table below (in bits)

	CThreadSlotData();
	~CThreadSlotData();

	int AllocSlot();
	void FreeSlot(int nSlot);
	void* GetValue(int nSlot);
	void SetValue(int nSlot, CNoTrackObject* pValue);	
	void DeleteValues(HINSTANCE hInst, bool bAll = false);						// delete all values in process/thread
	void AssignInstance(HINSTANCE hInst);										// assign instance handle to just constructed slots
	void* GetThreadValue(int nSlot);											// special version for threads only!

	void* PASCAL operator new(size_t, void* p) { return p; }
private:
	CCriticalSection m_criticalSection;

	void DeleteValues(CThreadData* pData, HINSTANCE hInst);
};

extern CThreadSlotData *_afxThreadData;

AFX_API void AFXAPI AfxInitLocalData(HINSTANCE hInst);
AFX_API void AFXAPI AfxTermLocalData(HINSTANCE hInst, bool bAll = false);

class CSeparateThread;

class CSeparateThreadThreader : public ThreadBase {
	CSeparateThread& m_st;
public:
	CSeparateThreadThreader(CSeparateThread& st);
	~CSeparateThreadThreader();
protected:
	void BeforeStart() override;
	void Execute() override;
	void Stop() override;

friend class CSeparateThread;
};

class CSeparateThread : public thread_group {
public:
	ptr<CSeparateThreadThreader> m_pT;

	~CSeparateThread();
	void Create(DWORD flags = 0);

	/*!!!
	void Sleep(DWORD dwMilliseconds = SLEEP_TIME)
	{
	m_pT->Sleep(dwMilliseconds);
	}*/

protected:
	virtual void Stop();
	virtual void BeforeStart() {}
	virtual void Execute() {}
	friend class CSeparateThreadThreader;
};

const UINT ERR_UNHANDLED_EXCEPTION = 254;
AFX_API void AFXAPI ProcessExceptionInExcept();
AFX_API void AFXAPI ProcessExceptionInCatch();


#ifdef WIN32

class CWinThread : public ThreadBase {
public:
	CPointer<CWnd> m_pMainWnd;
	CPointer<CWnd> m_pActiveWnd;     // active main window (may not be m_pMainWnd)
	LPVOID m_pThreadParams;
	AFX_THREADPROC m_pfnThreadProc;
	void (*m_lpfnOleTermOrFreeLib)();

	CWinThread(thread_group *ownRef = 0);

	CWinThread(AFX_THREADPROC pfnThreadProc, LPVOID pParam);
	void CommonConstruct();

	CWinThread *AsWinThread() { return this; }

	virtual int InitInstance() { return FALSE; }
	virtual int ExitInstance();

#if UCFG_WIN_MSG
	virtual bool IsIdleMessage(MSG& msg);

	bool DispatchThreadMessageEx(MSG& msg);  // helper
	virtual LRESULT ProcessWndProcException(RCExc e, const MSG *pMsg);
	virtual CWnd *GetMainWnd();
	virtual bool PreTranslateMessage(MSG& msg);
	virtual bool PumpMessage();
	int RunLoop();


#	if UCFG_GUI
	virtual bool OnIdle(LONG lCount);

#		if !UCFG_WCE
	virtual BOOL ProcessMessageFilter(int code, MSG *lpMsg);
#		endif

#	endif

#endif

protected:
	HRESULT m_hr;

#if UCFG_EXTENDED  
	void Execute();
	void OnEnd();
#endif
};

#endif	// WIN32


int AFXAPI GetThreadNumber();

class Thread
#ifdef WIN32
	:	public CWinThread
#else
	:	public ThreadBase
#endif
{
#ifdef WIN32
	typedef CWinThread base;
#else
	typedef ThreadBase base;
#endif

public:
	Thread(thread_group *ownRef = 0)
		:	base(ownRef)
	{}

	void Execute() override {
		ThreadBase::Execute();
	}
};


#ifdef _WIN32

class WorkItem : public Object {
public:
	typedef Interlocked interlocked_policy;

	ptr<PoolThread> ExecutingThread;
	HRESULT HResult;
	CBool Finished, m_bStop;

	WorkItem()
		:	HResult(S_OK)
	{}

#if UCFG_WIN32_FULL
	void QueueAPC();
#endif
	void Cancel();
protected:
	virtual void Execute() {}
	
	virtual void Stop() {
		m_bStop = true;
	}

	virtual void OnAPC() {}

friend class ThreadPool;
friend class PoolThread;
};

template <typename T>
class TWorkItem : public WorkItem {
public:
	T m_fun;
protected:
	virtual void Execute() {
		m_fun();
	}
};

template <typename T>
class PWorkItem : public WorkItem {
public:
	ptr<T> m_pfun;
protected:
	virtual void Execute() {
		m_pfun->operator()();
	}
};

class ThreadPool;

class PoolThread : public Thread {
	typedef Thread base;
public:
	ThreadPool& m_pool;

	CCriticalSection m_cs;
	ptr<WorkItem> m_wiToCancel, m_curWi;

	PoolThread(ThreadPool& pool);
protected:
	void Execute() override;
	void OnAPC() override;
};


class ThreadPool : public Object {
public:
	static ThreadPool *I;

	ThreadPool();

	~ThreadPool() {
		m_evStop.Set();
		m_tr.StopChilds();
		I = nullptr;
	}

/*!!!	template <typename T>
	static ptr<WorkItem> QueueUserWorkItem(const T& fun) {
		TWorkItem<T> *wi = new TWorkItem<T>;
		wi->m_fun = fun;
		return QueueUserWorkItem(ptr<WorkItem>(wi));
	}*/

	template <typename T>
	static ptr<WorkItem> QueueUserWorkItem(const ptr<T>& pfun) {
		PWorkItem<T> *wi = new PWorkItem<T>;
		wi->m_pfun = pfun;
		return QueueUserWorkItem(ptr<WorkItem>(wi));
	}

	static AFX_API ptr<WorkItem> AFXAPI QueueUserWorkItem(WorkItem* wi);

	static AFX_API ptr<WorkItem> AFXAPI QueueUserWorkItem(const ptr<WorkItem>& wi) {
		return QueueUserWorkItem(wi.get());
	}
protected:
	thread_group m_tr;
	CEvent m_evStop;
	CSemaphore m_sem;
	
	CCriticalSection m_cs;
	std::queue<ptr<WorkItem>> m_queue;

	static void Ensure();
	
friend class PoolThread;
};
#endif // _WIN32

extern bool g_bProcessDetached;


} // Ext::

