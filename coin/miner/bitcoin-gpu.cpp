/*######     Copyright (c) 1997-2013 Ufasoft  http://ufasoft.com  mailto:support@ufasoft.com,  Sergey Pavlov  mailto:dev@ufasoft.com #######################################
#                                                                                                                                                                          #
# This program is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation;  #
# either version 3, or (at your option) any later version. This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the      #
# implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details. You should have received a copy of the GNU #
# General Public License along with this program; If not, see <http://www.gnu.org/licenses/>                                                                               #
##########################################################################################################################################################################*/

#include <el/ext.h>

#include "bitcoin-common.h"

#if UCFG_BITCOIN_USE_CAL
#	include <el/comp/gpu-cal.h>
	using namespace Ext::Gpu;

#	include "miner-cal.cpp"
#endif

#if UCFG_BITCOIN_USE_CUDA
#	include <el/comp/gpu-cuda.cpp>
#	include "miner-cuda.cpp"
#endif

#if UCFG_BITCOIN_THERMAL_CONTROL
#	include <el/comp/gpu-adl.h>
using namespace Ext::Gpu;
#endif


#include <el/comp/ext-opencl.h>

#include "elf.h"


#include "resource.h"
#include "miner.h"
#include "bitcoin-sha256.h"

namespace Ext {

template <typename T, typename U>
struct PersistentTraits<pair<T, U>> {

	static void Read(const BinaryReader& rd, pair<T, U>& v) {
		rd >> v.first >> v.second;
	}

	static void Write(BinaryWriter& wr, const pair<T, U>& v) {
		wr << v.first << v.second;
	}
};


} // Ext::


namespace Coin {


#if UCFG_BITCOIN_THERMAL_CONTROL

static AdlEngine *Adl;
static vector<AdlDevice> s_activeDevices;

AdlEngine *GetAdl() {
	if (!Adl) {
		Adl = new AdlEngine;
		if (*Adl) {
			for (int count=Adl->DeviceCount, i=0; i<count; ++i) {
				AdlDevice dev = Adl->GetDevice(i);
				if (dev.Active)
					s_activeDevices.push_back(dev);
			}
			TRC(3, s_activeDevices.size() << " active devices");
		}
	}
	return Adl;
}

static void __cdecl DeleteAdl() {
	delete Adl;
	Adl = 0;
}
static AtExitRegistration s_regAdl(&DeleteAdl);

float GetMaxTemperature() {
	GetAdl();
	if (!*Adl || s_activeDevices.empty())
		return Temperature::FromCelsius(30);									// better than to FAIL
	float r = s_activeDevices[0].Temperature.ToCelsius();
	for (int i=1; i<s_activeDevices.size(); ++i)
		r = std::max(r, s_activeDevices[i].Temperature.ToCelsius());
	return r;
}

#endif // UCFG_BITCOIN_THERMAL_CONTROL


/*!!!R
static void __cdecl UnloadAtiCal() {
	if (s_hAtiCalRtDll)
		::FreeLibrary(s_hAtiCalRtDll);
	if (s_hAtiCalClDll)
		::FreeLibrary(s_hAtiCalClDll);
}
static AtExitRegistration s_regAtiCal(&UnloadAtiCal);
*/

#if UCFG_WIN32
	static HMODULE s_hOpenClDll;
#else
	static bool s_hOpenClDll = true;
#endif


class GpuThread : public GpuThreadBase {
	typedef GpuThreadBase base;
public:
	vector<ptr<GpuDevice>> Devices;

