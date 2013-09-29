/*######     Copyright (c) 1997-2013 Ufasoft  http://ufasoft.com  mailto:support@ufasoft.com,  Sergey Pavlov  mailto:dev@ufasoft.com #######################################
#                                                                                                                                                                          #
# This program is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation;  #
# either version 3, or (at your option) any later version. This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the      #
# implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details. You should have received a copy of the GNU #
# General Public License along with this program; If not, see <http://www.gnu.org/licenses/>                                                                               #
##########################################################################################################################################################################*/

#pragma once


#include <winbase.h>
#include <winuser.h>
#include <winver.h>

#if !UCFG_WCE
#	include <wincon.h>
#endif

#include <psapi.h>

#if UCFG_COM && defined(_MSC_VER) && !UCFG_MINISTL
//!!!R#	include <windows.h>
		//#undef lstrlenW // because used ::lstrlenW
#	undef lstrlen
#	ifdef _UNICODE
#		define lstrlen wcslen
#	else
#		define lstrlen strlen
#	endif
#	include <comdef.h> // must be after comment(lib, "ext.lib") to override som COM-support funcs

#	if !UCFG_STDSTL && defined(_M_IX86)
#		pragma comment(linker, "/NODEFAULTLIB:comsuppwd.lib")
#		pragma comment(linker, "/NODEFAULTLIB:comsuppw.lib")
#		pragma comment(linker, "/NODEFAULTLIB:comsuppd.lib")
#		pragma comment(linker, "/NODEFAULTLIB:comsupp.lib")

#		pragma comment(lib, "\\src\\foreign\\lib\\o_comsuppw.lib")
#	endif
#endif


#ifndef WM_NCDESTROY
#	define WM_NCDESTROY            (WM_APP - 1)
#endif


namespace Ext {

#ifdef GetNextWindow
#	undef GetNextWindow
	inline HWND GetNextWindow(HWND hWnd, UINT nDirection) { return ::GetWindow(hWnd, nDirection); }
#endif

inline HRESULT OleCheck(HRESULT hr) {
	if (FAILED(hr))
		Throw(hr);
	return hr;
}

int AFXAPI WinerrToErrno(int rc);

class AFX_CLASS Size : public SIZE {
public:
	typedef Size class_type;

	Size()
	{}

	Size(const SIZE& size)
		:	SIZE(size)
	{}

	Size(int initCX, int initCY) {
		cx = initCX;
		cy = initCY;
	}

	bool operator==(const SIZE& size) const { return cx == size.cx && cy == size.cy; }
	bool operator!=(const SIZE& size) const {return !operator==(size); }

	int get_Width() const { return cx; }
	void put_Width(int v) { cx = v; }
	DEFPROP_CONST(int, Width);

	int get_Height() const { return cy; }
	void put_Height(int v) { cy = v; }
	DEFPROP_CONST(int, Height);
};

/*!!!
inline size_t hash_value(const Size& size) { return stdext::hash_value(size.cx) + stdext::hash_value(size.cy); }
*/

/*!!!
template<> struct hash<Size>
{
size_t operator()(const Size& size) const { return hash<LONG>()(size.cx) + hash<LONG>()(size.cy); }
}; */

inline bool operator<(const Size& a, const Size& b) { return a.cx<b.cx || a.cx==b.cx && a.cy<b.cy; }

class AFX_CLASS Point : public POINT {
public:
	Point() {
	}

	Point(int initX, int initY) {
		x = initX;
		y = initY;
	}

	Point(const POINT& initPt) {
		*(POINT*)this = initPt;
	}

	Point(SIZE initSize);

	Point(LPARAM dwPoint) {
		x = (short)LOWORD(dwPoint);
		y = (short)HIWORD(dwPoint);
	}

	bool operator==(const POINT& point) const;
	void Offset(int xOffset, int yOffset);

	bool operator!=(const POINT& point) const {return !operator==(point);}
	void Offset(const POINT& pt) { Offset(pt.x, pt.y); }
	void Offset(const SIZE& sz) { Offset(sz.cx, sz.cy); }

	Point operator+(const SIZE& size) const { return Point(x + size.cx, y + size.cy); }
	Point operator-(const SIZE& size) const { return Point(x - size.cx, y - size.cy); }
	Point operator-() const { return Point(-x, -y); }
	Point operator+(const POINT& point) const { return Point(x + point.x, y + point.y); }

	// Operators returning CSize values
	Size operator-(POINT point) const 	{ return Size(x - point.x, y - point.y); }
};

typedef Point CPoint;

class Rectangle : public RECT {
public:
	typedef Rectangle class_type;

