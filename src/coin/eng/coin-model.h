/*######   Copyright (c) 2013-2019 Ufasoft  http://ufasoft.com  mailto:support@ufasoft.com,  Sergey Pavlov  mailto:dev@ufasoft.com ####
#                                                                                                                                     #
# 		See LICENSE for licensing information                                                                                         #
#####################################################################################################################################*/

#pragma once

#include <el/xml.h>

#include EXT_HEADER_DECIMAL
using namespace std::decimal;

#include EXT_HEADER_FUTURE
#include EXT_HEADER_OPTIONAL

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
using P2P::Peer;

#ifndef UCFG_COIN_GENERATE
#	define UCFG_COIN_GENERATE (!UCFG_WCE)
#endif

#ifndef UCFG_COIN_GENERATE_TXES_FROM_POOL
#	define UCFG_COIN_GENERATE_TXES_FROM_POOL 0
#endif

#include "param.h"

#include "../util/util.h"
#include "crypter.h"

namespace Coin {

class Link;
class SignatureHasher;

class NodeServices {
public:
	static const uint64_t
		NODE_NETWORK = 1
		, NODE_GETUTXO = 2
		, NODE_BLOOM = 4
		, NODE_WITNESS = 8
		, NODE_XTHIN = 16
		, NODE_NETWORK_LIMITED = 0x400;

	static String ToString(uint64_t s);
};

const int BLOCK_VERSION_AUXPOW = 256;

const uint32_t LOCKTIME_THRESHOLD = 500000000; // Year 1985. Pre-Bitcoin time

ENUM_CLASS(SigHashType) {
	ZERO = 0
	, SIGHASH_ALL			= 1
	, SIGHASH_NONE			= 2
	, SIGHASH_SINGLE		= 3
	, SIGHASH_FORKID		= 0x40			// BCH
	, SIGHASH_ANYONECANPAY	= 0x80
	, MASK = 0x1F,
} END_ENUM_CLASS(SigHashType)

class CoinEng;
class Block;
class Tx;
class TxObj;
class TxHashesOutNums;
class CConnectJob;
class CoinsView;
class TxOut;
struct OutPoint;
interface ITxoMap;

interface ITxMap {
	virtual Tx Find(const HashValue& hash) const = 0;
	virtual const Tx& Get(const HashValue& hash) const = 0;
	virtual const TxOut& GetOutputFor(const OutPoint& prev) const = 0;
	virtual void Add(const Tx& tx) = 0;
};

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

class DbWriter : public ProtocolWriter {
	typedef ProtocolWriter base;

public:
	observer_ptr<TxHashesOutNums> PTxHashesOutNums;
	observer_ptr<const Block> PCurBlock;
	observer_ptr<CConnectJob> ConnectJob;
	bool BlockchainDb;

	DbWriter(Stream& stm)
		: base(stm)
		, BlockchainDb(true)
	{
	}
};

class DbReader : public ProtocolReader {
	typedef ProtocolReader base;

public:
	CoinEng* Eng;
	observer_ptr<const Block> PCurBlock;
	int NOut;
	bool BlockchainDb;
	bool ForHeader; //!!!O Remove in 2020
	bool ReadTxHashes;

	DbReader(const Stream& stm, CoinEng* eng = 0)
		: base(stm, true)
		, Eng(eng)
		, NOut(-1)
		, BlockchainDb(true)
		, ForHeader(false)
		, ReadTxHashes(true) {
	}
};

struct OutPoint {
	HashValue TxHash;
	int32_t Index;

	OutPoint(const HashValue& txHash = HashValue::Null(), int32_t idx = -1)
		: TxHash(txHash)
		, Index(idx) {
	}

	bool operator==(const OutPoint& v) const {
		return !memcmp(this, &v, sizeof(TxHash) + sizeof(Index));
	}

	bool operator!=(const OutPoint& v) const { return !operator==(v); }

	bool IsNull() const {
		return TxHash == HashValue::Null() && -1 == Index;
	}

	void Write(BinaryWriter& wr) const {
		uint32_t buf[9];
		memcpy(buf, TxHash.data(), 32);
		buf[8] = htole32(Index);
		wr.Write(buf, sizeof buf);
	}

	void Read(const BinaryReader& rd) {
		rd >> TxHash >> Index;
	}

