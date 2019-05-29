/*######   Copyright (c) 2011-2019 Ufasoft  http://ufasoft.com  mailto:support@ufasoft.com,  Sergey Pavlov  mailto:dev@ufasoft.com ####
#                                                                                                                                     #
# 		See LICENSE for licensing information                                                                                         #
#####################################################################################################################################*/

#include <el/ext.h>

#include EXT_HEADER_FUTURE

#include <el/crypto/hash.h>

#include "coin-protocol.h"
#include "script.h"
#include "eng.h"

#	include <crypto/cryp/secp256k1.h>
#if UCFG_COIN_ECC=='S'
#else
#	define OPENSSL_NO_SCTP
#	include <openssl/err.h>
#	include <openssl/ec.h>
#endif

namespace Coin {

CTxes::CTxes(const CTxes& txes)												// Cloning ctor
	:	base(txes.size())
{
	for (int i = 0; i < size(); ++i)
		_self[i].m_pimpl = txes[i].m_pimpl->Clone();
}

pair<int, int> CTxes::StartingTxOutIdx(const HashValue& hashTx) const {
	pair<int, int> r(-1, -1);
	for (int i=0, n=0; i<size(); ++i) {
		const Tx& tx = _self[i];
		if (tx.m_pimpl->m_hash == hashTx)
			return make_pair(i, n);
		n += tx.TxOuts().size();
	}
	return make_pair(-1, -1);
}

pair<int, OutPoint> CTxes::OutPointByIdx(int idx) const {
	for (int i=0; i<size(); ++i) {
		const Tx& tx = _self[i];
		if (idx < tx.TxOuts().size())
			return make_pair(i, OutPoint(tx.m_pimpl->m_hash, idx));
		idx -= tx.TxOuts().size();
	}
	Throw(CoinErr::InconsistentDatabase);
}

pair<int, int> TxHashesOutNums::StartingTxOutIdx(const HashValue& hashTx) const {
	for (int i=0, n=0; i<size(); ++i) {
		const TxHashOutNum& hom = _self[i];
		if (hom.HashTx == hashTx)
			return make_pair(i, n);
		n += hom.NOuts;
	}
	return make_pair(-1, -1);
}

pair<int, OutPoint> TxHashesOutNums::OutPointByIdx(int idx) const {
	for (int i=0; i<size(); ++i) {
		const TxHashOutNum& hom = _self[i];
		if (idx < hom.NOuts)
			return make_pair(i, OutPoint(hom.HashTx, idx));
		idx -= hom.NOuts;
	}
	Throw(CoinErr::InconsistentDatabase);
}


BinaryWriter& operator<<(BinaryWriter& wr, const TxHashesOutNums& hons) {
	EXT_FOR (const TxHashOutNum& hon, hons) {
		(wr << hon.HashTx).Write7BitEncoded(hon.NOuts);
	}
	return wr;
}

const BinaryReader& operator>>(const BinaryReader& rd, TxHashesOutNums& hons) {
	hons.clear();
	hons.reserve(size_t(rd.BaseStream.Length / 33));				// upper bound
	while  (!rd.BaseStream.Eof()) {
		TxHashOutNum hon;
		hon.NOuts = (uint32_t)(rd >> hon.HashTx).Read7BitEncoded();
		hons.push_back(hon);
	}
	return rd;
}

TxHashesOutNums::TxHashesOutNums(RCSpan buf)
	: base(buf.size() / 33)											// upper bound
{
	CMemReadStream stm(buf);
	BinaryReader rd(stm);
	int i = 0;
	for (; !rd.BaseStream.Eof(); ++i) {
		TxHashOutNum& hon = _self[i];
		stm.ReadBuffer(hon.HashTx.data(), 32);
		hon.NOuts = (uint32_t)rd.Read7BitEncoded();
	}
	resize(i);
}

Target::Target(uint32_t v) {
	int32_t m = int32_t(v << 8) >> 8;
//	if (m < 0x8000)			// namecoin violates spec
//		Throw(errc::invalid_argument);
	m_value = v;
}

Target::Target(const BigInteger& bi) {
	Blob blob = bi.ToBytes();
	int size = blob.Size;
	m_value = size << 24;
	const uint8_t *p = blob.constData()+size;
	if (size >= 1)
		m_value |= p[-1] << 16;
	if (size >= 2)
		m_value |= p[-2] << 8;
	if (size >= 3)
		m_value |= p[-3];
}

Target::operator BigInteger() const {
	return BigInteger(int32_t(m_value << 8) >> 8) << 8*((m_value >> 24)-3);
}

bool Target::operator<(const Target& t) const {
	return BigInteger(_self) < BigInteger(t);
}

bool BlockHeader::IsInMainChain() const {
	return Eng().Db->HaveHeader(Hash(_self));
}

BlockObj::~BlockObj() {
	delete m_pMtx;
}

const CTxes& BlockObj::get_Txes() const {
	if (!m_bTxesLoaded) {
		if (m_txHashesOutNums.empty())
			Throw(E_FAIL);
		EXT_LOCK (Mtx()) {
			if (!m_bTxesLoaded) {
				m_txes.resize(m_txHashesOutNums.size());
#if UCFG_COIN_TXES_IN_BLOCKTABLE
				CoinEng& eng = Eng();
				eng.Db->ReadTxes(_self);
#else
				for (int i = 0; i < m_txHashesOutNums.size(); ++i) {
					Tx& tx = (m_txes[i] = Tx::FromDb(m_txHashesOutNums[i].HashTx));
					if (tx.Height != Height)								// protection against duplicated txes
						(tx = Tx(tx.m_pimpl->Clone())).m_pimpl->Height = Height;
				}
#endif
				m_bTxesLoaded = true;
			}
		}
	}
	return m_txes;
}

HashValue TxHashOf(const TxHashOutNum& hon, int n) {
	return hon.HashTx;
}

HashValue BlockObj::HashFromTx(const Tx& tx, int n) const {
	return Eng().HashFromTx(tx);
}

#if UCFG_COIN_MERKLE_FUTURES
static HashValue HashFromTx(CoinEng *peng, const BlockObj *block, int n) {
	CCoinEngThreadKeeper engKeeper(peng);
	const Tx& tx = block->get_Txes()[n];
	HashValue r = block->HashFromTx(tx, n);
	if (tx.m_pimpl->m_nBytesOfHash != 32)
		tx.SetHash(r);
	return r;
}

static HashValue WitnessHashFromTx(CoinEng *peng, const BlockObj *block, int n) {
    if (n == 0)
        return HashValue::Null();
    CCoinEngThreadKeeper engKeeper(peng);
    return WitnessHash(block->get_Txes()[n]);
}
#endif

struct CTxHasher {
	const BlockObj *m_block;
#if UCFG_COIN_MERKLE_FUTURES
	vector<future<HashValue>> *m_pfutures;
#endif
    CBool Witness;