	GpuThread(BitcoinMiner& miner, thread_group *tr)
		:	base(miner, tr)
	{
		for (int i=0; i<miner.Devices.size(); ++i) {
			if (miner.Devices[i]->IsAmdGpu)
				Devices.push_back(static_cast<GpuDevice*>(miner.Devices[i].get()));
		}

//!!!		m_gpuMiner = StaticCast<AmdGpuMiner>(gpuDevice.Miner);
//!!!		DevicePortion = m_gpuMiner->DevicePortion;


#ifdef X_DEBUG //!!!D
		//FileStream fs("S:\\SRC\\grid\\cal-client\\pixel-shader.il", FileMode::Open, FileAccess::Read);
		FileStream fs("C:\\OUT\\Debug\\btc.il", FileMode::Open, FileAccess::Read);
		Blob blob(0, (size_t)fs.Length);
		fs.ReadBuffer(blob.data(), blob.Size);
		String il = Encoding::UTF8.GetChars(blob);
		CalObject obj;
//!!!		obj.Compile(il, (CALtarget)8); 
		obj.Compile(il, m_gpuMiner->DeviceInfo.target); 
		m_gpuMiner->Image.Add(obj);
		m_gpuMiner->Image.Link();
//		int size = Image.get_Size();
//		Blob bl(0, size);
//		Image.Write(CMemBlock(bl.data(), size));
//		ofstream("C:\\OUT\\Debug\\test.image", ios::binary).write(bl.constData(), size);

/*		
		FileStream fs("C:\\OUT\\Debug\\btc-rv730.image", FileMode::Open, FileAccess::Read);
		Blob blob(0, fs.Length);
		fs.ReadBuffer(blob.data(), fs.Length);
		Image.Read(blob); */
#else
//		Image.Read(Miner.GpuDevices[idx].Image);
#endif
	}

	String GetDeviceName() {
		String r = "GPU/"+Device->Name;
#if UCFG_BITCOIN_THERMAL_CONTROL && UCFG_BITCOIN_USERAGENT_INFO
		if (*GetAdl())
			r +=  " T="+Convert::ToString((int)GetMaxTemperature()) + " M="+Convert::ToString(Miner.GpuIdleMilliseconds);
#endif
		return r;
	}

	void Execute() override {
		Name = "GPU Thread";

//!!!R		m_gpuMiner->InitBeforeRun();

		vector<ptr<GpuTask>> tasks;

		StartRound();
		for (int i=0; i<Devices.size(); ++i) {
			GpuDevice& gd = *Devices[i];
			GpuMiner& gm = *static_cast<GpuMiner*>(gd.Miner.get());

			int n = Miner.HashAlgo==HashAlgo::SCrypt ? thread::hardware_concurrency() : 1;
			for (int i=0; i<n; ++i) {
				if (ptr<BitcoinWorkData> wd = Miner.GetWorkForThread(_self, gm.DevicePortion, true)) {
					ptr<GpuTask> task = gm.CreateTask();
					task->Prepare(*wd, this);
					task->Run();
					tasks.push_back(task);
				} else {
					m_bStop = true;
					goto LAB_RET;
				}
			}
		}

		while (!m_bStop) {
			try {

				bool bSomeComplete = false;
				for (int i=0; i<tasks.size(); ++i) {
					ptr<GpuTask>& task = tasks[i];
					if (task->IsComplete()) {
						bSomeComplete = true;
						int res = task->ParseResults();
						CompleteRound(task->WorkData->LastNonce - task->WorkData->FirstNonce+1);
						if (ptr<BitcoinWorkData> wd = Miner.GetWorkForThread(_self, task->m_gpuMiner.DevicePortion, true)) {
							task->Prepare(*wd, this);
							task->Run();
							StartRound();
						} else
							return;
					}
				}
				if (!bSomeComplete) {
					Sleep(Miner.GpuIdleMilliseconds);
				}

				//m_gpuMiner->Run(wd, this);				
			} catch (thread_interrupted&) {
				break;
			} catch (RCExc ex) {
				*Miner.m_pTraceStream << ex.what() << endl;
				break;
			}

#if UCFG_BITCOIN_THERMAL_CONTROL
			if (*GetAdl()) {
				static int s_temp_try;
				if (!(++s_temp_try & 3) ) {				// 3 is param
					float t = GetMaxTemperature();
					static DateTime s_dtNext;
					if (t > Miner.MaxGpuTemperature) {
						int ms = std::min(Miner.GpuIdleMilliseconds + int(t - Miner.MaxGpuTemperature), 2000);
						if (ms != Miner.GpuIdleMilliseconds && DateTime::UtcNow() > s_dtNext) {
							*Miner.m_pTraceStream << "Thermal control decreases speed, T=" << (int)t << " C, delay=" << ms << endl;
							s_dtNext = DateTime::UtcNow()+TimeSpan::FromSeconds(10);
						}
						Miner.GpuIdleMilliseconds = ms;
					} else if (t < Miner.MaxGpuTemperature*2/3) {
						int ms = std::max(Miner.GpuIdleMilliseconds-1, 1);
						if (ms != Miner.GpuIdleMilliseconds && DateTime::UtcNow() > s_dtNext) {
							*Miner.m_pTraceStream << "Thermal control increases speed, T=" << (int)t << " C" << endl;
							s_dtNext = DateTime::UtcNow()+TimeSpan::FromSeconds(10);
						}
						Miner.GpuIdleMilliseconds = ms;
					}
				}
			}
#endif


#ifdef X_DEBUG
			}
#endif
		}
LAB_RET:
		mutex mtx;
		unique_lock<mutex> lk(mtx);										
		while (!tasks.empty()) {
			if (tasks.back()->IsComplete()) {
				tasks.pop_back();
				continue;
			}
			CvComplete.wait(lk);
		}
	}
};

