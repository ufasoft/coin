/*######   Copyright (c) 2011-2019 Ufasoft  http://ufasoft.com  mailto:support@ufasoft.com,  Sergey Pavlov  mailto:dev@ufasoft.com ####
#                                                                                                                                     #
# 		See LICENSE for licensing information                                                                                         #
#####################################################################################################################################*/

#pragma once

#include "irc.h"
#include <el/db/ext-sqlite.h>

#include <el/crypto/hash.h>

#include <el/crypto/bloom-filter.h>
using namespace Ext::Crypto;

#include <el/inet/detect-global-ip.h>
#include <el/inet/p2p-net.h>
using namespace Ext::Inet;

#include <el/libext/conf.h>


using P2P::Peer;

#include "coin-model.h"
#include "../util/opcode.h"

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
class Link;
class VersionMessage;
class VerackMessage;
class ChainParams;
class IBlockChainDb;
class GetDataMessage;
class Link;

extern const Version
	VER_BLOCKS_TABLE_IS_HASHTABLE,
	VER_PUBKEY_RECOVER,
	VER_HEADERS,
	DB_VER_COMPACT_UTXO,
	DB_VER_LATEST;

struct QueuedBlockItem {
	HashValue HashBlock;
	DateTime DtGetdataReq;
	Coin::Link& Link;

	QueuedBlockItem(Coin::Link& link, const HashValue& hashBlock, const DateTime& dtGetdataReq) : Link(link), HashBlock(hashBlock), DtGetdataReq(dtGetdataReq) {}
};

typedef list<QueuedBlockItem> BlocksInFlightList;

class CoinPeer : public P2P::Peer {
public:
	CoinPeer() { m_services = NodeServices::NODE_NETWORK; }
};

class MsgLoopThread : public Thread {
	typedef Thread base;

public:
	CoinDb& m_cdb;
	AutoResetEvent m_ev;

	MsgLoopThread(thread_group& tr, CoinDb& cdb) : base(&tr), m_cdb(cdb) {
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

	virtual CoinEng* CreateEng(CoinDb& cdb);

protected:
	CurrencyFactoryBase() : base(true) {}
};

template <class T> class CurrencyFactory : public CurrencyFactoryBase {
public:
	CurrencyFactory(RCString name) { Name = name; }

	CoinEng* CreateEng(CoinDb& cdb) override { return new T(cdb); }
};

class ChainParams {
	typedef ChainParams class_type;

public:
	HashValue Genesis;
	vector<String> BootUrls;

	vector<Blob> AlertPubKeys;
	Blob CheckpointMasterPubKey;

	typedef vector<pair<int, HashValue>> CCheckpoints;
	CCheckpoints Checkpoints;
	int LastCheckpointHeight;

	unordered_set<IPEndPoint> Seeds;
	String Name, Symbol;
	seconds BlockSpan;
	int64_t InitBlockValue;
	int64_t CoinValue;
	uint64_t MaxMoney;
    int64_t MinTxFee, DustRelayFee;
	int64_t MinTxOutAmount;
	double PowOfDifficultyToHalfSubsidy;
	int HalfLife;
	int CoinbaseMaturity;
	int AnnualPercentageRate;
	int TargetInterval;

	Target MaxTarget, InitTarget;
	HashValue HashValueMaxTarget;		// function of MaxTarget
	Target MaxPossibleTarget;

	uint32_t ProtocolMagic, DiskMagic;
	uint32_t ProtocolVersion;
	uint16_t DefaultPort;
	unsigned MaxBlockWeight;
	int IrcRange;
	int PayToScriptHashHeight;
	int CheckDupTxHeight;
	int BIP34Height, BIP65Height, BIP66Height, BIP68Height, SegwitHeight;

	CBool AuxPowEnabled;
	Coin::HashAlgo HashAlgo;
	CBool FullPeriod;
	CInt<int> ChainId;
	CInt<uint8_t> AddressVersion, ScriptAddressVersion;
	String Hrp;
	int AuxPowStartBlock;
	unsigned MedianTimeSpan;
	bool IsTestNet;
	bool AllowLiteMode;
	bool MiningAllowed;
	bool Listen;

	ChainParams() : LastCheckpointHeight(-1) { Init(); }

