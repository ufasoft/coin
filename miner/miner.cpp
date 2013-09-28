/*######     Copyright (c) 1997-2013 Ufasoft  http://ufasoft.com  mailto:support@ufasoft.com,  Sergey Pavlov  mailto:dev@ufasoft.com #######################################
#                                                                                                                                                                          #
# This program is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation;  #
# either version 3, or (at your option) any later version. This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the      #
# implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details. You should have received a copy of the GNU #
# General Public License along with this program; If not, see <http://www.gnu.org/licenses/>                                                                               #
##########################################################################################################################################################################*/

#include <el/ext.h>

#if UCFG_WIN32
#	include <el/libext/win32/ext-win.h>
#endif

#include "resource.h"

#include <el/libext/ext-http.h>
#include "coin-msg.h"

#include "miner.h"

#include "bitcoin-sha256sse.h"

#if UCFG_COIN_PRIME
#	include "xpt.h"
#endif

extern "C" {

HCoinMiner __cdecl CoinMiner_CreateObject() {
	return new Coin::BitcoinMiner;
}

void __cdecl CoinMiner_Release(HCoinMiner h) {
	h->Release();
}

void __cdecl CoinMiner_SetMainUrl(HCoinMiner h, const char *url) {
	h->SetMainUrl(url);
}

void __cdecl CoinMiner_SetLogin(HCoinMiner h, const char *login) {
	h->SetLogin(login);
}

void __cdecl CoinMiner_SetPassword(HCoinMiner h, const char *password) {
	h->SetPassword(password);
}

void __cdecl CoinMiner_SetThreadCount(HCoinMiner h, int n) {
	h->SetThreadCount(n);
}

void __cdecl CoinMiner_SetIntensity(HCoinMiner h, int n) {
	h->SetIntensity(n);
}

void __cdecl CoinMiner_SetHashAlgo(HCoinMiner h, int a) {
	h->SetHashAlgo(a);
}

void __cdecl CoinMiner_Start(HCoinMiner h) {
	h->Start();
}

void __cdecl CoinMiner_Stop(HCoinMiner h) {
	h->Stop();
}

double __cdecl CoinMiner_GetSpeed(HCoinMiner h) {
	return h->GetSpeed();
}

} // "C"


namespace Coin {



ComputationArchitecture *ComputationArchitecture::s_pChain;

class WebClientKeeper {
	WebClient*& m_cur;
	WebClient *m_prev;
public:
	WebClientKeeper(WebClient*& cur, WebClient& client)
		:	m_cur(cur)
		,	m_prev(cur)
	{
		cur = &client;
	}

