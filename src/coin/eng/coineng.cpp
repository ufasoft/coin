/*######   Copyright (c) 2011-2015 Ufasoft  http://ufasoft.com  mailto:support@ufasoft.com,  Sergey Pavlov  mailto:dev@ufasoft.com ####
#                                                                                                                                     #
# 		See LICENSE for licensing information                                                                                         #
#####################################################################################################################################*/

#include <el/ext.h>

#include <el/crypto/hash.h>
#include <el/db/bdb-reader.h>

//#include "coineng.h"
#include "coin-protocol.h"
#include "script.h"
#include "eng.h"

#if UCFG_COIN_COINCHAIN_BACKEND == COIN_BACKEND_DBLITE
#	include "backend-dblite.h"
#else
#	include "backend-sqlite.h"
#endif

#if defined(_MSC_VER)
#	pragma comment(lib, "cryp")
#	if UCFG_COIN_GENERATE
#		pragma comment(lib, "miner")
#	endif
#	ifndef _AFXDLL
#		pragma comment(lib, "libext.lib")
#		if !UCFG_STDSTL
#			pragma comment(lib, "..\\" UCFG_PLATFORM_SHORT_NAME "_Release\\lib\\elrt")
#			pragma comment(lib, "el-std")
#		endif
#		pragma comment(lib, "coinutil")
#		pragma comment(lib, "openssl")
#		pragma comment(lib, "dblite")
#		pragma comment(lib, "sqlite3")
#		if UCFG_USE_TOR
#			pragma comment(lib, "crypt32")
#			pragma comment(lib, "tor")
#		endif
#	endif		// !defined(_AFXDLL)
#endif			// _MSC_VER


using Ext::DB::DbException;

template<> Coin::CurrencyFactoryBase * StaticList<Coin::CurrencyFactoryBase>::Root = 0;

