/*######   Copyright (c) 2013-2019 Ufasoft  http://ufasoft.com  mailto:support@ufasoft.com,  Sergey Pavlov  mailto:dev@ufasoft.com ####
#                                                                                                                                     #
# 		See LICENSE for licensing information                                                                                         #
#####################################################################################################################################*/

#pragma once

#include EXT_HEADER_CONDITION_VARIABLE

#include <el/inet/http.h>
#include <el/inet/async-text-client.h>
#include <el/inet/json-rpc.h>
using namespace Ext::Inet;

#include "bitcoin-common.h"

#if UCFG_BITCOIN_USE_CAL
#	include <el/comp/gpu-cal.h>
using namespace Ext::Gpu;
#endif


#include "bitcoin-sha256.h"

#include "../util/util.h"
#include "../util/wallet-client.h"
#include "../util/miner-interface.h"

namespace Ext {
	namespace cl {
	class Device;
}}

namespace Coin {

class BitcoinMiner;

const float MAX_GPU_TEMPERATURE = 83; // Celsius
const float MAX_FPGA_TEMPERATURE = 60; // Celsius

const int BITCOIN_DEFAULT_GPU_IDLE_MILLISECONDS = 100;

const uint32_t FULL_TASK = uint32_t(0x100000000ULL / UCFG_BITCOIN_NPAR);

const int MIN_GETWORK_QUEUE = 3,
			MAX_GETWORK_QUEUE = 5;

const int NORMAL_WAIT = 2000, MAX_WAIT = 200000;


class BitcoinWorkData : public Object {
	typedef BitcoinWorkData class_type;
public:
	typedef InterlockedPolicy interlocked_policy;

	ptr<Coin::MinerBlock> MinerBlock;

	DateTime Timestamp;
	Blob Midstate, Data, Hash1;
	HashValue HashTarget;
	uint32_t FirstNonce, LastNonce;
	uint32_t Height;
	uint16_t RollNTime;
	Coin::HashAlgo HashAlgo;

	BitcoinWorkData()
		: FirstNonce(0)
		, LastNonce(uint32_t(-1))
		, Height(uint32_t(-1))
		, RollNTime(0)
		, HashAlgo(Coin::HashAlgo::Sha256)
	{}

	bool IsFull() const {
		return FirstNonce==0 && LastNonce == uint32_t(-1);
	}

	virtual BitcoinWorkData *Clone() const { return new BitcoinWorkData(*this); }

	static ptr<BitcoinWorkData> FromJson(RCString sjson, Coin::HashAlgo algo);
	void SetRollNTime(WebClient& wc);

	uint32_t get_Version() const { return htobe(*(uint32_t*)Data.constData()); }
	void put_Version(uint32_t v) { *(uint32_t*)Data.data() = htobe(v); }
	DEFPROP(uint32_t, Version);

	HashValue get_PrevBlockHash() const;
	void put_PrevBlockHash(const HashValue& v);
	DEFPROP(HashValue, PrevBlockHash);

	HashValue get_MerkleRoot() const;
	void put_MerkleRoot(const HashValue& v);
	DEFPROP(HashValue, MerkleRoot);

	DateTime get_BlockTimestamp() const { return DateTime::from_time_t(htobe(*(uint32_t*)(Data.constData()+68))); }
	void put_BlockTimestamp(const DateTime& dt) { *(uint32_t*)(Data.constData()+68) = betoh((uint32_t)to_time_t(dt)); }
	DEFPROP(DateTime, BlockTimestamp);

	uint32_t get_Bits() const { return htobe(*(uint32_t*)(Data.constData()+72)); }
	void put_Bits(uint32_t v) { *(uint32_t*)(Data.data()+72) = betoh(v); }
	DEFPROP(uint32_t, Bits);

	uint32_t get_Nonce() const { return htobe(*(uint32_t*)(Data.constData() + (HashAlgo==Coin::HashAlgo::Solid ? 84 : 76))); }
	void put_Nonce(uint32_t v) { *(uint32_t*)(Data.data() + (HashAlgo==Coin::HashAlgo::Solid ? 84 : 76)) = betoh(v); }
	DEFPROP(uint32_t, Nonce);

	bool TestNonceGivesZeroH(uint32_t nonce);

};

class ComputationDevice : public Object {
public:
	typedef InterlockedPolicy interlocked_policy;

	ptr<InterlockedObject> Miner;
	String Name, Description;
	Ext::Temperature Temperature;
	atomic<int> aHwErrors;
	CBool IsAmdGpu;
//!!!	int Index;

	ComputationDevice()
		: aHwErrors(0)
	{}

	virtual void Start(BitcoinMiner& miner, thread_group *tr) =0;