	array<uint8_t, 36> ToArray() const {
		array<uint8_t, 36> r;
		memcpy(r.data(), TxHash.data(), 32);
		*(uint32_t*)(r.data() + 32) = htole(Index);
		return r;
	}
};

inline ostream& operator<<(ostream& os, const OutPoint& op) {
	return os << op.TxHash << ":" << op.Index;
}


} // namespace Coin
namespace std {
template <> struct hash<Coin::OutPoint> {
	size_t operator()(const Coin::OutPoint& op) const {
		return hash<Coin::HashValue>()(op.TxHash) + hash<int32_t>()(op.Index);
	}
};
} // namespace std
namespace Coin {

COIN_CLASS uint8_t TryParseDestination(RCSpan pkScript, Span& hash160, Span& pubkey);

class COIN_CLASS TxOutBase {
	typedef TxOutBase class_type;
public:
	typedef AutoBlob<28> CScriptPubKey;		// 25 is enough for P2PKH outs, but 28 eats the same space because of alignment
	mutable CScriptPubKey m_scriptPubKey;

	TxOutBase(const Blob& script = Blob())
		: m_scriptPubKey(script) {
	}

#if UCFG_COIN_USE_NORMAL_MODE
	mutable CIdPk m_idPk; // changed when saving/locating Tx. Referenced by TxIn reading.
	uint8_t m_typ;

	Span get_ScriptPubKey() const;
#else
	Span get_ScriptPubKey() const noexcept { return m_scriptPubKey; }
#endif

	DEFPROP_GET(Span, ScriptPubKey);
};

class COIN_CLASS TxOut : public TxOutBase, public CPersistent {
	typedef TxOut class_type;
	typedef TxOutBase base;
public:
	int64_t Value;

	TxOut(int64_t val = -1, const Blob& script = Blob())
		: base(script)
		, Value(val) {
	}

	void Write(BinaryWriter& wr) const override;
	void Read(const BinaryReader& rd) override;

    int64_t DustThreshold(int64_t feeRate) const;
    bool IsDust(int64_t feeRate) const { return Value < DustThreshold(feeRate); }
	void CheckForDust() const;

    static Address CheckStandardType(RCSpan scriptPubKey);

	bool IsEmpty() const {
		return Value == 0 && get_ScriptPubKey().empty();
	}

	friend interface WalletBase;
	friend COIN_EXPORT const DbReader& operator>>(const DbReader& rd, Tx& tx);
};

typedef AutoBlob<72> StackValue;	// signature length

class VmStack {
public:
	typedef StackValue Value;

	static const Value TrueValue, FalseValue;
	vector<Value> Stack;

	SignatureHasher* m_signatureHasher;

	VmStack(SignatureHasher* signatureHasher)
		: m_signatureHasher(signatureHasher)
	{
		Stack.reserve(6);
	}

	Value& operator[](unsigned idx) { return GetStack(idx); }
	static BigInteger ToBigInteger(const Value& v);
	static Value FromBigInteger(const BigInteger& bi);
	Value& GetStack(unsigned idx);
	void SkipStack(int n);
	Value Pop();
	void Push(const Value& v);
};

class COIN_CLASS TxIn { // Keep this struct small without VTBL
	mutable Blob m_script;
	Blob m_sig;
	const TxObj* m_pTxo;
	TxOutBase PrevTxOut;
public:
	static const uint32_t SEQUENCE_LOCKTIME_DISABLE_FLAG = 1U << 31, SEQUENCE_LOCKTIME_TYPE_FLAG = 1 << 22, SEQUENCE_LOCKTIME_MASK = 0x0000ffff;
	static const int SEQUENCE_LOCKTIME_GRANULARITY = 9;

    vector<StackValue> Witness;
    OutPoint PrevOutPoint;
	uint32_t Sequence;
	uint8_t RecoverPubKeyType;

	TxIn()
		: m_script(nullptr)
		, m_pTxo(0)
		, Sequence(UINT_MAX)
		, RecoverPubKeyType(0) {
	}

	bool IsFinal() const {
		return Sequence == numeric_limits<uint32_t>::max();
	}

#if UCFG_COIN_USE_NORMAL_MODE
	Span Script() const;
#else
	Span Script() const { return m_script; }
#endif

	void put_Script(RCSpan script) {
		m_script = script;
	}

