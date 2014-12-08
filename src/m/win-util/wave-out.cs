/*######     Copyright (c) 1997-2012 Ufasoft  http://ufasoft.com  mailto:support@ufasoft.com,  Sergey Pavlov  mailto:dev@ufasoft.com #####
# This program is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published #
# by the Free Software Foundation; either version 3, or (at your option) any later version. This program is distributed in the hope that #
# it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. #
# See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with this #
# program; If not, see <http://www.gnu.org/licenses/>                                                                                    #
########################################################################################################################################*/

using System;
using System.Threading;
using System.Runtime.InteropServices;
using System.ComponentModel;

using Win32;

namespace Utils.Sound {


	public class WaveDataBlock : IDisposable
	{
//        public WaveOutBuffer NextBuffer;

        public WaveHdr m_Header;
        public byte[] m_HeaderData;
        private GCHandle m_HeaderHandle;
        private GCHandle m_HeaderDataHandle;


		WaveHandle m_WaveOut;

        public WaveDataBlock(WaveHandle waveOutHandle, int size)
		{
            m_WaveOut = waveOutHandle;

            m_HeaderHandle = GCHandle.Alloc(m_Header, GCHandleType.Pinned);
            m_Header.dwUser = (IntPtr)GCHandle.Alloc(this);
            m_HeaderData = new byte[size];
            m_HeaderDataHandle = GCHandle.Alloc(m_HeaderData, GCHandleType.Pinned);
            m_Header.lpData = m_HeaderDataHandle.AddrOfPinnedObject();
            m_Header.dwBufferLength = size;
			MM.Check(Api.waveOutPrepareHeader(m_WaveOut, ref m_Header, Marshal.SizeOf(m_Header)));
		}

        ~WaveDataBlock() {
			

            Dispose();
        }

		internal bool CanDispose = true;

        public void Dispose() {
			if (CanDispose) {
				if (m_Header.lpData != IntPtr.Zero) {
					MM.Check(Api.waveOutUnprepareHeader(m_WaveOut, ref m_Header, Marshal.SizeOf(m_Header)));
					m_HeaderHandle.Free();
					m_Header.lpData = IntPtr.Zero;
				}
				if (m_HeaderDataHandle.IsAllocated)
					m_HeaderDataHandle.Free();
			}

//            GC.SuppressFinalize(this);
        }

        public int Size
        {
            get { return m_Header.dwBufferLength; }
        }

        public IntPtr Data
        {
            get { return m_Header.lpData; }
        }

    }


	public class WaveOut : IDisposable {
		WaveHandle Handle = new WaveHandle();

		public int BitsPerSample = 8;
		public int SamplesPerSec = 8192;

		public double SecondsPerBlock = 0.5;

		public int SamplesPerBlock {
			get { return (int)(SamplesPerSec * SecondsPerBlock); }
		}

		public void Dispose() {
			Handle.Dispose();
		}

		public WaveDataBlock CreateDataBlock() {
			return new WaveDataBlock(Handle, SamplesPerBlock * BitsPerSample / 8);
		}

		protected virtual void OnDone(WaveDataBlock block) {
			block.CanDispose = true;
		}

		static void WaveOutProc(IntPtr hdrvr, WaveOutMsg uMsg, IntPtr dwUser, IntPtr dwParam1, IntPtr dwParam2) {
//!!!			GCHandle gch = GCHandle.FromIntPtr(dwUser);
			GCHandle gch = (GCHandle)dwUser;
			WaveOut waveOut = ((WaveOut)gch.Target);
			switch (uMsg) {
			case WaveOutMsg.MM_WOM_CLOSE:
				gch.Free();
				break;
			case WaveOutMsg.MM_WOM_DONE:
				WaveHdr hdr = (WaveHdr)Marshal.PtrToStructure(dwParam1, typeof(WaveHdr));

//!!!R				Log.WriteLine("MM_WOM_DONE {0:X8}", hdr.lpData);

//!!!				waveOut.OnDone((WaveDataBlock)GCHandle.FromIntPtr(hdr.dwUser).Target);
				waveOut.OnDone((WaveDataBlock)((GCHandle)hdr.dwUser).Target);
				break;
			}
		}

		waveOutProc waveOutProc = WaveOutProc;		// Keep ref to callback to prevent GC

		public void Open() {
			WaveFormat fmt = new WaveFormat(SamplesPerSec, BitsPerSample, 1);
			fmt.cbSize = (short)Marshal.SizeOf(fmt);
			fmt.wFormatTag = (short)WaveFormats.Pcm;
			MM.Check(Api.waveOutOpen(out Handle, Api.WAVE_MAPPER, fmt,
								waveOutProc,
								(IntPtr)(GCHandle.Alloc(this)),
								//!!!GCHandle.ToIntPtr(GCHandle.Alloc(this)),
								WaweOutFlag.CALLBACK_FUNCTION));
		}

		public void Write(WaveDataBlock block) {
			block.CanDispose = false;

//!!!R			Log.WriteLine("waveOutWrite {0:X8}", block.m_Header.lpData);

			MM.Check(Api.waveOutWrite(Handle, ref block.m_Header, Marshal.SizeOf(block.m_Header)));
		}

		public void Pause() {
			MM.Check(Api.waveOutPause(Handle));
		}

		public void Restart() {
			MM.Check(Api.waveOutRestart(Handle));
		}

		public void Reset() {
			MM.Check(Api.waveOutReset(Handle));
		}

		public int SamplePos {
			get {
				var mmtime = new MMTIME();
				mmtime.wType = TimeFormat.TIME_SAMPLES;
				MM.Check(Api.waveOutGetPosition(Handle, ref mmtime, (uint)Marshal.SizeOf(typeof(MMTIME))));
				return (int)mmtime.sample;
			}
		}
	}
}
