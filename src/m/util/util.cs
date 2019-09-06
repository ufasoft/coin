/*######   Copyright (c) 2011-2015 Ufasoft  http://ufasoft.com  mailto:support@ufasoft.com,  Sergey Pavlov  mailto:dev@ufasoft.com ####
#                                                                                                                                     #
#       See LICENSE for licensing information                                                                                         #
#####################################################################################################################################*/

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
using System.Runtime.InteropServices;
using System.Collections.Specialized;
using System.Net.Sockets;

namespace Utils {
    public delegate void SimpleDelegateHandler();
    public delegate void WriteLineDelegateHandler(string s, params object[] args);

    [Serializable]
    public abstract class IWritable {
        public virtual void Read(BinaryReader r) {
        }

        public virtual void Write(BinaryWriter w) {
        }
    }

#if UCFG_CF
    public static class CF_Extensions {
        public static string GetString(this Encoding enc, byte[] bytes) {
            return enc.GetString(bytes, 0, bytes.Length);
        }
    }
#endif


    public struct Pair<T, U> {
        public T First;
        public U Second;

        public Pair(T f, U s) {
            First = f;
            Second = s;
        }

        public override int GetHashCode() {
            return First.GetHashCode() ^ Second.GetHashCode();
        }

        public override bool Equals(object obj) {
            var pp = (Pair<T, U>)obj;
            return First.Equals(pp.First) && Second.Equals(pp.Second);
        }

        public override string ToString() {
            return string.Format("[{0}, {1}]", First, Second);
        }
    }


    public partial class Ut {

        public static void Swap<T>(ref T a, ref T b) {
            T t = a;
            a = b;
            b = t;
        }


        public static byte[] ReadBytesToEnd(Stream stm) {
            MemoryStream ms = new MemoryStream();
            for (int b; (b = stm.ReadByte()) != -1; ) {
                ms.WriteByte((byte)b);
            }
            return ms.ToArray();
        }

        public static void SkipWhiteSpace(TextReader r) {
            for (int ch; (ch = r.Peek()) != -1 && char.IsWhiteSpace((char)ch); )
                r.Read();
        }

        public static void StreamCpy(Stream dst, Stream src) {
            byte[] buffer = new byte[1024];
            for (int n; (n = src.Read(buffer, 0, buffer.Length)) != 0; )
                dst.Write(buffer, 0, n);
        }

        public static string DateTimeToString(DateTime dt) {
            return Convert.ToString(dt, new CultureInfo("RU-ru"));
        }

        public static DateTime StringToDateTime(string s) {
            return Convert.ToDateTime(s, new CultureInfo("RU-ru"));
        }

        public static string ReadOneLineFromStream(Stream stm) {
            StringBuilder sb = new StringBuilder();
            //          bool bCR = false;
            for (int i = 0; i < 1024; i++) {
                int b = stm.ReadByte();
                switch (b) {
                    case -1:
                        throw new ApplicationException();
                    case '\n':
                        return sb.ToString();
                    case '\r':
                        break;
                    default:
                        sb.Append((char)b);
                        break;
                }
            }
            throw new ApplicationException();
        }

        static Regex s_reHtmlInput = new Regex(@"\<input[^>]+name=""?(?<Name>[^""\s>]+)[^>]+value=""(?<Val>[^""]+)""[^>]*\>"
                                                                        , RegexOptions.Singleline | RegexOptions.IgnoreCase | RegexOptions.Compiled);

        public static NameValueCollection HtmlInputs(string html) {
            NameValueCollection nvc = new NameValueCollection();
            foreach (Match m in s_reHtmlInput.Matches(html))
                nvc[m.Groups["Name"].Value] = m.Groups["Val"].Value;
            return nvc;
        }

        static Regex s_reRowIndexView = new Regex(@"\<tr[^>]+RowIndxView.+?/tr\>", RegexOptions.Singleline | RegexOptions.IgnoreCase | RegexOptions.Compiled);
        static Regex s_reRowTD = new Regex(@"\<td[^>]*>\s*(?<Val>.*?)\s*\</td\>", RegexOptions.Singleline | RegexOptions.IgnoreCase | RegexOptions.Compiled);

