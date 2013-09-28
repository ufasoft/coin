#pragma once

#include EXT_HEADER_CONDITION_VARIABLE

#include <el/libext/ext-http.h>
#include <el/inet/async-text-client.h>
#include <el/inet/json-rpc.h>
using namespace Ext::Inet;

#include "bitcoin-common.h"

#if UCFG_BITCOIN_USE_CAL
#	include <el/comp/gpu-cal.h>
using namespace Ext::Gpu;
#endif


#include "bitcoin-sha256.h"

#include "util.h"
#include "miner-interface.h"

namespace Ext {
	namespace cl {
	class Device;
}}

namespace Coin {

class BitcoinMiner;

const float MAX_GPU_TEMPERATURE = 83; // Celsius
const float MAX_FPGA_TEMPERATURE = 60; // Celsius

const int BITCOIN_DEFAULT_GPU_IDLE_MILLISECONDS = 100;

const UInt32 FULL_TASK = UInt32(0x100000000ULL / UCFG_BITCOIN_NPAR);

const int MIN_GETWORK_QUEUE = 3,
			MAX_GETWORK_QUEUE = 5;

const int NORMAL_WAIT = 2000;

struct MinerTx {
	Blob Data;
	HashValue Hash;
};

class MinerBlock : public BlockBase {
	typedef BlockBase base;
public:
	VarValue JobId;
	mutable vector<MinerTx> Txes;
	String WorkId;
	Blob ConBaseAux;
	DateTime CurTime;
	TimeSpan ServerTimeOffset;
	DateTime MyExpireTime;
	Int64 CoinbaseValue;
	Blob Coinb1, Extranonce1, Extranonce2, CoinbaseAux, Coinb2;
	CCoinMerkleBranch MerkleBranch;

	byte TargetBE[32];

	MinerBlock()
		:	WorkId(nullptr)
		,	CoinbaseValue(-1)		
	{}

	static MinerBlock FromJson(const VarValue& json);
	static ptr<MinerBlock> FromStratumJson(const VarValue& json);
	Coin::HashValue MerkleRoot(bool bSave=true) const override;
protected:
	void SetTimestamps(const DateTime& dt) {
		ServerTimeOffset = (CurTime = dt) - DateTime::UtcNow();
	}
};

BinaryWriter& operator<<(BinaryWriter& wr, const MinerBlock& minerBlock);

class BitcoinWorkData : public Object {
	typedef BitcoinWorkData class_type;
public:
	typedef Interlocked interlocked_policy;

	ptr<Coin::MinerBlock> MinerBlock;

	DateTime Timestamp;
	Blob Midstate, Data, Hash1;
	byte TargetBE[32];
	UInt32 FirstNonce, LastNonce;
	UInt16 RollNTime;
	Coin::HashAlgo HashAlgo;

	BitcoinWorkData()
		:	FirstNonce(0)
		,	LastNonce(UInt32(-1))
		,	RollNTime(0)
	{}

	bool IsFull() const {
		return FirstNonce==0 && LastNonce == UInt32(-1);
	}

	virtual BitcoinWorkData *Clone() const { return new BitcoinWorkData(*this); }

	static ptr<BitcoinWorkData> FromJson(RCString sjson, Coin::HashAlgo algo);
	void SetRollNTime(WebClient& wc);

	UInt32 get_Version() const { return htobe(*(UInt32*)Data.constData()); }
	void put_Version(UInt32 v) { *(UInt32*)Data.data() = htobe(v); }
	DEFPROP(UInt32, Version);

	HashValue get_PrevBlockHash() const;
	void put_PrevBlockHash(const HashValue& v);
	DEFPROP(HashValue, PrevBlockHash);

	HashValue get_MerkleRoot() const;
	void put_MerkleRoot(const HashValue& v);
	DEFPROP(HashValue, MerkleRoot);

	DateTime get_BlockTimestamp() const { return DateTime::FromUnix(htobe(*(UInt32*)(Data.constData()+68))); }
	void put_BlockTimestamp(const DateTime& dt) { *(UInt32*)(Data.constData()+68) = betoh((UInt32)dt.UnixEpoch); }
	DEFPROP(DateTime, BlockTimestamp);