	void Init();
	void LoadFromXmlAttributes(IXmlAttributeCollection& xml);
	int Log10CoinValue() const; // precise calculation of log10(CoinValue)
	void AddSeedEndpoint(RCString seed);

private:
	void ReadParam(int& param, IXmlAttributeCollection& xml, RCString name);
	void ReadParam(int64_t& param, IXmlAttributeCollection& xml, RCString name);
};

class Alert : public NonInterlockedObject, public CPersistent {
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
	virtual void OnStateChanged() = 0;
};

interface IEngEvents {
	virtual void OnCreateDatabase() {}
	virtual void OnCloseDatabase() {}
	virtual void OnBestBlock(const Block& block) {}
	virtual void OnProcessBlock(const BlockHeader& block) {}
	virtual void OnBlockConnectDbtx(const Block& block) {}
	virtual void OnBeforeCommit() {}
	virtual void OnProcessTx(const Tx& tx) {}
	virtual void OnEraseTx(const HashValue& hashTx) {}
	virtual void OnBlockchainChanged() {}
	virtual void OnSetProgress(float v) {}
	virtual void OnChange() {}
	virtual void OnAlert(Alert * alert) {}
	virtual void OnPeriodic(const DateTime& now) {}
	virtual void OnPeriodicForLink(Link & link) {}
	virtual bool IsFromMe(const Tx& tx) { return false; }
	virtual void AddRecipient(const Address& a) {}
};

interface ICoinDbEvents {
	virtual void OnFilterChanged() {}
};

interface COIN_CLASS WalletBase : public Object, IEngEvents {
	typedef WalletBase class_type;
public:
	typedef InterlockedPolicy interlocked_policy;

	observer_ptr<CoinEng> m_eng;
	float Speed;
	observer_ptr<IIWalletEvents> m_iiWalletEvents;
#if UCFG_COIN_GENERATE
	KeyInfo m_genKeyInfo;

	mutex MtxMiner;
	unique_ptr<BitcoinMiner> Miner;
#endif
	WalletBase(CoinEng & eng) : m_eng(&eng), Speed(0) {}

	WalletBase() : Speed(0) {}

	CoinEng& get_Eng() { return *m_eng; }
	DEFPROP_GET(CoinEng&, Eng);

	virtual void AddRecipient(const Address& a) { Throw(E_NOTIMPL); }

#if UCFG_COIN_GENERATE
	void ReserveGenKey();
	Tx CreateCoinbaseTx();
	virtual Block CreateNewBlock();
	void RegisterForMining(WalletBase * wallet); // only single wallet for each Eng allowed
	void UnregisterForMining(WalletBase * wallet);
#else
	virtual Block CreateNewBlock() { Throw(E_NOTIMPL); }
#endif // UCFG_COIN_GENERATE
};

class CoinFilter : public BloomFilter, public Object, public CPersistent {
	typedef BloomFilter base;

public:
	typedef InterlockedPolicy interlocked_policy;

	static const uint8_t BLOOM_UPDATE_NONE = 0,
		BLOOM_UPDATE_ALL = 1,
		BLOOM_UPDATE_P2PUBKEY_ONLY = 2,
		BLOOM_UPDATE_MASK = 3;

	uint32_t Tweak;
	uint8_t Flags;

	CoinFilter() : Tweak(0), Flags(0) {}

	CoinFilter(int nElements, double falsePostitiveRate, uint32_t tweak, uint8_t flags);

	using base::Contains;
	bool Contains(const HashValue& hash) const { return base::Contains(hash.ToSpan()); }
	bool Contains(const OutPoint& op) const { return base::Contains(Span(op.ToArray())); }

	using base::Insert;
	void Insert(const OutPoint& op) { base::Insert(Span(op.ToArray())); }

	void Write(BinaryWriter& wr) const override;
	void Read(const BinaryReader& rd) override;
	bool IsRelevantAndUpdate(const Tx& tx);

protected:
	size_t Hash(RCSpan cbuf, int n) const override;
	Span FindScriptData(RCSpan script) const;
};

enum class ScriptContext { Top, P2SH, WitnessV0 };

class COIN_CLASS CoinDb : /*!!!R public P2P::DetectGlobalIp,*/ public NetManager, IEngEvents {
	typedef NetManager base;
	typedef CoinDb class_type;

public:
	recursive_mutex MtxDb;

	path DbWalletFilePath, DbPeersFilePath;
	SqliteConnection m_dbWallet;
	SqliteCommand CmdAddNewKey;
	SqliteCommand m_cmdIsFromMe, CmdGetBalance, CmdRecipients, CmdGetTxId, CmdGetTx, CmdGetTxes, CmdPendingTxes, CmdSetBestBlockHash, CmdFindCoin, CmdInsertCoin;

	recursive_mutex MtxDbPeers;
	SqliteConnection m_dbPeers;
	SqliteCommand CmdPeerByIp, CmdInsertPeer, CmdUpdatePeer, CmdEndpointByIp, CmdInsertEndpoint, CmdUpdateEndpoint;

	typedef unordered_map<HashValue160, KeyInfo> CHash160ToKey;
	CHash160ToKey Hash160ToKey;

	typedef unordered_map<HashValue160, KeyInfo> CP2SHToKey;
	CP2SHToKey P2SHToKey;

	ptr<CoinFilter> Filter;
	DateTime DtEarliestKey;

	CSubscriber<ICoinDbEvents> Events;

	thread_group m_tr;
	ptr<Coin::MsgLoopThread> MsgLoopThread;

	Coin::IrcManager IrcManager;

	array<uint8_t, 8> Salt;
	String m_masterPassword;
	pair<Blob, Blob> m_cachedMasterKey; // for DEFAULT_PASSWORD_ENCRYPT_METHOD