        public static string[][] HtmlGrid(string html) {
            MatchCollection matches = s_reRowIndexView.Matches(html);
            string[][] r = new string[matches.Count][];
            int i = 0;
            foreach (Match m in matches) {
                string row = m.Value;
                //              Console.WriteLine(row); //!!!D
                MatchCollection rm = s_reRowTD.Matches(row);
                string[] ar = new string[rm.Count];
                int j = 0;
                foreach (Match n in rm)
                    ar[j++] = n.Groups["Val"].Value;
                r[i++] = ar;
            }
            return r;
        }




        /*!!!D
        string[] ReadHttpHeader(Stream stm)
        {
            StreamReader r = new StreamReader(stm);
            string line; //!!! verify returned code 200
            ArrayList lines = new ArrayList();
            while ((line=r.ReadLine()) != "")
                lines.Add(line);
            string[] res = new string[lines.Count];
            for (int i=0; i<lines.Count; i++)
                res[i] = (string)lines[i];
            return res;
        }*/

        public static WebHeaderCollection ReadHttpHeader(Stream stm) {
            WebHeaderCollection h = new WebHeaderCollection();
            for (string line = ReadOneLineFromStream(stm); line != ""; ) {
                string s = line;
                while (true) {
                    line = ReadOneLineFromStream(stm);
                    if (line.Length == 0 || !Char.IsWhiteSpace(line[0]))
                        break;
                    s += line;
                }
                h.Add(s);
            }
            return h;
        }

        public static void WriteMailHeader(NameValueCollection hh, TextWriter w) {
            foreach (string key in hh)
                w.WriteLine("{0}: {1}", key, hh[key]);
        }

        public static void ReadMailHeader(NameValueCollection hh, TextReader r) {
            Regex re = new Regex(@"([^:]+):\s*(.*)");
            for (string line = r.ReadLine(); line != ""; ) {
                Match m = re.Match(line);
                if (!m.Success)
                    throw new ApplicationException("Invalid Mail Header");
                string name = m.Groups[1].Value;
                string value = m.Groups[2].Value;
                while ((line = r.ReadLine()).Length > 0 && Char.IsWhiteSpace(line[0]))
                    value += line.Trim();
                hh[name] = value;
            }
        }

        public static IPEndPoint ParseHostPort(string s) {
            Match m = Regex.Match(s, @"([^:]+)(:(\d+))?");
            string sHost = m.Groups[1].Value;
            int port = 0;
            if (m.Groups[3].Success)
                port = Convert.ToUInt16(m.Groups[3].Value);
            IPAddress a;
            if (Regex.Match(sHost, @"\d+\.\d+\.\d+\.\d+").Success)
                a = IPAddress.Parse(sHost);
            else
                a = Dns.GetHostEntry(sHost).AddressList[0];
            return new IPEndPoint(a, port);
        }

        public static bool IsGlobal(IPAddress addr) {
            byte[] ar = addr.GetAddressBytes();
            int hip = IPAddress.NetworkToHostOrder((int)Ut.PeekUInt32(ar, 0));
            return (hip & 0xFF000000) != 0x7F000000     // 127.x.x.x
                && (hip & 0xFFFF0000) != 0xC0A80000     // 192.168.x.x
                && (hip & 0xFFF00000) != 0xAC100000     // 172.16.x.x
                && (hip & 0xFF000000) != 0x0A000000;    // 10.x.x.x
        }

        public static IPAddress GetGlobalIPAddress() {
            foreach (IPAddress a in Dns.GetHostEntry(Dns.GetHostName()).AddressList)
                if (IsGlobal(a))
                    return a;
            return null;
        }

        public static void Throw() {
            throw new Exception();
        }

        public static IPEndPoint ReadEndPoint(BinaryReader r) {
            long addr = r.ReadUInt32();
            if (addr == 0)
                return null;
            int port = r.ReadInt16();
            return new IPEndPoint(addr, port);
        }

        public static Guid ReadGuid(BinaryReader r) {
            return new Guid(r.ReadBytes(16));
        }

