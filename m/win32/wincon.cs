/*######     Copyright (c) 1997-2012 Ufasoft  http://ufasoft.com  mailto:support@ufasoft.com,  Sergey Pavlov  mailto:dev@ufasoft.com #####
# This program is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published #
# by the Free Software Foundation; either version 3, or (at your option) any later version. This program is distributed in the hope that #
# it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. #
# See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with this #
# program; If not, see <http://www.gnu.org/licenses/>                                                                                    #
########################################################################################################################################*/

using System;
using System.IO;
using System.Runtime.InteropServices;
using System.Collections.Generic;
using System.Text;
using Microsoft.Win32.SafeHandles;

using BYTE = System.Byte;
using SHORT = System.Int16;
using WORD = System.UInt16;
using DWORD = System.UInt32;
using LONG = System.Int32;
using HANDLE = System.IntPtr;
using WCHAR = System.Char;
using CHAR = System.Byte;
using BOOL = System.Int32;

namespace Win32
{


[StructLayout(LayoutKind.Sequential)]
public struct COORD {
    public Int16 X, Y;

    public Int32 ToInt32() {
        return (UInt16)X | (Y << 16);
    }
}

[StructLayout(LayoutKind.Sequential)]
public struct SMALL_RECT {
    public Int16 Left, Top, Right, Bottom;
}

[StructLayout(LayoutKind.Sequential)]
public struct CHAR_INFO {
    public char Char;
    public UInt16 Attributes;
}

public struct CONSOLE_CURSOR_INFO {
    public DWORD dwSize;
    public bool bVisible;
}

public struct CONSOLE_SCREEN_BUFFER_INFO {
    public COORD dwSize;
    public COORD dwCursorPosition;
    public WORD wAttributes;
    public SMALL_RECT srWindow;
    public COORD dwMaximumWindowSize;
}

public enum ScreenBufferType {
    CONSOLE_TEXTMODE_BUFFER = 1
}

public enum ConsoleMode : uint {
    ENABLE_ECHO_INPUT = 4,
    ENABLE_INSERT_MODE = 0x20
}

public enum EventType : ushort {
    KEY_EVENT = 1,
    MOUSE_EVENT = 2,
    WINDOW_BUFFER_SIZE_EVENT = 4,
    MENU_EVENT = 8,
    FOCUS_EVENT = 0x10
}

[Flags]
public enum ControlKeyState : uint {
	RIGHT_ALT_PRESSED = 1,
	LEFT_ALT_PRESSED = 2,
	RIGHT_CTRL_PRESSED = 4,
	LEFT_CTRL_PRESSED = 8,
	SHIFT_PRESSED = 0x10,
	NUMLOCK_ON = 0x20,
	SCROLLLOCK_ON = 0x40,
	CAPSLOCK_ON = 0x80,
	ENHANCED_KEY = 0x100
}

[StructLayout(LayoutKind.Explicit)]
public struct CharUnion {
	[FieldOffset(0)]
	public WCHAR UnicodeChar;
	[FieldOffset(0)]
	public CHAR AsciiChar;
}

[StructLayout(LayoutKind.Sequential|LayoutKind.Explicit)]
public struct KEY_EVENT_RECORD {
	[FieldOffset(0)]
	public BOOL bKeyDown;
	[FieldOffset(4)]
	public WORD wRepeatCount;
	[FieldOffset(6)]
	public WORD wVirtualKeyCode;
	[FieldOffset(8)]
	public WORD wVirtualScanCode;
	[FieldOffset(10)]
	public CharUnion uChar;
	[FieldOffset(12)] public ControlKeyState dwControlKeyState;
}

public struct MOUSE_EVENT_RECORD {
	public COORD dwMousePosition;
	public DWORD dwButtonState, dwControlKeyState, dwEventFlags;
}

public struct FOCUS_EVENT_RECORD {
	public BOOL bSetFocus;
}

public struct MENU_EVENT_RECORD {
	public UInt32 dwCommandId;
}

public struct WINDOW_BUFFER_SIZE_RECORD {
	public COORD dwSize;
}

[StructLayout(LayoutKind.Explicit)]
public struct EventUnion {
    [FieldOffset(0)]
	public KEY_EVENT_RECORD KeyEvent;
    [FieldOffset(0)]
    public MOUSE_EVENT_RECORD MouseEvent;
    [FieldOffset(0)]
    public WINDOW_BUFFER_SIZE_RECORD WindowBufferSizeEvent;
    [FieldOffset(0)]
    public MENU_EVENT_RECORD MenuEvent;
    [FieldOffset(0)]
    public FOCUS_EVENT_RECORD FocusEvent;
}

[StructLayout(LayoutKind.Sequential)]
public struct INPUT_RECORD {
    public EventType EventType;
    public EventUnion Event;
}

public struct CONSOLE_FONT_INFO {
	public DWORD nFont;
	public COORD dwFontSize;
}

public partial class Api
{
    [DllImport("Kernel32.dll", SetLastError = true, CharSet = CharSet.Auto)]
    public static extern bool ScrollConsoleScreenBuffer(SafeHandle h, ref SMALL_RECT rect, IntPtr clip, Int32 dest, ref CHAR_INFO fill);