namespace Coin {


CoinEng *CurrencyFactoryBase::CreateEng(CoinDb& cdb) {
	return new CoinEng(cdb);
}

void ChainParams::Init() {
	IsTestNet = false;
	HashAlgo = Coin::HashAlgo::Sha256;
	ProtocolVersion = PROTOCOL_VERSION;
	AuxPowStartBlock = INT_MAX;
	CoinValue = 100000000;
	InitBlockValue = CoinValue * 50;
	MaxMoney = 21000000 * CoinValue;
	HalfLife = 210000;
	AnnualPercentageRate = 0;
	MinTxFee = CoinValue/10000;
	BlockSpan = seconds(600);
	TargetInterval = 2016;
	MinTxOutAmount = 0;
	MaxPossibleTarget = Target(0x1D7FFFFF);
	InitTarget = MaxPossibleTarget;
	CoinbaseMaturity = 100;
	PowOfDifficultyToHalfSubsidy = 1;
	PayToScriptHashHeight = numeric_limits<int>::max();
	ScriptAddressVersion = 5;
	CheckDupTxHeight = numeric_limits<int>::max();
	Listen = true;
	MedianTimeSpan = 11;
	AllowLiteMode = true;
	MiningAllowed = false;
}

Blob EncryptedPrivKey(BuggyAes& aes, const MyKeyInfo& key) {
	byte typ = DEFAULT_PASSWORD_ENCRYPT_METHOD;
	Blob privKey = key.PrivKey;
	return Blob(&typ, 1) + aes.Encrypt(privKey + Crc32().ComputeHash(privKey));
}


EXT_THREAD_PTR(void) t_bPayToScriptHash;

COIN_EXPORT CoinEng& ExternalEng() {
	return *dynamic_cast<CoinEng*>(HasherEng::GetCurrent());
}

CCoinEngThreadKeeper::CCoinEngThreadKeeper(CoinEng *cur)
	:	base(cur)
{
//	m_prev = t_pCoinEng;
	m_prevPayToScriptHash = t_bPayToScriptHash;
//	t_pCoinEng = cur;
	t_bPayToScriptHash = 0;
}

CCoinEngThreadKeeper::~CCoinEngThreadKeeper() {
//	t_pCoinEng = m_prev;
	t_bPayToScriptHash = (void*)m_prevPayToScriptHash;
}


CoinEng::CoinEng(CoinDb& cdb)
	: base(cdb)
	, m_cdb(cdb)
	, MaxBlockVersion(3)
	, m_freeCount(0)
	, CommitPeriod(0x1F)
	, m_mode(EngMode::Normal)
	, AllowFreeTxes(true)
	, UpgradingDatabaseHeight(0)
	, OffsetInBootstrap(0)
	, NextOffsetInBootstrap(0)
	, aPreferredDownloadPeers(0)
	, aSyncStartedPeers(0)
{
}

CoinEng::~CoinEng() {
}

void CoinEng::SetChainParams(const Coin::ChainParams& p) {
	ChainParams = p;
	Db = CreateBlockChainDb();
	if (ChainParams.IsTestNet)
		Caches.HashToBlockCache.SetMaxSize(2165);
}

void CoinEng::EnsureTransactionStarted() {
	if (!exchange(m_dbTxBegin, true)) {
		Db->BeforeEnsureTransactionStarted();
		Db->BeginTransaction();
	}
}

void CoinEng::CommitTransactionIfStarted() {
	unique_lock<recursive_mutex> lk(Mtx, try_to_lock);
	if (lk) {
		if (exchange(m_dbTxBegin, false)) {
			if (m_iiEngEvents)
				m_iiEngEvents->OnBeforeCommit();

#if UCFG_TRC
			DateTime dbgStart = Clock::now();
#endif
			Db->Commit();
		
#if UCFG_TRC
			TRC(2, duration_cast<milliseconds>(Clock::now() - dbgStart).count() << " ms, LastBlock: " << BestBlockHeight());
#endif
		}
	}
}

HashValue CoinEng::HashMessage(const ConstBuf& cbuf) {
	return Coin::Hash(cbuf);
}

HashValue CoinEng::HashForSignature(const ConstBuf& cbuf) {
	return Coin::Hash(cbuf);
}

HashValue CoinEng::HashFromTx(const Tx& tx) {
	return Coin::Hash(EXT_BIN(tx));
}

void CoinEng::Close() {
	TRC(1, "Closing " << GetDbFilePath());

	CCoinEngThreadKeeper keeper(this);
	CommitTransactionIfStarted();

	if (m_iiEngEvents)
		m_iiEngEvents->OnCloseDatabase();

	Db->Close();
}

ptr<IBlockChainDb> CoinEng::CreateBlockChainDb() {
#if UCFG_COIN_COINCHAIN_BACKEND == COIN_BACKEND_DBLITE
	return new DbliteBlockChainDb;
#else
	return new SqliteBlockChainDb;
#endif
}

void CoinEng::ClearByHeightCaches() {
	EXT_LOCKED(Caches.Mtx, Caches.HeightToHashCache.clear());
}

Block CoinEng::GetBlockByHeight(uint32_t height) {
	Block block(nullptr);

	EXT_LOCK (Caches.Mtx) {
		ChainCaches::CHeightToHashCache::iterator it = Caches.HeightToHashCache.find(height);
		if (it != Caches.HeightToHashCache.end() && Lookup(Caches.HashToBlockCache, it->second.first, block))
			return block;
	}
	block = Db->FindBlock(height);

#ifdef X_DEBUG //!!!D
	HashValue h1 = Hash(block);
	block.m_pimpl->m_hash.reset();
	ASSERT(h1 == Hash(block));
#endif

	ASSERT(block.Height == height);
	HashValue hashBlock = Hash(block);
	EXT_LOCK (Caches.Mtx) {
		Caches.HeightToHashCache.insert(make_pair((ChainCaches::CHeightToHashCache::key_type)height, hashBlock));
		Caches.HashToBlockCache.insert(make_pair(hashBlock, block));
	}
	return block;
}

BlockHeader CoinEng::FindHeader(const HashValue& hash) {
	EXT_LOCK(Caches.Mtx) {
		Block b(nullptr);
		if (Lookup(Caches.HashToBlockCache, hash, b))
			return b;
		ChainCaches::COrphanMap::iterator it = Caches.OrphanBlocks.find(hash);
		if (it != Caches.OrphanBlocks.end())
			return it->second.first;
	}
	return Tree.FindHeader(hash);
}
 
Block CoinEng::LookupBlock(const HashValue& hash) {
	Block r(nullptr);
	EXT_LOCK (Caches.Mtx) {
		if (Lookup(Caches.HashToBlockCache, hash, r))
			return r;
	}
	if (r = Db->FindBlock(hash)) {
		EXT_LOCK (Caches.Mtx) {
			Caches.HeightToHashCache.insert(make_pair((ChainCaches::CHeightToHashCache::key_type)r.Height, hash));
			Caches.HashToBlockCache.insert(make_pair(hash, r));
		}
	}
	return r;
}

bool CoinEng::CreateDb() {
	bool r = Db->Create(GetDbFilePath());

	TransactionScope dbtx(*Db);
	
	if (m_iiEngEvents)
		m_iiEngEvents->OnCreateDatabase();

	return r;
}

void ChainParams::ReadParam(int& param, IXmlAttributeCollection& xml, RCString name) {
	String a = xml.GetAttribute(name);
	if (!a.empty())
		param = stoi(a);
}

void ChainParams::ReadParam(int64_t& param, IXmlAttributeCollection& xml, RCString name) {
	String a = xml.GetAttribute(name);
	if (!a.empty())
		param = stoll(a);
}

void ChainParams::LoadFromXmlAttributes(IXmlAttributeCollection& xml) {
	Name = xml.GetAttribute("Name");
	if ((Symbol = xml.GetAttribute("Symbol")).empty())
		Symbol = Name;
	IsTestNet = xml.GetAttribute("IsTestNet") == "1";
	Genesis = HashValue(xml.GetAttribute("Genesis"));

	ReadParam(CoinValue, xml, "CoinValue");
	ReadParam(CoinbaseMaturity, xml, "CoinbaseMaturity");
	ReadParam(MinTxFee, xml, "MinTxFee");
	ReadParam(TargetInterval, xml, "TargetInterval");
	ReadParam(MinTxOutAmount, xml, "MinTxOutAmount");
	ReadParam(AnnualPercentageRate, xml, "AnnualPercentageRate");
	ReadParam(HalfLife, xml, "HalfLife");
	ReadParam(CheckDupTxHeight, xml, "CheckDupTxHeight");

	String a;

	if (!(a = xml.GetAttribute("BlockSpan")).empty())
		BlockSpan = seconds(stoi(a));

	if (!(a = xml.GetAttribute("InitBlockValue")).empty())
		InitBlockValue = CoinValue * stoll(a);

	if (!(a = xml.GetAttribute("MaxMoney")).empty())
		MaxMoney = stoll(a) * CoinValue;

	if (!(a = xml.GetAttribute("PowOfDifficultyToHalfSubsidy")).empty())
		PowOfDifficultyToHalfSubsidy = 1.0/stoi(a);

	a = xml.GetAttribute("MaxTargetHex");
	MaxTarget = !a.empty() ? Target(Convert::ToUInt32(a, 16)) : Target(0x1D00FFFF);

	if (!(a = xml.GetAttribute("InitTargetHex")).empty())
		InitTarget = Target(Convert::ToUInt32(a, 16));

	ProtocolMagic = Convert::ToUInt32(xml.GetAttribute("ProtocolMagicHex"), 16);

	if (!(a = xml.GetAttribute("ProtocolVersion")).empty())
		ProtocolVersion = Convert::ToUInt32(a);

	a = xml.GetAttribute("DefaultPort");
	DefaultPort = !a.empty() ? Convert::ToUInt16(a) : 8333;

	BootUrls = xml.GetAttribute("BootUrls").Split();

	a = xml.GetAttribute("IrcRange");
	IrcRange = !a.empty() ? stoi(a) : 0;

	a = xml.GetAttribute("AddressVersion");
	AddressVersion = !a.empty() ? Convert::ToByte(a) : (IsTestNet ? 111 : 0);

	a = xml.GetAttribute("ScriptAddressVersion");
	ScriptAddressVersion = !a.empty() ? Convert::ToByte(a) : (IsTestNet ? 196 : 5);

	a = xml.GetAttribute("ChainID");
	ChainId = !a.empty() ? stoi(a) : 0;

	a = xml.GetAttribute("PayToScriptHashHeight");
	if (!!a)
		PayToScriptHashHeight = stoi(a);

	if (!(a = xml.GetAttribute("Listen")).empty())
		Listen = stoi(a);

	if (!(a = xml.GetAttribute("AllowLiteMode")).empty())
		AllowLiteMode = stoi(a);

	if (!(a = xml.GetAttribute("MiningAllowed")).empty())
		MiningAllowed = stoi(a);

	if (!(a = xml.GetAttribute("MergedMiningStartBlock")).empty()) {
		AuxPowEnabled = true;
		AuxPowStartBlock = stoi(a);
	}

	if (!(a = xml.GetAttribute("Algo")).empty())
		HashAlgo = StringToAlgo(a);

	a = xml.GetAttribute("FullPeriod");
	FullPeriod = !a.empty() && atoi(a);
}

int ChainParams::Log10CoinValue() const {
	int r = (int)log10(double(CoinValue));
	return r;
}

static regex ReIp4Port("\\d+\\.\\d+\\.\\d+\\.\\d+:\\d+");

void ChainParams::AddSeedEndpoint(RCString seed) {
	Seeds.insert(regex_match(seed.c_str(), ReIp4Port)
		? IPEndPoint::Parse(seed)
		: IPEndPoint(IPAddress::Parse(seed), DefaultPort));
}

String CoinEng::GetCoinChainsXml() {
	String xml;
	path pathXml = System.GetExeDir() / "coin-chains.xml";
	if (exists(path(pathXml)))
		xml = File::ReadAllText(pathXml);
#if UCFG_WIN32
	else if (AfxHasResource("coin_chains.xml", RT_RCDATA))
		xml = Encoding::UTF8.GetChars(Resource("coin_chains.xml", RT_RCDATA));
#endif
	else
		Throw(CoinErr::XmlFileNotFound);
	return xml;
}

ptr<CoinEng> CoinEng::CreateObject(CoinDb& cdb, RCString name) {
	AFX_MANAGE_STATE(AfxGetStaticModuleState());
	String xml = GetCoinChainsXml();

	Coin::ChainParams params;
	try {
#if UCFG_WIN32		
		CUsingCOM usingCOM;
		XmlDocument doc = new XmlDocument;
		doc.LoadXml(xml);		// LoadXml() instead of Load() to eliminate loading shell32.dll
		XmlNodeReader rd(doc);
#else
		istringstream is(xml.c_str());
		XmlTextReader rd(is);
#endif
 		for (bool b=rd.ReadToDescendant("Chain"); b; b=rd.ReadToNextSibling("Chain")) {
			if (name == rd.GetAttribute("Name")) {
				params.Name = name;
				params.LoadFromXmlAttributes(rd);
				while (rd.Read()) {
					if (rd.NodeType == XmlNodeType::Element || rd.NodeType == XmlNodeType::EndElement) {
						String elName = rd.Name;
						if (elName == "Chain")
							break;
						if (rd.NodeType == XmlNodeType::Element) {
							if (elName == "Checkpoint") {
								int height = stoi(rd.GetAttribute("Block"));
								params.LastCheckpointHeight = std::max(height, params.LastCheckpointHeight);
								params.Checkpoints.push_back(make_pair(height, HashValue(rd.GetAttribute("Hash"))));
							} else if (elName == "Seed") {
								vector<String> ss = rd.GetAttribute("EndPoint").Trim().Split();
								EXT_FOR (const String& seed, ss)
									params.AddSeedEndpoint(seed);
							} else if (elName == "AlertPubKey") {
								params.AlertPubKeys.push_back(Blob::FromHexString(rd.GetAttribute("Key")));
							} else if (elName == "CheckpointMasterPubKey") {
								params.CheckpointMasterPubKey = Blob::FromHexString(rd.GetAttribute("Key"));
							}
						}
					}
				}
				goto LAB_RET;
			}
		}
	} catch (RCExc) {
	}
	Throw(CoinErr::XmlFileNotFound);
LAB_RET:
	ptr<CoinEng> peng;
	for (CurrencyFactoryBase *p=CurrencyFactoryBase::Root; p; p=p->Next)
		if (p->Name == name)
			peng = p->CreateEng(cdb);
	(peng ? peng : (peng = new CoinEng(cdb)))->SetChainParams(params);
	peng->AddressVersion = peng->ChainParams.AddressVersion;
	peng->ScriptAddressVersion = peng->ChainParams.ScriptAddressVersion;
	peng->ProtocolMagic = peng->ChainParams.ProtocolMagic;
	peng->Listen = peng->ChainParams.Listen;
	peng->DefaultPort = peng->ChainParams.DefaultPort;
	return peng;
}

class DnsThread : public Thread {
	typedef Thread base;
public:
	CoinEng& Eng;
	vector<String> DnsServers;

