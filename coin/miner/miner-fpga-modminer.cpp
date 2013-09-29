/*######     Copyright (c) 1997-2013 Ufasoft  http://ufasoft.com  mailto:support@ufasoft.com,  Sergey Pavlov  mailto:dev@ufasoft.com #######################################
#                                                                                                                                                                          #
# This program is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation;  #
# either version 3, or (at your option) any later version. This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the      #
# implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details. You should have received a copy of the GNU #
# General Public License along with this program; If not, see <http://www.gnu.org/licenses/>                                                                               #
##########################################################################################################################################################################*/

// Support of device "BPCFPGA ModMiner QUAD

#include <el/ext.h>

#ifdef HAVE_LIBUDEV
#	include <el/comp/device-manager.h>
#endif

#if UCFG_WIN32
#	include <devguid.h>

#	include <el/win/setupdi.h>
#endif

#include "serial-device.h"
#include "miner.h"
#include "dynclock.h"

namespace Coin {

const int MAX_FPGA_FREQUENCY = 250; // MHz
const int DEFAULT_FPGA_FREQUENCY = 220; // MHz

static const String BITSTREAM_FILENAME = "fpgaminer_x6500-overclocker-0402.bit";
static const String BITSTREAM_URL = String(MANUFACTURER_HTTP_URL) + "/files/bitstreams/" + BITSTREAM_FILENAME;
static const UInt32 BISTREAM_USER_ID = 0x42240402;
static const Blob BITSTREAM_SHA256SUM = Blob::FromHexString("2f90ac200f33d89ac2d00f56123269a6f8265b86873e7c125380eee9cd0274a6");

class MmqDevice;
class MmqThread;

class MmqChip : public Object, public DynClockDevice {
public:
	String Name;
	Ext::Temperature Temperature;
	ptr<BitcoinWorkData> WorkData, PrevWorkData;
	DateTime DtStartRound, DtNextWorkData;
	CPointer<Coin::MmqThread> MmqThread;
	byte Id;

	MmqChip() {
		FreqDatas.resize(MAX_FPGA_FREQUENCY/2+1);
		m_defaultClockUnits = m_clockUnits = DEFAULT_FPGA_FREQUENCY/2;
		m_maxClockUnits = MAX_FPGA_FREQUENCY/2;
	}

	int GetClock() override { return m_clockUnits * 2; }
	void SetClock(int mhz) override;

	void ChangeFrequency(int units) override;
};

class MmqDevice : public ComputationDevice {
public:
	ptr<MmqChip> Chips[4];

	ptr<SerialDeviceThread> m_thread;
	String PortName;
	Ext::Temperature TemperatureLimit;

	void Start(BitcoinMiner& miner, CThreadRef *tr) override;
};

class MmqThread : public SerialDeviceThread {
	typedef SerialDeviceThread base;
public:
	MmqDevice& Dev;

	MmqThread(MmqDevice& dev, BitcoinMiner& miner, CThreadRef *tr);

	void CommunicateDevice() override;

	void StatusRead() {
		byte buf;
#if UCFG_USE_POSIX
		int rc = fread(&buf, 1, 1, m_file);
		CCheck(rc);
		if (rc != 1)
			Throw(E_FAIL);
#else
		int fd = fileno(m_file);
		int rc = _read(fd, &buf, 1);
		if (rc < 1)
			Throw(E_FAIL);
#endif
		if (buf != 1)
			Throw(E_FAIL);
	}

	using base::Command;

	void Command(const ConstBuf cmd) {
		Write(cmd);
		StatusRead();
	}

	void Sync() {
		byte cmdNoop[46] = { 0 };
		memset(cmdNoop+1, 0xFF, (sizeof cmdNoop)-1);
		Command(ConstBuf(cmdNoop, sizeof cmdNoop), 0);
	}

	String GetDevDescription() {
		return (const char*)Command(ConstBuf("\x01", 1), size_t(-1)).constData();
	}

	int GetFpgaCount() {
		return *Command(ConstBuf("\x02", 1), 1).constData();
	}

