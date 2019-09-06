using System;
using System.Runtime.InteropServices;
using System.Collections.Generic;
using System.ComponentModel;

#if UCFG_CF
	using OpenNETCF.Runtime.InteropServices;
#endif

namespace Win32 {

	public enum WaveFormats {
		Pcm = 1,
		Float = 3
	}

	[StructLayout(LayoutKind.Sequential)]
	public class WaveFormat {
		public short wFormatTag;
		public short nChannels;
		public int nSamplesPerSec;
		public int nAvgBytesPerSec;
		public short nBlockAlign;
		public short wBitsPerSample;
		public short cbSize;

		public WaveFormat(int rate, int bits, int channels) {
			wFormatTag = (short)WaveFormats.Pcm;
			nChannels = (short)channels;
			nSamplesPerSec = rate;
			wBitsPerSample = (short)bits;
			cbSize = 0;

			nBlockAlign = (short)(channels * (bits / 8));
			nAvgBytesPerSec = nSamplesPerSec * nBlockAlign;
		}
	}


	[StructLayout(LayoutKind.Sequential)]
	public struct WaveHdr {
		public IntPtr lpData; // pointer to locked data buffer
		public int dwBufferLength; // length of data buffer
		public int dwBytesRecorded; // used for input only
		public IntPtr dwUser; // for client's use
		public int dwFlags; // assorted flags (see defines)
		public int dwLoops; // loop control counter
		public IntPtr lpNext; // PWaveHdr, reserved for driver
		public int reserved; // reserved for driver
	}

	public enum WaveOutMsg {
		MM_WOM_OPEN = 0x3BB,
		MM_WOM_CLOSE = 0x3BC,
		MM_WOM_DONE = 0x3BD
	}

	public delegate void waveOutProc(IntPtr hdrvr, WaveOutMsg uMsg, IntPtr dwUser, IntPtr dwParam1, IntPtr dwParam2);

	public enum MMSysErr {
		NoError = 0
	}

	[Flags]
	public enum WaweOutFlag {
		WAVE_FORMAT_QUERY = 0x0001,
		WAVE_ALLOWSYNC = 0x0002,
		WAVE_MAPPED  = 0x0004,
		WAVE_FORMAT_DIRECT  = 0x0008,
		WAVE_FORMAT_DIRECT_QUERY  = (WAVE_FORMAT_QUERY | WAVE_FORMAT_DIRECT),

		CALLBACK_NULL = 0,
		CALLBACK_FUNCTION = 0x00030000,
		CALLBACK_WINDOW = 0x00010000,
		CALLBACK_TASK   =    0x00020000,
		CALLBACK_THREAD = CALLBACK_TASK,
		CALLBACK_EVENT = 0x00050000,
		CALLBACK_TYPEMASK = 0x00070000,

	}

	public class MMException : Win32Exception {
		public MMException(MMSysErr err)
			: base((int)err | 0x5480000)			//!!! should be FACILITY number
		{
		}
	}

	public class MM {
		public static void Check(MMSysErr err) {
			if (err == MMSysErr.NoError)
				return;
			throw new MMException(err);
		}

	}

	public class WaveHandle : SafeHandle {
		public WaveHandle()
			: base(IntPtr.Zero, true) {
		}

		protected override bool ReleaseHandle() {
			return Api.waveOutClose(handle) == MMSysErr.NoError;
		}

		public override bool IsInvalid {
			get {
				return handle == IntPtr.Zero;
			}
		}
	}

	public enum TimeFormat : uint {
		TIME_MS = 1,
		TIME_SAMPLES = 2,
		TIME_BYTES = 4,
		TIME_SMPTE = 8,
		TIME_MIDI = 0x10,
		TIME_TICKS = 0x20
	}

	public struct SMPTE {
		public byte hour, min, sec, frame, fps, dummy, pad1, pad2;
	}

	[StructLayout(LayoutKind.Sequential | LayoutKind.Explicit)]
	public struct MMTIME {
		[FieldOffset(0)]
		public TimeFormat wType;

		[FieldOffset(4)]
		public uint ms;

		[FieldOffset(4)]
		public uint sample;

		[FieldOffset(4)]
		public uint cb;

		[FieldOffset(4)]
		public uint ticks;

		[FieldOffset(4)]
		public SMPTE smpte;
	}


	public partial class Api
	{


		public const int CALLBACK_FUNCTION = 0x00030000;    // dwCallback is a FARPROC 

		public const int TIME_MS = 0x0001;  // time in milliseconds 
		public const int TIME_SAMPLES = 0x0002;  // number of wave samples 
		public const int TIME_BYTES = 0x0004;  // current byte offset 

		public const int WAVE_MAPPER = -1;
		
#if UCFG_CF
		private const string mmdll = "coredll.dll";
#else
		private const string mmdll = "winmm.dll";
#endif

		// native calls
		[DllImport(mmdll)]
		public static extern int waveOutGetNumDevs();
		[DllImport(mmdll)]
		public static extern MMSysErr waveOutPrepareHeader(WaveHandle h, ref WaveHdr lpWaveOutHdr, int uSize);
		[DllImport(mmdll)]
		public static extern MMSysErr waveOutUnprepareHeader(WaveHandle h, ref WaveHdr lpWaveOutHdr, int uSize);
		[DllImport(mmdll)]
		public static extern MMSysErr waveOutWrite(WaveHandle h, ref WaveHdr lpWaveOutHdr, int uSize);
		[DllImport(mmdll)]
		public static extern MMSysErr waveOutOpen(out WaveHandle hWaveOut, int uDeviceID, WaveFormat lpFormat,
													waveOutProc dwCallback, IntPtr dwInstance, WaweOutFlag dwFlags);
		[DllImport(mmdll)]
		public static extern MMSysErr waveOutReset(WaveHandle h);
		[DllImport(mmdll)]
		public static extern MMSysErr waveOutClose(IntPtr hWaveOut);
		[DllImport(mmdll)]
		public static extern MMSysErr waveOutPause(WaveHandle h);
		[DllImport(mmdll)]
		public static extern MMSysErr waveOutRestart(WaveHandle h);
		[DllImport(mmdll)]
		public static extern int waveOutGetPosition(WaveHandle h, out int lpInfo, int uSize);
		[DllImport(mmdll)]
		public static extern int waveOutSetVolume(WaveHandle h, int dwVolume);
		[DllImport(mmdll)]
		public static extern int waveOutGetVolume(WaveHandle h, out int dwVolume);

		[DllImport(mmdll)]
		public static extern MMSysErr waveOutGetPosition(WaveHandle h, ref MMTIME mmtime, uint cbmmt);
	}

}

