/*######     Copyright (c) 1997-2012 Ufasoft  http://ufasoft.com  mailto:support@ufasoft.com,  Sergey Pavlov  mailto:dev@ufasoft.com #####
# This program is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published #
# by the Free Software Foundation; either version 3, or (at your option) any later version. This program is distributed in the hope that #
# it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. #
# See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with this #
# program; If not, see <http://www.gnu.org/licenses/>                                                                                    #
########################################################################################################################################*/

﻿using System;
using System.IO;
using System.Runtime.InteropServices;
using System.Drawing;
using System.Drawing.Imaging;
using System.Windows.Forms;

using Win32;

namespace Utils {

	public class PerformanceCounter {
		public static long Frequency {
			get {
				long r;
				Api.Win32Check(Api.QueryPerformanceFrequency(out r));
				return r;
			}
		}

		public static long Counter {
			get {
				long r;
				Api.Win32Check(Api.QueryPerformanceCounter(out r));
				return r;
			}
		}
	}

	public class Dib {
		public static void Stretch(Graphics g, Rectangle rcSrc, Rectangle dest, BitmapData bd) {
			var ar = new byte[Marshal.SizeOf(typeof(BITMAPINFOHEADER))+12];
			unsafe {

				fixed (byte* par = ar) {
					var pInfo = (BITMAPINFOHEADER*)par;
					pInfo->Init();
					pInfo->biWidth = bd.Width;
					pInfo->biHeight = -bd.Height;
					pInfo->biPlanes = 1;
					pInfo->biBitCount = 32;
					pInfo->biCompression = BitmapCompression.BI_BITFIELDS;
					pInfo->biSizeImage = (uint)(bd.Stride * bd.Height);
					UInt32* masks = (UInt32*)(par + Marshal.SizeOf(typeof(BITMAPINFOHEADER)));
					masks[0] = 0xFF0000;
					masks[1] = 0xFF00;
					masks[2] = 0xFF;

					IntPtr hdc = g.GetHdc();
					try {						
						int r = Api.StretchDIBits(hdc, dest.Left, dest.Top, dest.Width, dest.Height,
								rcSrc.Left, rcSrc.Top, rcSrc.Width, rcSrc.Height, bd.Scan0, ref *pInfo, 0, RasterOp.SRCCOPY);
						Api.Win32Check(Api.GDI_ERROR != r);
					} finally {
						g.ReleaseHdc(hdc);
					}
				}
			}
		}
	}


#if UCFG_CF

	public static partial class CF_Extensions {
		public static FileAttributes GetAttributes(this File file, string path) {
			uint r = Api.GetFileAttributes(path);
			Api.Win32Check(r != Api.INVALID_FILE_ATTRIBUTES);
			return (FileAttributes)r;
		}
	}
#endif

	public abstract class InputEvent {
	}

	public class KeyEvent : InputEvent {
		public bool KeyDown;
		public ushort RepeatCount;
		public Keys KeyCode;
		public ushort ScanCode;
		public char Char;

		public bool LeftAlt, RightAlt, LeftCtrl, RightCtrl, Shift;

		public bool Alt {
			get {
				return LeftAlt || RightAlt;
			}

			set {
				LeftAlt = RightAlt = value;
			}
		}

		public bool Ctrl {
			get {
				return LeftCtrl || RightCtrl;
			}

			set {
				LeftCtrl = RightCtrl = value;
			}
		}

		//!!!    public ControlKeyState ControlKeyState;
	}

	[Flags]
	public enum ConMouseButtons {
		None = 0,
		Left = 1,
		Right = 2,
		Left2 = 4,
		Left3 = 8,
		Left4 = 0x10,
	}

	public class MouseEvent : InputEvent {
		public ConMouseButtons Buttons;
		public int Clicks, X, Y;
		public bool DoubleClick;
	}

	public class FocusEvent : InputEvent {
	}

	public class MenuEvent : InputEvent {
	}

	public class WindowBufferSizeEvent : InputEvent {
	}

	[Flags]
	public enum KeyModifiers {
		Alt = 1,
		Shift = 2,
		Control = 4,
	}

}
