using System;
using System.Runtime.InteropServices;

namespace Win32 {
    [Serializable]
    [StructLayout(LayoutKind.Sequential)]
    public struct RECT {
        public int Left;
        public int Top;
        public int Right;
        public int Bottom;
    }

    [Serializable]
    [StructLayout(LayoutKind.Sequential)]
    public struct POINT {
        public int x;
        public int y;
    }
    
    public struct SIZE {
        public int cx;
        public int cy;
    }
    
    public struct FILETIME {
        public int dwLowDateTime;
        public int dwHighDateTime;

        public long ToLong() {
            return (uint)dwLowDateTime | ((Int64)dwHighDateTime << 32);
        }
    }
    
    public struct SYSTEMTIME {
        public short wYear;
        public short wMonth;
        public short wDayOfWeek;
        public short wDay;
        public short wHour;
        public short wMinute;
        public short wSecond;
        public short wMilliseconds;
    }

    [Serializable]
    [StructLayout(LayoutKind.Sequential)]
    public struct WINDOWPLACEMENT {
        public int length;
        public int flags;
        public int showCmd;
        public POINT minPosition;
        public POINT maxPosition;
        public RECT normalPosition;
    }
}