	Rectangle()
	{}

	Rectangle(int l, int t, int w, int h) { 
		left = l;
		top = t;
		right = l+w;
		bottom = t+h;
	}

	Rectangle(const RECT& rect)
		:	RECT(rect)
	{}

	Rectangle(const POINT& point, const SIZE& size) {
		right = (left = point.x)+size.cx;
		bottom = (top = point.y)+size.cy;
	}

	static Rectangle FromLTRB(int l, int t, int r, int b) {
		Rectangle rc;
		rc.left = l;
		rc.top = t;
		rc.right = r;
		rc.bottom = b;
		return rc;
	}

	operator LPCRECT() const { return this; }
	Rectangle& operator=(const RECT& rect);

	bool operator==(const RECT& rect) const { return ::EqualRect(this, &rect); }

	Rectangle& operator-=(const POINT& pt) {
		Offset(-pt.x, -pt.y);
		return *this;
	}

	void SetEmpty() { Win32Check(::SetRectEmpty(this)); }
	void Inflate(int dx, int dy) { Win32Check(::InflateRect(this, dx, dy)); }

	int get_Left() const { return left; }
	int put_Left(int v) { left = v; }
	DEFPROP_CONST(int, Left);

	int get_Top() const { return top; }
	int put_Top(int v) { top = v; }
	DEFPROP_CONST(int, Top);

	int get_Width() const { return right-left; }
	int put_Width(int v) { right = left+v; }
	DEFPROP_CONST(int, Width);

	int get_Height() const { return bottom-top; }
	void put_Height(int v) { bottom = top+v; }
	DEFPROP_CONST(int, Height);

	Size get_Size() const { class Size size(Width, Height); return size; }
	void put_Size(const Size& size) {
		Width = size.Width;
		Height = size.Height;
	}
	DEFPROP_CONST(Size, Size);

	bool Contains(const Point& point) const { return ::PtInRect(this, point); }

	void Offset(int x, int y);
	void Offset(const Point& pt) { Offset(pt.x, pt.y); }

	void Normalize();

	static Rectangle AFXAPI Intersect(const RECT& rect1, const RECT& rect2) {
		Rectangle r;
		Win32Check(::IntersectRect(&r, &rect1, &rect2));
		return r;
	}

	static Rectangle AFXAPI Subtract(const RECT& rect1, const RECT& rect2) {
		Rectangle r;
		Win32Check(::SubtractRect(&r, &rect1, &rect2));
		return r;
	}

	static Rectangle AFXAPI Union(const RECT& rect1, const RECT& rect2) {
		Rectangle r;
		Win32Check(::UnionRect(&r, &rect1, &rect2));
		return r;
	}

	Point& TopLeft() const {return *(Point*)this;}
	Point& BottomRight() const {return *((Point*)this+1);}
};

typedef Rectangle Rect;



}

#			if UCFG_OLE
#				include "ext-com.h"	
#				include "excom.h"
#			endif


//!!!R #ifndef _EXT
//!!!R 	#include "extheaders.h"
//!!!R #endif





//!!!D #include "ext_messages.h"






/*!!!#include <vector>
#include <map>
using namespace std;*/


#if !UCFG_WCE
//	#define offsetofclass(base, derived) ((DWORD_PTR)(static_cast<base*>((derived*)_AFX_PACKING))-_AFX_PACKING)
#endif

namespace Ext
{




	//!!!using namespace std;


	class CCriticalSection;
	class Stream;

	/*!!!R
	size_t hash_value(RCString s);

	inline size_t hash_value(short v) { return stdext::hash_value(int(v)); }
	inline size_t hash_value(unsigned short v) { return stdext::hash_value(int(v)); }

	inline size_t hash_value(long v) { return stdext::hash_value(int(v)); }
	inline size_t hash_value(unsigned long v) { return stdext::hash_value(int(v)); }

	inline size_t hash_value(const Int64& v) { return stdext::hash_value(int(v)) + stdext::hash_value(int(v >> 32)); }
	inline size_t hash_value(const UInt64& v) { return hash_value(*(const Int64*)&v); }

	inline size_t hash_value(double v) { return hash_value(*(Int64*)&v); }
	*/

	/*!!!
	template<> struct hash<String> 
	{
	size_t operator()(RCString s) const
	};
	*/







} // Ext::

