#pragma once

#include <el/db/ext-sqlite.h>
#include "irc.h"

#include <el/crypto/hash.h>
#include <el/crypto/ecdsa.h>
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

using P2P::NetManager;

namespace Coin {

class CoinEng;
class Inventory;
class CoinDb;
class CoinLink;
class VersionMessage;
class VerackMessage;

class IBlockChainDb;
}

namespace Coin {


const UInt64 NODE_NETWORK = 1;


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

class ChainParams :	public StaticList<ChainParams> {
	typedef ChainParams class_type;
	typedef StaticList<ChainParams> base;
public:
	String Name, Symbol;
	HashValue Genesis;
	TimeSpan BlockSpan;
	Int64 InitBlockValue;
	Int64 CoinValue;
	Int64 MaxMoney;
	Int64 MinTxFee;
	Int64 MinTxOutAmount;
	int HalfLife;
	int CoinbaseMaturity;
	int AnnualPercentageRate;
	int TargetInterval;
	Target MaxTarget, InitTarget;
	Target MaxPossibleTarget;
	UInt32 ProtocolMagic;
	UInt32 ProtocolVersion;
	UInt16 DefaultPort;
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
		,	AuxPowStartBlock(INT_MAX)
	{
		Init();
	}	

	ChainParams(RCString name, bool isTestNet)
		:	base(true)
		,	Name(name)
		,	AuxPowStartBlock(INT_MAX)
		,	IsTestNet(isTestNet)
		,	HashAlgo(Coin::HashAlgo::Sha256)
	{
		Init();
	}

	static void Add(const ChainParams& p);
	void Init();
	virtual CoinEng *CreateEng(CoinDb& cdb);
	virtual CoinEng *CreateObject(CoinDb& cdb);
	void LoadFromXmlAttributes(IXmlAttributeCollection& xml);
	int Log10CoinValue() const;				// precise calculation of log10(CoinValue)
	void AddSeedEndpoint(RCString seed);
};

class Alert : public Object, public CPersistent {
public:
	Int32 Ver;
	DateTime RelayUntil, Expiration;
	Int32 NId, NCancel;
	set<Int32> SetCancel;
	Int32 MinVer, MaxVer;
	set<String> SetSubVer;
	Int32 Priority;
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
	typedef Interlocked interlocked_policy;

	CPointer<CoinEng> m_eng;
	float Speed;
	CPointer<IIWalletEvents> m_iiWalletEvents;

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
	unique_ptr<BitcoinMiner> Miner;

	Blob m_genPubKey;
	HashValue160 m_genHash160;
	void ReserveGenKey();
	virtual Block CreateNewBlock();
	void RegisterForMining(WalletBase* wallet);			// only single wallet for each Eng allowed
	void UnregisterForMining(WalletBase* wallet);
#endif
};

class MyKeyInfo {
	typedef MyKeyInfo class_type;
public:
	Int64 KeyRowid;
	Blob PubKey;
	
	CngKey Key;
	DateTime Timestamp;
	String Comment;

	~MyKeyInfo();
	Address ToAddress() const;
	Blob PlainPrivKey() const;
	Blob EncryptedPrivKey(Aes& aes) const;

	Blob get_PrivKey() const { return m_privKey; }
//	void put_PrivKey(const Blob& v);
	DEFPROP_GET(Blob, PrivKey);

	HashValue160 get_Hash160() const {
		return Coin::Hash160(PubKey);
	}
	DEFPROP_GET(HashValue160, Hash160);

	void SetPrivData(const ConstBuf& cbuf, bool bCompressed);
	void SetPrivData(const PrivateKey& privKey);

	bool IsCompressed() const { return PubKey.Size == 33; }
private:
	Blob m_privKey;
};

class CoinFilter : public BloomFilter, public Object, public CPersistent {
	typedef BloomFilter base;
public:
	typedef Interlocked interlocked_policy;