        public static byte[] ReadByteArray(BinaryReader r) {
            int len = r.ReadInt32();
            return len == -1 ? null : r.ReadBytes(len);
        }

        public static DateTime ReadDateTime(BinaryReader r) {
            return new DateTime(r.ReadInt64());
        }


        public static string ReadSZ(BinaryReader r) {
            List<byte> ar = new List<byte>();
            for (byte b; (b = r.ReadByte()) != 0; )
                ar.Add(b);
            return Encoding.ASCII.GetString(ar.ToArray());
        }

        public static void Write(BinaryWriter w, IPEndPoint ep) {
            if (ep != null) {
                byte[] ar = ep.Address.GetAddressBytes();
                Int32 ip = ar[0] | (ar[1] << 8) | (ar[2] << 16) | (ar[3] << 24);
                w.Write(ip);
                w.Write((Int16)ep.Port);
            }
            else
                w.Write((Int32)0);
        }

        public static void Write(BinaryWriter w, Guid guid) {
            w.Write(guid.ToByteArray());
        }

        public static void Write(BinaryWriter w, byte[] ar) {
            if (ar == null)
                w.Write((int)-1);
            else {
                w.Write(ar.Length);
                w.Write(ar);
            }
        }

        public static void Write(BinaryWriter w, DateTime dt) {
            w.Write(dt.Ticks);
        }

        public static bool Equals(byte[] ar1, byte[] ar2) {
            if (ar1 == null || ar2 == null || ar1.Length != ar2.Length)
                return false;
            for (int i = 0; i < ar1.Length; i++)
                if (ar1[i] != ar2[i])
                    return false;
            return true;
        }

#if !UCFG_CF
        public static string GetProcessOutput(string fileName, string args) {
            Process p = new Process();
            p.StartInfo.UseShellExecute = false;
            p.StartInfo.RedirectStandardOutput = true;
            p.StartInfo.FileName = fileName;
            p.StartInfo.Arguments = args;
            p.Start();
            string r = p.StandardOutput.ReadToEnd();
            p.WaitForExit();
            return r;
        }
#endif

        public static string ToString(byte[] ar) {
            var sb = new StringBuilder();
            foreach (var x in ar)
                sb.AppendFormat("{0:X2}", x);
            return sb.ToString();
        }

        public static byte[] FromHexToByteArray(string s) {
            var r = new byte[s.Length / 2];
            for (int i = 0; i < r.Length; ++i)
                r[i] = Convert.ToByte(s.Substring(i * 2, 2), 16);
            return r;
        }

        public static DateTime DateTimeFromUnix(long sec) {
            return new DateTime(1970, 1, 1) + TimeSpan.FromSeconds(sec);
        }

        public static long DateTimeToUnix(DateTime dt) {
            return (long)(dt - new DateTime(1970, 1, 1)).TotalSeconds;
        }

        public static bool IsAscii(string s) {
            foreach (char ch in s)
                if ((int)ch > 127)
                    return false;
            return true;
        }

        public static UInt16 PeekUInt16(byte[] ar, int idx) {
            return (UInt16)(ar[idx] | (ar[idx + 1] << 8));
        }

        public static UInt32 PeekUInt32(byte[] ar, int idx) {
            return (UInt32)(ar[idx] | (ar[idx + 1] << 8) | (ar[idx + 2] << 16) | (ar[idx + 3] << 24));
        }

        public static UInt64 PeekUInt64(byte[] ar, int idx) {
            return (UInt64)((long)ar[idx] | ((long)ar[idx + 1] << 8) | ((long)ar[idx + 2] << 16) | ((long)ar[idx + 3] << 24)
                | ((long)ar[idx + 4] << 32) | ((long)ar[idx + 5] << 40) | ((long)ar[idx + 6] << 48) | ((long)ar[idx + 7] << 56));
        }

        public static void Poke(byte[] ar, int idx, UInt16 v) {
            ar[idx] = (byte)v;
            ar[idx+1] = (byte)(v>>8);
        }

        public static void Poke(byte[] ar, int idx, UInt32 v) {
            ar[idx] = (byte)v;
            ar[idx+1] = (byte)(v >> 8);
            ar[idx+2] = (byte)(v >> 16);
            ar[idx+3] = (byte)(v >> 24);
        }