#ifdef SetWindowLongPtrA
#undef SetWindowLongPtrA
inline LONG_PTR SetWindowLongPtrA( HWND hWnd, int nIndex, LONG_PTR dwNewLong )
{
	return( ::SetWindowLongA( hWnd, nIndex, LONG( dwNewLong ) ) );
}
#endif

#ifdef SetWindowLongPtrW
#undef SetWindowLongPtrW
inline LONG_PTR SetWindowLongPtrW( HWND hWnd, int nIndex, LONG_PTR dwNewLong )
{
	return( ::SetWindowLongW( hWnd, nIndex, LONG( dwNewLong ) ) );
}
#endif

#ifdef GetWindowLongPtrA
#undef GetWindowLongPtrA
inline LONG_PTR GetWindowLongPtrA( HWND hWnd, int nIndex )
{
	return( ::GetWindowLongA( hWnd, nIndex ) );
}
#endif

#ifdef GetWindowLongPtrW
#undef GetWindowLongPtrW
inline LONG_PTR GetWindowLongPtrW( HWND hWnd, int nIndex )
{
	return( ::GetWindowLongW( hWnd, nIndex ) );
}
#endif


namespace Ext {

#ifndef WIN32
#	error ext-win  work only on Win32 platforms
#endif

#if !UCFG_WCE
#	pragma comment(lib, "delayimp.lib")
#	pragma comment(lib, "wininet.lib")
#	pragma comment(lib, "psapi.lib")
#endif


#ifndef INVALID_FILE_ATTRIBUTES
	#define INVALID_FILE_ATTRIBUTES ((DWORD)-1)
#endif


class CWinFileFind {
public:
	CWinFileFind(LPCTSTR filespec);
	~CWinFileFind();
	void First(LPCTSTR filespec = 0);
	bool Next(WIN32_FIND_DATA& findData);
private:
	HANDLE m_h;
	CBool m_b;
	WIN32_FIND_DATA m_findData;
};

class COvlEvent : public OVERLAPPED, public Object {
public:
	CEvent m_ev;
	SafeHandle::HandleAccess m_ha;

	COvlEvent()
		:	m_ha(m_ev)
	{
		ZeroStruct(*static_cast<OVERLAPPED*>(this));
		hEvent = m_ha;
	}

	COvlEvent(bool)
		:	m_ev(false, true)
		,	m_ha(m_ev)
	{
		ZeroStruct(*static_cast<OVERLAPPED*>(this));
		hEvent = m_ha;
	}
};

ENUM_CLASS(DialogResult) {
	Yes = IDYES,
		No = IDNO,
} END_ENUM_CLASS(DialogResult)

ENUM_CLASS(MessageBoxButtons) {
	OK	= MB_OK,
		OKCancel	= MB_OKCANCEL,
		AbortRetryIgnore = MB_ABORTRETRYIGNORE,
		YesNoCancel	= MB_YESNOCANCEL,
		YesNo		= MB_YESNO,
		RetryCancel	= MB_RETRYCANCEL
	} END_ENUM_CLASS(MessageBoxButtons)

ENUM_CLASS(MessageBoxIcon) {
		Stop		= MB_ICONSTOP,
		Exclamation = MB_ICONEXCLAMATION,
		Error		= MB_ICONERROR,
		Warning		= MB_ICONWARNING,
		Information = MB_ICONINFORMATION,
		Question	= MB_ICONQUESTION,
		Hand		= MB_ICONHAND,
		Asterisk	= MB_ICONASTERISK
	} END_ENUM_CLASS(MessageBoxIcon)

class MessageBox {
public:
	EXT_API static DialogResult AFXAPI Show(RCString text);
	EXT_API static DialogResult AFXAPI Show(RCString text, RCString caption, int buttons, MessageBoxIcon icon = (MessageBoxIcon)0);
};


enum COsVersion {
	OSVER_FLAG_NT      = 0x10,
	OSVER_FLAG_UNICODE = 0x20,
	OSVER_FLAG_CE      = 0x40 | OSVER_FLAG_UNICODE,
	OSVER_FLAG_SERVER   = 0x80,
	OSVER_FLAG_2K_BRANCH = 0x100 | OSVER_FLAG_NT | OSVER_FLAG_UNICODE,