	IPAddress LocalIp4, LocalIp6;
	String ProxyString;
	CBool m_bLoaded, m_bStarted;

	CoinDb();
	~CoinDb();
	KeyInfo AddNewKey(KeyInfo ki);
	KeyInfo FindReservedKey(AddressType type = AddressType::P2SH);
	void TopUpPool();
	KeyInfo GenerateNewAddress(AddressType type, RCString comment);
	void RemoveKeyInfoFromReserved(const KeyInfo& keyInfo);

	bool get_WalletDatabaseExists();
	DEFPROP_GET(bool, WalletDatabaseExists);

	bool get_PeersDatabaseExists();
	DEFPROP_GET(bool, PeersDatabaseExists);

	void UpdateFilter();
	void LoadKeys(RCString password = nullptr);
	void Load();
	KeyInfo FindMine(RCSpan scriptPubKey, ScriptContext context = ScriptContext::Top);
	void Start();
	void Close();

	bool get_NeedPassword();
	DEFPROP_GET(bool, NeedPassword);

	String get_Password() { Throw(E_NOTIMPL); }
	void put_Password(RCString password);
	DEFPROP(String, Password);

	void ChangePassword(RCString oldPassword, RCString newPassword);
	KeyInfo GetMyKeyInfo(const HashValue160& hash160);
	KeyInfo GetMyKeyInfoByScriptHash(const HashValue160& hash160);
	Blob GetMyRedeemScript(const HashValue160& hash160);

	KeyInfo FindPrivkey(const Address& addr);
	bool SetPrivkeyComment(const Address& addr, RCString comment);
	void ImportXml(const path& filepath);
	void ImportDat(const path& filepath, RCString password);
	void ImportWallet(const path& filepath, RCString password);
	void ExportWalletToXml(const path& filepath);

	P2P::Link* CreateLink(thread_group& tr) override;
	void BanPeer(Peer& peer) override;
	void SavePeers(int idPeersNet, const vector<ptr<Peer>>& peers);
	bool IsFilterValid(CoinFilter* filter);
protected:
	void OnBestBlock(const Block& block) override;
private:
	void CreateWalletNetDbEntry(int netid, RCString name);
	void CreateDbWallet();

	void CreateDbPeers();

	void CheckPasswordPolicy(RCString password);
	void InitAes(BuggyAes& aes, RCString password, uint8_t encrtyptAlgo);
	void UpdateOrInsertPeer(const Peer& peer);
};

extern thread_local EnabledFeatures t_features;
extern thread_local ScriptPolicy t_scriptPolicy;

class COIN_CLASS CCoinEngThreadKeeper : public CHasherEngThreadKeeper {
	typedef CHasherEngThreadKeeper base;

	EnabledFeatures m_prevFeatures;
	ScriptPolicy m_prevScriptPolicy;
public:
	CCoinEngThreadKeeper(CoinEng* cur, ScriptPolicy *pScriptPolicy = nullptr, bool enableFeatures = false);
	~CCoinEngThreadKeeper();
};

class CoinMessage : public P2P::Message, public CoinSerialized, public CPrintable {
	typedef P2P::Message Base;
public:
	const char* const Cmd;
	uint32_t Checksum;
	bool WitnessAware = true;

	CoinMessage(const char* cmd)
		: Cmd(cmd)
	{}

	static ptr<CoinMessage> ReadFromStream(Link& link, const BinaryReader& rd);
	virtual void Write(ProtocolWriter& wr) const;
    virtual void Read(const ProtocolReader& rd);
	virtual void Trace(Coin::Link& link, bool bSend) const;
protected:
	void Print(ostream& os) const override;
	virtual void Process(Coin::Link& link) {}
	void ProcessMsg(P2P::Link& link) override;
private:
    void Write(BinaryWriter& wr) const override { Write((ProtocolWriter&)wr); }
    void Read(const BinaryReader& rd) override { Read((const ProtocolReader&)rd); }

	friend class CoinEng;
};

extern const Version
	//!!!R	NAMECOIN_VER_DOMAINS_TABLE,
	//!!!R	VER_DUPLICATED_TX,
	VER_NORMALIZED_KEYS,
	VER_KEYS32,
	VER_INDEXED_PEERS,
	VER_USE_HASH_TABLE_DB,
	VER_SEGWIT;

ENUM_CLASS(EngMode){
	DbLess
	, Lite			// Don't download all block contents block
	, BlockParser	// Download blocks, but don't save txes
	, Normal		// Compact saving, DEPRECATED
	, BlockExplorer // Keep map<address, txes>
	, Bootstrap		// Fast for Huge DB, DEFAULT
} END_ENUM_CLASS(EngMode)

inline CoinEng& Eng() noexcept {
	return *(CoinEng*)HasherEng::GetCurrent();
}
COIN_EXPORT CoinEng& ExternalEng();

class LocatorHashes : public vector<HashValue> {
	typedef LocatorHashes class_type;

public:
	int FindHeightInMainChain(bool bFullBlocks = false) const;
};

class IBlockChainDb : public InterlockedObject, public ITransactionable {
public:
	CInt<int> m_nCheckpont;
	String DefaultFileExt;

