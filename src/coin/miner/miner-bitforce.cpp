/*######     Copyright (c) 1997-2013 Ufasoft  http://ufasoft.com  mailto:support@ufasoft.com,  Sergey Pavlov  mailto:dev@ufasoft.com #######################################
#                                                                                                                                                                          #
# This program is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation;  #
# either version 3, or (at your option) any later version. This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the      #
# implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details. You should have received a copy of the GNU #
# General Public License along with this program; If not, see <http://www.gnu.org/licenses/>                                                                               #
##########################################################################################################################################################################*/

// Support of device "BitFORCE SHA256 Single"  http://butterflylabs.com/products/

#include <el/ext.h>

#include "serial-device.h"




#include "miner.h"

namespace Coin {

static const int DEVICE_TIMEOUT = 25; // 25.5 s is max timeout

static regex s_reTemparature("TEMP:(.+)");

class BitforceThread : public SerialDeviceThread {
	typedef SerialDeviceThread base;
public:
	BitforceThread(BitcoinMiner& miner, thread_group *tr)
		:	base(miner, tr)
	{
		MsTimeout = DEVICE_TIMEOUT*1000;
	}

	void CreateDeviceObject();

protected:
	String GetReply() {
		char buf[100];
#if UCFG_USE_POSIX
		if (!fgets(buf, sizeof buf, m_file)) {
			return nullptr;
/*!!!R			if (m_bStop)
				throw StopException();
			CCheck(-1); */
		}
		fseek(m_file, 0, SEEK_CUR);					// dont check retval, switch after reading
#else
		int fd = fileno(m_file);
		for (int i=0; i<sizeof buf - 1; ++i) {
			if (_read(fd, buf+i, 1) < 1)
				return nullptr;
			if (buf[i] == '\n') {
				buf[i+1] = 0;
				break;
			}
		}		
#endif
		String r = String(buf).Trim();
		TRC(3, "Reply: " << r);
		return r;
	}

	String Command(const char *cmd, size_t size = 3) {
		if (size == 3) {
			TRC(3, cmd);
		}
		Write(ConstBuf(cmd, size));
		return GetReply();
	}

	Ext::Temperature GetTemperature() {
		String s = Command("ZLX");
		cmatch m;
		Temperature r;
		if (regex_match(s.c_str(), m, s_reTemparature))
			r = Temperature::FromCelsius((float)strtod(String(m[1]), 0));
		return r;
	}

	void CommunicateDevice() override {
		UInt32 portion = FULL_TASK;

		String id = Command("ZGX");
		if (id == ">>>ID: BitFORCE SHA256 Version 1.0>>>") {
			Detected = true;
			CreateDeviceObject();
			m_evDetected.Set();
			m_evStartMining.Lock();
			char bufData[8+32+12+8];
			memset(bufData, '>', sizeof bufData);
			for (ptr<BitcoinWorkData> wd; !m_bStop && (wd = Miner.GetWorkForThread(_self, portion, false));) {
				StartRound();
				if (Command("ZDX") != "OK") {
					*Miner.m_pTraceStream << Name << " signaled Error" << endl;
					break;
				}
				memcpy(bufData+8, wd->Midstate.constData(), 32);
				memcpy(bufData+8+32, wd->Data.constData()+64, 12);
				TRC(3, "Data[64..75]: " << ConstBuf(wd->Data.constData()+64, 12) << ", Midstate: " << wd->Midstate);
				if (Command(bufData, sizeof bufData) != "OK") {
					*Miner.m_pTraceStream << Name << " signaled Error" << endl;
					break;						
				}
				if (Device)
					Device->Temperature = GetTemperature();
				String reply;
				while (!m_bStop && (reply = Command("ZFX")) == "BUSY")
					Thread::Sleep(10);
				if (m_bStop || reply == nullptr)
					break;
				TRC(3, "Reply: " << reply);
				if (reply.Left(12) == "NONCE-FOUND:") {
					vector<String> nn = reply.Substring(12).Split(",");
					for (int i=0; i<nn.size(); ++i)
						Miner.TestAndSubmit(wd, Convert::ToUInt32(nn[i], 16));
				} else if (reply != "NO-NONCE") {
					*Miner.m_pTraceStream << Name << " Unexpected reply: " << reply << endl;
					break;
				}
				Interlocked::Add(Miner.HashCount, portion);
				CompleteRound(0x100000000ULL);
			}
		}
	}
};

class BitforceDevice : public ComputationDevice {
public:
	CPointer<BitforceThread> m_thread;

	void Start(BitcoinMiner& miner, thread_group *tr) override {
		m_thread->m_owner = tr;
		miner.Threads.push_back(m_thread.get());
		m_thread->m_evStartMining.Set();
	}
};

void BitforceThread::CreateDeviceObject() {
	ptr<BitforceDevice> dev = new BitforceDevice;
	dev->Name = "BitForce Unit";
	dev->Description = m_devpath + " ["+dev->Name+"]";
	dev->m_thread = this;
	Device = dev.get();
	EXT_LOCK (Miner.MtxDevices) {
		Miner.Devices.push_back(dev.get());
		Miner.WorkingDevices.insert(m_devpath);
		Miner.TestedDevices.erase(m_devpath);
		if (AutoStart) {
			*Miner.m_pTraceStream << "Attached device: " << dev->Description << endl;
			dev->Start(Miner, Miner.m_tr);
		}
	}
}

class BitforceArchitecture : public FpgaArchitecture {
protected:
	void DetectNewDevices(BitcoinMiner& miner) override {
		if (!TimeToDetect())
			return;
		vector<String> devpaths = GetPossibleDevicePaths(miner);
		for (int i=0; i<devpaths.size(); ++i) {
			ptr<BitforceThread> t = new BitforceThread(miner, miner.m_tr);
			t->m_devpath = devpaths[i];
			t->AutoStart = AutoStart;
			t->Start();
			t->m_evDetected.Lock(1000);
			if (!t->Detected)
				t->Stop();
		}
	}

	void FillDevices(BitcoinMiner& miner) override {
		DetectNewDevices(miner);
		AutoStart = true;
	}
private:
	DateTime m_dtNextDetect;
	CBool AutoStart;

	vector<String> GetPossibleDevicePaths(BitcoinMiner& miner) {
		vector<String> r;
#if UCFG_USE_POSIX		
		r = Directory::GetFiles("/dev", "ttyUSB*");
#else
		r = SerialPort::GetPortNames();
		for (int i=0; i<r.size(); ++i)
			r[i] = "\\\\.\\"+r[i];
#endif
		TRC(2, r);
		EXT_LOCK (miner.MtxDevices) {
			for (int i=r.size(); i--;) {
				String devname = r[i];
				if (miner.WorkingDevices.count(devname) || miner.TestedDevices.count(devname))
					r.erase(r.begin()+i);
			}
			for (int i=0; i<r.size(); ++i)
				miner.TestedDevices.insert(r[i]);
		}
		return r;
	}

} g_bitforceArchitecture;

} // Coin::