	HashValue operator()(const Tx& tx, int n) {
#if UCFG_COIN_MERKLE_FUTURES
		return (*m_pfutures)[n].get();
#else
        return !Witness ? m_block->HashFromTx(tx, n)
            : n == 0 ? HashValue::Null() : WitnessHash(tx);
#endif
	}
};

static CCoinMerkleTree CalcTxHashes(const BlockObj& block) {
	const CTxes& txes = block.get_Txes();
#if UCFG_COIN_MERKLE_FUTURES
	vector<future<HashValue>> futures;
#endif

	CTxHasher h1 = { &block
#if UCFG_COIN_MERKLE_FUTURES
		, &futures
#endif
	};

#if UCFG_COIN_MERKLE_FUTURES
	CoinEng& eng = Eng();
	futures.reserve(txes.size());
	for (int i = 0; i < txes.size(); ++i)
		futures.push_back(std::async(&Coin::HashFromTx, &eng, &block, i));
#endif
	return BuildMerkleTree<Coin::HashValue>(txes, h1, &HashValue::Combine);
}

static CCoinMerkleTree CalcWitnessTxHashes(const BlockObj& block) {
    const CTxes& txes = block.get_Txes();
#if UCFG_COIN_MERKLE_FUTURES
    vector<future<HashValue>> futures;
#endif

    CTxHasher h1 = { &block
#if UCFG_COIN_MERKLE_FUTURES
        , &futures
#endif
    };
    h1.Witness = true;

#if UCFG_COIN_MERKLE_FUTURES
    CoinEng& eng = Eng();
    futures.reserve(txes.size());
    for (int i = 0; i < txes.size(); ++i)
        futures.push_back(std::async(&Coin::WitnessHashFromTx, &eng, &block, i));
#endif
    return BuildMerkleTree<Coin::HashValue>(txes, h1, &HashValue::Combine);
}

HashValue BlockObj::MerkleRoot(bool bSave) const {
	if (!bSave)
		return (m_merkleTree = CalcTxHashes(_self)).empty() ? Coin::HashValue::Null() : m_merkleTree.back();
	if (!m_merkleRoot) {
		if (m_txHashesOutNums.empty())
			get_Txes();						// to eliminate dead lock
		EXT_LOCK (Mtx()) {
			if (!m_merkleRoot) {
				m_merkleTree = m_txHashesOutNums.empty() ? CalcTxHashes(_self) : BuildMerkleTree<Coin::HashValue>(m_txHashesOutNums, &TxHashOf, &HashValue::Combine);
				m_merkleRoot = m_merkleTree.empty() ? Coin::HashValue::Null() : m_merkleTree.back();
			}
		}
	}
	return m_merkleRoot.value();
}

HashValue BlockObj::Hash() const {
	if (!m_hash) {
		BlockHeaderBinary header = {
			htole(Ver),
			{ 0 },
			{ 0 },
			htole((uint32_t)to_time_t(Timestamp)),
			htole(DifficultyTargetBits),
			htole(Nonce)
		};
		memcpy(header.PrevBlockHash, PrevBlockHash.data(), sizeof header.PrevBlockHash);
		memcpy(header.MerkleRoot, MerkleRoot().data(), sizeof header.MerkleRoot);
		Span hdata((const uint8_t*)&header, sizeof header);

		switch (Eng().ChainParams.HashAlgo) {
		case HashAlgo::Sha3:
			m_hash = HashValue(SHA3<256>().ComputeHash(hdata));
			break;
		case HashAlgo::Metis:
			m_hash = MetisHash(hdata);
			break;
		case HashAlgo::Groestl:
			m_hash = GroestlHash(hdata);
			break;
		default:
			m_hash = Coin::Hash(hdata);
		}
	}
	return m_hash.value();
}

HashValue BlockObj::PowHash() const {
	return Eng().ChainParams.HashAlgo == HashAlgo::SCrypt
		? ScryptHash(EXT_BIN(Ver << PrevBlockHash << MerkleRoot() << (uint32_t)to_time_t(Timestamp) << get_DifficultyTarget() << Nonce))
		: Hash();
}

Block::Block() {
	m_pimpl = Eng().CreateBlockObj();
}

void BlockObj::WriteHeader(ProtocolWriter& wr) const {
	base::WriteHeader(wr);
	CoinEng& eng = Eng();
	if (eng.ChainParams.AuxPowEnabled && (Ver & BLOCK_VERSION_AUXPOW)) {
		AuxPow->Write(wr);
	}
}

void BlockObj::ReadHeader(const ProtocolReader& rd, bool bParent, const HashValue *pMerkleRoot) {
	CoinEng& eng = Eng();

	Ver = rd.ReadInt32();

	rd >> PrevBlockHash;
	if (pMerkleRoot)
		m_merkleRoot = *pMerkleRoot;
	else {
		HashValue merkleRoot;
		rd >> merkleRoot;
		m_merkleRoot = merkleRoot;
	}
	Timestamp = DateTime::from_time_t(rd.ReadUInt32());
	DifficultyTargetBits = rd.ReadUInt32();
	Nonce = rd.ReadUInt32();

	if (!bParent && eng.ChainParams.AuxPowEnabled && (Ver & BLOCK_VERSION_AUXPOW)) {
		(AuxPow = new Coin::AuxPow)->Read(rd);
	} else
		AuxPow = nullptr;
}

void BlockObj::Write(ProtocolWriter& wr) const {
	WriteHeader(wr);
	CoinSerialized::Write(wr, get_Txes());
	WriteSuffix(wr);
}

void BlockObj::Read(const ProtocolReader& rd) {
#ifdef X_DEBUG//!!!D
	int64_t pos = rd.BaseStream.Position;
#endif

	ReadHeader(rd, false, 0);
	CoinSerialized::Read(rd, m_txes);
	m_bTxesLoaded = true;
	for (int i = 0; i < m_txes.size(); ++i) {
		Tx& tx = m_txes[i];
		tx.Height = Height;				// Valid only when Height already set
//!!!!R		tx.m_pimpl->N = i;
	}
//!!!R	m_bMerkleCalculated = true;
	m_hash.reset();
#ifdef X_DEBUG//!!!D
	int64_t len = rd.BaseStream.Position-pos;
	rd.BaseStream.Position = pos;
	Blob br(0, (size_t)len);
	rd.BaseStream.ReadBuffer(br.data(), (size_t)len);
	Binary = br;
#endif
}

void BlockObj::Write(DbWriter& wr) const {
	CoinEng& eng = Eng();

	CoinSerialized::WriteVarInt(wr, Ver);
	wr << (uint32_t)to_time_t(Timestamp) << get_DifficultyTarget() << Nonce;
	if (eng.Mode == EngMode::Lite || eng.Mode == EngMode::BlockParser || wr.ForHeader)
		wr << MerkleRoot();
	if (eng.Mode != EngMode::BlockParser) {
		if (eng.ChainParams.AuxPowEnabled && (Ver & BLOCK_VERSION_AUXPOW))
			AuxPow->Write(wr);
	}
}

void BlockObj::Read(const DbReader& rd) {
	CoinEng& eng = Eng();

	Ver = (uint32_t)CoinSerialized::ReadVarInt(rd);
	Timestamp = DateTime::from_time_t(rd.ReadUInt32());
	DifficultyTargetBits = rd.ReadUInt32();
	Nonce = rd.ReadUInt32();
	if (eng.Mode==EngMode::Lite || eng.Mode==EngMode::BlockParser || rd.ForHeader) {
		HashValue merkleRoot;
		rd >> merkleRoot;
		m_merkleRoot = merkleRoot;
	} else
		m_merkleRoot.reset();
	if (eng.Mode!=EngMode::BlockParser) {
		if (eng.ChainParams.AuxPowEnabled && (Ver & BLOCK_VERSION_AUXPOW))
			(AuxPow = new Coin::AuxPow)->Read(rd);
	}
}

BigInteger BlockObj::GetWork() const {
	return Eng().ChainParams.MaxTarget / get_DifficultyTarget();
}

void BlockObj::CheckPow(const Target& target) {
	if (ProofType() == ProofOf::Work) {
		HashValue hashPow = PowHash();
		uint8_t ar[33];
		memcpy(ar, hashPow.data(), 32);
		ar[32] = 0;
		if (BigInteger(ar, sizeof ar) >= BigInteger(target))
			Throw(CoinErr::ProofOfWorkFailed);
	}
}

int CoinEng::GetIntervalForModDivision(int height) {
	return ChainParams.TargetInterval;
}

int CoinEng::GetIntervalForCalculatingTarget(int height) {
	return ChainParams.TargetInterval;
}

Target CoinEng::KimotoGravityWell(const BlockHeader& headerLast, const Block& block, int minBlocks, int maxBlocks) {
	if (headerLast.Height < minBlocks)
		return ChainParams.MaxTarget;
	int actualSeconds = 0, targetSeconds = 0;
	BigInteger average;
	BlockHeader b = headerLast;
	for (int mass=1; b.Height>0 && (maxBlocks<=0 || mass<=maxBlocks); ++mass, b=b.GetPrevHeader()) {
		average += (BigInteger(b.get_DifficultyTarget()) - average)/mass;
		actualSeconds = max(0, (int)duration_cast<seconds>(headerLast.get_Timestamp() - b.get_Timestamp()).count());
		targetSeconds = (int)duration_cast<seconds>(ChainParams.BlockSpan * (double)mass).count();
		if (mass >= minBlocks) {
			double eventHorizonDeviation = 1 + 0.7084 * pow(mass / 28.2, -1.228);
			double ratio = actualSeconds && targetSeconds ? double(targetSeconds) / actualSeconds :	1;
			if (ratio<=1/eventHorizonDeviation || ratio>=eventHorizonDeviation)
				break;
		}
	}
	return Target(actualSeconds && targetSeconds ? (average * actualSeconds / targetSeconds) : average);
}

TimeSpan CoinEng::GetActualSpanForCalculatingTarget(const BlockObj& bo, int nInterval) {
	int r = bo.Height - nInterval + 1;
	if (bo.Height >= ChainParams.AuxPowStartBlock || (ChainParams.FullPeriod && r>0))
		--r;
	BlockHeader b = Tree.FindHeader(bo.PrevBlockHash);
	ASSERT(b.Height >= r);
	while (b.Height > r)
		b = b.IsInMainChain() ? Db->FindHeader(r) : b.GetPrevHeader();
	return bo.Timestamp - b.Timestamp;
}

TimeSpan CoinEng::AdjustSpan(int height, const TimeSpan& span, const TimeSpan& targetSpan) {
	return clamp(span, targetSpan/4, targetSpan*4);
}

static DateTime s_dt15Feb2012(2012, 2, 15);
static Target s_minTestnetTarget(0x1D0FFFFF);

Target CoinEng::GetNextTargetRequired(const BlockHeader& headerLast, const Block& block) {
	int nIntervalForMod = GetIntervalForModDivision(headerLast.Height);

	if ((headerLast.Height + 1) % nIntervalForMod) {
		if (ChainParams.IsTestNet) {
			if (block.get_Timestamp()-headerLast.Timestamp > ChainParams.BlockSpan*2)			// negative block timestamp delta allowed
				return ChainParams.MaxTarget;
			else {
				for (BlockHeader prev(headerLast);; prev = prev.GetPrevHeader())
					if (prev.Height % nIntervalForMod == 0 || prev.get_DifficultyTarget() != ChainParams.MaxTarget)
						return prev.DifficultyTarget;
			}
		}
		return headerLast.DifficultyTarget;
	}
	int nInterval = GetIntervalForCalculatingTarget(headerLast.Height);
	TimeSpan targetSpan = ChainParams.BlockSpan * nInterval,
		span = AdjustSpan(headerLast.Height, GetActualSpanForCalculatingTarget(*headerLast.m_pimpl, nInterval), targetSpan);

#ifdef X_DEBUG//!!!D
	{
		TimeSpan targetSpan1 = eng.ChainParams.BlockSpan * nInterval;
		TimeSpan actualSpan1 = (Timestamp - eng.GetBlockByHeight(Height-720).Timestamp);
		TimeSpan span1 = std::min(std::max(actualSpan1, targetSpan1/4), targetSpan1*4);
		double dAs0 = duration_cast<seconds>(eng.GetActualSpanForCalculatingTarget(_self, nInterval)).count();
		double dAs = actualSpan1.TotalSeconds,
			dSpan = span1.TotalSeconds,
			dTarget    = targetSpan1.TotalSeconds;
		Target t = std::min(Target(BigInteger(get_DifficultyTarget())*(int)span1.TotalSeconds/((int)targetSpan1.TotalSeconds)), ChainParams.MaxTarget);		//!!!

		TRC(1, "Alternative                  " << hex << BigInteger(t));
		Sleep(100);

		nInterval = nInterval;
	}
	String shex = EXT_STR(hex << BigInteger(ChainParams.MaxTarget));
	int len = strlen(shex);
#endif

	return Target(BigInteger(headerLast.get_DifficultyTarget()) * span.count() / targetSpan.count());
}

Target CoinEng::GetNextTarget(const BlockHeader& headerLast, const Block& block) {
	return std::min(ChainParams.MaxTarget, GetNextTargetRequired(headerLast, block));
}

static const TimeSpan s_spanFuture = seconds(MAX_FUTURE_SECONDS);

void BlockObj::CheckHeader() {
	CoinEng& eng = Eng();
	if (Height != -1) {
		if (Height >= eng.ChainParams.AuxPowStartBlock) {
			if ((Ver >> 16) != eng.ChainParams.ChainId)
				Throw(CoinErr::BlockDoesNotHaveOurChainId);
			if (AuxPow) {
				AuxPow->Check(Block(this));
				AuxPow->ParentBlock.CheckPow(DifficultyTarget);
			}
		} else if (AuxPow)
			Throw(CoinErr::AUXPOW_NotAllowed);
	}
	if (!AuxPow && ProofType()==ProofOf::Work) {
		if (eng.ChainParams.HashAlgo == HashAlgo::Sha256 || eng.ChainParams.HashAlgo == HashAlgo::Prime)
			CheckPow(DifficultyTarget);
		else if (Hash() != eng.ChainParams.Genesis) {
			HashValue hash = PowHash();
			uint8_t ar[33];
			memcpy(ar, hash.data(), 32);
			ar[32] = 0;
			if (BigInteger(ar, sizeof ar) >= BigInteger(get_DifficultyTarget())) {
				TRC(2, "E_COIN_ProofOfWorkFailed in block " << GetHash());
				Throw(CoinErr::ProofOfWorkFailed);
			}
		}
	}

	if (Timestamp > Clock::now() + s_spanFuture)
		Throw(CoinErr::BlockTimestampInTheFuture);
}

void BlockObj::Check(bool bCheckMerkleRoot) {
	CheckHeader();
	if (IsHeaderOnly())
		return;

	CoinEng& eng = Eng();
	if (eng.Mode != EngMode::Lite && (m_txes.empty() || m_txes.size() > MAX_BLOCK_SIZE))
		Throw(CoinErr::SizeLimits);

	if (eng.Mode != EngMode::Lite && eng.Mode != EngMode::BlockParser) {
		if (!m_txes[0].IsCoinBase())
			Throw(CoinErr::FirstTxIsNotCoinbase);
		for (int i = 1; i < m_txes.size(); ++i)
			if (m_txes[i].IsCoinBase())
				Throw(CoinErr::FirstTxIsNotTheOnlyCoinbase);
		EXT_FOR (const Tx& tx, m_txes) {
			tx.Check();
		}
		if (bCheckMerkleRoot && MerkleRoot(false) != m_merkleRoot.value())
			Throw(CoinErr::MerkleRootMismatch);
	}
	CheckSignature();
}

void Block::Check(bool bCheckMerkleRoot) const {
	if (Eng().Mode != EngMode::Lite)
		get_Txes();
	m_pimpl->Check(bCheckMerkleRoot);
}

void BlockObj::CheckCoinbaseTx(int64_t nFees) {
	CoinEng& eng = Eng();

	if (eng.Mode != EngMode::Lite && eng.Mode != EngMode::BlockParser) {
		int64_t valueOut = m_txes[0].ValueOut;
		int64_t subsidy = eng.GetSubsidy(Height, PrevBlockHash, eng.ToDifficulty(DifficultyTarget), true);
		if (valueOut > subsidy + nFees) {
			Throw(CoinErr::SubsidyIsVeryBig);
		}
	}
}

int BlockObj::GetBlockHeightFromCoinbase() const {
	Span cbuf = get_Txes()[0].TxIns()[0].Script();
	if (cbuf[0] > 4)
		return -1;
	CMemReadStream stm(cbuf);
	BigInteger bi;
	BinaryReader(stm) >> bi;
	return explicit_cast<int>(bi);
}

uint32_t Block::OffsetOfTx(const HashValue& hashTx) const {
	MemoryStream ms;
	ProtocolWriter wr(ms);
	m_pimpl->WriteHeader(wr);
	const CTxes& txes = m_pimpl->get_Txes();
	CoinSerialized::WriteVarInt(wr, txes.size());
	for (size_t i=0; i<txes.size(); ++i) {
		if (Hash(txes[i]) == hashTx)
			return uint32_t(ms.Position);
		txes[i].Write(wr);
	}
	Throw(E_FAIL);
}

uint32_t Block::GetSerializeSize(bool bWitnessAware) const {
    MemoryStream ms(MAX_BLOCK_SIZE);    //!!!TODO Use NullStream here
    ProtocolWriter wr(ms);
    wr.WitnessAware = bWitnessAware;
    Write(wr);
    return (uint32_t)(ms.Position);
}

void Block::LoadToMemory() {							//!!! Really don't loads TxIn Scripts
	EXT_FOR (const Tx& tx, get_Txes()) {
		tx.TxIns();
		EXT_FOR (const TxOut& txOut, tx.TxOuts()) {
			txOut.get_ScriptPubKey();
		}
	}
}

Block Block::GetOrphanRoot() const {					// ASSERT(eng.Caches.Mtx is locked)
	CoinEng& eng = Eng();
	const Block *r = this;
	for (ChainCaches::COrphanMap::iterator it; (it=eng.Caches.OrphanBlocks.find(r->PrevBlockHash))!=eng.Caches.OrphanBlocks.end();)
		r = &it->second.first;
	return *r;
}

TxFeeTuple RunConnectAsync(const CConnectJob *pjob, TxObj *to, bool bVerifySignature) {
	TxFeeTuple r;
	const CConnectJob& job = *pjob;
	CCoinEngThreadKeeper engKeeper(&job.Eng);
	t_features = job.Features;
	try {
		const Tx tx(to);
		r.Tx = tx;

		int64_t nValueIn = 0;
		if ((job.aSigOps += tx.SigOpCost(job.TxMap)) > MAX_BLOCK_SIGOPS_COST)
			Throw(CoinErr::TxTooManySigOps);
		SignatureHasher sigHasher(*tx.m_pimpl);
		if (bVerifySignature && tx.m_pimpl->HasWitness())
			sigHasher.CalcWitnessCache();

		auto& txIns = tx.TxIns();
		for (int nIn = 0; nIn < txIns.size(); ++nIn) {
			if (job.Failed)
				break;

			const TxIn& txIn = txIns[nIn];
			const OutPoint& op = txIn.PrevOutPoint;
			const Tx& txPrev = job.TxMap.Get(op.TxHash);
			const TxOut& txOut = txPrev.TxOuts()[op.Index];

			if (txPrev.IsCoinBase())
				job.Eng.CheckCoinbasedTxPrev(job.Height, txPrev);

			if (bVerifySignature) { // Skip ECDSA signature verification when connecting blocks (fBlock=true) during initial download
				sigHasher.m_bWitness = false;
				sigHasher.NIn = nIn;
				sigHasher.m_amount = txOut.Value;
				sigHasher.HashType = SigHashType::ZERO;
				sigHasher.VerifySignature(txPrev);
			}

			job.Eng.CheckMoneyRange(nValueIn += txOut.Value);
		}
		tx.CheckInOutValue(nValueIn, r.Fee, job.Eng.AllowFreeTxes ? 0 : tx.GetMinFee(1, false), job.DifficultyTarget);
	} catch (RCExc) {
		job.Failed = true;
	}
	return r;
}

int64_t RunConnectTask(const CConnectJob *pjob, const ConnectTask *ptask) {
	CoinEng& eng = pjob->Eng;
	CCoinEngThreadKeeper engKeeper(&eng);
	int64_t fee = 0;
	bool bVerifySignature = eng.BestBlockHeight() > eng.ChainParams.LastCheckpointHeight - INITIAL_BLOCK_THRESHOLD;
	EXT_FOR(const Tx& ptx, ptask->Txes) {
		fee += RunConnectAsync(pjob, ptx.m_pimpl.get(), bVerifySignature).Fee;
		if (pjob->Failed)
			break;
	}
	return fee;
}

static void RecoverPubKey(CConnectJob* connJob, const OutPoint* pop, TxObj* pTxObj, int nIn) {
	CCoinEngThreadKeeper engKeeper(&connJob->Eng);
	const OutPoint& op = *pop;

	const TxOut *txOut = &connJob->TxMap.GetOutputFor(op);
	Span pkScript = txOut->get_ScriptPubKey();
	TxIn& txIn = pTxObj->m_txIns[nIn];
	Span scriptSig = txIn.Script();
	Span pk = FindStandardHashAllSigPubKey(scriptSig);
	if (pk.data()) {
		Span sig = scriptSig.subspan(1, pk.data() - scriptSig.data() - 2);
		try {
			Sec256Signature sigObj;
			sigObj.AssignCompact(Sec256Signature(sig).ToCompact());
			Blob ser = sigObj.Serialize();
			if (ser == sig) {
				if (!pk.size())
					txIn.RecoverPubKeyType = 8;
				else {
					SignatureHasher hasher(*pTxObj);
					hasher.NIn = nIn;
					hasher.HashType = SigHashType::SIGHASH_ALL;
					HashValue hashSig = hasher.Hash(pkScript);
#ifdef X_DEBUG//!!!D
					static int s_i;
					s_i++;
					if (s_i >= 1) {
						if (!memcmp(pk.P, "\x41\x04\x06\xF3", 4)) {
							cout << "hashSig: " << hashSig << endl;
							cout << "pkScript: " << pkScript << endl;
							cout << "Pk: " << pk << endl;
							cout << "Sig: " << sig << endl;
							cout << "Recover: " << Sec256Dsa::RecoverPubKey(hashSig, sigObj, 1, false) << endl;
							s_i = s_i;
						}
						s_i = s_i;
					}
#endif
					bool bCompressed = pk.size() < 35;
					Span hsig = hashSig.ToSpan(),
						pubKeyData = Span(pk.data() + 1, pk.size() -1);
					for (uint8_t recid=0; recid<3; ++recid) {
						if (Equal(Sec256Dsa::RecoverPubKey(hsig, sigObj, recid, bCompressed), pubKeyData)) {
							txIn.RecoverPubKeyType = uint8_t(0x48 | (int(bCompressed) << 2) | recid);
							return;
						}
					}
					ASSERT(0);
				}
			}
		} catch (CryptoException&) {
		}
	}
}

/*!!!O
void CConnectJob::AsynchCheckAll(const vector<Tx>& txes) {
	vector<future<void>> futsTxOut;
	EXT_FOR (const Tx& tx, txes) {
		HashValue hashTx = Hash(tx);
		if (!tx.IsCoinBase()) {
			if (tx.IsCoinStake())
				tx.m_pimpl->GetCoinAge();		// cache CoinAge, which can't be calculated in Pooled Thread
			ptr<ConnectTask> task(new ConnectTask);
			auto& txIns = tx.TxIns();
			for (size_t nIn = 0; nIn < txIns.size(); ++nIn) {
				const OutPoint& op = txIns[nIn].PrevOutPoint;
				const HashValue& hashPrev = op.TxHash;
				CHashToTask::iterator it = HashToTask.find(hashPrev);
				if (it != HashToTask.end()) {
					ptr<ConnectTask> taskPrev = it->second;							// can't be const ref, following code replaces it
					if (taskPrev != task) {
						EXT_FOR (const Tx& txPrev, taskPrev->Txes) {
							task->Txes.push_back(txPrev);
							HashToTask[Hash(txPrev)] = task;
						}
						Tasks.erase(taskPrev);
					}
				} else if (!TxMap.count(hashPrev)) {
					TxMap.insert(make_pair(hashPrev, Tx::FromDb(hashPrev)));
				}
				if (Eng.Mode != EngMode::Bootstrap) {
					const TxOut *pTxOut = &TxMap.at(hashPrev).TxOuts().at(op.Index);
#if UCFG_COIN_PKSCRIPT_FUTURES
					futsTxOut.push_back(std::async(RecoverPubKey, &Eng, pTxOut, tx.m_pimpl.get(), nIn));
#else
					RecoverPubKey(&Eng, pTxOut, tx.m_pimpl.get(), nIn);
#endif
				}
			}
			task->Txes.push_back(tx);
			Tasks.insert(task);
			HashToTask.insert(make_pair(hashTx, task));
		}
		TxMap.insert(make_pair(hashTx, tx));
	}
	EXT_FOR (future<void>& fut, futsTxOut) {
		fut.wait();
	}

#if UCFG_COIN_TX_CONNECT_FUTURES
	vector<future<int64_t>> futures;
	futures.reserve(Tasks.size());
	EXT_FOR (const ptr<ConnectTask>& task, Tasks) {
		futures.push_back(std::async(RunConnectTask, this, task.get()));
	}
	for (int i = 0; i < futures.size(); ++i)
		Fee = Eng.CheckMoneyRange(Fee + futures[i].get());
#else
	EXT_FOR(const ptr<ConnectTask>& task, Tasks) {
		Fee = Eng.CheckMoneyRange(Fee + RunConnectTask(this, task.get()));
	}
#endif
}
*/

void CConnectJob::AsynchCheckAllSharedFutures(const vector<Tx>& txes) {
	bool bVerifySignature = Eng.BestBlockHeight() > Eng.ChainParams.LastCheckpointHeight - INITIAL_BLOCK_THRESHOLD;

	vector<shared_future<TxFeeTuple>> futsTx;
	futsTx.reserve(txes.size());
	vector<future<void>> futsTxOut;
	EXT_FOR(const Tx& tx, txes) {
		HashValue hashTx = Hash(tx);
		if (tx.IsCoinBase()) {
			TxMap.Add(tx);
		} else {
			if (tx.IsCoinStake())
				tx.m_pimpl->GetCoinAge();		// cache CoinAge, which can't be calculated in Pooled Thread
			auto& txIns = tx.TxIns();
			for (size_t nIn = 0; nIn < txIns.size(); ++nIn) {
				const OutPoint& op = txIns[nIn].PrevOutPoint;
				const HashValue& hashPrev = op.TxHash;
				if (!TxMap.Contains(hashPrev))
					TxMap.Add(Tx::FromDb(hashPrev));		//!!!TODO: make async
				if (Eng.Mode != EngMode::Bootstrap) {
					auto launchType = UCFG_COIN_PKSCRIPT_FUTURES ? launch::async : launch::deferred;
					futsTxOut.push_back(std::async(launchType, RecoverPubKey, this, &op, tx.m_pimpl.get(), nIn));
				}
			}

			auto launchType = UCFG_COIN_TX_CONNECT_FUTURES ? launch::async : launch::deferred;
			shared_future<TxFeeTuple> ft = std::async(launchType, RunConnectAsync, this, tx.m_pimpl.get(), bVerifySignature);
			futsTx.push_back(ft);
			TxMap.Add(hashTx, ft);
		}
	}
	EXT_FOR(future<void>& fut, futsTxOut) {
		fut.wait();
	}

	Fee = 0;
	for (auto ft : futsTx)
		Fee = Eng.CheckMoneyRange(Fee + ft.get().Fee);
}

void CConnectJob::PrepareTask(const HashValue160& hash160, const CanonicalPubKey& pubKey) {
	if (!pubKey.IsValid())
		return;
	Blob compressed;
	try {
		DBG_LOCAL_IGNORE_CONDITION(ExtErr::Crypto);
		compressed = pubKey.ToCompressed();
	} catch (RCExc) {
		return;
	}
	CMap::iterator it = Map.find(hash160);
	if (it != Map.end()) {
		PubKeyData& pkd = it->second;
		if (!!pkd.PubKey) {
			if (pkd.PubKey.Size == 20) {
				pkd.Update = !pkd.Insert;
				pkd.PubKey = compressed;

				ASSERT(CanonicalPubKey::FromCompressed(compressed).get_Hash160() == hash160);
			} else {
				pkd.IsTask = false;
				if (pkd.PubKey != compressed)
					pkd.PubKey = Blob(nullptr);
			}
		}
	} else {
		int64_t id = CIdPk(hash160);
		PubKeyData pkd;
		pkd.PubKey = compressed;
		Blob pk(nullptr);
		EXT_LOCK (Eng.Caches.Mtx) {
			ChainCaches::CCachePkIdToPubKey::iterator j = Eng.Caches.m_cachePkIdToPubKey.find(id);
			if (j != Eng.Caches.m_cachePkIdToPubKey.end()) {
				PubKeyHash160& pkh = j->second.first;
				if (pkh.Hash160 != hash160)
					return;
				ASSERT(pkh.PubKey.Data.empty() || Equal(pkh.PubKey.Data, pubKey.Data));
				if (pkd.Update = pkh.PubKey.Data.empty())
					pkh.PubKey = pubKey;
				goto LAB_SAVE;
			}
		}
		if (!!(pk = Eng.Db->FindPubkey(id))) {
			if (pkd.Update = (pk.Size == 20)) {
				if (HashValue160(pk) != hash160)
					return;
			} else if (pk != compressed)
				return;
		} else if (!InsertedIds.insert(id).second)
			return;
		else
			pkd.Insert = true;
LAB_SAVE:
		Map.insert(make_pair(hash160, pkd));
	}
}

void CConnectJob::Prepare(const Block& block) {
	if (Eng.Mode == EngMode::Lite || Eng.Mode == EngMode::BlockParser)
		return;

	EXT_FOR (const Tx& tx, block.get_Txes()) {
		EXT_FOR (const TxOut& txOut, tx.TxOuts()) {
			Span span160, spanPk;
			HashValue160 hash160;
			Blob pk;
			switch (uint8_t typ = TryParseDestination(txOut.get_ScriptPubKey(), span160, spanPk)) {
			case 20:
				pk = Blob(spanPk);
				hash160 = HashValue160(span160);
				if (!Map.count(hash160)) {
					PubKeyData pkd;
					int64_t id = CIdPk(hash160);
					if (!!(pk = Eng.Db->FindPubkey(id))) {
						pkd.PubKey = (pkd.IsTask = pk.Size != 20) || HashValue160(pk) == hash160 ? pk : Blob(nullptr);
						Map.insert(make_pair(hash160, pkd));
					} else {
						if (InsertedIds.insert(id).second) {
							pkd.Insert = true;
							pkd.PubKey = hash160;
							Map.insert(make_pair(hash160, pkd));
						}
					}
				}
				break;
			case 33:
			case 65:
				pk = Blob(spanPk);
				hash160 = HashValue160(span160);
				PrepareTask(hash160, CanonicalPubKey(pk));
				break;
			}
		}
	}
}

typedef pair<const HashValue160, CConnectJob::PubKeyData> PubKeyTask;

static void CalcPubkeyHash(PubKeyTask *pr) {
	if (DbPubKeyToHashValue160(pr->second.PubKey).Hash160 != pr->first)
		pr->second.PubKey = Blob(nullptr);
}

void CConnectJob::Calculate() {
#if UCFG_COIN_USE_FUTURES
	vector<future<void>> futures;
	for (CMap::iterator it=Map.begin(), e=Map.end(); it!=e; ++it) {
		if (it->second.IsTask)
			futures.push_back(std::async(CalcPubkeyHash, &*it));   // by pointer, because async params transferred by value
	}
	EXT_FOR (future<void>& fut, futures) {
		fut.wait();
	}
#else
	for (CMap::iterator it=Map.begin(), e=Map.end(); it!=e; ++it) {
		if (it->second.IsTask)
			CalcPubkeyHash(&*it);
	}
#endif
}

void BlockHeader::Connect() const {
	CoinEng& eng = Eng();
	HashValue hashBlock = Hash(_self);
#if UCFG_TRC
	if (!(Height & 0x1FF)) {
		TRC(4, "                \t" << Height << "\t" << hashBlock);
	}
#endif

	{
		CoinEngTransactionScope scopeBlockSavepoint(eng);
		eng.Db->InsertHeader(_self);
	}
	eng.SetBestHeader(_self);
	eng.Tree.RemovePersistentBlock(hashBlock);
	UpdateLastCheckpointed();
}

const uint8_t *Block::GetWitnessCommitment() const {
    auto& txes = get_Txes();
    const uint8_t *r = nullptr;
    if (txes.empty())
        return r;
    for (auto& txOut : txes[0].TxOuts()) {
        static const uint8_t commitmentSignature[6] = { (uint8_t)Opcode::OP_RETURN, 0x24, 0xAA, 0x21, 0xA9, 0xED };
        Span pkScript = txOut.ScriptPubKey;
        r = pkScript.size() >= 38 && !memcmp(pkScript.data(), commitmentSignature, sizeof(commitmentSignature)) ? pkScript.data() : r;
    }
    return r;
}

void Block::ContextualCheck(const BlockHeader& blockPrev) {
    CoinEng& eng = Eng();
    auto height = Height;
	if (height >= eng.ChainParams.BIP34Height) {
		if (Ver >= 2 && height != m_pimpl->GetBlockHeightFromCoinbase())
			Throw(CoinErr::BlockHeightMismatchInCoinbase);
	}

    auto mtp = blockPrev.GetMedianTimePast();
    if (Timestamp <= mtp)
        Throw(CoinErr::TooEarlyTimestamp);

    auto lockTimeCutoff = height >= eng.ChainParams.BIP68Height ? mtp : Timestamp;
	CTxes& txes = m_pimpl->m_txes;
    for (auto& tx : txes) {
        tx.Height = height;
        if (!tx.IsFinal(height, lockTimeCutoff))
            Throw(CoinErr::ContainsNonFinalTx);
    }

    bool bHaveWitness = false;
    if (height >= eng.ChainParams.SegwitHeight) {
        if (auto witnessCommitment = GetWitnessCommitment()) {
            bHaveWitness = true;
            auto& coinbaseWitness = txes[0].TxIns()[0].Witness;
            if (coinbaseWitness.size() != 1 || coinbaseWitness[0].Size != 32)
                Throw(CoinErr::BadWitnessNonceSize);
            HashValue ar[2] = { CalcWitnessTxHashes(*m_pimpl).back(), HashValue(coinbaseWitness[0].data()) };
            auto hashWitness = Hash(Span((const uint8_t*)ar, sizeof ar));
            if (hashWitness != HashValue(witnessCommitment + 6))
                Throw(CoinErr::BadWitnessMerkleMatch);
        }
    }
    if (!bHaveWitness)
        for (auto& tx : txes)
            if (tx.m_pimpl->HasWitness())
                Throw(CoinErr::UnexpectedWitness);

    if (Weight() > MAX_BLOCK_WEIGHT)
        Throw(CoinErr::BadBlockWeight);
}

void Block::Connect() const {
	CoinEng& eng = Eng();
	HashValue hashBlock = Hash(_self);
	TRC(3, Height << " " << hashBlock <<  (eng.Mode == EngMode::Lite ? String() : EXT_STR(" " << setw(4) << get_Txes().size() << " Txes")));

	Check(false);		// Check Again without bCheckMerkleRoot
	int64_t nFees = 0;

    int height = Height;
    uint32_t minVersion =
        height >= eng.ChainParams.BIP65Height ? 4
        : height >= eng.ChainParams.BIP66Height ? 3
        : height >= eng.ChainParams.BIP34Height ? 2
        : 0;
    if (Ver < minVersion)
        Throw(CoinErr::BadBlockVersion);

    EnabledFeatures& f = t_features;
    f.PayToScriptHash = height >= eng.ChainParams.PayToScriptHashHeight;
	f.CheckLocktimeVerify = height >= eng.ChainParams.BIP65Height;
	f.VerifyDerEnc = height >= eng.ChainParams.BIP66Height;
	f.CheckSequenceVerify = height >= eng.ChainParams.BIP68Height;
	f.SegWit = height >= eng.ChainParams.SegwitHeight;

	CConnectJob job(eng);
	job.DifficultyTarget = DifficultyTarget;

	switch (eng.Mode) {
	case EngMode::Normal:
	case EngMode::BlockExplorer:
	case EngMode::Bootstrap:
		job.Features = t_features;
		job.Height = Height;

		job.AsynchCheckAllSharedFutures(Txes);
		if (job.Failed)
			Throw(CoinErr::ProofOfWorkFailed);
		nFees = job.Fee;
	}

	CBoolKeeper keepDisabledPkCache(eng.Caches.PubkeyCacheEnabled, false);
	{
		eng.EnsureTransactionStarted();
		CoinEngTransactionScope scopeBlockSavepoint(eng);

		switch (eng.Mode) {
		case EngMode::Normal:
		case EngMode::BlockExplorer:
			job.Prepare(_self);
		case EngMode::Bootstrap:
			job.Calculate();
		}

		for (CConnectJob::CMap::iterator it = job.Map.begin(), e = job.Map.end(); it != e; ++it) {
			if (it->second.Insert) {
				eng.Db->InsertPubkey(int64_t(CIdPk(it->first)), it->second.PubKey);
			} else if (it->second.Update)
				eng.Db->UpdatePubkey(int64_t(CIdPk(it->first)), it->second.PubKey);
		}

		EXT_LOCK (eng.Caches.Mtx) {
			for (CConnectJob::CMap::iterator it=job.Map.begin(), e=job.Map.end(); it!=e; ++it) {				//!!!? necessary to sync cache
				if (it->second.Update) {
					int64_t id = CIdPk(it->first);
					eng.Caches.m_cachePkIdToPubKey.erase(id);													// faster than call CanonicalPubKey::FromCompressed()
/*!!!R 				ChainCaches::CCachePkIdToPubKey::iterator j = eng.Caches.m_cachePkIdToPubKey.find(id);
					if (j != eng.Caches.m_cachePkIdToPubKey.end())
						j->second.first.PubKey = CanonicalPubKey::FromCompressed(it->second.PubKey);					*/
				}
			}
		}

		if (eng.Mode != EngMode::Lite) {
			EXT_FOR (const Tx& tx, get_Txes()) {
				if (!tx.IsCoinBase()) {
					vector<Tx> vTxPrev;
					if (eng.Mode != EngMode::BlockParser) {
						const vector<TxIn>& txIns = tx.TxIns();
						vTxPrev.reserve(txIns.size());
						for (auto& txIn : txIns)
							vTxPrev.push_back(job.TxMap.Get(txIn.PrevOutPoint.TxHash));
					}
					eng.OnConnectInputs(tx, vTxPrev, true, false);
				}
			}
		}

		if (Height > 0)
			m_pimpl->CheckCoinbaseTx(nFees);

		eng.Db->InsertBlock(_self, job);

		eng.OnConnectBlock(_self);
		eng.Caches.DtBestReceived = Clock::now();

		EXT_LOCK (eng.Caches.Mtx) {
			eng.Caches.HeightToHashCache.insert(make_pair((ChainCaches::CHeightToHashCache::key_type)Height, hashBlock));
		}

		eng.Events.OnBlockConnectDbtx(_self);
	}
	eng.SetBestBlock(_self);
	eng.Tree.RemovePersistentBlock(Hash(_self));

	Inventory inv(eng.Mode == EngMode::Lite || eng.Mode == EngMode::BlockParser ? InventoryType::MSG_FILTERED_BLOCK : InventoryType::MSG_BLOCK, hashBlock);

	EXT_LOCK(eng.MtxPeers) {
		for (CoinEng::CLinks::iterator it=begin(eng.Links); it!=end(eng.Links); ++it) {
			Link& link = static_cast<Link&>(**it);
			if (Height > link.LastReceivedBlock-2000)
				link.Push(inv);
		}
	}

	EXT_LOCK(eng.Caches.Mtx) {
		eng.Caches.HeightToHashCache.insert(make_pair((ChainCaches::CHeightToHashCache::key_type)Height, hashBlock));
		eng.Caches.HashToBlockCache.insert(make_pair(hashBlock, _self));
	}

	EXT_FOR (const Tx& tx, get_Txes()) {
		eng.Events.OnProcessTx(tx);
	}

	if (eng.Mode != EngMode::Lite && eng.Mode != EngMode::BlockParser) {
		EXT_FOR(const Tx& tx, get_Txes()) {
			eng.TxPool.Remove(tx);
		}
	}

	UpdateLastCheckpointed();

#ifdef X_DEBUG//!!!D
	eng.Db->Checkpoint();
#endif
}

// BIP113
DateTime BlockHeader::GetMedianTimePast() const {
	CoinEng& eng = Eng();

	vector<DateTime> ar(eng.ChainParams.MedianTimeSpan);
	vector<DateTime>::iterator beg = ar.end(), end = beg;
	BlockHeader block = _self;
	int height = Height;
	for (int i = height, e = std::max(0, int(height - ar.size() + 1)); block && i >= e; --i, block = eng.Tree.FindHeader(block.PrevBlockHash))
		*--beg = block.Timestamp;
	sort(beg, end);
	return *(beg + (end - beg) / 2);
}

BlockHeader BlockHeader::GetPrevHeader() const {
	if (BlockHeader r = Eng().Tree.FindHeader(PrevBlockHash))
		return r;
	Throw(CoinErr::OrphanedChain);
}

bool BlockHeader::IsSuperMajority(int minVersion, int nRequired, int nToCheck) const {
	int n = 0;
	BlockHeader block = _self;
	for (int i=0; i<nToCheck && n<nRequired; ++i, block = block.GetPrevHeader()) {
		n += int(block.Ver >= minVersion);
		if (block.Height == 0)
			break;
	}
	return n >= nRequired;
}

void BlockHeader::CheckAgainstCheckpoint() const {
	if (!g_conf.Checkpoints)
		return;
	CoinEng& eng = Eng();
	int h = Height;
	if (h > eng.Tree.HeightLastCheckpointed) {
		for (size_t i = eng.ChainParams.Checkpoints.size(); i--;) {
			const pair<int, HashValue>& cp = eng.ChainParams.Checkpoints[i];
			if (cp.first == h) {
				if (Hash(_self) == cp.second)
					return;
				goto LAB_THROW;
			}
		}
		return;
	}
LAB_THROW:
	TRC(3, "Checkpoint failed for block: " << h << " " << Hash(_self));
	Throw(CoinErr::RejectedByCheckpoint);
}

void BlockHeader::UpdateLastCheckpointed() const {
	CoinEng& eng = Eng();
	auto& checkpoints = eng.ChainParams.Checkpoints;
	if (checkpoints.empty() || Height > checkpoints.back().first)
		return;
	HashValue hash = Hash(_self);
	for (auto& cp : checkpoints) {
		if (cp.second == hash) {
			eng.Tree.HeightLastCheckpointed = cp.first;
			break;
		}
	}
}

bool BlockHeader::HasBestChainWork() const {
	CoinEng& eng = Eng();

	BlockHeader bestHeader = eng.BestHeader();
	if (!bestHeader)
		return true;
	BigInteger work = 0, forkWork = 0;

	BlockTreeItem btiRoot(_self);
	HashValue hash = Hash(_self);
	while (!eng.Db->HaveHeader(hash)) {
		hash = btiRoot.PrevBlockHash;
		forkWork += eng.ChainParams.MaxTarget / exchange(btiRoot, eng.Tree.GetHeader(btiRoot.PrevBlockHash)).DifficultyTarget;
	}

	HashValue hashB = Hash(bestHeader);
	for (BlockTreeItem bti=bestHeader; hashB!=hash && forkWork > work;) {
		hashB = bti.PrevBlockHash;
		work += eng.ChainParams.MaxTarget / exchange(bti, eng.Tree.GetHeader(bti.PrevBlockHash)).DifficultyTarget;
	}

	return forkWork > work;
}

void BlockHeader::Accept() {
	CoinEng& eng = Eng();

	HashValue hash = Hash(_self);
	if (eng.Tree.FindHeader(hash))
		return;
	m_pimpl->CheckHeader();
	if (!PrevBlockHash)
		m_pimpl->Height = 0;
	else if (BlockTreeItem btiPrev = eng.Tree.FindHeader(PrevBlockHash))
		m_pimpl->Height = btiPrev.Height + 1;
	else {
		TRC(4, "BadPrevBlock: " <<  PrevBlockHash);
		Throw(CoinErr::BadPrevBlock);
	}
	CheckAgainstCheckpoint();

	EXT_LOCK(eng.Mtx) {
		if (!eng.Runned)
			Throw(ExtErr::ThreadInterrupted);
		if (Height <= eng.BestHeaderHeight() && eng.Tree.FindHeader(hash))
			return;																	// RaceCondition check, header already accepted by parallel thread.
		if (!HasBestChainWork()) {
			TRC(2, "Fork detected: " << Height << " " << hash);
			eng.Tree.Add(_self);
		} else {
			if (eng.BestBlock() && Hash(eng.BestHeader()) != get_PrevBlockHash())
				eng.Reorganize(_self);
			else {
				Connect();
			}
		}
//!!!?


	}
}

void Block::Accept() {
	CoinEng& eng = Eng();

	if (!PrevBlockHash)
		m_pimpl->Height = 0;
	else {
		BlockHeader blockPrev = eng.Tree.FindHeader(PrevBlockHash);

		Target targetNext = eng.GetNextTarget(blockPrev, _self);
		if (get_DifficultyTarget() != targetNext) {
			TRC(1, "Should be                  " << hex << BigInteger(eng.GetNextTarget(blockPrev, _self)));
			TRC(1, "CurBlock DifficultyTarget: " << hex << BigInteger(get_DifficultyTarget()));
			TRC(1, "Should be                  " << hex << BigInteger(eng.GetNextTarget(blockPrev, _self)));
#ifdef _DEBUG//!!!D
 			eng.GetNextTarget(blockPrev, _self);
#endif
			Throw(CoinErr::IncorrectProofOfWork);
		}

        m_pimpl->Height = blockPrev.Height + 1;
        ContextualCheck(blockPrev);
	}

	HashValue hash = Hash(_self);
	m_pimpl->ComputeAttributes();

	EXT_LOCK (eng.Mtx) {
		if (!eng.Runned)
			Throw(ExtErr::ThreadInterrupted);
		if (Height <= eng.BestBlockHeight() && eng.HaveBlock(hash))
			return;		// RaceCondition check, block already accepted by parallel thread.
		bool bConnectToMainChain = IsInMainChain();
		if (!bConnectToMainChain) {
			CheckAgainstCheckpoint();
			if (BlockTreeItem bti = eng.Tree.FindInMap(hash)) {
				eng.Tree.Add(_self);
				return;
			} else if (!(bConnectToMainChain = HasBestChainWork())) {
				TRC(2, "Fork detected: " << Height << " " << hash);
				eng.Tree.Add(_self);
				return;
			}
		}
		if (eng.BestBlock() && Hash(eng.BestBlock()) != get_PrevBlockHash())
			eng.Reorganize(_self);
		else
			Connect();

		bool bNotifyWallet = !eng.Events.Subscribers.empty() && !eng.IsInitialBlockDownload();
		if (bNotifyWallet || !(Height % eng.CommitPeriod))
			eng.CommitTransactionIfStarted();
		if (bNotifyWallet)
			eng.Events.OnProcessBlock(_self);
	}

	eng.Events.OnBlockchainChanged();
}

void Block::Process(Link *link, bool bRequested) {
	CoinEng& eng = Eng();
	HashValue hash = Hash(_self);

	bool bHave = eng.HaveBlock(hash);

	TRC(8, (bHave ? "Have " : "") << hash << "  Prev: " << PrevBlockHash);		//!!!D was 8

	if (bHave)
		return;

	try {
		DBG_LOCAL_IGNORE_CONDITION(CoinErr::BlockNotFound);

		Check(true);
	} catch (Exception& ex) {
		if (ex.code() == CoinErr::BlockNotFound
			|| ex.code() == CoinErr::TxNotFound)					// possible in PoS blocks
			return;
		throw;
	}
	if (eng.HaveAllBlocksUntil(PrevBlockHash) || hash == eng.ChainParams.Genesis) {
		Accept();

		vector<HashValue> vec;
		vec.push_back(hash);
		for (int i = 0; i < vec.size(); ++i) {
			HashValue h = vec[i];
LAB_AGAIN:
			if (!eng.Runned)
				Throw(ExtErr::ThreadInterrupted);
			Block blk(nullptr);
			EXT_LOCK (eng.Caches.Mtx) {
				for (ChainCaches::COrphanMap::iterator it=eng.Caches.OrphanBlocks.begin(); it!=eng.Caches.OrphanBlocks.end(); ++it) {
					Block& blockNext = it->second.first;
					if (blockNext.PrevBlockHash == h) {
						blk = blockNext;
						eng.Caches.OrphanBlocks.erase(it);
						goto LAB_FOUND_ORPHAN;
					}
				}
			}
		LAB_FOUND_ORPHAN:
			if (blk) {
				try {
					blk.Accept();
					vec.push_back(Hash(blk));
				} catch (RCExc) {
					goto LAB_AGAIN;
				}
			} else {
				vector<Block> nexts = eng.Tree.FindNextBlocks(h);
				EXT_FOR(Block& blk, nexts) {
					try {
						blk.Accept();
						vec.push_back(Hash(blk));
					} catch (RCExc) {
					}
				}
			}
		}
	} else if (bRequested) {
		if (BlockTreeItem bti = eng.Tree.FindHeader(hash)) {
			m_pimpl->Height = bti.Height;
			eng.Tree.Add(_self);
			TRC(4, "requested    \t" << bti.Height << "\t" << hash << " added to Tree");
		} else {
			EXT_LOCK(eng.Caches.Mtx) {
				eng.Caches.OrphanBlocks.insert(make_pair(hash, _self));

				TRC(4, "B \t" << hash << " added to OrphanBlocks.size= " << eng.Caches.OrphanBlocks.size());
			}
		}
	} else {
		TRC(3, "B " << "Not-requested Orphan " << hash << " discarded");
	}
}


} // Coin::