	UInt32 get_Bits() const { return htobe(*(UInt32*)(Data.constData()+72)); }
	void put_Bits(UInt32 v) { *(UInt32*)(Data.data()+72) = betoh(v); }
	DEFPROP(UInt32, Bits);

	UInt32 get_Nonce() const { return htobe(*(UInt32*)(Data.constData() + (HashAlgo==Coin::HashAlgo::Solid ? 84 : 76))); }
	void put_Nonce(UInt32 v) { *(UInt32*)(Data.data() + (HashAlgo==Coin::HashAlgo::Solid ? 84 : 76)) = betoh(v); }
	DEFPROP(UInt32, Nonce);

	bool TestNonceGivesZeroH(UInt32 nonce);

};

class ComputationDevice : public Object {
public:
	typedef Interlocked interlocked_policy;

	ptr<Object> Miner;
	String Name, Description;
	Ext::Temperature Temperature;
	volatile Int32 HwErrors;
	CBool IsAmdGpu;
//!!!	int Index;

	ComputationDevice()
		:	HwErrors(0)
	{}

	virtual void Start(BitcoinMiner& miner, CThreadRef *tr) =0;

	virtual Ext::Temperature GetTemperature() { return Temperature; }
};

class ComputationArchitecture {
public:
	static ComputationArchitecture *s_pChain;
	ComputationArchitecture *m_pNext;

	ComputationArchitecture()
		:	m_pNext(exchange(s_pChain, this))
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
	Int64 m_prevTsc;
	condition_variable CvComplete;
	UInt32 DevicePortion;
	ptr<ComputationDevice> Device;

	WorkerThreadBase(BitcoinMiner& miner, CThreadRef *tr)
		:	base(tr)
		,	Miner(miner)
		,	CurrentWebClient(0)
		,	m_nHashes(0)
		,	m_prevTsc(0)
		,	DevicePortion(128)
	{
		m_bSleeping = true;
	}

	virtual String GetDeviceName() { return nullptr; };
	void Stop() override;

	void CompleteRound(UInt64 nHashes);

	void StartRound() {
		m_dtStartRound = DateTime::UtcNow();
	}
protected:
	WebClient *CurrentWebClient;
	TimeSpan m_span;
	UInt64 m_nHashes;

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

class WorkerThread : public WorkerThreadBase {
	typedef WorkerThreadBase base;
public:
	WorkerThread(BitcoinMiner& miner, CThreadRef *tr)
		:	base(miner, tr)
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

	void MineSha256(BitcoinWorkData& wd);
	void MineScrypt(BitcoinWorkData& wd);
	void MinePrime(XptWorkData& wd);
	void MineSolid(BitcoinWorkData& wd);

	friend class BitcoinMiner;
};


class CpuDevice : public ComputationDevice {
public:
	void Start(BitcoinMiner& miner, CThreadRef *tr) override;
};

class GpuDevice : public ComputationDevice {
};

class AmdGpuDevice : public GpuDevice {
public:
	AmdGpuDevice() {
		IsAmdGpu = true;
	}

	void Start(BitcoinMiner& miner, CThreadRef *tr) override;
};

class GpuThreadBase : public WorkerThreadBase {
	typedef WorkerThreadBase base;
public:
	GpuThreadBase(BitcoinMiner& miner, CThreadRef *tr)
		:	base(miner, tr)
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
		:	Timeout(-1)
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
	int State;
	ptr<BitcoinWorkData> m_wd;

	StratumTask(int state)
		:	State(0)
	{}
};

class ConnectionClient {
public:
	BitcoinMiner& Miner;

	ConnectionClient(BitcoinMiner& miner)
		:	Miner(miner)
	{}

