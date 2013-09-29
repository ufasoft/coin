/*######     Copyright (c) 1997-2012 Ufasoft  http://ufasoft.com  mailto:support@ufasoft.com,  Sergey Pavlov  mailto:dev@ufasoft.com #####
# This program is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published #
# by the Free Software Foundation; either version 3, or (at your option) any later version. This program is distributed in the hope that #
# it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. #
# See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with this #
# program; If not, see <http://www.gnu.org/licenses/>                                                                                    #
########################################################################################################################################*/

﻿using System;
using System.Collections.Generic;
using System.Text;
using System.Runtime.InteropServices;

using Win32;

namespace Utils {

    public struct ErrorRange {
        public int First, Last;
        public string ModuleName;
    }

    public class Com {
        public static List<ErrorRange> Ranges = new List<ErrorRange>();

        public static string GetMessageForHR(int hr) {
            foreach (var range in Ranges) {
                if (hr >= range.First && hr <= range.Last) {
                    IntPtr hModule = Api.GetModuleHandle(range.ModuleName);
                    var sb = new StringBuilder(1024);
                    if (0 != Api.FormatMessage(FormatMessageFlags.FORMAT_MESSAGE_FROM_HMODULE
                                          | FormatMessageFlags.FORMAT_MESSAGE_ARGUMENT_ARRAY,
                                          hModule, hr, 0, sb, sb.Capacity, IntPtr.Zero)) {
                        return sb.ToString();
                    }
                }
            }
            return null;
        }

        public static void ThrowExceptionForHR(int hr, string msg) {
            string s = GetMessageForHR(hr);
            if (s != null)
                msg = s;
            var r = new ExtException(msg, hr);
            throw r;
        }

        public static void ThrowExceptionForHR(COMException x) {
            int hr = x.ErrorCode;
            ThrowExceptionForHR(hr, x.Message);
        }

        public static void Call(Action fun) {
            try {
                fun();
            }
            catch (COMException x) {
                ThrowExceptionForHR(x);
                throw;
            }
        }


        public static void Call<A1>(Action<A1> fun, A1 a1) {
            try {
                fun(a1);
            }
            catch (COMException x) {
                ThrowExceptionForHR(x);
                throw;
            }
        }

        public static void Call<A1, A2>(Action<A1, A2> fun, A1 a1, A2 a2) {
            try {
                fun(a1, a2);
            }
            catch (COMException x) {
                ThrowExceptionForHR(x);
                throw;
            }
        }

        public static void CallWrap(Action fun) {
            try {
                fun();
            }
            catch (COMException x) {
                ThrowExceptionForHR(x);
                throw;
            }
        }

        public static T CallWrap<T>(Func<T> fun) {
            try {
                return fun();
            }
            catch (COMException x) {
                ThrowExceptionForHR(x);
                throw;
            }
        }

        public static R Call<A1, R>(Func<A1, R> fun, A1 a1) {
            try {
                return fun(a1);
            }
            catch (COMException x) {
                ThrowExceptionForHR(x);
                throw;
            }
        }
    }



}