        public static void Poke(byte[] ar, int idx, UInt64 v) {
            ar[idx] = (byte)v;
            ar[idx + 1] = (byte)(v >> 8);
            ar[idx + 2] = (byte)(v >> 16);
            ar[idx + 3] = (byte)(v >> 24);
            ar[idx + 4] = (byte)(v >> 32);
            ar[idx + 5] = (byte)(v >> 40);
            ar[idx + 6] = (byte)(v >> 48);
            ar[idx + 7] = (byte)(v >> 56);
        }

        public static int TraceLevel = 1;
    }


    public delegate void ExceptionHandler(Exception e);

    public interface IExceptable {
        void ExceptionRaised(Exception e);
    }

    public class IThreadable {
        public Thread Thread;
        internal ThreadMan m_ThreadMan;
        public EventWaitHandle StopEvent = new EventWaitHandle(false, EventResetMode.ManualReset);

        public volatile bool bStop;
        public int SleepTime = 60000;

        public virtual void Start() {
            (Thread = new Thread(new ThreadStart(Execute))).Start();
        }

        public virtual void Stop() {
            bStop = true;
            StopEvent.Set();
#if !UCFG_CF
            if (Thread != null)
                Thread.Interrupt();
#endif
        }

        public virtual void Join() {
            if (Thread != null) {
                Thread.Join();
                //        Logger.Log("Joined {0}",this);
            }
        }

        protected virtual void VExecute() {
        }

        protected virtual void VExecuteNonLoop() {
            while (!bStop) {
                try {
                    VExecute();
                }
                catch (Exception e) {
                    Debug.Print("{0}", e);
                    if (this is IExceptable)
                        ((IExceptable)this).ExceptionRaised(e);
                }
                if (StopEvent.WaitOne(SleepTime, false))
                    break;
            }
        }

        void Execute() {
            try {
                VExecuteNonLoop();
            }
            catch (Exception) {
            }
            finally {
                //!!!Logger.Log("Terminating {0}",this);
                ThreadMan tm = m_ThreadMan;
                if (tm != null)
                    tm.Remove(this);
                //!!!Logger.Log("Terminated {0}",this);
            }
        }
    }

    public class Log {
        public static bool PrintDate = true;
        public static TextWriter TextWriter = Console.Error;

        public static event WriteLineDelegateHandler OnWriteLine;

        public static bool FlushLog() {
            TextWriter.Flush();
            return true;
        }

        static public void WriteLine(string s, params object[] args) {
            var sb = new StringBuilder();
            if (PrintDate)
                sb.AppendFormat("{0}\t", DateTime.Now);
            sb.AppendFormat(s, args);
            string line = sb.ToString();
            TextWriter.Write(line);
            TextWriter.Write("\n");
            TextWriter.Flush(); //!!!
            if (OnWriteLine != null)
                OnWriteLine(line);
        }

        static public void WriteLine(object x) {
            WriteLine("{0}", x);
            //          TextWriter.Write("{0}\t", DateTime.Now);
            //          TextWriter.WriteLine(x);
            //          TextWriter.Flush();
        }
    }

    /*!!!
    public class Trace {
        public static TextWriter TextWriter = Console.Error;

        public static event WriteLineDelegateHandler OnWriteLine;

        [Conditional("TRACE")]
        static public void WriteLine(string s, params object[] args) {
            string line = string.Format("{0}\t", DateTime.Now) + string.Format(s, args);
            TextWriter.Write(line + "\n");
            TextWriter.Flush();
            if (OnWriteLine != null)
                OnWriteLine(line);
        }

        [Conditional("TRACE")]
        static public void WriteLine(object x) {
            WriteLine("{0}", x);
            //          TextWriter.Write("{0}\t", DateTime.Now);
            //          TextWriter.WriteLine(x);
            //          TextWriter.Flush();
        }
    } */

    public class ThreadMan {
        ArrayList ar = new ArrayList();

        bool bTerminating;

        public void Add(IThreadable th) {
            if (bTerminating)
                throw new Exception();
            th.m_ThreadMan = this;
            th.Start();
            lock (ar)
                ar.Add(th);
        }