/*!!!R
static bool TryCompileCal(const CResID& resId, CALtarget target, CalObject& obj) {
#ifdef X_DEBUG//!!!D
	FileStream fs("S:\\SRC\\grid\\cal-client\\btc.il ", FileMode::Open, FileAccess::Read);
	Blob blob(0, (size_t)fs.Length);
	fs.ReadBuffer(blob.data(), blob.Size);
	String il = Encoding::UTF8.GetChars(blob);
	obj.Compile(il, target);
	return true;
#endif

	DBG_LOCAL_IGNORE_WIN32(ERROR_RESOURCE_NAME_NOT_FOUND));
	try {
		Resource res(resId, RT_RCDATA);
		String il = Encoding::UTF8.GetChars(res);
		obj.Compile(il, target);
		return true;
	} catch (RCExc) {
		return false;
	}
}
*/

struct Section {
	String Name;
	Buf Mb;
};

Buf FindTextSectionFromElf(const Buf& mb) {
	if (0 == mb.Size)
		Throw(E_FAIL);
	if (mb.Size < 1024)
		Throw(E_FAIL);
	Elf32_Ehdr& hdr = *(Elf32_Ehdr*)mb.P;
	if (memcmp(hdr.e_ident, "\x7F\x45\x4C\x46\x01\x01\x01", 7))
		Throw(E_FAIL);
	if (0 == hdr.e_shoff)
		Throw(E_FAIL);
	Elf32_Shdr& sech = *(Elf32_Shdr*)(mb.P+hdr.e_shoff+hdr.e_shstrndx*hdr.e_shentsize);
	const byte *sh = mb.P + hdr.e_shoff;

	vector<Section> vSec;
	for (int i=0; i<hdr.e_shnum; ++i) {
		Elf32_Shdr& entry = *(Elf32_Shdr*)(sh+i*hdr.e_shentsize);
		Section sec;
		sec.Name = (const char*)mb.P+sech.sh_offset+entry.sh_name;
		sec.Mb = Buf(mb.P+entry.sh_offset, entry.sh_size);
		vSec.push_back(sec);
	}
	Section *sec = 0;
	for (int i=0; i<vSec.size(); ++i) {
		if (vSec[i].Name == ".text") {
			sec = &vSec[i];
			return sec->Mb;
		}
	}
	Throw(E_FAIL);
}

