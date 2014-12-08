/*######     Copyright (c) 1997-2012 Ufasoft  http://ufasoft.com  mailto:support@ufasoft.com,  Sergey Pavlov  mailto:dev@ufasoft.com #####
# This program is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published #
# by the Free Software Foundation; either version 3, or (at your option) any later version. This program is distributed in the hope that #
# it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. #
# See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with this #
# program; If not, see <http://www.gnu.org/licenses/>                                                                                    #
########################################################################################################################################*/

using System;
using System.Runtime.InteropServices;
using System.Drawing;
using System.Windows.Forms;
using Microsoft.Win32.SafeHandles;



using Win32;

using Utils;

namespace GuiComp {
    


public class Con {

    static ScreenBuffer activeScreenBuffer;
    static InputBuffer inputBuffer;

    public static InputBuffer InputBuffer {
        get {
            if (inputBuffer == null) {
                SafeHandle h = Api.CreateFile("CONIN$", Rights.GENERIC_READ | Rights.GENERIC_WRITE, Share.FILE_SHARE_READ|Share.FILE_SHARE_WRITE, IntPtr.Zero, Disposition.OPEN_EXISTING, 0, IntPtr.Zero);
                Api.Win32Check(!h.IsInvalid);
                inputBuffer = new InputBuffer() {
                    Handle = h
                };
            }
            return inputBuffer;
        }
    }
    
    public static ScreenBuffer ActiveScreenBuffer {
        get {
            if (activeScreenBuffer == null) {
                SafeHandle h = Api.CreateFile("CONOUT$", Rights.GENERIC_READ | Rights.GENERIC_WRITE, Share.FILE_SHARE_READ|Share.FILE_SHARE_WRITE, IntPtr.Zero, Disposition.OPEN_EXISTING, 0, IntPtr.Zero);
                Api.Win32Check(!h.IsInvalid);
                activeScreenBuffer = new ScreenBuffer() {
                    Handle = h
                };
            }
            return activeScreenBuffer;
        }

        set {
            activeScreenBuffer = value;
            Api.Win32Check(Api.SetConsoleActiveScreenBuffer(value.Handle));
        }
    }

    public static UInt32 InputCodePage {
        get {
            return Api.GetConsoleCP();
        }
        set {
            Api.Win32Check(Api.SetConsoleCP(value));
        }
    }

    public static UInt32 OutputCodePage {
        get {
            return Api.GetConsoleOutputCP();
        }
        set {
            Api.Win32Check(Api.SetConsoleOutputCP(value));
        }
    }

	public static int NumberOfMouseButtons {
		get {
			UInt32 dw;
			Api.Win32Check(Api.GetNumberOfConsoleMouseButtons(out dw));
			return (int)dw;
		}
	}

	public static void Free() {
		Api.Win32Check(Api.FreeConsole());
	}
}





public class InputBuffer {
    internal SafeHandle Handle;

    public void Flush() {
        Api.Win32Check(Api.FlushConsoleInputBuffer(Handle));
    }

    public static InputEvent TranslateEvent(INPUT_RECORD ir) {
        switch (ir.EventType)
        {
		case EventType.KEY_EVENT:
			{
				var e = new KeyEvent() {
					KeyDown = ir.Event.KeyEvent.bKeyDown != 0,
					RepeatCount = ir.Event.KeyEvent.wRepeatCount,
					KeyCode = (Keys)ir.Event.KeyEvent.wVirtualKeyCode,
					ScanCode = ir.Event.KeyEvent.wVirtualScanCode,
					Char = (char)ir.Event.KeyEvent.uChar.AsciiChar,			//!!! Char/Unicode
					LeftAlt = (ir.Event.KeyEvent.dwControlKeyState & ControlKeyState.LEFT_ALT_PRESSED) != 0,
					RightAlt = (ir.Event.KeyEvent.dwControlKeyState & ControlKeyState.RIGHT_ALT_PRESSED) != 0,
					LeftCtrl = (ir.Event.KeyEvent.dwControlKeyState & ControlKeyState.LEFT_CTRL_PRESSED) != 0,
					RightCtrl = (ir.Event.KeyEvent.dwControlKeyState & ControlKeyState.RIGHT_CTRL_PRESSED) != 0,
					Shift = (ir.Event.KeyEvent.dwControlKeyState & ControlKeyState.SHIFT_PRESSED) != 0
				};
				return e;
			}
		case EventType.MOUSE_EVENT:
			{
				var e = new MouseEvent();
				e.X = ir.Event.MouseEvent.dwMousePosition.X;
				e.Y = ir.Event.MouseEvent.dwMousePosition.Y;
				e.Buttons = (ConMouseButtons)ir.Event.MouseEvent.dwButtonState;
				e.DoubleClick = (ir.Event.MouseEvent.dwEventFlags & 2) != 0;  //!!! Should be Sym 
				return e;
			}	
		case EventType.FOCUS_EVENT:
			return new FocusEvent();
		case EventType.MENU_EVENT:
			return new MenuEvent();
		case EventType.WINDOW_BUFFER_SIZE_EVENT:
			return new WindowBufferSizeEvent();
        default:
            throw new ApplicationException("Unknown EventType");
        }
    }

	public InputEvent ReadInputEvent() {
		var ar = new INPUT_RECORD[1];
		UInt32 dw;
		Api.Win32Check(Api.ReadConsoleInput(Handle, ar, (uint)ar.Length, out dw));
		return TranslateEvent(ar[0]);
	}

