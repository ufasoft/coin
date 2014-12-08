/*######     Copyright (c) 1997-2014 Ufasoft  http://ufasoft.com  mailto:support@ufasoft.com,  Sergey Pavlov  mailto:dev@ufasoft.com #######################################
#                                                                                                                                                                          #
# This program is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation;  #
# either version 3, or (at your option) any later version. This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the      #
# implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details. You should have received a copy of the GNU #
# General Public License along with this program; If not, see <http://www.gnu.org/licenses/>                                                                               #
##########################################################################################################################################################################*/

#include <el/ext.h>

#include <el/comp/mean.h>

#if UCFG_WIN32
#	include <el/libext/win32/ext-win.h>
#endif

#include "resource.h"

#include <el/libext/ext-http.h>
#include "coin-msg.h"

#include "miner.h"


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
		copy(t.begin(), t.end(), r.HashTarget.data());
	}
	if (jres.HasKey("target_share")) {
		Blob t = Blob::FromHexString(jres["target_share"].ToString().Substring(2));
		if (t.Size != 32)
			Throw(E_INVALIDARG);
		reverse_copy(t.begin(), t.end(), r.HashTarget.data());
	}
	pair<UInt32, UInt32> nonceRange = FromOptionalNonceRange(jres);
	r.FirstNonce = nonceRange.first;
	r.LastNonce = nonceRange.second;

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
		"\"noncerange\":\"00001FB0000021AF\"}, \"error\": null}";

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

