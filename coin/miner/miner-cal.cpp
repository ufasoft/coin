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
#	include <el/comp/gpu-cal.cpp>
#endif

#if UCFG_BITCOIN_THERMAL_CONTROL
#	include <el/comp/gpu-adl.h>
#endif

using namespace Ext::Gpu;

#include "resource.h"
#include "miner.h"
#include "bitcoin-sha256.h"

#include "patch-bfi_int.h"


namespace Coin {


static HMODULE s_hAtiCalRtDll, s_hAtiCalClDll;

String TargetToString(CALtarget target) {
	switch (int(target)) {
	case CAL_TARGET_600: return "ATI RV600";
    case CAL_TARGET_610: return "ATI RV610";
    case CAL_TARGET_630: return "ATI RV630";
    case CAL_TARGET_670: return "ATI RV670";
    case CAL_TARGET_7XX: return "ATI RV700";
    case CAL_TARGET_770: return "ATI RV770";
    case CAL_TARGET_710: return "ATI RC710";
    case CAL_TARGET_730: return "ATI RV730";
    case CAL_TARGET_CYPRESS: return "RV770";
    case CAL_TARGET_JUNIPER: return "Juniper";
    case CAL_TARGET_REDWOOD: return "Redwood";
    case CAL_TARGET_CEDAR: return "Cedar";
    case CAL_TARGET_WRESTLER: return "Wrestler";
    case CAL_TARGET_CAYMAN: return "Cayman";
    case CAL_TARGET_BARTS: return "Barts";
	case CAL_TARGET_TAHITI: return "Tahiti";
	case CAL_TARGET_PITCAIRN: return "Pitcairn";
	case CAL_TARGET_CAPE_VERDE: return "Cape Verde";
	case CAL_TARGET_TRINITY: return "Trinity";
	default:
		return "Unknown";
	};
}


mutex CalEngineWrap::s_cs;
volatile Int32 CalEngineWrap::RefCount;
CalEngineWrap *CalEngineWrap::I;

void MemToResource(CalResource<UInt32>& res, const UInt32 *data, size_t size) {
	CalResMap<UInt32> map(res);
	for (int i=0; i<size; ++i)
		*(map+i*4) = *(map+i*4+1) = *(map+i*4+2) = *(map+i*4+3) = data[i];
}

void MemToResourceCompact(CalResource<UInt32>& res, const UInt32 *data, size_t size) {
	CalResMap<UInt32> map(res);
	memcpy(map, data, size*sizeof(UInt32));
}

bool StartCal() {
#ifdef _WIN32
#	ifdef _WIN64
		String dllName = "aticalrt64.dll", clDllName = "aticalcl64.dll";
#	else
		String dllName = "aticalrt.dll", clDllName = "aticalcl.dll";
#	endif
		if (!(s_hAtiCalRtDll = ::LoadLibrary(dllName)) || !(s_hAtiCalClDll = ::LoadLibrary(clDllName)))
			return false;
		return true;
#else
	return false;
#endif
}

class AmdGpuMiner;

class AmdGpuTask : public GpuTask {
	typedef GpuTask base;
public:
	AmdGpuMiner& GpuMiner;
#if UCFG_BITCOIN_USE_CAL
	CalContext Ctx;
	CalResource<UInt32> m_resI;
	CalResource<UInt32> m_resM3;
	CalGlobalResource<UInt32> m_resO;	
	CALprogramGrid m_grid;
	CALevent m_calEvent;
	CalModule Module;
	ResourceBinder<CalGlobalResource<UInt32> > m_bind;
#endif

	AmdGpuTask(AmdGpuMiner& gpuMiner);

	~AmdGpuTask() {
	}

	void Wait();
	void Prepare(const BitcoinWorkData& wd, WorkerThreadBase *pwt) override;
	void Run() override;
	bool IsComplete() override;
	int ParseResults() override;
};


class AmdGpuMiner : public GpuMiner {
	typedef GpuMiner base;
public:
	RefPtr<CalEngineWrap, CalEngineWrap> EngineWrap;
	CalDevice Device;
	CALdeviceinfo DeviceInfo;
	CALdeviceattribs DeviceAttribs;
	CalImage Image;
//	CalResource<UInt32> m_resCB5;
	int CalWidth;
	int Index;
	CBool m_bEvergreen;
	
//!!!	ptr<AmdGpuTask> Task0, Task1, RunnedTask;