    unsigned CountWitnessSigOps() const;
	void Write(ProtocolWriter& wr, bool writeForSigHash = false) const;
	void Read(const BinaryReader& rd);

	friend class TxObj;
};

class COIN_CLASS Target {
public:
	uint32_t m_value;

	explicit Target(uint32_t v = 0x1d00ffff);
	explicit Target(const BigInteger& bi);

	bool operator==(const Target& t) const;
	bool operator<(const Target& t) const;

	operator BigInteger() const;

	void Write(BinaryWriter& wr) const {
		wr << m_value;
	}

	void Read(const BinaryReader& rd) {
		_self = Target(rd.ReadUInt32());
	}
};

class COIN_CLASS TxObj : public Object, public CoinSerialized {
	typedef TxObj class_type;

public:
	typedef InterlockedPolicy interlocked_policy;

	mutable Coin::HashValue m_hash;

	vector<TxOut> TxOuts;
	optional<DateTime> Timestamp;
	DateTime LockTimestamp;

	mutable vector<TxIn> m_txIns;
	uint32_t LockBlock;
	int32_t Height;
#if UCFG_COIN_TXES_IN_BLOCKTABLE
	uint32_t OffsetInBlock;
#endif
	mutable volatile uint8_t m_nBytesOfHash;
	CBool m_bIsCoinBase;
	mutable volatile bool m_bLoadedIns;

	TxObj();

	virtual TxObj* Clone() const {
		return new TxObj(_self);
	}

	virtual void Write(ProtocolWriter& wr) const;
	virtual void Read(const ProtocolReader& rd);
	virtual void WritePrefix(BinaryWriter& wr) const {
	}
	virtual void ReadPrefix(const BinaryReader& rd) {
	}
	virtual void WriteSuffix(BinaryWriter& wr) const {
	}
	virtual void ReadSuffix(const BinaryReader& rd) {
	}
	bool HasWitness() const;
	bool IsFinal(int height = 0, const DateTime dt = DateTime(0)) const;
	virtual int64_t GetCoinAge() const {
		Throw(E_NOTIMPL);
	}
	void ReadTxIns(const DbReader& rd) const;
	virtual String GetComment() const {
		return nullptr;
	}

	bool IsCoinBase() const {
		return m_txIns.empty() ? bool(m_bIsCoinBase) : m_txIns.size() == 1 && m_txIns[0].PrevOutPoint.IsNull();
	}
	virtual bool IsCoinStake() const { return false; }
protected:
	const vector<TxIn>& TxIns() const;
	virtual void CheckCoinStakeReward(int64_t reward, const Target& target) const {
	}
private:
	friend class Tx;
	friend class SignatureHasher;
	friend class Vm;
};

class SignatureHasher {
public:
	HashValue m_hashPrevOuts, m_hashSequence, m_hashOuts;
	uint64_t m_amount;
	const TxObj& m_txoTo;
	uint32_t NIn;
	SigHashType HashType;
	bool m_bWitness;

	SignatureHasher(const TxObj& txoTo);
	void CalcWitnessCache();
	HashValue HashForSig(RCSpan script);
	bool VerifyWitnessProgram(Vm& vm, uint8_t witnessVer, RCSpan witnessProgram);
	bool VerifyScript(RCSpan scriptSig, Span scriptPk);
	const OutPoint& GetOutPoint() const;
	void VerifySignature(RCSpan scriptPk);
	bool CheckSig(Span sig, RCSpan pubKey, RCSpan script, bool bInMultiSig = false);
};

ENUM_CLASS(MinFeeMode){Block, Tx, Relay} END_ENUM_CLASS(MinFeeMode);

class COIN_CLASS Tx : public Pimpl<TxObj>, public IPersistent<ProtocolWriter, ProtocolReader> {
	typedef Tx class_type;
public:
    enum class StandardStatus {
        Standard
        , Version
        , TxSize
        , ScriptSigSize
        , ScriptSigNotPushOnly
        , ScriptPubKey
        , BareMultiSig
        , Dust
        , MultiOpReturn
    };

	Tx() {
	}

	/*!!!	Tx(std::nullptr_t) {
	}*/

	Tx(TxObj* txObj) {
		m_pimpl = txObj;
	}

	static bool AllowFree(double priority);
	static bool TryFromDb(const HashValue& hash, Tx* ptx);
	static Tx FromDb(const HashValue& hash);