	OSVER_95      = 1,
	OSVER_98      = 2,
	OSVER_ME      = 3,
	OSVER_NT4     = 4 | OSVER_FLAG_NT | OSVER_FLAG_UNICODE,
	OSVER_2000    = 5 | OSVER_FLAG_2K_BRANCH,
	OSVER_XP      = 6 | OSVER_FLAG_2K_BRANCH,
	OSVER_CE      = 7 | OSVER_FLAG_CE,
	OSVER_CE_4		= 8 | OSVER_FLAG_CE,
	OSVER_SERVER_2003 = 9 | OSVER_FLAG_2K_BRANCH | OSVER_FLAG_SERVER,
	OSVER_VISTA = 10 | OSVER_FLAG_2K_BRANCH,
	OSVER_2008 = 10 | OSVER_FLAG_2K_BRANCH | OSVER_FLAG_SERVER,
	OSVER_CE_5  = 9 | OSVER_FLAG_CE,
	OSVER_CE_6  = 10 | OSVER_FLAG_CE,
	OSVER_CE_FUTURE  = 11 | OSVER_FLAG_CE,
	OSVER_7 = 13 | OSVER_FLAG_2K_BRANCH,
	OSVER_2008_R2 = 13 | OSVER_FLAG_2K_BRANCH | OSVER_FLAG_SERVER,
	OSVER_8 = 14 | OSVER_FLAG_2K_BRANCH,
	OSVER_FUTURE = 15 | OSVER_FLAG_2K_BRANCH
};

AFX_API COsVersion AFXAPI GetOsVersion();
inline bool AFXAPI IsUnicode() { return GetOsVersion() & OSVER_FLAG_UNICODE; }
AFX_API DWORD AFXAPI WaitWithMS(DWORD dwMS, HANDLE h0, HANDLE h1 = 0, HANDLE h2 = 0, HANDLE h3 = 0, HANDLE h4 = 0, HANDLE h5 = 0);
AFX_API DWORD AFXAPI Wait(HANDLE h0, HANDLE h1 = 0, HANDLE h2 = 0, HANDLE h3 = 0, HANDLE h4 = 0, HANDLE h5 = 0);

class AFX_CLASS FileVersionInfo {
public:
	typedef FileVersionInfo class_type;

	Blob m_blob;

	FileVersionInfo(RCString fileName = nullptr);
	String GetStringFileInfo(RCString s);

	const VS_FIXEDFILEINFO& get_FixedInfo();
	DEFPROP_GET(const VS_FIXEDFILEINFO&, FixedInfo);

	Version GetFileVersionN() {
		const VS_FIXEDFILEINFO& fi = FixedInfo;		
		return Version::FromFileInfo(fi.dwFileVersionMS, fi.dwFileVersionLS);	
	}

	Version GetProductVersionN() {
		const VS_FIXEDFILEINFO& fi = FixedInfo;		
		return Version::FromFileInfo(fi.dwProductVersionMS, fi.dwProductVersionLS);
	}

	String get_FileDescription() { return GetStringFileInfo("FileDescription"); }
	DEFPROP_GET(String, FileDescription);

	String get_CompanyName() { return GetStringFileInfo("CompanyName"); }
	DEFPROP_GET(String, CompanyName);

	String get_ProductName() { return GetStringFileInfo("ProductName"); }
	DEFPROP_GET(String, ProductName);

	String get_InternalName() { return GetStringFileInfo("InternalName"); }
	DEFPROP_GET(String, InternalName);

	String get_OriginalFilename() { return GetStringFileInfo("OriginalFilename"); }
	DEFPROP_GET(String, OriginalFilename);

	String get_ProductVersion() { return GetStringFileInfo("ProductVersion"); }
	DEFPROP_GET(String, ProductVersion);

	String get_FileVersion() { return GetStringFileInfo("FileVersion"); }
	DEFPROP_GET(String, FileVersion);

	String get_LegalCopyright() { return GetStringFileInfo("LegalCopyright"); }
	DEFPROP_GET(String, LegalCopyright);

	String get_SpecialBuild() { return GetStringFileInfo("SpecialBuild"); }
	DEFPROP_GET(String, SpecialBuild);
};

class ProcessModule {
public:
	CPointer<Process> Process;
	CInt<HMODULE> HModule;

	ProcessModule() {
	}

	ProcessModule(class Process& process, HMODULE hModule);

	void *get_BaseAddress() const { return m_mi.lpBaseOfDll; }
	DEFPROP_GET(void*, BaseAddress);

	size_t get_ModuleMemorySize() const { return m_mi.SizeOfImage; }
	DEFPROP_GET(size_t, ModuleMemorySize);

	void *get_EntryPointAddress() const { return m_mi.EntryPoint; }
	DEFPROP_GET(void*, EntryPointAddress);

	String get_ModuleName() const;
	DEFPROP_GET(String, ModuleName);