	static const byte BLOOM_UPDATE_NONE = 0,
		BLOOM_UPDATE_ALL = 1,
		BLOOM_UPDATE_P2PUBKEY_ONLY = 2,
		BLOOM_UPDATE_MASK = 3;

	UInt32 Tweak;
	byte Flags;

	CoinFilter()
		:	Tweak(0)
		,	Flags(0)
	{}

	CoinFilter(int nElements, double falsePostitiveRate, UInt32 tweak, byte flags);

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

class COIN_CLASS CoinDb : public P2P::DetectGlobalIp, public NetManager {
	typedef CoinDb class_type;
public:
	recursive_mutex MtxDb;

	String DbWalletFilePath, DbPeersFilePath;
	String FilenameSuffix;
	SqliteConnection m_dbWallet;
	SqliteCommand  CmdAddNewKey;
	SqliteCommand m_cmdIsFromMe, CmdGetBalance, CmdRecipients, CmdGetTxId, CmdPendingTxes, CmdSetBestBlockHash,
		CmdFindCoin, CmdInsertCoin;

	recursive_mutex MtxDbPeers;
	SqliteConnection m_dbPeers;
	SqliteCommand m_cmdIsBanned, CmdPeerByIp, CmdInsertPeer, CmdUpdatePeer,
		CmdEndpointByIp, CmdInsertEndpoint, CmdUpdateEndpoint;

	typedef unordered_map<HashValue160, MyKeyInfo> CHash160ToMyKey;
	CHash160ToMyKey Hash160ToMyKey;
	ptr<CoinFilter> Filter;

	thread_group m_tr;
	ptr<Coin::MsgLoopThread> MsgLoopThread;

	Coin::IrcManager IrcManager;

	String m_masterPassword;
	array<byte, 8> Salt;
	IPAddress LocalIp4, LocalIp6;
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
	void ImportXml(RCString filepath);
	void ImportDat(RCString filepath, RCString password);
	void ImportWallet(RCString filepath, RCString password);
	void ExportWalletToXml(RCString filepath);

	void OnIpDetected(const IPAddress& ip) override;
	Link *CreateLink(thread_group& tr) override;
	bool IsBanned(const IPAddress& ip) override;
	void BanPeer(Peer& peer) override;
	void SavePeers(int idPeersNet, const vector<ptr<Peer>>& peers);
private:

	void CreateWalletNetDbEntry(int netid, RCString name);	
	void CreateDbWallet();
	
	void CreateDbPeers();

	void CheckPasswordPolicy(RCString password);
	void InitAes(Aes& aes, RCString password, byte encrtyptAlgo);
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
	UInt32 Checksum;

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
	Lite,
	Normal,
	BlockExplorer
} END_ENUM_CLASS(EngMode)

inline CoinEng& Eng() noexcept { return *(CoinEng*)HasherEng::GetCurrent(); }
COIN_EXPORT CoinEng& ExternalEng();

class LocatorHashes : public vector<HashValue> {
	typedef LocatorHashes class_type;
public:
	int FindIndexInMainChain() const;

	int get_DistanceBack() const;
	DEFPROP_GET(int, DistanceBack);
};

class IBlockChainDb : public Object, public ITransactionable  {
public:
	CInt<int> m_nCheckpont;
	String DefaultFileExt;
	CBool IsSlow;

	IBlockChainDb()
		:	DefaultFileExt(".db")
	{}

	virtual void *GetDbObject()																	=0;
	virtual ITransactionable& GetSavepointObject()												=0;

	virtual bool Create(RCString path)															=0;
	virtual bool Open(RCString path)															=0;			// returns false if DBConvering deffered
	virtual void Close(bool bAsync = true)																		=0;	
	virtual void Recreate();
	
	virtual Version CheckUserVersion()															=0;
	virtual void UpgradeTo(const Version& ver)													=0;
	virtual void TryUpgradeDb(const Version& verApp) {}