	enum class LocktimeType { Block, Timestamp };

	static LocktimeType LocktimeTypeOf(uint32_t nLockTime) {
		return nLockTime >= LOCKTIME_THRESHOLD ? LocktimeType::Timestamp : LocktimeType::Block;
	}

	void EnsureCreate(CoinEng& eng);
	virtual void Write(ProtocolWriter& wr) const override;
    virtual void Read(const ProtocolReader& rd) override;
	void Check() const;
    static int64_t CalcMinFee(uint32_t size, int64_t feeRate);
	int64_t GetMinFee(uint32_t blockSize = 1, bool bAllowFree = true, MinFeeMode mode = MinFeeMode::Block, uint32_t nBytes = uint32_t(-1)) const;

	int32_t get_Height() const { return m_pimpl->Height; }
	DEFPROP_GET(int32_t, Height);

    uint32_t GetSerializeSize(bool witnessAware = false) const;

    uint32_t get_Weight() const;
    DEFPROP_GET(uint32_t, Weight);

    StandardStatus CheckStandard() const;

	uint32_t get_LockBlock() const { return m_pimpl->LockBlock; }
	void put_LockBlock(uint32_t v) {
		m_pimpl->LockBlock = v;
	}
	DEFPROP(uint32_t, LockBlock);

	int64_t get_ValueOut() const;
	DEFPROP_GET(int64_t, ValueOut);

	int get_DepthInMainChain() const;
	DEFPROP_GET(int, DepthInMainChain);

	unsigned SigOpCost(const ITxoMap& txoMap) const;
	pair<int, DateTime> CalculateSequenceLocks(vector<int>& inHeights, const HashValue& hashBlock) const;
	void CheckInOutValue(int64_t nValueIn, int64_t& nFees, int64_t minFee, const Target& target) const;
	void ConnectInputs(CoinsView& view, int32_t height, int& nBlockSigOps, int64_t& nFees, bool bBlock, bool bMiner, int64_t minFee, const Target& target) const;
	unsigned GetP2SHSigOpCount(const ITxoMap& txoMap) const;
	bool IsNewerThan(const Tx& txOld) const;
	void WriteTxIns(DbWriter& wr) const;

	const vector<TxOut>& TxOuts() const { return m_pimpl->TxOuts; }
	vector<TxOut>& TxOuts() { return m_pimpl->TxOuts; }
	const vector<TxIn>& TxIns() const { return m_pimpl->TxIns(); }

	int64_t get_Fee() const;
	DEFPROP_GET(int64_t, Fee);

	void SetHash(const HashValue& hash) const {
		m_pimpl->m_hash = hash;
		m_pimpl->m_nBytesOfHash = 32;
	}

	void SetReducedHash(RCSpan txid8) const {
		memcpy(m_pimpl->m_hash.data(), txid8.data(), m_pimpl->m_nBytesOfHash = 8);
	}

private:
	friend interface WalletBase;
	friend class Wallet;
	friend class CoinDb;
	friend class EmbeddedMiner;
	friend COIN_EXPORT HashValue Hash(const Tx& tx);
	friend COIN_EXPORT const DbReader& operator>>(const DbReader& rd, Tx& tx);
};

class Penny : public TxOut {
	typedef Penny class_type;
public:
	Coin::OutPoint OutPoint;
	CBool IsSpent;

	Penny() {
		Value = 0;
	}

	bool operator==(const Penny& v) const {
		return OutPoint == v.OutPoint && Value == v.Value;
	}

	int64_t get_Debit() const;
	DEFPROP_GET(int64_t, Debit);

	bool get_IsFromMe() const { return Debit > 0; }
	DEFPROP_GET(bool, IsFromMe);

	size_t GetHashCode() const {
		return hash<Coin::OutPoint>()(OutPoint);
	}
};

EXT_DEF_HASH_NS(Coin, Penny);

class Signer {
public:
	SignatureHasher Hasher;
	KeyInfo Key;
	Tx& m_tx;

	Signer(Tx& tx)
		: Hasher(*tx.m_pimpl)
		, Key(nullptr)
		, m_tx(tx)
	{}

