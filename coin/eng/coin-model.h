/*######     Copyright (c) 1997-2013 Ufasoft  http://ufasoft.com  mailto:support@ufasoft.com,  Sergey Pavlov  mailto:dev@ufasoft.com #######################################
#                                                                                                                                                                          #
# This program is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation;  #
# either version 3, or (at your option) any later version. This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the      #
# implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details. You should have received a copy of the GNU #
# General Public License along with this program; If not, see <http://www.gnu.org/licenses/>                                                                               #
##########################################################################################################################################################################*/

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


#include <p2p/net/p2p-net.h>
using P2P::Link;
using P2P::Peer;



#ifndef UCFG_COIN_GENERATE
#	define UCFG_COIN_GENERATE (!UCFG_WCE && UCFG_WIN32)
#endif

#include "param.h"

#include "util.h"
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
class TxHashesOutNums;
class CConnectJob;

struct PeerInfoBase {
	IPEndPoint Ep;
	UInt64 Services;

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
	CPointer<TxHashesOutNums> PTxHashesOutNums;
	CPointer<const Block> PCurBlock;
	CPointer<CConnectJob> ConnectJob;
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
	CPointer<const Block> PCurBlock;

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
	Int32 Index;

	OutPoint(const HashValue& txHash = HashValue(), Int32 idx = -1)
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
		*(UInt32*)(r.data()+32) = htole(Index);
		return r;
	}
};

} namespace std {
	template<> struct hash<Coin::OutPoint> {
		size_t operator()(const Coin::OutPoint& op) const {
			return hash<Coin::HashValue>()(op.TxHash)+hash<Int32>()(op.Index);
		}
	};
} namespace Coin {

class COIN_CLASS TxIn : public CPersistent {
private:
	mutable Blob m_script;
	Blob m_sig;
public:
	OutPoint PrevOutPoint;
	UInt32 Sequence;

	TxIn()
		:	Sequence(UINT_MAX)
		,	m_script(nullptr)
	{}

	bool IsFinal() const {
		return Sequence == numeric_limits<UInt32>::max();
	}

	const Blob& Script() const;

	void put_Script(const ConstBuf& script) { m_script = script; }

	void Write(BinaryWriter& wr) const override;
	void Read(const BinaryReader& rd) override;

	friend class TxObj;
};

COIN_CLASS byte TryParseDestination(const ConstBuf& pkScript, HashValue160& hash160, Blob& pubkey);

class COIN_CLASS TxOut : public CPersistent {
	typedef TxOut class_type;

//	DBG_OBJECT_COUNTER(class_type);
public:
	Int64 Value;
	mutable CIdPk m_idPk;									// changed when saving/locading Tx. Referenced by TxIn reading.
	mutable Blob m_pkScript;

	TxOut(Int64 val = -1, const Blob& script = Blob())
		:	m_pkScript(script)
		,	Value(val)
	{}

	void Write(BinaryWriter& wr) const override;
	void Read(const BinaryReader& rd) override;

	const Blob& get_PkScript() const;
	DEFPROP_GET(const Blob&, PkScript);

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

#ifdef X_DEBUG//!!!D
class COIN_CLASS  TxCounter {
public:
	TxCounter();
	TxCounter(const TxCounter&);
	~TxCounter();
};
#endif

class COIN_CLASS Target {
public:
	UInt32 m_value;

	explicit Target(UInt32 v = 0x1d00ffff);
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
	typedef Interlocked interlocked_policy;

	vector<TxOut> TxOuts;
	DateTime LockTimestamp;

	mutable vector<TxIn> m_txIns;
	mutable Coin::HashValue m_hash;
	UInt32 LockBlock;
	Int32 Height;
	mutable volatile byte m_nBytesOfHash;
	CBool m_bIsCoinBase;
	mutable volatile bool m_bLoadedIns;

	TxObj();

	virtual TxObj *Clone() const { return new TxObj(_self); }

	virtual void Write(BinaryWriter& wr) const;
	virtual void Read(const BinaryReader& rd);
	virtual void WritePrefix(BinaryWriter& wr) const {}
	virtual void ReadPrefix(const BinaryReader& rd) {}
    bool IsFinal(int height = 0, const DateTime dt = 0) const;
	virtual Int64 GetCoinAge() const { Throw(E_NOTIMPL); }
	void ReadTxIns(const DbReader& rd) const;
protected:
	const vector<TxIn>& TxIns() const;
	virtual void CheckCoinStakeReward(Int64 reward, const Target& target) const  {}
private:

	friend class Tx;
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
	UInt32 GetSerializeSize() const;
	Int64 GetMinFee(UInt32 blockSize = 1, bool bAllowFree = true, MinFeeMode mode = MinFeeMode::Block) const;

	Int32 get_Height() const { return m_pimpl->Height; }
	void put_Height(Int32 v) { m_pimpl->Height = v; }
	DEFPROP(Int32, Height);

	UInt32 get_LockBlock() const { return m_pimpl->LockBlock; }
	void put_LockBlock(UInt32 v) { m_pimpl->LockBlock = v; }
	DEFPROP(UInt32, LockBlock);

	Int64 get_ValueOut() const;
	DEFPROP_GET(Int64, ValueOut);

	int get_DepthInMainChain() const;
	DEFPROP_GET(int, DepthInMainChain);	

	void CheckInOutValue(Int64 nValueIn, Int64& nFees, Int64 minFee, const Target& target) const;
	void ConnectInputs(CTxMap& txMap, Int32 height, int& nBlockSigOps, Int64& nFees, bool bBlock, bool bMiner, Int64 minFee, const Target& target) const;

	int get_SigOpCount() const;
	DEFPROP_GET(int, SigOpCount);	

	int GetP2SHSigOpCount(const CTxMap& txMap) const;
	bool IsFinal(int height = 0, const DateTime dt = 0) const { return m_pimpl->IsFinal(height, dt); }
	bool IsNewerThan(const Tx& txOld) const;
	void WriteTxIns(DbWriter& wr) const;
	decimal64 GetDecimalFee() const;

	const vector<TxOut>& TxOuts() const { return m_pimpl->TxOuts; }
	vector<TxOut>& TxOuts() { return m_pimpl->TxOuts; }
	const vector<TxIn>& TxIns() const { return m_pimpl->TxIns(); }

	bool IsCoinBase() const {
		return m_pimpl->m_txIns.empty() ? bool(m_pimpl->m_bIsCoinBase) : m_pimpl->m_txIns.size()==1 && m_pimpl->m_txIns[0].PrevOutPoint.IsNull();
	}

	bool IsCoinStake() const {
		return !TxIns().empty() && !TxIns()[0].PrevOutPoint.IsNull() && TxOuts().size()>=0 && TxOuts()[0].IsEmpty();
	}

	Int64 get_Fee() const;
	DEFPROP_GET(Int64, Fee);

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
	friend HashValue SignatureHash(const ConstBuf& script, const Tx& txTo, int nIn, Int32 nHashType);
};

COIN_EXPORT HashValue Hash(const CPersistent& pers);

DbWriter& operator<<(DbWriter& wr, const Tx& tx);
COIN_EXPORT const DbReader& operator>>(const DbReader& rd, Tx& tx);


class AuxPow;

} namespace Ext {
	template <> struct ptr_traits<Coin::AuxPow> {
		typedef Interlocked interlocked_policy;
	};
} namespace Coin {

struct TxHashOutNum {
	HashValue HashTx;
	UInt32 NOuts;
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
#ifdef X_DEBUG //!!!D
	Blob Binary;
#endif

	ptr<Coin::AuxPow> AuxPow;
	
	TxHashesOutNums m_txHashesOutNums;
	
	mutable volatile bool m_bTxesLoaded;
	mutable optional<Coin::HashValue> m_hash;

	BlockObj()
		:	m_bTxesLoaded(false)
	{}

	~BlockObj();
	virtual Coin::HashValue Hash() const;
	virtual Coin::HashValue PowHash(CoinEng& eng) const;
	Coin::HashValue MerkleRoot(bool bSave=true) const override;

	virtual void Write(DbWriter& wr) const;
	virtual void Read(const DbReader& rd);
	
	Target get_DifficultyTarget() const { return Target(DifficultyTargetBits); }
	DEFPROP_GET(Target, DifficultyTarget);

	virtual ProofOf ProofType() const { return ProofOf::Work; }
	const Coin::CTxes& get_Txes() const;
	virtual HashValue HashFromTx(const Tx& tx, int n) const;
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
	{
	}

	virtual BlockObj *Clone() { return new BlockObj(*this); }
	void WriteHeader(BinaryWriter& wr) const override;
	virtual void ReadHeader(const BinaryReader& rd, bool bParent);
	virtual void Write(BinaryWriter& wr) const;
	virtual void Read(const BinaryReader& rd);
	virtual BigInteger GetWork() const;
	virtual void CheckPow(const Target& target);
	virtual void CheckSignature() {}
	virtual void Check(bool bCheckMerkleRoot);
	virtual void CheckCoinbaseTx(Int64 nFees);	
	virtual void ComputeAttributes() {}

private:
	mutable CCoinMerkleTree m_merkleTree;

	friend class Block;
};

class COIN_CLASS Block : public Pimpl<BlockObj>, public CPersistent {
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
	void ReadHeader(const BinaryReader& rd, bool bParent = false) { m_pimpl->ReadHeader(rd, bParent); }

	void Write(BinaryWriter& wr) const override { m_pimpl->Write(wr); }
	void Read(const BinaryReader& rd) override { m_pimpl->Read(rd); }

	void LoadToMemory();

	UInt32 GetSerializeSize() const {
		return (UInt32)EXT_BIN(_self).Size;
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

	UInt32 get_Nonce() const {
		return m_pimpl->Nonce;
	}
	DEFPROP_GET(UInt32, Nonce);

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

	bool HasBestChainWork() const;
	Block GetOrphanRoot() const;
	void Connect() const;
	void Disconnect() const;
	void Reorganize();	
	void Accept();	
	void Process(Link *link = 0);

	bool IsInMainChain() const;
	bool IsSuperMajority(int minVersion, int nRequired, int nToCheck);

	void Check(bool bCheckMerkleRoot) const {
		get_Txes();
		m_pimpl->Check(bCheckMerkleRoot);
	}

	UInt32 get_Ver() const { return m_pimpl->Ver; }
	void put_Ver(UInt32 v) { m_pimpl->Ver = v; }
	DEFPROP(UInt32, Ver);

	int get_ChainId() const { return Ver >> 16; }
	void put_ChainId(int v) { Ver = (Ver & 0xFFFF) | (v << 16); }
	DEFPROP(int, ChainId);

	DateTime GetMedianTimePast();

	Block Clone() const {
		return Block(m_pimpl->Clone());
	}
protected:
	friend class BlockObj;
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
	
	typedef LruMap<UInt32, HashValue> CHeightToHashCache;
	CHeightToHashCache HeightToHashCache;	

	typedef LruMap<HashValue, Tx> CHashToTxCache;
	CHashToTxCache HashToTxCache;
	
	DateTime DtBestReceived;

	typedef LruMap<HashValue, Block> COrphanMap;
	COrphanMap OrphanBlocks;

	typedef LruMap<HashValue, Tx> CRelayHashToTx;
	CRelayHashToTx m_relayHashToTx;
	
	typedef LruMap<Int64, PubKeyHash160> CCachePkIdToPubKey;
	CCachePkIdToPubKey m_cachePkIdToPubKey;
	bool PubkeyCacheEnabled;

	Block m_bestBlock;

	ChainCaches();

	friend class CoinEng;
};


class COIN_CLASS Address : public HashValue160, public CPrintable {
public:
	String Comment;
	byte Ver;

	Address();
	explicit Address(const HashValue160& hash, RCString comment = "");
	explicit Address(const HashValue160& hash, byte ver);
	explicit Address(RCString s, CoinEng *eng);
	void CheckVer(CoinEng& eng) const;
	String ToString() const override;

	bool operator<(const Address& a) const {
		return Ver < a.Ver ||
			(Ver == a.Ver && memcmp(data(), a.data(), 20) < 0);
	}
};

class PrivateKey : public CPrintable {
public:
	PrivateKey() {}
	PrivateKey(const ConstBuf& cbuf, bool bCompressed);
	explicit PrivateKey(RCString s);
	pair<Blob, bool> GetPrivdataCompressed() const;
	String ToString() const override;
private:
	Blob m_blob;
};


extern const byte s_mergedMiningHeader[4];
int CalcMerkleIndex(int merkleSize, int chainId, UInt32 nonce);
COIN_EXPORT ConstBuf FindStandardHashAllSigPubKey(const ConstBuf& cbuf);


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

class CConnectJob : public NonCopiable {
public:
	CoinEng& Eng;
	
	typedef unordered_map<HashValue, ptr<ConnectTask>> CHashToTask;
	CHashToTask HashToTask;	

	unordered_set<ptr<ConnectTask>> Tasks;
	CBool PayToScriptHash;
	Int32 Height;
	CTxMap TxMap;
	Int64 Fee;
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

	unordered_set<Int64> InsertedIds;


	mutable volatile Int32 SigOps;
	mutable volatile bool Failed;

	CConnectJob(CoinEng& eng)
		:	Eng(eng)
		,	SigOps(0)
		,	Fee(0)
		,	Failed(false)
	{}

	void AsynchCheckAll(const vector<Tx>& txes);
	void Prepare(const Block& block);
	void Calculate();
	void PrepareTask(const HashValue160& hash160, const ConstBuf& pubKey);
};



} // Coin::