        public void SignalStop() {
            bTerminating = true;
            lock (ar)
                foreach (IThreadable th in ar)
                    th.Stop();
        }

        internal void Remove(IThreadable th) {
            lock (ar)
                ar.Remove(th);
        }

        public void Join() {
            ArrayList aJoin;
            lock (ar) {
                Thread.Sleep(100);
                aJoin = (ArrayList)ar.Clone();
            }
            //      foreach (IThreadable th in aJoin)
            //        Logger.Log("Joining {0}",th);//!!!
            foreach (IThreadable th in aJoin)
                th.Join();
        }

        public void Stop() {
            SignalStop();
            Join();
        }
    }

    public sealed class GCBeep {
        ~GCBeep() {
            MessageBeep(-1);
/*!!!
            if (!Environment.HasShutdownStarted)
                new GCBeep();
 * */
        }

        [DllImport("user32")]
        private extern static bool MessageBeep(int uType);
    }


    public class ConsoleCtrl : IDisposable {
        public enum ConsoleEvent {
            CTRL_C = 0,     // From wincom.h
            CTRL_BREAK = 1,
            CTRL_CLOSE = 2,
            CTRL_LOGOFF = 5,
            CTRL_SHUTDOWN = 6
        }

        public delegate void ControlEventHandler(ConsoleEvent consoleEvent);

        public event ControlEventHandler ControlEvent;

        ControlEventHandler eventHandler;

        public ConsoleCtrl() {
            // save this to a private var so the GC doesn't collect it...
            eventHandler = new ControlEventHandler(Handler);
            SetConsoleCtrlHandler(eventHandler, true);
        }

        ~ConsoleCtrl() {
            Dispose(false);
        }

        public void Dispose() {
            Dispose(true);
        }

        void Dispose(bool disposing) {
            if (disposing) {
                GC.SuppressFinalize(this);
            }
            if (eventHandler != null) {
                SetConsoleCtrlHandler(eventHandler, false);
                eventHandler = null;
            }
        }

        private void Handler(ConsoleEvent consoleEvent) {
            if (ControlEvent != null)
                ControlEvent(consoleEvent);
        }

        [DllImport("kernel32.dll")]
        static extern bool SetConsoleCtrlHandler(ControlEventHandler e, bool add);
    }

    public class SocketLoop {
        public static void Loop(Socket sockS, Socket sockD) {
            byte[] buf = new byte[32000];
            List<Socket> ar = new List<Socket>(),
                                     rest = new List<Socket>();
            rest.Add(sockS);
            rest.Add(sockD);
            while (true) {
                ar.Clear();
                foreach (Socket s in rest)
                    ar.Add(s);
                Socket.Select(ar, null, null, 1000000000); // -1 don't works
                if (ar.Count == 0)
                    continue;
                //                  Console.Error.WriteLine("Select returned {0}", ar.Count);
                foreach (Socket s in ar) {
                    Socket d = s == sockS ? sockD : sockS;
                    //                      Console.Error.Write(s == sockS ? "In " : "Out ");
                    int n = s.Receive(buf);
                    //                      Console.Error.WriteLine("recv {0}", n);
                    if (n > 0)
                        d.Send(buf, n, 0);
                    else {
                        d.Shutdown(SocketShutdown.Send);
                        rest.Remove(s);
                    }
                }
                if (rest.Count == 0)
                    break;
            }
        }


    }


    /// <summary>
    /// Arguments class
    /// </summary>
    public class Arguments {
        // Variables
        private StringDictionary Parameters;