	String get_FileName() const;
	DEFPROP_GET_CONST(String, FileName);

	Ext::FileVersionInfo get_FileVersionInfo() const {
		return Ext::FileVersionInfo(FileName);
	}
	DEFPROP_GET_CONST(Ext::FileVersionInfo, FileVersionInfo);
private:
	MODULEINFO m_mi;
};

struct ProcessStartInfo {
	String FileName,
		Arguments,
		WorkingDirectory;
	CBool CreateNewWindow,
		CreateNoWindow,
		RedirectStandardInput,
		RedirectStandardOutput,
		RedirectStandardError;
	DWORD Flags;

	ProcessStartInfo()
		:	Flags(0)
	{}
};

class Process : public SafeHandle { //!!!
public:
	ProcessStartInfo StartInfo;

#if UCFG_EXTENDED && !UCFG_WCE
protected:
	File m_fileIn, m_fileOut, m_fileErr;
public:
	FileStream StandardInput,
		StandardOutput,
		StandardError;
#endif
public:
	Process(HANDLE handle) {
		Attach(handle, false);
		CommonInit();
	}

	enum ProcessEnum {
		Current
	};

	AFX_API Process();
	AFX_API Process(ProcessEnum current);
	AFX_API Process(DWORD id, DWORD dwAccess = MAXIMUM_ALLOWED, bool bInherit = false);

	bool Start();
	static EXT_API std::unique_ptr<Process> AFXAPI Start(ProcessStartInfo psi);

	EXT_API std::unique_ptr<CWinThread> Create(RCString commandLine, DWORD dwFlags = 0, const char *dir = 0, bool bInherit = false, STARTUPINFO *psi = 0);

	void Terminate(DWORD dwExitCode)  {	Win32Check(::TerminateProcess(HandleAccess(*this), dwExitCode)); }

	void WaitForExit(DWORD ms = INFINITE) { WaitWithMS(ms, HandleAccess(*this)); }

	DWORD get_ExitCode() const {
		DWORD r;
		Win32Check(::GetExitCodeProcess(HandleAccess(*this), &r));
		return r;
	}
	DEFPROP_GET(DWORD, ExitCode);

	AFX_API CTimesInfo get_Times() const;
	DEFPROP_GET(CTimesInfo, Times);

	DWORD get_ID() const;
	DEFPROP_GET(DWORD, ID);

	HWND get_MainWindowHandle() const;
	DEFPROP_GET(HWND, MainWindowHandle);

#if !UCFG_WCE
	AFX_API DWORD get_Version() const;
	DEFPROP_GET(DWORD, Version);

	bool get_IsWow64();
	DEFPROP_GET(bool, IsWow64);

	DWORD get_PriorityClass() { return Win32Check(::GetPriorityClass(HandleAccess(*this))); }
	void put_PriorityClass(DWORD pc) { Win32Check(::SetPriorityClass(HandleAccess(*this), pc)); }
	DEFPROP(DWORD, PriorityClass);
	void GenerateConsoleCtrlEvent(DWORD dwCtrlEvent = CTRL_BREAK_EVENT);

	String get_MainModuleFileName();
	DEFPROP_GET(String, MainModuleFileName);
#endif

	void FlushInstructionCache(LPCVOID base = 0, SIZE_T size = 0) { Win32Check(::FlushInstructionCache(HandleAccess(*this), base, size)); }

	DWORD VirtualProtect(void *addr, size_t size, DWORD flNewProtect);
	MEMORY_BASIC_INFORMATION VirtualQuery(const void *addr);
	SIZE_T ReadMemory(LPCVOID base, LPVOID buf, SIZE_T size);
	SIZE_T WriteMemory(LPVOID base, LPCVOID buf, SIZE_T size); 
	
	EXT_API std::vector<ProcessModule> get_Modules();
	DEFPROP_GET(std::vector<ProcessModule>, Modules);
protected:
	bool Valid() const {
		return DangerousGetHandleEx() != 0;
	}

	void CommonInit();
private:
	mutable CInt<DWORD> m_pid;
};

class Wow64FsRedirectionKeeper {
	typedef Wow64FsRedirectionKeeper class_type;
public:
	Wow64FsRedirectionKeeper() {}

	Wow64FsRedirectionKeeper(bool) {		// arg don't have value, always disable
		Disable();
	}

	void Disable();
	~Wow64FsRedirectionKeeper();
private:
	void *m_oldValue;
	CBool m_bDisabled;

