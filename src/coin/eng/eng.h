/*######   Copyright (c) 2011-2015 Ufasoft  http://ufasoft.com  mailto:support@ufasoft.com,  Sergey Pavlov  mailto:dev@ufasoft.com ####
#                                                                                                                                     #
# 		See LICENSE for licensing information                                                                                         #
#####################################################################################################################################*/

#pragma once

#include <el/db/ext-sqlite.h>
#include "irc.h"

#include <el/crypto/hash.h>

#ifndef UCFG_COIN_ECC
#	define UCFG_COIN_ECC 'S'			// Secp256k1
#endif

#if UCFG_COIN_ECC=='S'
#	include <crypto/cryp/secp256k1.h>
#endif

#include <el/crypto/bloom-filter.h>
using namespace Ext::Crypto;

#include <el/inet/p2p-net.h>
#include <el/inet/detect-global-ip.h>
using namespace Ext::Inet;

using P2P::Link;
using P2P::Peer;

#include "coin-model.h"

#if UCFG_COIN_GENERATE
#	include "../miner/miner.h"
#endif

#ifndef UCFG_COIN_DEFAULT_PORT
#	define UCFG_COIN_DEFAULT_PORT 8333
#endif

#include "crypter.h"

using P2P::NetManager;

namespace Coin {

class CoinEng;
class Inventory;
class CoinDb;
class CoinLink;
class VersionMessage;
class VerackMessage;
class ChainParams;
class IBlockChainDb;
}

namespace Coin {


const uint64_t NODE_NETWORK = 1;


class CoinPeer : public P2P::Peer {
public:
	CoinPeer() {
		m_services = NODE_NETWORK;
	}
};

class MsgLoopThread : public Thread {
	typedef Thread base;
public:
	CoinDb& m_cdb;
	AutoResetEvent m_ev;

	MsgLoopThread(thread_group& tr, CoinDb& cdb)
		:	base(&tr)
		,	m_cdb(cdb)
	{
//		StackSize = UCFG_THREAD_STACK_SIZE;
	}

	void Execute() override;

	void Stop() override {
		m_bStop = true;
		m_ev.Set();
	}
};

class CurrencyFactoryBase : public StaticList<CurrencyFactoryBase> {
	typedef StaticList<CurrencyFactoryBase> base;
public:
	String Name;

	virtual CoinEng *CreateEng(CoinDb& cdb);
protected:
	CurrencyFactoryBase()
		:	base(true)
	{}
};

template <class T>
class CurrencyFactory : public CurrencyFactoryBase {
public:
	CurrencyFactory(RCString name) {
		Name = name;
	}

	CoinEng *CreateEng(CoinDb& cdb) override {
		return new T(cdb);
	}
};

class ChainParams {
	typedef ChainParams class_type;
public:
	String Name, Symbol;
	HashValue Genesis;
	seconds BlockSpan;
	int64_t InitBlockValue;
	int64_t CoinValue;
	int64_t MaxMoney;
	int64_t MinTxFee;
	int64_t MinTxOutAmount;
	int HalfLife;
	int CoinbaseMaturity;
	int AnnualPercentageRate;
	int TargetInterval;
	Target MaxTarget, InitTarget;
	Target MaxPossibleTarget;
	uint32_t ProtocolMagic;
	uint32_t ProtocolVersion;
	uint16_t DefaultPort;
	vector<String> BootUrls;
	int IrcRange;
	int PayToScriptHashHeight;
	int CheckDupTxHeight;
	double PowOfDifficultyToHalfSubsidy;

	CBool AuxPowEnabled;
	Coin::HashAlgo HashAlgo;
	CBool FullPeriod;
	CInt<int> ChainId;
	CInt<byte> AddressVersion, ScriptAddressVersion;
	int AuxPowStartBlock;
	bool IsTestNet;
	bool AllowLiteMode;
	bool MiningAllowed;
	bool Listen;
	size_t MedianTimeSpan;
	
	typedef unordered_map<int, HashValue> CCheckpoints;
	CCheckpoints Checkpoints;

	unordered_set<IPEndPoint> Seeds;
	int LastCheckpointHeight;
	vector<Blob> AlertPubKeys;
	Blob CheckpointMasterPubKey;

	ChainParams()
		:	LastCheckpointHeight(-1)
	{
		Init();
	}	

