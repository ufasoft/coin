/*######   Copyright (c) 2011-2015 Ufasoft  http://ufasoft.com  mailto:support@ufasoft.com,  Sergey Pavlov  mailto:dev@ufasoft.com ####
#                                                                                                                                     #
# 		See LICENSE for licensing information                                                                                         #
#####################################################################################################################################*/

#include <el/ext.h>

#include EXT_HEADER_FUTURE

#include <el/crypto/hash.h>

#include "coin-protocol.h"
#include "script.h"
#include "eng.h"

#if UCFG_COIN_ECC=='S'
#	include <crypto/cryp/secp256k1.h>
#else
#	define OPENSSL_NO_SCTP
#	include <openssl/err.h>
#	include <openssl/ec.h>
#endif

namespace Coin {

CTxes::CTxes(const CTxes& txes)												// Cloning ctor
	:	base(txes.size())
{
	for (int i=0; i<size(); ++i)
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
	pair<int, int> r(-1, -1);
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
	hons.reserve(size_t(rd.BaseStream.Length / (32+1)));				// heuristic upper bound
	while  (!rd.BaseStream.Eof()) {
		TxHashOutNum hon;
		hon.NOuts = (uint32_t)(rd >> hon.HashTx).Read7BitEncoded();
		hons.push_back(hon);
	}
	return rd;
}

TxHashesOutNums::TxHashesOutNums(const ConstBuf& buf) {
	reserve(buf.Size / (32+1));					// heuristic upper bound
	CMemReadStream stm(buf);
	BinaryReader rd(stm);
	while (!rd.BaseStream.Eof()) {
		TxHashOutNum hon;
		stm.ReadBuffer(hon.HashTx.data(), 32);
		hon.NOuts = (uint32_t)rd.Read7BitEncoded();
		push_back(hon);
	}
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
	const byte *p = blob.constData()+size;
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


BlockObj::~BlockObj() {
}

const CTxes& BlockObj::get_Txes() const {
	if (!m_bTxesLoaded) {
		if (m_txHashesOutNums.empty())
			Throw(E_FAIL);
		EXT_LOCK (m_mtx) {
			if (!m_bTxesLoaded) {
				m_txes.resize(m_txHashesOutNums.size());
				for (int i=0; i<m_txHashesOutNums.size(); ++i) {
					Tx& tx = (m_txes[i] = Tx::FromDb(m_txHashesOutNums[i].HashTx));
					if (tx.Height != Height)								// protection against duplicated txes
						(tx = Tx(tx.m_pimpl->Clone())).m_pimpl->Height = Height;
				}
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
	return block->HashFromTx(block->get_Txes()[n], n);
}
#endif

struct CTxHasher {
	const BlockObj *m_block;
#if UCFG_COIN_MERKLE_FUTURES
	vector<future<HashValue>> *m_pfutures;
#endif

	HashValue operator()(const Tx& tx, int n) {
#if UCFG_COIN_MERKLE_FUTURES
		return (*m_pfutures)[n].get();
#else
		return m_block->HashFromTx(tx, n);
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
	for (int i=0; i<txes.size(); ++i)
		futures.push_back(std::async(&Coin::HashFromTx, &eng, &block, i));
#endif
	return BuildMerkleTree<Coin::HashValue>(txes, h1, &HashValue::Combine);
}

HashValue BlockObj::MerkleRoot(bool bSave) const {
	if (!bSave)
		return (m_merkleTree = CalcTxHashes(_self)).empty() ? Coin::HashValue::Null() : m_merkleTree.back();
	if (!m_merkleRoot) {
		if (m_txHashesOutNums.empty())
			get_Txes();						// to eliminate dead lock
		EXT_LOCK (m_mtx) {
			if (!m_merkleRoot) {
				m_merkleTree = m_txHashesOutNums.empty() ? CalcTxHashes(_self) : BuildMerkleTree<Coin::HashValue>(m_txHashesOutNums, &TxHashOf, &HashValue::Combine);
				m_merkleRoot = m_merkleTree.empty() ? Coin::HashValue::Null() : m_merkleTree.back();
			}
		}
	}
	return m_merkleRoot.get();
}

HashValue BlockObj::Hash() const {
	if (!m_hash) {
		Blob hdata = EXT_BIN(Ver << PrevBlockHash << MerkleRoot() << (uint32_t)to_time_t(Timestamp) << get_DifficultyTarget() << Nonce);
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
	return m_hash.get();
}

HashValue BlockObj::PowHash() const {
	return Eng().ChainParams.HashAlgo == HashAlgo::SCrypt
		? ScryptHash(EXT_BIN(Ver << PrevBlockHash << MerkleRoot() << (uint32_t)to_time_t(Timestamp) << get_DifficultyTarget() << Nonce))
		: Hash();
}

Block::Block() {
	m_pimpl = Eng().CreateBlockObj();
}

Block Block::GetPrevBlock() const {
	if (Block r = Eng().LookupBlock(PrevBlockHash))
		return r;
	Throw(CoinErr::OrphanedChain);
}

void BlockObj::WriteHeader(BinaryWriter& wr) const {
	base::WriteHeader(wr);
	CoinEng& eng = Eng();
	if (eng.ChainParams.AuxPowEnabled && (Ver & BLOCK_VERSION_AUXPOW)) {
		AuxPow->Write(wr);
	}
}

void BlockObj::ReadHeader(const BinaryReader& rd, bool bParent, const HashValue *pMerkleRoot) {
	CoinEng& eng = Eng();

	Ver = rd.ReadUInt32();

	if (!eng.ChainParams.IsTestNet && !bParent) {
		uint16_t ver = (Ver & 0xFF);
		if (ver > 112)															//!!!?
			Throw(ExtErr::Protocol_Violation);
		else if (ver < 1 || ver > eng.MaxBlockVersion)
			Throw(ExtErr::NewProtocolVersion);

//!!! can be checked only the Height known		if (!bParent && ChainId != eng.ChainParams.ChainId)
//			Throw(CoinErr::BlockDoesNotHaveOurChainId);
	}

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

void BlockObj::Write(BinaryWriter& wr) const {
	WriteHeader(wr);
	CoinSerialized::Write(wr, get_Txes());
	WriteSuffix(wr);
}

void BlockObj::Read(const BinaryReader& rd) {
#ifdef X_DEBUG//!!!D
	int64_t pos = rd.BaseStream.Position;
#endif

	ReadHeader(rd, false, 0);
	CoinSerialized::Read(rd, m_txes);
	m_bTxesLoaded = true;
	for (int i=0; i<m_txes.size(); ++i) {
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
	if (eng.Mode==EngMode::Lite || eng.Mode==EngMode::BlockParser)
		wr << MerkleRoot();
	if (eng.Mode!=EngMode::BlockParser) {
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
	if (eng.Mode==EngMode::Lite || eng.Mode==EngMode::BlockParser) {
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
		byte ar[33];
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

Target CoinEng::KimotoGravityWell(const Block& blockLast, const Block& block, int minBlocks, int maxBlocks) {
	if (blockLast.Height < minBlocks)
		return ChainParams.MaxTarget;
	int actualSeconds = 0, targetSeconds = 0;
	BigInteger average;
	Block b = blockLast;
	for (int mass=1; b.Height>0 && (maxBlocks<=0 || mass<=maxBlocks); ++mass, b=b.GetPrevBlock()) {
		average += (BigInteger(b.get_DifficultyTarget()) - average)/mass;
		actualSeconds = max(0, (int)duration_cast<seconds>(blockLast.get_Timestamp() - b.get_Timestamp()).count());
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
	Block b = LookupBlock(bo.PrevBlockHash);
	ASSERT(b.Height >= r);
	while (b.Height > r)
		b = b.IsInMainChain() ? GetBlockByHeight(r) : b.GetPrevBlock();
	return bo.Timestamp - b.Timestamp;
}

TimeSpan CoinEng::AdjustSpan(int height, const TimeSpan& span, const TimeSpan& targetSpan) {
	return clamp(span, targetSpan/4, targetSpan*4);	
}

static DateTime s_dt15Feb2012(2012, 2, 15);
static Target s_minTestnetTarget(0x1D0FFFFF);

Target CoinEng::GetNextTargetRequired(const Block& blockLast, const Block& block) {
	int nIntervalForMod = GetIntervalForModDivision(blockLast.Height);

	if ((blockLast.Height+1) % nIntervalForMod) {
		if (ChainParams.IsTestNet) {
			if (block.get_Timestamp()-blockLast.Timestamp > ChainParams.BlockSpan*2)			// negative block timestamp delta allowed
				return ChainParams.MaxTarget;
			else {
				for (Block prev(blockLast);; prev = prev.GetPrevBlock())
					if (prev.Height % nIntervalForMod == 0 || prev.get_DifficultyTarget() != ChainParams.MaxTarget)
						return prev.DifficultyTarget;
			}
		}
		return blockLast.DifficultyTarget;
	}
	int nInterval = GetIntervalForCalculatingTarget(blockLast.Height);
	TimeSpan targetSpan = ChainParams.BlockSpan * nInterval,
		span = AdjustSpan(blockLast.Height, GetActualSpanForCalculatingTarget(*blockLast.m_pimpl, nInterval), targetSpan);

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

	return Target(BigInteger(blockLast.get_DifficultyTarget()) * span.count() / targetSpan.count());
}

Target CoinEng::GetNextTarget(const Block& blockLast, const Block& block) {
	return std::min(ChainParams.MaxTarget, GetNextTargetRequired(blockLast, block));
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
			byte ar[33];
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
	if (eng.Mode!=EngMode::Lite && (m_txes.empty() || m_txes.size() > MAX_BLOCK_SIZE))
		Throw(CoinErr::SizeLimits);

	if (eng.Mode!=EngMode::Lite && eng.Mode!=EngMode::BlockParser) {
		if (!m_txes[0].IsCoinBase())
			Throw(CoinErr::FirstTxIsNotCoinbase);
		for (int i=1; i<m_txes.size(); ++i)
			if (m_txes[i].IsCoinBase())
				Throw(CoinErr::FirstTxIsNotTheOnlyCoinbase);	
		EXT_FOR (const Tx& tx, m_txes) {
			tx.Check();
		}
		if (bCheckMerkleRoot && MerkleRoot(false) != m_merkleRoot.get())
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

	if (eng.Mode!=EngMode::Lite && eng.Mode!=EngMode::BlockParser) {
		int64_t valueOut = m_txes[0].ValueOut;
		int64_t subsidy = eng.GetSubsidy(Height, PrevBlockHash, eng.ToDifficulty(DifficultyTarget), true);
		if (valueOut > subsidy+nFees) {
			Throw(CoinErr::SubsidyIsVeryBig);
		}
	}
}

int BlockObj::GetBlockHeightFromCoinbase() const {
	ConstBuf cbuf = get_Txes()[0].TxIns()[0].Script();
	if (cbuf.P[0] > 4)
		return -1;
	CMemReadStream stm(cbuf);
	BigInteger bi;
	BinaryReader(stm) >> bi;
	return explicit_cast<int>(bi);
}

uint32_t Block::OffsetOfTx(const HashValue& hashTx) const {
	MemoryStream ms;
	BinaryWriter wr(ms);
	m_pimpl->WriteHeader(wr);
	const CTxes& txes = m_pimpl->get_Txes();	
	CoinSerialized::WriteVarInt(wr, txes.size());
	for (size_t i=0; i<txes.size(); ++i) {
		if (Hash(txes[i]) == hashTx)
			return uint32_t(ms.Position);
		wr << txes[i];
	}
	Throw(E_FAIL);
}

void Block::LoadToMemory() {							//!!! Really don't loads TxIn Scripts
	EXT_FOR (const Tx& tx, get_Txes()) {
		tx.TxIns();
		EXT_FOR (const TxOut& txOut, tx.TxOuts()) {
			txOut.get_PkScript();
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

int64_t RunConnectTask(const CConnectJob *pjob, const ConnectTask *ptask) {
	const CConnectJob& job = *pjob;
	const ConnectTask& task = *ptask;
	CCoinEngThreadKeeper engKeeper(&job.Eng);
	t_bPayToScriptHash = (void*)(bool)job.PayToScriptHash;
	int64_t fee = 0;
	try {
		EXT_FOR (const Tx& ptx, task.Txes) {
			int64_t nValueIn = 0;
			const Tx& tx = ptx;
			if (job.aSigOps+=tx.SigOpCount > MAX_BLOCK_SIGOPS)
				Throw(CoinErr::TxTooManySigOps);
			for (int nIn=0; nIn<tx.TxIns().size(); ++nIn) {
				if (job.Failed)
					return fee;

				const TxIn& txIn = tx.TxIns()[nIn];
				const OutPoint& op = txIn.PrevOutPoint;
				const Tx& txPrev = job.TxMap.at(op.TxHash);

				const TxOut& txOut = txPrev.TxOuts()[op.Index];
				if (t_bPayToScriptHash &&  IsPayToScriptHash(txOut.PkScript)) {
					if (job.aSigOps+=CalcSigOpCount(txOut.PkScript, txIn.Script()) > MAX_BLOCK_SIGOPS)
						Throw(CoinErr::TxTooManySigOps);
				}

				if (txPrev.IsCoinBase())
					job.Eng.CheckCoinbasedTxPrev(job.Height, txPrev);

				if (job.Eng.BestBlockHeight() > job.Eng.ChainParams.LastCheckpointHeight-INITIAL_BLOCK_THRESHOLD)	// Skip ECDSA signature verification when connecting blocks (fBlock=true) during initial download
					VerifySignature(txPrev, tx, nIn);

				job.Eng.CheckMoneyRange(nValueIn += txOut.Value);
			}
			tx.CheckInOutValue(nValueIn, fee, job.Eng.AllowFreeTxes ? 0 : tx.GetMinFee(1, false), job.DifficultyTarget); 
		}
	} catch (RCExc) {
		job.Failed = true;
	}
	return fee;
}

static void RecoverPubKey(CoinEng *eng, const TxOut *txOut, TxObj* pTxObj, int nIn) {
	CCoinEngThreadKeeper engKeeper(eng);
	const Blob& pkScript = txOut->get_PkScript();
	TxIn& txIn = pTxObj->m_txIns[nIn];
	ConstBuf pk = FindStandardHashAllSigPubKey(txIn.Script());
	if (pk.P) {
		ConstBuf sig(txIn.Script().constData()+1, pk.P-txIn.Script().constData()-2);
		try {
			Sec256Signature sigObj;
			sigObj.AssignCompact(Sec256Signature(sig).ToCompact());
			Blob ser = sigObj.Serialize();
			if (ser == sig) {
				if (!pk.Size)
					txIn.RecoverPubKeyType = 8;
				else {
					HashValue hashSig = SignatureHash(pkScript, *pTxObj, nIn, 1);
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
					bool bCompressed = pk.Size<35;
					ConstBuf hsig = hashSig.ToConstBuf(),
						pubKeyData = ConstBuf(pk.P+1, pk.Size-1);
					for (byte recid=0; recid<3; ++recid) {
						if (Sec256Dsa::RecoverPubKey(hsig, sigObj, recid, bCompressed) == pubKeyData) {
							txIn.RecoverPubKeyType = byte(0x48 | (int(bCompressed) << 2) | recid);
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

void CConnectJob::AsynchCheckAll(const vector<Tx>& txes) {
	vector<future<void>> futsTxOut;
	EXT_FOR (const Tx& tx, txes) {
		HashValue hashTx = Hash(tx);
		if (!tx.IsCoinBase()) {
			if (tx.IsCoinStake())
				tx.m_pimpl->GetCoinAge();		// cache CoinAge, which can't be calculated in Pooled Thread
			ptr<ConnectTask> task(new ConnectTask);
			for (size_t nIn=0; nIn<tx.TxIns().size(); ++nIn) {
				const OutPoint& op = tx.TxIns()[nIn].PrevOutPoint;
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
#if UCFG_COIN_PKSCRIPT_FUTURES
					futsTxOut.push_back(std::async(RecoverPubKey, &Eng, &TxMap.at(hashPrev).TxOuts().at(op.Index), tx.m_pimpl.get(), nIn));
#else
					RecoverPubKey(&Eng, &TxMap.at(hashPrev).TxOuts().at(op.Index), tx.m_pimpl.get(), nIn);
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
	for (int i=0; i<futures.size(); ++i)
		Fee = Eng.CheckMoneyRange(Fee + futures[i].get());
#else
	EXT_FOR(const ptr<ConnectTask>& task, Tasks) {
		Fee = Eng.CheckMoneyRange(Fee + RunConnectTask(this, task.get()));
	}
#endif
}

void CConnectJob::PrepareTask(const HashValue160& hash160, const CanonicalPubKey& pubKey) {
	if (!pubKey.IsValid())
		return;
	Blob compressed;
	try {
		DBG_LOCAL_IGNORE_CONDITION(ExtErr::Crypto);
//!!!R		DBG_LOCAL_IGNORE(MAKE_HRESULT(SEVERITY_ERROR, FACILITY_OPENSSL, EC_R_INVALID_ENCODING));
//!!!R		DBG_LOCAL_IGNORE(MAKE_HRESULT(SEVERITY_ERROR, FACILITY_OPENSSL, EC_R_POINT_IS_NOT_ON_CURVE));
//!!!R		DBG_LOCAL_IGNORE(MAKE_HRESULT(SEVERITY_ERROR, FACILITY_OPENSSL, ERR_R_EC_LIB));
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
				ASSERT(!pkh.PubKey.Data || pkh.PubKey.Data==pubKey.Data);
				if (pkd.Update = !pkh.PubKey.Data)
					pkh.PubKey = pubKey;
				goto LAB_SAVE;
			}
		}
		if (!!(pk = Eng.Db->FindPubkey(id))) {
			if (pkd.Update = (pk.Size==20)) {
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
	EXT_FOR (const Tx& tx, block.get_Txes()) {
		if (Eng.ShouldBeSaved(tx)) {
			EXT_FOR (const TxOut& txOut, tx.TxOuts()) {
				HashValue160 hash160;
				Blob pk;
				switch (byte typ = txOut.TryParseDestination(hash160, pk)) {
				case 20:
					if (!Map.count(hash160)) {
						PubKeyData pkd;
						int64_t id = CIdPk(hash160);
						if (!!(pk = Eng.Db->FindPubkey(id))) {
							pkd.PubKey = (pkd.IsTask = pk.Size != 20) || HashValue160(pk)==hash160 ? pk : Blob(nullptr);
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
					PrepareTask(hash160, CanonicalPubKey(pk));
					break;
				}
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

void Block::Connect() const {
	CoinEng& eng = Eng();
	TRC(3, "           Height: " << Height << "   " << Hash(_self) <<  (eng.Mode==EngMode::Lite ? String() : EXT_STR(" " << setw(4) << get_Txes().size() << " Txes")));

	Check(false);		// Check Again without bCheckMerkleRoot
	int64_t nFees = 0;

	t_bPayToScriptHash = (void*)(Height >= eng.ChainParams.PayToScriptHashHeight);

	CConnectJob job(eng);
	job.DifficultyTarget = DifficultyTarget;

	switch (eng.Mode) {
	case EngMode::Normal:
	case EngMode::Bootstrap:
	case EngMode::BlockExplorer:
		job.PayToScriptHash = (void*)t_bPayToScriptHash;
		job.Height = Height;

		job.AsynchCheckAll(Txes);
		if (job.Failed)
			Throw(CoinErr::ProofOfWorkFailed);
		nFees = job.Fee;

		if (eng.Mode != EngMode::Bootstrap)
			job.Prepare(_self);
		job.Calculate();
	}

	CBoolKeeper keepDisabledPkCache(eng.Caches.PubkeyCacheEnabled, false);
	{
		eng.EnsureTransactionStarted();
		CoinEngTransactionScope scopeBlockSavepoint(eng);

		for (CConnectJob::CMap::iterator it=job.Map.begin(), e=job.Map.end(); it!=e; ++it) {
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
						for (size_t i=0; i<txIns.size(); ++i)
							vTxPrev.push_back(job.TxMap.at(txIns[i].PrevOutPoint.TxHash));
					}
					eng.OnConnectInputs(tx, vTxPrev, true, false);
				}
			}
		}

		if (Height > 0)
			m_pimpl->CheckCoinbaseTx(nFees);

		MemoryStream ms;
		DbWriter wr(ms);
		wr.ConnectJob.reset(&job);
		m_pimpl->Write(wr);
//!!!?			wr << byte(0);

		HashValue hashBlock = Hash(_self);
		Coin::TxHashesOutNums txHashOutNums;

		switch (eng.Mode) {
		case EngMode::Bootstrap:
			{
				vector<uint32_t> txOffsets = eng.Db->InsertBlock(_self, ms, ConstBuf());
				for (size_t i=0; i<m_pimpl->m_txes.size(); ++i) {
					const Tx& tx = m_pimpl->m_txes[i];
					Coin::HashValue txHash = Coin::Hash(tx);
					uint32_t offset = htole(txOffsets[i]);
					eng.Db->InsertTx(tx, txHashOutNums, txHash, Height, ConstBuf(), CoinEng::SpendVectorToBlob(vector<bool>(tx.TxOuts().size(), true)), ConstBuf(&offset, 3));
					tx.SpendInputs();
				}
			}
			break;
		default:
			for (size_t i=0; i<m_pimpl->m_txes.size(); ++i) {
				if (!eng.Runned)
					Throw(ExtErr::ThreadInterrupted);
				const Tx& tx = m_pimpl->m_txes[i];
				if (eng.ShouldBeSaved(tx)) {
					Coin::HashValue txHash = Coin::Hash(tx);
	#ifdef X_DEBUG//!!!D
					ofsapp << "  " << i << "\t" << txHash << "\n";
	#endif

					MemoryStream msTx;
					DbWriter dw(msTx);
					dw.ConnectJob.reset(&job);
					dw << tx;
					MemoryStream msTxIns;
					DbWriter wrIns(msTxIns);
					wrIns.PCurBlock.reset(this);
					wrIns.ConnectJob.reset(&job);
					wrIns.PTxHashesOutNums.reset(&txHashOutNums);
					tx.WriteTxIns(wrIns);
					eng.Db->InsertTx(tx, txHashOutNums, txHash, Height, msTxIns, CoinEng::SpendVectorToBlob(vector<bool>(tx.TxOuts().size(), true)), msTx);

					TxHashOutNum hom = { txHash, (uint32_t)tx.TxOuts().size() };
					txHashOutNums.push_back(hom);

					tx.SpendInputs();
				}

	//				tx.SaveCoins();
			}

			eng.Db->InsertBlock(_self, ms, EXT_BIN(txHashOutNums));
		}
		eng.OnConnectBlock(_self);
		eng.Caches.DtBestReceived = Clock::now();

		EXT_LOCK (eng.Caches.Mtx) {
			eng.Caches.HeightToHashCache.insert(make_pair((ChainCaches::CHeightToHashCache::key_type)Height, hashBlock));
		}


#ifdef X_DEBUG//!!!D
	static FileStream g_fs("c:\\work\\coin\\Bitcoin-testnet2.dump", FileMode::Create, FileAccess::Write);
	BinaryWriter(g_fs) << uint32_t(eng.ChainParams.ProtocolMagic) << uint32_t(m_pimpl->Binary.Size);
	g_fs.WriteBuf(m_pimpl->Binary);
	g_fs.Flush();
#endif


		if (eng.m_iiEngEvents)
			eng.m_iiEngEvents->OnBlockConnectDbtx(_self);
	}
	eng.SetBestBlock(_self);

	if (eng.m_iiEngEvents) {
		EXT_FOR (const Tx& tx, get_Txes()) {
			eng.m_iiEngEvents->OnProcessTx(tx);
		}
	}
}

DateTime Block::GetMedianTimePast() {
	CoinEng& eng = Eng();

	vector<DateTime> ar(eng.ChainParams.MedianTimeSpan);
	vector<DateTime>::iterator beg = ar.end(), end = beg;
	Block block = _self;
	for (int i=Height; block && i>=std::max(0, int(Height - ar.size()+1)); --i, block=eng.LookupBlock(block.PrevBlockHash))
		*--beg = block.Timestamp;
	sort(beg, end);
	return *(beg + (end-beg)/2);
}

bool Block::HasBestChainWork() const {
	CoinEng& eng = Eng();

	if (!eng.BestBlock())
		return true;
	BigInteger work = 0, forkWork = 0;
	Block blockRoot;
	for (blockRoot=_self; !blockRoot.IsInMainChain();)
		forkWork += exchange(blockRoot, blockRoot.GetPrevBlock()).GetWork();
	for (Block b=eng.BestBlock(); Hash(b)!=Hash(blockRoot) && forkWork>work;)
		work += exchange(b, b.GetPrevBlock()).GetWork();
	return forkWork > work;
}

void Block::Accept() {
	CoinEng& eng = Eng();

	if (!PrevBlockHash)
		m_pimpl->Height = 0;	
	else {
		Block blockPrev = eng.LookupBlock(PrevBlockHash);
		
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

		if (Timestamp <= blockPrev.GetMedianTimePast())
			Throw(CoinErr::TooEarlyTimestamp);

		m_pimpl->Height = blockPrev.Height+1;
	}

	EXT_FOR (Tx& tx, m_pimpl->m_txes) {
		tx.Height = Height;
		if (!tx.IsFinal(Height, Timestamp))
			Throw(CoinErr::ContainsNonFinalTx);
	}

	HashValue hash = Hash(_self);
	m_pimpl->ComputeAttributes();

	ChainParams::CCheckpoints::iterator itCheckPoint = eng.ChainParams.Checkpoints.find(Height);
	if (itCheckPoint != eng.ChainParams.Checkpoints.end() && itCheckPoint->second != hash) {
		TRC(3, "Checkpoint failed for block: " << hash << "  H: " << Height);
		Throw(CoinErr::RejectedByCheckpoint);
	}

//!!!T	if (Ver >= 2 && IsSuperMajority(2, 750, 1000) && Height != m_pimpl->GetBlockHeightFromCoinbase())				// very expensive check
//		Throw(CoinErr::BlockHeightMismatchInCoinbase);

	EXT_LOCK (eng.Mtx) {
		if (!eng.Runned)
			Throw(ExtErr::ThreadInterrupted);
		if (Height <= eng.BestBlockHeight() && eng.HaveBlock(hash))
			return;		// RaceCondition check, block already accepted by parallel thread.
		if (!HasBestChainWork()) {
			TRC(2, "Fork detected: " << hash << "  Height: " << Height);
			EXT_LOCKED(eng.Caches.Mtx, eng.Caches.HashToAlternativeChain.insert(make_pair(hash, _self)));
		} else {
			if (eng.BestBlock() && Hash(eng.BestBlock()) != get_PrevBlockHash())
				Reorganize();
			else {
				Connect();
				EXT_LOCK (eng.Caches.Mtx) {
					eng.Caches.HeightToHashCache.insert(make_pair((ChainCaches::CHeightToHashCache::key_type)Height, hash));
					eng.Caches.HashToBlockCache.insert(make_pair(hash, _self));
#ifdef X_DEBUG//!!!D
		TRC(1, "Before LogObjectCounters");
		LogObjectCounters();
#endif
				}

				if (eng.Mode!=EngMode::Lite && eng.Mode!=EngMode::BlockParser) {
					EXT_FOR (const Tx& tx, get_Txes()) {
						eng.RemoveFromMemory(tx);
					}
				}
			}
		
			Inventory inv(eng.Mode==EngMode::Lite || eng.Mode==EngMode::BlockParser ? InventoryType::MSG_FILTERED_BLOCK : InventoryType::MSG_BLOCK, hash);

			EXT_LOCK (eng.MtxPeers) {
				for (CoinEng::CLinks::iterator it=begin(eng.Links); it!=end(eng.Links); ++it) {
					CoinLink& link = static_cast<CoinLink&>(**it);
					if (Height > link.LastReceivedBlock-2000)
						link.Push(inv);	
				}
			}

			bool bNotifyWallet = eng.m_iiEngEvents && !eng.IsInitialBlockDownload();
			if (bNotifyWallet || !(Height % eng.CommitPeriod))
				eng.CommitTransactionIfStarted();
			if (bNotifyWallet)
				eng.m_iiEngEvents->OnProcessBlock(_self);
		}
	}
	eng.Tree.RemovePersistentBlock(hash);

	if (eng.m_iiEngEvents)
		eng.m_iiEngEvents->OnBlockchainChanged();
}

void Block::Process(P2P::Link *link) {
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
	if (hash==eng.ChainParams.Genesis || eng.HaveBlockInMainTree(PrevBlockHash)) {
		Accept();

		vector<HashValue> vec;
		vec.push_back(hash);
		for (int i=0; i<vec.size(); ++i) {
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
			}
		}
	} else {
		Block orphanRoot(nullptr);
		EXT_LOCK (eng.Caches.Mtx) {
			eng.Caches.OrphanBlocks.insert(make_pair(hash, _self));
			if (link)
				orphanRoot = GetOrphanRoot();
		}
//!!!R		if (orphanRoot)
//!!!R			link->Send(new GetBlocksMessage(eng.BestBlock(), Hash(orphanRoot)));

		TRC(4, "B " << eng.Tree.Find(hash).Height << " " << hash << " added to OrphanBlocks.size()= " << EXT_LOCKED(eng.Caches.Mtx, eng.Caches.OrphanBlocks.size()));
	}
}

bool Block::IsInMainChain() const {
	return Eng().Db->HaveBlock(Hash(_self));
}

bool Block::IsSuperMajority(int minVersion, int nRequired, int nToCheck) {
	int n = 0;
	Block block = _self;
	for (int i=0; i<nToCheck && n<nRequired; ++i, block = block.GetPrevBlock()) {
		n += int(block.Ver >= minVersion);
		if (block.Height == 0)
			break;
	}
	return n >= nRequired;
}


} // Coin::

