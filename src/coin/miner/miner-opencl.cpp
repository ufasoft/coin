/*######   Copyright (c) 2013-2015 Ufasoft  http://ufasoft.com  mailto:support@ufasoft.com,  Sergey Pavlov  mailto:dev@ufasoft.com ####
#                                                                                                                                     #
# 		See LICENSE for licensing information                                                                                         #
#####################################################################################################################################*/

#include <el/ext.h>

#include <el/comp/ext-opencl.h>

#include "miner.h"
#include "bitcoin-sha256.h"

namespace Coin {

class OpenclMiner;

class OpenclTask : public GpuTask {
	typedef GpuTask base;
public:
	OpenclMiner& m_clMiner;
	cl::CommandQueue Queue;
	cl::Kernel Kern;

	cl::Buffer Buffer;
	cl::Event m_ev;
	int m_buf[512];
	int m_bufTest[512];

	OpenclTask(OpenclMiner& clMiner);
	void Prepare(const BitcoinWorkData& wd, WorkerThreadBase *pwt) override;
	void Run() override;
	bool IsComplete() override;
	void Wait() override;
	int ParseResults() override;
};

class ScryptOpenclTask : public OpenclTask {
	typedef OpenclTask base;
public:
	ScryptOpenclTask(OpenclMiner& clMiner);
	void Prepare(const BitcoinWorkData& wd, WorkerThreadBase *pwt) override;
	void Run() override;
	int ParseResults() override;
	bool IsComplete() override;
private:
	Blob m_iodata, m_ihashes;
	cl::Buffer m_iobuf;
	CBool m_bComplete;

	void OnComplete();
	static void CL_CALLBACK Callback(cl_event, cl_int, void *ctx);
};

class OpenclMiner : public GpuMiner {
	typedef GpuMiner base;
public:
	cl::Device Device;
	cl::Context Ctx;
	cl::Program Prog;
	cl::CommandQueue Queue;
	vector<cl::Buffer> ScratchBuffers;

	mutex MtxEvent;
//	cl::Event EvKernSync;
	queue<pair<cl::Buffer, cl::Event>> ScratchQueue;

	OpenclMiner(BitcoinMiner& miner, const cl::Device& dev, const ConstBuf& binary);

//!!!	void InitBeforeRun() override;
	ptr<GpuTask> CreateTask() override {
		switch (Miner.HashAlgo) {
		case HashAlgo::Sha256:
			{
				ptr<OpenclTask> r = new OpenclTask(_self);
				r->m_bIsVector2 = m_bIsVector2;
				r->Kern = cl::Kernel(Prog, "search");
				return r;
			}
		case HashAlgo::SCrypt:
			{
				ptr<OpenclTask> r = new ScryptOpenclTask(_self);
				r->m_bIsVector2 = m_bIsVector2;
				r->Kern = cl::Kernel(Prog, "ScryptCore");
				return r;
			}
		default:
			Throw(E_NOTIMPL);
		}
	}
	int Run(const BitcoinWorkData& wd, WorkerThreadBase *pwt) override;

