using System;
using System.Runtime.InteropServices;
using System.Text;
using System.ComponentModel;

using HANDLE = System.IntPtr;
using HWND = System.IntPtr;
using HDC = System.IntPtr;
using LONG = System.Int32;
using DWORD = System.UInt32;

namespace Win32 {

[Flags]
public enum StateSystem : int
{
	STATE_SYSTEM_UNAVAILABLE    =    0x00000001,  // Disabled
	STATE_SYSTEM_SELECTED       =    0x00000002,
	STATE_SYSTEM_FOCUSED        =    0x00000004,
	STATE_SYSTEM_PRESSED        =    0x00000008,
	STATE_SYSTEM_CHECKED        =    0x00000010,
	STATE_SYSTEM_MIXED          =    0x00000020,  // 3-state checkbox or toolbar button
	STATE_SYSTEM_INDETERMINATE  =    STATE_SYSTEM_MIXED,
	STATE_SYSTEM_READONLY       =    0x00000040,
	STATE_SYSTEM_HOTTRACKED     =    0x00000080,
	STATE_SYSTEM_DEFAULT        =    0x00000100,
	STATE_SYSTEM_EXPANDED       =    0x00000200,
	STATE_SYSTEM_COLLAPSED      =    0x00000400,
	STATE_SYSTEM_BUSY           =    0x00000800,
	STATE_SYSTEM_FLOATING       =    0x00001000,  // Children "owned" not "contained" by parent
	STATE_SYSTEM_MARQUEED       =    0x00002000,
	STATE_SYSTEM_ANIMATED       =    0x00004000,
	STATE_SYSTEM_INVISIBLE      =    0x00008000,
	STATE_SYSTEM_OFFSCREEN      =    0x00010000,
	STATE_SYSTEM_SIZEABLE       =    0x00020000,
	STATE_SYSTEM_MOVEABLE       =    0x00040000,
	STATE_SYSTEM_SELFVOICING    =    0x00080000,
	STATE_SYSTEM_FOCUSABLE      =    0x00100000,
	STATE_SYSTEM_SELECTABLE     =    0x00200000,
	STATE_SYSTEM_LINKED         =    0x00400000,
	STATE_SYSTEM_TRAVERSED      =    0x00800000,
	STATE_SYSTEM_MULTISELECTABLE=    0x01000000,  // Supports multiple selection
	STATE_SYSTEM_EXTSELECTABLE  =    0x02000000,  // Supports extended selection
	STATE_SYSTEM_ALERT_LOW      =    0x04000000,  // This information is of low priority
	STATE_SYSTEM_ALERT_MEDIUM   =    0x08000000,  // This information is of medium priority
	STATE_SYSTEM_ALERT_HIGH     =    0x10000000,  // This information is of high priority
	STATE_SYSTEM_PROTECTED      =    0x20000000,  // access to this is restricted
	STATE_SYSTEM_VALID          =    0x3FFFFFFF
}

[Flags]
public enum ScrollBarSelect
{
	SB_HORZ = 0,
	SB_VERT = 1,
	SB_CTL = 2,
	SB_BOTH = 3
}

public struct SCROLLINFO 
{
	public int cbSize;
	public int fMask;
	public int nMin;
	public int nMax;
	public int nPage;
	public int nPos;
	public int nTrackPos;
}
															
public struct SCROLLBARINFO
{
	public const int CCHILDREN_SCROLLBAR = 5;

	public int cbSize;
  public RECT rcScrollBar;
  public int dxyLineButton;
  public int xyThumbTop;
  public int xyThumbBottom;
  public int reserved;

	[MarshalAs(UnmanagedType.ByValArray, SizeConst = CCHILDREN_SCROLLBAR + 1)]
	public DWORD[] rgstate;