	Temperature GetTemperature(byte devid) {
		byte cmd[2] = { 10, devid };
		return Temperature::FromCelsius(*Command(ConstBuf(cmd, sizeof cmd), 1).constData());
	}

	bool SetClock(byte devid, int mhz) {
		byte cmd[6] = { 6, devid, (byte)min(250, max(1, mhz & ~1)), 0, 0, 0 };
		bool r = *Command(ConstBuf(cmd, sizeof cmd), 1).constData();
		*Miner.m_pTraceStream << "Setting " << Dev.Chips[devid]->Name << " CLK = " << mhz << " MHz " << endl;
		return r;	
	}

	UInt32 GetUserId(byte devid) {
		byte cmd[2] = { 4, devid};
		return letoh(*(UInt32*)Command(ConstBuf(cmd, sizeof cmd), 4).constData());
	}

	static const byte DEVID_ALL = 4;

	void UploadBitstream(byte devid, const ConstBuf& bitstream) {
		byte cmd[6] = { 5, devid, byte(bitstream.Size), byte(bitstream.Size >> 8), byte(bitstream.Size >> 16), byte(bitstream.Size >> 24)};
		byte rc = *Command(ConstBuf(cmd, sizeof cmd), 1).constData();
		if (rc != 1) {
			Throw(E_FAIL);
		}
		for (int i=0; i<bitstream.Size && !m_bStop; i+=32) {
			int len = min(32, int(bitstream.Size-i));
			Write(ConstBuf(bitstream.P+i, len));
			StatusRead();
			if (!(i & 0xFFF))
				*Miner.m_pTraceStream << "\rProgramming FPGA" << (devid==DEVID_ALL ? String("") : " "+Dev.Chips[devid]->Name) << ", please DONT'T EXIT until complete: " << i*100/bitstream.Size << "%                      " << flush;
		}
		if (m_bStop)
			Throw(E_EXT_ThreadStopped);
			
		StatusRead();

		*Miner.m_pTraceStream << "\rProgramming FPGA complete                                                     " << endl;
	}

	void SendWorkData(byte devid, const BitcoinWorkData& wd) {
		byte cmd[46] = { 8, devid };
		memcpy(cmd+2, wd.Midstate.constData(), 32);
		memcpy(cmd+2+32, wd.Data.constData()+64, 12);
		Command(ConstBuf(cmd, sizeof cmd));
	}

	UInt32 GetNonce(byte devid) {
		byte cmd[2] = { 9, devid};
		return letoh(*(UInt32*)Command(ConstBuf(cmd, sizeof cmd), 4).constData());
	}
};


class MmqArchitecture : public FpgaArchitecture {
	typedef FpgaArchitecture base;
public:
	MmqArchitecture() {
		SysName = "ModMiner";
	}

	void CheckBitstream(BitcoinMiner& miner, const ConstBuf& cbuf) {
		if (SHA256().ComputeHash(cbuf) != BITSTREAM_SHA256SUM) {
			*miner.m_pTraceStream << "Invalid Bitstream Checksum" << endl;
			Throw(E_FAIL);
		}			
	}

