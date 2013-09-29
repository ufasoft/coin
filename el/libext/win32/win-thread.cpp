/*######     Copyright (c) 1997-2013 Ufasoft  http://ufasoft.com  mailto:support@ufasoft.com,  Sergey Pavlov  mailto:dev@ufasoft.com #######################################
#                                                                                                                                                                          #
# This program is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation;  #
# either version 3, or (at your option) any later version. This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the      #
# implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details. You should have received a copy of the GNU #
# General Public License along with this program; If not, see <http://www.gnu.org/licenses/>                                                                               #
##########################################################################################################################################################################*/

#include <el/ext.h>

#include <ddeml.h>

#if UCFG_EXTENDED
#	if !UCFG_WCE
#		include <eh.h>
#	endif
#	if UCFG_GUI
#		include <el/gui/controls.h>
#	endif
#endif
//!!!#include <exception>//!!!



namespace Ext {

using namespace std;

#if UCFG_EXTENDED

int CWinThread::ExitInstance() {
#if UCFG_WIN_MSG
	return (int)AfxGetCurrentMessage()->wParam;
#else
	return true;
#endif
}

#if UCFG_WIN_MSG

class CWndAttachKeeper {
	CWnd& m_wnd;
public:
	CWndAttachKeeper(CWnd& wnd, HWND hwnd)
		:	m_wnd(wnd)
	{
		m_wnd.Attach(hwnd);
	}

