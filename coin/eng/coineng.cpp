/*######     Copyright (c) 1997-2013 Ufasoft  http://ufasoft.com  mailto:support@ufasoft.com,  Sergey Pavlov  mailto:dev@ufasoft.com #######################################
#                                                                                                                                                                          #
# This program is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation;  #
# either version 3, or (at your option) any later version. This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the      #
# implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details. You should have received a copy of the GNU #
# General Public License along with this program; If not, see <http://www.gnu.org/licenses/>                                                                               #
##########################################################################################################################################################################*/

#include <el/ext.h>

#include <el/crypto/hash.h>
#include <el/db/bdb-reader.h>


//#include "coineng.h"
#include "coin-protocol.h"
#include "script.h"
#include "eng.h"

using Ext::DB::DbException;

template<> Coin::ChainParams * StaticList<Coin::ChainParams>::Root = 0;

namespace Coin {



void MsgLoopThread::Execute() {
	Name = "MsgLoopThread";

	DateTime dtNextPeriodic = 0;

	try {
		DBG_LOCAL_IGNORE_WIN32(WSAENOTSOCK);

		while (!m_bStop) {			
			DateTime now = DateTime::UtcNow();
			if (now > dtNextPeriodic) {
				EXT_LOCK (m_cdb.MtxNets) {
					for (int i=0; i<m_cdb.m_nets.size(); ++i) {
						CoinEng& eng = *static_cast<CoinEng*>(m_cdb.m_nets[i]);
						CCoinEngThreadKeeper engKeeper(&eng);
						eng.OnPeriodicMsgLoop();
					}
				}
				dtNextPeriodic = now+TimeSpan::FromSeconds(P2P::PERIODIC_SECONDS);
			} else
				m_ev.Lock(P2P::PERIODIC_SECONDS*1000);
		}
	} catch (RCExc ex) {
		if (HResultInCatch(ex) != HRESULT_FROM_WIN32(WSAENOTSOCK))
			throw;
	}

//!!!R	Net.OnCloseMsgLoop();
}


void ChainParams::Add(const ChainParams& p) {
//!!!	AvailableChains.insert(make_pair(p.Name, p));
}

void ChainParams::Init() {
	HashAlgo = Coin::HashAlgo::Sha256;
	ProtocolVersion = PROTOCOL_VERSION;
	CoinValue = 100000000;
	InitBlockValue = CoinValue * 50;
	MaxMoney = 21000000 * CoinValue;
	HalfLife = 210000;
	AnnualPercentageRate = 0;
	MinTxFee = CoinValue/10000;
	BlockSpan = TimeSpan::FromSeconds(600);
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
}


CoinEng *ChainParams::CreateObject(CoinDb& cdb, IBlockChainDb *db) {
	CoinEng *r = new CoinEng(cdb);
	r->SetChainParams(_self, db);
	return r;
}

MyKeyInfo::~MyKeyInfo() {
}

Address MyKeyInfo::ToAddress() const {
	return Address(Eng(), Hash160, Comment);
}

Blob MyKeyInfo::PlainPrivKey() const {
	byte typ = 0;
	return Blob(&typ, 1) + get_PrivKey();
}

Blob MyKeyInfo::EncryptedPrivKey(Aes& aes) const {
	byte typ = 'A';
	Blob privKey = PrivKey;
	return Blob(&typ, 1) + aes.Encrypt(privKey + Crc32().ComputeHash(privKey));
}


/*
void MyKeyInfo::put_PrivKey(const Blob& v) {
	if (v.Size > 32)
		Throw(E_FAIL);
	Key = CngKey::Import(m_privKey = v, CngKeyBlobFormat::OSslEccPrivateBignum);
	Blob pubKey = Key.Export(CngKeyBlobFormat::OSslEccPublicBlob);
	if (PubKey.Size != 0 && PubKey != pubKey)
		Throw(E_FAIL);
	Hash160 = Coin::Hash160(PubKey = pubKey);
}*/

void MyKeyInfo::SetPrivData(const ConstBuf& cbuf, bool bCompressed) {
	ASSERT(cbuf.Size <= 32);
	m_privKey = cbuf.Size==32 ? cbuf : ConstBuf(Blob(0, 32-cbuf.Size)+cbuf);

	if (m_privKey.Size != 32)
		Throw(E_FAIL);
	Key = CngKey::Import(m_privKey, CngKeyBlobFormat::OSslEccPrivateBignum);
	Blob pubKey = Key.Export(bCompressed ? CngKeyBlobFormat::OSslEccPublicCompressedBlob : CngKeyBlobFormat::OSslEccPublicBlob);
	if (PubKey.Size != 0 && PubKey != pubKey)
		Throw(E_FAIL);
	PubKey = pubKey;
}


void MyKeyInfo::SetPrivData(const PrivateKey& privKey) {
	pair<Blob, bool> pp = privKey.GetPrivdataCompressed();
	SetPrivData(pp.first, pp.second);
}

//!!!R EXT_THREAD_PTR(CoinEng, t_pCoinEng);
EXT_THREAD_PTR(void, t_bPayToScriptHash);

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
	:	base(cdb)
	,	m_cdb(cdb)
	,	MaxBlockVersion(2)
	,	m_freeCount(0)
	,	CommitPeriod(0x1F)
	,	Mode(EngMode::Normal)
	,	AllowFreeTxes(true)
	,	UpgradingDatabaseHeight(0)
{
}

CoinEng::~CoinEng() {
}

void CoinEng::SetChainParams(const Coin::ChainParams& p, IBlockChainDb *db) {
	ChainParams = p;
	Db = db;
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
			DateTime dbgStart = DateTime::UtcNow();
#endif
			Db->Commit();
		
#if UCFG_TRC
			TRC(2, (DateTime::UtcNow()-dbgStart).TotalMilliseconds << " ms, LastBlock: " << BestBlockHeight());
#endif
		}
	}
}