    [DllImport("Kernel32.dll", SetLastError = true)]
    public static extern bool SetConsoleActiveScreenBuffer(SafeHandle h);

    [DllImport("Kernel32.dll", SetLastError = true)]
    public static extern bool SetConsoleCursorPosition(SafeHandle h, COORD coord);

    [DllImport("Kernel32.dll", SetLastError = true)]
    public static extern bool SetConsoleCursorInfo(SafeHandle h, ref CONSOLE_CURSOR_INFO ci);

    [DllImport("Kernel32", SetLastError = true)]
    public static extern bool GetConsoleScreenBufferInfo(SafeHandle h, out CONSOLE_SCREEN_BUFFER_INFO sbi);

    [DllImport("Kernel32", SetLastError = true)]
	public static extern bool SetConsoleTextAttribute(SafeHandle h, UInt16 attr);

    [DllImport("Kernel32.dll", SetLastError = true)]
    public static extern SafeFileHandle CreateConsoleScreenBuffer(Rights rights, Share shareMode, IntPtr sec, ScreenBufferType type, IntPtr buf);

    [DllImport("Kernel32.dll", SetLastError = true)]
    public static extern bool GetConsoleMode(SafeHandle h, out ConsoleMode mode);

    [DllImport("Kernel32.dll", SetLastError = true)]
    public static extern bool ReadConsoleOutputCharacter(SafeHandle h, char[] buf, UInt32 nLength, COORD coord, out DWORD dwRead);

    [DllImport("Kernel32.dll", SetLastError = true)]
    public static extern bool ReadConsoleOutputCharacterA(SafeHandle h, byte[] buf, UInt32 nLength, COORD coord, out DWORD dwRead);
 
    [DllImport("Kernel32.dll", SetLastError = true)]
    public static extern bool ReadConsoleOutputAttribute(SafeHandle h, UInt16[] buf, UInt32 nLength, COORD coord, out DWORD dwRead);

    [DllImport("Kernel32.dll", SetLastError = true)]
    public static extern bool WriteConsoleOutputCharacterA(SafeHandle h, byte[] buf, UInt32 nLength, COORD coord, out DWORD dwRead);

    [DllImport("Kernel32.dll", SetLastError = true)]
    public static extern bool WriteConsoleOutputCharacter(SafeHandle h, char[] buf, UInt32 nLength, COORD coord, out DWORD dwRead);

    [DllImport("Kernel32", SetLastError = true)]
    public static extern bool WriteConsoleOutputAttribute(SafeHandle h, UInt16[] buf, UInt32 nLength, COORD coord, out DWORD dwRead);

	[DllImport("Kernel32", SetLastError = true)]
	public static extern bool WriteConsoleA(SafeHandle h, byte[] buf, UInt32 nLength, out DWORD dwRead, IntPtr reserv);

    [DllImport("Kernel32.dll", SetLastError = true)]
    public static extern bool FlushConsoleInputBuffer(SafeHandle h);

    [DllImport("Kernel32.dll")]
    public static extern UInt32 GetConsoleOutputCP();

    [DllImport("Kernel32.dll")]
    public static extern UInt32 GetConsoleCP();

    [DllImport("Kernel32.dll", SetLastError = true)]
    public static extern bool SetConsoleOutputCP(UInt32 cp);

    [DllImport("Kernel32.dll", SetLastError = true)]
    public static extern bool SetConsoleCP(UInt32 cp);

    [DllImport("Kernel32", SetLastError = true)]
	public static extern bool ReadConsoleInput(SafeHandle h, [Out] INPUT_RECORD[] ir, DWORD len, out DWORD dwRead);

    [DllImport("Kernel32", SetLastError = true)]
	public static extern bool GetNumberOfConsoleInputEvents(SafeHandle h, out DWORD dw);

	[DllImport("Kernel32", SetLastError = true)]
	public static extern bool FreeConsole();

	[DllImport("Kernel32", SetLastError = true)]
	public static extern bool GetNumberOfConsoleMouseButtons(out DWORD dwButtons);

	[DllImport("Kernel32", SetLastError = true)]
	public static extern bool GetCurrentConsoleFont(SafeHandle h, bool bNaximumWindows, out CONSOLE_FONT_INFO lpConsoleCurrentFont);
}


}