	virtual bool HaveBlock(const HashValue& hash)												=0;
	virtual Block FindBlock(const HashValue& hash)												=0;
	virtual Block FindBlock(int height)															=0;
	virtual int GetMaxHeight()																	=0;
	virtual TxHashesOutNums GetTxHashesOutNums(int height)										=0;
	virtual void InsertBlock(int height, const HashValue& hash, const ConstBuf& data, const ConstBuf& txData) =0;
	virtual void DeleteBlock(int height, const vector<Int64>& txids)							=0;

	virtual bool FindTx(const HashValue& hash, Tx *ptx)											=0;
	virtual bool FindTxById(const ConstBuf& txid8, Tx *ptx)										=0;
	virtual vector<Int64> GetTxesByPubKey(const HashValue160& pubkey)							=0;

	virtual void ReadTxIns(const HashValue& hash, const TxObj& txObj)							=0;
	virtual pair<int, int> FindPrevTxCoords(DbWriter& wr, int height, const HashValue& hash)	=0;
	virtual void InsertTx(const Tx& tx, const TxHashesOutNums& hashesOutNums, const HashValue& txHash, int height, const ConstBuf& txIns, const ConstBuf& spend, const ConstBuf& data) =0;
	virtual vector<bool> GetCoinsByTxHash(const HashValue& hash)								=0;
	virtual void SaveCoinsByTxHash(const HashValue& hash, const vector<bool>& vec)				=0;

	virtual void BeginEngTransaction()															=0;
	virtual void SetProgressHandler(int(* pfn)(void*), void*p = 0, int n = 1)					=0;
	virtual vector<Block> GetBlocks(const LocatorHashes& locators, const HashValue& hashStop)	=0;

	virtual Blob FindPubkey(Int64 id)															=0;
	virtual void InsertPubkey(Int64 id, const ConstBuf& pk)										=0;
	virtual void UpdatePubkey(Int64 id, const ConstBuf& pk)										=0;
	
	virtual void Vacuum() {}
	virtual void BeforeEnsureTransactionStarted() {}

};

ptr<IBlockChainDb> CreateBlockChainDb();
ptr<IBlockChainDb> CreateSqliteBlockChainDb();

class COIN_CLASS CoinEng : public HasherEng, public P2P::Net, public ITransactionable {
	typedef CoinEng class_type;
	typedef P2P::Net base;
public:
	CoinDb& m_cdb;

	Coin::ChainParams ChainParams;
	UInt16 MaxBlockVersion;

	recursive_mutex Mtx;

	ptr<IBlockChainDb> Db;

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

	CPointer<WalletBase> m_iiEngEvents;

	int UpgradingDatabaseHeight;
	
	ptr<Coin::ChannelClient> ChannelClient;
	int CommitPeriod;
	EngMode Mode;

	ptr<CoinFilter> Filter;
	unordered_set<HashValue> CheckedFilteredTxHashes;

	CBool InWalJournalMode;
	CBool JournalModeDelete;
	CBool m_bSomeInvReceived;
	CBool m_dbTxBegin;

	bool get_LiteMode() { return Mode == EngMode::Lite; }
	DEFPROP_GET(bool, LiteMode);

	bool AllowFreeTxes;

	CoinEng(CoinDb& cdb);
	~CoinEng();
	static ptr<CoinEng> CreateObject(CoinDb& cdb, RCString name);
	void ContinueLoad0();
	void ContinueLoad();
	virtual void Load();
	virtual void Close();

	virtual ptr<IBlockChainDb> CreateBlockChainDb();

	void Start() override;
	void Stop();

	Block BestBlock();
	void SetBestBlock(const Block& b);
	int BestBlockHeight();

	String m_dbFilePath;
	virtual String GetDbFilePath();	