	Blob SignHash(const HashValue& hash);
	void Sign(const KeyInfo& randomKey, const Penny& penny, uint32_t nIn, SigHashType hashType = SigHashType::SIGHASH_ALL, Span scriptPrereq = Span());
protected:
	virtual KeyInfo GetMyKeyInfo(const HashValue160& hash160) = 0;
	virtual KeyInfo GetMyKeyInfoByScriptHash(const HashValue160& hash160) = 0;
	virtual Blob GetMyRedeemScript(const HashValue160& hash160) = 0;
};


HashValue WitnessHash(const Tx& tx);

COIN_EXPORT HashValue Hash(const CPersistent& pers);

DbWriter& operator<<(DbWriter& wr, const Tx& tx);
COIN_EXPORT const DbReader& operator>>(const DbReader& rd, Tx& tx);

class AuxPow;

} // namespace Coin
namespace Ext {
template <> struct ptr_traits<Coin::AuxPow> { typedef InterlockedPolicy interlocked_policy; };
} // namespace Ext
namespace Coin {

struct TxHashOutNum {
	HashValue HashTx;
	uint32_t NOuts;
};

class TxHashesOutNums : public vector<TxHashOutNum> {
	typedef vector<TxHashOutNum> base;

public:
	TxHashesOutNums() {
	}
	explicit TxHashesOutNums(RCSpan buf);
	pair<int, int> StartingTxOutIdx(const HashValue& hashTx) const; // return pair<TxIdxInBlock, OutIdxInBlock>
	pair<int, OutPoint> OutPointByIdx(int idx) const;				// return pair<TxIdxInBlock, OutPoint>
};

BinaryWriter& operator<<(BinaryWriter& wr, const TxHashesOutNums& hons);
const BinaryReader& operator>>(const BinaryReader& rd, TxHashesOutNums& hons);

class CTxes : public vector<Tx> {
	typedef vector<Tx> base;

public:
	CTxes() {
	}

	CTxes(const CTxes& txes);										// Cloning ctor
	pair<int, int> StartingTxOutIdx(const HashValue& hashTx) const; // return pair<TxIdxInBlock, OutIdxInBlock>
	pair<int, OutPoint> OutPointByIdx(int idx) const;				// return pair<TxIdxInBlock, OutPoint>

	using base::operator[];

	const Tx& operator[](const HashValue& hashTx) const {
		return at(StartingTxOutIdx(hashTx).first);
	}
};

DbWriter& operator<<(DbWriter& wr, const CTxes& txes);
COIN_EXPORT const DbReader& operator>>(const DbReader& rd, CTxes& txes);

ENUM_CLASS(ProofOf){
	Work
	, Stake
} END_ENUM_CLASS(ProofOf);

class COIN_CLASS BlockObj : public BlockBase {
	typedef BlockBase base;
	typedef BlockObj class_type;

	mutable mutex* volatile m_pMtx;
	mutable CCoinMerkleTree m_merkleTree;
public:
	mutable CTxes m_txes;
	ptr<Coin::AuxPow> AuxPow;

	TxHashesOutNums m_txHashesOutNums;
	mutable uint64_t OffsetInBootstrap;

	mutable Coin::HashValue m_hash;
	mutable volatile bool m_bHashCalculated;
	mutable volatile bool m_bTxesLoaded;

	BlockObj()
		: OffsetInBootstrap(0)
		, m_pMtx(0)
		, m_bTxesLoaded(false)
		, m_bHashCalculated(false)
	{
	}

	~BlockObj();
	virtual const Coin::HashValue& Hash() const;
	virtual Coin::HashValue PowHash() const;
	Coin::HashValue MerkleRoot(bool bSave = true) const override;

//!!!R	virtual void Write(DbWriter& wr) const;
	virtual void Read(const DbReader& rd);	//!!!O Remove in 2020

	Target get_DifficultyTarget() const {
		return Target(DifficultyTargetBits);
	}
	DEFPROP_GET(Target, DifficultyTarget);

	virtual ProofOf ProofType() const {
		return ProofOf::Work;
	}
	const Coin::CTxes& get_Txes() const;
	virtual HashValue HashFromTx(const Tx& tx, int n) const;

	void WriteHeader(ProtocolWriter& wr) const override;
	virtual void ReadHeader(const ProtocolReader& rd, bool bParent, const HashValue* pMerkleRoot);
	virtual void WriteHeaderInMessage(ProtocolWriter& wr) const;
	virtual void ReadHeaderInMessage(const ProtocolReader& rd);
	virtual BigInteger GetWork() const;