	void Init();
	void LoadFromXmlAttributes(IXmlAttributeCollection& xml);
	int Log10CoinValue() const;				// precise calculation of log10(CoinValue)
	void AddSeedEndpoint(RCString seed);
private:
	void ReadParam(int& param, IXmlAttributeCollection& xml, RCString name);
	void ReadParam(int64_t& param, IXmlAttributeCollection& xml, RCString name);
};

class Alert : public Object, public CPersistent {
public:
	int32_t Ver;
	DateTime RelayUntil, Expiration;
	int32_t NId, NCancel;
	set<int32_t> SetCancel;
	int32_t MinVer, MaxVer;
	set<String> SetSubVer;
	int32_t Priority;
	String Comment, StatusBar, Reserved;

	void Read(const BinaryReader& rd) override;
};

interface IIWalletEvents {
public:
	virtual void OnStateChanged() =0;
};

interface COIN_CLASS WalletBase : public Object {
	typedef WalletBase class_type;
public:
	typedef InterlockedPolicy interlocked_policy;

	observer_ptr<CoinEng> m_eng;
	float Speed;
	observer_ptr<IIWalletEvents> m_iiWalletEvents;

	WalletBase(CoinEng& eng)
		:	m_eng(&eng)
		,	Speed(0)
	{}

	WalletBase()
		:	Speed(0)
	{}

	CoinEng& get_Eng() { return *m_eng; }
	DEFPROP_GET(CoinEng&, Eng);

	virtual pair<Blob, HashValue160> GetReservedPublicKey() { Throw(E_NOTIMPL); }
	virtual void OnCreateDatabase() {}
	virtual void OnCloseDatabase() {}
	virtual void OnProcessBlock(const Block& block) {}
	virtual void OnBlockConnectDbtx(const Block& block) {}
	virtual void OnBeforeCommit() {}
	virtual void OnProcessTx(const Tx& tx) {}
	virtual void OnEraseTx(const HashValue& hashTx) {}
	virtual void OnBlockchainChanged() {}
	virtual void OnSetProgress(float v) {}
	virtual void OnChange() {}
	virtual void OnAlert(Alert *alert) {}
	virtual bool IsFromMe(const Tx& tx) { return false; }
	virtual void OnPeriodic() {}
	virtual void OnPeriodicForLink(CoinLink& link) {}	
	virtual void AddRecipient(const Address& a) { Throw(E_NOTIMPL); }

#if UCFG_COIN_GENERATE
	mutex MtxMiner;
	unique_ptr<BitcoinMiner> Miner;

	Blob m_genPubKey;
	HashValue160 m_genHash160;
	void ReserveGenKey();
	Tx CreateCoinbaseTx();
	virtual Block CreateNewBlock();
	void RegisterForMining(WalletBase* wallet);			// only single wallet for each Eng allowed
	void UnregisterForMining(WalletBase* wallet);
#else
	virtual Block CreateNewBlock() { Throw(E_NOTIMPL); }
#endif // UCFG_COIN_GENERATE
};

class CoinFilter : public BloomFilter, public Object, public CPersistent {
	typedef BloomFilter base;
public:
	typedef InterlockedPolicy interlocked_policy;

	static const byte BLOOM_UPDATE_NONE = 0,
		BLOOM_UPDATE_ALL = 1,
		BLOOM_UPDATE_P2PUBKEY_ONLY = 2,
		BLOOM_UPDATE_MASK = 3;

	uint32_t Tweak;
	byte Flags;

	CoinFilter()
		:	Tweak(0)
		,	Flags(0)
	{}

	CoinFilter(int nElements, double falsePostitiveRate, uint32_t tweak, byte flags);

	using base::Contains;
	bool Contains(const HashValue& hash) const { return base::Contains(hash); }
	bool Contains(const OutPoint& op) const { return base::Contains(op.ToArray()); }

	using base::Insert;
	void Insert(const OutPoint& op) { base::Insert(op.ToArray()); }

	void Write(BinaryWriter& wr) const override;
	void Read(const BinaryReader& rd) override;
	bool IsRelevantAndUpdate(const Tx& tx);
protected:
	size_t Hash(const ConstBuf& cbuf, int n) const override;
	ConstBuf FindScriptData(const ConstBuf& script) const;
};

class COIN_CLASS CoinDb : /*!!!R public P2P::DetectGlobalIp,*/ public NetManager {
	typedef NetManager base;
	typedef CoinDb class_type;
public:
	recursive_mutex MtxDb;