	virtual Ext::Temperature GetTemperature() { return Temperature; }
};

class ComputationArchitecture {
public:
	static ComputationArchitecture *s_pChain;
	ComputationArchitecture *m_pNext;

	ComputationArchitecture()
		: m_pNext(exchange(s_pChain, this))
	{
	}

	virtual void FillDevices(BitcoinMiner& miner) =0;
	virtual void DetectNewDevices(BitcoinMiner& miner) {}
};

class GpuArchitecture : public ComputationArchitecture {
};

class SerialDeviceArchitecture : public ComputationArchitecture {
public:
	void DetectNewDevices(BitcoinMiner& miner) override;
protected:
	DateTime m_dtNextDetect;
	String SysName;

	bool TimeToDetect();
};

class FpgaArchitecture : public SerialDeviceArchitecture {
protected:
};


class WorkerThreadBase : public Thread {
	typedef Thread base;
public:
	BitcoinMiner& Miner;
	int64_t m_prevTsc;
	condition_variable CvComplete;
	uint32_t DevicePortion;
	ptr<ComputationDevice> Device;

	WorkerThreadBase(BitcoinMiner& miner, thread_group *tr)
		: base(tr)
		, Miner(miner)
		, m_prevTsc(0)
		, DevicePortion(128)
		, CurrentWebClient(0)
		, m_nHashes(0)
	{
	}

	virtual String GetDeviceName() { return nullptr; };
	void Stop() override;

	void CompleteRound(uint64_t nHashes);

	void StartRound() {
		m_dtStartRound = Clock::now();
	}
protected:
	WebClient *CurrentWebClient;
	TimeSpan m_span;
	uint64_t m_nHashes;

#if UCFG_WIN32
	void OnAPC() override {
		base::OnAPC();
		if (CurrentWebClient)
			CurrentWebClient->CurrentRequest->ReleaseFromAPC();
	}
#endif

private:
	DateTime m_dtStartRound;

	friend class BitcoinMiner;
};

class XptWorkData;

class CpuMinerThread : public WorkerThreadBase {
	typedef WorkerThreadBase base;
public:
	CpuMinerThread(BitcoinMiner& miner, thread_group *tr)
		: base(miner, tr)
	{}

	String GetDeviceName() override {
#if UCFG_CPU_X86_X64
		return "CPU/"+CpuInfo().Name +"/"+ Convert::ToString(CpuInfo().Cpuid(1).EAX, 16);
#else
		return "CPU";
#endif
	}
protected:
	void Execute() override;

	friend class BitcoinMiner;
};


class CpuDevice : public ComputationDevice {
public:
	void Start(BitcoinMiner& miner, thread_group *tr) override;
};

class GpuDevice : public ComputationDevice {
};

class AmdGpuDevice : public GpuDevice {
public:
	AmdGpuDevice() {
		IsAmdGpu = true;
	}

	void Start(BitcoinMiner& miner, thread_group *tr) override;
};

class GpuThreadBase : public WorkerThreadBase {
	typedef WorkerThreadBase base;
public:
	GpuThreadBase(BitcoinMiner& miner, thread_group *tr)
		: base(miner, tr)
	{}
};

struct SUrlTtr {
	String Url;
	int TtrMinutes;

	bool operator==(const SUrlTtr& x) const {
		return Url==x.Url && TtrMinutes==x.TtrMinutes;
	}

	bool operator!=(const SUrlTtr& x) const {
		return !operator==(x);
	}
};

class BitcoinWebClient : public WebClient {
	typedef WebClient base;

public:
	int Timeout;

	BitcoinWebClient()
		: Timeout(-1)
	{}
protected:
	void OnHttpWebRequest(HttpWebRequest& req) {
		base::OnHttpWebRequest(req);
		if (Timeout >= 0)
			req.Timeout = Timeout;
	}
};


void ThrowRejectionError(RCString reason);

class StratumTask : public JsonRpcRequest {
public:
	ptr<BitcoinWorkData> m_wd;
	int State;

	StratumTask(int state)
		: State(0)
	{}
};

class ConnectionClient {
public:
	BitcoinMiner& Miner;

	mutex MtxData;
	ptr<Coin::MinerBlock> MinerBlock;

	ConnectionClient(BitcoinMiner& miner)
		: Miner(miner)
	{}

	virtual ~ConnectionClient() {}
	virtual void Submit(BitcoinWorkData *wd) =0;
	virtual void SetEpServer(const IPEndPoint& epServer) =0;
	virtual bool HasWorkData() { return MinerBlock; }
	virtual ptr<BitcoinWorkData> GetWork();
	virtual void OnPeriodic() {}
};

class StratumClient : public AsyncTextClient, public ConnectionClient {
	typedef AsyncTextClient base;
public:
	HashValue HashTarget;					// these values have the same meaning
	Ext::Inet::JsonRpc JsonRpc;
	Blob ExtraNonce1, ExtraNonce2;