	virtual void WriteSuffix(BinaryWriter& wr) const {
	}
	virtual void WriteDbSuffix(BinaryWriter& wr) const {
	}
	virtual void ReadDbSuffix(const BinaryReader& rd) {
	}
	bool IsHeaderOnly() const {
		return !m_bTxesLoaded && m_txHashesOutNums.empty();
	}
	virtual void CheckHeader();

protected:
	BlockObj(BlockObj& bo);
	mutex& Mtx() const;

	virtual BlockObj* Clone() {
		return new BlockObj(*this);
	}
	virtual void Write(ProtocolWriter& wr) const;
	virtual void Read(const ProtocolReader& rd);
	virtual void CheckPow(const Target& target);
	virtual void CheckSignature() {}
	virtual void Check(bool bCheckMerkleRoot);
	virtual void CheckCoinbaseTx(int64_t nFees);
	int GetBlockHeightFromCoinbase() const;
	virtual void ComputeAttributes() {}
private:
	friend class Block;
	friend class BlockHeader;
	friend class TxMessage;
	friend class MerkleBlockMessage;
};

class COIN_CLASS BlockHeader : public Pimpl<BlockObj> , public IPersistent<ProtocolWriter, ProtocolReader> {
	typedef Pimpl<BlockObj> base;
	typedef BlockHeader class_type;

public:
	BlockHeader(BlockObj* pimpl = nullptr) {
		m_pimpl = pimpl;
	}

	void WriteHeader(ProtocolWriter& wr) const {
		m_pimpl->WriteHeader(wr);
	}

	void ReadHeader(const ProtocolReader& rd, bool bParent = false, const HashValue* pMerkleRoot = 0) {
		m_pimpl->ReadHeader(rd, bParent, pMerkleRoot);
	}

	int get_Height() const {
		return m_pimpl->Height;
	}
	DEFPROP_GET(int, Height);

	int get_SafeHeight() const {
		return m_pimpl ? m_pimpl->Height : -1;
	}
	DEFPROP_GET(int, SafeHeight);

	int get_ChainId() const {
		return m_pimpl->Ver >> 16;
	}
	void put_ChainId(int v) {
		m_pimpl->Ver = (m_pimpl->Ver & 0xFFFF) | (v << 16);
	}
	DEFPROP(int, ChainId);

	DateTime get_Timestamp() const {
		return m_pimpl->Timestamp;
	}
	DEFPROP_GET(DateTime, Timestamp);

	Coin::HashValue get_MerkleRoot() const {
		return m_pimpl->MerkleRoot();
	}
	DEFPROP_GET(HashValue, MerkleRoot);

	const HashValue& get_PrevBlockHash() const {
		return m_pimpl->PrevBlockHash;
	}
	DEFPROP_GET(const HashValue&, PrevBlockHash);

	bool IsInTrunk() const;
	DateTime GetMedianTimePast() const;
	BlockHeader GetPrevHeader() const;
	bool IsSuperMajority(int minVersion, int nRequired, int nToCheck) const;
	void CheckAgainstCheckpoint() const;
	void UpdateLastCheckpointed() const;
	bool HasBestChainWork() const;
	virtual void Connect() const;
	virtual void Accept(Link *link);
};

class COIN_CLASS Block : public BlockHeader {
	typedef BlockHeader base;
	typedef Block class_type;

public:
	Block();

	Block(std::nullptr_t) {
	}

	Block(BlockObj* pimpl)
		: base(pimpl) {
	}

	void Write(ProtocolWriter& wr) const override { m_pimpl->Write(wr); }
    void Read(const ProtocolReader& rd) override { m_pimpl->Read(rd); }

	uint32_t OffsetOfTx(const HashValue& hashTx) const;

	void LoadToMemory();

    uint32_t GetSerializeSize(bool bWitnessAware = false) const;

    size_t Weight() const { return GetSerializeSize(true); }

	const CTxes& get_Txes() const {
		return m_pimpl->get_Txes();
	}
	DEFPROP_GET(const CTxes&, Txes);

	const Coin::TxHashesOutNums& get_TxHashesOutNums() const {
		return m_pimpl->m_txHashesOutNums;
	}
	DEFPROP_GET(const Coin::TxHashesOutNums&, TxHashesOutNums);

