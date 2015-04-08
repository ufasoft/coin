/*######   Copyright (c) 2013-2015 Ufasoft  http://ufasoft.com  mailto:support@ufasoft.com,  Sergey Pavlov  mailto:dev@ufasoft.com ####
#                                                                                                                                     #
# 		See LICENSE for licensing information                                                                                         #
#####################################################################################################################################*/

#pragma once

#include <el/xml.h>

#include EXT_HEADER_DECIMAL
using namespace std::decimal;

#include <el/bignum.h>
#include <el/db/ext-sqlite.h>

using Ext::DB::ITransactionable;
using Ext::DB::TransactionScope;

using Ext::DB::sqlite_(NS)::DbDataReader;
using Ext::DB::sqlite_(NS)::SqliteConnection;
using Ext::DB::sqlite_(NS)::SqliteCommand;
using Ext::DB::sqlite_(NS)::SqliteException;
using Ext::DB::sqlite_(NS)::SqliteMalloc;
using Ext::DB::sqlite_(NS)::SqliteVfs;


#include <el/inet/p2p-net.h>
using namespace Ext::Inet;
using P2P::Link;
using P2P::Peer;



#ifndef UCFG_COIN_GENERATE
#	define UCFG_COIN_GENERATE (!UCFG_WCE && UCFG_WIN32)
#endif

#include "param.h"

#include "../util/util.h"
#include "coin-msg.h"

namespace Coin {

const int BLOCK_VERSION_AUXPOW = 256;

enum {
    SIGHASH_ALL = 1,
    SIGHASH_NONE = 2,
    SIGHASH_SINGLE = 3,
    SIGHASH_ANYONECANPAY = 0x80
};

class CoinEng;
class Block;
class Tx;
class TxObj;
class TxHashesOutNums;
class CConnectJob;

struct PeerInfoBase {
	IPEndPoint Ep;
	uint64_t Services;

	void Write(BinaryWriter& wr) const;
	void Read(const BinaryReader& rd);
};

struct PeerInfo : public PeerInfoBase {
	typedef PeerInfoBase base;

	DateTime Timestamp;

	void Write(BinaryWriter& wr) const;
	void Read(const BinaryReader& rd);
};

class DbWriter : public BinaryWriter {
	typedef BinaryWriter base;
public:
	observer_ptr<TxHashesOutNums> PTxHashesOutNums;
	observer_ptr<const Block> PCurBlock;
	observer_ptr<CConnectJob> ConnectJob;
	bool BlockchainDb;

	DbWriter(Stream& stm)
		:	base(stm)
		,	BlockchainDb(true)
	{
	}
};

class DbReader : public BinaryReader {
	typedef BinaryReader base;
public:
	CoinEng *Eng;
	bool BlockchainDb;
	bool ReadTxHashes;
	observer_ptr<const Block> PCurBlock;

	DbReader(const Stream& stm, CoinEng *eng = 0)
		:	base(stm)
		,	Eng(eng)
		,	BlockchainDb(true)
		,	ReadTxHashes(true)
	{
	}
};

struct OutPoint {
	HashValue TxHash;
	int32_t Index;

	OutPoint(const HashValue& txHash = HashValue(), int32_t idx = -1)
		:	TxHash(txHash)
		,	Index(idx)
	{}

	bool operator==(const OutPoint& v) const {
		return TxHash == v.TxHash && Index == v.Index;
	}

	bool operator!=(const OutPoint& v) const {
		return !operator==(v);
	}

	bool IsNull() const {
		return TxHash==HashValue() && -1==Index;
	}

	void Write(BinaryWriter& wr) const {
		wr << TxHash << Index;
	}

	void Read(const BinaryReader& rd) {
		rd >> TxHash >> Index;
	}