	Blob LoadBitstream(BitcoinMiner& miner) {
		Blob blob;
		try {
#if UCFG_BITCOIN_USE_RESOURCE
			CResID resId(BITSTREAM_FILENAME.c_str());
			Resource res(resId, RT_RCDATA);
			blob = res;
#else
			blob = File::ReadAllBytes(Path::Combine(Path::GetDirectoryName(System.ExeFilePath), BITSTREAM_FILENAME));
#endif
		} catch (RCExc) {
			*miner.m_pTraceStream << "Bitstream file " << BITSTREAM_FILENAME << " not found" << endl;
			String filename = Path::Combine(AfxGetCApp()->AppDataDir, BITSTREAM_FILENAME);
			
			if (File::Exists(filename))
				blob = File::ReadAllBytes(filename);
			else {
				*miner.m_pTraceStream << "Downloading Bitstream: " << BITSTREAM_URL << endl;
				blob = WebClient().DownloadData(BITSTREAM_URL);
				CheckBitstream(miner, blob);
				File::WriteAllBytes(filename, blob);
			}
		}
		CheckBitstream(miner, blob);
		return blob;		
	}
protected:
	void DetectNewDevices(BitcoinMiner& miner) override {
		if (!TimeToDetect())
			return;
		vector<String> devpaths = GetPossibleDevicePaths(miner);
		for (int i=0; i<devpaths.size(); ++i) {
			ptr<MmqDevice> dev = new MmqDevice;
			dev->TemperatureLimit = miner.MaxFpgaTemperature;
#ifdef X_DEBUG//!!!D
			dev->TemperatureLimit = Temperature::FromCelsius(44);
#endif
			dev->PortName = devpaths[i];
			static regex rePortNum(".+(\\d+)");
			cmatch m;
			dev->Name = "MMQ";
			if (regex_match(dev->PortName.c_str(), m, rePortNum))
				dev->Name += m[1];
			miner.Devices.push_back(dev.get());
			ptr<MmqThread> t = new MmqThread(*dev, miner, miner.m_tr);
			dev->m_thread = t.get();
			t->Start();
			t->m_evDetected.Lock(100000);
			if (!t->Detected)
				t->Stop();
			else {
				int n = 0;
				for (int i=0; i<4; ++i)
					n += bool(dev->Chips[i]);
				dev->Description += EXT_STR(", " << n << " FPGA" << (n>1 ? "s" : ""));
			}

//			if (AutoStart)
//				dev->Start(miner, miner.m_tr);
		}
	}

	void FillDevices(BitcoinMiner& miner) override {
		DetectNewDevices(miner);
		AutoStart = true;
	}
private:
	CBool AutoStart;