	~WebClientKeeper() {
		m_cur = m_prev;
	}
};

void WorkerThreadBase::Stop() {
	base::Stop();
	Miner.m_evDataReady.Set();
}

void WorkerThreadBase::CompleteRound(UInt64 nHashes) {
	m_span += DateTime::UtcNow()-m_dtStartRound;
	m_nHashes += nHashes;

	Miner.OnRoundComplete();
}


MinerBlock MinerBlock::FromJson(const VarValue& json) {
	MinerBlock r;
	r.Ver = (UInt32)json["version"].ToInt64();
	r.Height = UInt32(json["height"].ToInt64());
	r.PrevBlockHash = HashValue(json["previousblockhash"].ToString());

	r.DifficultyTargetBits = Convert::ToUInt32(json["bits"].ToString(), 16);
	
	VarValue txes = json["transactions"];

	r.Txes.resize(txes.size()+1);

	r.SetTimestamps(DateTime::FromUnix(json["curtime"].ToInt64()));

	r.MyExpireTime = DateTime::UtcNow() + TimeSpan::FromSeconds(json.HasKey("expires") ? Int32(json["expires"].ToInt64()) : 600);

	{
		byte arPfx[11] = { 1, 0, 0, 0, 1, 9, 0x03, byte(r.Height), byte(r.Height >> 8), byte(r.Height >> 16), 4 };
		r.Coinb1 = Blob(arPfx, sizeof arPfx);
		r.Extranonce2 = Blob(0, 4);

		if (json.HasKey("coinbaseaux")) {
			MemoryStream ms;
			VarValue coinbaseAux = json["coinbaseaux"];
			vector<String> keys = coinbaseAux.Keys();
			EXT_FOR (RCString key, keys) {
				ms.WriteBuf(Blob::FromHexString(coinbaseAux[key].ToString()));
			}
			r.CoinbaseAux = ConstBuf(ms);
		}
	}

	for (int i=0; i<txes.size(); ++i) {
		MinerTx& mtx = r.Txes[i+1];
		VarValue jtx = txes[i];
		mtx.Data = Blob::FromHexString(jtx["data"].ToString());
		mtx.Hash = Hash(mtx.Data);

		if (jtx.HasKey("hash")) {
			HashValue hash = HashValue(jtx["hash"].ToString());
			if (hash != mtx.Hash)
				Throw(E_FAIL);
		}		
	}

	struct TxHasher {
		const Coin::MinerBlock *minerBlock;

		HashValue operator()(const MinerTx& mtx, int n) const {
			return &mtx == &minerBlock->Txes[0] ? Hash(mtx.Data) : mtx.Hash;
		}
	} txHasher =  { &r };
	r.MerkleBranch = BuildMerkleTree<Coin::HashValue>(r.Txes, txHasher, &HashValue::Combine).GetBranch(0);

	if (json.HasKey("workid"))
		r.WorkId = json["workid"].ToString();

	if (json.HasKey("coinbasevalue"))
		r.CoinbaseValue = json["coinbasevalue"].ToInt64();

	if (json.HasKey("coinbasetxn")) {
		VarValue jtx = json["coinbasetxn"];
		Blob blob = Blob::FromHexString(jtx["data"].ToString());
		if (blob.Size <= 36+4+1)
			Throw(E_FAIL);
		byte& lenScript = *(blob.data()+36+4+1);
		lenScript += 4;
		size_t cbFirst = 36+4+1+1+lenScript-4;
		
		r.Coinb1 = Blob(blob.data(), cbFirst);
		r.Extranonce1 = Blob(0, 0);
		r.Extranonce2 = Blob(0, 4);
		r.Coinb2 = Blob(blob.data()+cbFirst, blob.Size-cbFirst);
	}	

	BitsToTargetBE(Convert::ToUInt32(json["bits"].ToString(), 16), r.TargetBE);

	/*!!!R
	if (json.HasKey("target"))
		r.Target = Blob::FromHexString(json["target"].ToString());
	else {
		r.Target = Blob::FromHexString("00000000ffffffffffffffffffffffffffffffffffffffffffffffffffffffff"); //!!!TODO should be calculated from DifficultyTargetBits
	}*/

	return r;
}

HashValue MinerBlock::MerkleRoot(bool bSave) const {
	if (!m_merkleRoot) {
		if (Txes.empty())
			Txes.resize(1);
		Txes[0].Data = Coinb1+Extranonce1+Extranonce2+CoinbaseAux+Coinb2;
		m_merkleRoot = MerkleBranch.Apply(Hash(Txes[0].Data));
	}
	return m_merkleRoot.get();
}

BinaryWriter& operator<<(BinaryWriter& wr, const MinerBlock& block) {
	block.WriteHeader(wr);

	CoinSerialized::WriteVarInt(wr, block.Txes.size());
	EXT_FOR (const MinerTx& mtx, block.Txes) {
		wr.BaseStream.WriteBuf(mtx.Data);
	}
	return wr;
}

ptr<BitcoinWorkData> BitcoinWorkData::FromJson(RCString sjson, Coin::HashAlgo algo) {
	VarValue json = ParseJson(sjson);
	ptr<BitcoinWorkData> wd = new BitcoinWorkData;
	BitcoinWorkData& r = *wd;
	r.HashAlgo = algo;
	VarValue jres = json["result"];
	if (jres.HasKey("midstate"))
		r.Midstate = Blob::FromHexString(jres["midstate"].ToString());
	if (jres.HasKey("data"))
		r.Data = Blob::FromHexString(jres["data"].ToString());
	if (jres.HasKey("hash1"))
		r.Hash1 = Blob::FromHexString(jres["hash1"].ToString());
	if (jres.HasKey("target")) {
		Blob t = Blob::FromHexString(jres["target"].ToString());
		if (t.Size != 32)
			Throw(E_INVALIDARG);
		reverse_copy(t.begin(), t.end(), r.TargetBE);
	}
	if (jres.HasKey("target_share")) {
		Blob t = Blob::FromHexString(jres["target_share"].ToString().Substring(2));
		if (t.Size != 32)
			Throw(E_INVALIDARG);
		memcpy(r.TargetBE, t.constData(), 32);
	}
	if (jres.HasKey("noncerange")) {
		Blob blob = Blob::FromHexString(jres["noncerange"].ToString());
		if (blob.Size == 8) {
			r.FirstNonce = UInt32(ntohl(*(Int32*)blob.constData()));
			r.LastNonce = UInt32(ntohl(*(Int32*)(blob.constData()+4)));
		}
	}
	if (Coin::HashAlgo::Sha256 == r.HashAlgo) {
		if (0 == r.Hash1.Size) {
			static const byte s_hash1[64] = {
				0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
				0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
				0, 0, 0, 0x80, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
				0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0
			};
			r.Hash1 = Blob(s_hash1, sizeof s_hash1);
		}
		if (0 == r.Midstate.Size)
			r.Midstate = CalcSha256Midstate(r.Data);
		else {
			ASSERT(r.Midstate == CalcSha256Midstate(r.Data));
		}
	}
	return wd;
}

static const char s_sha256TestJson[] = "{\"id\": 0, \"result\": "
		"{\"data\": \"000000010fa597b30f766c011dfd84e881360a50aa927954157ee47f00006c2600000000e0defbcaf28d04805b78b783d1336eea0300186e81543b2ace02c6a9ac84f58b4d59b74d1b02855200000000000000800000000000000000000000000000000000000000000000000000000000000000000000000000000080020000\", "
		"\"target\": \"ffffffffffffffffffffffffffffffffffffffffffffffffffffffff00000000\", "
//valid too				"\"firstnonce\":\"30000000\", \"lastnonce\":\"3FFFFFFF\"}, \"error\": null}");
		"\"noncerange\":\"8D9CB6008D9DB600\"}, \"error\": null}";

static const char s_scryptTestJson[] = "{\"id\": 0, \"result\": "
		"{\"data\": \"000000019e6bc6041817f90455e2ab4371305834c8ef78a303e913f2676620f1d6698be0f90aff7fe3186d74be8a9d11f90850da5072609ebba667ee15e82482da5baf424f039c071d00e84800000000000000800000000000000000000000000000000000000000000000000000000000000000000000000000000080020000\", "
		"\"target\": \"ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff0000\", "
		"\"noncerange\":\"00001FB0000023AF\"}, \"error\": null}";

static const char s_solidTestJson[] = "{\"id\": 0, \"result\": "
		"{\"data\": \"010000004f569181ae30af2404edabc3d55ecb810337e85499e4baa44d336117a73a00002c95a645de51f693c60e03a4ba00155415edafae3b60204beb7dbf902e2654f3071f03000000000010843e4f00000000130700000000000002b8d40000000000000000000000000000000000636f696e6f74726f6e000000613b051d\", "
		"\"target_share\": \"0x00003fffffffffffffffffffffffffffffffffffffffffffffffffffffffffff\", "
		"\"noncerange\":\"0000000000003000\"}, \"error\": null}";

ptr<BitcoinWorkData> BitcoinMiner::GetTestData() {
	ptr<BitcoinWorkData> r;
	switch (HashAlgo) {
	case Coin::HashAlgo::Sha256:
		r = BitcoinWorkData::FromJson(s_sha256TestJson, HashAlgo);
		break;
	case Coin::HashAlgo::SCrypt:
		r = BitcoinWorkData::FromJson(s_scryptTestJson, HashAlgo);
		break;
	case Coin::HashAlgo::Solid:
		r = BitcoinWorkData::FromJson(s_solidTestJson, HashAlgo);
		break;
#if UCFG_COIN_PRIME
	case Coin::HashAlgo::Prime:
		r = GetPrimeTestData();
		break;
#endif
	default:
		Throw(E_FAIL);
	}
	return r;
}

void BitcoinWorkData::SetRollNTime(WebClient& wc) {
	String sRollNTime = wc.get_ResponseHeaders().Get("X-Roll-NTime");
	if (!!sRollNTime) {
		if (sRollNTime == "Y")
			RollNTime = 1;
		else if (atoi(sRollNTime))
			RollNTime = Convert::ToUInt16(sRollNTime);
	}
}

HashValue BitcoinWorkData::get_PrevBlockHash() const {
	UInt32 ar[8];
	for (int i=0; i<8; ++i)
		ar[i] = htobe(*(UInt32*)(Data.constData() + 4 + i*4));
	return HashValue(ConstBuf(ar, 32));
}

void BitcoinWorkData::put_PrevBlockHash(const HashValue& v) {
	const UInt32 *p = (const UInt32*)v.data();
	for (int i=0; i<8; ++i)
		*(UInt32*)(Data.constData() + 4 + i*4) = betoh(p[i]);
}

HashValue BitcoinWorkData::get_MerkleRoot() const {
	UInt32 ar[8];
	for (int i=0; i<8; ++i)
		ar[i] = htobe(*(UInt32*)(Data.constData() + 36 + i*4));
	return HashValue(ConstBuf(ar, 32));
}

void BitcoinWorkData::put_MerkleRoot(const HashValue& v) {
	const UInt32 *p = (const UInt32*)v.data();
	for (int i=0; i<8; ++i)
		*(UInt32*)(Data.constData() + 36 + i*4) = betoh(p[i]);
}

UInt32 BitcoinWorkData::get_Nonce() const {
	return *(const UInt32*)(Data.constData() + (HashAlgo==Coin::HashAlgo::Solid ? 84 : 64+12));
}

void BitcoinWorkData::put_Nonce(UInt32 v) {
	*(UInt32*)(Data.data() + (HashAlgo==Coin::HashAlgo::Solid ? 84 : 64+12)) = v;
}

bool BitcoinWorkData::TestNonceGivesZeroH(UInt32 nonce) {
	Nonce = nonce;
	Blob hash(0, 32);
	switch (HashAlgo) {
	case Coin::HashAlgo::Sha256:
		{
			BitcoinSha256 sha256;
			sha256.PrepareData(Midstate.constData(), Data.constData()+64, Hash1.constData());
			hash = sha256.FullCalc();
		}
		break;
	default:
		Throw(E_NOTIMPL);
	}
	return 0 == *(UInt32*)(hash.constData()+28);
}

void CpuDevice::Start(BitcoinMiner& miner, CThreadRef *tr) {
	for (int i=0; i<miner.ThreadCount; ++i) {
		ptr<WorkerThread> t = new WorkerThread(miner, tr);
		t->Device = this;
		miner.Threads.push_back(t.get());
		t->Start();
	}
}

BitcoinMiner::BitcoinMiner()
	:	HashCount(0)
	,	SubmittedCount(0)
	,	AcceptedCount(0)
	,	NPAR(UCFG_BITCOIN_NPAR)
	,	Verbosity(0)
	,	GetworkPeriod(15)
	,	Login(nullptr)
	,	Password(nullptr)
	,	ThreadCount(-1)
	,	MaxGpuTemperature(Temperature::FromCelsius(MAX_GPU_TEMPERATURE))
	,	MaxFpgaTemperature(Temperature::FromCelsius(MAX_FPGA_TEMPERATURE))
	,	GpuIdleMilliseconds(1)
	,	m_bTryGpu(true)
	,	m_bTryFpga(true)
	,	HashAlgo(Coin::HashAlgo::Sha256)
	,	m_bLongPolling(UCFG_BITCOIN_LONG_POLLING)
	,	m_dtSwichToMainUrl(DateTime::MaxValue)
	,	m_msWait(NORMAL_WAIT)
	,	m_minGetworkQueue(MIN_GETWORK_QUEUE)
	,	Speed(0)
	,	EntireHashCount(0)
//!!!R	,	m_lastBlockCache(20)
#if UCFG_BITCOIN_TRACE
	,	m_pTraceStream(&cerr)
	,	m_pWTraceStream(&wcerr)
#else
	,	m_pTraceStream(&GetNullStream())
	,	m_pWTraceStream(&GetWNullStream())
#endif
{
	memset(BestHashBE, 0xFF, sizeof BestHashBE);
	if (!ThreadCount)
		ThreadCount = 1;

	UserAgentString = String(MANUFACTURER);
#if UCFG_WIN32
	try {
		DBG_LOCAL_IGNORE_WIN32(ERROR_RESOURCE_TYPE_NOT_FOUND);	

		UserAgentString += " "+AfxGetCApp()->GetInternalName()+"/"+FileVersionInfo(System.ExeFilePath).ProductVersion;
	} catch (RCExc) {
	}
#else
	UserAgentString += " "+String(VER_INTERNALNAME_STR)+"/"+VER_PRODUCTVERSION_STR;
#endif
	UserAgentString += " (" + Environment.OSVersion.VersionString + ") ";
}

void BitcoinMiner::InitDevices(vector<String>& selectedDevs) {
	if (!m_bInited) {
		m_bInited = true;

		ptr<ComputationDevice> dev = new CpuDevice;
		dev->Name = "CPU";
#if UCFG_CPU_X86_X64
		dev->Description = String(CpuInfo().Name);	
#elif defined(_M_ARM)
		dev->Description = "ARM";	
#endif
		int nCore = Environment.ProcessorCoreCount;
		dev->Description += EXT_STR(", " << nCore << " core" << (nCore>1 ? "s" : ""));
		Devices.push_back(dev);

		for (ComputationArchitecture *p=ComputationArchitecture::s_pChain; p; p=p->m_pNext) {
			if (m_bTryGpu) {
				if (GpuArchitecture *arch = dynamic_cast<GpuArchitecture*>(p))
					arch->FillDevices(_self);
			}
			if (m_bTryFpga) {
				if (FpgaArchitecture *arch = dynamic_cast<FpgaArchitecture*>(p))
					arch->FillDevices(_self);
			}
		}

		if (Devices.size() > 1 && ThreadCount < 0 && selectedDevs.empty())					// don't use CPU if other devices are present
			ThreadCount = 0;
		if (ThreadCount < 0)
			ThreadCount = thread::hardware_concurrency();
	}
}

BitcoinWebClient BitcoinMiner::GetWebClient(WorkerThreadBase *wt) {
	BitcoinWebClient wc;
	wc.CacheLevel = RequestCacheLevel::BypassCache;
	wc.Credentials.UserName = Login;
	wc.Credentials.Password = Password;

	if (!ProxyString.IsEmpty())
		wc.Proxy = WebProxy::FromString(ProxyString);

	if (!UserAgentString.IsEmpty()) {
		wc.UserAgent = UserAgentString;
#if UCFG_BITCOIN_USERAGENT_INFO
		if (wt) {
			String devName = wt->GetDeviceName();
			if (!!devName) {
				static int s_cpus = Environment.ProcessorCount,
						   s_cores = Environment.ProcessorCoreCount;

				wc.UserAgent += " " + devName + " PC="+Convert::ToString(s_cpus)
										 + " CC="+Convert::ToString(s_cores);
				double sec = wt->m_span.TotalSeconds;
				if (sec > 1) {
					ostringstream os;
					os << " P=" << setprecision(3) << wt->m_nHashes/sec/1000000;
					wt->m_nHashes = 0;
					wt->m_span = TimeSpan();
					wc.UserAgent += os.str();
				}
				Int64 tsc = __rdtsc(),
					diff = tsc - exchange(wt->m_prevTsc, tsc);
				if (diff < 0 )
					wc.UserAgent += " TSCDiff="+Convert::ToString(diff);
			}
		}
#endif
	}
	wc.Headers.Set("X-Mining-Extensions", "hostlist longpoll midstate noncerange rollntime switchto");
	return wc;
}

class LongPollingThread : public WorkerThreadBase {
	typedef WorkerThreadBase base;
public:
	String LongPollingUrl;
	String LongPollId;

