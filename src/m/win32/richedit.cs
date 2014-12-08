/*######     Copyright (c) 1997-2012 Ufasoft  http://ufasoft.com  mailto:support@ufasoft.com,  Sergey Pavlov  mailto:dev@ufasoft.com #####
# This program is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published #
# by the Free Software Foundation; either version 3, or (at your option) any later version. This program is distributed in the hope that #
# it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. #
# See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with this #
# program; If not, see <http://www.gnu.org/licenses/>                                                                                    #
########################################################################################################################################*/

using System;
using System.Runtime.InteropServices;
using System.Text;

using HANDLE = System.IntPtr;
using HWND = System.IntPtr;
using HDC = System.IntPtr;

namespace Win32
{
	[Flags]
	public enum ScrollInfoFlags : int
	{
		SIF_RANGE = 0x0001,
		SIF_PAGE = 0x0002,
		SIF_POS = 0x0004,
		SIF_DISABLENOSCROLL = 0x0008,
		SIF_TRACKPOS = 0x0010,
		SIF_ALL = (SIF_RANGE | SIF_PAGE | SIF_POS | SIF_TRACKPOS)
	}

	[Flags]
	public enum RichEditMask : int
	{
		ENM_NONE = 0x00000000,
		ENM_CHANGE = 0x00000001,
		ENM_UPDATE	=	0x00000002,
		ENM_SCROLL	=			0x00000004,
		ENM_SCROLLEVENTS	=	0x00000008,
		ENM_DRAGDROPDONE	=	0x00000010,
		ENM_PARAGRAPHEXPANDED	= 0x00000020,
		ENM_PAGECHANGE	=		0x00000040,
		ENM_KEYEVENTS		=	0x00010000,
		ENM_MOUSEEVENTS	=		0x00020000,
		ENM_REQUESTRESIZE	=	0x00040000,
		ENM_SELCHANGE		=	0x00080000,
		ENM_DROPFILES	=		0x00100000,
		ENM_PROTECTED	=		0x00200000,
		ENM_CORRECTTEXT	=		0x00400000,		// PenWin specific 
		ENM_IMECHANGE		=	0x00800000,		// Used by RE1.0 compatibility
		ENM_LANGCHANGE	=		0x01000000,
		ENM_OBJECTPOSITIONS	=	0x02000000,
		ENM_LINK	=			0x04000000,
		ENM_LOWFIRTF		=	0x08000000,
	}

	public class RichEdit
	{
		public const int EM_GETSCROLLPOS = (int)WM.WM_USER + 221;
		public const int EM_SETSCROLLPOS = (int)WM.WM_USER + 222;

		[DllImport("user32")]
		public static extern int SendMessage(HWND hwnd, int wMsg, int wParam, ref POINT p);

	}	
}
	
	