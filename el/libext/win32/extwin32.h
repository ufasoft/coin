/*######     Copyright (c) 1997-2013 Ufasoft  http://ufasoft.com  mailto:support@ufasoft.com,  Sergey Pavlov  mailto:dev@ufasoft.com #######################################
#                                                                                                                                                                          #
# This program is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation;  #
# either version 3, or (at your option) any later version. This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the      #
# implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details. You should have received a copy of the GNU #
# General Public License along with this program; If not, see <http://www.gnu.org/licenses/>                                                                               #
##########################################################################################################################################################################*/

#pragma once

#include <el/libext/win32/ext-win.h>

#include <manufacturer.h>

#include <shellapi.h>

namespace Ext {

class ThreadBase;
class CAppBase;
class CMenu;
class COccManager;
class CToolTipCtrl;
class CControlBarBase;

#if UCFG_WCE
#pragma comment(lib, "ole32.lib")
//!!!	#pragma comment(lib, "wininet.lib")
#pragma comment(lib, "oleaut32.lib")
#else
#pragma comment(lib, "advapi32.lib")
#pragma comment(lib, "version.lib")
#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "mswsock.lib") 
//!!!#pragma comment(lib, "winmm.lib")
//!!!#pragma comment(lib, "msacm32.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "comdlg32.lib")
#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "htmlhelp.lib")
#pragma comment(lib, "shlwapi.lib")
#endif

//!!!#pragma comment(linker, "/OPT:NOWIN98")

//!!!#pragma check_stack(off)


#ifndef _USING_EXT
#	define _USING_EXT
#endif

class CThreadRef;
class Socket;
class DateTime;
class COleVariant;
class CRegistryValue;
class CRegistryValuesIterator;
//!!!R class CPersistent;
struct AFX_CMDHANDLERINFO;
struct AFX_DISPMAP_ENTRY;
struct AFX_EVENTSINKMAP_ENTRY;
struct AFX_MSGMAP_ENTRY;
class AFX_MODULE_STATE;
struct _ATL_OBJMAP_ENTRY;
class _AFX_THREAD_STATE;
class CMessageProcessor;
class CComClass;
class CComClassFactoryImpl;
class CComGeneralClass;
class CComTypeLibHolder;
class COleControlSite;
class COleControlContainer;
class CComObjectRootBase;
class RegistryKey;
class Stream;
class CBitmap;
class CBrush;
class CCmdTarget;
class CDC;
class CDocManager;
class CDocTemplate;
class CDocument;
class CDynLinkLibrary;
class File;
class CFont;
class CFrameWnd;
class CGdiObject;
class ImageList;
class CPalette;
class CListBox;
class CRuntimeClass;
class COleObjectFactory;
class CResID;
class CView;
class CWinApp;
class CWinThread;
class CWnd;





} // Ext::

#include <el/libext/datetime.h>