	IBlockChainDb() : DefaultFileExt(".db") {}

	virtual void* GetDbObject() = 0;
	virtual ITransactionable& GetSavepointObject() = 0;

	virtual bool Create(const path& p) = 0;
	virtual bool Open(const path& p) = 0; // returns false if DBConvering deffered
	virtual void Close(bool bAsync = true) = 0;
	virtual void Recreate(const Version& verOld);
	virtual void Checkpoint() = 0;

	virtual Version CheckUserVersion() = 0;
	virtual void UpgradeTo(const Version& ver) = 0;
	virtual void TryUpgradeDb(const Version& verApp) {}

	virtual bool HaveHeader(const HashValue& hash) = 0;
	virtual bool HaveBlock(const HashValue& hash) = 0;
	virtual int FindHeight(const HashValue& hash) = 0;
	virtual BlockHeader FindHeader(int height) = 0;
	virtual BlockHeader FindHeader(const HashValue& hash) = 0;
	virtual BlockHeader FindHeader(const BlockRef& bref) = 0;
	virtual optional<pair<uint64_t, uint32_t>> FindBlockOffset(const HashValue& hash) = 0;
	virtual Block FindBlock(const HashValue& hash) = 0;
	virtual Block FindBlock(int height) = 0;
	virtual Block FindBlockPrefixSuffix(int height) { return FindBlock(height); }
	virtual int GetMaxHeight() = 0;
	virtual int GetMaxHeaderHeight() = 0;
	virtual TxHashesOutNums GetTxHashesOutNums(int height) = 0;
	virtual pair<OutPoint, TxOut> GetOutPointTxOut(int height, int idxOut) = 0;
	virtual void InsertBlock(const Block& block, CConnectJob& job) = 0;
	virtual void InsertHeader(const BlockHeader& header, bool bUpdateMaxHeight) = 0;
	virtual void DeleteBlock(int height, const vector<int64_t>* txids) = 0;

	virtual bool FindTx(const HashValue& hash, Tx* ptx) = 0;
	virtual bool FindTxByHash(const HashValue& hashTx, Tx* ptx) = 0;
	virtual vector<int64_t> GetTxesByPubKey(const HashValue160& pubkey) = 0;

	virtual void ReadTxes(const BlockObj& bo) = 0;
	virtual void ReadTxIns(const HashValue& hash, const TxObj& txObj) = 0;
	virtual pair<int, int> FindPrevTxCoords(DbWriter& wr, int height, const HashValue& hash) = 0;
	virtual void InsertTx(const Tx& tx, uint16_t nTx, const TxHashesOutNums& hashesOutNums, const HashValue& txHash, int height, RCSpan txIns, RCSpan spend, RCSpan data, uint32_t txOffset) = 0;

	virtual void InsertSpentTxOffsets(const unordered_map<HashValue, SpentTx>& spentTxOffsets) { Throw(E_NOTIMPL); } // used only for EngMode::Bootstrap

	virtual vector<bool> GetCoinsByTxHash(const HashValue& hash) = 0;
	virtual void SaveCoinsByTxHash(const HashValue& hash, const vector<bool>& vec) = 0;
	virtual void UpdateCoins(const OutPoint& op, bool bSpend, int32_t heightCur);
	virtual void PruneTxo(const OutPoint& op, int32_t heightCur) {}

	virtual void BeginEngTransaction() = 0;
	virtual void SetProgressHandler(int (*pfn)(void*), void* p = 0, int n = 1) = 0;
	virtual vector<BlockHeader> GetBlockHeaders(const LocatorHashes& locators, const HashValue& hashStop) = 0;

	virtual Blob FindPubkey(int64_t id) = 0;
	virtual void InsertPubkey(int64_t id, RCSpan pk) = 0;
	virtual void UpdatePubkey(int64_t id, RCSpan pk) = 0;

	virtual void Vacuum() {}
	virtual void BeforeEnsureTransactionStarted() {}

	virtual uint64_t GetBoostrapOffset() { return 0; }
	virtual void SetBoostrapOffset(uint64_t v) {}

	virtual int32_t GetLastPrunedHeight() { return 0; }
	virtual void SetLastPrunedHeight(int32_t height) {}
	virtual Blob ReadBlob(uint64_t offset, uint32_t size) { return Blob();  }
	virtual void CopyTo(uint64_t offset, uint32_t size, Stream& stm) { }

	virtual ptr<CoinFilter> GetFilter() =0;
	virtual void SetFilter(CoinFilter *filter) = 0;
};

ptr<IBlockChainDb> CreateBlockChainDb();
ptr<IBlockChainDb> CreateSqliteBlockChainDb();

struct BlockTreeItem : public BlockHeader { // may be full block too
	typedef BlockHeader base;

	BlockTreeItem() {}
	BlockTreeItem(const BlockHeader& header);