	public static SCROLLBARINFO Init()
	{
		SCROLLBARINFO sbi = new SCROLLBARINFO();
		sbi.cbSize = Marshal.SizeOf(sbi);
		return sbi;
	}
}

public enum OBJID : uint
{
	OBJID_WINDOW = 0x00000000,
									 OBJID_SYSMENU = 0xFFFFFFFF,
									 OBJID_TITLEBAR = 0xFFFFFFFE,
		OBJID_MENU = 0xFFFFFFFD,
		OBJID_CLIENT = 0xFFFFFFFC,
		OBJID_VSCROLL = 0xFFFFFFFB,
		OBJID_HSCROLL = 0xFFFFFFFA,
		OBJID_SIZEGRIP = 0xFFFFFFF9,
		OBJID_CARET = 0xFFFFFFF8,
		OBJID_CURSOR = 0xFFFFFFF7,
		OBJID_ALERT = 0xFFFFFFF6,
		OBJID_SOUND = 0xFFFFFFF5,
		OBJID_QUERYCLASSNAMEIDX = 0xFFFFFFF4,
		OBJID_NATIVEOM = 0xFFFFFFF0
}

public enum WM : int {
	WM_ACTIVATE = 0x6,
	WM_ACTIVATEAPP = 0x1C,
	WM_ASKCBFORMATNAME = 0x30C,
	WM_CANCELJOURNAL = 0x4B,
	WM_CANCELMODE = 0x1F,
	WM_CHANGECBCHAIN = 0x30D,
	WM_CHAR = 0x102,
	WM_CHARTOITEM = 0x2F,
	WM_CHILDACTIVATE = 0x22,
	WM_CLEAR = 0x303,
	WM_CLOSE = 0x10,
	WM_COMMAND = 0x111,
	WM_COMMNOTIFY = 0x44, 
	WM_COMPACTING = 0x41,
	WM_COMPAREITEM = 0x39,
	WM_COPY = 0x301,
	WM_COPYDATA = 0x4A,
	WM_CREATE = 0x1,
	WM_CTLCOLORBTN = 0x135,
	WM_CTLCOLORDLG = 0x136,
	WM_CTLCOLOREDIT = 0x133,
	WM_CTLCOLORLISTBOX = 0x134,
	WM_CTLCOLORMSGBOX = 0x132,
	WM_CTLCOLORSCROLLBAR = 0x137,
	WM_CTLCOLORSTATIC = 0x138,
	WM_CUT = 0x300,
	WM_DDE_ACK = (WM_DDE_FIRST + 4),
	WM_DDE_ADVISE = (WM_DDE_FIRST + 2),
	WM_DDE_DATA = (WM_DDE_FIRST + 5),
	WM_DDE_EXECUTE = (WM_DDE_FIRST + 8),
	WM_DDE_FIRST = 0x3E0,
	WM_DDE_INITIATE = (WM_DDE_FIRST),
	WM_DDE_LAST = (WM_DDE_FIRST + 8),
	WM_DDE_POKE = (WM_DDE_FIRST + 7),
	WM_DDE_REQUEST = (WM_DDE_FIRST + 6),
	WM_DDE_TERMINATE = (WM_DDE_FIRST + 1),
	WM_DDE_UNADVISE = (WM_DDE_FIRST + 3),
	WM_DEADCHAR = 0x103,
	WM_DELETEITEM = 0x2D,
	WM_DESTROY = 0x2,
	WM_DESTROYCLIPBOARD = 0x307,
	WM_DEVMODECHANGE = 0x1B,
	WM_DRAWCLIPBOARD = 0x308,
	WM_DRAWITEM = 0x2B,
	WM_DROPFILES = 0x233,
	WM_ENABLE = 0xA,
	WM_ENDSESSION = 0x16,
	WM_ENTERIDLE = 0x121,
	WM_ENTERMENULOOP = 0x211,
	WM_ERASEBKGND = 0x14,
	WM_EXITMENULOOP = 0x212,
	WM_FONTCHANGE = 0x1D,
	WM_GETDLGCODE = 0x87,
	WM_GETFONT = 0x31,
	WM_GETHOTKEY = 0x33,
	WM_GETMINMAXINFO = 0x24,
	WM_GETTEXT = 0xD,
	WM_GETTEXTLENGTH = 0xE,
	WM_HOTKEY = 0x312,
	WM_HSCROLL = 0x114,
	WM_HSCROLLCLIPBOARD = 0x30E,
	WM_ICONERASEBKGND = 0x27,
	WM_INITDIALOG = 0x110,
	WM_INITMENU = 0x116,
	WM_INITMENUPOPUP = 0x117,
	WM_KEYDOWN = 0x100,
	WM_KEYFIRST = 0x100,
	WM_KEYLAST = 0x108,
	WM_KEYUP = 0x101,
	WM_KILLFOCUS = 0x8,
	WM_LBUTTONDBLCLK = 0x203,
	WM_LBUTTONDOWN = 0x201,
	WM_LBUTTONUP = 0x202,
	WM_MBUTTONDBLCLK = 0x209,
	WM_MBUTTONDOWN = 0x207,
	WM_MBUTTONUP = 0x208,
	WM_MDIACTIVATE = 0x222,
	WM_MDICASCADE = 0x227,
	WM_MDICREATE = 0x220,
	WM_MDIDESTROY = 0x221,
	WM_MDIGETACTIVE = 0x229,
	WM_MDIICONARRANGE = 0x228,
	WM_MDIMAXIMIZE = 0x225,
	WM_MDINEXT = 0x224,
	WM_MDIREFRESHMENU = 0x234,
	WM_MDIRESTORE = 0x223,
	WM_MDISETMENU = 0x230,
	WM_MDITILE = 0x226,
	WM_MEASUREITEM = 0x2C,
	WM_MENUCHAR = 0x120,
	WM_MENUSELECT = 0x11F,
	WM_MOUSEACTIVATE = 0x21,
	WM_MOUSEFIRST = 0x200,
	WM_MOUSELAST = 0x209,
	WM_MOUSEMOVE = 0x200,
	WM_MOVE = 0x3,
	WM_NCACTIVATE = 0x86,
	WM_NCCALCSIZE = 0x83,
	WM_NCCREATE = 0x81,
	WM_NCDESTROY = 0x82,
	WM_NCHITTEST = 0x84,
	WM_NCLBUTTONDBLCLK = 0xA3,
	WM_NCLBUTTONDOWN = 0xA1,
	WM_NCLBUTTONUP = 0xA2,
	WM_NCMBUTTONDBLCLK = 0xA9,
	WM_NCMBUTTONDOWN = 0xA7,
	WM_NCMBUTTONUP = 0xA8,
	WM_NCMOUSEMOVE = 0xA0,
	WM_NCPAINT = 0x85,
	WM_NCRBUTTONDBLCLK = 0xA6,
	WM_NCRBUTTONDOWN = 0xA4,
	WM_NCRBUTTONUP = 0xA5,
	WM_NEXTDLGCTL = 0x28,
	WM_NULL = 0x0,
	WM_OTHERWINDOWCREATED = 0x42, 
	WM_OTHERWINDOWDESTROYED = 0x43, 
	WM_PAINT = 0xF,
	WM_PAINTCLIPBOARD = 0x309,
	WM_PAINTICON = 0x26,
	WM_PALETTECHANGED = 0x311,
	WM_PALETTEISCHANGING = 0x310,
	WM_PARENTNOTIFY = 0x210,
	WM_PASTE = 0x302,
	WM_PENWINFIRST = 0x380,
	WM_PENWINLAST = 0x38F,
	WM_POWER = 0x48,
	WM_QUERYDRAGICON = 0x37,
	WM_QUERYENDSESSION = 0x11,
	WM_QUERYNEWPALETTE = 0x30F,
	WM_QUERYOPEN = 0x13,
	WM_QUEUESYNC = 0x23,
	WM_QUIT = 0x12,
	WM_RBUTTONDBLCLK = 0x206,
	WM_RBUTTONDOWN = 0x204,
	WM_RBUTTONUP = 0x205,
	WM_RENDERALLFORMATS = 0x306,
	WM_RENDERFORMAT = 0x305,
	WM_SETCURSOR = 0x20,
	WM_SETFOCUS = 0x7,
	WM_SETFONT = 0x30,
	WM_SETHOTKEY = 0x32,
	WM_SETREDRAW = 0xB,
	WM_SETTEXT = 0xC,
	WM_SHOWWINDOW = 0x18,
	WM_SIZE = 0x5,
	WM_SIZECLIPBOARD = 0x30B,
	WM_SPOOLERSTATUS = 0x2A,
	WM_SYSCHAR = 0x106,
	WM_SYSCOLORCHANGE = 0x15,
	WM_SYSCOMMAND = 0x112,
	WM_SYSDEADCHAR = 0x107,
	WM_SYSKEYDOWN = 0x104,
	WM_SYSKEYUP = 0x105,
	WM_TIMECHANGE = 0x1E,
	WM_TIMER = 0x113,
	WM_UNDO = 0x304,
	WM_USER = 0x400,
	WM_VKEYTOITEM = 0x2E,
	WM_VSCROLL = 0x115,
	WM_VSCROLLCLIPBOARD = 0x30A,
	WM_WINDOWPOSCHANGED = 0x47,
	WM_WINDOWPOSCHANGING = 0x46,
	WM_WININICHANGE = 0x1A
}

public enum SW : int {
    SW_SHOWNORMAL = 1,
    SW_SHOWMINIMIZED = 2
}

public partial class Api {	