	double CurrentDifficulty;				//
	int State;

	StratumClient(Coin::BitcoinMiner& miner);
	void Call(RCString method, const vector<VarValue>& params, ptr<StratumTask> task = nullptr);
	void Submit(BitcoinWorkData *wd) override;
protected:
	void SetEpServer(const IPEndPoint& epServer) override { EpServer = epServer; }

	void SetDifficulty(double difficulty);
	void OnLine(RCString line) override;
	void Execute() override;
};

class MINER_CLASS BitcoinMiner : public IMiner {
	typedef BitcoinMiner class_type;

	CInt<int> m_getWorkMethodLevel;

	String BitcoinUrl;
	vector<SUrlTtr> HostList;
	CInt<int> HostIndex;

	String GetCurrentUrl();

	mutex m_csLongPollingThread;
	ptr<Thread> m_LongPollingThread;

	Coin::HashAlgo m_hashAlgo;
public:
	HashValue HashBest;

	DateTime DtStart;
	int64_t EntireHashCount;

	mutex m_csGetWork;
	ManualResetEvent m_evDataReady;
	AutoResetEvent m_evGetWork;

	int GpuIdleMilliseconds;
	CBool m_bTryGpu, m_bTryFpga, m_bLongPolling;

	mutex MtxDevices;
	vector<ptr<ComputationDevice>> Devices;
	unordered_set<String> WorkingDevices, TestedDevices;

	ostream *m_pTraceStream;
	wostream *m_pWTraceStream;

	int m_msWait;
	int NPAR;

	atomic<int32_t> aHashCount;
	volatile float ChainsExpectedCount;
	atomic<int> aSubmittedCount, aAcceptedCount;

	CInt<int> Intensity;
	int Verbosity;
	String UserAgentString;

	observer_ptr<Coin::ConnectionClient> ConnectionClient;
	ptr<Thread> ConnectionClientThread;

	ptr<IWalletClient> WalletClient;
	String DestinationAddress;

	mutex m_csCurrentData;
	ptr<Coin::MinerBlock> MinerBlock;

	typedef list<ptr<BitcoinWorkData>> CTaskQueue;
	CTaskQueue TaskQueue;

	list<ptr<BitcoinWorkData>> WorksToSubmit;

	DateTime m_dtNextGetWork;

	String MainBitcoinUrl, Login, Password;
	DateTime m_dtSwichToMainUrl;
	String ProxyString;

	observer_ptr<thread_group> m_tr;
	vector<ptr<Thread> > Threads;
	ptr<Thread> GetWorkThread;
	ptr<Thread> GpuThread;
	int m_minGetworkQueue;
	int ThreadCount;
	Temperature MaxGpuTemperature, MaxFpgaTemperature;

	int GetworkPeriod;
	float Speed;					// float to be atomic
	float CPD; 						//!!! should be atomic
	volatile uint32_t MaxHeight;

	CBool m_bStop;
	CBool m_bInited;
	CBool Started;
	CBool Pooled;

	BitcoinMiner();
	virtual ~BitcoinMiner() {}

	void Release() override { delete this; }

	Coin::HashAlgo get_HashAlgo() { return m_hashAlgo; }
	void put_HashAlgo(Coin::HashAlgo v);
	DEFPROP(Coin::HashAlgo, HashAlgo);

	void InitDevices(vector<String>& selectedDevs);
	ptr<BitcoinWorkData> GetWorkForThread(WorkerThreadBase& wt, uint32_t portion, bool bAllHashAlgoAllowed);
	virtual BitcoinWebClient GetWebClient(WorkerThreadBase *wt);
	virtual void SetSpeedCPD(float speed, float cpd);
	//!!!	void SetNewData(const BitcoinWorkData& wd);
	void SetNewData(const BitcoinWorkData& wd, bool bClearOld = false);
	void SetWebInfo(const WebHeaderCollection& headers);
	void CheckLongPolling(const WebHeaderCollection& respHeaders, RCString longPollUri = nullptr, RCString longPollId = nullptr);
	void CallNotifyRequest(WorkerThreadBase& wt, RCString url, RCString longPollId);

	String GetMethodName(bool bSubmit);
	ptr<BitcoinWorkData> GetWorkFromMinerBlock(const DateTime& now, Coin::MinerBlock *minerBlock);
	virtual ptr<BitcoinWorkData> GetWork(WebClient*& curWebClient);
	virtual void SubmitResult(WebClient*& curWebClient, const BitcoinWorkData& wd);

