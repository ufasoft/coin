/*######     Copyright (c) 1997-2013 Ufasoft  http://ufasoft.com  mailto:support@ufasoft.com,  Sergey Pavlov  mailto:dev@ufasoft.com #######################################
#                                                                                                                                                                          #
# This program is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation;  #
# either version 3, or (at your option) any later version. This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the      #
# implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details. You should have received a copy of the GNU #
# General Public License along with this program; If not, see <http://www.gnu.org/licenses/>                                                                               #
##########################################################################################################################################################################*/

#include <el/ext.h>

#include <el/comp/gpu-cuda.h>


#include "miner.h"
#include "bitcoin-sha256.h"

using namespace Ext::Gpu;

namespace Coin {

static CDynamicLibrary s_dllNvcuda;

#ifdef WIN32
#	define NVCUDA_DLL "nvcuda.dll"
#else
#	define NVCUDA_DLL "nvcuda.so"
#endif

class CudaDevice : public GpuDevice {
public:
	CuDevice m_cdev;

	void Start(BitcoinMiner& miner, thread_group *tr) override;
};

class CudaTask : public GpuTask {
	typedef GpuTask base;
public:
	BitcoinMiner& Miner;
	CuDevice m_cdev;
	CuEngine& Eng;
	CuContext Ctx;
	int m_bufTest[512];
	CuBuffer Mem;
	CuModule Module;
	CuKernel Kern;
	int Block;
	int DevicePortion;

	CudaTask(GpuMiner& gm, CuEngine& eng, const CuDevice& cdev)
		:	base(gm)
		,	m_cdev(cdev)
		,	Miner(gm.Miner)
		,	Eng(eng)
		,	Block(64)
	{
		Ctx = m_cdev.CreateContext(CU_CTX_SCHED_BLOCKING_SYNC);
		Mem.Alloc(sizeof m_bufTest);
		String skernel = Miner.GetCudaCode();
		Module = CuModule::FromData((const char*)skernel);
		Kern = Module.GetFunction("_Z6searchjjjjjjjjjjjjjjjjjjjjPj");

		int maxGridX = m_cdev.MaxGridDimX;

		DevicePortion = 2*1024*1024 / UCFG_BITCOIN_NPAR;
		for (int simd=m_cdev.MultiprocessorCount; simd; simd>>=1) {
			int newDevicePortion = DevicePortion << 1;
			if (newDevicePortion > maxGridX*Block/UCFG_BITCOIN_NPAR)
				break;
			DevicePortion = newDevicePortion;
		}
	}

	void Prepare(const BitcoinWorkData& wd, WorkerThreadBase *pwt) override {
		m_pwt = pwt;
		WorkData = wd;
		m_nparOrig = WorkData.LastNonce-WorkData.FirstNonce+1,
		m_npar = m_nparOrig; //!!!?

		PrepareData(WorkData.Midstate.constData(), WorkData.Data.constData()+64, WorkData.Hash1.constData());
		m_w[3] = wd.FirstNonce;
	}

	void Run() override {
		ZeroStruct(m_bufTest);
		Mem.FromHostMem(m_bufTest, sizeof(m_bufTest));

		for (int i=0; i<8; ++i)
			Kern.ParamSetv(i*4, m_midstate[i]);
		Kern.ParamSetv(32, m_midstate_after_3[4]);	// B1
		Kern.ParamSetv(36, m_midstate_after_3[5]);	// C1
		Kern.ParamSetv(40, m_midstate_after_3[6]);	// D1
		Kern.ParamSetv(44, m_midstate_after_3[0]);	// F1
		Kern.ParamSetv(48, m_midstate_after_3[1]);	// G1
		Kern.ParamSetv(52, m_midstate_after_3[2]);	// H1

#ifdef X_DEBUG//!!!D
		WorkData.FirstNonce = 0x8d9cb675;
#endif

		Kern.ParamSetv(56, WorkData.FirstNonce);
		Kern.ParamSetv(60, m_w[2]);	
		Kern.ParamSetv(64, m_w[16]);
		Kern.ParamSetv(68, m_w[17]);
		Kern.ParamSetv(72, m_preVal4);
		Kern.ParamSetv(76, m_t1);
		Kern.ParamSetv(80, &Mem.m_h, sizeof(Mem.m_h));
		Kern.ParamSetSize(80+sizeof(Mem.m_h));

#ifdef X_DEBUG//!!!D
		Kern.SetBlockShape(1);
		Kern.Launch(1, 1);
		Mem.ToHostMem(m_bufTest, sizeof(m_bufTest));

		BitcoinSha256 sha;
		Blob data = WorkData.Data;
		*(UInt32*)(data.data()+64+12) = WorkData.FirstNonce;
		sha.PrepareData(m_midstate, data.constData()+64, WorkData.Hash1.constData());
		memcpy(sha.m_w1, sha.m_midstate_after_3, 8*sizeof(UInt32));		
		sha.CalcRoundsTest(sha.m_w, sha.m_midstate, sha.m_w1, 16, 3, 64);
		UInt32 v[8];
		memcpy(v, g_sha256_hinit, 8*sizeof(UInt32));
		UInt32 ee = sha.CalcRoundsTest(sha.m_w1, v, v, 16, 0, 1);				// We enough this

		if (m_bufTest[0])
			m_npar = m_npar;

#else
		Kern.SetBlockShape(Block);
		Kern.Launch(m_npar/Block, 1);
		Mem.ToHostMem(m_bufTest, sizeof(m_bufTest));
#endif
	}