	LongPollingThread(BitcoinMiner& miner)
		:	base(miner, miner.m_tr)
	{}
protected:
	void Execute() override;
};

String BitcoinMiner::GetCurrentUrl() {
	String r;
	EXT_LOCK (m_csCurrentData) {

		r = BitcoinUrl;
	}
	return r;
}

static regex s_reBitcoinUrl("^(http://)?[^/]+(/)?");

void BitcoinMiner::CheckLongPolling(WebClient& wc, RCString longPollUri, RCString longPollId) {
	EXT_LOCK (m_csLongPollingThread) {
		if (m_bLongPolling) {
			String longPollingPath = longPollUri != nullptr ? longPollUri : wc.get_ResponseHeaders().Get("X-Long-Polling");
			if (!!longPollingPath) {
				if (m_LongPollingThread)
					return;
				String url = GetCurrentUrl();
				cmatch m;
				if (regex_search(url.c_str(), m, s_reBitcoinUrl)) {
					LongPollingThread *t = new LongPollingThread(_self);
					t->LongPollId = longPollId;
					ssize_t idx = -1;
					if (m[2].matched)
						idx = m.position(2);
					if (longPollingPath.Left(7) == "http://")
						t->LongPollingUrl = longPollingPath;
					else
						t->LongPollingUrl = (idx==-1 ? url : url.Left(idx)) + longPollingPath;
					m_LongPollingThread = t;
					t->Start();
				}
			} else if (m_LongPollingThread) {
				m_LongPollingThread->Stop();
				m_LongPollingThread = nullptr;
			}
		}
	}
}

static regex s_reUrlTtr("\"host\":\"([^\"]+)\",\"port\":(\\d+),\"ttr\":(\\d+)");

vector<SUrlTtr> GetUrlTtrs(RCString s) {
	vector<SUrlTtr> r;
	const char *p = s.c_str();
	for (cregex_iterator it(p, p+strlen(p), s_reUrlTtr), e; it!=e; ++it) {
		SUrlTtr urlTtr = { "http://"+String((*it)[1])+":"+String((*it)[2]) , Convert::ToInt32(String((*it)[3])) };
		r.push_back(urlTtr);
	}
	return r;
}


/*!!!R?
void BitcoinMiner::SetNewData(const BitcoinWorkData& wd) {
	if (wd.Midstate.Size != 32 || wd.Data.Size != 128 || wd.Target.Size != 32 || wd.Hash1.Size != 64)
		Throw(E_FAIL);

	DateTime now = DateTime::UtcNow();
	EXT_LOCK (m_csCurrentData) {
		TaskQueue.push_back(wd);
		m_dtNextGetWork = now + TimeSpan::FromSeconds(m_LongPollingThread ? 60 : GetworkPeriod);
		m_evDataReady.Set();
	}
}
*/

void BitcoinMiner::SetNewData(const BitcoinWorkData& wd, bool bClearOld) {
	if (wd.Data.Size != 128 ||
		wd.HashAlgo == Coin::HashAlgo::Sha256 && (wd.Midstate.Size != 32 || wd.Hash1.Size != 64))
		Throw(E_FAIL);

	DateTime now = DateTime::UtcNow();
	EXT_LOCK (m_csCurrentData) {
		if (bClearOld)
			TaskQueue.clear();
		else {
			DateTime dtObsolete = wd.Timestamp-TimeSpan::FromSeconds(60);
			while (TaskQueue.size() >= MAX_GETWORK_QUEUE || !TaskQueue.empty() && TaskQueue.front()->Timestamp < dtObsolete)
				TaskQueue.pop_front();
		}

		TaskQueue.push_back((BitcoinWorkData*)&wd);
		for (int i=1; i<=wd.RollNTime; ++i) {
			ptr<BitcoinWorkData> wd1 = wd.Clone();
			*reinterpret_cast<Int32*>(wd1->Data.data()+68) += htobe32(i);
			TaskQueue.push_back(wd1);
		}
		m_dtNextGetWork = now + TimeSpan::FromSeconds(m_LongPollingThread ? 60 : GetworkPeriod);
#ifdef X_DEBUG//!!!D
		m_dtNextGetWork = now+TimeSpan::FromSeconds(1000);
		this->m_minGetworkQueue = 1;
#endif
		m_evDataReady.Set();
		
	}
}

void BitcoinMiner::SetWebInfo(WebClient& wc) {
	DateTime now = DateTime::UtcNow();
	EXT_LOCK (m_csCurrentData) {
		if (now > m_dtSwichToMainUrl) {
			BitcoinUrl = MainBitcoinUrl;
			m_dtSwichToMainUrl = DateTime::MaxValue;
			*m_pTraceStream << "\rSwitching back to " << BitcoinUrl  << endl;
		}

		String switchTo = wc.get_ResponseHeaders().Get("X-Switch-To");
		if (!!switchTo) {
			vector<SUrlTtr> urlTtrs = GetUrlTtrs(switchTo);
			if (!urlTtrs.empty()) {
				BitcoinUrl = urlTtrs[0].Url;
				m_dtSwichToMainUrl = now + TimeSpan::FromSeconds(urlTtrs[0].TtrMinutes*60);
				*m_pTraceStream << "\rSwitching to " << BitcoinUrl  << endl;
			}
		}

		String hostList = wc.get_ResponseHeaders().Get("X-Host-List");
		if (!!hostList) {
			vector<SUrlTtr> urlTtrs = GetUrlTtrs(hostList);
			if (!urlTtrs.empty() && urlTtrs != HostList) {
				HostList = urlTtrs;
				HostIndex = 0;
				if (BitcoinUrl != HostList[0].Url) {
					*m_pTraceStream << "\rSwitching to " << HostList[0].Url  << endl;
				}
				MainBitcoinUrl = BitcoinUrl = HostList[0].Url;
			}
		}
	}
}

void BitcoinMiner::CallNotifyRequest(WorkerThreadBase& wt, RCString url, RCString longPollId) {
	DBG_LOCAL_IGNORE_WIN32(ERROR_INTERNET_TIMEOUT);	
	DBG_LOCAL_IGNORE_WIN32(ERROR_INTERNET_OPERATION_CANCELLED);	

	BitcoinWebClient wc = GetWebClient(&wt);
	wc.Timeout = 10*60*1000;
	WebClientKeeper keeper(wt.CurrentWebClient, wc);

	String s = !!longPollId ? wc.UploadString(url, "{\"method\": \"getblocktemplate\", \"params\": [{\"capabilities\": [\"coinbasetxn\", \"workid\", \"coinbase/append\", \"longpollid\"], \"longpollid\": \"" + longPollId + "\"}], \"id\":0}")
							: wc.DownloadString(url);
	TRC(0, "Long Polling returned");
	EXT_LOCK (m_csLongPollingThread) {
		m_LongPollingThread = nullptr;
	}
	if (2 == m_getWorkMethodLevel) {
		VarValue json = ParseJson(s);
		json = json["result"];
		EXT_LOCK (m_csCurrentData) {
			MinerBlock = new Coin::MinerBlock(MinerBlock::FromJson(json));
		}
		if (json.HasKey("longpollid")) {
			String longpollid = json["longpollid"].ToString();
			String uri = nullptr;
			if (json.HasKey("longpolluri"))
				uri = json["longpolluri"].ToString();
			else if (json.HasKey("longpoll"))
				uri = json["longpoll"].ToString();
			CheckLongPolling(wc, uri, longpollid);  // should be before SetNewData() because SetNewData() uses m_LongPollingThread
		}
	} else if (ptr<BitcoinWorkData> wd = BitcoinWorkData::FromJson(s, HashAlgo)) {
		wd->SetRollNTime(wc);
		SetWebInfo(wc);
		CheckLongPolling(wc);  // should be before SetNewData() because SetNewData() uses m_LongPollingThread
		wd->Timestamp = DateTime::UtcNow();
		SetNewData(*wd, true);
	}
}

void LongPollingThread::Execute() {
	Name = "LongPollingThread";

	try {
		Miner.CallNotifyRequest(_self, LongPollingUrl, LongPollId);
	} catch (RCExc e) {
		HRESULT hr = e.HResult;
		TRC(0, "Long Polling TIME-OUT");

		EXT_LOCK (Miner.m_csLongPollingThread) {
			Miner.m_LongPollingThread = nullptr;
		}
	}
}

ptr<BitcoinWorkData> BitcoinMiner::GetWorkForThread(WorkerThreadBase& wt, UInt32 portion, bool bAllHashAlgoAllowed) {
	ptr<BitcoinWorkData> wd;
	while (!m_bStop) {
		bool bGetWork = false;
		EXT_LOCK (m_csCurrentData) {
			for (CTaskQueue::iterator it=TaskQueue.begin(), e=TaskQueue.end(); it!=e; ++it) {
				BitcoinWorkData *w = *it;
				if (!bAllHashAlgoAllowed && w->HashAlgo!=Coin::HashAlgo::Sha256)		// Stop GPU threads because they can't process Scrypt
					return wd;
				UInt32 por = portion;
				if (w->HashAlgo == Coin::HashAlgo::Prime) {
					wd = w;
					TaskQueue.erase(it);
					break;
				}
				if (w->HashAlgo == Coin::HashAlgo::Solid) {
					if (por >> 10)
						por >>= 10;
				}				
				Int64 req = Int64(por) * UCFG_BITCOIN_NPAR,
					  avail = Int64(w->LastNonce)-Int64(w->FirstNonce)+1;
				if (por != FULL_TASK) {
					Int64 delta = std::min(req, avail);
					wd = w->Clone();
					wd->LastNonce = UInt32(wd->FirstNonce+delta-1);
					w->FirstNonce = wd->LastNonce+1;
					if (delta == avail)
						TaskQueue.erase(it);
					break;
				} else if (avail >= req) {
					wd = w;
					TaskQueue.erase(it);
					break;
				}
			}
			bGetWork = TaskQueue.size() < m_minGetworkQueue;
		}
		if (bGetWork)
			m_evGetWork.Set();
		if (wd)
			break;
		m_evDataReady.Reset();
		m_evDataReady.Lock(1000);
	}
	
/*!!!	UInt32 firstNonce = wd.FirstNonce+next*BitcoinMiner::NONCE_STEP,
			lastNonce = UInt32(firstNonce + Int64(BitcoinMiner::NONCE_STEP)*BitcoinSha256::NPAR/BitcoinSha256::NPAR - 1);    //  BitcoinSha256::NPAR can be non power of 2
	wd.FirstNonce = firstNonce;
	wd.LastNonce = lastNonce; */
	return wd;
}

String BitcoinMiner::GetMethodName(bool bSubmit) {
	switch ((int)m_getWorkMethodLevel) {
	case 0:
	case 2:
		return "getblocktemplate";
	}
	if (HashAlgo == Coin::HashAlgo::Solid)
		return bSubmit ? "sc_testwork" : "sc_getwork";
	else
		return "getwork";
}

ptr<BitcoinWorkData> BitcoinMiner::GetWorkFromMinerBlock(const DateTime& now, Coin::MinerBlock *minerBlock) {
#ifdef X_DEBUG//!!!D
	return GetTestData();
#endif

	size_t cbN2 = minerBlock->Extranonce2.Size;
	byte *pN2 = minerBlock->Extranonce2.data();
	if (cbN2 == 1)
		++*pN2;
	else if (cbN2 < 4)
		++*(UInt16*)pN2;
	else if (cbN2 < 8)
		++*(UInt32*)pN2;
	else
		++*(UInt64*)pN2;

	minerBlock->m_merkleRoot.reset();
	minerBlock->Timestamp = now + minerBlock->ServerTimeOffset;

	ptr<BitcoinWorkData> wd = new BitcoinWorkData;

	MemoryStream stm;
	minerBlock->WriteHeader(BinaryWriter(stm).Ref());
	Blob blob = stm;
	size_t len = blob.Size;
	blob.Size = 128;
	FormatHashBlocks(blob.data(), len);

	wd->Hash1 = Blob(0, 64);
	FormatHashBlocks(wd->Hash1.data(), 32);

	wd->Timestamp = now;
	wd->Data = blob;
	wd->Midstate = CalcSha256Midstate(wd->Data);
	wd->HashAlgo = HashAlgo;
	memcpy(wd->TargetBE, minerBlock->TargetBE, sizeof wd->TargetBE);

	HashValue merkle = HashValue(ConstBuf(wd->Data.constData()+36, 32));
//!!!R	m_lastBlockCache.insert(make_pair(merkle, *MinerBlock));
	wd->MinerBlock = new Coin::MinerBlock(*minerBlock);
	return wd;
}

ptr<BitcoinWorkData> BitcoinMiner::GetWork(WebClient*& curWebClient) {
	if (ConnectionClient)
		return ConnectionClient->GetWork();

	DateTime now = DateTime::UtcNow();
	String url;
	EXT_LOCK (m_csCurrentData) {
		if (MinerBlock) {
			if (now < MinerBlock->MyExpireTime)
				return GetWorkFromMinerBlock(now, MinerBlock);
			MinerBlock = nullptr;
		}
		url = BitcoinUrl;
	}

	ptr<BitcoinWorkData> pwd = new BitcoinWorkData;
LAB_FALLBACK_GETWORK:
	BitcoinWebClient wc = GetWebClient(nullptr);
	WebClientKeeper keeper(curWebClient, wc);
	String method = GetMethodName("false");
	try {
		pwd->Timestamp = now;

		if (method == "getblocktemplate") {
			String sjson = wc.UploadString(url, "{\"method\": \"" + method + "\", \"params\": [{\"capabilities\": [\"coinbasetxn\", \"workid\", \"coinbase/append\", \"longpollid\"]}], \"id\":0}");
			
			String xStratum = wc.get_ResponseHeaders().Get("X-Stratum");
			if (xStratum != nullptr) {
				*m_pTraceStream << "\rSwitching to " << xStratum  << endl;
				CreateConnectionClient(xStratum);
				return pwd;
			}

			VarValue json = ParseJson(sjson);
			json = json["result"];
			if (!json.HasKey("version")) {
				m_getWorkMethodLevel = 1;
				goto LAB_FALLBACK_GETWORK;
			}
			ptr<Coin::MinerBlock> newMinerBlock = new Coin::MinerBlock(MinerBlock::FromJson(json));
			EXT_LOCK (m_csCurrentData) {
				MinerBlock = newMinerBlock;
			}
			m_getWorkMethodLevel = 2;
			SetWebInfo(wc);

			if (json.HasKey("longpollid")) {
				String longpollid = json["longpollid"].ToString();
				String uri = nullptr;
				if (json.HasKey("longpolluri"))
					uri = json["longpolluri"].ToString();
				else if (json.HasKey("longpoll"))
					uri = json["longpoll"].ToString();
				CheckLongPolling(wc, uri, longpollid);  // should be before SetNewData() because SetNewData() uses m_LongPollingThread
			}

			return GetWorkFromMinerBlock(now, MinerBlock);
		} else if (pwd = BitcoinWorkData::FromJson(wc.UploadString(url, "{\"method\": \"" + method + "\", \"params\": [], \"id\":0}"), HashAlgo)) {
			pwd->SetRollNTime(wc);
			SetWebInfo(wc);
			m_msWait = NORMAL_WAIT;
		}
		CheckLongPolling(wc);  // should be before SetNewData() because SetNewData() uses m_LongPollingThread
		return pwd;
	} catch (RCExc ex) {
		*m_pTraceStream << "\r" << ex.Message << endl;
		if (m_getWorkMethodLevel == 0) {
			m_getWorkMethodLevel = 1;
			goto LAB_FALLBACK_GETWORK;
		}
		EXT_LOCK (m_csCurrentData) {
			if (HostIndex+1 < HostList.size()) {
				SUrlTtr& urlTtr = HostList[++HostIndex];
				BitcoinUrl = urlTtr.Url;
				m_dtSwichToMainUrl = urlTtr.TtrMinutes ? DateTime::UtcNow()+TimeSpan::FromSeconds(urlTtr.TtrMinutes*60) : DateTime::MaxValue;
				*m_pTraceStream << "\rSwitching to " << BitcoinUrl  << endl;
				return pwd;
			} else 
				HostIndex = 0;
		}
	}
	m_msWait = std::min(100000, m_msWait*2);
	return pwd;
}

static regex s_reTrueResult("\"(share_valid|result)\"\\s*:\\s*true");

bool BitcoinMiner::SubmitResult(WebClient*& curWebClient, const BitcoinWorkData& wd) {
	ostringstream sdata;
	
	String arg;
	if (m_getWorkMethodLevel == 2) {
		HashValue merkle = HashValue(ConstBuf(wd.Data.constData()+36, 32));
		if (!wd.MinerBlock)
			return false;
		Coin::MinerBlock& mblock = *wd.MinerBlock;
		/*!!!R
		CLastBlockCache::iterator it = m_lastBlockCache.find(merkle);
		if (it == m_lastBlockCache.end())
			return false;
		Coin::MinerBlock& mblock = it->second.first; */
		mblock.Nonce = _byteswap_ulong(wd.Nonce);
		sdata << EXT_BIN(mblock);
		String s = String(sdata.str());
		arg = "{\"method\":\"submitblock\",\"params\":[\"" + s + "\"";
		if (mblock.WorkId != nullptr)
			arg += ", \"workid\":\""+mblock.WorkId+"\"";			
		arg += "],\"id\":1}";		
	} else {
		sdata << wd.Data;
		if (wd.HashAlgo == Coin::HashAlgo::Solid)
			arg = "{\"method\":\"sc_testwork\",\"params\":[\"" + String(sdata.str()).ToLower() + "\"],\"id\":1}";
		else
			arg = "{\"params\":[\"" + String(sdata.str()) + "\"],\"method\":\"getwork\",\"id\":\"jsonrpc\"}";
	}
	BitcoinWebClient wc = GetWebClient(nullptr);
	WebClientKeeper keeper(curWebClient, wc);
	String r = wc.UploadString(GetCurrentUrl(), arg);
	VarValue json = ParseJson(r),
		jresult = json["result"];
	switch (jresult.type()) {
	case VarType::Null:
		return true;
	case VarType::String:
		ThrowRejectionError(jresult.ToString());
	}
	return regex_search(r.c_str(), s_reTrueResult);
}

Blob BitcoinMiner::CalcHash(const BitcoinWorkData& wd) {
	Blob hash(0, 32);
	switch (HashAlgo) {
	case Coin::HashAlgo::Sha256:
		{
			BitcoinSha256 sha256;
			sha256.PrepareData(wd.Midstate.constData(), wd.Data.constData()+64, wd.Hash1.constData());
			hash = sha256.FullCalc();
		}
		break;
	case Coin::HashAlgo::SCrypt:
		{
			UInt32 data[20];
			for (int i=0; i<_countof(data); ++i)
				data[i] = _byteswap_ulong(((UInt32*)wd.Data.data())[i]);
			array<UInt32, 8> res = CalcSCryptHash(ConstBuf(data, sizeof data));
			memcpy(hash.data(), &res[0], hash.Size);
		}
		break;
#if UCFG_BITCOIN_SOLIDCOIN
	case Coin::HashAlgo::Solid:
		{
			HashValue res = SolidcoinHash(wd.Data);
			memcpy(hash.data(), &res[0], hash.Size);
		}
		break;
#endif
	default:
		Throw(E_FAIL);
	}
	return hash;
}

bool BitcoinMiner::TestAndSubmit(WorkerThreadBase& wt, BitcoinWorkData *wd, UInt32 nonce) {
	wd->Nonce = nonce;	
	Blob hash = CalcHash(*wd);
	std::reverse(hash.data(), hash.data()+32);

	TRC(3, "Pre-result Nonce Found: " << hex << nonce << ", hash: " << hash);

	if (memcmp(hash.constData(), wd->TargetBE, 32) < 0) {
		EXT_LOCK (m_csCurrentData) {
			WorksToSubmit.push_back(wd);
		}
		m_evGetWork.Set();

		if (memcmp(hash.constData(), BestHashBE, sizeof BestHashBE) < 0) {
			memcpy(BestHashBE, hash.constData(), sizeof BestHashBE);
			*m_pTraceStream  << "Best Hash: " << hash << endl;
		}
		return true;
	} else {
//		TRC(2, "Hash above than Target: " << hash);
		return false;
	}
}

String  BitcoinMiner::GetCalIlCode(bool bEvergreen) {
#if UCFG_BITCOIN_USE_RESOURCE
	CResID resId(bEvergreen ? ID_CAL_IL_EVERGREEN : ID_CAL_IL);
	Resource res(resId, RT_RCDATA);
	return Encoding::UTF8.GetChars(res);
#else
	return File::ReadAllText(Path::Combine(Path::GetDirectoryName(System.ExeFilePath), bEvergreen ? ID_CAL_IL_EVERGREEN : ID_CAL_IL));
#endif
}

String BitcoinMiner::GetOpenclCode() {
	String sResId;
	switch (HashAlgo) {
	case Coin::HashAlgo::Sha256:
		sResId = ID_OPENCL;
		break;
	case Coin::HashAlgo::SCrypt:
		sResId = ID_OPENCL_SCRYPT;
		break;
	default:
		Throw(E_NOTIMPL);
	}
#if UCFG_BITCOIN_USE_RESOURCE
	CResID resId(sResId.c_str());
	Resource res(resId, RT_RCDATA);
	return Encoding::UTF8.GetChars(res);
#else
	return File::ReadAllText(Path::Combine(Path::GetDirectoryName(System.ExeFilePath), sResId));
#endif
}

String BitcoinMiner::GetCudaCode() {
#if UCFG_BITCOIN_USE_RESOURCE
	CResID resId(ID_CUDA_PTX);
	Resource res(resId, RT_RCDATA);
	return Encoding::UTF8.GetChars(res);
#else
	return File::ReadAllText(Path::Combine(Path::GetDirectoryName(System.ExeFilePath), ID_CUDA_PTX));
#endif
}

void BitcoinMiner::Print(const BitcoinWorkData& wd, bool bSuccess, RCString message) {
	Blob hash = CalcHash(wd);
	std::reverse(hash.data(), hash.data()+32);

	ostringstream os;
	os << hash;
	String shash = os.str();
	if (!Verbosity)
		shash = shash.Substring(56, 8);
	*m_pTraceStream  << (isatty(fileno(stdout)) ? "\r" : "") << DateTime::Now() << " Result: " << shash << (bSuccess ? " accepted" : (" rejected "+(!message.IsEmpty() ? message : String()))) << "                " << endl;
}

class GetWorkThread : public Thread {
	typedef Thread base;
public:
	BitcoinMiner& Miner;