	virtual ~ConnectionClient() {}
	virtual void Submit(BitcoinWorkData *wd) =0;
	virtual void SetEpServer(const IPEndPoint& epServer) =0;
	virtual bool HasWorkData() =0;
	virtual ptr<BitcoinWorkData> GetWork() =0;
	virtual void OnPeriodic() {}
};

class StratumClient : public AsyncTextClient, public ConnectionClient {
	typedef AsyncTextClient base;
public:
	int State;
	Ext::Inet::JsonRpc JsonRpc;
	byte TargetBE[32];
	Blob Extranonce1, Extranonce2;

	mutex MtxData;
	ptr<Coin::MinerBlock> MinerBlock;

	StratumClient(Coin::BitcoinMiner& miner);
	void Call(RCString method, const vector<VarValue>& params, ptr<StratumTask> task = nullptr);
	void Submit(BitcoinWorkData *wd) override;
protected:
	void SetEpServer(const IPEndPoint& epServer) override { EpServer = epServer; }

	void SetDifficulty(double difficulty);
	void OnLine(RCString line) override;
	void Execute() override;
	ptr<BitcoinWorkData> GetWork() override;

	bool HasWorkData() override {
		return MinerBlock;
	}
};

class MINER_CLASS BitcoinMiner : public IMiner {
public:
	int GetworkPeriod;
	float Speed;		// float to be atomic
	Int64 EntireHashCount;
	float CPD; //!!! should be atomic
	DateTime DtStart;
	volatile UInt32 MaxHeight;

	mutex m_csGetWork;
	ManualResetEvent m_evDataReady;
	AutoResetEvent m_evGetWork;

	int GpuIdleMilliseconds;
	CBool m_bTryGpu, m_bTryFpga, m_bLongPolling;
	Coin::HashAlgo HashAlgo;

	byte BestHashBE[32];

	mutex MtxDevices;
	vector<ptr<ComputationDevice>> Devices;
	unordered_set<String> WorkingDevices, TestedDevices;

	ostream *m_pTraceStream;
	wostream *m_pWTraceStream;

	int m_msWait;
	int NPAR;
	
	volatile Int32 HashCount;
	volatile float ChainsExpectedCount;
	volatile Int32 SubmittedCount, AcceptedCount;

	CInt<int> Intensity;
	int Verbosity;
	String UserAgentString;

	CPointer<Coin::ConnectionClient> ConnectionClient;
	ptr<Thread> ConnectionClientThread;
	
	mutex m_csCurrentData;
	ptr<Coin::MinerBlock> MinerBlock;

	typedef list<ptr<BitcoinWorkData>> CTaskQueue;
	CTaskQueue TaskQueue;

	list<ptr<BitcoinWorkData>> WorksToSubmit;

	DateTime m_dtNextGetWork;

	String MainBitcoinUrl, Login, Password;
	DateTime m_dtSwichToMainUrl;
	String ProxyString;

	int ThreadCount;
	CPointer<CThreadRef> m_tr;
	vector<ptr<Thread> > Threads;
	ptr<Thread> GetWorkThread;
	ptr<Thread> GpuThread;
	int m_minGetworkQueue; 
	Temperature MaxGpuTemperature, MaxFpgaTemperature;

//!!!R	typedef LruMap<HashValue, Coin::MinerBlock> CLastBlockCache;
//!!!R	CLastBlockCache m_lastBlockCache;


	CBool m_bStop;
	CBool m_bInited;
	CBool Started;

	BitcoinMiner();
	virtual ~BitcoinMiner() {}

	void Release() override { delete this; }

	void InitDevices(vector<String>& selectedDevs);
	ptr<BitcoinWorkData> GetWorkForThread(WorkerThreadBase& wt, UInt32 portion, bool bAllHashAlgoAllowed);
	virtual BitcoinWebClient GetWebClient(WorkerThreadBase *wt);
//!!!	void SetNewData(const BitcoinWorkData& wd);
	void SetNewData(const BitcoinWorkData& wd, bool bClearOld = false);
	void SetWebInfo(WebClient& wc);
	void CheckLongPolling(WebClient& wc, RCString longPollUri = nullptr, RCString longPollId = nullptr);
	void CallNotifyRequest(WorkerThreadBase& wt, RCString url, RCString longPollId);	
	