	bool IsComplete() override {
		return true;			//!!!?
	}

	void Wait() override {
		Throw(E_NOTIMPL);
	}

	int ParseResults() override {
		int r = 0;
		if (m_bufTest[256]) {
			for (int i=0; i<256; ++i) {
				if (UInt32 nonce = m_bufTest[i]) {
					++r;
					if (m_pwt)
						m_pwt->Miner.TestAndSubmit(*m_pwt, WorkData, nonce);
				}
			}
		}

		if (m_pwt)
			Interlocked::Add(m_pwt->Miner.HashCount, m_nparOrig/UCFG_BITCOIN_NPAR);
		return r;
	}
};

class CudaMiner : public GpuMiner {
	typedef GpuMiner base;
public:
	CuEngine& Eng;
	CuDevice m_cdev;

	CudaMiner(BitcoinMiner& miner, CuEngine& eng, const CuDevice& cdev)
		:	base(miner)
		,	Eng(eng)
		,	m_cdev(cdev)
	{}

	ptr<GpuTask> CreateTask() override {
		ptr<GpuTask> r = new CudaTask(_self, Eng, m_cdev);
		r->m_bIsVector2 = m_bIsVector2;
		return r;
	}

	int Run(const BitcoinWorkData& wd, WorkerThreadBase *pwt) override { Throw(E_NOTIMPL); }
};

class CudaThread : public GpuThreadBase {
	typedef GpuThreadBase base;
public:
	CudaThread(BitcoinMiner& miner, thread_group *tr)
		:	base(miner, tr)
	{
	}

	void Execute() override {
		Name = "CUDA Thread";

		ptr<CudaMiner> gm = static_cast<CudaMiner*>(Device->Miner.get());
		ptr<CudaTask> task = static_cast<CudaTask*>(gm->CreateTask().get());

		for (BitcoinWorkData wd; !m_bStop && (wd = Miner.GetWorkForThread(_self, task->DevicePortion, false));) {
			StartRound();
			task->Prepare(wd, this);
			task->Run();
			task->ParseResults();
			CompleteRound(task->WorkData.LastNonce-task->WorkData.FirstNonce+1);
		}
	}
};

void CudaDevice::Start(BitcoinMiner& miner, thread_group *tr) {
	ptr<GpuThreadBase> t = new CudaThread(miner, tr);
	t->Device = this;				//!!! right thing only when 1 device
	miner.Threads.push_back(t.get());
	t->Start();
}


class CudaArchitecture : public GpuArchitecture {
	CuEngine m_eng;
public:
	CudaArchitecture()
		:	m_eng(false)	
	{	
	}

	void FillDevices(BitcoinMiner& miner) override {
		try {
			DBG_LOCAL_IGNORE_NAME(MAKE_HRESULT(SEVERITY_ERROR, FACILITY_GPU_CU, CUDA_ERROR_NO_DEVICE), ignCUDA_ERROR_NO_DEVICE);	
			DBG_LOCAL_IGNORE_WIN32(ERROR_MOD_NOT_FOUND);
#if defined(_DEBUG) && UCFG_WIN32 //!!!D
			if (!LoadLibraryA(NVCUDA_DLL))
				return;
#endif
			s_dllNvcuda.Load(NVCUDA_DLL);
			m_eng.Init();
		} catch (RCExc) {
			return;
		}

		for (int i=0, count=m_eng.DeviceCount(); i<count; ++i) {
			CuDevice cdev = m_eng.GetDevice(i);

			ptr<CudaMiner> gm = new CudaMiner(miner, m_eng, cdev);

			ptr<CudaTask> task = static_cast<CudaTask*>(gm->CreateTask().get());
			task->Prepare(miner.GetTestData(), nullptr);
			task->Run();
			if (1 == task->ParseResults()) {
				ptr<CudaDevice> dev = new CudaDevice;
				dev->m_cdev = cdev;
				dev->Name = "GPU "+cdev.Name;
				dev->Description = Convert::ToString(cdev.MultiprocessorCount)+" SIMDs";
				dev->Miner = StaticCast<Object>(gm);
				miner.Devices.push_back(dev.get());
			}
		}
	}
} g_cudaArchitecture;

} // Coin::