Blob ExtractImageFromCl(cl::Device& clDevice, RCString source, bool bEvergreen) {
	cl::CContextProps props;
	props[CL_CONTEXT_PLATFORM] = (cl_context_properties)clDevice.get_Platform().Id;
//	props[CL_CONTEXT_OFFLINE_DEVICES_AMD] = 1;
	cl::Context ctx(clDevice, &props);
	cl::Program prog(ctx, source);
	if (bEvergreen) {
		prog.Options["VECTORS"] = nullptr;
	}
	prog.build();
	vector<cl::ProgramBinary> bb = prog.Binaries;
	for (int i=0; i<bb.size(); ++i)
		if (bb[i].Device() == clDevice())
			return bb[i].Binary;
	return Blob();
}

class GDevice {
public:
	cl::Device m_clDevice;
	int m_calDevice;

	GDevice(const cl::Device& clDevice = cl::Device(), int calDevice = -1)
		:	m_clDevice(clDevice)
		,	m_calDevice(calDevice)
	{}

	bool IsEvergreen() {
		String n = m_clDevice.Name;
		return n == "RV770" ||     //!!!?
			n == "Juniper" || n == "Redwood" || n == "Cedar" || n == "Wrestler" || n == "Cayman" || n == "Barts";
	}

	static vector<GDevice> GetAll();
};

vector<GDevice> GDevice::GetAll() {
	vector<GDevice> r;

#if UCFG_BITCOIN_USE_CAL
	typedef map<int, pair<CALtarget, String>> CCal2Target;
	CCal2Target cal2target;

	RefPtr<CalEngineWrap, CalEngineWrap> engineWrap = CalEngineWrap::Get();
	int count = engineWrap->Engine.DeviceCount();
	for (int i=0; i<count; ++i) {
		CALdeviceinfoRec info = engineWrap->Engine.GetDeviceInfo(i);
		CalDevice dev = engineWrap->Engine.GetDevice(i);
		cal2target[i] = make_pair(info.target, TargetToString(info.target));
	}
#endif

	if (s_hOpenClDll) {
		vector<cl::Platform> pp = cl::Platform::GetAll();
		for (int i=0; i<pp.size(); ++i) {
			cl::Platform& pl = pp[i];
			String sver = pl.Version;
			if (Version(sver.Split().at(1)) < Version(1, 1)) {
				TRC(1, sver << " detected, but 1.1 required");
				continue;
			}

			String pname = pl.Vendor;
#ifdef X_DEBUG//!!!D
			if (pl.Vendor != "NVIDIA Corporation")
				continue;
#endif
			vector<cl::Device> dd;
			try {
				DBG_LOCAL_IGNORE_NAME(MAKE_HRESULT(SEVERITY_ERROR, FACILITY_OPENCL, -(CL_DEVICE_NOT_FOUND)), ignCL_DEVICE_NOT_FOUND);					

				dd = pl.GetDevices(CL_DEVICE_TYPE_GPU);
			} catch (RCExc) {
				continue;
			}

			for (int j=0; j<dd.size(); ++j) {
				cl::Device dev = dd[j];
				if (dev.Available && dev.EndianLittle) {
					String name = dev.Name;
						
					int calDevice = 1;
					if (pl.Vendor == "Advanced Micro Devices, Inc.") {
#if UCFG_BITCOIN_USE_CAL
						for (CCal2Target::iterator it=cal2target.begin(); it!=cal2target.end(); ++it) {
							if (!it->second.second.CompareNoCase(name)) {
								calDevice = it->first;
								cal2target.erase(it);
								break;
							}
						}
#endif
					}
					r.push_back(GDevice(dev, calDevice));
				}
			}
		}
	}

#if UCFG_BITCOIN_USE_CAL
	for (CCal2Target::iterator it=cal2target.begin(); it!=cal2target.end(); ++it)
		r.push_back(GDevice(Cl::Device(), it->first));
#endif
	return r;
}


void AmdGpuDevice::Start(BitcoinMiner& miner, thread_group *tr) {
	if (!miner.GpuThread) {
		ptr<GpuThreadBase> t = new GpuThread(miner, tr);
		t->Device = this;				//!!! right thing only when 1 device
		miner.Threads.push_back(t.get());
		miner.GpuThread = t.get();
		t->Start();
	}
}