	String GetMethodName(bool bSubmit);
	ptr<BitcoinWorkData> GetWorkFromMinerBlock(const DateTime& now, Coin::MinerBlock *minerBlock);
	virtual ptr<BitcoinWorkData> GetWork(WebClient*& curWebClient);
	virtual bool SubmitResult(WebClient*& curWebClient, const BitcoinWorkData& wd);
	
	void SetIntensity(int v) override {
		Intensity = v;
	}

	Blob CalcHash(const BitcoinWorkData& wd);
	bool TestAndSubmit(WorkerThreadBase& wt, BitcoinWorkData *wd, UInt32 nonce);
	virtual String GetCalIlCode(bool bEvergreen);
	virtual String GetOpenclCode();
	virtual String GetCudaCode();

	void Print(const BitcoinWorkData& wd, bool bSuccess, RCString message = "");

	bool UseSse2() {
#if UCFG_CPU_X86_X64
		return CpuInfo().HasSse2;
#endif
		return false;
	}

	void CreateConnectionClient(RCString s);

	void Start(CThreadRef *tr);
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
	CInt<int> m_getWorkMethodLevel;

	String BitcoinUrl;
	vector<SUrlTtr> HostList;
	CInt<int> HostIndex;

	String GetCurrentUrl();

	mutex m_csLongPollingThread;
	ptr<Thread> m_LongPollingThread;

	friend class WorkerThread;
	friend class LongPollingThread;
	friend class GetWorkThread;
};

class GpuMiner;

class GpuTask : public BitcoinSha256 {
public:
	ptr<BitcoinWorkData> WorkData;
	CPointer<WorkerThreadBase> m_pwt;
	GpuMiner& m_gpuMiner;
	int m_npar, m_nparOrig;
	CBool m_bIsVector2;

	GpuTask(GpuMiner& gpuMiner)
		:	m_gpuMiner(gpuMiner)
	{}

	virtual void Prepare(const BitcoinWorkData& wd, WorkerThreadBase *pwt) =0;
	virtual void Run() =0;
	virtual bool IsComplete() =0;
	virtual void Wait() =0;
	virtual int ParseResults() =0;
};

class GpuMiner : public Object {
public:
	BitcoinMiner& Miner;
	UInt32 DevicePortion;
	CBool m_bIsVector2;

	GpuMiner(BitcoinMiner& miner)
		:	Miner(miner)
		,	DevicePortion(1024*1024/UCFG_BITCOIN_NPAR)
	{}

//!!!	virtual void InitBeforeRun() {}
	virtual ptr<GpuTask> CreateTask() =0;
	virtual int Run(const BitcoinWorkData& wd, WorkerThreadBase *pwt) =0;

	virtual void SetDevicePortion() {
		DevicePortion = std::max(256, 1 << (20 + Miner.Intensity)) / UCFG_BITCOIN_NPAR;
	}
};

ptr<GpuMiner> CreateOpenclMiner(BitcoinMiner& miner, cl::Device& dev, const ConstBuf& binary);

#if UCFG_BITCOIN_USE_CAL
bool StartCal();
ptr<GpuMiner> CreateCalMiner(BitcoinMiner& miner, CALtarget calTarget, int idx);
String TargetToString(CALtarget target);

struct CalEngineWrap {
	CalEngine Engine;

	static mutex s_cs;
	static volatile Int32 RefCount;
	static CalEngineWrap *I;
	
	static CalEngineWrap *Get() {
		EXT_LOCK (s_cs) {
			if (!I)
				I = new CalEngineWrap;
			return I;
		}
	}

	static void AddRef(CalEngineWrap *p) {
		Interlocked::Increment(RefCount);
	}

	static void Release(CalEngineWrap *p) {
		EXT_LOCK (s_cs) {
			if (!Interlocked::Decrement(RefCount)) {
				delete I;
				I = nullptr;
			}
		}
	}
};


#endif // UCFG_BITCOIN_USE_CAL


} // Coin::