	path DbWalletFilePath, DbPeersFilePath;
	String FilenameSuffix;
	SqliteConnection m_dbWallet;
	SqliteCommand  CmdAddNewKey;
	SqliteCommand m_cmdIsFromMe, CmdGetBalance, CmdRecipients, CmdGetTxId, CmdGetTx, CmdGetTxes, CmdPendingTxes, CmdSetBestBlockHash,
		CmdFindCoin, CmdInsertCoin;

	recursive_mutex MtxDbPeers;
	SqliteConnection m_dbPeers;
	SqliteCommand CmdPeerByIp, CmdInsertPeer, CmdUpdatePeer,
		CmdEndpointByIp, CmdInsertEndpoint, CmdUpdateEndpoint;
	
	typedef unordered_map<HashValue160, MyKeyInfo> CHash160ToMyKey;
	CHash160ToMyKey Hash160ToMyKey;
	ptr<CoinFilter> Filter;

	thread_group m_tr;
	ptr<Coin::MsgLoopThread> MsgLoopThread;

	Coin::IrcManager IrcManager;

	array<byte, 8> Salt;
	String m_masterPassword;
	pair<Blob, Blob> m_cachedMasterKey;				// for DEFAULT_PASSWORD_ENCRYPT_METHOD

	IPAddress LocalIp4, LocalIp6;
	String ProxyString;
	CBool m_bLoaded;

	CoinDb();
	~CoinDb();

	MyKeyInfo& AddNewKey(MyKeyInfo& ki);
	MyKeyInfo *FindReservedKey();
	void TopUpPool(int lim = KEYPOOL_SIZE);
	const MyKeyInfo& GenerateNewAddress(RCString comment, int lim = KEYPOOL_SIZE);
	void RemovePubKeyFromReserved(const Blob& pubKey);
	void RemovePubHash160FromReserved(const HashValue160& hash160);

	bool get_WalletDatabaseExists();
	DEFPROP_GET(bool, WalletDatabaseExists);

	bool get_PeersDatabaseExists();
	DEFPROP_GET(bool, PeersDatabaseExists);

	void LoadKeys(RCString password = nullptr);
	void Load();
	void Close();

	bool get_NeedPassword();
	DEFPROP_GET(bool, NeedPassword);

	String get_Password() { Throw(E_NOTIMPL); }
	void put_Password(RCString password);
	DEFPROP(String, Password);

	void ChangePassword(RCString oldPassword, RCString newPassword);
	MyKeyInfo GetMyKeyInfo(const HashValue160& hash160);

	bool SetPrivkeyComment(const HashValue160& hash160, RCString comment);
	void ImportXml(const path& filepath);
	void ImportDat(const path& filepath, RCString password);
	void ImportWallet(const path& filepath, RCString password);
	void ExportWalletToXml(const path& filepath);

//!!!R	void OnIpDetected(const IPAddress& ip) override;
	Link *CreateLink(thread_group& tr) override;
	void BanPeer(Peer& peer) override;
	void SavePeers(int idPeersNet, const vector<ptr<Peer>>& peers);
private:

	void CreateWalletNetDbEntry(int netid, RCString name);	
	void CreateDbWallet();
	
	void CreateDbPeers();

	void CheckPasswordPolicy(RCString password);
	void InitAes(BuggyAes& aes, RCString password, byte encrtyptAlgo);
	void UpdateOrInsertPeer(const Peer& peer);
};


extern EXT_THREAD_PTR(void) t_bPayToScriptHash;

class COIN_CLASS CCoinEngThreadKeeper : public CHasherEngThreadKeeper {
	typedef CHasherEngThreadKeeper base;

//!!!R	CoinEng *m_prev;
	bool m_prevPayToScriptHash;
public:
	CCoinEngThreadKeeper(CoinEng *cur);
	~CCoinEngThreadKeeper();
};

class CoinMessage : public P2P::Message, public CoinSerialized, public CPrintable {
	typedef P2P::Message Base;
public:
	String Command;
	uint32_t Checksum;

	CoinMessage(RCString command)
		:	Command(command)
	{
	}

	static ptr<CoinMessage> ReadFromStream(P2P::Link& link, const BinaryReader& rd);
	void Write(BinaryWriter& wr) const override;
	void Read(const BinaryReader& rd) override;
protected:
	String ToString() const override;