	/*!!!R
	BlockTreeItem(const HashValue& hashPrev = HashValue::Null(), int height = -1)
		: PrevBlockHash(hashPrev)
		, Height(height)
	{}

	EXPLICIT_OPERATOR_BOOL() const { return !Header ? 0 : EXT_CONVERTIBLE_TO_TRUE; }
	*/
};

class BlockTree {
public:
	CoinEng& Eng;
	mutable mutex Mtx;
	typedef unordered_map<HashValue, BlockTreeItem> CMap; //!!!TODO change to unordered_set<BlockTreeItem> to save space
	CMap Map;
	int HeightLastCheckpointed;
	//----

	BlockTree(CoinEng& eng)
		: Eng(eng)
		, HeightLastCheckpointed(-1)
	{}

	void Clear();
	BlockTreeItem FindInMap(const HashValue& hashBlock) const;
	BlockHeader FindHeader(const HashValue& hashBlock) const;
	BlockHeader FindHeader(const BlockRef& bref) const;
	BlockTreeItem GetHeader(const HashValue& hashBlock) const;	//!!!?R
	BlockTreeItem GetHeader(const BlockRef& bref) const;
	Block FindBlock(const HashValue& hashBlock) const;
	Block GetBlock(const HashValue& hashBlock) const;
	BlockHeader GetAncestor(const HashValue& hashBlock, int height) const;
	BlockHeader LastCommonAncestor(const HashValue& ha, const HashValue& hb) const;
	vector<Block> FindNextBlocks(const HashValue& hashBlock) const;
	void Add(const BlockHeader& header);
	void RemovePersistentBlock(const HashValue& hashBlock);
};

class EngEvents : IEngEvents {
public:
	vector<IEngEvents*> Subscribers;

	void OnCreateDatabase() override;
	void OnCloseDatabase() override;
	void OnBestBlock(const Block& block) override;
	void OnProcessBlock(const BlockHeader& block) override;
	void OnBlockConnectDbtx(const Block& block) override;
	void OnBeforeCommit() override;
	void OnProcessTx(const Tx& tx) override;
	void OnEraseTx(const HashValue& hashTx) override;
	void OnBlockchainChanged() override;
	void OnSetProgress(float v) override;
	void OnChange() override;
	void OnAlert(Alert * alert) override;
	void OnPeriodic(const DateTime& now) override;
	void OnPeriodicForLink(Link & link) override;
	bool IsFromMe(const Tx& tx) override;
	void AddRecipient(const Address& a) override;
};


class TxInfo {
public:
	uint64_t FeeRatePerKB;
	Tx Tx;

	TxInfo() {}
	TxInfo(const class Tx& tx, uint32_t serializationSize);
};

// Tx Pool
class TxPool {
public:
	CoinEng& Eng;

	recursive_mutex Mtx;

	typedef unordered_map<HashValue, TxInfo> CHashToTxInfo;
	CHashToTxInfo m_hashToTxInfo;

	typedef unordered_map<OutPoint, Tx> COutPointToNextTx;
	COutPointToNextTx m_outPointToNextTx;

	typedef unordered_map<HashValue, Tx> CHashToOrphan;
	CHashToOrphan m_hashToOrphan;

	typedef unordered_multimap<HashValue, HashValue> CHashToHash;
	CHashToHash m_prevHashToOrphanHash;
	//----

	TxPool(CoinEng& eng);
	void Add(const TxInfo& txInfo);
	void Remove(const Tx& tx);
	void EraseOrphanTx(const HashValue& hash);
	void AddOrphan(const Tx& tx);
	bool AddToPool(const Tx& tx, vector<HashValue>& vQueue);
	void OnTxMessage(const Tx& tx);
};

class CoinConf : public Ext::Inet::P2P::P2PConf {
public:
	int64_t MinTxFee, MinRelayTxFee;	// Fee rate per KB (1000 bytes)
	String RpcUser, RpcPassword;
	String AddressType, ChangeType;
	int RpcPort, RpcThreads;
	int KeyPool;
	bool Checkpoints, Server, AcceptNonStdTxn, Testnet;

	CoinConf();
	Coin::AddressType GetAddressType() { return ToAddressType(AddressType); }
	Coin::AddressType GetChangeType() { return ChangeType.empty() ? GetAddressType() : ToAddressType(ChangeType); }
private:
	static Coin::AddressType ToAddressType(RCString s);
};

extern CoinConf g_conf;

enum class JumpAction {
	Continue
	, Retry
	, Break
};

class COIN_CLASS CoinEng : public HasherEng, public P2P::Net, public ITransactionable, public ICoinDbEvents {
	typedef CoinEng class_type;
	typedef P2P::Net base;

public:
	CoinDb& m_cdb;

	Coin::ChainParams ChainParams;

	recursive_mutex Mtx;

	ptr<IBlockChainDb> Db;
	uint64_t OffsetInBootstrap, NextOffsetInBootstrap;

	BlockTree Tree;
	class TxPool TxPool;

