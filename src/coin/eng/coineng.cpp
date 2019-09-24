/*######   Copyright (c) 2011-2019 Ufasoft  http://ufasoft.com  mailto:support@ufasoft.com,  Sergey Pavlov  mailto:dev@ufasoft.com ####
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
#	pragma comment(lib, "inet")
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
#	endif // !defined(_AFXDLL)
#endif	 // _MSC_VER

using Ext::DB::DbException;

template <> Coin::CurrencyFactoryBase* StaticList<Coin::CurrencyFactoryBase>::Root = 0;

namespace Coin {

CoinEng* CurrencyFactoryBase::CreateEng(CoinDb& cdb) {
	return new CoinEng(cdb);
}

void ChainParams::Init() {
	IsTestNet = false;
	HashAlgo = Coin::HashAlgo::Sha256;
	ProtocolVersion = ProtocolVersion::PROTOCOL_VERSION;
	DiskMagic = 0;
	AuxPowStartBlock = INT_MAX;
	CoinValue = 100000000;
	InitBlockValue = CoinValue * 50;
	MaxMoney = 21000000 * CoinValue;
	HalfLife = 210000;
	AnnualPercentageRate = 0;
	MinTxFee = CoinValue / 10000;
    DustRelayFee = DUST_RELAY_TX_FEE;
	BlockSpan = seconds(600);
	TargetInterval = 2016;
	MinTxOutAmount = 0;
	MaxPossibleTarget = Target(0x1D7FFFFF);
	InitTarget = MaxPossibleTarget;
	CoinbaseMaturity = COINBASE_MATURITY;
	MaxBlockWeight = MAX_BLOCK_WEIGHT;
	PowOfDifficultyToHalfSubsidy = 1;
	PayToScriptHashHeight = numeric_limits<int>::max();
	BIP34Height = BIP65Height = BIP66Height = BIP68Height = SegwitHeight = 0;
	ScriptAddressVersion = 5;
	CheckDupTxHeight = numeric_limits<int>::max();
	Listen = true;
	MedianTimeSpan = 11;
	AllowLiteMode = true;
	MiningAllowed = false;
}

Blob EncryptedPrivKey(BuggyAes& aes, const KeyInfo& key) {
	uint8_t typ = DEFAULT_PASSWORD_ENCRYPT_METHOD;
	const PrivateKey& privKey = key.PrivKey;
	return Blob(&typ, 1) + aes.Encrypt(privKey + Crc32().ComputeHash(privKey));
}

thread_local EnabledFeatures t_features;
thread_local ScriptPolicy t_scriptPolicy;

void EngEvents::OnCreateDatabase() {
	for (auto& s : Subscribers)
		s->OnCreateDatabase();
}

void EngEvents::OnCloseDatabase() {
	for (auto& s : Subscribers)
		s->OnCloseDatabase();
}

void EngEvents::OnBestBlock(const Block& block) {
	for (auto& s : Subscribers)
		s->OnBestBlock(block);
}

void EngEvents::OnProcessBlock(const BlockHeader& block) {
	for (auto& s : Subscribers)
		s->OnProcessBlock(block);
}

void EngEvents::OnBlockConnectDbtx(const Block& block) {
	for (auto& s : Subscribers)
		s->OnBlockConnectDbtx(block);
}

void EngEvents::OnBeforeCommit() {
	for (auto& s : Subscribers)
		s->OnBeforeCommit();
}

void EngEvents::OnProcessTx(const Tx& tx) {
	for (auto& s : Subscribers)
		s->OnProcessTx(tx);
}

void EngEvents::OnEraseTx(const HashValue& hashTx) {
	for (auto& s : Subscribers)
		s->OnEraseTx(hashTx);
}

void EngEvents::OnBlockchainChanged() {
	for (auto& s : Subscribers)
		s->OnBlockchainChanged();
}

void EngEvents::OnSetProgress(float v) {
	for (auto& s : Subscribers)
		s->OnSetProgress(v);
}

void EngEvents::OnChange() {
	for (auto& s : Subscribers)
		s->OnChange();
}

void EngEvents::OnAlert(Alert * alert) {
	for (auto& s : Subscribers)
		s->OnAlert(alert);
}

void EngEvents::OnPeriodic(const DateTime& now) {
	for (auto& s : Subscribers)
		s->OnPeriodic(now);
}

void EngEvents::OnPeriodicForLink(Link & link) {
	for (auto& s : Subscribers)
		s->OnPeriodicForLink(link);
}

bool EngEvents::IsFromMe(const Tx& tx) {
	for (auto& s : Subscribers)
		if (s->IsFromMe(tx))
			return true;
	return false;
}

void EngEvents::AddRecipient(const Address& a) {
	for (auto& s : Subscribers)
		s->AddRecipient(a);
}

CoinEngTransactionScope::CoinEngTransactionScope(CoinEng& eng)
	: Eng(eng)
	, m_db(Eng.Db->GetSavepointObject())
{
	Eng.BeginTransaction();
	m_db.BeginTransaction();
}

CoinEngTransactionScope::~CoinEngTransactionScope() {
	if (!m_bCommitted) {
		if (InException) {
			m_db.Rollback();
			Eng.Rollback();
		} else {
			m_db.Commit();
			Eng.Commit();
		}
	}
}

COIN_EXPORT CoinEng& ExternalEng() {
	return *dynamic_cast<CoinEng*>(HasherEng::GetCurrent());
}

CCoinEngThreadKeeper::CCoinEngThreadKeeper(CoinEng* cur, ScriptPolicy *pScriptPolicy, bool enableFeatures)
	: base(cur) {
	//	m_prev = t_pCoinEng;
	m_prevFeatures = t_features;
	m_prevScriptPolicy = t_scriptPolicy;
	//	t_pCoinEng = cur;
	t_features = EnabledFeatures(enableFeatures);
	t_scriptPolicy = pScriptPolicy ? *pScriptPolicy : ScriptPolicy(false);
}

CCoinEngThreadKeeper::~CCoinEngThreadKeeper() {
	//	t_pCoinEng = m_prev;
	t_scriptPolicy = m_prevScriptPolicy;
	t_features = m_prevFeatures;
}

CoinEng::CoinEng(CoinDb& cdb)
	: base(cdb)
	, Tree(_self)
	, TxPool(*this)
	, m_cdb(cdb)
	, MaxBlockVersion(3)
	, m_dtLastFreeTx(Clock::now())
	, m_freeCount(0)
	, CommitPeriod(0x1F)
	, m_mode(EngMode::Normal)
	, AllowFreeTxes(true)
	, UpgradingDatabaseHeight(0)
	, Rescanning(false)
	, OffsetInBootstrap(0)
	, NextOffsetInBootstrap(0)
	, aPreferredDownloadPeers(0)
	, aSyncStartedPeers(0)
	, MaxOpcode(Opcode::OP_NOP10)
{
	StallingTimeout = BLOCK_STALLING_TIMEOUT;
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
			Events.OnBeforeCommit();

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

String CoinEng::BlockStringId(const HashValue& hashBlock) {
	return EXT_STR(Db->FindHeight(hashBlock) << "/" << hashBlock);
}

HashValue CoinEng::HashMessage(RCSpan cbuf) {
	return Coin::Hash(cbuf);
}

HashValue CoinEng::HashForWallet(RCSpan s) {
	return Coin::Hash(s);
}

HashValue CoinEng::HashForSignature(RCSpan cbuf) {
	return Coin::Hash(cbuf);
}

HashValue CoinEng::HashFromTx(const Tx& tx, bool widnessAware) {
    MemoryStream stm;
	ProtocolWriter wr(stm);
	wr.WitnessAware = widnessAware;
    tx.Write(wr);
	return Coin::Hash(stm);
}

void CoinEng::Close() {
	TRC(1, "Closing " << GetDbFilePath());

	CCoinEngThreadKeeper keeper(this);
	CommitTransactionIfStarted();

	Events.OnCloseDatabase();

	Db->Close();
}

ptr<IBlockChainDb> CoinEng::CreateBlockChainDb() {
#if UCFG_COIN_COINCHAIN_BACKEND == COIN_BACKEND_DBLITE
	return new DbliteBlockChainDb(_self);
#else
	return new SqliteBlockChainDb(_self);
#endif
}

void CoinEng::ClearByHeightCaches() {
	EXT_LOCKED(Caches.Mtx, Caches.HeightToHashCache.clear());
}

Block CoinEng::GetBlockByHeight(uint32_t height) {
	EXT_LOCK(Caches.Mtx) {
		ChainCaches::CHeightToHashCache::iterator it = Caches.HeightToHashCache.find(height);
		if (it != Caches.HeightToHashCache.end())
			if (auto oblock = Lookup(Caches.HashToBlockCache, it->second.first))
				return oblock.value();
	}
	Block block = Db->FindBlock(height);

#ifdef X_DEBUG //!!!D
	HashValue h1 = Hash(block);
	block->m_hash.reset();
	ASSERT(h1 == Hash(block));
#endif

	ASSERT(block.Height == height);
	HashValue hashBlock = Hash(block);
	EXT_LOCK(Caches.Mtx) {
		Caches.HeightToHashCache.insert(make_pair((ChainCaches::CHeightToHashCache::key_type)height, hashBlock));
		Caches.HashToBlockCache.insert(make_pair(hashBlock, block));
	}
	return block;
}

BlockHeader CoinEng::FindHeader(const HashValue& hash) {
	EXT_LOCK(Caches.Mtx) {
		if (auto ob = Lookup(Caches.HashToBlockCache, hash))
			return ob.value();
		ChainCaches::COrphanMap::iterator it = Caches.OrphanBlocks.find(hash);
		if (it != Caches.OrphanBlocks.end())
			return it->second.first;
	}
	return Tree.FindHeader(hash);
}

Block CoinEng::LookupBlock(const HashValue& hash) {
	EXT_LOCK(Caches.Mtx) {
		if (auto ob = Lookup(Caches.HashToBlockCache, hash))
			return ob.value();
	}
	Block r(nullptr);
	if (r = Db->FindBlock(hash)) {
		EXT_LOCK(Caches.Mtx) {
			Caches.HeightToHashCache.insert(make_pair((ChainCaches::CHeightToHashCache::key_type)r.Height, hash));
			Caches.HashToBlockCache.insert(make_pair(hash, r));
		}
	}
	return r;
}

bool CoinEng::CreateDb() {
	bool r = Db->Create(GetDbFilePath());
	OffsetInBootstrap = 0;

	if (Mode == EngMode::Lite) {
		CoinEngTransactionScope scopeBlockSavepoint(_self);
		Db->SetFilter(m_cdb.Filter);
	}

	TransactionScope dbtx(*Db);
	Events.OnCreateDatabase();
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
    ReadParam(BIP34Height, xml, "BIP34Height");
    ReadParam(BIP65Height, xml, "BIP65Height");
	ReadParam(BIP66Height, xml, "BIP66Height");
	ReadParam(BIP68Height, xml, "BIP68Height");
    ReadParam(SegwitHeight, xml, "SegwitHeight");

	String a;

	if (!(a = xml.GetAttribute("BlockSpan")).empty())
		BlockSpan = seconds(stoi(a));

	if (!(a = xml.GetAttribute("InitBlockValue")).empty())
		InitBlockValue = CoinValue * stoll(a);

	if (!(a = xml.GetAttribute("MaxMoney")).empty())
		MaxMoney = stoull(a) * CoinValue;

	if (!(a = xml.GetAttribute("PowOfDifficultyToHalfSubsidy")).empty())
		PowOfDifficultyToHalfSubsidy = 1.0 / stoi(a);

	a = xml.GetAttribute("MaxTargetHex");
	MaxTarget = !a.empty() ? Target(Convert::ToUInt32(a, 16)) : Target(0x1D00FFFF);
	HashValueMaxTarget = HashValue::FromDifficultyBits(MaxTarget.m_value);

	if (!(a = xml.GetAttribute("InitTargetHex")).empty())
		InitTarget = Target(Convert::ToUInt32(a, 16));

	ProtocolMagic = Convert::ToUInt32(xml.GetAttribute("ProtocolMagicHex"), 16);
	DiskMagic = (a = xml.GetAttribute("DiskMagicHex")).empty() ? ProtocolMagic : Convert::ToUInt32(a, 16);

	if (!(a = xml.GetAttribute("ProtocolVersion")).empty())
		ProtocolVersion = Convert::ToUInt32(a);

	a = xml.GetAttribute("DefaultPort");
	DefaultPort = !a.empty() ? Convert::ToUInt16(a) : 8333;

	BootUrls = xml.GetAttribute("BootUrls").Split();

	a = xml.GetAttribute("IrcRange");
	IrcRange = !a.empty() ? stoi(a) : 0;

	a = xml.GetAttribute("AddressVersion");
	AddressVersion = !a.empty() ? Convert::ToByte(a) : (IsTestNet ? 111 : 0);

	a = xml.GetAttribute("Hrp");
	Hrp = !a.empty() ? a : (IsTestNet ? "bc" : "tb");

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
	Seeds.insert(regex_match(seed.c_str(), ReIp4Port) ? IPEndPoint::Parse(seed) : IPEndPoint(IPAddress::Parse(seed), DefaultPort));
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
		doc.LoadXml(xml); // LoadXml() instead of Load() to eliminate loading shell32.dll
		XmlNodeReader rd(doc);
#else
		istringstream is(xml.c_str());
		XmlTextReader rd(is);
#endif
		for (bool b = rd.ReadToDescendant("Chain"); b; b = rd.ReadToNextSibling("Chain")) {
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
								EXT_FOR(const String& seed, ss)
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
	for (CurrencyFactoryBase* p = CurrencyFactoryBase::Root; p; p = p->Next)
		if (p->Name == name)
			peng = p->CreateEng(cdb);
	(peng ? peng : (peng = new CoinEng(cdb)))->SetChainParams(params);
	const Coin::ChainParams& p = peng->ChainParams;
	peng->AddressVersion = p.AddressVersion;
	peng->ScriptAddressVersion = p.ScriptAddressVersion;
	peng->Hrp = p.Hrp;
	peng->ProtocolMagic = p.ProtocolMagic;
	peng->Listen = p.Listen;
	peng->DefaultPort = p.DefaultPort;
	return peng;
}

class DnsThread : public Thread {
	typedef Thread base;

public:
	CoinEng& Eng;
	vector<String> DnsServers;

	DnsThread(CoinEng& eng, const vector<String>& dnsServers)
		: base(&eng.m_tr)
		, Eng(eng)
		, DnsServers(dnsServers) {
	}

protected:
	void Execute() override {
		Name = "DnsThread";

		DBG_LOCAL_IGNORE_WIN32(WSAHOST_NOT_FOUND);
		DBG_LOCAL_IGNORE_WIN32(WSATRY_AGAIN);

		DateTime date = Clock::now() - TimeSpan::FromDays(1);
		EXT_FOR(const String& dns, DnsServers) {
			if (m_bStop)
				break;
			try {
				IPHostEntry he = Dns::GetHostEntry(dns);
				EXT_FOR(const IPAddress& a, he.AddressList) {
					IPEndPoint ep = IPEndPoint(a, Eng.ChainParams.DefaultPort);
					//					TRC(2, ep);
					Eng.Add(ep, NodeServices::NODE_NETWORK, date);
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
	default_random_engine rng(Rand());
	shuffle(ar.begin(), ar.end(), rng);
	for (int i = 0; i < ar.size(); ++i) {
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

BlockHeader CoinEng::BestBlock() {
	EXT_LOCK(Caches.Mtx) {
		return Caches.m_bestBlock;
	}
}

int CoinEng::BestBlockHeight() {
	return EXT_LOCKED(Caches.Mtx, Caches.m_bestBlock.SafeHeight);
}

void CoinEng::SetBestBlock(const BlockHeader& b) {
	EXT_LOCK(Caches.Mtx) {
		Caches.m_bestBlock = b;
		if (b.SafeHeight > Caches.m_bestHeader.SafeHeight)
			Caches.m_bestHeader = b;
	}
}

void CoinEng::SetBestBlock(const Block& b) {
	EXT_LOCK(Caches.Mtx) {
		Caches.m_bestBlock = b;
		if (b.SafeHeight > Caches.m_bestHeader.SafeHeight)
			Caches.m_bestHeader = b;
	}
	if (b && !IsInitialBlockDownload()) {
		Events.OnBestBlock(b);
	}
}

void CoinEng::put_Mode(EngMode mode) {
	if (!ChainParams.AllowLiteMode && mode == EngMode::Lite)
		Throw(errc::invalid_argument);

#if !UCFG_COIN_USE_NORMAL_MODE
	if (mode == EngMode::Normal || mode == EngMode::BlockExplorer)
		Throw(errc::not_supported);
#endif
	bool bRunned = Runned;
	if (bRunned)
		Stop();
	SetBestHeader(nullptr);
	SetBestBlock(Block(nullptr));
	Tree.Clear();
	m_mode = mode;
	Filter = nullptr;
	m_dbFilePath = "";
	if (bRunned) {
		Load();
		Start();
	}
	TRC(0, "Switched " << ChainParams.Symbol << " to " << (mode == EngMode::Lite ? "Lite" : "Bootstrap") << " mode");
}

void CoinEng::PurgeDatabase() {
	bool bRunned = Runned;
	if (bRunned)
		Stop();
	Db->Close(false);
	SetBestHeader(nullptr);
	SetBestBlock(Block(nullptr));
	filesystem::remove(GetDbFilePath());
	Filter = nullptr;
	if (bRunned) {
		Load();
		Start();
	}
}

void CoinEng::AddLink(P2P::LinkBase* link) {
	base::AddLink(link);
	Events.OnChange();
}

void CoinEng::OnCloseLink(P2P::LinkBase& link) {
	Link& clink = static_cast<Link&>(link);
	base::OnCloseLink(link);
	RemoveVerNonce(clink);
	aPreferredDownloadPeers -= clink.IsPreferredDownload;
	aSyncStartedPeers -= clink.IsSyncStarted;
	EXT_LOCK(Mtx) {
		for (QueuedBlockItem& qbi : clink.BlocksInFlight) {
			MapBlocksInFlight.erase(qbi.HashBlock);
			TRC(5, "Removing from MapBlocksInFlight: " << BlockStringId(qbi.HashBlock));
		}
	}

	Events.OnChange();
}

void CoinEng::OnPeriodicMsgLoop(const DateTime& now) {
	EXT_LOCK(m_mtxThreadStateChange) {
		if (Runned) {
			base::OnPeriodicMsgLoop(now);
			CommitTransactionIfStarted();

			Events.OnPeriodic(now);
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
				m_ev.lock(P2P::PERIODIC_SECONDS * 1000);
			else {
				dtNextPeriodic = now + spanPeriodic;
				EXT_LOCK(m_cdb.MtxNets) {
					for (int i = 0; i < m_cdb.m_nets.size(); ++i) {
						CoinEng& eng = *static_cast<CoinEng*>(m_cdb.m_nets[i]);
						CCoinEngThreadKeeper engKeeper(&eng);
						eng.OnPeriodicMsgLoop(now);
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

void CoinEng::Misbehaving(P2P::Message* m, int howMuch, RCString command, RCString reason) {
	if (!command.empty())
		m->LinkPtr->Send(new RejectMessage(RejectReason::Invalid, command, reason));
	Peer& peer = *m->LinkPtr->Peer;
	if ((peer.Misbehavings += howMuch) > P2P::MAX_PEER_MISBEHAVINGS) {
		m_cdb.BanPeer(peer);
		throw;
	}
}

void CoinEng::OnMessage(P2P::Message* m) {
	if (!Runned)
		return;
	CCoinEngThreadKeeper engKeeper(this);
	P2P::Link& link = *m->LinkPtr;
	try {
		//		DBG_LOCAL_IGNORE_CONDITION(CoinErr::TxPrematureSpend);
		DBG_LOCAL_IGNORE_CONDITION(CoinErr::IncorrectProofOfWork);
		DBG_LOCAL_IGNORE_CONDITION(CoinErr::RejectedByCheckpoint);
		DBG_LOCAL_IGNORE_CONDITION(CoinErr::AlertVerifySignatureFailed);
		DBG_LOCAL_IGNORE_CONDITION(CoinErr::ProofOfWorkFailed);
		DBG_LOCAL_IGNORE_CONDITION(CoinErr::BadPrevBlock);
		DBG_LOCAL_IGNORE_CONDITION(CoinErr::VersionMessageMustBeFirst);

		if (auto cm = dynamic_cast<CoinMessage*>(m))
			cm->Trace((Link&)link, false);

		if (!link.PeerVersion) {
			if (auto reject = dynamic_cast<RejectMessage*>(m)) {
				if (reject->Command == "version" && reject->Code == RejectReason::Obsolete) {
					link.NetManager->BanPeer(*link.Peer);		//!!!? Should find  better way to communicate
					link.Stop();
					return;
				}
			} else if (!dynamic_cast<VersionMessage*>(m))
				Throw(CoinErr::VersionMessageMustBeFirst);
		}
		m->ProcessMsg(link);
	} catch (const DbException&) {
		EXT_LOCK(Mtx) {
			if (CriticalException == std::exception_ptr())
				CriticalException = std::current_exception();
		}
		try {
			Db->Close();
		} catch (RCExc) {
		}
		Events.OnChange();
		throw;
	} catch (const PeerMisbehavingException& ex) {
		TRC(2, "Misbehaving " << ex.what());
		Misbehaving(m, ex.HowMuch);
	} catch (Exception& ex) {
		const error_code& ecode = ex.code();
		if (ecode.category() != coin_category()) {
			TRC(2, ex.what());
			if (ecode == ExtErr::ObsoleteProtocolVersion) {
				link.Send(new RejectMessage(RejectReason::Obsolete, "version", EXT_STR("Version must be " << ProtocolVersion::MIN_PEER_PROTO_VERSION << " or greater")));
				throw;
			}
			Misbehaving(m, 1);
		} else {
			switch ((CoinErr)ecode.value()) {
			case CoinErr::GetBlocksLocatorSize:		// Disconnect on these errors
				throw;
			case CoinErr::SizeLimits:
			case CoinErr::DupTxInputs: Misbehaving(m, 100); break;
			case CoinErr::RejectedByCheckpoint:
				link.Send(new RejectMessage(RejectReason::CheckPoint, "block", "checkpoint mismatch"));
				Misbehaving(m, 100);
				break;
			case CoinErr::MoneyOutOfRange:					Misbehaving(m, 100, "block", "bad-txns-txouttotal-toolarge"); break;
			case CoinErr::BadTxnsVoutNegative:				Misbehaving(m, 100, "block", "bad-txns-vout-negative"); break;
			case CoinErr::BlockHeightMismatchInCoinbase:	Misbehaving(m, 100, "block", "bad-cb-height"); break;
			case CoinErr::FirstTxIsNotCoinbase:				Misbehaving(m, 100, "block", "bad-cb-missing"); break;
			case CoinErr::FirstTxIsNotTheOnlyCoinbase:		Misbehaving(m, 100, "block", "bad-cb-multiple"); break;
			case CoinErr::BadCbLength:						Misbehaving(m, 100, "block", "bad-cb-length"); break;
			case CoinErr::ProofOfWorkFailed:				Misbehaving(m, 50, "block", "high-hash"); break;
			case CoinErr::BadTxnsVinEmpty:					Misbehaving(m, 10, "block", "bad-txns-vin-empty"); break;
			case CoinErr::BadTxnsVoutEmpty:					Misbehaving(m, 10, "block", "bad-txns-vout-empty"); break;
			case CoinErr::BadTxnsPrevoutNull:				Misbehaving(m, 10, "block", "bad-txns-prevout-null"); break;
			case CoinErr::BadPrevBlock:						Misbehaving(m, 10, "block", "bad-prevblk"); break;
			case CoinErr::TxMissingInputs: link.Send(new RejectMessage(RejectReason::TxMissingInputs, "tx", "bad-txns-inputs-missingorspent")); break;
			case CoinErr::TxPrematureSpend: link.Send(new RejectMessage(RejectReason::TxPrematureSpend, "tx", "bad-txns-premature-spend-of-coinbase")); break;
			case CoinErr::TxFeeIsLow: link.Send(new RejectMessage(RejectReason::InsufficientFee, "tx", "insufficient fee")); break;
			case CoinErr::ContainsNonFinalTx:
				if (dynamic_cast<TxMessage*>(m))
					link.Send(new RejectMessage(RejectReason::NonStandard, "tx", "non-final"));
				else
					Misbehaving(m, 10, "block", "bad-txns-nonfinal");
				break;
			case CoinErr::VersionMessageMustBeFirst: Misbehaving(m, 1, "", ""); throw;
			case CoinErr::TxNotFound:
			case CoinErr::AllowedErrorDuringInitialDownload: break;
			case CoinErr::AlertVerifySignatureFailed: break;
			case CoinErr::IncorrectProofOfWork: goto LAB_MIS;
			case CoinErr::DupVersionMessage: link.Send(new RejectMessage(RejectReason::Duplicate, "version", "Duplicate version message"));
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

	Ext::Random rnd;
	uint64_t nonce;
	do {
		rnd.NextBytes(span<uint8_t>((uint8_t*)&nonce, sizeof nonce));
	} while (nonce == 0);
	static_cast<Coin::Link&>(link).PingNonceSent = nonce;
	link.LastPingTimestamp = Clock::now();
	link.Send(new PingMessage(nonce));
}

size_t CoinEng::GetMessageHeaderSize() {
	return sizeof(SCoinMessageHeader);
}

size_t CoinEng::GetMessagePayloadSize(RCSpan buf) {
	DBG_LOCAL_IGNORE_CONDITION(ExtErr::Protocol_Violation);

	const SCoinMessageHeader& hdr = *(SCoinMessageHeader*)buf.data();
	if (le32toh(hdr.Magic) != ChainParams.ProtocolMagic)
		Throw(ExtErr::Protocol_Violation);
	return le32toh(hdr.PayloadSize) + sizeof(uint32_t);
	//!!!R?	if (strcmp(hdr.Command, "version") && strcmp(hdr.Command, "verack"))
}

void CoinEng::OnInitLink(P2P::Link& link) {
	P2P::Net::OnInitLink(link);
	if (!link.Incoming) {
		CCoinEngThreadKeeper engKeeper(this);
		SendVersionMessage(static_cast<Link&>(link));
	}
}

ptr<P2P::Message> CoinEng::RecvMessage(P2P::Link& link, const BinaryReader& rd) {
	CCoinEngThreadKeeper engKeeper(this);
	Link& clink = static_cast<Link&>(link);

	uint32_t magic = rd.ReadUInt32();
	if (magic != ChainParams.ProtocolMagic)
		Throw(ExtErr::Protocol_Violation);

	DBG_LOCAL_IGNORE_CONDITION(CoinErr::Misbehaving);
	ptr<CoinMessage> m = CoinMessage::ReadFromStream(clink, rd);
	if (m) {
		TRC(6, ChainParams.Symbol << " " << link.Peer->get_EndPoint() << " " << *m);
	}

	return m.get();
}

Block CoinEng::GetPrevBlockPrefixSuffixFromMainTree(const Block& block) {
	EXT_LOCK(Caches.Mtx) {
		if (auto ob = Lookup(Caches.HashToBlockCache, block.PrevBlockHash))
			return ob.value();
	}
	return Db->FindBlockPrefixSuffix(block.Height - 1);
}

CoinPeer* CoinEng::CreatePeer() {
	return new CoinPeer;
}

void CoinEng::Push(const Inventory& inv) {
	EXT_LOCK(MtxPeers) {
		for (CLinks::iterator it = begin(Links); it != end(Links); ++it)
			(static_cast<Link*>((*it).get()))->Push(inv);
	}
}

void CoinEng::Push(const TxInfo& txInfo) {
	EXT_LOCK(MtxPeers) {
		for (auto& p : Links) {
			Link& clink = *static_cast<Link*>(p.get());
			if (clink.RelayTxes && txInfo.FeeRatePerKB >= clink.MinFeeRate) {
				EXT_LOCK(clink.MtxFilter) {
					if (txInfo.FeeRatePerKB >= clink.MinFeeRate && (!clink.Filter || clink.Filter->IsRelevantAndUpdate(txInfo.Tx)))
						clink.Push(Inventory(InventoryType::MSG_TX, Hash(txInfo.Tx)));
				}
			}
		}
	}
}

void CoinEng::Broadcast(CoinMessage* m) {
	EXT_LOCK(MtxPeers) {
		for (auto& pLinkBase : Links) {			// early broadcast found Block
			Link& link = static_cast<Link&>(*pLinkBase);
			link.Send(m);
		}
	}
}

bool CoinEng::IsFromMe(const Tx& tx) {
	return Events.IsFromMe(tx);
}

void CoinEng::Relay(const TxInfo& txInfo) {
	HashValue hash = Hash(txInfo.Tx);

	TRC(TRC_LEVEL_TX_MESSAGE, hash);

	if (!txInfo.Tx->IsCoinBase() && !HaveTxInDb(hash)) {
		EXT_LOCKED(Caches.Mtx, Caches.m_relayHashToTx.insert(make_pair(hash, txInfo.Tx)));
		Push(txInfo);
	}
}

Blob CoinEng::SpendVectorToBlob(const vector<bool>& vec) {
	MemoryStream stm;
	uint8_t v = 0;
	int i;
	for (i = 0; i < vec.size(); ++i) {
		v |= vec[i] << (i & 7);
		if ((i & 7) == 7) {
			stm.WriteBuffer(&v, 1);
			v = 0;
		}
	}
	if ((i & 7) != 0)
		stm.WriteBuffer(&v, 1);
	return Span(stm);
}

CoinEngApp::CoinEngApp() {
	m_internalName = VER_INTERNALNAME_STR;

#if !UCFG_WCE
	String coinAppData = Environment::GetEnvironmentVariable("COIN_APPDATA");
	if (!coinAppData.empty())
		m_appDataDir = ToPath(coinAppData);
#endif

	auto appDataDir = get_AppDataDir();
#if UCFG_TRC //!!!D
	if (!CTrace::s_nLevel)
		CTrace::s_nLevel = 0x3F;
	String logDir = Environment::GetEnvironmentVariable("COIN_LOGDIR");
	CTrace::SetOStream(new CycledTraceStream((logDir.empty() ? appDataDir : path(logDir.c_str())) / "coin.log", true));
	TRC(0, "Starting");
#endif
	auto pathExampleConf = appDataDir / (UCFG_COIN_CONF_FILENAME ".example");
	ostringstream osExample;
	osExample << "# Example Configuration file, you may rename it to " << UCFG_COIN_CONF_FILENAME << ", uncomment some options and edit them\n\n";
	g_conf.SaveSample(osExample);
	String sExample = osExample.str();
	if (!exists(pathExampleConf) || file_size(pathExampleConf) != sExample.length()) {
		ofstream ofs(pathExampleConf);
		ofs << sExample;
	}
	if (ifstream ifs = ifstream(appDataDir / UCFG_COIN_CONF_FILENAME))
		g_conf.Load(ifs);

	static regex reArg("^--?([a-z0-9]+)=(\\S+)$");
	bool hasListen = false, hasConnect = false, hasProxy = false;
	for (auto& arg : m_argvp) {							// command-line args can override .conf
		smatch m;
		string sarg(arg);
		if (regex_match(sarg, m, reArg)) {
			String o = m[1];
			hasListen |= (o == "listen");
			hasConnect |= (o == "connect");
			hasProxy |= (o == "proxystring");
			g_conf.AssignOption(o, m[2]);
		}
	}
	if (!hasListen && (hasConnect || hasProxy))
		g_conf.Listen = false;
}

Version CoinEngApp::get_ProductVersion() {
	const int ar[] = {VER_PRODUCTVERSION};
	return Version(ar[0], ar[1]);
}

CoinEngApp theApp;

void SetUserVersion(SqliteConnection& db, const Version& ver) {
	Version v = ver == Version() ? DB_VER_LATEST : ver;
	db.ExecuteNonQuery(EXT_STR("PRAGMA user_version = " << ((v.Major << 16) | v.Minor)));
}

const Version
	VER_NORMALIZED_KEYS(0, 30),
	VER_KEYS32(0, 35),
	VER_INDEXED_PEERS(0, 35),
	VER_USE_HASH_TABLE_DB(0, 81),
	VER_SEGWIT(1, 0);

void CoinEng::UpgradeDb(const Version& ver) {
}

void CoinEng::UpgradeTo(const Version& ver) {
	Db->UpgradeTo(ver);
}

void CoinEng::TryUpgradeDb() {
	Db->TryUpgradeDb(DB_VER_LATEST);
}

path CoinEng::GetBootstrapPath() {
	if (Mode != EngMode::Bootstrap)
		Throw(errc::invalid_argument);
	return AfxGetCApp()->get_AppDataDir() / ToPath(ChainParams.Symbol) / ToPath(ChainParams.Symbol + ".bootstrap.dat");
}

path CoinEng::VGetDbFilePath() {
	String suffix = ".normal";
	switch (Mode) {
	case EngMode::Lite: suffix = ".spv"; break;
	case EngMode::BlockParser: suffix = ".blocks"; break;
	case EngMode::BlockExplorer: suffix = ".explorer"; break;
	case EngMode::Bootstrap: suffix = ".bootstrap-index"; break;
	}
	path r = AfxGetCApp()->get_AppDataDir() / ToPath(ChainParams.Symbol) / ToPath(ChainParams.Symbol + suffix);
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

void CoinEng::LoadPeers() {
	if (m_cdb.m_dbPeers) {
		EXT_LOCK(m_cdb.MtxDbPeers) {
			SqliteCommand cmd("SELECT id FROM nets WHERE name=?", m_cdb.m_dbPeers);
			DbDataReader dr = cmd.Bind(1, ChainParams.Name).ExecuteReader();
			if (dr.Read())
				m_idPeersNet = dr.GetInt32(0);
			else {
				SqliteCommand("INSERT INTO nets (name) VALUES (?)", m_cdb.m_dbPeers).Bind(1, ChainParams.Name).ExecuteNonQuery();
				m_idPeersNet = (int)m_cdb.m_dbPeers.LastInsertRowId;
			}

			if (!g_conf.Connect.empty()) {
				if (g_conf.Connect[0] != "0")
					for (auto& ip : g_conf.Connect)
						Add(IPEndPoint(IPAddress::Parse(ip), ChainParams.DefaultPort), NodeServices::NODE_NETWORK, Clock::now(), TimeSpan(0), false);
				return;
			}

			SqliteCommand cmdPeers("SELECT endpoints.ip, port, lastlive, services FROM endpoints JOIN peers ON endpoints.ip=peers.ip WHERE NOT peers.ban AND netid=?", m_cdb.m_dbPeers);
			for (DbDataReader dr = cmdPeers.Bind(1, m_idPeersNet).ExecuteReader(); dr.Read();) {
				if (ptr<Peer> peer = Add(IPEndPoint(IPAddress(dr.GetBytes(0)), (uint16_t)dr.GetInt32(1)), (uint64_t)dr.GetInt64(3), DateTime::from_time_t(dr.GetInt32(2))))
					peer->IsDirty = false;
			}
		}
	}
}

void CoinEng::ContinueLoad0() {
	m_cdb.Load();
	LoadPeers();

	//!!!	ListeningPort = ChainParams.DefaultPort;
}

void CoinEng::LoadLastBlockInfo() {
	int hMax = Db->GetMaxHeight(), hHeaderMax = Db->GetMaxHeaderHeight();
	if (hMax >= 0) {
		SetBestBlock(GetBlockByHeight(hMax));
		Caches.DtBestReceived = BestBlock().Timestamp;

		TRC(2, "BestBlock : " << hMax << " " << Hash(BestBlock()));
	}
	if (hHeaderMax >= 0) {
		SetBestHeader(Db->FindHeader(hHeaderMax));

		for (size_t i = ChainParams.Checkpoints.size(); i--;) {
			const pair<int, HashValue>& cp = ChainParams.Checkpoints[i];
			if (cp.first <= hHeaderMax) {
				ASSERT(Hash(Db->FindHeader(cp.first)) == cp.second); //!!! rare case when checkpoint set of new client contraverses saved fork. TODO solution: clear DB here
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
	AFX_MANAGE_STATE(AfxGetStaticModuleState());

	CCoinEngThreadKeeper engKeeper(this);
	path dbFilePath = GetDbFilePath();
	create_directories(dbFilePath.parent_path());
	bool bContinue;
	OffsetInBootstrap = 0;
	if (exists(dbFilePath)) {
		try {
			bContinue = OpenDb();
			if (bContinue)
				TryUpgradeDb();
		} catch (UnsupportedOldVersionException& ex) {
			Db->Recreate(ex.Version);
			bContinue = true;
		}
	} else
		bContinue = CreateDb();

	ContinueLoad0();
	if (bContinue)
		ContinueLoad();
}

void CoinEng::TryStartBootstrap() {
	if (Mode == EngMode::Bootstrap) {
		path pathBootstrap = GetBootstrapPath();
		TRC(2, "Checking for " << pathBootstrap);
		if (exists(pathBootstrap) && Db->GetBoostrapOffset() < file_size(pathBootstrap))
			(new BootstrapDbThread(_self, pathBootstrap, true))->Start(); // uses field CoinEng.Runned. Must be called after base::Start()
	}
}

void CoinEng::AddSeeds() {
	StartDns();

#if UCFG_COIN_USE_IRC
	if (m_cdb.ProxyString.ToUpper() != "TOR")
		StartIrc();
#endif

	DateTime now = Clock::now();
	Ext::Random rng;
	EXT_FOR(const IPEndPoint& ep, ChainParams.Seeds) {
		Add(ep, NodeServices::NODE_NETWORK, now - TimeSpan::FromDays(7 + rng.Next(7)));
	}
}

void CoinEng::Start() {
	if (Runned)
		return;

	EXT_LOCK(m_cdb.MtxDb) {
		if (Mode != EngMode::Lite)
			Filter = nullptr;
		else {
			Filter = Db->GetFilter();
			if (!m_cdb.IsFilterValid(Filter)) {
				PurgeDatabase();
				Load();
				CoinEngTransactionScope scopeBlockSavepoint(_self);
				Db->SetFilter(m_cdb.Filter);
			}
		}
		m_cdb.Events += this;
	}

	Net::Start();
	EXT_LOCK(m_cdb.MtxNets) {
		m_cdb.m_nets.push_back(this);
		m_cdb.Events += this;
	}
	m_cdb.Start();

	TryStartBootstrap();
	TryStartPruning();

	if (g_conf.Connect.empty())	   // Don't use seeds when we have "connect=" option
		AddSeeds();
}

void CoinEng::OnFilterChanged() {		// under m_cdb.MtxDb
	if (Mode != EngMode::Lite)
		return;
	ptr<FilterLoadMessage> m;
	if (Mode == EngMode::Lite) {
		CoinEngTransactionScope scopeBlockSavepoint(_self);
		Db->SetFilter(Filter = m_cdb.Filter);
		m = new FilterLoadMessage(Filter.get());
	}
	if (m)
		Broadcast(m);
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
		m_cdb.Events -= this;
		Ext::Remove(m_cdb.m_nets, this);
	}
#if UCFG_COIN_USE_IRC
	if (ChannelClient)
		m_cdb.IrcManager.DetachChannelClient(exchange(ChannelClient, nullptr));
#endif
	EXT_LOCK(m_mtxThreadStateChange) {
		m_tr.join_all();
	}
	Close();
}

void GetDataMessage::Process(Link& link) {
	CoinEng& eng = static_cast<CoinEng&>(*link.Net);
	base::Process(link);

	ptr<NotFoundMessage> mNotFound;
	EXT_FOR(const Inventory& inv, Invs) {
		if (Thread::CurrentThread->m_bStop)
			break;
		switch (inv.Type) {
		case InventoryType::MSG_BLOCK:
		case InventoryType::MSG_WITNESS_BLOCK:
		case InventoryType::MSG_FILTERED_BLOCK:
		case InventoryType::MSG_FILTERED_WITNESS_BLOCK:
			if (eng.Mode == EngMode::Lite || eng.Mode == EngMode::Normal)
				break;		// Low performance in NormalMode

			if (eng.Mode == EngMode::Bootstrap && link.HasWitness && bool(inv.Type & InventoryType::MSG_WITNESS_FLAG)) {
				if (auto o = eng.Db->FindBlockOffset(inv.HashValue)) {
					ptr<BlockMessage> m = new BlockMessage(Block(nullptr));
					m->Offset = o.value().first;
					m->Size = o.value().second;
					m->WitnessAware = true;
					link.Send(m);
					break;
				}
			}

			if (Block block = eng.Tree.FindBlock(inv.HashValue)) {
				//		ASSERT(EXT_LOCKED(eng.Mtx, (block->m_hash.reset(), Hash(block)==inv.HashValue)));

				switch (inv.Type) {
				case InventoryType::MSG_BLOCK:
				case InventoryType::MSG_WITNESS_BLOCK: {
					TRC(2, "Block Message sending");

					ptr<BlockMessage> m = new BlockMessage(block);
					m->WitnessAware = bool(inv.Type & InventoryType::MSG_WITNESS_FLAG);
					link.Send(m);
				} break;
				case InventoryType::MSG_FILTERED_BLOCK:
				case InventoryType::MSG_FILTERED_WITNESS_BLOCK: {
					ptr<MerkleBlockMessage> mbm;
					vector<Tx> matchedTxes = EXT_LOCKED(link.MtxFilter, link.Filter ? (mbm = new MerkleBlockMessage)->Init(block, *link.Filter) : vector<Tx>());
					if (mbm) {
						link.Send(mbm);
						EXT_LOCK(link.Mtx) {
							for (size_t i = matchedTxes.size(); i--;)
								if (link.KnownInvertorySet.count(Inventory(InventoryType::MSG_TX, Hash(matchedTxes[i]))))
									matchedTxes.erase(matchedTxes.begin() + i);
						}
						for (auto& tx : matchedTxes)
							link.Send(new TxMessage(tx));
					}
				} break;
				}

				if (inv.HashValue == link.HashContinue) {
					ptr<InvMessage> m = new InvMessage;
					m->Invs.push_back(Inventory(InventoryType::MSG_BLOCK, Hash(eng.BestBlock())));
					link.Send(m);
					link.HashContinue = HashValue::Null();
				}
			}
			break;
		case InventoryType::MSG_TX:
		case InventoryType::MSG_WITNESS_TX:
			{
				ptr<TxMessage> m;
				EXT_LOCK(eng.Caches.Mtx) {
					if (auto otx = Lookup(eng.Caches.m_relayHashToTx, inv.HashValue))
						m = new TxMessage(otx.value());
				}
				if (m) {
					m->WitnessAware = bool(inv.Type & InventoryType::MSG_WITNESS_FLAG);
					link.Send(m);
				}
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

void NotFoundMessage::Process(Link& link) {
	CoinEng& eng = Eng();
	if (eng.Mode == EngMode::Lite) {
		//!!!TODO
	}
}

void CoinPartialMerkleTree::Init(const vector<HashValue>& vHash, const dynamic_bitset<uint8_t>& bsMatch) {
	Bitset.clear();
	Items.clear();
	TraverseAndBuild(CalcTreeHeight(), 0, &vHash[0], bsMatch);
}

pair<HashValue, vector<HashValue>> CoinPartialMerkleTree::ExtractMatches() const {
	if (NItems == 0 || NItems > MAX_BLOCK_SIZE / 60 || Items.size() > NItems)
		Throw(CoinErr::Misbehaving);
	size_t nBitsUsed = 0;
	int nHashUsed = 0;
	pair<HashValue, vector<HashValue>> r;
	r.first = TraverseAndExtract(CalcTreeHeight(), 0, nBitsUsed, nHashUsed, r.second);
	return r;
}

DateTime CoinEng::GetTimestampForNextBlock() {
	int64_t epoch = to_time_t(std::max(BestBlock().GetMedianTimePast() + seconds(1), Clock::now())); // round to seconds
	return DateTime::from_time_t(epoch);
}

double CoinEng::ToDifficulty(const Target& target) {
	BigInteger n = ChainParams.MaxPossibleTarget, d = target;
	int nlen = n.Length, dlen = d.Length, exp = nlen - (dlen + DBL_MANT_DIG);
	if (exp > 0) {
		d <<= exp;
	} else {
		n <<= -exp;
	}
	BigInteger r = n / d;
	double difficult = ::ldexp((double)r.AsDouble(), exp);
	return difficult;
}

int64_t CoinEng::GetSubsidy(int height, const HashValue& prevBlockHash, double difficulty, bool bForCheck) {
	return ChainParams.InitBlockValue >> (height / ChainParams.HalfLife);
}

void CoinEng::CheckCoinbasedTxPrev(int hCur, int hOut) {
	if (hCur - hOut < ChainParams.CoinbaseMaturity)
		Throw(CoinErr::TxPrematureSpend);
}

void IBlockChainDb::Recreate(const Version& verOld) {
	CoinEng& eng = Eng();
	Close(false);
	path pathOld = eng.GetDbFilePath(),
		pathBak = pathOld;
	pathBak += ".bak-" + verOld.ToString();
	filesystem::rename(pathOld, pathBak);
	eng.CreateDb();
}

void IBlockChainDb::UpdateCoins(const OutPoint& op, bool bSpend, int32_t heightCur) {
	vector<bool> vSpend = GetCoinsByTxHash(op.TxHash);
	if (!bSpend)
		vSpend.resize(std::max(vSpend.size(), size_t(op.Index + 1)));
	else if (op.Index >= vSpend.size() || !vSpend[op.Index])
		Throw(CoinErr::InputsAlreadySpent);
	vSpend[op.Index] = !bSpend;
	SaveCoinsByTxHash(op.TxHash, vSpend);
}

Version CheckUserVersion(SqliteConnection& db) {
	int dbver = (int)SqliteCommand("PRAGMA user_version", db).ExecuteInt64Scalar();
	Version dver = Version(dbver >> 16, dbver & 0xFFFF);
	if (dver.Major <= DB_VER_LATEST.Major)	// Version mj.x is compatible with mj.y
		return dver;
	db.Close();
	throw VersionException(dver);
}

CEngStateDescription::CEngStateDescription(CoinEng& eng, RCString s)
	: m_eng(eng)
	, m_s(s)
{
	TRC(2, s);

	EXT_LOCKED(m_eng.m_mtxStates, m_eng.m_setStates.insert(s));
	m_eng.Events.OnChange();
}

CEngStateDescription::~CEngStateDescription() {
	EXT_LOCKED(m_eng.m_mtxStates, m_eng.m_setStates.erase(m_s));
	m_eng.Events.OnChange();
}

} // namespace Coin