	GetWorkThread(BitcoinMiner& miner, CThreadRef *tr)
		:	base(tr)
		,	Miner(miner)
		,	CurrentWebClient(0)
	{
		m_bSleeping = true;
		StackSize = 128*1024;
	}

protected:
	WebClient *CurrentWebClient;

#if UCFG_WIN32
	void OnAPC() override {
		base::OnAPC();
		if (CurrentWebClient)
			CurrentWebClient->CurrentRequest->ReleaseFromAPC();
	}
#endif

	void Stop() override {
		base::Stop();
		Miner.m_evDataReady.Set();
		Miner.m_evGetWork.Set();
	}

	void Execute() override {
		Name = "GetWorkThread";

#if UCFG_COM
		CUsingCOM usingCOM;
#endif
		DBG_LOCAL_IGNORE_WIN32(ERROR_INTERNET_TIMEOUT);
		DBG_LOCAL_IGNORE_WIN32(ERROR_HTTP_INVALID_SERVER_RESPONSE);
				
		deque<pair<DateTime, Int64>> deq;
		for (DateTime dt=Miner.DtStart; !m_bStop; ) {
			if (Miner.ConnectionClient) {
				bool bWait = !Miner.ConnectionClient->HasWorkData();
				/*!!!
				EXT_LOCK (Miner.m_csCurrentData) {
					bWait = !Miner.MinerBlock;
				}*/
				if (bWait)
					goto LAB_WAIT;
			}

			if (EXT_LOCKED(Miner.m_csCurrentData, dt > Miner.m_dtNextGetWork || Miner.TaskQueue.size() < Miner.m_minGetworkQueue)) {
				if (ptr<BitcoinWorkData> wd = Miner.GetWork(CurrentWebClient)) {
					wd->Timestamp = dt = DateTime::UtcNow();
					Miner.SetNewData(*wd);
					continue;
				}
			}	

LAB_WAIT:
			Miner.m_evGetWork.Lock(Miner.m_msWait);

			while (true) {
				ptr<BitcoinWorkData> wd;
				EXT_LOCK (Miner.m_csCurrentData) {
					if (Miner.WorksToSubmit.empty())
						break;
					wd = Miner.WorksToSubmit.front();
					Miner.WorksToSubmit.pop_front();
				}
				try {
					if (Miner.ConnectionClient) {
						Miner.ConnectionClient->Submit(*wd);
					} else {
						bool r = Miner.SubmitResult(CurrentWebClient, *wd);
						if (r)
							Interlocked::Increment(Miner.AcceptedCount);
						Miner.Print(*wd, r, CurrentWebClient ? CurrentWebClient->get_ResponseHeaders().Get("X-Reject-Reason") : String(nullptr));
					}
				} catch (RCExc ex) {
					*Miner.m_pWTraceStream << "\r" << ex.Message << endl;
				}
			}

			Int64 nProcessed = Int64(Interlocked::Exchange(Miner.HashCount, 0));
			if (Miner.HashAlgo != HashAlgo::Prime)
				nProcessed *= UCFG_BITCOIN_NPAR;
			Miner.EntireHashCount += nProcessed;
			deq.push_back(make_pair(dt, nProcessed));
			double sec = ((dt = DateTime::UtcNow())-deq.front().first).TotalSeconds;
			if (sec > 1) {
				Int64 sum = 0;
				for (auto i=deq.begin(); i!=deq.end(); ++i)
					sum += i->second;
				Miner.Speed = float(sum/sec);
			}
			if (deq.size() > 20)
				deq.pop_front();

			for (ComputationArchitecture *p=ComputationArchitecture::s_pChain; p; p=p->m_pNext) {
				if (Miner.m_bTryGpu) {
					if (GpuArchitecture *arch = dynamic_cast<GpuArchitecture*>(p))
						arch->DetectNewDevices(Miner);
				}
				if (Miner.m_bTryFpga) {
					if (FpgaArchitecture *arch = dynamic_cast<FpgaArchitecture*>(p))
						arch->DetectNewDevices(Miner);
				}
			}
			if (Miner.ConnectionClient) {
				try {
					Miner.ConnectionClient->OnPeriodic();
				} catch (RCExc ex) {
					*Miner.m_pWTraceStream << "\r" << ex.Message << endl;
				}
			}
		}
	}
};

void BitcoinMiner::CreateConnectionClient(RCString s) {
	Uri uri(s);
	if (uri.Scheme == "stratum+tcp") {
		StratumClient *c = new StratumClient(_self);
		ConnectionClient = c;
		ConnectionClientThread = c;
#if UCFG_COIN_PRIME
	} else if (uri.Scheme == "xpt+tcp") {
		HashAlgo = Coin::HashAlgo::Prime;
		XptClient *c = new XptClient(_self);
		ConnectionClient = c;
		ConnectionClientThread = c;
#endif
	} else
		Throw(E_INVALIDARG);
	ConnectionClient->SetEpServer(IPEndPoint(uri.Host, (UInt16)uri.Port));
	ConnectionClientThread->Start();
}

void BitcoinMiner::Start(CThreadRef *tr) {
	AFX_MANAGE_STATE(AfxGetStaticModuleState());

	if (Started)
		return;

	TRC(2, "");

	if (!MainBitcoinUrl.IsEmpty()) {
		Uri uri(MainBitcoinUrl);
		if (Login.IsEmpty())
			Login = uri.UserName;
		if (Password.IsEmpty())
			Password = uri.Password;
		BitcoinUrl = MainBitcoinUrl;
		if (uri.Scheme == "stratum+tcp" || uri.Scheme == "xpt+tcp")
			CreateConnectionClient(MainBitcoinUrl);
	}
	m_tr = tr;
	vector<String> selectedDevs;
	InitDevices(selectedDevs);
	if (!Threads.empty())
		return;
	DtStart = DateTime::UtcNow();
	m_evDataReady.Reset();
	m_evGetWork.Set();
	(GetWorkThread = new Coin::GetWorkThread(_self, tr))->Start();
	for (int i=0; i<Devices.size(); ++i)
		Devices[i]->Start(_self, tr);
	Started = true;
}

void BitcoinMiner::Stop() {
	if (!Started)
		return;
	TRC(2, "");
	m_bStop = true;
	for (int i=0; i<Threads.size(); ++i)
		Threads[i]->Stop();
	ptr<Thread> longPollingThread = EXT_LOCKED(m_csLongPollingThread, m_LongPollingThread);
	if (longPollingThread)
		longPollingThread->Stop();
	if (ConnectionClientThread)
		ConnectionClientThread->Stop();
	if (GetWorkThread)
		GetWorkThread->Stop();
	for (int i=0; i<Threads.size(); ++i)
		Threads[i]->Join();
	if (GetWorkThread)
		GetWorkThread->Join();
	Threads.clear();
	GetWorkThread = nullptr;
	GpuThread = nullptr;
#if !UCFG_USE_LIBCURL
	if (longPollingThread)
		longPollingThread->Join();
#endif
	if (ConnectionClient) {
		ConnectionClientThread->Join();
		ConnectionClientThread = nullptr;
		ConnectionClient = nullptr;
	}
	m_bStop = false;
	Started = false;
}

void WorkerThread::MineSha256(BitcoinWorkData& wd) {
	BitcoinSha256 *bcSha;

#if UCFG_BITCOIN_ASM
	DECLSPEC_ALIGN(64) byte bufShaAlgo[sizeof(SseBitcoinSha256) + (16*(32*UCFG_BITCOIN_WAY+8)) + 256];		// max possible size with SSE buffers
	if (Miner.UseSse2())
		bcSha = new(bufShaAlgo) SseBitcoinSha256;
	else
#else
	DECLSPEC_ALIGN(64) byte bufShaAlgo[sizeof(BitcoinSha256) + (16*(32*UCFG_BITCOIN_WAY+8)) + 256];		// max possible size
#endif
		bcSha = new(bufShaAlgo) BitcoinSha256;

	bcSha->PrepareData(wd.Midstate.constData(), wd.Data.constData()+64, wd.Hash1.constData());

	for (UInt32 nonce=wd.FirstNonce, end=wd.LastNonce+1; nonce!=end;) {
		if (bcSha->FindNonce(nonce)) {
			BitcoinSha256 sha256;
			sha256.PrepareData(wd.Midstate.constData(), wd.Data.constData()+64, wd.Hash1.constData());
			while (true) {
				if (!Miner.TestAndSubmit(_self, &wd, nonce)) {
					*Miner.m_pTraceStream << "\rFound NONCE not accepted by Target" << endl;
				}
/*!!!R					bool b = sha256.FindNonce(nonce); //!!!D
				cout << "sha256.FindNonce(nonce)= " << b << "  " << hex << nonce << endl;  //!!!D
				*/
				if (!(++nonce % UCFG_BITCOIN_NPAR) || !sha256.FindNonce(nonce))
					break;
			}
		}				

		Interlocked::Increment(Miner.HashCount);
		if (m_bStop)
			break;
	}
}

void WorkerThread::MineScrypt(BitcoinWorkData& wd) {
	UInt32 data[20];
	for (int i=0; i<_countof(data); ++i)
		data[i] = _byteswap_ulong(((UInt32*)wd.Data.data())[i]);
	UInt32 targ = be32toh(*(UInt32*)wd.TargetBE);

#ifdef _M_X64
	for (UInt64 nonce=wd.FirstNonce, end=UInt64(wd.LastNonce)+1; nonce<end;) {
#else
	for (UInt32 nonce=wd.FirstNonce, end=wd.LastNonce+1; nonce!=end;) {
#endif
		for (int i=0; i<UCFG_BITCOIN_NPAR;) {
			data[19] = UInt32(nonce); //!!!D

#if defined(_M_X64) && UCFG_BITCOIN_ASM
			array<array<UInt32, 8>, 3> res3 = CalcSCryptHash_80_3way(data);
			for (int j=0; j<3; ++j) {
				if (res3[j][7] <= targ && !Miner.TestAndSubmit(_self, &wd, _byteswap_ulong(UInt32(nonce)+j))) {
					*Miner.m_pTraceStream << "\rFound NONCE not accepted by Target" << endl;
				}
			}
			i += 3;
			nonce += 3;
#else
			array<UInt32, 8> res = CalcSCryptHash(ConstBuf(data, sizeof data));

			if (res[7] <= targ && !Miner.TestAndSubmit(_self, &wd, _byteswap_ulong(nonce))) {
				*Miner.m_pTraceStream << "\rFound NONCE not accepted by Target" << endl;
			}

				++i;
				++nonce;
#endif
		}				
		Interlocked::Increment(Miner.HashCount);
		if (m_bStop)
			break;
	}
}

#if UCFG_BITCOIN_SOLIDCOIN
void WorkerThread::MineSolid(BitcoinWorkData& wd) {
	ASSERT(wd.Data.Size == 128);
	SolidcoinBlock blk;
	memcpy(&blk, wd.Data.constData(), wd.Data.Size);
	blk.nTime += 1+int((DateTime::UtcNow()-wd.Timestamp).TotalSeconds);	
	blk.nNonce1 += wd.FirstNonce;
	for (UInt32 nonce=wd.FirstNonce, end=wd.LastNonce+1; nonce!=end; ++nonce) {
		HashValue hash1 = SolidcoinHash(ConstBuf(&blk, sizeof blk));
		if (!hash1[31]) {
			Blob hash(&hash1[0], 32);
			std::reverse(hash.data(), hash.data()+32);
			if (memcmp(hash.constData(), wd.TargetBE, 32) < 0) {
				wd.Data = Blob(&blk, sizeof blk);
				if (!Miner.TestAndSubmit(_self, &wd, (UInt32)blk.nNonce1)) {
					*Miner.m_pTraceStream << "Found NONCE not accepted by Target" << endl;
				}
			}
		}
		((UInt32&)blk.nNonce1)++;			// nonce
	}
	Interlocked::Add(Miner.HashCount, (wd.LastNonce-wd.FirstNonce+1)/UCFG_BITCOIN_NPAR);
}
#endif // UCFG_BITCOIN_SOLIDCOIN

void WorkerThread::Execute() {
	Name = "WorkerThread";

	Priority = THREAD_PRIORITY_LOWEST;

	while (!m_bStop) {

#ifdef X_DEBUG //!!!D
	ptr<BitcoinWorkData> wd = Miner.GetTestData();
	/*!!!
	((UInt32*)wd.Data.data())[16+3] = wd.FirstNonce;
	BitcoinSha256 sha256;
	sha256.PrepareData(wd.Midstate.constData(), wd.Data.constData()+64, wd.Hash1.constData());
	Blob hash = sha256.FullCalc();*/

#else
		ptr<BitcoinWorkData> wd = Miner.GetWorkForThread(_self, DevicePortion, true);
#endif

		if (!wd)
			return;
#ifdef X_DEBUG
		if ((wd.FirstNonce % BitcoinMiner::NONCE_STEP) != 0 ||
			((wd.LastNonce+1) % BitcoinMiner::NONCE_STEP) != 0
			|| wd.FirstNonce >= wd.LastNonce)
			cerr << "Invalid nonce numbers" << endl;
#endif

		StartRound();
		switch (wd->HashAlgo) {
		case Coin::HashAlgo::Sha256:
			MineSha256(*wd);
			break;
		case Coin::HashAlgo::SCrypt:
			MineScrypt(*wd);
			break;
#if UCFG_COIN_PRIME
		case Coin::HashAlgo::Prime:
			MinePrime(dynamic_cast<XptWorkData&>(*wd));
			break;
#endif
#if UCFG_BITCOIN_SOLIDCOIN
		case Coin::HashAlgo::Solid:
			MineSolid(*wd);
			break;
#endif
		default:
			Throw(E_NOTIMPL);
		}
		CompleteRound(wd->LastNonce-wd->FirstNonce+1);
	}
}


#if defined(_AFXDLL) || defined(_USRDLL)
	 CWinApp theMiner;
#endif


static const char * const s_reasons[] = {
	"bad-cb-flag", "bad-cb-length", "bad-cb-prefix", "bad-diffbits", "bad-prevblk", "bad-txnmrklroot", "bad-txns", "bad-version", "duplicate", "high-hash", "rejected", "stale-prevblk", "stale-work",
	"time-invalid", "time-too-new", "time-too-old", "unknown-user", "unknown-work"
};

void ThrowRejectionError(RCString reason) {
	for (int i=0; i<_countof(s_reasons); ++i)
		if (s_reasons[i] == reason)
			Throw(E_COIN_MINER_BAD_CB_FLAG+i);
	Throw(E_FAIL);
}

} // Coin::