	void Add(const Tx& tx) {
		m_pimpl->m_merkleRoot.reset();
		m_pimpl->m_bHashCalculated = false;
		m_pimpl->m_bTxesLoaded = true;
		m_pimpl->m_txes.push_back(tx);
	}

	Tx& GetFirstTxRef() {
		return m_pimpl->m_txes[0];
	}

	Block GetOrphanRoot() const;
	void Check(bool bCheckMerkleRoot) const;
	void Check() const;
    const uint8_t *GetWitnessCommitment() const;
    void ContextualCheck(const BlockHeader& blockPrev);
	void Connect() const override;
	void Disconnect() const;
	void Accept(Link *link) override;
	void Process(Link* link = 0, bool bRequested = true);

	Block Clone() const {
		return Block(m_pimpl->Clone());
	}

protected:
	friend class BlockObj;
	friend class MerkleBlockMessage;
};

inline const HashValue& Hash(const BlockHeader& block) {
	return block->Hash();
}

class MerkleTx : public Tx {
	typedef Tx base;

public:
	HashValue BlockHash;
	CCoinMerkleBranch MerkleBranch;

	MerkleTx() {
		MerkleBranch.m_h2 = &HashValue::Combine;
	}

	void Write(ProtocolWriter& wr) const override;
	void Read(const ProtocolReader& rd) override;
};

class CoinPartialMerkleTree : public PartialMerkleTree<HashValue, PFN_Combine> {
	typedef PartialMerkleTree<HashValue, PFN_Combine> base;

public:
	CoinPartialMerkleTree()
		: base(&HashValue::Combine) {
	}

	void Init(const vector<HashValue>& vHash, const dynamic_bitset<uint8_t>& bsMatch);
	pair<HashValue, vector<HashValue>> ExtractMatches() const; // returns pair<MerkleRoot, matches[]>
};

class AuxPow : public Object, public MerkleTx {
	typedef MerkleTx base;

public:
	CCoinMerkleBranch ChainMerkleBranch;
	Block ParentBlock;

	AuxPow() {
		ChainMerkleBranch.m_h2 = &HashValue::Combine;
	}

	void Write(ProtocolWriter& wr) const override;
	void Read(const ProtocolReader& rd) override;
	void Write(DbWriter& wr) const;
	void Read(const DbReader& rd);
	void Check(const Block& blockAux);
};

struct PubKeyHash160 {
	CanonicalPubKey PubKey;
	HashValue160 Hash160;

	PubKeyHash160()
		: PubKey(nullptr) {
	}

	explicit PubKeyHash160(const CanonicalPubKey& pubKey)
		: PubKey(pubKey)
		, Hash160(pubKey.Hash160) {
	}

	PubKeyHash160(const CanonicalPubKey& pubKey, const HashValue160& hash160)
		: PubKey(pubKey)
		, Hash160(hash160) {
	}
};

PubKeyHash160 DbPubKeyToHashValue160(RCSpan mb);

struct SpentTx {
	HashValue HashTx;
	uint32_t Height;
	uint32_t Offset;
	uint16_t N; //!!!S may be not enough for 32MB blocks
};

class ChainCaches {
public:
	mutex Mtx;

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

	BlockHeader m_bestHeader, m_bestBlock;

	typedef list<SpentTx> CCacheSpentTxes;
	CCacheSpentTxes m_cacheSpentTxes; // used for fast Reorganize()

	ChainCaches();
	void Add(const SpentTx& stx);

	friend class CoinEng;
};

struct EnabledFeatures {
	bool PayToScriptHash, CheckLocktimeVerify, VerifyDerEnc, CheckSequenceVerify, SegWit;

	EnabledFeatures(bool bSet = false) {
		PayToScriptHash = CheckLocktimeVerify = VerifyDerEnc = CheckSequenceVerify = SegWit = bSet;
	}
};

struct ScriptPolicy {
	bool CleanStack, DiscourageUpgradableWidnessProgram, MinimalData;