        // Constructor
        public Arguments(string[] Args) {
            Parameters = new StringDictionary();
            Regex Spliter = new Regex(@"^-{1,2}|^/|=|:",
                RegexOptions.IgnoreCase | RegexOptions.Compiled);

            Regex Remover = new Regex(@"^['""]?(.*?)['""]?$",
                RegexOptions.IgnoreCase | RegexOptions.Compiled);

            string Parameter = null;
            string[] Parts;

            // Valid parameters forms:
            // {-,/,--}param{ ,=,:}((",')value(",'))
            // Examples:
            // -param1 value1 --param2 /param3:"Test-:-work"
            //   /param4=happy -param5 '--=nice=--'
            foreach (string Txt in Args) {
                // Look for new parameters (-,/ or --) and a
                // possible enclosed value (=,:)
                Parts = Spliter.Split(Txt, 3);

                switch (Parts.Length) {
                    // Found a value (for the last parameter
                    // found (space separator))
                    case 1:
                        if (Parameter != null) {
                            if (!Parameters.ContainsKey(Parameter)) {
                                Parts[0] =
                                    Remover.Replace(Parts[0], "$1");

                                Parameters.Add(Parameter, Parts[0]);
                            }
                            Parameter = null;
                        }
                        // else Error: no parameter waiting for a value (skipped)
                        break;

                    // Found just a parameter
                    case 2:
                        // The last parameter is still waiting.
                        // With no value, set it to true.
                        if (Parameter != null) {
                            if (!Parameters.ContainsKey(Parameter))
                                Parameters.Add(Parameter, "true");
                        }
                        Parameter = Parts[1];
                        break;

                    // Parameter with enclosed value
                    case 3:
                        // The last parameter is still waiting.
                        // With no value, set it to true.
                        if (Parameter != null) {
                            if (!Parameters.ContainsKey(Parameter))
                                Parameters.Add(Parameter, "true");
                        }

                        Parameter = Parts[1];

                        // Remove possible enclosing characters (",')
                        if (!Parameters.ContainsKey(Parameter)) {
                            Parts[2] = Remover.Replace(Parts[2], "$1");
                            Parameters.Add(Parameter, Parts[2]);
                        }

                        Parameter = null;
                        break;
                }
            }
            // In case a parameter is still waiting
            if (Parameter != null) {
                if (!Parameters.ContainsKey(Parameter))
                    Parameters.Add(Parameter, "true");
            }
        }

        // Retrieve a parameter value if it exists
        // (overriding C# indexer property)
        public string this[string Param] => (Parameters[Param]);

    }

    public class Oct {
        public const UInt16
          o14 = 12,
          o20 = 16,
          o27 = 23,
          o30 = 24,
          o34 = 28,
          o37 = 31,
          o67 = 55,
          o70 = 56,
          o77 = 63,
          o100 = 64,
          o200 = 128,
          o240 = 160,
          o241 = 161,
          o242 = 162,
          o250 = 168,
          o254 = 172,
          o257 = 175,
          o260 = 176,
          o261 = 177,
          o262 = 178,
          o264 = 180,
          o270 = 184,
          o277 = 191,
          o300 = 192,
          o377 = 255,
          o400 = 256,
          o700 = 448,
          o4000 = 2048,
          o4400 = 2304,
          o5000 = 2560,
          o6000 = 3072,
          o7000 = 3584,
          o7700 = 4032,
          o70000 = 28672,
          o74000 = 30720,
          o100000 = 32768,
          o104000 = 34816,
          o104400 = 35072,
          o107000 = 36352,
          o120000 = 40960,
          o177400 = 65280,
          o177700 = 65472,
          o177760 = 65520,
          o177770 = 65528;
    }

    public class ExtException : ApplicationException {
        public ExtException(string msg, int errorCode)
            : base(msg) {
            HResult = errorCode;
        }

        public ExtException(string msg)
            : base(msg) {
            HResult = unchecked((int)0x80004005);            // E_FAIL
        }
    }

    public enum DtmfKey {
        Key_0,
        Key_1,
        Key_2,
        Key_3,
        Key_4,
        Key_5,
        Key_6,
        Key_7,
        Key_8,
        Key_9,
        Key_Star,
        Key_Pound,
        Key_A,
        Key_B,
        Key_C,
        Key_D,
        Key_Flash
    }


    public class PasswordGenerator {
        public int Length = 8;
        public string Chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZ" + "abcdefghijklmnopqrstuvwxyz" + "0123456789";

        public string Generate() {
            var rnd = new Random();
            var sb = new StringBuilder();
            for (int i = 0; i < Length; ++i)
                sb.Append(Chars[rnd.Next(Chars.Length)]);
            return sb.ToString();
        }
    };
}