namespace Ext {



class AFX_CLASS CMultiLock
{
public:
	CMultiLock(CSyncObject* pObjects[], DWORD dwCount, bool bInitialLock = false);
	~CMultiLock();
	DWORD Lock(DWORD dwTimeOut = INFINITE, bool bWaitForAll = true, DWORD dwWakeMask = 0);
	void Unlock();
	BOOL Unlock(Int32 lCount, Int32 *lpPrevCount = 0);
	BOOL IsLocked(DWORD dwObject);
protected:
	CSyncObject ** m_ppObjectArray;
//!!!R	HANDLE* m_pHandleArray;
	bool*   m_bLockedArray;
	DWORD   m_dwCount;
};














AFX_API String AFXAPI ToShortPath(RCString path);

//!!!D AFX_API String AFXAPI ExtractFilePath(RCString path);
//!!!DAFX_API String AFXAPI GetFullPathNameEx(RCString s);
//!!!AFX_API bool AFXAPI DirectoryExists(RCString path);
//!!!AFX_EXT_API HRESULT ProcessOleException(CException *e);
//!!!AFX_EXT_API istream& operator >>(istream& is, String& s);
//AFX_EXT_API ostream& operator <<(ostream& os, const String& s);
AFX_API void AFXAPI SelfRegisterDll(RCString path);
AFX_API void AFXAPI WinExecAndWait(RCString name);
AFX_API HCURSOR AFXAPI GetHandCursor();



//!!!const THREAD_TIMEOUT = 5000;


#undef AFX_DATA
#define AFX_DATA AFX_CORE_DATA





LRESULT CALLBACK AfxWndProcBase(HWND hWnd, UINT nMsg, WPARAM wParam, LPARAM lParam);
AFX_API LRESULT AFXAPI AfxWndProc(const MSG& msg);




class AFX_CLASS CWaitCursor
{
public:
	CWaitCursor();
	~CWaitCursor();
	static void AFXAPI Restore();
};


// special struct for WM_SIZEPARENT
struct AFX_SIZEPARENTPARAMS
{
	HDWP hDWP;
	Rectangle rect;       // parent client rectangle (trim as appropriate)
	SIZE sizeTotal;  // total size on each side as layout proceeds
	BOOL bStretch;   // should stretch to fill all space
};



AFX_API void * AFXAPI AfxGetResource(const CResID& resID, const CResID& lpszType);


AFX_API bool AFXAPI AfxHasResourceString(UINT nIDS);
AFX_API bool AFXAPI AfxHasResource(const CResID& lpszName, const CResID& lpszType);











} // Ext::


namespace Ext {

#define THIS_FILE          __FILE__

	/*!!!R
#if UCFG_EXTENDED

#	if !defined(ASSERT) && defined(_DEBUG) //!!!
#	define ASSERT(f) \
do \
{ \
if (!(f) && AfxAssertFailedLine(THIS_FILE, __LINE__)) \
AfxDebugBreak(); \
} while (0) \

#	endif

#endif

#ifndef ASSERT //!!!
#	define ASSERT(f) ((void*)0)
#endif
*/





#define ASSERT_POINTER(p, type) \
	ASSERT(((p) != NULL) && AfxIsValidAddress((p), sizeof(type), FALSE))

#define ASSERT_NULL_OR_POINTER(p, type) \
	ASSERT(((p) == NULL) || AfxIsValidAddress((p), sizeof(type), FALSE))

#define IS_COMMAND_ID(nID)  ((nID) & 0x8000)

class AFX_CLASS CCmdUI        // simple helper class
{
public:
	UINT m_nID;
	UINT m_nIndex;          // menu item or other index
	UINT m_nIndexMax;

	// if a menu item
	CPointer<CMenu> m_pMenu;         // NULL if not a menu
	CPointer<CMenu> m_pSubMenu;      // sub containing menu item
	CPointer<CMenu> m_pParentMenu;
	CPointer<CWnd> m_pOther;

	CBool m_bContinueRouting;
	CBool m_bEnableChanged;