	ScriptPolicy(bool bSet = false)
		: MinimalData(false)
	{
		CleanStack = DiscourageUpgradableWidnessProgram = bSet;
	}
};

extern const uint8_t s_mergedMiningHeader[4];
int CalcMerkleIndex(int merkleSize, int chainId, uint32_t nonce);
Span FindStandardHashAllSigPubKey(RCSpan cbuf);

class ConnectTask : public Object {
public:
	vector<Tx> Txes;
};

class CTxMap : public ITxMap {
public:
	Tx Find(const HashValue& hash) const override;
	const Tx& Get(const HashValue& hash) const override;
	const TxOut& GetOutputFor(const OutPoint& prev) const override;
	void Add(const Tx& tx) override;
private:
	unordered_map<HashValue, Tx> m_map;

	friend class CoinsView;
};

struct TxFeeTuple{
	Tx Tx;
	int64_t Fee;

	TxFeeTuple()
		: Fee(0)
	{}

	TxFeeTuple(const Coin::Tx& tx)
		: Tx(tx)
		, Fee(0)
	{}
};

class CFutureTxMap : public ITxMap {
	CoinEng& m_eng;

	mutable mutex m_mtx;
	unordered_map<HashValue, shared_future<TxFeeTuple>> m_map;
	//----
public:
	CFutureTxMap(CoinEng& eng) : m_eng(eng) {}
	const Tx& Get(const HashValue& hash) const override;
	void Add(const HashValue& hash, const shared_future<TxFeeTuple>& ft);
	void Ensure(const HashValue& hash);
	bool Contains(const HashValue& hash) const;
	Tx Find(const HashValue& hash) const override;
	const TxOut& GetOutputFor(const OutPoint& prev) const override;
	void Add(const Tx& tx) override;
};

typedef TxOut Txo;

interface ITxoMap {
	virtual Txo Get(const OutPoint& op) const = 0;
};

class TxoMap : public ITxoMap {
	CoinEng& m_eng;

	struct Entry {
		shared_future<Txo> Ft;
		CBool InCurrentBlock;

		Entry() {}

		Entry(shared_future<Txo>&& ft, bool inCurrentBlock)
			: Ft(move(ft))
			, InCurrentBlock(inCurrentBlock)
		{}
	};

	mutable mutex m_mtx;
	mutable unordered_map<OutPoint, Entry> m_map;
	//----
public:
	TxoMap(CoinEng& eng) : m_eng(eng) {}
	void Add(const OutPoint& op, int height);
	void AddAllOuts(const HashValue& hashTx, const Tx& tx);
	Txo Get(const OutPoint& op) const override;
};

class CoinsView : public ITxMap, public ITxoMap {
public:
	CoinEng& Eng;
	mutable CTxMap TxMap;

	CoinsView(CoinEng& eng) : Eng(eng) {}
	bool HasInput(const OutPoint& op) const;
	void SpendInput(const OutPoint& op);
	Tx Find(const HashValue& hash) const override;
	const Tx& Get(const HashValue& hash) const override;
	const TxOut& GetOutputFor(const OutPoint& prev) const override;
	Txo Get(const OutPoint& op) const override;
	void Add(const Tx& tx) override;
private:
	mutable unordered_map<OutPoint, bool> m_outPoints;

	friend void swap(CoinsView& x, CoinsView& y);
};

inline void swap(CoinsView& x, CoinsView& y) {
	std::swap(x.TxMap, y.TxMap);
	swap(x.m_outPoints, y.m_outPoints);
}

class CConnectJob : noncopyable {
public:
	CoinEng& Eng;

	EnabledFeatures Features;
	TxoMap TxoMap;
	int64_t Fee;
	Target DifficultyTarget;

	struct PubKeyData {
		Blob PubKey;
		CBool Insert, Update, IsTask;

		PubKeyData()
			: PubKey(nullptr) {
		}
	};

	typedef unordered_map<HashValue160, PubKeyData> CMap;
	CMap Map;

	unordered_set<int64_t> InsertedIds;

	int MaxSigOps;
	int32_t Height;
	mutable atomic<int> aSigOps;
	mutable volatile bool Failed;

	CConnectJob(CoinEng& eng);
	void AsynchCheckAll(const vector<Tx>& txes);	//!!!Obsolete
	void AsynchCheckAllSharedFutures(const vector<Tx>& txes, int height);
	void Prepare(const Block& block);
	void Calculate();
	void PrepareTask(const HashValue160& hash160, const CanonicalPubKey& pubKey);
};

} // namespace Coin