	EXT_DISABLE_COPY_CONSTRUCTOR
};

class AFX_CLASS CVirtualMemory {
public:
	void *m_address;

	CVirtualMemory()
		:	m_address(0)
	{}

	CVirtualMemory(void *lpAddress, DWORD dwSize, DWORD flAllocationType = MEM_RESERVE, DWORD flProtect = PAGE_READWRITE);
	~CVirtualMemory();
	void Allocate(void *lpAddress, DWORD dwSize, DWORD flAllocationType = MEM_RESERVE, DWORD flProtect = PAGE_READWRITE);
	void Commit(void *lpAddress, DWORD dwSize, DWORD flProtect = PAGE_READWRITE);
	void Free();
};

class CHeap {
public:
	CHeap();
	~CHeap();
	void Destroy();
	size_t Size(void *p, DWORD flags = 0);
	void *Alloc(size_t size, DWORD flags = 0);
	void Free(void *p, DWORD flags = 0);
private:
	HANDLE m_h;
};

#if UCFG_WND
template <class T> class CHandleMap {
	std::unordered_map<HANDLE, T*> m_permanentMap;
	std::unordered_map<HANDLE, T*> m_temporaryMap;
public:
	~CHandleMap() { 
		DeleteTemp();
	}

	T *FromHandle(HANDLE h);
	void DeleteTemp() {
		while (m_temporaryMap.size() != 0) {
			std::unordered_map<HANDLE, T*>::iterator i = m_temporaryMap.begin();
			T *p = i->second;
			m_temporaryMap.erase(i);
			p->SetHandle(0);
			delete p;
		}
	}

	void SetPermanent(HANDLE h, T* permOb);
	void RemoveHandle(HANDLE h);
	T* LookupPermanent(HANDLE h);
	T* LookupTemporary(HANDLE h);
};

template <class T> T *CHandleMap<T>::FromHandle(HANDLE h) {
	T *p = LookupPermanent(h);
	if (p)
		return p;
	if (p = LookupTemporary(h))
		return p;
	p = new T;
	m_temporaryMap[h] = p; //!!!.SetAt(h,p);
	p->SetHandle(h);
	return p;
}



template <class T> void CHandleMap<T>::SetPermanent(HANDLE h, T* permOb) {
	m_permanentMap[h] = permOb; //!!!.SetAt(h, permOb);
}

template <class T> void CHandleMap<T>::RemoveHandle(HANDLE h) {
	m_permanentMap.erase(h);  
}

template <class T> T* CHandleMap<T>::LookupPermanent(HANDLE h) {
	T *p = 0;
	Lookup(m_permanentMap, h, p);
	return p;
}

template <class T> T* CHandleMap<T>::LookupTemporary(HANDLE h) {
	T *p = 0;
	Lookup(m_temporaryMap, h, p);
	return p;
}
#endif

class CDynLinkLibrary;

struct AFX_EXTENSION_MODULE {
	HINSTANCE m_hModule;
	CDynLinkLibrary *m_pLibrary;
	CBool bInitialized;
};

#if UCFG_EXTENDED
	class CDC;
	class CMenu;
	class CGdiObject;
	class ImageList;
	class CToolTipCtrl;
	class CControlBarBase;
	class CFrameWnd;
	class CView;


	extern AFX_EXTENSION_MODULE coreDLL;

	class EXTAPI CDynLinkLibrary : public CCmdTarget {
	public:
		HINSTANCE m_hModule;
		bool m_bSystem;

		CDynLinkLibrary(AFX_EXTENSION_MODULE& state, bool bSystem = false);
		CDynLinkLibrary(HINSTANCE hModule);
		~CDynLinkLibrary();
	};


#	if !UCFG_WCE
	class AFX_CLASS CWindowsHook {
	public:
		HHOOK m_handle;

		CWindowsHook();
		~CWindowsHook();
		void Set(int idHook, HOOKPROC lpfn, HINSTANCE hMod, DWORD dwThreadId);
		void Unhook();
		LRESULT CallNext(int nCode, WPARAM wParam, LPARAM lParam);
	};
#	endif

#endif

typedef int (__cdecl * _PNH)( size_t );

class CThreadHandleMaps {
public:
#if UCFG_WND
	CHandleMap<CWnd> m_mapHWND;
#endif

#if UCFG_EXTENDED
#	if UCFG_GUI
	CHandleMap<CDC> m_mapHDC;
	CHandleMap<CMenu> m_mapHMENU;
	CHandleMap<CGdiObject> m_mapHGDIOBJ;
	CHandleMap<ImageList> m_mapHIMAGELIST;
#	endif
#endif
};

class AFX_MODULE_THREAD_STATE : public CNoTrackObject {
public:
	_PNH m_pfnNewHandler;
	//!!!  COwnerArray<CComTypeLibHolder> m_tlList;
	CPointer<ThreadBase> m_pCurrentThread;

#if UCFG_COM_IMPLOBJ
	typedef std::vector<std::unique_ptr<CComClassFactoryImpl>> CFactories;
	CFactories m_factories;