	void SetIntensity(int v) override {
		Intensity = v;
	}

	bool TestAndSubmit(BitcoinWorkData *wd, uint32_t beNonce);
	virtual String GetCalIlCode(bool bEvergreen);
	virtual String GetOpenclCode();
	virtual String GetCudaCode();

	void Print(const BitcoinWorkData& wd, bool bSuccess, RCString message = "");

	bool UseSse2() {
#if UCFG_CPU_X86_X64
		return CpuInfo().Features.SSE2;
#endif
		return false;
	}

	void CreateConnectionClient(RCString s);

	void Start(thread_group *tr);
	void Start() override { Start(nullptr); }

	virtual void OnRoundComplete() {}

	void Stop() override;

	void SetMainUrl(const char *url) override { MainBitcoinUrl = url; }
	void SetLogin(const char *login) override { Login = login; }
	void SetPassword(const char *password) override { Password = password; }
	void SetThreadCount(int n) override { ThreadCount = n; }
	void SetHashAlgo(int a) override { HashAlgo = (Coin::HashAlgo)a; }
	double GetSpeed() override { return Speed; }

	ptr<BitcoinWorkData> GetTestData();
private:
	friend class CpuMinerThread;
	friend class LongPollingThread;
	friend class GetWorkThread;
};

class GpuMiner;

class GpuTask : public BitcoinSha256 {
public:
	ptr<BitcoinWorkData> WorkData;
	observer_ptr<WorkerThreadBase> m_pwt;
	GpuMiner& m_gpuMiner;
	int m_npar, m_nparOrig;
	CBool m_bIsVector2;

	GpuTask(GpuMiner& gpuMiner)
		: m_gpuMiner(gpuMiner)
	{}

	virtual void Prepare(const BitcoinWorkData& wd, WorkerThreadBase *pwt) =0;
	virtual void Run() =0;
	virtual bool IsComplete() =0;
	virtual void Wait() =0;
	virtual int ParseResults() =0;
};

class GpuMiner : public InterlockedObject {
public:
	BitcoinMiner& Miner;
	uint32_t DevicePortion;
	CBool m_bIsVector2;

	GpuMiner(BitcoinMiner& miner)
		: Miner(miner)
		, DevicePortion(1024 * 1024 / UCFG_BITCOIN_NPAR)
	{}

//!!!	virtual void InitBeforeRun() {}
	virtual ptr<GpuTask> CreateTask() =0;
	virtual int Run(const BitcoinWorkData& wd, WorkerThreadBase *pwt) =0;

	virtual void SetDevicePortion() {
		DevicePortion = std::max(256, 1 << (20 + Miner.Intensity)) / UCFG_BITCOIN_NPAR;
	}
};

ptr<GpuMiner> CreateOpenclMiner(BitcoinMiner& miner, cl::Device& dev, RCSpan binary);

#if UCFG_BITCOIN_USE_CAL
bool StartCal();
ptr<GpuMiner> CreateCalMiner(BitcoinMiner& miner, CALtarget calTarget, int idx);
String TargetToString(CALtarget target);

struct CalEngineWrap {
	CalEngine Engine;

	static mutex s_cs;
	static atomic<int> aRefCount;
	static CalEngineWrap *I;

	static CalEngineWrap *Get() {
		EXT_LOCK (s_cs) {
			if (!I)
				I = new CalEngineWrap;
			return I;
		}
	}

	static void AddRef(CalEngineWrap *p) {
		++aRefCount;
	}

	static void Release(CalEngineWrap *p) {
		EXT_LOCK (s_cs) {
			if (!--aRefCount) {
				delete I;
				I = nullptr;
			}
		}
	}
};


#endif // UCFG_BITCOIN_USE_CAL

class Hasher : public StaticList<Hasher> {
	typedef StaticList<Hasher> base;
public:
	String Name;
	HashAlgo Algo;

	Hasher(RCString name, HashAlgo algo)
		: base(true)
		, Name(name)
		, Algo(algo)
	{}

	static MINER_CLASS Hasher *GetRoot();
	static Hasher& Find(HashAlgo algo);

	virtual HashValue CalcHash(RCSpan cbuf) { Throw(E_NOTIMPL); }
	virtual HashValue CalcWorkDataHash(const BitcoinWorkData& wd);
	virtual uint32_t MineOnCpu(BitcoinMiner& miner, BitcoinWorkData& wd);
protected:
	virtual size_t GetDataSize() noexcept { return 80; }
	virtual void SetNonce(uint32_t *buf, uint32_t nonce) noexcept;
	virtual void MineNparNonces(BitcoinMiner& miner, BitcoinWorkData& wd, uint32_t *buf, uint32_t nonce);
};


} // Coin::
