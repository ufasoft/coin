/*######     Copyright (c) 1997-2012 Ufasoft  http://ufasoft.com  mailto:support@ufasoft.com,  Sergey Pavlov  mailto:dev@ufasoft.com #####
# This program is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published #
# by the Free Software Foundation; either version 3, or (at your option) any later version. This program is distributed in the hope that #
# it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. #
# See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with this #
# program; If not, see <http://www.gnu.org/licenses/>                                                                                    #
########################################################################################################################################*/

using System;
using System.Runtime.InteropServices;

namespace Win32
{	 
	public struct RECT 
	{
		public int Left;
		public int Top;
		public int Right;
		public int Bottom;
	}
	public struct POINT 
	{
		public int x;
		public int y;
	}
	public struct SIZE 
	{
		public int cx;
		public int cy;
	}
	public struct FILETIME 
	{
		public int dwLowDateTime;
		public int dwHighDateTime;

		public long ToLong() {
			return (uint)dwLowDateTime | ((Int64)dwHighDateTime << 32);
		}
	}
	public struct SYSTEMTIME 
	{
		public short wYear;
		public short wMonth;
		public short wDayOfWeek;
		public short wDay;
		public short wHour;
		public short wMinute;
		public short wSecond;
		public short wMilliseconds;
	}
}