	typedef std::vector<std::unique_ptr<CComClass>> CClassList;
	CClassList m_classList;
	CComClass *m_comClass;
#endif
	AFX_MODULE_THREAD_STATE();
	~AFX_MODULE_THREAD_STATE();
	CThreadHandleMaps& GetHandleMaps();
private:
	std::auto_ptr<CThreadHandleMaps> m_handleMaps;
};

AFX_API AFX_MODULE_THREAD_STATE * AFXAPI AfxGetModuleThreadState();


class CWinApp;
class CAppBase;

class EXTAPI AFX_MODULE_STATE : public CNoTrackObject {
public:
	CThreadLocal<AFX_MODULE_THREAD_STATE> m_thread;
	CWinApp *m_pCurrentWinApp;
#if UCFG_COMPLEX_WINAPP
	CAppBase *m_pCurrentCApp;
#endif
	String m_currentAppName;
	HMODULE m_hCurrentInstanceHandle;
	HMODULE m_hCurrentResourceHandle;
	DWORD m_fRegisteredClasses;

	void SetHInstance(HMODULE hModule);

#if UCFG_COM_IMPLOBJ
	CComModule m_comModule;
	CComTypeLibHolder m_typeLib;
	std::unique_ptr<CComClass> m_pComClass;

	typedef std::vector<COleObjectFactory*> CFactories;
	CFactories m_factoryList;
#endif
#if UCFG_OCC
	CPointer<COccManager> m_pOccManager;	
#endif
	void (AFXAPI *m_pfnFilterToolTipMessage)(MSG*, CWnd*);
#if defined(_AFXDLL) && UCFG_EXTENDED
	typedef std::vector<ptr<CDynLinkLibrary>> CLibraryList;
	CLibraryList m_libraryList;
#endif

#if defined(_AFXDLL)
	WNDPROC m_pfnAfxWndProc;
	DWORD m_dwVersion;

	AFX_MODULE_STATE(bool bDLL, WNDPROC pfnAfxWndProc);
#else
	explicit AFX_MODULE_STATE(bool bDLL);
#endif
	bool m_bDLL;


	~AFX_MODULE_STATE();

	String get_FileName();
	DEFPROP_GET(String, FileName);
};



extern AFX_MODULE_STATE _afxBaseModuleState;


#ifdef _AFXDLL
#	define _AFX_CMDTARGET_GETSTATE() (m_pModuleState)
#else
#	define _AFX_CMDTARGET_GETSTATE() (AfxGetModuleState())
#endif



#ifdef _AFXDLL
#	define AFX_MANAGE_STATE(p) AFX_MAINTAIN_STATE2 _ctlState(p);
#else
#	define AFX_MANAGE_STATE(p)
#endif

//!!!extern AFX_MODULE_STATE afxModuleState;

class _AFX_THREAD_STATE : public CNoTrackObject {
public:
	AFX_MODULE_STATE *m_pModuleState;
	AFX_MODULE_STATE *m_pPrevModuleState;
	const MSG *m_pLastSentMsg;
	CPointer<CWnd> m_pWndInit;
#if UCFG_EXTENDED && UCFG_GUI
	CPointer<CToolTipCtrl> m_pToolTip;
	CPointer<CControlBarBase> m_pLastStatus; // last flyby status control bar
#endif
	HWND m_hTrackingWindow;         // see CWnd::TrackPopupMenu
	HMENU m_hTrackingMenu;
	MSG m_msgCur;
	String m_szTempClassName;
#if !UCFG_WCE && UCFG_EXTENDED
	CPointer<CFrameWnd> m_pRoutingFrame;
	CWindowsHook m_hookCbt,
		m_hookMsg;
	CPointer<CView> m_pRoutingView;          // see CCmdTarget::GetRoutingView
#endif
	CPointer<CWnd> m_pAlternateWndInit;
	HWND m_hLockoutNotifyWindow;    // see CWnd::OnCommand
	CPointer<CWnd> m_pLastHit;
	int m_nLastHit;
	void *m_pLastInfo;			//	TOOLINFO
	int m_nLastStatus;      // last flyby status message
	bool m_bInMsgFilter;
	bool m_bDlgCreate;