	~CWndAttachKeeper() {
		m_wnd.Detach();
	}
};
#endif


void CWinThread::Execute() {
#if UCFG_WIN_MSG
	//!!!auto_ptr<CEvent> event2(pStartup->m_pEvent2);
	CWnd threadWnd;

	// forced initialization of the thread
	AfxInitThread();

	// thread inherits app's main window if not already set
	CWinApp* pApp = AfxGetApp();
	HWND hwnd = 0;
	if (pApp && !m_pMainWnd && pApp->m_pMainWnd->GetSafeHwnd())
	{
		// just attach the HWND
		hwnd = pApp->m_pMainWnd->m_hWnd;
		m_pMainWnd = &threadWnd;
	}
	CWndAttachKeeper wndAttachKeeper(threadWnd, hwnd);
	/*!!!  catch (RCExc e)
	{
	threadWnd.Detach();
	m_hr = e.HResult;
	//!!!      pStartup->m_hr = hr;
	//!!!      pStartup->m_event.Set();
	//!!!    bDelete = false; //!!!
	//    m_exitCode = (UINT)-1;
	throw;
	}
	*/
	//!!!    pStartup->m_event.Set();
	//!!!event2->Lock();
	// first -- check for simple worker thread
	if (m_pfnThreadProc)
		m_exitCode = (*m_pfnThreadProc)(m_pThreadParams);
	else if (!InitInstance())
		m_exitCode = ExitInstance();
	else
		RunLoop();
#endif
}

void CWinThread::OnEnd() {
	if (m_lpfnOleTermOrFreeLib)
		m_lpfnOleTermOrFreeLib();
	ThreadBase::OnEnd();
}

#endif // UCFG_EXTENDED

#if UCFG_WIN_MSG
#	include "win32/mfcimpl.h"


MSG * AFXAPI AfxGetCurrentMessage() {
	return &AfxGetThreadState()->m_msgCur;
}



bool CWinThread::IsIdleMessage(MSG& msg) {
	if (msg.message == WM_MOUSEMOVE || msg.message == WM_NCMOUSEMOVE) {
		return true;
	}

	// WM_PAINT and WM_SYSTIMER (caret blink)
	return msg.message != WM_PAINT && msg.message != 0x0118;
}


bool CWinThread::DispatchThreadMessageEx(MSG& msg) {
	const AFX_MSGMAP* pMessageMap; pMessageMap = GetMessageMap();
	const AFX_MSGMAP_ENTRY* lpEntry;

	for (; pMessageMap->pfnGetBaseMap; pMessageMap=(*pMessageMap->pfnGetBaseMap)())
	{
		// Note: catch not so common but fatal mistake!!
		//      BEGIN_MESSAGE_MAP(CMyThread, CMyThread)
		ASSERT(pMessageMap != (*pMessageMap->pfnGetBaseMap)());

		if (msg.message < 0xC000)
		{
			// constant window message
			if ((lpEntry = AfxFindMessageEntry(pMessageMap->lpEntries,
				msg.message, 0, 0)) != NULL)
				goto LDispatch;
		}
		else
		{
			// registered windows message
			lpEntry = pMessageMap->lpEntries;
			while ((lpEntry = AfxFindMessageEntry(lpEntry, 0xC000, 0, 0)) != NULL)
			{
				UINT* pnID = (UINT*)(lpEntry->nSig);
				ASSERT(*pnID >= 0xC000);
				// must be successfully registered
				if (*pnID == msg.message)
					goto LDispatch;
				lpEntry++;      // keep looking past this one
			}
		}
	}
	return FALSE;

LDispatch:
	union MessageMapFunctions mmf;
	mmf.pfn_THREAD = 0;
	mmf.pfn = lpEntry->pfn;

	// always posted, so return value is meaningless

	(this->*mmf.pfn_THREAD)(msg.wParam, msg.lParam);
	return TRUE;
}

BOOL AfxInternalPreTranslateMessage(MSG* pMsg)
{
	//	ASSERT_VALID(this);

	CWinThread *pThread = AfxGetThread();
	if( pThread )
	{
		// if this is a thread-message, short-circuit this function
		if (pMsg->hwnd == NULL && pThread->DispatchThreadMessageEx(*pMsg))
			return TRUE;
	}

	// walk from target to main window
	CWnd* pMainWnd = AfxGetMainWnd();
	if (CWnd::WalkPreTranslateTree(pMainWnd->GetSafeHwnd(), pMsg))
		return TRUE;

	// in case of modeless dialogs, last chance route through main
	//   window's accelerator table
	if (pMainWnd != NULL)
	{
		CWnd* pWnd = CWnd::FromHandle(pMsg->hwnd);
		if (pWnd->GetTopLevelParent() != pMainWnd)
			return pMainWnd->PreTranslateMessage(pMsg);
	}

	return FALSE;   // no special processing
}

static BOOL AfxPreTranslateMessage(MSG* pMsg) {
	if (CWinThread *pThread = AfxGetThread())
		return pThread->PreTranslateMessage(*pMsg);
	else
		return AfxInternalPreTranslateMessage( pMsg );
}

static BOOL AfxInternalPumpMessage()
{
	_AFX_THREAD_STATE *pState = AfxGetThreadState();

	if (!::GetMessage(&(pState->m_msgCur), NULL, NULL, NULL))
	{
		/*!!!
		#ifdef _DEBUG
		TRACE(traceAppMsg, 1, "CWinThread::PumpMessage - Received WM_QUIT.\n");
		pState->m_nDisablePumpCount++; // application must die
		#endif
		*/
		// Note: prevents calling message loop things in 'ExitInstance'
		// will never be decremented
		return FALSE;
	}

#ifdef _DEBUG
	/*!!!
	if (pState->m_nDisablePumpCount != 0)
	{
	TRACE(traceAppMsg, 0, "Error: CWinThread::PumpMessage called when not permitted.\n");
	ASSERT(FALSE);
	}
	*/
#endif

#ifdef _DEBUG
	//!!!	_AfxTraceMsg(_T("PumpMessage"), &(pState->m_msgCur));
#endif

	// process this message

	if (pState->m_msgCur.message != WM_KICKIDLE && !AfxPreTranslateMessage(&(pState->m_msgCur)))
	{
		::TranslateMessage(&(pState->m_msgCur));
		::DispatchMessage(&(pState->m_msgCur));
	}
	return TRUE;
}

bool CWinThread::PreTranslateMessage(MSG& msg) {
	// if this is a thread-message, short-circuit this function
	if (msg.hwnd == NULL && DispatchThreadMessageEx(msg))
		return true;
	// walk from target to main window
	CWnd *pMainWnd = AfxGetMainWnd();
	if (CWnd::WalkPreTranslateTree(pMainWnd->GetSafeHwnd(), &msg))
		return true;
	// in case of modeless dialogs, last chance route through main
	//   window's accelerator table
	if (pMainWnd != NULL)
	{
		CWnd* pWnd = CWnd::FromHandle(msg.hwnd);
		if (pWnd->GetTopLevelParent() != pMainWnd)
			return pMainWnd->PreTranslateMessage(&msg);
	}
	return false;
}

bool CWinThread::PumpMessage() {
	return AfxInternalPumpMessage();
}

BOOL AFXAPI AfxPumpMessage() {
	if (CWinThread *pThread = AfxGetThread())
		return pThread->PumpMessage();
	else
		return AfxInternalPumpMessage();
}

int CWinThread::RunLoop() {
	bool bIdle = true;
	LONG lIdleCount = 0;
	_AFX_THREAD_STATE* pState = AfxGetThreadState();
	while (true)
	{
#if UCFG_GUI
		while (bIdle && !::PeekMessage(&(pState->m_msgCur), 0, 0, 0, PM_NOREMOVE))
			bIdle = OnIdle(lIdleCount++);
#endif
		do
		{
			//#ifndef NO_GUI //!!!
			if (!PumpMessage())
				return ExitInstance();
			//#endif

			if (IsIdleMessage(pState->m_msgCur))
			{
				bIdle = true;
				lIdleCount = 0;
			}
		}
		while (::PeekMessage(&(pState->m_msgCur), NULL, NULL, NULL, PM_NOREMOVE));
	}
}


#endif // UCFG_WIN_MSG

bool CWinThread::OnIdle(LONG lCount)
{
	if (lCount <= 0)
	{
		if (m_pMainWnd && m_pMainWnd->m_hWnd && m_pMainWnd->IsVisible())
		{
			MSG msg = {m_pMainWnd->m_hWnd, WM_IDLEUPDATECMDUI, TRUE, 0};
			AfxCallWndProc(m_pMainWnd, msg);
#if !UCFG_WCE
			m_pMainWnd->SendMessageToDescendants(WM_IDLEUPDATECMDUI, TRUE, 0, TRUE, TRUE);
#endif
		}
	}
	return lCount < 0;
}

#if !UCFG_WCE

BOOL CWinThread::ProcessMessageFilter(int code, MSG *lpMsg) {
	if (lpMsg == NULL)
		return FALSE;   // not handled

	//!!!	CFrameWnd* pTopFrameWnd;
	CWnd* pMainWnd;
	CWnd* pMsgWnd;
	switch (code)
	{
	case MSGF_DDEMGR:
		// Unlike other WH_MSGFILTER codes, MSGF_DDEMGR should
		//  never call the next hook.
		// By returning FALSE, the message will be dispatched
		//  instead (the default behavior).
		return FALSE;

	case MSGF_MENU:
		pMsgWnd = CWnd::FromHandle(lpMsg->hwnd);
		/*!!!		if (pMsgWnd != NULL)
		{
		pTopFrameWnd = pMsgWnd->GetTopLevelFrame();
		if (pTopFrameWnd != NULL && pTopFrameWnd->IsTracking() &&
		pTopFrameWnd->m_bHelpMode)
		{
		pMainWnd = AfxGetMainWnd();
		if ((m_pMainWnd != NULL) && (IsEnterKey(lpMsg) || IsButtonUp(lpMsg)))
		{
		pMainWnd->SendMessage(WM_COMMAND, ID_HELP);
		return TRUE;
		}
		}
		}*/
		// fall through...

	case MSGF_DIALOGBOX:    // handles message boxes as well.
		pMainWnd = AfxGetMainWnd();
		/*!!!		if (afxData.nWinVer < 0x333 && pMainWnd != NULL && IsHelpKey(lpMsg))
		{
		pMainWnd->SendMessage(WM_COMMAND, ID_HELP);
		return TRUE;
		}*/
		if (code == MSGF_DIALOGBOX && m_pActiveWnd != NULL &&
			lpMsg->message >= WM_KEYFIRST && lpMsg->message <= WM_KEYLAST)
		{
			// need to translate messages for the in-place container
			_AFX_THREAD_STATE* pThreadState = _afxThreadState.GetData();
			if (pThreadState->m_bInMsgFilter)
				return FALSE;
			pThreadState->m_bInMsgFilter = TRUE;    // avoid reentering this code
			MSG msg = *lpMsg;
			if (m_pActiveWnd->IsEnabled() && PreTranslateMessage(msg))
			{
				pThreadState->m_bInMsgFilter = FALSE;
				return TRUE;
			}
			pThreadState->m_bInMsgFilter = FALSE;    // ok again
		}
		break;
	}
	return FALSE;   // default to not handled
}

static LRESULT CALLBACK _AfxMsgFilterHook(int code, WPARAM wParam, LPARAM lParam) {
	CWinThread* pThread;
	if (AfxGetModuleState()->m_bDLL || (code < 0 && code != MSGF_DDEMGR) || !(pThread = AfxGetThread()))
		return AfxGetThreadState()->m_hookMsg.CallNext(code, wParam, lParam);
	ASSERT(pThread != NULL);
	return (LRESULT)pThread->ProcessMessageFilter(code, (LPMSG)lParam);
}

#endif // !UCFG_WCE

void AFXAPI AfxInitThread() {
#if !UCFG_WCE
	if (!AfxGetModuleState()->m_bDLL)
		AfxGetThreadState()->m_hookMsg.Set(WH_MSGFILTER, _AfxMsgFilterHook, 0, ::GetCurrentThreadId());
#endif
}


} // Ext::