	public uint NumberOfInputEvents {
		get {
			uint r;
			Api.Win32Check(Api.GetNumberOfConsoleInputEvents(Handle, out r));
			return r;
		}
	}

    public ConsoleMode Mode {
        get {
            ConsoleMode r;
            Api.Win32Check(Api.GetConsoleMode(Handle, out r));
            return r;
        }
    }

    public bool InsertMode {
        get {
            return (Mode & ConsoleMode.ENABLE_INSERT_MODE) != 0;
        }
    }

}

public class ScreenBuffer {
    internal SafeHandle Handle;

	public UInt16 Attributes {
		get {
			CONSOLE_SCREEN_BUFFER_INFO sbi;
			Api.Win32Check(Api.GetConsoleScreenBufferInfo(Handle, out sbi));
			return sbi.wAttributes;
		}
		set {
			Api.Win32Check(Api.SetConsoleTextAttribute(Handle, value));
		}
	}

    public Point CursorPosition {
        get {
            CONSOLE_SCREEN_BUFFER_INFO sbi;
            Api.Win32Check(Api.GetConsoleScreenBufferInfo(Handle, out sbi));
            return new Point(sbi.dwCursorPosition.X, sbi.dwCursorPosition.Y);
        }
        set {
            var coord = new COORD() {
                X = (short)value.X,
                Y = (short)value.Y
            };
            Api.Win32Check(Api.SetConsoleCursorPosition(Handle, coord));
        }
    }

    public void SetCursorInfo(int size, bool visible) {
        var ci = new CONSOLE_CURSOR_INFO() {
            dwSize = (uint)size,
            bVisible = visible
        };
        Api.Win32Check(Api.SetConsoleCursorInfo(Handle, ref ci));
    }

    public static ScreenBuffer Create() {
        SafeHandle h = Api.CreateConsoleScreenBuffer(Rights.GENERIC_READ | Rights.GENERIC_WRITE, Share.FILE_SHARE_READ|Share.FILE_SHARE_WRITE, IntPtr.Zero, ScreenBufferType.CONSOLE_TEXTMODE_BUFFER, IntPtr.Zero);
        Api.Win32Check(!h.IsInvalid);
        return new ScreenBuffer() {
            Handle = h
        };
    }

    public void Scroll(Rectangle rect, Rectangle? clip, Point origin, CHAR_INFO fill) {
        var r = new SMALL_RECT() {
            Left = (short)rect.Left,
            Top = (short)rect.Top,
            Right = (short)rect.Right,
            Bottom = (short)rect.Bottom
        };

        var d = new COORD() {
            X = (short)origin.X,
            Y = (short)(origin.Y) //!!!
        };

        Api.Win32Check(Api.ScrollConsoleScreenBuffer(Handle, ref r , IntPtr.Zero, d.ToInt32(), ref fill));
    }

    public void Scroll(Rectangle rect, Rectangle? clip, Point origin) {
        var ci = new CHAR_INFO();
        ci.Char = ' ';
        Scroll(rect, clip, origin, ci);
    }

    public char GetChar(int x, int y) {
        var ar = new char[1];

        var coord = new COORD() {
            X = (short)x,
            Y = (short)y
        };
        UInt32 dw;
        Api.Win32Check(Api.ReadConsoleOutputCharacter(Handle, ar, 1, coord, out dw));
        return ar[0];
    }

    public byte GetCharByte(int x, int y) {
        var ar = new byte[1];

        var coord = new COORD() {
            X = (short)x,
            Y = (short)y
        };
        UInt32 dw;
        Api.Win32Check(Api.ReadConsoleOutputCharacterA(Handle, ar, 1, coord, out dw));
        return ar[0];
    }

    public UInt16 GetAttr(int x, int y) {
        var ar = new UInt16[1];

        var coord = new COORD() {
            X = (short)x,
            Y = (short)y
        };
        UInt32 dw;
        Api.Win32Check(Api.ReadConsoleOutputAttribute(Handle, ar, 1, coord, out dw));
        return ar[0];
    }

    public void SetChar(int x, int y, char ch) {
        var ar = new char[] { ch };

        var coord = new COORD() {
            X = (short)x,
            Y = (short)y
        };
        UInt32 dw;
        Api.Win32Check(Api.WriteConsoleOutputCharacter(Handle, ar, 1, coord, out dw));
    }

    public void SetCharByte(int x, int y, byte ch) {
        var ar = new byte[] { (byte)ch };

        var coord = new COORD() {
            X = (short)x,
            Y = (short)y
        };
        UInt32 dw;
        Api.Win32Check(Api.WriteConsoleOutputCharacterA(Handle, ar, 1, coord, out dw));
    }

    public void SetAttr(int x, int y, UInt16 attr) {
        var ar = new UInt16[] { attr };

        var coord = new COORD() {
            X = (short)x,
            Y = (short)y
        };
        UInt32 dw;
        Api.Win32Check(Api.WriteConsoleOutputAttribute(Handle, ar, 1, coord, out dw));
    }

	public void Write(byte[] ar) {
		UInt32 dw;
		Api.Win32Check(Api.WriteConsoleA(Handle, ar, (uint)ar.Length, out dw, IntPtr.Zero));
	}
}


}