	[DllImport("user32")]
	public static extern int SetScrollInfo(HWND hwnd, ScrollBarSelect n, ref SCROLLINFO lpcScrollInfo, bool redraw);

	[DllImport("user32", SetLastError = true)]
	public static extern bool GetScrollInfo(HWND hwnd, ScrollBarSelect n, ref SCROLLINFO lpcScrollInfo);

	[DllImport("user32", SetLastError = true)]
	public static extern bool GetScrollBarInfo(HWND hwnd, OBJID idObject, ref SCROLLBARINFO psbi);

	[DllImport("user32", SetLastError = true)]
	public static extern bool ShowScrollBar(HWND hwnd, ScrollBarSelect wBar, bool bShow);

    [DllImport("user32", SetLastError = true)]
    public static extern bool GetKeyboardState(byte[] state);

    [DllImport("user32")]
    public static extern short GetAsyncKeyState(int vkey);

	[DllImport("user32")]
	public static extern HWND GetForegroundWindow();

	[DllImport("user32")]
	public static extern bool SetForegroundWindow(HWND hWnd);

    [DllImport("user32", SetLastError = true)]
    public static extern bool SetWindowPlacement(IntPtr hWnd, [In] ref WINDOWPLACEMENT lpwndpl);

    [DllImport("user32", SetLastError = true)]
    public static extern bool GetWindowPlacement(IntPtr hWnd, out WINDOWPLACEMENT lpwndpl);

	public static void Win32Check(bool b) {
        int err = Marshal.GetLastWin32Error();
		if (!b)
			throw new Win32Exception();
	}

	public static int Win32Check(int r) {
		Win32Check(r == 0);
		return r;
	}
}

}
	
	