	friend class CoinEng;
};

extern const Version
//!!!R	NAMECOIN_VER_DOMAINS_TABLE,
//!!!R	VER_DUPLICATED_TX,
	VER_NORMALIZED_KEYS,
	VER_KEYS32,
	VER_INDEXED_PEERS,
	VER_USE_HASH_TABLE_DB;

ENUM_CLASS(EngMode) {
	DbLess,
	Lite,							// Don't download all block contents block
	BlockParser,					// Download blocks, but don't save txes
	Normal,
	BlockExplorer,					// Keep map<address, txes>
	Bootstrap						// Fast for Huge DB
} END_ENUM_CLASS(EngMode)

inline CoinEng& Eng() noexcept { return *(CoinEng*)HasherEng::GetCurrent(); }
COIN_EXPORT CoinEng& ExternalEng();

class LocatorHashes : public vector<HashValue> {
	typedef LocatorHashes class_type;
public:
	int FindHeightInMainChain() const;

	int get_DistanceBack() const;
	DEFPROP_GET(int, DistanceBack);
};

class IBlockChainDb : public Object, public ITransactionable  {
public:
	CInt<int> m_nCheckpont;
	String DefaultFileExt;

	IBlockChainDb()
		:	DefaultFileExt(".db")
	{}

	virtual void *GetDbObject()																	=0;
	virtual ITransactionable& GetSavepointObject()												=0;

	virtual bool Create(const path& p)															=0;
	virtual bool Open(const path& p)															=0;			// returns false if DBConvering deffered
	virtual void Close(bool bAsync = true)																		=0;	
	virtual void Recreate();
	
	virtual Version CheckUserVersion()															=0;
	virtual void UpgradeTo(const Version& ver)													=0;
	virtual void TryUpgradeDb(const Version& verApp) {}

	virtual bool HaveBlock(const HashValue& hash)												=0;
	virtual Block FindBlock(const HashValue& hash)												=0;
	virtual Block FindBlock(int height)															=0;
	virtual Block FindBlockPrefixSuffix(int height) { return FindBlock(height); }
	virtual int GetMaxHeight()																	=0;
	virtual TxHashesOutNums GetTxHashesOutNums(int height)										=0;
	virtual vector<uint32_t> InsertBlock(const Block& block, const ConstBuf& data, const ConstBuf& txData) =0;
	virtual void DeleteBlock(int height, const vector<int64_t>& txids)							=0;

	virtual bool FindTx(const HashValue& hash, Tx *ptx)											=0;
	virtual bool FindTxById(const ConstBuf& txid8, Tx *ptx)										=0;
	virtual vector<int64_t> GetTxesByPubKey(const HashValue160& pubkey)							=0;

	virtual void ReadTxIns(const HashValue& hash, const TxObj& txObj)							=0;
	virtual pair<int, int> FindPrevTxCoords(DbWriter& wr, int height, const HashValue& hash)	=0;
	virtual void InsertTx(const Tx& tx, const TxHashesOutNums& hashesOutNums, const HashValue& txHash, int height, const ConstBuf& txIns, const ConstBuf& spend, const ConstBuf& data) =0;

	virtual void InsertSpentTxOffsets(const unordered_map<HashValue, pair<uint32_t, uint32_t>>& spentTxOffsets) { Throw(E_NOTIMPL); }	// used only for EngMode::Bootstrap

	virtual vector<bool> GetCoinsByTxHash(const HashValue& hash)								=0;
	virtual void SaveCoinsByTxHash(const HashValue& hash, const vector<bool>& vec)				=0;
	virtual void UpdateCoins(const OutPoint& op, bool bSpend);

	virtual void BeginEngTransaction()															=0;
	virtual void SetProgressHandler(int(* pfn)(void*), void*p = 0, int n = 1)					=0;
	virtual vector<Block> GetBlockHeaders(const LocatorHashes& locators, const HashValue& hashStop)	=0;

	virtual Blob FindPubkey(int64_t id)															=0;
	virtual void InsertPubkey(int64_t id, const ConstBuf& pk)										=0;
	virtual void UpdatePubkey(int64_t id, const ConstBuf& pk)										=0;
	
	virtual void Vacuum() {}
	virtual void BeforeEnsureTransactionStarted() {}

	virtual uint64_t GetBoostrapOffset() { return 0; }
	virtual void SetBoostrapOffset(uint64_t v) {}
};

ptr<IBlockChainDb> CreateBlockChainDb();
ptr<IBlockChainDb> CreateSqliteBlockChainDb();

class COIN_CLASS CoinEng : public HasherEng, public P2P::Net, public ITransactionable {
	typedef CoinEng class_type;
	typedef P2P::Net base;
public:
	CoinDb& m_cdb;