	AmdGpuMiner(BitcoinMiner& miner, int idx);

	~AmdGpuMiner() {
//		if (RunnedTask)
//			RunnedTask->Wait();
	}

	ptr<GpuTask> CreateTask() override {
		ptr<GpuTask> r = new AmdGpuTask(_self);
		r->m_bIsVector2 = m_bIsVector2;
		return r;
	}

	void InitModules() {
//		Task0->Module.Init(Image);
//		Task1->Module.Init(Image);
	}

	bool UseVector() {
		return m_bEvergreen;
	}

	int PacketStep() {
		return UseVector() ? 4 : 1;
	}

	int PacketSize() {
		return PacketStep()*32;
	}


#ifdef X_DEBUG
#	define RESULT_BUFFER_SIZE 512
#else
#	define RESULT_BUFFER_SIZE 0
#endif

	int Run(const BitcoinWorkData& wd, WorkerThreadBase *pwt) override;
};

AmdGpuTask::AmdGpuTask(AmdGpuMiner& gpuMiner)
	:	base(gpuMiner)
	,	GpuMiner(gpuMiner)
	,	Ctx(GpuMiner.Device)
	,	m_resI(GpuMiner.Device, 4)
	,	m_resM3(GpuMiner.Device, 4)
	,	Module(Ctx)
	,	m_resO(false)
	,	m_bind(Module, "g[]", m_resO, false)
{	
	Module.Init(gpuMiner.Image);	
}

void AmdGpuTask::Prepare(const BitcoinWorkData& wd, WorkerThreadBase *pwt) {
	m_pwt = pwt;
	WorkData = wd;
	m_nparOrig = WorkData.LastNonce-WorkData.FirstNonce+1,
	m_npar = m_nparOrig / GpuMiner.PacketStep();
 

//!!!	BitcoinSha256 sha;
	PrepareData(WorkData.Midstate.constData(), WorkData.Data.constData()+64, WorkData.Hash1.constData());
	m_w[3] = wd.FirstNonce;
//			sha.m_w[3] = 0x8D9CB675;		//!!!D

	UInt32 w[16];
	memcpy(w, m_w+1, sizeof(UInt32)*15);
	memcpy(w+15, m_w, sizeof(UInt32)*1);
	MemToResourceCompact(m_resI, w, 16);

	UInt32 cb3[16];
	memcpy(cb3, m_midstate_after_3, sizeof(UInt32)*8);
	memcpy(cb3+8, m_midstate, sizeof(UInt32)*8);
	MemToResourceCompact(m_resM3, cb3, 16);	

	/*!!!
	{ //!!!D
		CalResMap<UInt32> m(m_resCB5);
		*(m + 0) = 3;
		*(m + 1) = 64;
		*(m + 2) = 0;
		*(m + 3) = 61;
	}*/


	Module.Set("cb1", Ctx.GetMem(m_resI));
	Module.Set("cb3", Ctx.GetMem(m_resM3));

//			Module.Set("cb5", Ctx.GetMem(m_resCB5));

#if RESULT_BUFFER_SIZE
	m_resO.Init(GpuMiner.Device, 128, RESULT_BUFFER_SIZE/128);
#else
	int height = m_npar/GpuMiner.CalWidth;
	m_resO.Init(GpuMiner.Device, GpuMiner.CalWidth, std::max(1, (height* GpuMiner.PacketStep()+127)/128));
#endif
	m_bind.Bind();

	m_grid.flags            = 0;
	m_grid.gridBlock.width  = 128; // should be same value as what is in the kernel for thread group size.
	m_grid.gridBlock.height = 1;
	m_grid.gridBlock.depth  = 1;
	m_grid.gridSize.width   = (m_npar + m_grid.gridBlock.width - 1) / m_grid.gridBlock.width;
	m_grid.gridSize.height  = 1;
	m_grid.gridSize.depth   = 1;
}

void AmdGpuTask::Run() {
	m_calEvent = Module.RunGrid(m_grid);
}

void AmdGpuTask::Wait() {
	Module.Wait(m_calEvent, m_pwt ? GpuMiner.Miner.GpuIdleMilliseconds : 5);
}

bool AmdGpuTask::IsComplete() {
	CALresult rc = calCtxIsEventDone(Module.m_ctx, m_calEvent);
	if (rc == 0)
		return true;
	if (rc != CAL_RESULT_PENDING)
		CalCheck(rc);
	return false;
}

int AmdGpuTask::ParseResults() {
	int r = 0;
	CalResMap<UInt32> mapO(m_resO);
	UInt32 *p = mapO;

	bool bUseVector = GpuMiner.UseVector();
#if RESULT_BUFFER_SIZE
	for (int j=0, e=RESULT_BUFFER_SIZE; j<e; ++j, p+=GpuMiner.PacketStep()) {
#else
	for (int j=0, e=m_nparOrig/GpuMiner.PacketSize(); j<e; ++j, p+=GpuMiner.PacketStep()) {
#endif
		if (p[0] ||
			bUseVector && (p[1] || p[2] || p[3])) {
			++r;
			TRC(1, "GPU found a hash for i=  " << j*GpuMiner.PacketSize() << " / " << m_nparOrig);
			if (m_pwt) {
				for (int i=0; i<GpuMiner.PacketSize(); ++i)
					GpuMiner.Miner.TestAndSubmit(*m_pwt, WorkData, WorkData.FirstNonce+(j*GpuMiner.PacketSize())+i);
			}
		}
	}
	if (m_pwt)
		Interlocked::Add(GpuMiner.Miner.HashCount, m_nparOrig/UCFG_BITCOIN_NPAR);
	m_bind.Unbind();
	return r;

}


AmdGpuMiner::AmdGpuMiner(BitcoinMiner& miner, int idx)
	:	base(miner)
	,	EngineWrap(CalEngineWrap::Get())
	,	Device(EngineWrap->Engine.GetDevice(idx))
//		,	m_resCB5(Device, 1)
	,	CalWidth(1024)
	,	Index(idx)
{
	DeviceInfo = EngineWrap->Engine.GetDeviceInfo(Device.Index);
	DeviceAttribs = EngineWrap->Engine.GetDeviceAttribs(Device.Index);
	CalWidth = DeviceInfo.maxResource1DWidth;
		
//!!!	Task0 = new AmdGpuTask(_self);
//!!!	Task1 = new AmdGpuTask(_self);
}

int AmdGpuMiner::Run(const BitcoinWorkData& wd, WorkerThreadBase *pwt) {
	return 0;
	/*!!!
	if (pwt) {
		Task0->Prepare(wd, pwt);
		bool bRunned = false;
		if (bRunned = RunnedTask)
			(Task1 = RunnedTask)->Wait();
		(RunnedTask = Task0)->Run();
		Task0 = Task1;
		int r = 0;
		if (bRunned)
			r = Task0->ParseResults();
		return r;
	} else {
		Task0->Prepare(wd, pwt);
		Task0->Run();
		Task0->Wait();
		return Task0->ParseResults();
	}*/
	
//			cerr << npar << " complete\n";
/*!!!D
	memcpy(sha.m_w1, sha.m_midstate_after_3, 8*sizeof(UInt32));		
	sha.CalcRoundsTest(sha.m_w, sha.m_midstate, sha.m_w1, 16, 3, 64);
	UInt32 v[8];
	memcpy(v, g_sha256_hinit, 8*sizeof(UInt32));
	UInt32 ee = sha.CalcRoundsTest(sha.m_w1, v, v, 16, 0, 61);				// We enough this
	*/

}

ptr<GpuMiner> CreateCalMiner(BitcoinMiner& miner, CALtarget calTarget, int idx) {
	bool bEvergreen = calTarget >= CAL_TARGET_CYPRESS && calTarget <= CAL_TARGET_BARTS;
	CalObject obj;
	obj.Compile(miner.GetCalIlCode(bEvergreen), calTarget);
	ptr<AmdGpuMiner> m = new AmdGpuMiner(miner, idx);
	m->m_bEvergreen = bEvergreen;
	if (bEvergreen) {
		CalImage image;
		image.Add(obj);
		image.Link();
		Blob binary;
		binary.Size = image.Size;
		image.Write(Buf(binary.data(), binary.Size));
		PatchBfiInt patcher;
		patcher.Patch(Buf(binary.data(), binary.Size));
		m->Image.Read(binary);
	} else {						
		m->Image.Add(obj);
		m->Image.Link();
	}
	m->InitModules();
	return m.get();
}

} // Coin::