	void SetDevicePortion() override {
		if (Miner.HashAlgo == HashAlgo::SCrypt) {
			size_t size = (size_t)std::min(uint64_t(numeric_limits<size_t>::max()), (uint64_t)Device.get_MaxMemAllocSize());
			uint64_t sizeToUse = Device.GlobalMemSize*2/3;
			DevicePortion = std::max(size_t(std::min(sizeToUse, uint64_t(size)) >> 17), size_t(512)) / UCFG_BITCOIN_NPAR;		// Test Data has size 512
			size_t bufSize = size_t(DevicePortion) * UCFG_BITCOIN_NPAR * 128*1024;
			for (; sizeToUse >= bufSize; sizeToUse-=bufSize)
				ScratchBuffers.push_back(cl::Buffer(Ctx, 0, bufSize));
		} else
			base::SetDevicePortion();				
	}
private:
};

static CEvent s_ev;

void CL_CALLBACK EventNotify(cl_event ev, cl_int status, void *userData) {
	s_ev.Set();
}

OpenclTask::OpenclTask(OpenclMiner& clMiner)
	:	base(clMiner)
	,	m_clMiner(clMiner)
{
	ZeroStruct(m_buf);
	Buffer = cl::Buffer(m_clMiner.Ctx, CL_MEM_WRITE_ONLY|CL_MEM_USE_HOST_PTR, sizeof m_buf, m_buf);
}

void OpenclTask::Prepare(const BitcoinWorkData& wd, WorkerThreadBase *pwt) {
	m_pwt.reset(pwt);
	WorkData = (BitcoinWorkData*)&wd;
	m_nparOrig = WorkData->LastNonce - WorkData->FirstNonce + 1,
	m_npar = m_nparOrig; //!!!?

	PrepareData(WorkData->Midstate.constData(), WorkData->Data.constData()+64, WorkData->Hash1.constData());
	m_w[3] = wd.FirstNonce;
}

void OpenclTask::Run() {
	for (int i=0; i<8; ++i)
		Kern.setArg(i, m_midstate[i]);

	Kern.setArg(8, m_midstate_after_3[4]);	// B1
	Kern.setArg(9, m_midstate_after_3[5]);	// C1
	Kern.setArg(10, m_midstate_after_3[6]);	// D1
	Kern.setArg(11, m_midstate_after_3[0]);	// F1
	Kern.setArg(12, m_midstate_after_3[1]);	// G1
	Kern.setArg(13, m_midstate_after_3[2]);	// H1
	
	Kern.setArg(14, m_bIsVector2 ? (uint32_t(WorkData->FirstNonce) >> 1) : uint32_t(WorkData->FirstNonce));
	Kern.setArg(15, m_w[2]);	
	Kern.setArg(16, m_w[16]);
	Kern.setArg(17, m_w[17]);
	Kern.setArg(18, m_preVal4);
	Kern.setArg(19, m_t1);

	Kern.setArg(20, Buffer);

	cl::NDRange range(m_bIsVector2 ? m_nparOrig/2 : m_nparOrig);

#ifdef X_DEBUG//!!!D
	static int s_i;
	if (++s_i > 1) {
		m_clMiner.Queue.Enqueue(m_clMiner.Kern, range);
		m_ev = m_clMiner.Queue.EnqueueMarker();
		m_ev.SetCallback(&EventNotify, 0);
		m_clMiner.Queue.Flush();
		return;
	}
#endif

	m_clMiner.Queue.enqueueNDRangeKernel(Kern, cl::NullRange, range);
	m_ev = m_clMiner.Queue.enqueueReadBuffer(Buffer, false, 0, sizeof(m_bufTest), m_bufTest);
//!!!R	m_ev = m_clMiner.Queue.enqueueMarkerWithWaitList();

	m_clMiner.Queue.flush();
}

bool OpenclTask::IsComplete() {
#ifdef X_DEBUG//!!!D

	//Cl::Wait(m_ev);
	//m_ev.Close();
	Sleep(200);
	return true;
#endif
	cl::ExecutionStatus st = m_ev.Status;
	if (st == cl::ExecutionStatus::Complete) {
		return true;
	}
	int rc = (int)st;
	if (rc < 0)
		cl::ClCheck(rc);
	return false;
}

void OpenclTask::Wait() {
	m_ev.wait();
}

int OpenclTask::ParseResults() {
	int r = 0;
	if (m_bufTest[256]) {
		for (int i=0; i<256; ++i) {
			if (uint32_t nonce = m_bufTest[i]) {
				++r;
				if (m_pwt)
					m_pwt->Miner.TestAndSubmit(WorkData, nonce);
			}
		}
		ZeroStruct(m_bufTest);
		m_clMiner.Queue.enqueueWriteBuffer(Buffer, false, 0, sizeof(m_bufTest), m_bufTest);
	}

	if (m_pwt)
		m_pwt->Miner.aHashCount += m_nparOrig/UCFG_BITCOIN_NPAR;
	return r;
}

ptr<GpuMiner> CreateOpenclMiner(BitcoinMiner& miner, cl::Device& dev, const ConstBuf& binary) {
	return new OpenclMiner(miner, dev, binary);
}

OpenclMiner::OpenclMiner(BitcoinMiner& miner, const cl::Device& dev, const ConstBuf& binary)
	:	base(miner)
	,	Device(dev)
{
	TRC(2, "Profiling Timer Resolution: " << size_t(dev.ProfilingTimerResolution));

	cl::CContextProps props;
	props[CL_CONTEXT_PLATFORM] = (cl_context_properties)dev.get_Platform().Id;
//!!!?	props[CL_CONTEXT_OFFLINE_DEVICES_AMD] = 1;
	Ctx = cl::Context(dev, &props);
	if (miner.HashAlgo != HashAlgo::SCrypt)
		Queue = cl::CommandQueue(Ctx, Device);
	
	Prog = cl::Program::FromBinary(Ctx, Device, binary);
	vector<cl::Device> devs(1, Device);
	Prog.build(devs);
}

int OpenclMiner::Run(const BitcoinWorkData& wd, WorkerThreadBase *pwt) {
	return 0;
}

ScryptOpenclTask::ScryptOpenclTask(OpenclMiner& clMiner)
	:	base(clMiner)
	,	m_iobuf(m_clMiner.Ctx, 0, size_t(clMiner.DevicePortion) * UCFG_BITCOIN_NPAR * 128)

{
	cl_command_queue_properties prop = 0;
#ifdef X_DEBUG
	prop |= CL_QUEUE_PROFILING_ENABLE;
#endif
	Queue = cl::CommandQueue(m_clMiner.Ctx, m_clMiner.Device, prop);

	m_iodata.Size = size_t(clMiner.DevicePortion) * UCFG_BITCOIN_NPAR * 128;
	m_ihashes.Size = size_t(clMiner.DevicePortion) * UCFG_BITCOIN_NPAR * 32;
}

void ScryptOpenclTask::Prepare(const BitcoinWorkData& wd, WorkerThreadBase *pwt) {
	m_pwt.reset(pwt);
	WorkData = (BitcoinWorkData*)&wd;
	m_nparOrig = WorkData->LastNonce - WorkData->FirstNonce + 1,
	m_npar = m_nparOrig; //!!!?
	
	uint32_t data[20];
	for (int i=0; i<size(data); ++i)
		data[i] = _byteswap_ulong(((uint32_t*)WorkData->Data.data())[i]);

	for (uint32_t nonce=wd.FirstNonce, end=wd.LastNonce+1, n=0; nonce!=end; ++nonce, ++n) {
		data[19] = nonce;

		ConstBuf password(data, sizeof data);
		SHA256 sha;
		Blob ihash(sha.ComputeHash(password));
		const uint32_t *pIhash = (uint32_t*)ihash.constData();
		memcpy(m_ihashes.data()+32*n, pIhash, 32);

		uint32_t tmp[32];
#if UCFG_CPU_X86_X64
		CalcPbkdf2Hash_80_4way(tmp, pIhash, password);
#else
		for (int i=0; i<4; ++i) {
			Blob blob = CalcPbkdf2Hash(pIhash, password, i+1);
			memcpy(tmp + i*8, blob.constData(), 32);
		}
#endif
		ShuffleForSalsa((uint32_t*)(m_iodata.data() + n*128), tmp);

#ifdef X_DEBUG//!!!D
		AlignedMem am((1025*32)*sizeof(uint32_t), 128);
		memcpy(tmp, m_iodata.data() + n*128, 128);
		ScryptCore_x86x64(tmp, (uint32_t*)am.get());
		uint32_t tmp1[32];
		UnShuffleForSalsa(tmp1, tmp);
		Blob blob = CalcPbkdf2Hash((uint32_t*)(m_ihashes.data()+32*n), ConstBuf(tmp1, sizeof tmp1), 1);
		const uint32_t *p = (const uint32_t*)blob.constData();
		if (p[7] < 0x10000) {
			cout << endl;
		}
#endif

	}
}

int ScryptOpenclTask::ParseResults() {
	int r = 0;

	for (int n=0; n<m_npar; ++n) {
		uint32_t tmp[32];
		UnShuffleForSalsa(tmp, (uint32_t*)(m_iodata.data() + n*128));
		Blob blob(CalcPbkdf2Hash((uint32_t*)(m_ihashes.data()+32*n), ConstBuf(tmp, sizeof tmp), 1));
		const uint32_t *p = (const uint32_t*)blob.constData();
		if (p[7] < 0x10000) {
			++r;
			if (m_pwt)
				m_pwt->Miner.TestAndSubmit(WorkData, _byteswap_ulong(WorkData->FirstNonce + n));
		}	
	}

	if (m_pwt)
		m_pwt->Miner.aHashCount += m_nparOrig/UCFG_BITCOIN_NPAR;
	return r;
}

bool ScryptOpenclTask::IsComplete() {
	return m_bComplete;
}

void ScryptOpenclTask::OnComplete() {
	ParseResults();

	m_pwt->CompleteRound(m_npar);
	ptr<BitcoinWorkData> wd;
	if (!m_pwt->m_bStop && (wd = m_clMiner.Miner.GetWorkForThread(*m_pwt, m_gpuMiner.DevicePortion, true))) {
		Prepare(*wd, m_pwt);
		Run();
		m_pwt->StartRound();
	} else {
		m_bComplete = true;
		m_pwt->CvComplete.notify_one();
	}
}

void CL_CALLBACK ScryptOpenclTask::Callback(cl_event ev, cl_int st, void *ctx) {
	cl::Event evObj;
	evObj.m_h = ev;

	if (st == CL_COMPLETE)
		((ScryptOpenclTask*)ctx)->OnComplete();
	else if (st < 0)
		cl::ClCheck(st);
}

#ifdef X_DEBUG

void CL_CALLBACK OclProfileCallback(cl_event ev, cl_int st, void *ctx) {
	cl::Event evObj;
	evObj.m_h = ev;

	TRC(3, cl::CommandTypeToString(evObj.CommandType) << " " << ((evObj.ProfilingCommandEnd - evObj.ProfilingCommandStart))/1000 << " us");
}

#	define OCL_PROFILE_EVENT(ev) (ev).Retain().setCallback(&OclProfileCallback);
#else
#	define OCL_PROFILE_EVENT(ev) ev
#endif


void ScryptOpenclTask::Run() {
	vector<cl::Event> events(2);
	OCL_PROFILE_EVENT(events[0] = Queue.enqueueWriteBuffer(m_iobuf, false, 0, m_iodata.Size, m_iodata.constData()));
	Kern.setArg(0, m_iobuf);
	EXT_LOCK (m_clMiner.MtxEvent) {
		cl::Buffer scratch;
		if (m_clMiner.ScratchQueue.size() < m_clMiner.ScratchBuffers.size()) {
			scratch = m_clMiner.ScratchBuffers[m_clMiner.ScratchQueue.size()];
			events.resize(1);
		} else {
			scratch = m_clMiner.ScratchQueue.front().first;
			events[1] = m_clMiner.ScratchQueue.front().second;
			m_clMiner.ScratchQueue.pop();
		}
		Kern.setArg(1, scratch);

		events[0] = Queue.enqueueNDRangeKernel(Kern, cl::NullRange, m_nparOrig, cl::NullRange, &events);
		m_clMiner.ScratchQueue.push(make_pair(scratch, events[0]));
		OCL_PROFILE_EVENT(events[0]);
	}
	events.resize(1);
	m_ev = Queue.enqueueReadBuffer(m_iobuf, false, 0, m_iodata.Size, m_iodata.data(), &events);
	if (m_pwt)
		m_ev.Retain().setCallback(&Callback, this);
	OCL_PROFILE_EVENT(m_ev);
	Queue.flush();
}


} // Coin::