	Coin::ChainParams ChainParams;
	uint16_t MaxBlockVersion;

	recursive_mutex Mtx;

	ptr<IBlockChainDb> Db;
	uint64_t OffsetInBootstrap, NextOffsetInBootstrap;

	int m_idPeersNet;
	std::exception_ptr CriticalException;
	
	ChainCaches Caches;
	
	// Tx Pool
	class CTxPool {
	public:
		recursive_mutex Mtx;

		typedef unordered_map<HashValue, Tx> CHashToTx;
		CHashToTx m_hashToTx;

		typedef unordered_map<OutPoint, Tx> COutPointToNextTx;
		COutPointToNextTx m_outPointToNextTx;

		typedef unordered_map<HashValue, Tx> CHashToOrphan;
		typedef unordered_multimap<HashValue, HashValue> CHashToHash;
		CHashToOrphan m_hashToOrphan;
		CHashToHash m_prevHashToOrphanHash;
	} TxPool;

	observer_ptr<WalletBase> m_iiEngEvents;

	static const int HEIGHT_BOOTSTRAPING = -2;
	int UpgradingDatabaseHeight;

	mutex m_mtxStates;
	set<String> m_setStates;
	
	ptr<Coin::ChannelClient> ChannelClient;
	int CommitPeriod;	

	ptr<CoinFilter> Filter;

	CBool InWalJournalMode;
	CBool JournalModeDelete;
	CBool m_bSomeInvReceived;
	CBool m_dbTxBegin;
	bool AllowFreeTxes;

	EngMode get_Mode() { return m_mode; }
	void put_Mode(EngMode mode);
	DEFPROP(EngMode, Mode);

	CoinEng(CoinDb& cdb);
	~CoinEng();
	static String GetCoinChainsXml();
	static ptr<CoinEng> CreateObject(CoinDb& cdb, RCString name);
	void ContinueLoad0();
	void ContinueLoad();
	virtual void Load();
	virtual void Close();
	void PurgeDatabase();

	virtual ptr<IBlockChainDb> CreateBlockChainDb();

	void Start() override;
	void SignalStop();
	void Stop();

	Block BestBlock();
	void SetBestBlock(const Block& b);
	int BestBlockHeight();

	path m_dbFilePath;
	path GetDbFilePath();

	bool CheckSelfVerNonce(uint64_t nonce) {
		bool bSelf = false;
		EXT_LOCK (m_mtxVerNonce) {
			 if (bSelf = m_nonce2link.count(nonce))
				m_nonce2link[nonce]->OnSelfLink();
		}
		return !bSelf;
	}

	void RemoveVerNonce(P2P::Link& link) {
		EXT_LOCK (m_mtxVerNonce) {
			for (CNonce2link::iterator it=m_nonce2link.begin(), e=m_nonce2link.end(); it!=e; ++it)
				if (it->second.get() == &link) {
					m_nonce2link.erase(it);
					break;
				}
		}
	}

	virtual void SetChainParams(const Coin::ChainParams& p);
	void SendVersionMessage(Link& link);	
	bool HaveBlockInMainTree(const HashValue& hash);
	bool HaveBlock(const HashValue& hash);
	bool HaveTxInDb(const HashValue& hashTx);
	bool Have(const Inventory& inv);
	Block GetPrevBlockPrefixSuffixFromMainTree(const Block& block);
	bool IsFromMe(const Tx& tx);
	void AddToMemory(const Tx& tx);
	void RemoveFromMemory(const Tx& tx);	
	void Push(const Inventory& inv);
	void Push(const Tx& tx);
	bool AddToPool(const Tx& tx, vector<HashValue>& vQueue);
	void ExportToBootstrapDat(const path& pathBoostrap);
	Block GetBlockByHeight(uint32_t height);
	Block LookupBlock(const HashValue& hash);
	static Blob SpendVectorToBlob(const vector<bool>& vec);
	void EnsureTransactionStarted();
	void CommitTransactionIfStarted();

	virtual HashValue HashMessage(const ConstBuf& cbuf);
	virtual HashValue HashForSignature(const ConstBuf& cbuf);
	virtual HashValue HashFromTx(const Tx& tx);