bool BitcoinWorkData::TestNonceGivesZeroH(UInt32 nonce) {
	Nonce = betoh(nonce);
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

void CpuDevice::Start(BitcoinMiner& miner, thread_group *tr) {
	for (int i=0; i<miner.ThreadCount; ++i) {
		ptr<WorkerThread> t = new WorkerThread(miner, tr);
		t->Device = this;
		miner.Threads.push_back(t.get());
		t->Start();
	}
}

BitcoinMiner::BitcoinMiner()
	:	HashCount(0)
	,	MaxHeight(0)
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
	,	m_bLongPolling(true)
	,	m_dtSwichToMainUrl(DateTime::MaxValue)
	,	m_msWait(NORMAL_WAIT)
	,	m_minGetworkQueue(MIN_GETWORK_QUEUE)
	,	Speed(0)
	,	CPD(0)
	,	ChainsExpectedCount(0)
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
	memset(HashBest.data(), 0xFF, 32);
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
//!!! UserAgentString += " (" + Environment.OSVersion.VersionString + ") "; //!!! XPT server wants short strings
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
	wc.Headers.Set("X-Mining-Extensions", "hostlist longpoll midstate noncerange rollntime stratum switchto");
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
				m_LongPollingThread->interrupt();
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
			*m_pTraceStream << "Switching back to " << BitcoinUrl  << endl;
		}

		String switchTo = wc.get_ResponseHeaders().Get("X-Switch-To");
		if (!!switchTo) {
			vector<SUrlTtr> urlTtrs = GetUrlTtrs(switchTo);
			if (!urlTtrs.empty()) {
				BitcoinUrl = urlTtrs[0].Url;
				m_dtSwichToMainUrl = now + TimeSpan::FromSeconds(urlTtrs[0].TtrMinutes*60);
				*m_pTraceStream << "Switching to " << BitcoinUrl  << endl;
			}
		}

		String hostList = wc.get_ResponseHeaders().Get("X-Host-List");
		if (!!hostList) {
			vector<SUrlTtr> urlTtrs = GetUrlTtrs(hostList);
			if (!urlTtrs.empty() && urlTtrs != HostList) {
				HostList = urlTtrs;
				HostIndex = 0;
				if (BitcoinUrl != HostList[0].Url) {
					*m_pTraceStream << "Switching to " << HostList[0].Url  << endl;
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
		ptr<Coin::MinerBlock> newMinerBlock = MinerBlock::FromJson(json);
		EXT_LOCKED(m_csCurrentData, MinerBlock = newMinerBlock);
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
	} catch (RCExc) {
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

	size_t cbN2 = minerBlock->ExtraNonce2.Size;
	byte *pN2 = minerBlock->ExtraNonce2.data();
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
	wd->Height = minerBlock->Height;

	MemoryStream stm;
	minerBlock->WriteHeader(BinaryWriter(stm).Ref());
	Blob blob = stm;
	size_t len = blob.Size;
	blob.Size = 128;
	FormatHashBlocks(blob.data(), len);

	wd->Hash1 = Blob(0, 64);
	FormatHashBlocks(wd->Hash1.data(), 32);

	wd->HashAlgo = HashAlgo;
	wd->Timestamp = now;
	wd->Data = blob;
	switch (HashAlgo) {
	case Coin::HashAlgo::Sha3:
		break;
	default:
		wd->Midstate = CalcSha256Midstate(wd->Data);
	}
	wd->HashTarget = minerBlock->HashTarget;

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
	String method = GetMethodName(false);
	try {
		pwd->Timestamp = now;

		if (method == "getblocktemplate") {
			String sjson = wc.UploadString(url, "{\"method\": \"" + method + "\", \"params\": [{\"capabilities\": [\"coinbasetxn\", \"workid\", \"coinbase/append\", \"longpollid\"]}], \"id\":0}");
			
			String xStratum = wc.get_ResponseHeaders().Get("X-Stratum");
			if (xStratum != nullptr) {
				*m_pTraceStream << "\nSwitching to " << xStratum  << endl;
				CreateConnectionClient(xStratum);
				return ConnectionClient->GetWork();
			}

			VarValue json = ParseJson(sjson);
			json = json["result"];
			if (!json.HasKey("version")) {
				m_getWorkMethodLevel = 1;
				if (pwd = BitcoinWorkData::FromJson(sjson, HashAlgo)) {
					pwd->SetRollNTime(wc);
					SetWebInfo(wc);
					m_msWait = NORMAL_WAIT;
				} else
					goto LAB_FALLBACK_GETWORK;
			} else {
				ptr<Coin::MinerBlock> newMinerBlock = MinerBlock::FromJson(json);
				EXT_LOCKED(m_csCurrentData, MinerBlock = newMinerBlock);
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
			}
		} else if (pwd = BitcoinWorkData::FromJson(wc.UploadString(url, "{\"method\": \"" + method + "\", \"params\": [], \"id\":0}"), HashAlgo)) {
			pwd->SetRollNTime(wc);
			SetWebInfo(wc);
			m_msWait = NORMAL_WAIT;
		}
		CheckLongPolling(wc);  // should be before SetNewData() because SetNewData() uses m_LongPollingThread
		return pwd;
	} catch (RCExc ex) {
		*m_pTraceStream << ex.what() << endl;
		if (m_getWorkMethodLevel == 0) {
			m_getWorkMethodLevel = 1;
			goto LAB_FALLBACK_GETWORK;
		}
		EXT_LOCK (m_csCurrentData) {
			if (HostIndex+1 < HostList.size()) {
				SUrlTtr& urlTtr = HostList[++HostIndex];
				BitcoinUrl = urlTtr.Url;
				m_dtSwichToMainUrl = urlTtr.TtrMinutes ? DateTime::UtcNow()+TimeSpan::FromSeconds(urlTtr.TtrMinutes*60) : DateTime::MaxValue;
				*m_pTraceStream << "Switching to " << BitcoinUrl  << endl;
				return nullptr;
			} else 
				HostIndex = 0;
		}
	}
	m_msWait = std::min(MAX_WAIT, m_msWait*2);
	return nullptr;
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
		mblock.Nonce = wd.Nonce;
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

bool BitcoinMiner::TestAndSubmit(BitcoinWorkData *wd, UInt32 nonce) {
	wd->Nonce = betoh(nonce);
	HashValue hash = Hasher::Find(wd->HashAlgo).CalcWorkDataHash(*wd);

	TRC(3, "Pre-result Nonce Found: " << hex << nonce << ", hash: " << hash);

	if (hash < wd->HashTarget) {

#ifdef X_DEBUG//!!!D
		cout << "\nMerkle: " << wd->MerkleRoot << "\n";
#endif

		EXT_LOCK (m_csCurrentData) {
			WorksToSubmit.push_back(wd);
		}
		m_evGetWork.Set();

		if (hash < HashBest) {
			HashBest = hash;
			*m_pTraceStream  << "Best Hash: " << hash << endl;
		}
		return true;
	} else {
//		HashValue hash1 = Hasher::Find(wd->HashAlgo).CalcWorkDataHash(*wd);
//		HashValue hash2 = Hasher::Find(wd->HashAlgo).CalcWorkDataHash(*wd);
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
	String dir = Path::GetDirectoryName(System.ExeFilePath),
		filename = Path::Combine(dir, sResId);
	if (!File::Exists(filename) && File::Exists(dir + "/../" + sResId))
		filename = dir + "/../" + sResId;
	return File::ReadAllText(filename);
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
	if (bSuccess)
		Interlocked::Increment(AcceptedCount);
	ostringstream os;
	os << Hasher::Find(wd.HashAlgo).CalcWorkDataHash(wd);
	String shash = os.str();
	if (!Verbosity) {
		int nPos = 0;
		for (; nPos<10 && shash[nPos]=='0'; ++nPos)
			;
		shash = shash.Substring(nPos, 8);
	}
	*m_pTraceStream  << DateTime::Now() << " Result: " << shash << (bSuccess ? " accepted" : " rejected") << "   " << (!message.IsEmpty() ? message : String()) << "              " << endl;
}

class GetWorkThread : public Thread {
	typedef Thread base;
public:
	BitcoinMiner& Miner;

	GetWorkThread(BitcoinMiner& miner, thread_group *tr)
		:	base(tr)
		,	Miner(miner)
		,	CurrentWebClient(0)
	{
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
		
		MeanCalculator<Int64> meanHash(20);
		MeanCalculator<double> meanChain(20);
		
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

			dt = DateTime::UtcNow();
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
						Miner.ConnectionClient->Submit(wd);
					} else {
						bool r = Miner.SubmitResult(CurrentWebClient, *wd);
						Miner.Print(*wd, r, CurrentWebClient ? CurrentWebClient->get_ResponseHeaders().Get("X-Reject-Reason") : String(nullptr));
					}
				} catch (RCExc ex) {
					*Miner.m_pTraceStream << ex.what() << endl;
				}
			}

			Int64 nProcessed = Interlocked::Exchange(Miner.HashCount, 0);
			if (Miner.HashAlgo != HashAlgo::Prime)
				nProcessed *= UCFG_BITCOIN_NPAR;
			Miner.EntireHashCount += nProcessed;
			meanHash.AddValue(nProcessed);
			Miner.Speed = float(meanHash.FindMeanValue());
			Miner.CPD = float(meanChain.FindMeanValue() * (60*60*24));

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
					*Miner.m_pTraceStream << ex.what() << endl;
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
		XptClient *c = new XptClient(_self);
		ConnectionClient = c;
		ConnectionClientThread = c;
#endif
	} else
		Throw(E_INVALIDARG);
	ConnectionClient->SetEpServer(IPEndPoint(uri.Host, (UInt16)uri.Port));
	ConnectionClientThread->Start();
}

void BitcoinMiner::Start(thread_group *tr) {
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
		Threads[i]->interrupt();
	ptr<Thread> longPollingThread = EXT_LOCKED(m_csLongPollingThread, m_LongPollingThread);
	if (longPollingThread)
		longPollingThread->interrupt();
	if (ConnectionClientThread)
		ConnectionClientThread->interrupt();
	if (GetWorkThread)
		GetWorkThread->interrupt();
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
		StartRound();
		UInt32 nHash = Hasher::Find(wd->HashAlgo).MineOnCpu(Miner, *wd);
		CompleteRound(nHash);
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

ptr<BitcoinWorkData> ConnectionClient::GetWork() {
	DateTime now = DateTime::UtcNow();
	EXT_LOCK (MtxData) {
		if (MinerBlock) {
			if (now < MinerBlock->MyExpireTime)
				return Miner.GetWorkFromMinerBlock(now, MinerBlock.get());
			MinerBlock = nullptr;
		}
	}
	return nullptr;
}

Hasher *Hasher::GetRoot() {
	return Root;				// Root static member can't be exported
}

Hasher& Hasher::Find(HashAlgo algo) {
	for (Hasher *p=Root; p; p=p->Next) {
		if (p->Algo == algo)
			return *p;
	}
	Throw(E_NOTIMPL);
}

HashValue Hasher::CalcWorkDataHash(const BitcoinWorkData& wd) {
	UInt32 buf[100];
	int nWords = int(GetDataSize() / sizeof(UInt32));
	ASSERT(nWords <= _countof(buf));
	const UInt32 *p = (UInt32*)wd.Data.constData();
	for (int i=0; i<nWords; ++i)
		buf[i] = betoh(p[i]);
	return CalcHash(ConstBuf(buf, nWords*sizeof(UInt32)));
}

void Hasher::SetNonce(UInt32 *buf, UInt32 nonce) noexcept {
	buf[19] = nonce;
}

void Hasher::MineNparNonces(BitcoinMiner& miner, BitcoinWorkData& wd, UInt32 *buf, UInt32 nonce) {
	size_t cbBuf = GetDataSize();
	for (int i=0; i<UCFG_BITCOIN_NPAR; ++i, ++nonce) {
		SetNonce(buf, nonce);
		HashValue hash = CalcHash(ConstBuf(buf, cbBuf));
		if (!hash[31] && hash <= wd.HashTarget)
			miner.TestAndSubmit(&wd, htobe(nonce));
	}
}

UInt32 Hasher::MineOnCpu(BitcoinMiner& miner, BitcoinWorkData& wd) {
	UInt32 buf[100];
	int nWords = int(GetDataSize() / sizeof(UInt32));
	ASSERT(nWords <= _countof(buf));
	const UInt32 *p = (UInt32*)wd.Data.constData();
	for (int i=0; i<nWords; ++i)
		buf[i] = betoh(p[i]);
	UInt32 nonce = wd.FirstNonce;
	for (UInt32 end=wd.LastNonce+1; nonce!=end && !Ext::this_thread::interruption_requested(); nonce+=UCFG_BITCOIN_NPAR, Interlocked::Increment(miner.HashCount)) {
		MineNparNonces(miner, wd, buf, nonce);
		if (wd.Height!=0 && wd.Height!=UInt32(-1) && wd.Height!=miner.MaxHeight)
			break;
	}
	return nonce-wd.FirstNonce;
}

} // Coin::


template<> Coin::Hasher * StaticList<Coin::Hasher>::Root = 0;





