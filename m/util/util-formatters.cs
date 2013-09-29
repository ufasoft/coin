/*######     Copyright (c) 1997-2012 Ufasoft  http://ufasoft.com  mailto:support@ufasoft.com,  Sergey Pavlov  mailto:dev@ufasoft.com #####
# This program is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published #
# by the Free Software Foundation; either version 3, or (at your option) any later version. This program is distributed in the hope that #
# it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. #
# See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with this #
# program; If not, see <http://www.gnu.org/licenses/>                                                                                    #
########################################################################################################################################*/

/***************************************************************************
 * This software written by Ufasoft  http://www.ufasoft.com                *
 * It is Public Domain and can be used in any Free or Commercial projects  *
 * with keeping these License lines in Help, Documentation, About Dialogs  *
 * and Source Code files, derived from this.                               *
 * ************************************************************************/

using System;
using System.Threading;
using System.Collections;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Net;
using System.Text;
using System.Globalization;
using System.Text.RegularExpressions;
using System.Runtime.Serialization.Formatters.Binary;
using System.Runtime.InteropServices;
using System.Collections.Specialized;
using System.Net.Sockets;


namespace Utils {

	public partial class Ut {

		public static byte[] ToArray(IWritable obj) {
			MemoryStream stm = new MemoryStream();
			obj.Write(new BinaryWriter(stm));
			//!!!new BinaryFormatter().Serialize(stm,obj);
			return stm.ToArray();
		}

		public static byte[] ObjToArray(object obj) {
			MemoryStream stm = new MemoryStream();
			new BinaryFormatter().Serialize(stm, obj);
			return stm.ToArray();
		}

		public static object FromData(byte[] data) {
			return new BinaryFormatter().Deserialize(new MemoryStream(data));
		}


	}

	public class NumberFormatter {
		public int N = 2;

		public virtual string ToString(object x) {
			int v = (int)x;
			return PadResult(string.Format("{0:X}", v));
		}

		protected string PadResult(string s) {
			return s.PadLeft(N, '0');
		}
	};

	public class OctalNumberFormatter : NumberFormatter {
		public OctalNumberFormatter() {
			N = 6;
		}

		public override string ToString(object x) {
			int v;
			if (x is bool)
				return x.ToString();
			else if (x is UInt16)
				v = (UInt16)x;
			else if (x is int)
				v = (int)x;
			else
				v = (int)(long)x;
			return PadResult(Convert.ToString(v, 8));
		}
	}

}