	virtual void OnCheck(const Tx& tx) {}
	virtual void OnConnectInputs(const Tx& tx, const vector<Tx>& vTxPrev, bool bBlock, bool bMiner) {}
	virtual void OnConnectBlock(const Block& block) {}
	virtual void OnDisconnectInputs(const Tx& tx) {}
	virtual bool ShouldBeSaved(const Tx& tx) { return Mode!=EngMode::Lite && Mode!=EngMode::BlockParser; }
	virtual void CheckForDust(const Tx& tx) {}

	virtual int64_t GetSubsidy(int height, const HashValue& prevBlockHash, double difficulty = 0, bool bForCheck = false) {
		return ChainParams.InitBlockValue >> (height/ChainParams.HalfLife);
	}

	virtual void CheckCoinbasedTxPrev(int height, const Tx& txPrev);

	DateTime GetTimestampForNextBlock();

	void AddOrphan(const Tx& tx);
	void EraseOrphanTx(const HashValue& hash);
	void Relay(const Tx& tx);

	bool IsInitialBlockDownload();
	void OnPeriodicMsgLoop() override;
	CoinPeer *CreatePeer() override;
//!!!R	void FillAddress(Address& addr, RCString s);

	int64_t CheckMoneyRange(int64_t v) {
		if (v < 0 || v > ChainParams.MaxMoney)
			Throw(CoinErr::MoneyOutOfRange);
		return v;
	}

	virtual double ToDifficulty(const Target& target);

	virtual int64_t GetMinRelayTxFee() {
		return ChainParams.CoinValue/10000;
	}

	virtual Target GetNextTarget(const Block& blockLast, const Block& block);

	bool GetPkId(const HashValue160& hash160, CIdPk& id);
	bool GetPkId(const ConstBuf& cbuf, CIdPk& id);
	HashValue160 GetHash160ById(int64_t id);
	Blob GetPkById(int64_t rowid);
	virtual void UpgradeDb(const Version& ver);

	virtual CoinMessage *CreateVersionMessage();
	virtual CoinMessage *CreateVerackMessage();
	virtual CoinMessage *CreateAddrMessage();
	virtual CoinMessage *CreateInvMessage();
	virtual CoinMessage *CreateGetDataMessage();
	virtual CoinMessage *CreateNotFoundMessage();
	virtual CoinMessage *CreateGetBlocksMessage();
	virtual CoinMessage *CreateGetHeadersMessage();
	virtual CoinMessage *CreateTxMessage();
	virtual CoinMessage *CreateBlockMessage();
	virtual CoinMessage *CreateHeadersMessage();
	virtual CoinMessage *CreateGetAddrMessage();
	virtual CoinMessage *CreateCheckOrderMessage();
	virtual CoinMessage *CreateSubmitOrderMessage();
	virtual CoinMessage *CreateReplyMessage();
	virtual CoinMessage *CreatePingMessage();
	virtual CoinMessage *CreatePongMessage();
	virtual CoinMessage *CreateMemPoolMessage();
	virtual CoinMessage *CreateAlertMessage();
	virtual CoinMessage *CreateMerkleBlockMessage();
	virtual CoinMessage *CreateFilterLoadMessage();
	virtual CoinMessage *CreateFilterAddMessage();
	virtual CoinMessage *CreateFilterClearMessage();
	virtual CoinMessage *CreateRejectMessage();
	virtual CoinMessage *CreateCheckPointMessage();

	virtual TxObj *CreateTxObj() { return new TxObj; }	
	virtual	bool CreateDb();
	virtual bool OpenDb();
	path GetBootstrapPath();

	void BeginTransaction() override {}
	void Commit() override {}
	void Rollback() override {}
protected:
	void StartIrc();

	size_t GetMessageHeaderSize() override;
	size_t GetMessagePayloadSize(const ConstBuf& buf) override;
	ptr<P2P::Message> RecvMessage(Link& link, const BinaryReader& rd) override;
	
	void OnInitLink(Link& link) override {
		P2P::Net::OnInitLink(link);
		if (!link.Incoming) {
			CCoinEngThreadKeeper engKeeper(this);
			SendVersionMessage(link);
		}
	}

	void SavePeers() override;

	void OnMessageReceived(P2P::Message *m) override;
	void OnPingTimeout(P2P::Link& link) override;
	void AddLink(P2P::LinkBase *link) override;	
	void OnCloseLink(P2P::LinkBase& link) override;
	