	DnsThread(CoinEng& eng, const vector<String>& dnsServers)
		:	base(&eng.m_tr)
		,	Eng(eng)
		,	DnsServers(dnsServers)
	{
	}
protected:
	void Execute() override {
		Name = "DnsThread";

		DBG_LOCAL_IGNORE_WIN32(WSAHOST_NOT_FOUND);
		DBG_LOCAL_IGNORE_WIN32(WSATRY_AGAIN);

		DateTime date = Clock::now() - TimeSpan::FromDays(1);
		EXT_FOR (const String& dns, DnsServers) {
			if (m_bStop)
				break;
			try {
				IPHostEntry he = Dns::GetHostEntry(dns);
				EXT_FOR (const IPAddress& a, he.AddressList) {
					IPEndPoint ep = IPEndPoint(a, Eng.ChainParams.DefaultPort);
//					TRC(2, ep);
					Eng.Add(ep, uint64_t(NodeServices::NODE_NETWORK), date);
				}
			} catch (RCExc) {
			}
		}
	}
};


static regex s_reDns("dns://(.+)");

void CoinEng::StartDns() {
	vector<String> dnsServers;
	vector<String> ar = ChainParams.BootUrls;
	random_shuffle(ar.begin(), ar.end());
	for (int i=0; i<ar.size(); ++i) {
		String url = ar[i];
		cmatch m;
		if (regex_match(url.c_str(), m, s_reDns))
			dnsServers.push_back(m[1]);
	}
	if (!dnsServers.empty())
		(new DnsThread(_self, dnsServers))->Start();
}

BlockHeader CoinEng::BestHeader() {
	EXT_LOCK(Caches.Mtx) {
		return Caches.m_bestHeader;
	}
}

int CoinEng::BestHeaderHeight() {
	return EXT_LOCKED(Caches.Mtx, Caches.m_bestHeader.SafeHeight);
}

void CoinEng::SetBestHeader(const BlockHeader& bh) {
	EXT_LOCKED(Caches.Mtx, Caches.m_bestHeader = bh);
}

Block CoinEng::BestBlock() {
	EXT_LOCK(Caches.Mtx) {
		return Caches.m_bestBlock;
	}
}

int CoinEng::BestBlockHeight() {
	return EXT_LOCKED(Caches.Mtx, Caches.m_bestBlock.SafeHeight);
}

void CoinEng::SetBestBlock(const Block& b) {
	EXT_LOCK(Caches.Mtx) {
		Caches.m_bestBlock = b;
		if (b.SafeHeight > Caches.m_bestHeader.SafeHeight)
			Caches.m_bestHeader = b;
	}
}

void CoinEng::put_Mode(EngMode mode) {
	if (!ChainParams.AllowLiteMode && mode==EngMode::Lite)
		Throw(errc::invalid_argument);

	bool bRunned = Runned;
	if (bRunned)
		Stop();
	SetBestHeader(nullptr);
	SetBestBlock(nullptr);
	m_mode = mode;
	Filter = nullptr;
	m_dbFilePath = "";
	if (bRunned) {
		Load();
		Start();
	}
}

void CoinEng::PurgeDatabase() {
	bool bRunned = Runned;
	if (bRunned)
		Stop();
	Db->Close(false);
	SetBestHeader(nullptr);
	SetBestBlock(nullptr);
	sys::remove(GetDbFilePath());
	Filter = nullptr;
	if (bRunned) {
		Load();
		Start();
	}
}

void CoinEng::AddLink(P2P::LinkBase *link) {
	base::AddLink(link);
	if (m_iiEngEvents)
		m_iiEngEvents->OnChange();
}

void CoinEng::OnCloseLink(P2P::LinkBase& link) {
	CoinLink& clink = static_cast<CoinLink&>(link);
	base::OnCloseLink(link);
	RemoveVerNonce(static_cast<P2P::Link&>(link));
	aPreferredDownloadPeers -= clink.IsPreferredDownload;
	aSyncStartedPeers -= clink.IsSyncStarted;
	EXT_LOCK(Mtx) {
		EXT_FOR(QueuedBlockItem& qbi, clink.BlocksInFlight) {
			MapBlocksInFlight.erase(qbi.HashBlock);
			TRC(5, "Removing from MapBlocksInFlight: " << qbi.HashBlock);
		}
	}

	if (m_iiEngEvents)
		m_iiEngEvents->OnChange();
}

void CoinEng::OnPeriodicMsgLoop() {
	EXT_LOCK (m_mtxThreadStateChange) {
		if (Runned) {
			base::OnPeriodicMsgLoop();
			CommitTransactionIfStarted();

			if (m_iiEngEvents)
				m_iiEngEvents->OnPeriodic();
		}
	}
}

void MsgLoopThread::Execute() {
	Name = "MsgLoopThread";

	DateTime dtNextPeriodic(0);
	const TimeSpan spanPeriodic = seconds(P2P::PERIODIC_SECONDS);

	try {
		DBG_LOCAL_IGNORE_CONDITION(errc::not_a_socket);

		while (!m_bStop) {
			DateTime now = Clock::now();
			if (now < dtNextPeriodic)
				m_ev.lock(P2P::PERIODIC_SECONDS*1000);
			else {
				dtNextPeriodic = now + spanPeriodic;
				EXT_LOCK(m_cdb.MtxNets) {
					for (int i=0; i<m_cdb.m_nets.size(); ++i) {
						CoinEng& eng = *static_cast<CoinEng*>(m_cdb.m_nets[i]);
						CCoinEngThreadKeeper engKeeper(&eng);
						eng.OnPeriodicMsgLoop();
					}
				}
			}
		}
	} catch (system_error& ex) {
		if (ex.code() != errc::not_a_socket)
			throw;
	}
}

void CoinEng::SavePeers() {
	m_cdb.SavePeers(m_idPeersNet, GetAllPeers());
}

void CoinEng::Misbehaving(P2P::Message *m, int howMuch, RCString command, RCString reason) {
	if (!command.empty())
		m->Link->Send(new RejectMessage(RejectReason::Invalid, command, reason));
	Peer& peer = *m->Link->Peer;
	if ((peer.Misbehavings += howMuch) > P2P::MAX_PEER_MISBEHAVINGS) {
		m_cdb.BanPeer(peer);
		throw;
	}
}

void CoinEng::OnMessage(P2P::Message *m) {
	if (!Runned)
		return;
	CCoinEngThreadKeeper engKeeper(this);
	try {
		DBG_LOCAL_IGNORE_CONDITION(CoinErr::RecentCoinbase);
		DBG_LOCAL_IGNORE_CONDITION(CoinErr::IncorrectProofOfWork);
		DBG_LOCAL_IGNORE_CONDITION(CoinErr::RejectedByCheckpoint);
		DBG_LOCAL_IGNORE_CONDITION(CoinErr::AlertVerifySignatureFailed);
		DBG_LOCAL_IGNORE_CONDITION(CoinErr::ProofOfWorkFailed);
		DBG_LOCAL_IGNORE_CONDITION(CoinErr::BadPrevBlock);
		DBG_LOCAL_IGNORE_CONDITION(CoinErr::VersionMessageMustBeFirst);

		TRC(4, m->Link->Peer->get_EndPoint().Address << ": " << *static_cast<CoinMessage*>(m));

		if (!m->Link->PeerVersion && !dynamic_cast<VersionMessage*>(m))
			Throw(CoinErr::VersionMessageMustBeFirst);
		m->Process(*m->Link);
	} catch (const DbException&) {
		EXT_LOCK (Mtx) {
			if (CriticalException == std::exception_ptr())
				CriticalException = std::current_exception();
		}
		try {
			Db->Close();
		} catch (RCExc) {
		}	
		if (m_iiEngEvents)
			m_iiEngEvents->OnChange();
		throw;
	} catch (const PeerMisbehavingException& ex) {
		TRC(2, "Misbehaving " << ex.what());
		Misbehaving(m, ex.HowMuch);
	} catch (Exception& ex) {
		if (ex.code().category() != coin_category()) {
			TRC(2, ex.what());
			if (ex.code() == ExtErr::ObsoleteProtocolVersion) {
				m->Link->Send(new RejectMessage(RejectReason::Obsolete, "version", EXT_STR("Version must be " << MIN_PEER_PROTO_VERSION << " or greater")));
				throw;
			}
			Misbehaving(m, 1);
		} else {
			switch ((CoinErr)ex.code().value()) {
			case CoinErr::SizeLimits:		
			case CoinErr::DupTxInputs:
				Misbehaving(m, 100);
				break;
			case CoinErr::RejectedByCheckpoint:
				m->Link->Send(new RejectMessage(RejectReason::CheckPoint, "block", "checkpoint mismatch"));
				Misbehaving(m, 100);
				break;
			case CoinErr::MoneyOutOfRange:
				Misbehaving(m, 100, "block", "bad-txns-txouttotal-toolarge");
				break;
			case CoinErr::BadTxnsVoutNegative:
				Misbehaving(m, 100, "block", "bad-txns-vout-negative");
				break;
			case CoinErr::BlockHeightMismatchInCoinbase:
				Misbehaving(m, 100, "block", "bad-cb-height");
				break;
			case CoinErr::FirstTxIsNotCoinbase:
				Misbehaving(m, 100, "block", "bad-cb-missing");
				break;
			case CoinErr::FirstTxIsNotTheOnlyCoinbase:
				Misbehaving(m, 100, "block", "bad-cb-multiple");
				break;
			case CoinErr::BadCbLength:
				Misbehaving(m, 100, "block", "bad-cb-length");
				break;
			case CoinErr::ProofOfWorkFailed:
				Misbehaving(m, 50, "block", "high-hash");
				break;			
			case CoinErr::BadTxnsVinEmpty:
				Misbehaving(m, 10, "block", "bad-txns-vin-empty");
				break;
			case CoinErr::BadTxnsVoutEmpty:
				Misbehaving(m, 10, "block", "bad-txns-vout-empty");
				break;
			case CoinErr::BadTxnsPrevoutNull:
				Misbehaving(m, 10, "block", "bad-txns-prevout-null");
				break;
			case CoinErr::BadPrevBlock:
				Misbehaving(m, 10, "block", "bad-prevblk");
				break;
			case CoinErr::TxFeeIsLow:
				m->Link->Send(new RejectMessage(RejectReason::InsufficientFee, "tx", "insufficient fee"));
				break;
			case CoinErr::ContainsNonFinalTx:
				if (dynamic_cast<TxMessage*>(m))
					m->Link->Send(new RejectMessage(RejectReason::NonStandard, "tx", "non-final"));
				else
					Misbehaving(m, 10, "block", "bad-txns-nonfinal");
				break;
			case CoinErr::VersionMessageMustBeFirst:
				Misbehaving(m, 1, "", "");
				throw;
			case CoinErr::TxNotFound:
			case CoinErr::AllowedErrorDuringInitialDownload:
				break;
			case CoinErr::RecentCoinbase:			//!!!? not misbehavig
			case CoinErr::AlertVerifySignatureFailed:
				break;
			case CoinErr::IncorrectProofOfWork:
				goto LAB_MIS;
			case CoinErr::DupVersionMessage:
				m->Link->Send(new RejectMessage(RejectReason::Duplicate, "version", "Duplicate version message"));
			case CoinErr::VeryBigPayload:
			default:
				TRC(2, ex.what());
	LAB_MIS:
				Misbehaving(m, 1);
			}
		}
	}
}

void CoinEng::OnPingTimeout(P2P::Link& link) {
	CCoinEngThreadKeeper engKeeper(this);
	link.Send(CreatePingMessage());
}

size_t CoinEng::GetMessageHeaderSize() {
	return sizeof(SCoinMessageHeader);
}

size_t CoinEng::GetMessagePayloadSize(const ConstBuf& buf) {
	DBG_LOCAL_IGNORE_CONDITION(ExtErr::Protocol_Violation);

	const SCoinMessageHeader& hdr = *(SCoinMessageHeader*)buf.P;
	if (le32toh(hdr.Magic) != ChainParams.ProtocolMagic)
		Throw(ExtErr::Protocol_Violation);
	return le32toh(hdr.PayloadSize) + sizeof(uint32_t);
//!!!R?	if (strcmp(hdr.Command, "version") && strcmp(hdr.Command, "verack"))
}

ptr<P2P::Message> CoinEng::RecvMessage(Link& link, const BinaryReader& rd) {
	CCoinEngThreadKeeper engKeeper(this);

	uint32_t magic = rd.ReadUInt32();
	if (magic != ChainParams.ProtocolMagic)
		Throw(ExtErr::Protocol_Violation);

	DBG_LOCAL_IGNORE_CONDITION(CoinErr::Misbehaving);
	ptr<CoinMessage> m = CoinMessage::ReadFromStream(link, rd);		
	if (m) {
		TRC(6, ChainParams.Symbol << " " << link.Peer->get_EndPoint() << " " << *m);
	}

	return m.get();
}

Block CoinEng::GetPrevBlockPrefixSuffixFromMainTree(const Block& block) {
	Block r(nullptr);
	if (EXT_LOCKED(Caches.Mtx, Lookup(Caches.HashToBlockCache, block.PrevBlockHash, r)))
		return r;
	return Db->FindBlockPrefixSuffix(block.Height-1);
}

CoinPeer *CoinEng::CreatePeer() {
	return new CoinPeer;
}

void CoinEng::Push(const Inventory& inv) {
	EXT_LOCK (MtxPeers) {
		for (CLinks::iterator it=begin(Links); it!=end(Links); ++it)
			(static_cast<CoinLink*>((*it).get()))->Push(inv);		
	}
}

void CoinEng::Push(const Tx& tx) {
	EXT_LOCK (MtxPeers) {
		for (CLinks::iterator it=begin(Links); it!=end(Links); ++it) {
			CoinLink& clink = *static_cast<CoinLink*>((*it).get());
			if (clink.RelayTxes && EXT_LOCKED(clink.MtxFilter, !clink.Filter || clink.Filter->IsRelevantAndUpdate(tx)))
				clink.Push(Inventory(InventoryType::MSG_TX, Hash(tx)));
		}
	}
}

bool CoinEng::IsFromMe(const Tx& tx) {
	return m_iiEngEvents ? m_iiEngEvents->IsFromMe(tx) : false;
}

void CoinEng::Relay(const Tx& tx) {
	HashValue hash = Hash(tx);
	
	TRC(2, hash);

	if (!tx.IsCoinBase() && !HaveTxInDb(hash)) {
		EXT_LOCKED(Caches.Mtx, Caches.m_relayHashToTx.insert(make_pair(hash, tx)));
		Push(tx);
	}
}

Blob CoinEng::SpendVectorToBlob(const vector<bool>& vec) {
	MemoryStream stm;
	byte v = 0;
	int i;
	for (i=0; i<vec.size(); ++i) {
		v |= vec[i] << (i & 7);
		if ((i & 7) == 7) {
			stm.WriteBuffer(&v, 1);
			v = 0;
		}
	}
	if ((i & 7) != 0)
		stm.WriteBuffer(&v, 1);
	return stm;
}

CoinEngApp::CoinEngApp() {
	m_internalName = VER_INTERNALNAME_STR;

#if !UCFG_WCE
	String coinAppData = Environment::GetEnvironmentVariable("COIN_APPDATA");
	if (!coinAppData.empty())
		m_appDataDir = ToPath(coinAppData);
#endif

#if UCFG_TRC //!!!D
	if (!CTrace::s_nLevel)
		CTrace::s_nLevel = 0x3F;
	CTrace::SetOStream(new TraceStream(get_AppDataDir() / "coin.log"));
	TRC(0, "Starting");
#endif
}

Version CoinEngApp::get_ProductVersion() {
	const int ar[] = { VER_PRODUCTVERSION };
	return Version(ar[0], ar[1]);
}

CoinEngApp theApp;

void SetUserVersion(SqliteConnection& db, const Version& ver) {
	Version v = ver==Version() ? theApp.ProductVersion : ver;
	db.ExecuteNonQuery(EXT_STR("PRAGMA user_version = " << ((v.Major << 16) | v.Minor)));
}

const Version
	VER_NORMALIZED_KEYS(0, 30),
	VER_KEYS32(0, 35),
	VER_INDEXED_PEERS(0, 35),
	VER_USE_HASH_TABLE_DB(0, 81);


void CoinEng::UpgradeDb(const Version& ver) {
}

void CoinEng::UpgradeTo(const Version& ver) {
	Db->UpgradeTo(ver);
}

void CoinEng::TryUpgradeDb() {
	Db->TryUpgradeDb(theApp.ProductVersion);
}

path CoinEng::GetBootstrapPath() {
	if (Mode != EngMode::Bootstrap)
		Throw(errc::invalid_argument);
	return AfxGetCApp()->get_AppDataDir() / ToPath(ChainParams.Symbol) / ToPath(ChainParams.Symbol + ".bootstrap.dat");
}

path CoinEng::VGetDbFilePath() {
	path r = AfxGetCApp()->get_AppDataDir() / ToPath(ChainParams.Name);
	if (!m_cdb.FilenameSuffix.empty())
		r += m_cdb.FilenameSuffix.c_str();
	else  {
		switch (Mode) {
		case EngMode::Lite:
   			r += ".spv";
   			break;
		case EngMode::BlockParser:
   			r += ".blocks";
   			break;
   		case EngMode::BlockExplorer:
   			r = AfxGetCApp()->get_AppDataDir() / ToPath(ChainParams.Symbol) / ToPath(ChainParams.Symbol + ".explorer");
   			break;
   		case EngMode::Bootstrap:
   			r = AfxGetCApp()->get_AppDataDir() / ToPath(ChainParams.Symbol) / ToPath(ChainParams.Symbol + ".bootstrap-index");
   			break;
   		}
   	}
	return r;
}

path CoinEng::GetDbFilePath() {
	if (m_dbFilePath.empty()) {
		m_dbFilePath = VGetDbFilePath();
		m_dbFilePath += ToPath(Db->DefaultFileExt);
	}
	return m_dbFilePath;
}

bool CoinEng::OpenDb() {
	return Db->Open(GetDbFilePath());
}

void CoinEng::ContinueLoad0() {
	m_cdb.Load();
	if (m_cdb.m_dbPeers) {	
		EXT_LOCK (m_cdb.MtxDbPeers) {
			SqliteCommand cmd("SELECT id FROM nets WHERE name=?", m_cdb.m_dbPeers);
			DbDataReader dr = cmd.Bind(1, ChainParams.Name).ExecuteReader();
			if (dr.Read()) 
				m_idPeersNet = dr.GetInt32(0);
			else {
				SqliteCommand("INSERT INTO nets (name) VALUES (?)", m_cdb.m_dbPeers)
					.Bind(1, ChainParams.Name)
					.ExecuteNonQuery();
				m_idPeersNet = (int)m_cdb.m_dbPeers.LastInsertRowId;
			}

			SqliteCommand cmdPeers("SELECT endpoints.ip, port, lastlive, services FROM endpoints JOIN peers ON endpoints.ip=peers.ip WHERE NOT peers.ban AND netid=?", m_cdb.m_dbPeers);
			for (DbDataReader dr=cmdPeers.Bind(1, m_idPeersNet).ExecuteReader(); dr.Read();) {
				if (ptr<Peer> peer = Add(IPEndPoint(IPAddress(dr.GetBytes(0)), (uint16_t)dr.GetInt32(1)), (uint64_t)dr.GetInt64(3), DateTime::from_time_t(dr.GetInt32(2))))
					peer->IsDirty = false;
			}
		}
	}
	//!!!	ListeningPort = ChainParams.DefaultPort;
}

void CoinEng::LoadLastBlockInfo() {
	int hMax = Db->GetMaxHeight(),
		hHeaderMax = Db->GetMaxHeaderHeight();
	if (hMax >= 0) {
		SetBestBlock(GetBlockByHeight(hMax));
		Caches.DtBestReceived = BestBlock().Timestamp;

		TRC(2, "BestBlock : " << hMax << " " << Hash(BestBlock()));
	}
	if (hHeaderMax >= 0) {
		SetBestHeader(Db->FindHeader(hHeaderMax));

		for (size_t i=ChainParams.Checkpoints.size(); i--;) {
			const pair<int, HashValue>& cp = ChainParams.Checkpoints[i];
			if (cp.first <= hHeaderMax) {
				ASSERT(Hash(Db->FindHeader(cp.first)) == cp.second);		//!!! rare case when checkpoint set of new client contraverses saved fork. TODO solution: clear DB here
				Tree.HeightLastCheckpointed = cp.first;
				break;
			}
		}

		TRC(2, "BestHeader: " << hHeaderMax << " " << Hash(BestHeader()));
	}
}

void CoinEng::ContinueLoad() {
	LoadLastBlockInfo();
}

void CoinEng::Load() {
	CCoinEngThreadKeeper engKeeper(this);

	path dbFilePath = GetDbFilePath();
	create_directories(dbFilePath.parent_path());
	bool bContinue;
	OffsetInBootstrap = 0;
	if (exists(dbFilePath)) {
		bContinue = OpenDb();
		if (bContinue)
			TryUpgradeDb();
	} else
		bContinue = CreateDb();

	ContinueLoad0();
	if (bContinue)
		ContinueLoad();
}

void CoinEng::Start() {
	if (Runned)
		return;

	EXT_LOCKED(m_cdb.MtxDb, Filter = Mode==EngMode::Lite ? m_cdb.Filter : nullptr);

	Net::Start();
	EXT_LOCKED(m_cdb.MtxNets, m_cdb.m_nets.push_back(this));

	if (Mode == EngMode::Bootstrap) {
		path pathBootstrap = GetBootstrapPath();
		TRC(2, "Checking for " << pathBootstrap);
		if (exists(pathBootstrap) && Db->GetBoostrapOffset() < file_size(pathBootstrap))
			(new BootstrapDbThread(_self, pathBootstrap))->Start();							// uses field CoinEng.Runned. Must be called after base::Start()
	}

	StartDns();

	if (m_cdb.ProxyString.ToUpper() != "TOR")
		StartIrc();

	DateTime now = Clock::now();
	Ext::Random rng;
#ifdef X_DEBUG//!!!T
	ChainParams.AddSeedEndpoint("192.168.0.2");
#endif
	EXT_FOR(const IPEndPoint& ep, ChainParams.Seeds) {
		Add(ep, uint64_t(NodeServices::NODE_NETWORK), now-TimeSpan::FromDays(7 + rng.Next(7)));
	}
}

void CoinEng::SignalStop() {
	Runned = false;
	EXT_LOCK(m_mtxThreadStateChange) {
		m_tr.interrupt_all();
	}
}

void CoinEng::Stop() {
	if (Runned)
		SignalStop();
	EXT_LOCK(m_cdb.MtxNets) {
		Ext::Remove(m_cdb.m_nets, this);
	}
	if (ChannelClient)
		m_cdb.IrcManager.DetachChannelClient(exchange(ChannelClient, nullptr));
	EXT_LOCK(m_mtxThreadStateChange) {
		m_tr.join_all();
	}
	Close();
}

void GetDataMessage::Process(P2P::Link& link) {
	CoinEng& eng = static_cast<CoinEng&>(*link.Net);
	base::Process(link);
	CoinLink& clink = static_cast<CoinLink&>(link);
	
	ptr<NotFoundMessage> mNotFound;
	EXT_FOR (const Inventory& inv, Invs) {
		if (Thread::CurrentThread->m_bStop)
			break;
		switch (inv.Type) {
		case InventoryType::MSG_BLOCK:
		case InventoryType::MSG_FILTERED_BLOCK:
			if (eng.Mode == EngMode::Lite
#ifdef _UCFG_DEBUG //!!!
				|| eng.Mode == EngMode::Normal			// Low performance
#endif
				)
				break;

			{
				if (Block block = eng.Tree.FindBlock(inv.HashValue)) {
			//		ASSERT(EXT_LOCKED(eng.Mtx, (block.m_pimpl->m_hash.reset(), Hash(block)==inv.HashValue)));

					if (InventoryType::MSG_BLOCK == inv.Type) {
						TRC(2, "Block Message sending");

						link.Send(new BlockMessage(block));
					} else {
						ptr<MerkleBlockMessage> mbm;
						vector<Tx> matchedTxes = EXT_LOCKED(clink.MtxFilter, clink.Filter ? (mbm = new MerkleBlockMessage)->Init(block, *clink.Filter) : vector<Tx>());
						if (mbm) {
							link.Send(mbm);
							EXT_LOCK(clink.Mtx) {
								for (size_t i=matchedTxes.size(); i--;)
									if (clink.KnownInvertorySet.count(Inventory(InventoryType::MSG_TX, Hash(matchedTxes[i]))))
										matchedTxes.erase(matchedTxes.begin()+i);
							}
							EXT_FOR(const Tx& tx, matchedTxes) {
								link.Send(new TxMessage(tx));
							}
						}
					}
					
					if (inv.HashValue == clink.HashContinue) {
						ptr<InvMessage> m = new InvMessage;
						m->Invs.push_back(Inventory(InventoryType::MSG_BLOCK, Hash(eng.BestBlock())));
						link.Send(m);
						clink.HashContinue = HashValue::Null();
					}			
				}
			}
			break;
		case InventoryType::MSG_TX:
			{
				ptr<TxMessage> m;
				EXT_LOCK (eng.Caches.Mtx) {
					ChainCaches::CRelayHashToTx::iterator it = eng.Caches.m_relayHashToTx.find(inv.HashValue);
					if (it != eng.Caches.m_relayHashToTx.end())
						m = new TxMessage(it->second.first);
				}
				if (m)
					link.Send(m);
				else {
					if (!mNotFound)
						mNotFound = new NotFoundMessage;
					mNotFound->Invs.push_back(inv);
				}
			}
			break;
		}
	}
	if (mNotFound)
		link.Send(mNotFound);
}

void NotFoundMessage::Process(P2P::Link& link) {
	CoinEng& eng = Eng();
	if (eng.Mode==EngMode::Lite) {
		//!!!TODO
	}
}

void CoinPartialMerkleTree::Init(const vector<HashValue>& vHash, const dynamic_bitset<byte>& bsMatch) {
	Bitset.clear();
	Items.clear();
    TraverseAndBuild(CalcTreeHeight(), 0, &vHash[0], bsMatch);
}

pair<HashValue, vector<HashValue>> CoinPartialMerkleTree::ExtractMatches() const {
	if (NItems==0 || NItems>MAX_BLOCK_SIZE/60 || Items.size()>NItems)
		Throw(CoinErr::Misbehaving);
	size_t nBitsUsed = 0;
	int nHashUsed = 0;
	pair<HashValue, vector<HashValue>> r;
	r.first = TraverseAndExtract(CalcTreeHeight(), 0, nBitsUsed, nHashUsed, r.second);
	return r;
}

DateTime CoinEng::GetTimestampForNextBlock() {
	int64_t epoch = to_time_t(std::max(BestBlock().GetMedianTimePast()+TimeSpan::FromSeconds(1), Clock::now()));		// round to seconds
	return DateTime::from_time_t(epoch);
}

double CoinEng::ToDifficulty(const Target& target) {
	BigInteger n = ChainParams.MaxPossibleTarget,
			   d = target;
	int nlen = n.Length,
		dlen = d.Length,
		exp = nlen - (dlen+DBL_MANT_DIG);
	if (exp > 0) {
		d <<= exp;
	} else {
		n <<= -exp;
	}
	BigInteger r = n/d;
	double difficult = ::ldexp((double)r.AsDouble() , exp);
	return difficult;
}

int64_t CoinEng::GetSubsidy(int height, const HashValue& prevBlockHash, double difficulty, bool bForCheck) {
	return ChainParams.InitBlockValue >> (height/ChainParams.HalfLife);
}

void CoinEng::CheckCoinbasedTxPrev(int height, const Tx& txPrev) {
	if (height-txPrev.Height < ChainParams.CoinbaseMaturity)
		Throw(CoinErr::RecentCoinbase);
}

void IBlockChainDb::Recreate() {
	CoinEng& eng = Eng();	
	Close(false);
	sys::remove(eng.GetDbFilePath());
	eng.CreateDb();
}

void IBlockChainDb::UpdateCoins(const OutPoint& op, bool bSpend) {
	vector<bool> vSpend = GetCoinsByTxHash(op.TxHash);
	if (!bSpend)
		vSpend.resize(std::max(vSpend.size(), size_t(op.Index+1)));
	else if (op.Index >= vSpend.size() || !vSpend[op.Index])
		Throw(CoinErr::InputsAlreadySpent);
	vSpend[op.Index] = !bSpend;
	SaveCoinsByTxHash(op.TxHash, vSpend);
}

Version CheckUserVersion(SqliteConnection& db) {
	Version ver = theApp.ProductVersion;
	SqliteCommand cmd("PRAGMA user_version", db);
	int dbver = (int)cmd.ExecuteInt64Scalar();
	Version dver = Version(dbver >> 16, dbver & 0xFFFF);
	if (dver > ver && (dver.Major!=ver.Major || dver.Minor/10 != ver.Minor/10)) {					// Versions [mj.x0 .. mj.x9] are DB-compatible
		db.Close();
		throw VersionException(dver);
	}
	return dver;
}

CEngStateDescription::CEngStateDescription(CoinEng& eng, RCString s)
	: m_eng(eng)
	, m_s(s) {
	TRC(2, s);

	EXT_LOCKED(m_eng.m_mtxStates, m_eng.m_setStates.insert(s));
	if (m_eng.m_iiEngEvents)
		m_eng.m_iiEngEvents->OnChange();
}

CEngStateDescription::~CEngStateDescription() {
	EXT_LOCKED(m_eng.m_mtxStates, m_eng.m_setStates.erase(m_s));
	if (m_eng.m_iiEngEvents)
		m_eng.m_iiEngEvents->OnChange();
}

} // Coin::