	typedef unordered_map<HashValue, BlocksInFlightList::iterator> CMapBlocksInFlight;
	CMapBlocksInFlight MapBlocksInFlight;
	//----

	std::exception_ptr CriticalException;

	ChainCaches Caches;

	EngEvents Events;

	static const int HEIGHT_BOOTSTRAPING = -2;
	int UpgradingDatabaseHeight;
	bool Rescanning;

	mutex m_mtxStates;
	set<String> m_setStates;

	ptr<Coin::ChannelClient> ChannelClient;		//!!!?
	ptr<CoinFilter> Filter;

	path m_dbFilePath;

	int CommitPeriod;
	int m_idPeersNet;

	atomic<int> aPreferredDownloadPeers;
	atomic<int> aSyncStartedPeers;

	uint16_t MaxBlockVersion; //!!!Obsolete, never used
	Opcode MaxOpcode;
	CBool InWalJournalMode;
	CBool JournalModeDelete;
	CBool m_dbTxBegin;
	bool AllowFreeTxes;
private:
	EngMode m_mode;

	mutex m_mtxThreadStateChange;

	mutex m_mtxVerNonce;
	typedef unordered_map<uint64_t, ptr<P2P::Link>> CNonce2link;
	CNonce2link m_nonce2link;

	mutex MtxBestUpdate;
	DateTime m_dtPrevBestUpdate;
	HashValue m_hashPrevBestUpdate;

	DateTime m_dtLastFreeTx;
	double m_freeCount;
public:
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

	void TryStartBootstrap();
	void TryStartPruning();
	void Start() override;
	void SignalStop();
	void Stop();

	BlockHeader BestHeader();
	void SetBestHeader(const BlockHeader& bh);
	int BestHeaderHeight();

	BlockHeader BestBlock();
	void SetBestBlock(const BlockHeader& b);
	void SetBestBlock(const Block& b);
	int BestBlockHeight();

	path GetDbFilePath();

	bool CheckSelfVerNonce(uint64_t nonce);
	void RemoveVerNonce(Link& link);

	virtual void SetChainParams(const Coin::ChainParams& p);
	void SendVersionMessage(Link& link);
	bool HaveAllBlocksUntil(const HashValue& hash);
	bool HaveBlock(const HashValue& hash);
	bool HaveTxInDb(const HashValue& hashTx);
	bool AlreadyHave(const Inventory& inv);
	Block GetPrevBlockPrefixSuffixFromMainTree(const Block& block);
	bool IsFromMe(const Tx& tx);
	void Push(const Inventory& inv);
	void Push(const TxInfo& txInfo);
	void Broadcast(CoinMessage* m);
	void ExportToBootstrapDat(const path& pathBoostrap);
	virtual void ClearByHeightCaches();
	Block GetBlockByHeight(uint32_t height);
	BlockHeader FindHeader(const HashValue& hash);
	Block LookupBlock(const HashValue& hash);
	static Blob SpendVectorToBlob(const vector<bool>& vec);
	void EnsureTransactionStarted();
	void CommitTransactionIfStarted();
	void Reorganize(const BlockHeader& header);

	String BlockStringId(const HashValue& hashBlock);
	virtual HashValue HashMessage(RCSpan cbuf);
	virtual HashValue HashForWallet(RCSpan s);
	virtual HashValue HashForSignature(RCSpan cbuf);
	virtual HashValue HashFromTx(const Tx& tx, bool widnessAware = false);

	HashValue WitnessHashFromTx(const Tx& tx) { return HashFromTx(tx, true); }

	virtual void OnCheck(const Tx& tx) {}
	virtual void OnConnectInputs(const Tx& tx, const vector<Txo>& vTxo, bool bBlock, bool bMiner) {}
	virtual void OnConnectBlock(const Block& block) {}
	virtual void OnDisconnectInputs(const Tx& tx) {}
	virtual void CheckForDust(const Tx& tx) {}

	virtual int64_t GetSubsidy(int height, const HashValue& prevBlockHash, double difficulty = 0, bool bForCheck = false);
	virtual void CheckCoinbasedTxPrev(int hCur, int hOut);

	DateTime GetTimestampForNextBlock();

	void Relay(const TxInfo& txInfo);

	bool IsInitialBlockDownload();
	bool MarkBlockAsReceived(const HashValue& hashBlock);
	void MarkBlockAsInFlight(GetDataMessage& mGetData, Link& link, const Inventory& inv);
	void OnPeriodicMsgLoop(const DateTime& now) override;
	BlockHeader ProcessNewBlockHeaders(const vector<BlockHeader>& headers, Link *link);
	CoinPeer* CreatePeer() override;
	JumpAction TryJumpToBlockchain(int sym, Link *link);

	uint64_t CheckMoneyRange(uint64_t v) {
		if (v > ChainParams.MaxMoney)
			Throw(CoinErr::MoneyOutOfRange);
		return v;
	}

	virtual double ToDifficulty(const Target& target);