void CoinEng::Close() {
	TRC(1, "Closing " << GetDbFilePath());

	CommitTransactionIfStarted();

	if (m_iiEngEvents)
		m_iiEngEvents->OnCloseDatabase();

	Db->Close();
}

Block CoinEng::GetBlockByHeight(UInt32 height) {
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

CoinMessage *CoinEng::CreateVersionMessage() {
	return new VersionMessage;
}

CoinMessage *CoinEng::CreateVerackMessage() {
	return new VerackMessage;
}

CoinMessage *CoinEng::CreateAddrMessage() {
	return new AddrMessage;
}

CoinMessage *CoinEng::CreateInvMessage() {
	return new InvMessage;
}

CoinMessage *CoinEng::CreateGetDataMessage() {
	return new GetDataMessage; 
}

CoinMessage *CoinEng::CreateGetBlocksMessage() {
	return new GetBlocksMessage;
}

CoinMessage *CoinEng::CreateGetHeadersMessage() {
	return new GetHeadersMessage;
}

CoinMessage *CoinEng::CreateTxMessage() {
	return new TxMessage;
}

CoinMessage *CoinEng::CreateBlockMessage() {
	return new BlockMessage;
}

CoinMessage *CoinEng::CreateHeadersMessage() {
	return new HeadersMessage;
}

CoinMessage *CoinEng::CreateGetAddrMessage() {
	return new GetAddrMessage;
}

CoinMessage *CoinEng::CreateCheckOrderMessage() {
	return new CheckOrderMessage;
}

CoinMessage *CoinEng::CreateSubmitOrderMessage() {
	return new SubmitOrderMessage;
}

CoinMessage *CoinEng::CreateReplyMessage() {
	return new ReplyMessage;
}

CoinMessage *CoinEng::CreatePingMessage() {
	return new PingMessage;
}

CoinMessage *CoinEng::CreatePongMessage() {
	return new PongMessage;
}

CoinMessage *CoinEng::CreateMemPoolMessage() {
	return new MemPoolMessage;
}

CoinMessage *CoinEng::CreateAlertMessage() {
	return new AlertMessage;
}

CoinMessage *CoinEng::CreateMerkleBlockMessage() {
	return new MerkleBlockMessage;
}

CoinMessage *CoinEng::CreateFilterLoadMessage() {
	return new FilterLoadMessage;
}

CoinMessage *CoinEng::CreateFilterAddMessage() {
	return new FilterAddMessage;
}

CoinMessage *CoinEng::CreateFilterClearMessage() {
	return new FilterClearMessage;
}

CoinMessage *CoinEng::CreateCheckPointMessage() {
	return new CoinMessage("checkpoint");
}


Block CoinEng::LookupBlock(const HashValue& hash) {
	Block r(nullptr);
	EXT_LOCK (Caches.Mtx) {
		if (Lookup(Caches.HashToBlockCache, hash, r) || Lookup(Caches.HashToAlternativeChain, hash, r))
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

void CoinEng::LoadLastBlockInfo() {
	int heightMax = Db->GetMaxHeight();
	if (heightMax >= 0) {
		SetBestBlock(GetBlockByHeight(heightMax));
		Caches.DtBestReceived = BestBlock().Timestamp;

		TRC(2, Hash(BestBlock()));
	}
}

bool CoinEng::CreateDb() {
	bool r = Db->Create(GetDbFilePath());

	TransactionScope dbtx(*Db);


	if (m_iiEngEvents)
		m_iiEngEvents->OnCreateDatabase();

	CreateAdditionalTables();
	return r;
}

void ChainParams::LoadFromXmlAttributes(IXmlAttributeCollection& xml) {
	Name = xml.GetAttribute("Name");
	if ((Symbol = xml.GetAttribute("Symbol")).IsEmpty())
		Symbol = Name;
	IsTestNet = xml.GetAttribute("IsTestNet") == "1";
	Genesis = HashValue(xml.GetAttribute("Genesis"));

	String a;

	if (!(a = xml.GetAttribute("BlockSpan")).IsEmpty())
		BlockSpan = TimeSpan::FromSeconds(Convert::ToInt32(a));

	if (!(a = xml.GetAttribute("CoinValue")).IsEmpty())
		CoinValue = Convert::ToInt64(a);

	if (!(a = xml.GetAttribute("CoinbaseMaturity")).IsEmpty())
		CoinbaseMaturity = Convert::ToInt32(a);

	if (!(a = xml.GetAttribute("InitBlockValue")).IsEmpty())
		InitBlockValue = CoinValue*(Convert::ToInt64(a));

	if (!(a = xml.GetAttribute("MaxMoney")).IsEmpty())
		MaxMoney = Convert::ToInt64(a) * CoinValue;

	if (!(a = xml.GetAttribute("MinTxFee")).IsEmpty())
		MinTxFee = Convert::ToInt64(a);

	if (!(a = xml.GetAttribute("TargetInterval")).IsEmpty())
		TargetInterval = Convert::ToInt32(a);	

	if (!(a = xml.GetAttribute("MinTxOutAmount")).IsEmpty())
		MinTxOutAmount = Convert::ToInt64(a);

	if (!(a = xml.GetAttribute("AnnualPercentageRate")).IsEmpty())
		AnnualPercentageRate = Convert::ToInt32(a);	

	if (!(a = xml.GetAttribute("HalfLife")).IsEmpty())
		HalfLife = Convert::ToInt32(a);

	if (!(a = xml.GetAttribute("PowOfDifficultyToHalfSubsidy")).IsEmpty())
		PowOfDifficultyToHalfSubsidy = 1.0/Convert::ToInt32(a);

	a = xml.GetAttribute("MaxTargetHex");
	MaxTarget = !a.IsEmpty() ? Target(Convert::ToUInt32(a, 16)) : Target(0x1D00FFFF);

	if (!(a = xml.GetAttribute("InitTargetHex")).IsEmpty())
		InitTarget = Target(Convert::ToUInt32(a, 16));

	ProtocolMagic = Convert::ToUInt32(xml.GetAttribute("ProtocolMagicHex"), 16);

	if (!(a = xml.GetAttribute("ProtocolVersion")).IsEmpty())
		ProtocolVersion = Convert::ToUInt32(a);

	a = xml.GetAttribute("DefaultPort");
	DefaultPort = !a.IsEmpty() ? Convert::ToUInt16(a) : 8333;

	BootUrls = xml.GetAttribute("BootUrls").Split();

	a = xml.GetAttribute("IrcRange");
	IrcRange = !a.IsEmpty() ? Convert::ToInt32(a) : 0;

	a = xml.GetAttribute("AddressVersion");
	AddressVersion = !a.IsEmpty() ? Convert::ToByte(a) : (IsTestNet ? 111 : 0);

	a = xml.GetAttribute("ScriptAddressVersion");
	ScriptAddressVersion = !a.IsEmpty() ? Convert::ToByte(a) : (IsTestNet ? 196 : 5);

	a = xml.GetAttribute("ChainID");
	ChainId = !a.IsEmpty() ? Convert::ToInt32(a) : 0;

	a = xml.GetAttribute("PayToScriptHashHeight");
	if (!!a)
		PayToScriptHashHeight = Convert::ToInt32(a);

	if (!(a = xml.GetAttribute("Listen")).IsEmpty())
		Listen = Convert::ToInt32(a);

	a = xml.GetAttribute("CheckDupTxHeight");
	if (!a.IsEmpty())
		CheckDupTxHeight = Convert::ToInt32(a);	

	if (!(a = xml.GetAttribute("MergedMiningStartBlock")).IsEmpty()) {
		AuxPowEnabled = true;
		AuxPowStartBlock = Convert::ToInt32(a);
	}

	if (!(a = xml.GetAttribute("Algo")).IsEmpty())
		HashAlgo = StringToAlgo(a);

	a = xml.GetAttribute("FullPeriod");
	FullPeriod = !a.IsEmpty() && atoi(a);
}

int ChainParams::CoinValueExp() const {
	int r = 0;
	for (Int64 n=CoinValue; n >= 10; ++r)
		n /= 10;
	return r;
}

static regex ReIp4Port("\\d+\\.\\d+\\.\\d+\\.\\d+:\\d+");

void ChainParams::AddSeedEndpoint(RCString seed) {
	Seeds.insert(regex_match(seed.c_str(), ReIp4Port)
		? IPEndPoint::Parse(seed)
		: IPEndPoint(IPAddress::Parse(seed), DefaultPort));
}



ptr<CoinEng> CoinEng::CreateObject(CoinDb& cdb, RCString name, ptr<IBlockChainDb> db) {
	String pathXml = Path::Combine(Path::GetDirectoryName(System.ExeFilePath), "coin-chains.xml");
	if (!db)
		db = CreateBlockChainDb();
	ptr<CoinEng> peng;
	if (!File::Exists(pathXml))
		Throw(E_COIN_XmlFileNotFound);
	try {
#if UCFG_WIN32		
		CUsingCOM usingCOM;
		XmlDocument doc = new XmlDocument;
		doc.LoadXml(File::ReadAllText(pathXml));		// LoadXml() instead of Load() to eliminate loading shell32.dll
		XmlNodeReader rd(doc);
#else
		ifstream ifs(pathXml.c_str());
		XmlTextReader rd(ifs);
#endif
 		for (bool b=rd.ReadToDescendant("Chain"); b; b=rd.ReadToNextSibling("Chain")) {
			Coin::ChainParams params;
			if (name == (params.Name = rd.GetAttribute("Name"))) {
				params.LoadFromXmlAttributes(rd);
				while (rd.Read()) {
					if (rd.NodeType == XmlNodeType::Element || rd.NodeType == XmlNodeType::EndElement) {
						String elName = rd.Name;
						if (elName == "Chain")
							break;
						if (rd.NodeType == XmlNodeType::Element) {
							if (elName == "Checkpoint") {
								int height = Convert::ToInt32(rd.GetAttribute("Block"));
								params.LastCheckpointHeight = std::max(height, params.LastCheckpointHeight);
								params.Checkpoints.insert(make_pair(height, HashValue(rd.GetAttribute("Hash"))));
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
				
				for (Coin::ChainParams *p=Coin::ChainParams::Root; p; p=p->Next) 
					if (p->Name == name) {
						params.MaxPossibleTarget = p->MaxPossibleTarget;
						peng = p->CreateObject(cdb, db);
					}
				if (!peng)
					peng = params.CreateObject(cdb, db);
				peng->SetChainParams(params, db);
				goto LAB_RET;
			}
		}
	} catch (RCExc) {
	}
	Throw(E_COIN_XmlFileNotFound);
LAB_RET:
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

		EXT_FOR (const String& dns, DnsServers) {
			try {
				IPHostEntry he = Dns::GetHostEntry(dns);
				DateTime now = DateTime::UtcNow();
				EXT_FOR (const IPAddress& a, he.AddressList) {
					IPEndPoint ep = IPEndPoint(a, Eng.ChainParams.DefaultPort);
					TRC(2, ep);
					Eng.Add(ep, NODE_NETWORK, now-TimeSpan::FromDays(1));
				}
			} catch (RCExc) {
			}
		}
	}
};


static regex s_reDns("dns://(.+)");
static regex s_reIrc("irc://([^:/]+)(:(\\d+))?/(.+)");

void CoinEng::StartIrc() {
	if (!ChannelClient) {
		vector<String> ar = ChainParams.BootUrls;
		random_shuffle(ar.begin(), ar.end());
		IPEndPoint epServer;
		String channel;
		vector<String> dnsServers;
		for (int i=0; i<ar.size(); ++i) {
			String url = ar[i];
			cmatch m;
			if (regex_match(url.c_str(), m, s_reDns)) {
				dnsServers.push_back(m[1]);
			} else if (regex_match(url.c_str(), m, s_reIrc)) {
				if (channel.IsEmpty()) {
					String host = m[1];
					UInt16 port = UInt16(m[2].matched ? atoi(String(m[3])) : 6667);
					epServer = IPEndPoint(host, port);
					ostringstream os;
					os << m[4].str();
					if (!ChainParams.IsTestNet && ChainParams.IrcRange > 0)
						os << std::setw(2) << std::setfill('0') << Ext::Random().Next(ChainParams.IrcRange);
					channel = os.str();
				}
			}
		}
		if (!dnsServers.empty())
			(new DnsThread(_self, dnsServers))->Start();
		if (!channel.IsEmpty()) {
			ChannelClient = new Coin::ChannelClient(_self);
//!!!			ChannelClient->ListeningPort = ListeningPort;
			m_cdb.IrcManager.AttachChannelClient(epServer, channel, ChannelClient);
		}
	}
}

void CoinEng::Start() {
	if (Runned)
		return;

	Net::Start();
	EXT_LOCK (m_cdb.MtxNets) {
		m_cdb.m_nets.push_back(this);
	}
	StartIrc();	

	DateTime now = DateTime::UtcNow();
	Ext::Random rng;
	EXT_FOR (const IPEndPoint& ep, ChainParams.Seeds) {
		Add(ep, NODE_NETWORK, now-TimeSpan::FromDays(7+rng.Next(7)));
	}
}

void CoinEng::Stop() {
	Runned = false;
	EXT_LOCK (m_mtxThreadStateChange) {
		m_tr.interrupt_all();
	}
	EXT_LOCK (m_cdb.MtxNets) {
		Ext::Remove(m_cdb.m_nets, this);
	}
	if (ChannelClient)
		m_cdb.IrcManager.DetachChannelClient(exchange(ChannelClient, nullptr));
	EXT_LOCK (m_mtxThreadStateChange) {
		m_tr.join_all();
	}
	Close();
}

Block CoinEng::BestBlock() {
	EXT_LOCK (Caches.Mtx) {
		return Caches.m_bestBlock;
	}
}

void CoinEng::SetBestBlock(const Block& b) {
	EXT_LOCK (Caches.Mtx) {
		Caches.m_bestBlock = b;
	}
}

int CoinEng::BestBlockHeight() {
	EXT_LOCK (Caches.Mtx) {
		return Caches.m_bestBlock.SafeHeight;
	}
}

void CoinEng::AddLink(P2P::LinkBase *link) {
	base::AddLink(link);
	if (m_iiEngEvents)
		m_iiEngEvents->OnChange();
}

void CoinEng::OnCloseLink(P2P::LinkBase& link) {
	base::OnCloseLink(link);
	RemoveVerNonce(static_cast<P2P::Link&>(link));
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

void CoinEng::SavePeers() {
	m_cdb.SavePeers(m_idPeersNet, GetAllPeers());
}

void CoinEng::Misbehaving(P2P::Message *m, int howMuch) {
	Peer& peer = *m->Link->Peer;
	if ((peer.Misbehavings += howMuch) > P2P::MAX_PEER_MISBEHAVINGS) {
		m_cdb.BanPeer(peer);
		throw;
	}
}

void CoinEng::OnMessageReceived(P2P::Message *m) {
	if (!Runned)
		return;
	CCoinEngThreadKeeper engKeeper(this);
	try {
		DBG_LOCAL_IGNORE_NAME(E_COIN_RecentCoinbase, E_COIN_RecentCoinbase);
		DBG_LOCAL_IGNORE_NAME(E_COIN_IncorrectProofOfWork, E_COIN_IncorrectProofOfWork);
		DBG_LOCAL_IGNORE_NAME(E_COIN_RejectedByCheckpoint, E_COIN_RejectedByCheckpoint);

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
	} catch (const PeerMisbehavingExc& ex) {
		TRC(2, ex.Message);
		Misbehaving(m, ex.HowMuch);
	} catch (RCExc ex) {
		switch (HResultInCatch(ex)) {
		case E_COIN_SizeLimits:
		case E_COIN_RejectedByCheckpoint:
			Misbehaving(m, 100);
			break;
		case E_COIN_TxNotFound:
		case E_COIN_AllowedErrorDuringInitialDownload:
			break;
		case E_COIN_RecentCoinbase:			//!!!? not misbehavig
		case E_COIN_AlertVerifySignatureFailed:
			break;
		case E_COIN_IncorrectProofOfWork:
			goto LAB_MIS;
		case E_COIN_VeryBigPayload:
		default:
			TRC(2, ex.what());
LAB_MIS:
			Misbehaving(m, 1);
		}
	}
}

size_t CoinEng::GetMessageHeaderSize() {
	return sizeof(SCoinMessageHeader);
}

size_t CoinEng::GetMessagePayloadSize(const ConstBuf& buf) {
	DBG_LOCAL_IGNORE_NAME(E_EXT_Protocol_Violation, ignE_EXT_Protocol_Violation);	

	const SCoinMessageHeader& hdr = *(SCoinMessageHeader*)buf.P;
	if (le32toh(hdr.Magic) != ChainParams.ProtocolMagic)
		Throw(E_EXT_Protocol_Violation);
	return le32toh(hdr.PayloadSize) + sizeof(UInt32);
//!!!R?	if (strcmp(hdr.Command, "version") && strcmp(hdr.Command, "verack"))
}

ptr<P2P::Message> CoinEng::RecvMessage(Link& link, const BinaryReader& rd) {
	CCoinEngThreadKeeper engKeeper(this);

	UInt32 magic = rd.ReadUInt32();
	if (magic != ChainParams.ProtocolMagic)
		Throw(E_EXT_Protocol_Violation);
	ptr<CoinMessage> m = CoinMessage::ReadFromStream(link, rd);
		
	TRC(4, ChainParams.Symbol << " " << link.Peer->get_EndPoint() << " " << *m);

	return m.get();
}

void CoinEng::SendVersionMessage(Link& link) {
	ptr<VersionMessage> m = (VersionMessage*)CreateVersionMessage();
	m->UserAgent = "/" MANUFACTURER " " VER_PRODUCTNAME_STR ":" VER_PRODUCTVERSION_STR "/";		// BIP 0014
	m->RemoteTimestamp = DateTime::UtcNow();
	RandomRef().NextBytes(Buf(&m->Nonce, sizeof m->Nonce));
	EXT_LOCK (m_mtxVerNonce) {
		m_nonce2link[m->Nonce] = &link;
	}
	if (Listen)
		m->LocalPeerInfo.Ep = NetManager.LocalEp4;		//!!!? IPv6
	m->RemotePeerInfo.Ep = link.Tcp.Client.RemoteEndPoint;
	m->LastReceivedBlock = BestBlockHeight();
	link.Send(m);
}

bool CoinEng::HaveBlockInMainTree(const HashValue& hash) {
	EXT_LOCK (Caches.Mtx) {
		if (Caches.HashToBlockCache.count(hash) || Caches.HashToAlternativeChain.count(hash))
			return true;
	}
	return Db->HaveBlock(hash);
}

bool CoinEng::HaveBlock(const HashValue& hash) {
	EXT_LOCK (Caches.Mtx) {
		if (Caches.OrphanBlocks.count(hash))
			return true;
	}
	return HaveBlockInMainTree(hash);
}

bool CoinEng::HaveTxInDb(const HashValue& hashTx) {
	return Tx::TryFromDb(hashTx, nullptr);
}

bool CoinEng::Have(const Inventory& inv) {
	switch (inv.Type) {
	case InventoryType::MSG_BLOCK:
		return HaveBlock(inv.HashValue);
	case InventoryType::MSG_TX:
		if (EXT_LOCKED(TxPool.Mtx, TxPool.m_hashToTx.count(inv.HashValue)))			// don't combine with next expr, because lock's scope will be wider
			return true;
		return HaveTxInDb(inv.HashValue);
	default:
		return false; //!!!TODO
	}
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
		Inventory inv(InventoryType::MSG_TX, hash);
		EXT_LOCK (Caches.Mtx) {
			Caches.m_relayHashToTx.insert(make_pair(hash, tx));
		}
		Push(tx);
	}
}

bool CoinEng::IsInitialBlockDownload() {
	if (UpgradingDatabaseHeight)
		return true;
	Block bestBlock = BestBlock();
	if (!bestBlock || bestBlock.Height < ChainParams.LastCheckpointHeight-INITIAL_BLOCK_THRESHOLD)
		return true;
	DateTime now = DateTime::UtcNow();
	HashValue hash = Hash(bestBlock);
	EXT_LOCK (MtxBestUpdate) {
		if (m_hashPrevBestUpdate != hash) {
			m_hashPrevBestUpdate = hash;
			m_dtPrevBestUpdate = now;
		}
		return now-m_dtPrevBestUpdate < TimeSpan::FromSeconds(20) && bestBlock.Timestamp < now-TimeSpan::FromHours(24);
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
	m_internalName = "Coin";

#if !UCFG_WCE
	m_appDataDir = Environment::GetEnvironmentVariable("COIN_APPDATA");
#endif

#if UCFG_TRC //!!!D
//	CTrace::s_nLevel = 0xF;
	static ofstream s_ofs((Path::Combine(get_AppDataDir(), "coin.log")).ToOsString(), ios::binary);
	CTrace::s_pOstream = &s_ofs;//!!!D
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
//!!!R	NAMECOIN_VER_DOMAINS_TABLE(0, 26),
//!!!R	VER_DUPLICATED_TX(0, 27),
	VER_NORMALIZED_KEYS(0, 30),
	VER_KEYS32(0, 35),
	VER_INDEXED_PEERS(0, 35);


void CoinEng::UpgradeDb(const Version& ver) {
/*!!!R	if (ver == VER_DUPLICATED_TX) {
		if (ChainParams.IsTestNet) {
			m_db.ExecuteNonQuery("DELETE FROM txes");
			m_db.ExecuteNonQuery("DELETE FROM blocks");
		}
	}*/
}

void CoinEng::UpgradeTo(const Version& ver) {
	Db->UpgradeTo(ver);
}

void CoinEng::TryUpgradeDb() {
	Db->TryUpgradeDb(theApp.ProductVersion);
}

String CoinEng::GetDbFilePath() {
	if (m_dbFilePath.IsEmpty()) {
		m_dbFilePath = Path::Combine(AfxGetCApp()->get_AppDataDir(), ChainParams.Name);
		if (!m_cdb.FilenameSuffix.IsEmpty())
			m_dbFilePath += m_cdb.FilenameSuffix;
		else  {
			switch (Mode) {
			case EngMode::Lite:
				m_dbFilePath += ".lite";
				break;
			case EngMode::BlockExplorer:
				m_dbFilePath += ".explorer";
				break;
			}
		}
		m_dbFilePath += Db->DefaultFileExt;
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
				if (ptr<Peer> peer = Add(IPEndPoint(IPAddress(dr.GetBytes(0)), (UInt16)dr.GetInt32(1)), (UInt64)dr.GetInt64(3), DateTime::from_time_t(dr.GetInt32(2))))
					peer->IsDirty = false;
			}
		}
	}
	//!!!	ListeningPort = ChainParams.DefaultPort;
}

void CoinEng::ContinueLoad() {
	LoadLastBlockInfo();
}

void CoinEng::Load() {
	String dbFilePath = GetDbFilePath();
	Directory::CreateDirectory(Path::GetDirectoryName(dbFilePath));
	bool bContinue;
	if (File::Exists(dbFilePath)) {
		bContinue = OpenDb();
		if (bContinue)
			TryUpgradeDb();
	} else
		bContinue = CreateDb();

	ContinueLoad0();
	if (bContinue)
		ContinueLoad();
}

VersionMessage::VersionMessage()
	:	base("version")
	,	ProtocolVer(Eng().ChainParams.ProtocolVersion)
	,	Services(0)
	,	LastReceivedBlock(-1)
	,	RelayTxes(true)
{
	CoinEng& eng = Eng();
	if (eng.LiteMode)
		RelayTxes = !eng.Filter;
	else
		Services |= NODE_NETWORK;
}

void VersionMessage::Write(BinaryWriter& wr) const {
	wr << ProtocolVer << Services << Int64(to_time_t(RemoteTimestamp)) << RemotePeerInfo << LocalPeerInfo << Nonce;
	WriteString(wr, UserAgent);
	wr << LastReceivedBlock << RelayTxes;
}

void VersionMessage::Read(const BinaryReader& rd) {
	ProtocolVer = rd.ReadUInt32();
	Services = rd.ReadUInt64();
	RemoteTimestamp = DateTime::from_time_t(rd.ReadUInt64());
	if (ProtocolVer >= 106) {
		rd >> RemotePeerInfo >>	LocalPeerInfo >> Nonce;
		UserAgent = ReadString(rd);
		if (ProtocolVer >= 209) {
			LastReceivedBlock = (Int32)rd.ReadUInt32();
			RelayTxes = rd.BaseStream.Eof() ? true : rd.ReadByte();
		}
	}
}

void VersionMessage::Process(P2P::Link& link) {
	if (ProtocolVer < 209)
		Throw(E_NOTIMPL);

	CoinEng& eng = Eng();
	CoinLink& clink = static_cast<CoinLink&>(link);
	if (eng.CheckSelfVerNonce(Nonce)) {
		clink.PeerVersion = ProtocolVer;
		clink.Send(eng.CreateVerackMessage());
		clink.LastReceivedBlock = LastReceivedBlock;
		clink.RelayTxes = RelayTxes;

		if (link.Incoming)
			eng.SendVersionMessage(link);
		else {
			link.Send(new GetAddrMessage);

			if (!eng.m_bSomeInvReceived && (link.Peer->Services & NODE_NETWORK)) {
				link.Send(new GetBlocksMessage(eng.BestBlock(), HashValue()));
			}
		}

		if (eng.LiteMode && eng.Filter) {
			link.Send(new FilterLoadMessage(eng.Filter.get()));
		}
	}
	if (link.Incoming)
		if (ptr<Peer> p = link.Net->Add(link.Tcp.Client.RemoteEndPoint, Services, DateTime::UtcNow()))
			link.Peer = p;
	eng.Good(link.Peer);
}

String VersionMessage::ToString() const {
	return base::ToString() + String(EXT_STR(" Ver: " << ProtocolVer << ", " << UserAgent << ", LastBlock: " << LastReceivedBlock));
}

void VerackMessage::Process(P2P::Link& link) {
	Eng().RemoveVerNonce(link);
}

String Inventory::ToString() const {
	ostringstream os;
	switch (Type) {
	case InventoryType::MSG_TX: os << "MSG_TX "; break;
	case InventoryType::MSG_BLOCK: os << "MSG_BLOCK "; break;
	case InventoryType::MSG_FILTERED_BLOCK: os << "MSG_FILTERED_BLOCK "; break;
	}
	os << HashValue;
	return os.str();
}

void InvGetDataMessage::Write(BinaryWriter& wr) const {
	CoinSerialized::Write(wr, Invs);
}

void InvGetDataMessage::Read(const BinaryReader& rd) {
	CoinSerialized::Read(rd, Invs);
}

void InvGetDataMessage::Process(P2P::Link& link) {
	if (Invs.size() > MAX_INV_SZ)
		throw PeerMisbehavingExc(20);
}

String InvGetDataMessage::ToString() const {
	ostringstream os;
	for (int i=0; i<Invs.size(); ++i)
		os << (i ? ", " : "     ") << Invs[i];
	return base::ToString() + String(os.str());
}

void InvMessage::Process(P2P::Link& link) {
	CoinEng& eng = static_cast<CoinEng&>(*link.Net);

	base::Process(link);
	eng.m_bSomeInvReceived = true;

	ptr<GetDataMessage> m = new GetDataMessage;
	if (!eng.BestBlock()) {
		TRC(1, "Requesting for block " << eng.ChainParams.Genesis);

		m->Invs.push_back(Inventory(InventoryType::MSG_BLOCK, eng.ChainParams.Genesis));
	}

	Inventory invLastBlock;
	EXT_FOR (const Inventory& inv, Invs) {
#ifdef X_DEBUG//!!!D
		if (inv.Type==InventoryType::MSG_BLOCK) {
			String s = " ";
			if (Block block = eng.LookupBlock(inv.HashValue))
				s += Convert::ToString(block.Height);
			TRC(3, "Inv block " << inv.HashValue << s);
		}
#endif

		if (eng.Have(inv)) {
			if (inv.Type==InventoryType::MSG_BLOCK) {
				Block orphanRoot(nullptr);
				EXT_LOCK (eng.Caches.Mtx) {
					Block block(nullptr);
					if (Lookup(eng.Caches.OrphanBlocks, inv.HashValue, block))
						orphanRoot = block.GetOrphanRoot();
				}
				if (orphanRoot)
					link.Send(new GetBlocksMessage(eng.BestBlock(), Hash(orphanRoot)));
				else
					invLastBlock = inv;							// Always request the last block in an inv bundle (even if we already have it), as it is the trigger for the other side to send further invs.
														// If we are stuck on a (very long) side chain, this is necessary to connect earlier received orphan blocks to the chain again.
			}
		} else {
			if (inv.Type == InventoryType::MSG_BLOCK)
				m->Invs.push_back(inv);
			else {
				switch (eng.Mode) {
				case EngMode::BlockExplorer:
					break;
				default:
					m->Invs.push_back(inv);
				}
			}
		}
	}
	if (invLastBlock.HashValue != HashValue()) {
		TRC(2, "InvLastBlock: " << invLastBlock.HashValue);

		m->Invs.push_back(invLastBlock);
		link.Send(new GetBlocksMessage(eng.LookupBlock(invLastBlock.HashValue), HashValue()));
	}

	if (!m->Invs.empty()) {
		m->Invs.resize(std::min(m->Invs.size(), MAX_INV_SZ));
		link.Send(m);
	}
}

void GetDataMessage::Process(P2P::Link& link) {
	CoinEng& eng = static_cast<CoinEng&>(*link.Net);
	base::Process(link);
	CoinLink& clink = static_cast<CoinLink&>(link);

	if (eng.LiteMode)
		return;

	EXT_FOR (const Inventory& inv, Invs) {
		if (Thread::CurrentThread->m_bStop)
			break;
		switch (inv.Type) {
		case InventoryType::MSG_BLOCK:
		case InventoryType::MSG_FILTERED_BLOCK:
//#if UCFG_DEBUG //!!!
//			break;
//#endif
			if (eng.Db->IsSlow)
				break;		// SQLite is too slow

			{
				if (Block block = eng.LookupBlock(inv.HashValue)) {
			//		ASSERT(EXT_LOCKED(eng.Mtx, (block.m_pimpl->m_hash.reset(), Hash(block)==inv.HashValue)));

					if (InventoryType::MSG_BLOCK == inv.Type) {
						TRC(1, "Block Message sending");
						link.Send(new BlockMessage(block));
					} else {
						ptr<MerkleBlockMessage> mbm;
						vector<Tx> matchedTxes;
						EXT_LOCK (clink.MtxFilter) {
							if (clink.Filter)
								matchedTxes = (mbm = new MerkleBlockMessage)->Init(block, *clink.Filter);
						}
						if (mbm) {
							link.Send(mbm);
							EXT_FOR (const Tx& tx, matchedTxes) {
								if (!EXT_LOCKED(clink.Mtx, clink.KnownInvertorySet.count(Inventory(InventoryType::MSG_TX, Hash(tx)))))
									link.Send(new TxMessage(tx));									
							}
						}
					}
					
					if (inv.HashValue == clink.HashContinue) {
						ptr<InvMessage> m = new InvMessage;
						m->Invs.push_back(Inventory(InventoryType::MSG_BLOCK, Hash(eng.BestBlock())));
						link.Send(m);
						clink.HashContinue = HashValue();
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
			}
			break;
		}
	}
}

String TxMessage::ToString() const {
	return base::ToString() + String(EXT_STR(" " << Hash(Tx)));
}

void BlockMessage::Process(P2P::Link& link) {
	CoinEng& eng = Eng();

	CoinLink& clink = static_cast<CoinLink&>(link);
	HashValue hash = Hash(Block);
	EXT_LOCKED(clink.Mtx, clink.KnownInvertorySet.insert(Inventory(InventoryType::MSG_BLOCK, hash)));
	if (eng.UpgradingDatabaseHeight)
		return;
	Block.Process(&link);
}

String BlockMessage::ToString() const {
	ostringstream os;
	if (Block.Height >= 0)
		os << " " << Block.Height;
	os << " " << Hash(Block);
	return base::ToString() + String(os.str());
}

void PingMessage::Process(P2P::Link& link) {
	ptr<PongMessage> m = new PongMessage;
	m->Nonce = Nonce;
	link.Send(m);
}

DateTime CoinEng::GetTimestampForNextBlock() {
	Int64 epoch = to_time_t(std::max(BestBlock().GetMedianTimePast()+TimeSpan::FromSeconds(1), DateTime::UtcNow()));		// round to seconds
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

void CoinEng::CheckCoinbasedTxPrev(int height, const Tx& txPrev) {
	if (height-txPrev.Height < ChainParams.CoinbaseMaturity)
		Throw(E_COIN_RecentCoinbase);
}

void IBlockChainDb::Recreate() {
	CoinEng& eng = Eng();
	Close();
	File::Delete(eng.GetDbFilePath());
	eng.CreateDb();
}

Version CheckUserVersion(SqliteConnection& db) {
	Version ver = theApp.ProductVersion;
	SqliteCommand cmd("PRAGMA user_version", db);
	int dbver = (int)cmd.ExecuteInt64Scalar();
	Version dver = Version(dbver >> 16, dbver & 0xFFFF);
	if (dver > ver && (dver.Major!=ver.Major || dver.Minor/10 != ver.Minor/10)) {					// Versions [mj.x0 .. mj.x9] are DB-compatible
		db.Close();
		VersionExc exc;
		exc.Version = dver;
		throw exc;
	}
	return dver;
}

} // Coin::