	bool CheckSelfVerNonce(UInt64 nonce) {
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
	bool IsFromMe(const Tx& tx);
	void AddToMemory(const Tx& tx);
	void RemoveFromMemory(const Tx& tx);	
	void Push(const Inventory& inv);
	void Push(const Tx& tx);
	bool AddToPool(const Tx& tx, vector<HashValue>& vQueue);
	Block GetBlockByHeight(UInt32 height);
	Block LookupBlock(const HashValue& hash);
	static Blob SpendVectorToBlob(const vector<bool>& vec);
	void EnsureTransactionStarted();
	void CommitTransactionIfStarted();

	virtual HashValue HashMessage(const ConstBuf& cbuf) { return Coin::Hash(cbuf); }

	virtual void OnCheck(const Tx& tx) {}
	virtual void OnConnectInputs(const Tx& tx, const vector<Tx>& vTxPrev, bool bBlock, bool bMiner) {}
	virtual void OnConnectBlock(const Block& block) {}
	virtual void OnDisconnectInputs(const Tx& tx) {}
	virtual bool ShouldBeSaved(const Tx& tx) { return !LiteMode; }

	virtual Int64 GetSubsidy(int height, const HashValue& prevBlockHash, double difficulty = 0, bool bForCheck = false) {
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

	Int64 CheckMoneyRange(Int64 v) {
		if (v < 0 || v > ChainParams.MaxMoney)
			Throw(E_COIN_MoneyOutOfRange);
		return v;
	}

	virtual double ToDifficulty(const Target& target);

	virtual Int64 GetMinRelayTxFee() {
		return ChainParams.CoinValue/10000;
	}

	virtual Target GetNextTarget(const Block& blockLast, const Block& block);

	virtual bool MiningAllowed() { return true; }


	bool GetPkId(const HashValue160& hash160, CIdPk& id);
	bool GetPkId(const ConstBuf& cbuf, CIdPk& id);
	HashValue160 GetHash160ById(Int64 id);
	Blob GetPkById(Int64 rowid);
	virtual void UpgradeDb(const Version& ver);

	virtual CoinMessage *CreateVersionMessage();
	virtual CoinMessage *CreateVerackMessage();
	virtual CoinMessage *CreateAddrMessage();
	virtual CoinMessage *CreateInvMessage();
	virtual CoinMessage *CreateGetDataMessage();
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
	virtual void UpdateMinFeeForTxOuts(Int64& minFee, const Int64& baseFee, const Tx& tx);
private:
	mutex m_mtxThreadStateChange;	

	mutex m_mtxVerNonce;
	typedef unordered_map<UInt64, ptr<Link>> CNonce2link;
	CNonce2link m_nonce2link;

	mutex MtxBestUpdate;
	DateTime m_dtPrevBestUpdate;
	HashValue m_hashPrevBestUpdate;

	DateTime m_dtLastFreeTx;
	double m_freeCount;

	void UpgradeTo(const Version& ver);
	void LoadLastBlockInfo();
	void Misbehaving(P2P::Message *m, int howMuch);

	friend class Block;
	friend class Tx;
	friend class BlockObj;
	friend class VersionMessage;
	friend class CoinMessage;
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

class TxNotFoundExc : public Exception {
	typedef Exception base;
public:
	HashValue HashTx;

	TxNotFoundExc(const HashValue& hashTx)
		:	base(E_COIN_TxNotFound)
		,	HashTx(hashTx)
	{
	}

	String get_Message() const override {
		return EXT_STR(base::get_Message() << " " << HashTx);
	}
};

class PeerMisbehavingExc : public Exception {
	typedef Exception base;
public:
	int HowMuch;

	PeerMisbehavingExc(int howMuch = 1)
		:	base(E_COIN_Misbehaving)
		,	HowMuch(howMuch)
	{}
};


class VersionExc : public Exception {
	typedef Exception base;
public:
	Ext::Version Version;

	VersionExc()
		:	base(E_EXT_DB_Version)
	{
	}

	String get_Message() const override {
		String r = base::get_Message();
		r += " "+Version.ToString(2);
		return r;
	}
};

Version CheckUserVersion(SqliteConnection& db);


} // Coin::