class OpenClArchitecture : public GpuArchitecture {

	void FillDevices(BitcoinMiner& miner) override {
#if UCFG_BITCOIN_USE_CAL
		if (!StartCal())
			return;
#endif
#if UCFG_WIN32
		s_hOpenClDll = ::LoadLibrary(String("opencl.dll"));
#endif


#ifdef X_UCFG_BITCOIN_THERMAL_CONTROL //!!!R
		try {
			GetMaxTemperature();
		} catch (RCExc) {
			return;
		}
#endif
#if UCFG_BITCOIN_USE_CAL
		RefPtr<CalEngineWrap, CalEngineWrap> engineWrap = CalEngineWrap::Get();
#endif		
		vector<GDevice> devs = GDevice::GetAll();

		for (int i=0; i<devs.size(); ++i) {
			GDevice& dev = devs[i];
			try {
#if UCFG_BITCOIN_USE_CAL
				CALdeviceinfoRec info = engineWrap->Engine.GetDeviceInfo(dev.m_calDevice);
				CALdeviceattribs attr = engineWrap->Engine.GetDeviceAttribs(dev.m_calDevice);
				bool bEvergreen = info.target >= CAL_TARGET_CYPRESS && info.target <= CAL_TARGET_BARTS;
#else
				bool bEvergreen = dev.IsEvergreen();
#endif
				Blob binary;		

				if (dev.m_clDevice.Valid()) {
					String sCode = miner.GetOpenclCode();
					String key = EXT_STR("opencl_"+dev.m_clDevice.Name << "_" << hash<String>()(sCode));
					Blob blob;
					try {
						/*!!!! some bug in x64
						if (PersistentCache::TryGet(key, blob))
							binary = blob; */
					} catch (RCExc) {
					}
					if (0 == binary.Size) {
						try {
							binary = ExtractImageFromCl(dev.m_clDevice, miner.GetOpenclCode(), bEvergreen);

//!!!							PersistentCache::Set(key, binary);
						} catch (FileNotFoundException&) {
							throw;
						} catch (RCExc ex) {
							*miner.m_pTraceStream << ex.what() << endl;
							throw;
						}
					}
				}
				ptr<GpuMiner> gm;
				if (0 != binary.Size) {
					gm = CreateOpenclMiner(miner, dev.m_clDevice, binary);
				} else {
#if UCFG_BITCOIN_USE_CAL
					gm = CreateCalMiner(miner, info.target, i);
#endif
				}

				gm->SetDevicePortion();
				gm->m_bIsVector2 = dev.m_clDevice.Valid() && bEvergreen;


	#ifndef X_DEBUG//!!!D
				ptr<GpuTask> task = gm->CreateTask();
				task->Prepare(*miner.GetTestData(), nullptr);
				task->Run();
				task->Wait();
				if (1 == task->ParseResults()) 
	//!!!R			if (1 == gm->Run(miner.GetTestData(), nullptr))
	#endif
				{
					ptr<GpuDevice> gdev = new AmdGpuDevice;
#if UCFG_BITCOIN_USE_CAL
					gdev->Name = "GPU "+TargetToString(info.target);
					gdev->Description = Convert::ToString(attr.numberOfSIMD)+" SIMDs";
#else
					gdev->Name = "GPU "+dev.m_clDevice.Name;
					gdev->Description = Convert::ToString(dev.m_clDevice.MaxComputeUnits)+" SIMDs";
#endif
	//				gdev->Index = i;
					gdev->Miner = StaticCast<Object>(gm);
					miner.Devices.push_back(gdev.get());
				}

			} catch (FileNotFoundException&) {
				throw;
			} catch (RCExc& ex) {
				*miner.m_pTraceStream << ex.what() << endl;
			}
		}

	}
} g_openclArchitecture;


} // Coin::