	virtual Target GetNextTarget(const BlockHeader& headerLast, const Block& block);

	//!!!?	bool GetPkId(const HashValue160& hash160, CIdPk& id);
	//!!!?	bool GetPkId(RCSpan cbuf, CIdPk& id);
	HashValue160 GetHash160ById(int64_t id);
	CanonicalPubKey GetPkById(int64_t rowid);
	virtual void UpgradeDb(const Version& ver);

	virtual BlockObj* CreateBlockObj() { return new BlockObj; }

	virtual CoinMessage* CreateVersionMessage();
	virtual CoinMessage* CreateVerackMessage();
	virtual CoinMessage* CreateAddrMessage();
	virtual CoinMessage* CreateInvMessage();
	virtual CoinMessage* CreateGetDataMessage();
	virtual CoinMessage* CreateNotFoundMessage();
	virtual CoinMessage* CreateGetBlocksMessage();
	virtual CoinMessage* CreateGetHeadersMessage();
	virtual CoinMessage* CreateTxMessage();
	virtual CoinMessage* CreateBlockMessage();
	virtual CoinMessage* CreateHeadersMessage();
	virtual CoinMessage* CreateGetAddrMessage();
	virtual CoinMessage* CreateCheckOrderMessage();
	virtual CoinMessage* CreateSubmitOrderMessage();
	virtual CoinMessage* CreateReplyMessage();
	virtual CoinMessage* CreatePingMessage();
	virtual CoinMessage* CreatePongMessage();
	virtual CoinMessage* CreateMemPoolMessage();
	virtual CoinMessage* CreateAlertMessage();
	virtual CoinMessage* CreateMerkleBlockMessage();
	virtual CoinMessage* CreateFilterLoadMessage();
	virtual CoinMessage* CreateFilterAddMessage();
	virtual CoinMessage* CreateFilterClearMessage();
	virtual CoinMessage* CreateRejectMessage();
	virtual CoinMessage* CreateCheckPointMessage();
    virtual CoinMessage* CreateSendHeadersMessage();
    virtual CoinMessage* CreateFeeFilterMessage();
    virtual CoinMessage* CreateGetBlockTransactionsMessage();
    virtual CoinMessage* CreateBlockTransactionsMessage();
    virtual CoinMessage* CreateSendCompactBlockMessage();
    virtual CoinMessage* CreateCompactBlockMessage();

	virtual TxObj* CreateTxObj() { return new TxObj; }
	virtual bool CreateDb();
	virtual bool OpenDb();
	path GetBootstrapPath();

	void BeginTransaction() override {}
	void Commit() override {}
	void Rollback() override {}

	virtual void SetPolicyFlags(int height);
	virtual void VerifySignature(SignatureHasher& sigHasher, RCSpan scriptPk);		//!!!? never overridden
	virtual void PatchSigHasher(SignatureHasher& sigHasher) {}
	virtual void CheckHashType(SignatureHasher& sigHasher, uint32_t sigHashType) {}
	virtual bool IsCheckSigOp(Opcode opcode);
	virtual int GetMaxSigOps(const Block& block) { return MAX_BLOCK_SIGOPS_COST; }
	virtual void CheckBlock(const Block& block) {}
	virtual void OrderTxes(CTxes& txes) {}

	void ConnectTx(CConnectJob& job, vector<shared_future<TxFeeTuple>>& futsTx, const Tx& tx, int height, bool bVerifySignature);
	virtual vector<shared_future<TxFeeTuple>> ConnectBlockTxes(CConnectJob& job, const vector<Tx>& txes, int height);
	virtual bool CoinEng::IsValidSignatureEncoding(RCSpan sig);
	virtual bool VerifyHash(RCSpan pubKey, const HashValue& hash, RCSpan sig);
protected:
	void StartDns();
#if UCFG_COIN_USE_IRC
	void StartIrc();
#endif

	size_t GetMessageHeaderSize() override;
	size_t GetMessagePayloadSize(RCSpan buf) override;
	void OnInitLink(P2P::Link& link) override;
	ptr<P2P::Message> RecvMessage(P2P::Link& link, const BinaryReader& rd) override;

	void SavePeers() override;

	void OnMessage(P2P::Message* m) override;
	void OnPingTimeout(P2P::Link& link) override;
	void AddLink(P2P::LinkBase* link) override;
	void OnCloseLink(P2P::LinkBase& link) override;

	virtual void TryUpgradeDb();
	virtual int GetIntervalForModDivision(int height);
	virtual int GetIntervalForCalculatingTarget(int height);
	Target KimotoGravityWell(const BlockHeader& headerLast, const Block& block, int minBlocks, int maxBlocks);
	virtual TimeSpan AdjustSpan(int height, const TimeSpan& span, const TimeSpan& targetSpan);
	virtual BlockHeader GetFirstHeaderOfInterval(const BlockObj& bo, int nInterval);
	virtual TimeSpan GetActualSpanForCalculatingTarget(const BlockHeader& firstHeader, const BlockObj& bo);
	virtual Target GetNextTargetRequired(const BlockHeader& headerLast, const Block& block);
	virtual void UpdateMinFeeForTxOuts(int64_t& minFee, const int64_t& baseFee, const Tx& tx);
	virtual path VGetDbFilePath();