	array<byte, 36> ToArray() const {
		array<byte, 36> r;
		memcpy(r.data(), TxHash.data(), 32);
		*(uint32_t*)(r.data()+32) = htole(Index);
		return r;
	}
};

} namespace std {
	template<> struct hash<Coin::OutPoint> {
		size_t operator()(const Coin::OutPoint& op) const {
			return hash<Coin::HashValue>()(op.TxHash)+hash<int32_t>()(op.Index);
		}
	};
} namespace Coin {

class COIN_CLASS TxIn {		// Keep this struct small without VTBL
private:
	mutable Blob m_script;
	Blob m_sig;
public:
	OutPoint PrevOutPoint;
	uint32_t Sequence;
	byte RecoverPubKeyType;

	TxIn()
		:	Sequence(UINT_MAX)
		,	m_script(nullptr)
		,	RecoverPubKeyType(0)
		,	N(-1)
		,	m_pTxo(0)
	{}

	bool IsFinal() const {
		return Sequence == numeric_limits<uint32_t>::max();
	}

	const Blob& Script() const;

	void put_Script(const ConstBuf& script) { m_script = script; }

	void Write(BinaryWriter& wr) const;
	void Read(const BinaryReader& rd);
private:
	const TxObj *m_pTxo;
	int N;

	friend class TxObj;
};

COIN_CLASS byte TryParseDestination(const ConstBuf& pkScript, HashValue160& hash160, Blob& pubkey);

class COIN_CLASS TxOut : public CPersistent {
	typedef TxOut class_type;

//	DBG_OBJECT_COUNTER(class_type);
public:
	int64_t Value;
	mutable CIdPk m_idPk;									// changed when saving/locading Tx. Referenced by TxIn reading.
	mutable Blob m_pkScript;

	TxOut(int64_t val = -1, const Blob& script = Blob())
		:	m_pkScript(script)
		,	Value(val)
	{}

	void Write(BinaryWriter& wr) const override;
	void Read(const BinaryReader& rd) override;

	const Blob& get_PkScript() const;
	DEFPROP_GET(const Blob&, PkScript);

	void CheckForDust() const;

	bool IsEmpty() const {
		return Value==0 && get_PkScript().Size==0;
	}

	byte TryParseDestination(HashValue160& hash160, Blob& pubkey) const {
		return Coin::TryParseDestination(get_PkScript(), hash160, pubkey);
	}
private:
	byte m_typ;

	friend interface WalletBase;
	friend COIN_EXPORT const DbReader& operator>>(const DbReader& rd, Tx& tx);
};

class Tx;
class CTxMap;
class CoinsView;

class COIN_CLASS Target {
public:
	uint32_t m_value;

	explicit Target(uint32_t v = 0x1d00ffff);
	explicit Target(const BigInteger& bi);

	bool operator<(const Target& t) const;

	operator BigInteger() const;

	void Write(BinaryWriter& wr) const {
		wr << m_value;
	}

	void Read(const BinaryReader& rd) {
		_self = Target(rd.ReadUInt32());
	}	
};

class COIN_CLASS TxObj : public Object, public CoinSerialized  {
	typedef TxObj class_type;

//	DBG_OBJECT_COUNTER(class_type);
public:
	typedef InterlockedPolicy interlocked_policy;

	vector<TxOut> TxOuts;
	DateTime LockTimestamp;

	mutable vector<TxIn> m_txIns;
	mutable Coin::HashValue m_hash;
	uint32_t LockBlock;
	int32_t Height;
	mutable volatile byte m_nBytesOfHash;
	CBool m_bIsCoinBase;
	mutable volatile bool m_bLoadedIns;

	TxObj();

	virtual TxObj *Clone() const { return new TxObj(_self); }

	virtual void Write(BinaryWriter& wr) const;
	virtual void Read(const BinaryReader& rd);
	virtual void WritePrefix(BinaryWriter& wr) const {}
	virtual void ReadPrefix(const BinaryReader& rd) {}
	virtual void WriteSuffix(BinaryWriter& wr) const {}
	virtual void ReadSuffix(const BinaryReader& rd) {}
    bool IsFinal(int height, const DateTime dt) const;
	virtual int64_t GetCoinAge() const { Throw(E_NOTIMPL); }
	void ReadTxIns(const DbReader& rd) const;
	virtual String GetComment() const { return nullptr; }
protected:
	const vector<TxIn>& TxIns() const;
	virtual void CheckCoinStakeReward(int64_t reward, const Target& target) const  {}
private:
	friend class Tx;
	friend HashValue SignatureHash(const ConstBuf& script, const TxObj& txoTo, int nIn, int32_t nHashType);
};

ENUM_CLASS(MinFeeMode) {
	Block,
	Tx,
	Relay
} END_ENUM_CLASS(MinFeeMode);


class COIN_CLASS Tx : public Pimpl<TxObj>, public CPersistent {
	typedef Tx class_type;

//	DBG_OBJECT_COUNTER(class_type);
public:
	Tx() {
	}

/*!!!	Tx(std::nullptr_t) {
	}*/

	Tx(TxObj *txObj) {
		m_pimpl = txObj;
	}

	static bool AllowFree(double priority);	
	static bool TryFromDb(const HashValue& hash, Tx *ptx);
	static Tx FromDb(const HashValue& hash);	

	void EnsureCreate();
	void Write(BinaryWriter& wr) const override;
	void Read(const BinaryReader& rd) override;
	void Check() const;
	uint32_t GetSerializeSize() const;
	int64_t GetMinFee(uint32_t blockSize = 1, bool bAllowFree = true, MinFeeMode mode = MinFeeMode::Block, uint32_t nBytes = uint32_t(-1)) const;

	int32_t get_Height() const { return m_pimpl->Height; }
	void put_Height(int32_t v) { m_pimpl->Height = v; }
	DEFPROP(int32_t, Height);

	uint32_t get_LockBlock() const { return m_pimpl->LockBlock; }
	void put_LockBlock(uint32_t v) { m_pimpl->LockBlock = v; }
	DEFPROP(uint32_t, LockBlock);

	int64_t get_ValueOut() const;
	DEFPROP_GET(int64_t, ValueOut);

	int get_DepthInMainChain() const;
	DEFPROP_GET(int, DepthInMainChain);	

	int get_SigOpCount() const;
	DEFPROP_GET(int, SigOpCount);	

	void CheckInOutValue(int64_t nValueIn, int64_t& nFees, int64_t minFee, const Target& target) const;
	void ConnectInputs(CoinsView& view, int32_t height, int& nBlockSigOps, int64_t& nFees, bool bBlock, bool bMiner, int64_t minFee, const Target& target) const;
	int GetP2SHSigOpCount(const CTxMap& txMap) const;
	bool IsFinal(int height = 0, const DateTime dt = DateTime(0)) const { return m_pimpl->IsFinal(height, dt); }
	bool IsNewerThan(const Tx& txOld) const;
	void SpendInputs() const;
	void WriteTxIns(DbWriter& wr) const;

	const vector<TxOut>& TxOuts() const { return m_pimpl->TxOuts; }
	vector<TxOut>& TxOuts() { return m_pimpl->TxOuts; }
	const vector<TxIn>& TxIns() const { return m_pimpl->TxIns(); }

	bool IsCoinBase() const {
		return m_pimpl->m_txIns.empty() ? bool(m_pimpl->m_bIsCoinBase) : m_pimpl->m_txIns.size()==1 && m_pimpl->m_txIns[0].PrevOutPoint.IsNull();
	}

	bool IsCoinStake() const {
		return !TxIns().empty() && !TxIns()[0].PrevOutPoint.IsNull() && TxOuts().size()>=2 && TxOuts()[0].IsEmpty();
	}

	int64_t get_Fee() const;
	DEFPROP_GET(int64_t, Fee);

	void SetHash(const HashValue& hash) const {
		m_pimpl->m_hash = hash;
		m_pimpl->m_nBytesOfHash = (byte)hash.size();
	}

	void SetReducedHash(const ConstBuf& txid8) const {
		memcpy(m_pimpl->m_hash.data(), txid8.P, m_pimpl->m_nBytesOfHash = 8);
	}
private:

	friend class Txes;
	friend interface WalletBase;
	friend class Wallet;
	friend class SolidcoinBlockObj;
	friend class CoinDb;
	friend class EmbeddedMiner;
	friend COIN_EXPORT HashValue Hash(const Tx& tx);
	friend COIN_EXPORT const DbReader& operator>>(const DbReader& rd, Tx& tx);
};

COIN_EXPORT HashValue Hash(const CPersistent& pers);

DbWriter& operator<<(DbWriter& wr, const Tx& tx);
COIN_EXPORT const DbReader& operator>>(const DbReader& rd, Tx& tx);


class AuxPow;

} namespace Ext {
	template <> struct ptr_traits<Coin::AuxPow> {
		typedef InterlockedPolicy interlocked_policy;
	};
} namespace Coin {

struct TxHashOutNum {
	HashValue HashTx;
	uint32_t NOuts;
};

class TxHashesOutNums : public vector<TxHashOutNum> {
	typedef vector<TxHashOutNum> base;
public:
	TxHashesOutNums() {}
	explicit TxHashesOutNums(const ConstBuf& buf);
	pair<int, int> StartingTxOutIdx(const HashValue& hashTx) const;			// return pair<TxIdxInBlock, OutIdxInBlock>
	pair<int, OutPoint> OutPointByIdx(int idx) const;						// return pair<TxIdxInBlock, OutPoint>
};

BinaryWriter& operator<<(BinaryWriter& wr, const TxHashesOutNums& hons);
const BinaryReader& operator>>(const BinaryReader& rd, TxHashesOutNums& hons);

class CTxes : public vector<Tx> {
	typedef vector<Tx> base;
public:
	CTxes() {
	}

	CTxes(const CTxes& txes);												// Cloning ctor
	pair<int, int> StartingTxOutIdx(const HashValue& hashTx) const;			// return pair<TxIdxInBlock, OutIdxInBlock>
	pair<int, OutPoint> OutPointByIdx(int idx) const;						// return pair<TxIdxInBlock, OutPoint>
	
	using base::operator[];

	const Tx& operator[](const HashValue& hashTx) const {
		return at(StartingTxOutIdx(hashTx).first);
	}
};

DbWriter& operator<<(DbWriter& wr, const CTxes& txes);
COIN_EXPORT const DbReader& operator>>(const DbReader& rd, CTxes& txes);

ENUM_CLASS(ProofOf) {
	Work,
	Stake
} END_ENUM_CLASS(ProofOf);


class COIN_CLASS BlockObj : public BlockBase {
	typedef BlockBase base;
	typedef BlockObj class_type;

//	DBG_OBJECT_COUNTER(class_type);
public:
	ptr<Coin::AuxPow> AuxPow;
	
	TxHashesOutNums m_txHashesOutNums;
	mutable uint64_t OffsetInBootstrap;
	
	mutable volatile bool m_bTxesLoaded;
	mutable optional<Coin::HashValue> m_hash;

	BlockObj()
		:	m_bTxesLoaded(false)
		,	OffsetInBootstrap(0)
	{}

	~BlockObj();
	virtual Coin::HashValue Hash() const;
	virtual Coin::HashValue PowHash() const;
	Coin::HashValue MerkleRoot(bool bSave=true) const override;

	virtual void Write(DbWriter& wr) const;
	virtual void Read(const DbReader& rd);
	
	Target get_DifficultyTarget() const { return Target(DifficultyTargetBits); }
	DEFPROP_GET(Target, DifficultyTarget);

	virtual ProofOf ProofType() const { return ProofOf::Work; }
	const Coin::CTxes& get_Txes() const;
	virtual HashValue HashFromTx(const Tx& tx, int n) const;
	void WriteHeader(BinaryWriter& wr) const override;
	virtual void WriteSuffix(BinaryWriter& wr) const {}
	virtual void WriteDbSuffix(BinaryWriter& wr) const {}
	virtual void ReadDbSuffix(const BinaryReader& rd) {}
protected:	
	mutable mutex m_mtx;
	mutable CTxes m_txes;

	BlockObj(BlockObj& bo)
		:	base(bo)
		,	m_hash(bo.m_hash)
		,	m_txes(bo.m_txes)
		,	AuxPow(bo.AuxPow)
		,	m_merkleTree(bo.m_merkleTree)
		,	m_bTxesLoaded(bo.m_bTxesLoaded)
		,	m_txHashesOutNums(bo.m_txHashesOutNums)
		,	OffsetInBootstrap(bo.OffsetInBootstrap)
	{
	}

	virtual BlockObj *Clone() { return new BlockObj(*this); }
	virtual void ReadHeader(const BinaryReader& rd, bool bParent, const HashValue *pMerkleRoot);
	virtual void Write(BinaryWriter& wr) const;
	virtual void Read(const BinaryReader& rd);
	virtual BigInteger GetWork() const;
	virtual void CheckPow(const Target& target);
	virtual void CheckSignature() {}
	virtual void Check(bool bCheckMerkleRoot);
	virtual void CheckCoinbaseTx(int64_t nFees);	
	int GetBlockHeightFromCoinbase() const;
	virtual void ComputeAttributes() {}
private:
	mutable CCoinMerkleTree m_merkleTree;

	friend class Block;
	friend class TxMessage;
	friend class MerkleBlockMessage;
};

class COIN_CLASS Block : public Pimpl<BlockObj>, public CPersistent {
	typedef Pimpl<BlockObj> base;
	typedef Block class_type;
public:
	Block();

	Block(std::nullptr_t) {
	}

	Block(BlockObj *pimpl) {
		m_pimpl = pimpl;
	}

	Block GetPrevBlock() const;
	
	void WriteHeader(BinaryWriter& wr) const { m_pimpl->WriteHeader(wr); }
	void ReadHeader(const BinaryReader& rd, bool bParent = false, const HashValue *pMerkleRoot = 0) { m_pimpl->ReadHeader(rd, bParent, pMerkleRoot); }

	void Write(BinaryWriter& wr) const override { m_pimpl->Write(wr); }
	void Read(const BinaryReader& rd) override { m_pimpl->Read(rd); }
	uint32_t OffsetOfTx(const HashValue& hashTx) const;

	void LoadToMemory();

	uint32_t GetSerializeSize() const {
		return (uint32_t)EXT_BIN(_self).Size;
	}

	int get_Height() const {
		return m_pimpl->Height;
	}
	DEFPROP_GET(int, Height);

	int get_SafeHeight() const {
		return m_pimpl ? m_pimpl->Height : -1;
	}
	DEFPROP_GET(int, SafeHeight);

	DateTime get_Timestamp() const {
		return m_pimpl->Timestamp;
	}
	DEFPROP_GET(DateTime, Timestamp);

	Coin::HashValue get_MerkleRoot() const {
		return m_pimpl->MerkleRoot();
	}
	DEFPROP_GET(HashValue, MerkleRoot);

	HashValue get_PrevBlockHash() const {
		return m_pimpl->PrevBlockHash;
	}
	DEFPROP_GET(HashValue, PrevBlockHash);

	uint32_t get_Nonce() const {
		return m_pimpl->Nonce;
	}
	DEFPROP_GET(uint32_t, Nonce);

	Target get_DifficultyTarget() const {
		return m_pimpl->DifficultyTarget;
	}
	DEFPROP_GET(Target, DifficultyTarget);

	ProofOf get_ProofType() const { return m_pimpl->ProofType(); }
	DEFPROP_GET(ProofOf, ProofType);

	const CTxes& get_Txes() const { return m_pimpl->get_Txes(); }
	DEFPROP_GET(const CTxes&, Txes);

	const Coin::TxHashesOutNums& get_TxHashesOutNums() const { return m_pimpl->m_txHashesOutNums; }
	DEFPROP_GET(const Coin::TxHashesOutNums&, TxHashesOutNums);

	void Add(const Tx& tx) {
		m_pimpl->m_merkleRoot.reset();
		m_pimpl->m_hash.reset();
		m_pimpl->m_bTxesLoaded = true;
		m_pimpl->m_txes.push_back(tx);	
	}
		
	Tx& GetFirstTxRef() { return m_pimpl->m_txes[0]; }
	void CheckPow(const Target& target) const { return m_pimpl->CheckPow(target); }
	BigInteger GetWork() const { return m_pimpl->GetWork(); }
		
	Block GetOrphanRoot() const;
	void Check(bool bCheckMerkleRoot) const;
	void Check() const;
	void Connect() const;
	void Disconnect() const;
	void Reorganize();	
	bool HasBestChainWork() const;
	void Accept();	
	void Process(Link *link = 0);

	bool IsInMainChain() const;
	bool IsSuperMajority(int minVersion, int nRequired, int nToCheck);

	uint32_t get_Ver() const { return m_pimpl->Ver; }
	void put_Ver(uint32_t v) { m_pimpl->Ver = v; }
	DEFPROP(uint32_t, Ver);

	int get_ChainId() const { return Ver >> 16; }
	void put_ChainId(int v) { Ver = (Ver & 0xFFFF) | (v << 16); }
	DEFPROP(int, ChainId);

	DateTime GetMedianTimePast();

	Block Clone() const {
		return Block(m_pimpl->Clone());
	}
	
protected:
	friend class BlockObj;
	friend class MerkleBlockMessage;
};

inline HashValue Hash(const Block& block) {
	return block.m_pimpl->Hash();
}

class MerkleTx : public Tx {
	typedef Tx base;
public:
    HashValue BlockHash;
    CCoinMerkleBranch MerkleBranch;

	MerkleTx() {
		MerkleBranch.m_h2 = &HashValue::Combine;
	}

	void Write(BinaryWriter& wr) const override;
	void Read(const BinaryReader& rd) override;
};

class CoinPartialMerkleTree : public PartialMerkleTree<HashValue, PFN_Combine> {
	typedef PartialMerkleTree<HashValue, PFN_Combine> base;
public:
	CoinPartialMerkleTree()
		:	base(&HashValue::Combine)
	{}

	void Init(const vector<HashValue>& vHash, const dynamic_bitset<byte>& bsMatch);
	pair<HashValue, vector<HashValue>> ExtractMatches() const;		// returns pair<MerkleRoot, matches[]>
};

class AuxPow : public Object, public MerkleTx {
	typedef MerkleTx base;
public:
    CCoinMerkleBranch ChainMerkleBranch;
	Block ParentBlock;

	AuxPow() {
		ChainMerkleBranch.m_h2 = &HashValue::Combine;
	}

	void Write(BinaryWriter& wr) const override;
	void Read(const BinaryReader& rd) override;
	void Write(DbWriter& wr) const;
	void Read(const DbReader& rd);
	void Check(const Block& blockAux);
};

struct PubKeyHash160 {
	Blob PubKey;
	HashValue160 Hash160;

	PubKeyHash160()
		:	PubKey(nullptr)
	{}

	explicit PubKeyHash160(const Blob& pubKey)
		:	PubKey(pubKey)
		,	Hash160(Coin::Hash160(pubKey))
	{}

	PubKeyHash160(const Blob& pubKey, const HashValue160& hash160)
		:	PubKey(pubKey)
		,	Hash160(hash160)
	{}
};

PubKeyHash160 DbPubKeyToHashValue160(const ConstBuf& mb);

class ChainCaches {
public:
	mutex Mtx;

	unordered_map<HashValue, Block> HashToAlternativeChain;		//!!! can grow to huge size

	LruMap<HashValue, Block> HashToBlockCache;
	
	typedef LruMap<uint32_t, HashValue> CHeightToHashCache;
	CHeightToHashCache HeightToHashCache;	

	typedef LruMap<HashValue, Tx> CHashToTxCache;
	CHashToTxCache HashToTxCache;
	
	DateTime DtBestReceived;

	typedef LruMap<HashValue, Block> COrphanMap;
	COrphanMap OrphanBlocks;

	typedef LruMap<HashValue, Tx> CRelayHashToTx;
	CRelayHashToTx m_relayHashToTx;
	
	typedef LruMap<int64_t, PubKeyHash160> CCachePkIdToPubKey;
	CCachePkIdToPubKey m_cachePkIdToPubKey;
	bool PubkeyCacheEnabled;

	Block m_bestBlock;

	struct SpentTx {
		HashValue HashTx;
		uint32_t Height;
		uint32_t Offset;
	};
	typedef list<SpentTx> CCacheSpentTxes;
	CCacheSpentTxes m_cacheSpentTxes;											// used for fast Reorganize()
	static const size_t MAX_LAST_SPENT_TXES = 5000;

	ChainCaches();

	friend class CoinEng;
};


extern const byte s_mergedMiningHeader[4];
int CalcMerkleIndex(int merkleSize, int chainId, uint32_t nonce);
ConstBuf FindStandardHashAllSigPubKey(const ConstBuf& cbuf);


inline bool IsPubKey(const ConstBuf& cbuf) {
	return (cbuf.Size == 33 && (cbuf.P[0]==2 || cbuf.P[0]==3)) ||
		(cbuf.Size == 65 || cbuf.P[0]==4);
}

class ConnectTask : public Object {
public:
	vector<Tx> Txes;
};

class CTxMap : public unordered_map<HashValue, Tx> {
public:
	const TxOut& GetOutputFor(const OutPoint& prev) const;
};

class CoinsView {
public:
	CTxMap TxMap;

	bool HasInput(const OutPoint& op) const;
	void SpendInput(const OutPoint& op);
private:
	mutable unordered_map<OutPoint, bool> m_outPoints;

	friend void swap(CoinsView& x, CoinsView& y);
};

inline void swap(CoinsView& x, CoinsView& y) {
	swap(x.TxMap, y.TxMap);
	swap(x.m_outPoints, y.m_outPoints);
}

class CConnectJob : noncopyable {
public:
	CoinEng& Eng;
	
	typedef unordered_map<HashValue, ptr<ConnectTask>> CHashToTask;
	CHashToTask HashToTask;	

	unordered_set<ptr<ConnectTask>> Tasks;
	CBool PayToScriptHash;
	int32_t Height;
	CTxMap TxMap;
	int64_t Fee;
	Target DifficultyTarget;

	struct PubKeyData {
		Blob PubKey;
		CBool Insert, Update, IsTask;

		PubKeyData()
			:	PubKey(nullptr)
		{}
	};

	typedef unordered_map<HashValue160, PubKeyData> CMap;
	CMap Map;

	unordered_set<int64_t> InsertedIds;

	mutable atomic<int> aSigOps;
	mutable volatile bool Failed;

	CConnectJob(CoinEng& eng)
		:	Eng(eng)
		, aSigOps(0)
		,	Fee(0)
		,	Failed(false)
	{}

	void AsynchCheckAll(const vector<Tx>& txes);
	void Prepare(const Block& block);
	void Calculate();
	void PrepareTask(const HashValue160& hash160, const ConstBuf& pubKey);
};



} // Coin::