	CCmdUI();
	bool DoUpdate(CCmdTarget* pTarget, BOOL bDisableIfNoHndler);
	virtual void Enable(bool bOn = true);
	virtual void SetCheck(int nCheck = 1);   // 0, 1 or 2 (indeterminate)
	virtual void SetRadio(bool bOn = true);
#if !UCFG_WCE
	virtual void SetText(LPCTSTR lpszText);
#endif
};




#if UCFG_WCE

#define MUTEX_ALL_ACCESS MUTANT_ALL_ACCESS //!!!
#define PSH_WIZARD97            0x00002000
#define PROCESSOR_ARCHITECTURE_AMD64            9
#define PROCESSOR_ARCHITECTURE_IA32_ON_WIN64    10
#define HINSTANCE_ERROR 32

#ifndef INVALID_FILE_ATTRIBUTES
#define INVALID_FILE_ATTRIBUTES ((DWORD)-1)
#endif

#ifndef MF_DISABLED
#define MF_DISABLED         0x00000002L
#endif

#ifndef MF_BITMAP
#define MF_BITMAP           0x00000004L
#endif

#define SM_DBCSENABLED          42
#define DS_NOIDLEMSG        0x100L  /* WM_ENTERIDLE message will not be sent */
#define WM_ENTERIDLE                    0x0121
#define MSGF_DIALOGBOX      0
#define WM_NCMOUSEMOVE                  0x00A0
#define BUFSIZ  512
const std::streamoff _BADOFF = -1;	
#define _IOFBF          0x0000
#define IDI_APPLICATION     MAKEINTRESOURCE(32512)


#else

//!!!D using ATL::COleDateTime;


#endif // UCFG_WCE







struct AFX_OLDTOOLINFO //!!!
{
	UINT cbSize;
	UINT uFlags;
	HWND hwnd;
	UINT uId;
	RECT rect;
	HINSTANCE hinst;
	LPTSTR lpszText;
};

#define ID_TIMER_WAIT   0xE000  // timer while waiting to show status
#define ID_TIMER_CHECK  0xE001  // timer to check for removal of status






class AFX_CLASS CCursor {
public:
	static Point AFXAPI get_Position();
	static void AFXAPI put_Position(const POINT& pt);
	DEFPROP(Point, Position);

	static int AFXAPI Show(bool bShow) { return ::ShowCursor(bShow); }

	static void AFXAPI Clip(const RECT& rect) { Win32Check(::ClipCursor(&rect)); }
};

//!!!extern EXT_DATA CProcess Process;
extern EXT_DATA CCursor Cursor;
//!!!extern EXT_DATA int g_nTraceLevel;

//!!! extern EXT_DATA LPTOP_LEVEL_EXCEPTION_FILTER g_pfnUnhadledExceptionFilter;


class CShellBrowseDialog //!!!
{
public:
	CWnd *m_parent;
	UINT m_flags;
	String m_path,
		m_title,
		m_displayName;

	CShellBrowseDialog(CWnd *parent = 0);
	bool DoModal();
};


class CSeparateThread;




bool AFXAPI InputQuery(RCString caption, RCString promt, String& s);


#pragma pack(push, 1)

struct DLGTEMPLATEEX
{
	WORD dlgVer;
	WORD signature;
	DWORD helpID;
	DWORD exStyle;
	DWORD style;
	WORD cDlgItems;
	short x;
	short y;
	short cx;
	short cy;
};

struct DLGITEMTEMPLATEEX
{
	DWORD helpID;
	DWORD exStyle;
	DWORD style;
	short x;
	short y;
	short cx;
	short cy;
	DWORD id;
};

#pragma pack(pop)

} // Ext::

//#include "DateTime.h"

namespace Ext {

	/*!!!R
inline std::ostream& AFXAPI operator<<(std::ostream& os, const ConstBuf& mb) {
	os << "{ ";
	for (size_t i=0; i<mb.Size; ++i)
		os << std::hex << std::setw(2) << std::setfill('0') << int(mb.P[i]) << " ";
	return os << "}";
}

*/


















extern String g_ExceptionMessage;






} // Ext::


extern "C" {
IMPEXP_API void __cdecl My_except_handler3();
IMPEXP_API void __cdecl My_except_handler4();
IMPEXP_API void __cdecl My_SEH_prolog();
IMPEXP_API void __cdecl My_SEH_epilog();
IMPEXP_API void __cdecl My_SEH_prolog4();
IMPEXP_API void __cdecl My_SEH_epilog4();
IMPEXP_API void __cdecl My_EH_prolog2(void *handler);
IMPEXP_API void __cdecl My__ehvec_copy_ctor();
IMPEXP_API void __fastcall My__security_check_cookie(uintptr_t);
LONG WINAPI My__CxxSetUnhandledExceptionFilter();
}