	virtual BlockObj *CreateBlockObj() { return new BlockObj; }	
	virtual void TryUpgradeDb();
	virtual int GetIntervalForModDivision(int height);
	virtual int GetIntervalForCalculatingTarget(int height);
	Target KimotoGravityWell(const Block& blockLast, const Block& block, int minBlocks, int maxBlocks);
	virtual TimeSpan AdjustSpan(int height, const TimeSpan& span, const TimeSpan& targetSpan);
	virtual TimeSpan GetActualSpanForCalculatingTarget(const BlockObj& bo, int nInterval);
	virtual Target GetNextTargetRequired(const Block& blockLast, const Block& block);
	virtual void UpdateMinFeeForTxOuts(int64_t& minFee, const int64_t& baseFee, const Tx& tx);
	virtual path VGetDbFilePath();
private:
	EngMode m_mode;

	mutex m_mtxThreadStateChange;	

	mutex m_mtxVerNonce;
	typedef unordered_map<uint64_t, ptr<Link>> CNonce2link;
	CNonce2link m_nonce2link;

	mutex MtxBestUpdate;
	DateTime m_dtPrevBestUpdate;
	HashValue m_hashPrevBestUpdate;

	DateTime m_dtLastFreeTx;
	double m_freeCount;

	void UpgradeTo(const Version& ver);
	void LoadLastBlockInfo();
	void Misbehaving(P2P::Message *m, int howMuch, RCString command = nullptr, RCString reason = nullptr);

	friend class Block;
	friend class Tx;
	friend class BlockObj;
	friend class VersionMessage;
	friend class CoinMessage;
};

class CoinEngTransactionScope : noncopyable {
public:
	CoinEng& Eng;
	ITransactionable& m_db;

	CoinEngTransactionScope(CoinEng& eng)
		:	Eng(eng)
		,	m_db(Eng.Db->GetSavepointObject())
	{
		Eng.BeginTransaction();
		m_db.BeginTransaction();
	}

	~CoinEngTransactionScope() {
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

	void Commit() {
		m_db.Commit();
		Eng.Commit();
		m_bCommitted = true;
	}
private:
	CInException InException;
	CBool m_bCommitted;
};

class BootstrapDbThread : public Thread {
	typedef Thread base;
public:
	CoinEng& Eng;
	path PathBootstrap;
	CBool Exporting;

	BootstrapDbThread(CoinEng& eng, const path& pathBootstrap)
		:	base(&eng.m_tr)
		,	Eng(eng)
		,	PathBootstrap(pathBootstrap)
	{}
protected:
	void BeforeStart() override;
	void Execute() override;
};

class CoinEngApp : public CAppBase {
	typedef CoinEngApp class_type;
public:
	CUsingSockets m_usingSockets;

	CoinEngApp();

	Version get_ProductVersion();
	DEFPROP_GET(Version, ProductVersion);
};

//extern CoinEngApp theApp;

void SetUserVersion(SqliteConnection& db, const Version& ver = Version());

Blob ToUncompressedKey(const ConstBuf& cbuf);
Blob ToCompressedKey(const ConstBuf& cbuf);

class TxNotFoundException : public Exception {
	typedef Exception base;
public:
	HashValue HashTx;

	TxNotFoundException(const HashValue& hashTx)
		:	base(CoinErr::TxNotFound)
		,	HashTx(hashTx)
	{
	}
protected:
	String get_Message() const override {
		return EXT_STR(base::get_Message() << " " << HashTx);
	}
};

class PeerMisbehavingException : public Exception {
	typedef Exception base;
public:
	int HowMuch;

	PeerMisbehavingException(int howMuch = 1)
		:	base(CoinErr::Misbehaving)
		,	HowMuch(howMuch)
	{}
};


class VersionException : public Exception {
	typedef Exception base;
public:
	Ext::Version Version;

	VersionException(const Ext::Version& ver = Ext::Version())
		:	base(ExtErr::DB_Version)
		,	Version(ver)
	{
	}

	~VersionException() noexcept {}		//!!! GCC 4.6

protected:
	String get_Message() const override {
		return base::get_Message() + " " + Version.ToString(2);
	}
};

Version CheckUserVersion(SqliteConnection& db);

class CEngStateDescription : noncopyable {
public:
	CEngStateDescription(CoinEng& eng, RCString s);
	~CEngStateDescription();
private:
	CoinEng& m_eng;
	String m_s;
};


} // Coin::


