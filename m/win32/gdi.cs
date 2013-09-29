/*######     Copyright (c) 1997-2012 Ufasoft  http://ufasoft.com  mailto:support@ufasoft.com,  Sergey Pavlov  mailto:dev@ufasoft.com #####
# This program is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published #
# by the Free Software Foundation; either version 3, or (at your option) any later version. This program is distributed in the hope that #
# it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. #
# See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with this #
# program; If not, see <http://www.gnu.org/licenses/>                                                                                    #
########################################################################################################################################*/

﻿using System;
using System.Runtime.InteropServices;

#if UCFG_CF
using SafeFileHandle = System.IntPtr;
#else
using Microsoft.Win32.SafeHandles;
#endif

using BYTE = System.Byte;
using SHORT = System.Int16;
using WORD = System.UInt16;
using DWORD = System.UInt32;
using LONG = System.Int32;
using HANDLE = System.IntPtr;

namespace Win32 {

public enum BitmapCompression : uint {
	BI_RGB     = 0,
	BI_RLE8    = 1,
	BI_RLE4    = 2, 
	BI_BITFIELDS = 3,
	BI_JPEG     = 4, 
	BI_PNG      = 5
}

public enum RasterOp : uint {
	SRCCOPY  = 0x00CC0020
}

[StructLayout(LayoutKind.Sequential)]
public struct BITMAPINFOHEADER {
	public uint biSize;
	public int biWidth;
	public int biHeight;
	public ushort biPlanes;
	public ushort biBitCount;
	public BitmapCompression biCompression;
	public uint biSizeImage;
	public int biXPelsPerMeter;
	public int biYPelsPerMeter;
	public uint biClrUsed;
	public uint biClrImportant;

	public void Init() {
		biSize = (uint)Marshal.SizeOf(this);
	}
}

public partial class Api {
	public const int GDI_ERROR = -1;

#if UCFG_CF
	private const string gdi = "coredll.dll";
#else
		private const string gdi = "gdi32.dll";
#endif


	[DllImport(gdi, SetLastError = true)]
	public static extern int StretchDIBits(IntPtr hdc, int XDest, int YDest,
	   int nDestWidth, int nDestHeight, int XSrc, int YSrc, int nSrcWidth,
	   int nSrcHeight, IntPtr lpBits, [In] ref BITMAPINFOHEADER lpBitsInfo, uint iUsage,
	  RasterOp dwRop);
}

}