	_AFX_THREAD_STATE();
	~_AFX_THREAD_STATE();
};

extern EXT_DATA CThreadLocal<_AFX_THREAD_STATE> _afxThreadState;

struct AFX_CLASS AFX_MAINTAIN_STATE2
{
	AFX_MAINTAIN_STATE2(AFX_MODULE_STATE* pModuleState);
	~AFX_MAINTAIN_STATE2();
protected:
	AFX_MODULE_STATE* m_pPrevModuleState;
	_AFX_THREAD_STATE* m_pThreadState;
};


AFX_API AFX_MODULE_STATE * AFXAPI AfxGetModuleState();
AFX_MODULE_STATE * AFXAPI AfxGetStaticModuleState();
AFX_API _AFX_THREAD_STATE* AFXAPI AfxGetThreadState();
AFX_API void AFXAPI AfxWinInit(HINSTANCE hInstance, HINSTANCE hPrevInstance, RCString lpCmdLine, int nCmdShow);
AFX_API int AFXAPI AfxWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, RCString lpCmdLine, int nCmdShow);
AFX_API HRESULT AFXAPI AfxDllCanUnloadNow();
AFX_API void AFXAPI AfxTlsAddRef();
AFX_API void AFXAPI AfxTlsRelease();
AFX_API bool AFXAPI AfxIsValidAddress(const void* lp, UINT nBytes, bool bReadWrite = true);
AFX_API CWinApp * AFXAPI AfxGetApp();
AFX_API void AFXAPI RegCheck(LONG v, LONG allowableError = ERROR_SUCCESS);
AFX_API void AFXAPI AfxEndThread(UINT nExitCode, bool bDelete = true);
AFX_API CStringVector AsciizArrayToStringArray(const TCHAR *p);

inline String AFXAPI AfxGetAppName() { return AfxGetModuleState()->m_currentAppName; }
inline HINSTANCE AFXAPI AfxGetInstanceHandle() { return AfxGetModuleState()->m_hCurrentInstanceHandle; }

//#if UCFG_EXTENDED
inline HMODULE AFXAPI AfxGetResourceHandle() { return AfxGetModuleState()->m_hCurrentResourceHandle; }
//#endif

/*!!!
struct CResource {
	HINSTANCE m_hInst;
	HRSRC m_hResource;
	HGLOBAL m_hTemplate;
	void *m_p;
	DWORD m_size;
};*/

#ifdef _AFXDLL
	AFX_MODULE_STATE* AFXAPI AfxSetModuleState(AFX_MODULE_STATE* pNewState);
	AFX_API HINSTANCE AFXAPI AfxTryFindResourceHandle(const CResID& lpszName, const CResID& lpszType);
	AFX_API HINSTANCE AFXAPI AfxFindResourceHandle(const CResID& lpszName, const CResID& lpszType);
#else
#	define AfxFindResourceHandle(lpszResource, lpszType) AfxGetResourceHandle()
#endif



AFX_API void AFXAPI ProcessExceptionInFilter(EXCEPTION_POINTERS *ep);

/*!!!R ULONGLONG AFXAPI StrToVersion(RCString s);
EXTAPI String AFXAPI VersionToStr(const LONGLONG& v, int n = 4);
String AFXAPI VersionToStr(const VS_FIXEDFILEINFO& fi, int n = 4);
*/


AFX_API String AFXAPI TryGetVersionString(const FileVersionInfo& vi, RCString name, RCString val = "");

AFX_API String AFXAPI GetAppDataManufacturerFolder();

class CodepageCvt : public std::codecvt<wchar_t, char, mbstate_t> {
	typedef std::codecvt<wchar_t, char, mbstate_t> base;
public:
	explicit CodepageCvt(int cp)
		:	m_cp(cp)
	{}
private:
	int m_cp;

	EXT_API result do_out(mbstate_t& state, const wchar_t *_First1, const wchar_t *_Last1, const wchar_t *& _Mid1, char *_First2, char *_Last2, char *& _Mid2) const override;
};

AFX_API void AFXAPI AfxCoreInitModule();
void AFXAPI AfxInitRichEdit();


} // Ext::

#if UCFG_WCE
	typedef HANDLE HINST;
#else
	typedef HINSTANCE HINST;
#endif


//!!!#ifndef ASSERT //!!!
//!!!#	define ASSERT(f) ((void*)0)
//!!!#endif