	virtual void OpCat(VmStack& stack);
	virtual void OpSubStr(VmStack& stack);
	virtual void OpLeft(VmStack& stack);
	virtual void OpRight(VmStack& stack);
	virtual void OpSize(VmStack& stack);
	virtual void OpInvert(VmStack& stack);
	virtual void OpAnd(VmStack& stack);
	virtual void OpOr(VmStack& stack);
	virtual void OpXor(VmStack& stack);
	virtual void Op2Mul(VmStack& stack);
	virtual void Op2Div(VmStack& stack);
	virtual void OpMul(VmStack& stack);
	virtual void OpDiv(VmStack& stack);
	virtual void OpMod(VmStack& stack);
	virtual void OpLShift(VmStack& stack);
	virtual void OpRShift(VmStack& stack);
	virtual void OpCheckDataSig(VmStack& stack);
	virtual void OpCheckDataSigVerify(VmStack& stack);

	void OnFilterChanged() override;
private:
	void LoadPeers();
	void UpgradeTo(const Version& ver);
	void LoadLastBlockInfo();
	void Misbehaving(P2P::Message* m, int howMuch, RCString command = nullptr, RCString reason = nullptr);
	void AddSeeds();

	friend class Block;
	friend class Tx;
	friend class BlockObj;
	friend class VersionMessage;
	friend class CoinMessage;
	friend class TxPool;
	friend class Vm;
};


class CoinEngTransactionScope : noncopyable {
public:
	CoinEng& Eng;
	ITransactionable& m_db;
private:
	CInException InException;
	CBool m_bCommitted;
public:
	CoinEngTransactionScope(CoinEng& eng);
	~CoinEngTransactionScope();

	void Commit() {
		m_db.Commit();
		Eng.Commit();
		m_bCommitted = true;
	}
};

class BootstrapDbThread : public Thread {
	typedef Thread base;
public:
	CoinEng& Eng;
	path PathBootstrap;
	CBool Exporting, Indexing;

	BootstrapDbThread(CoinEng& eng, const path& pathBootstrap, bool bIndexing = false)
		: base(&eng.m_tr)
		, Eng(eng)
		, PathBootstrap(pathBootstrap)
		, Indexing(bIndexing)
	{}
protected:
	void BeforeStart() override;
	void Execute() override;
};

class PruneDbThread : public Thread {
	typedef Thread base;
public:
	CoinEng& Eng;
	int From, To;

	PruneDbThread(CoinEng& eng, int from, int to)
		: base(&eng.m_tr)
		, Eng(eng)
		, From(from)
		, To(to)
	{}
protected:
	void Execute() override;
};

class ExportKeysThread : public Thread {
	typedef Thread base;
public:
	CoinEng& Eng;
	path Path;
	String Password;

	ExportKeysThread(CoinEng& eng, const path& p, RCString password) : base(&eng.m_tr), Eng(eng), Path(p), Password(password) {}

protected:
	void Execute() override;
};

class CoinEngApp : public CAppBase {
	typedef CoinEngApp class_type;

public:
	CUsingSockets m_usingSockets;

	CoinEngApp();

	Version get_ProductVersion();
	DEFPROP_GET(Version, ProductVersion);

private:
	void LoadConf(const path& confPath);
};

//extern CoinEngApp theApp;

void SetUserVersion(SqliteConnection& db, const Version& ver = Version());

class TxNotFoundException : public Exception {
	typedef Exception base;
public:
	HashValue HashTx;

	TxNotFoundException(CoinErr err, const HashValue& hashTx) : base(err), HashTx(hashTx) {}
	TxNotFoundException(const HashValue& hashTx) : base(CoinErr::TxNotFound), HashTx(hashTx) {}
protected:
	String get_Message() const override { return EXT_STR(base::get_Message() << " " << HashTx); }
};

class WitnessProgramException : public Exception {
	typedef Exception base;
public:
	WitnessProgramException(CoinErr err) : base(err) {}
};

class PeerMisbehavingException : public Exception {
	typedef Exception base;

public:
	int HowMuch;

	PeerMisbehavingException(int howMuch = 1) : base(CoinErr::Misbehaving), HowMuch(howMuch) {}
};

Version CheckUserVersion(SqliteConnection& db);

class CEngStateDescription : noncopyable {
	CoinEng& m_eng;
	String m_s;
public:
	CEngStateDescription(CoinEng& eng, RCString s);
	~CEngStateDescription();
};

int DetectBlockchain(RCString userAgent);
int DetectBlockchain(const HashValue& hashGenesis);

#if UCFG_COIN_USE_NORMAL_MODE
void RecoverPubKey(CConnectJob* connJob, const OutPoint* pop, TxObj* pTxObj, int nIn);
#endif

} // namespace Coin