	vector<String> GetPossibleDevicePaths(BitcoinMiner& miner) {
		vector<String> r;
#if UCFG_WIN32
		DiClassDevs devs(DIGCF_PRESENT, GUID_DEVCLASS_PORTS);
		EXT_FOR (DiDeviceInfo di, devs) {
			if (di.DeviceDesc == "ModMiner QUAD")
				r.push_back("\\\\.\\"+String(RegistryKey(di.OpenRegKey()).QueryValue("PortName")));
		}
#elif defined(HAVE_LIBUDEV)
		DeviceManager dm;
		SystemDevices devs(dm);
		devs.AddMatchSubSystem("tty");
		devs.AddMatchProperty("ID_VENDOR", "BTCFPGA");
		devs.ScanDevices();
		for (SystemDevices::iterator it=devs.begin(), e=devs.end(); it!=e; ++it) {
			SystemDevice dev = *it;
			r.push_back(dev.DevNode);
		}
#else
		r = Directory::GetFiles("/dev", "ttyACM*");
#endif

//		TRC(2, r);
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

} g_mmqArchitecture;

MmqThread::MmqThread(MmqDevice& dev, BitcoinMiner& miner, CThreadRef *tr)
	:	base(miner, tr)
	,	Dev(dev)
{
	MsTimeout = 500;
	m_devpath = Dev.PortName;
}

void MmqThread::CommunicateDevice() {
	Sync();
	try {
		Dev.Description = GetDevDescription();
	} catch (RCExc) {
		Throw(E_COIN_MINER_FPGA_PROTOCOL);
	}
	int n = GetFpgaCount();
	if (!n)
		return;
	const char * const suffixes[4] = { "a", "b", "c", "d" };
	for (byte devid=0; devid<n; ++devid) {
		MmqChip *chip = new MmqChip;
		(Dev.Chips[chip->Id = devid] = chip)->Name = Dev.Name + suffixes[devid];
		chip->MmqThread = this;
	}
#ifdef X_DEBUG//!!!D
	Dev.Chips[0] = nullptr;
	Dev.Chips[1] = nullptr;
	Dev.Chips[2] = nullptr;
#endif
	Detected = true;
	m_evDetected.Set();
	m_evStartMining.Lock();
	if (m_bStop)
		return;
	for (byte devid=0; devid < n; ++devid) {
		UInt32 userId = GetUserId(devid);
		if (userId != BISTREAM_USER_ID) {
			*Miner.m_pTraceStream << Dev.Chips[devid]->Name << " has not BITSTREAM uploaded" << endl;
			UploadBitstream(DEVID_ALL, g_mmqArchitecture.LoadBitstream(Miner));
			break;
		}
	}
	for (byte devid=0; devid<n; ++devid) {
		if (Dev.Chips[devid]) {
			Dev.Chips[devid]->SetClock(DEFAULT_FPGA_FREQUENCY);
			GetNonce(devid);	// read prev  results
		}
	}

	DateTime dtPrev;
	while (!m_bStop) {
		Temperature tempMax = Temperature::FromCelsius(0);
		for (byte devid=0; devid<4; ++devid) {
			if (Dev.Chips[devid]) {
				MmqChip& chip = *Dev.Chips[devid];
				chip.Temperature = GetTemperature(devid);
				tempMax = max(tempMax, chip.Temperature);
			}
		}
		Dev.Temperature = tempMax;

		int hashrate = 0;
		for (byte devid=0; devid<4; ++devid) {
			if (Dev.Chips[devid]) {
				MmqChip& chip = *Dev.Chips[devid];
				hashrate += chip.GetClock();
				int nTotal = 0, nErr = 0;
				for (UInt32 nonce; (nonce=GetNonce(devid))!=0xFFFFFFFF;) {
#ifdef X_DEBUG//!!!D
						cerr <<  "\nNONCE: " << hex << nonce << "\n" << endl;
#endif

					++nTotal;
					if (chip.WorkData && chip.WorkData->TestNonceGivesZeroH(nonce))
						Miner.TestAndSubmit(_self, chip.WorkData, nonce);
					else if (chip.PrevWorkData && chip.PrevWorkData->TestNonceGivesZeroH(nonce))
						Miner.TestAndSubmit(_self, chip.PrevWorkData, nonce);
					else {
						++nErr;
						Interlocked::Increment(Dev.HwErrors);
						*Miner.m_pTraceStream << chip.Name << " Hardware Error (" << Dev.HwErrors << ")" << endl;
					}
				}
				if (nTotal)
					chip.Update(double(nErr) / nTotal);
			}				
		}

		DateTime now = DateTime::UtcNow();
		for (byte devid=0; devid<4; ++devid) {
			if (Dev.Chips[devid]) {
				MmqChip& chip = *Dev.Chips[devid];
				if (now > chip.DtNextWorkData) {
					if (chip.Temperature > Dev.TemperatureLimit) {
						*Miner.m_pTraceStream << "Chip is very Hot  ";
						chip.SetClock(max(2, chip.GetClock() - 2));
						chip.m_maxClockUnits = chip.m_clockUnits;
					}

					chip.DtNextWorkData = (chip.DtStartRound = now) + TimeSpan::FromSeconds(double(0x100000000 / (chip.GetClock() * 1000000)));
					if (chip.WorkData) {
						m_span += now - chip.DtStartRound;
					}

					if (ptr<BitcoinWorkData> wd = Miner.GetWorkForThread(_self, FULL_TASK, false)) {
						chip.PrevWorkData = exchange(chip.WorkData, wd);
						SendWorkData(devid, *wd);						
						m_nHashes += 0x100000000;
					}
				}
			}
		}

		if (dtPrev != DateTime())
			Interlocked::Add(Miner.HashCount, Int32(Int64((now - dtPrev).TotalMilliseconds)  * hashrate * 1000 / UCFG_BITCOIN_NPAR));
		dtPrev = now;

		Thread::Sleep(100);
	}
}

void MmqDevice::Start(BitcoinMiner& miner, CThreadRef *tr) {
	m_thread->m_evStartMining.Set();
}

void MmqChip::SetClock(int mhz) {
	m_clockUnits = mhz/2;
	MmqThread->SetClock(Id, mhz);
}

void MmqChip::ChangeFrequency(int units) {
	SetClock(units*2);
}

} // Coin::

