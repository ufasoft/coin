/*######     Copyright (c) 1997-2014 Ufasoft  http://ufasoft.com  mailto:support@ufasoft.com,  Sergey Pavlov  mailto:dev@ufasoft.com #######################################
#                                                                                                                                                                          #
# This program is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation;  #
# either version 3, or (at your option) any later version. This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the      #
# implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details. You should have received a copy of the GNU #
# General Public License along with this program; If not, see <http://www.gnu.org/licenses/>                                                                               #
##########################################################################################################################################################################*/

#include <el/ext.h>

#include EXT_HEADER_FUTURE

#include <el/crypto/hash.h>

#define OPENSSL_NO_SCTP
#include <openssl/err.h>
#include <openssl/ec.h>

#include "coin-protocol.h"
#include "script.h"
#include "eng.h"
#include "coin-msg.h"

namespace Coin {

HashValue160 Hash160(const ConstBuf& mb) {
	return HashValue160(RIPEMD160().ComputeHash(SHA256().ComputeHash(mb)));
}

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
	Throw(E_COIN_InconsistentDatabase);
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
	Throw(E_COIN_InconsistentDatabase);
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
		hon.NOuts = (UInt32)(rd >> hon.HashTx).Read7BitEncoded();
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
		hon.NOuts = (UInt32)rd.Read7BitEncoded();
		push_back(hon);
	}
}

Target::Target(UInt32 v) {
	Int32 m = Int32(v << 8) >> 8;
//	if (m < 0x8000)			// namecoin violates spec
//		Throw(E_INVALIDARG);	
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
	return BigInteger(Int32(m_value << 8) >> 8) << 8*((m_value >> 24)-3);
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

HashValue Identity(const HashValue& hash) {
	return hash;
}

HashValue TxHashOf(const TxHashOutNum& hon, int n) {
	return hon.HashTx;
}

HashValue BlockObj::HashFromTx(const Tx& tx, int n) const {
	return Coin::Hash(tx);
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
	if (bSave) {
		if (!m_merkleRoot) {
			if (m_txHashesOutNums.empty())
				get_Txes();						// to eliminate dead lock
			EXT_LOCK (m_mtx) {
				if (!m_merkleRoot) {
					m_merkleTree = m_txHashesOutNums.empty() ? CalcTxHashes(_self) : BuildMerkleTree<Coin::HashValue>(m_txHashesOutNums, &TxHashOf, &HashValue::Combine);
					m_merkleRoot = m_merkleTree.empty() ? Coin::HashValue() : m_merkleTree.back();
				}
			}
		}
		return m_merkleRoot.get();
	} else
		return (m_merkleTree = CalcTxHashes(_self)).empty() ? Coin::HashValue() : m_merkleTree.back();
}

HashValue BlockObj::Hash() const {
	if (!m_hash) {
		Blob hdata = EXT_BIN(Ver << PrevBlockHash << MerkleRoot() << (UInt32)to_time_t(Timestamp) << get_DifficultyTarget() << Nonce);
		switch (Eng().ChainParams.HashAlgo) {
		case HashAlgo::Sha3:
			m_hash = HashValue(SHA3<256>().ComputeHash(hdata));
			break;
		case HashAlgo::Metis:
			m_hash = MetisHash(hdata);
			break;
		default:
			m_hash = Coin::Hash(hdata);
		}
	}
	return m_hash.get();
}

HashValue BlockObj::PowHash(CoinEng& eng) const {
	return eng.ChainParams.HashAlgo == HashAlgo::SCrypt
		? ScryptHash(EXT_BIN(Ver << PrevBlockHash << MerkleRoot() << (UInt32)to_time_t(Timestamp) << get_DifficultyTarget() << Nonce))
		: Hash();
}

Block::Block() {
	m_pimpl = Eng().CreateBlockObj();
}

Block Block::GetPrevBlock() const {
	if (Block r = Eng().LookupBlock(PrevBlockHash))
		return r;
	Throw(E_COIN_OrphanedChain);
}

void BlockObj::WriteHeader(BinaryWriter& wr) const {
	base::WriteHeader(wr);
	CoinEng& eng = Eng();
	if (eng.ChainParams.AuxPowEnabled && (Ver & BLOCK_VERSION_AUXPOW)) {
		AuxPow->Write(wr);
	}
}

void BlockObj::ReadHeader(const BinaryReader& rd, bool bParent) {
	CoinEng& eng = Eng();

	Ver = rd.ReadUInt32();

	if (!eng.ChainParams.IsTestNet) {
		UInt16 ver = (Ver & 0xFF);
		if (ver > 112)															//!!!?
			Throw(E_EXT_Protocol_Violation);
		else if (ver < 1 || ver > eng.MaxBlockVersion)
			Throw(E_EXT_New_Protocol_Version);

//!!! can be checked only the Height known		if (!bParent && ChainId != eng.ChainParams.ChainId)
//			Throw(E_COIN_BlockDoesNotHaveOurChainId);
	}

	HashValue merkleRoot;
	rd >> PrevBlockHash >> merkleRoot;
	m_merkleRoot = merkleRoot;
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
}

void BlockObj::Read(const BinaryReader& rd) {
#ifdef X_DEBUG//!!!D
	Int64 pos = rd.BaseStream.Position;
#endif

	ReadHeader(rd, false);
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
	Int64 len = rd.BaseStream.Position-pos;
	rd.BaseStream.Position = pos;
	Blob br(0, (size_t)len);
	rd.BaseStream.ReadBuffer(br.data(), (size_t)len);
	Binary = br;
#endif
}

void BlockObj::Write(DbWriter& wr) const {
	CoinEng& eng = Eng();

	CoinSerialized::WriteVarInt(wr, Ver);
	wr << (UInt32)to_time_t(Timestamp) << get_DifficultyTarget() << Nonce;
	if (eng.ChainParams.AuxPowEnabled && (Ver & BLOCK_VERSION_AUXPOW))
		AuxPow->Write(wr);
}

void BlockObj::Read(const DbReader& rd) {
	CoinEng& eng = Eng();

	Ver = (UInt32)CoinSerialized::ReadVarInt(rd);
	Timestamp = DateTime::from_time_t(rd.ReadUInt32());
	DifficultyTargetBits = rd.ReadUInt32();
	Nonce = rd.ReadUInt32();
	m_merkleRoot.reset();
	if (eng.ChainParams.AuxPowEnabled && (Ver & BLOCK_VERSION_AUXPOW))
		(AuxPow = new Coin::AuxPow)->Read(rd);
}

BigInteger BlockObj::GetWork() const {
	return Eng().ChainParams.MaxTarget / get_DifficultyTarget();
}

void BlockObj::CheckPow(const Target& target) {
	if (ProofType() == ProofOf::Work) {
		Hash();
		byte ar[33];
		memcpy(ar, m_hash.get().data(), 32);
		ar[32] = 0;
		if (BigInteger(ar, sizeof ar) >= BigInteger(target))
			Throw(E_COIN_ProofOfWorkFailed);
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
		actualSeconds = max(0, int((blockLast.get_Timestamp() - b.get_Timestamp()).get_TotalSeconds()));
		targetSeconds = int((ChainParams.BlockSpan * mass).get_TotalSeconds());
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
	return bo.Timestamp - GetBlockByHeight(r).Timestamp;					//!!! should use curchain instead of main chain, but will fail only for forks longer than TargetInterval
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
		double dAs0 = eng.GetActualSpanForCalculatingTarget(_self, nInterval).TotalSeconds;
		double dAs = actualSpan1.TotalSeconds,
			dSpan = span1.TotalSeconds,
			dTarget    = targetSpan1.TotalSeconds;
		Target t = std::min(Target(BigInteger(get_DifficultyTarget())*(int)span1.TotalSeconds/((int)targetSpan1.TotalSeconds)), ChainParams.MaxTarget);		//!!!

		TRC(1, "Alternative                  " << hex << BigInteger(t));
		Sleep(100);

		nInterval = nInterval;
	}
#endif

#ifdef X_DEBUG//!!!D
	String shex = EXT_STR(hex << BigInteger(ChainParams.MaxTarget));
	int len = strlen(shex);

#endif

	return Target(BigInteger(blockLast.get_DifficultyTarget())*span.get_Ticks()/targetSpan.get_Ticks());
}

Target CoinEng::GetNextTarget(const Block& blockLast, const Block& block) {
	return std::min(ChainParams.MaxTarget, GetNextTargetRequired(blockLast, block));
}

void BlockObj::Check(bool bCheckMerkleRoot) {
	if (m_txes.empty() || m_txes.size() > MAX_BLOCK_SIZE)
		Throw(E_COIN_SizeLimits);
	CoinEng& eng = Eng();

	if (Height != -1) {
		if (Height >= eng.ChainParams.AuxPowStartBlock) {
			if ((Ver >> 16) != eng.ChainParams.ChainId)
				Throw(E_COIN_BlockDoesNotHaveOurChainId);
			if (AuxPow) {
				AuxPow->Check(Block(this));
				AuxPow->ParentBlock.CheckPow(DifficultyTarget);
			}
		} else if (AuxPow)
			Throw(E_COIN_AUXPOW_NotAllowed);
	}
	if (!AuxPow && ProofType()==ProofOf::Work) {
		if (eng.ChainParams.HashAlgo == HashAlgo::Sha256 || eng.ChainParams.HashAlgo == HashAlgo::Prime)
			CheckPow(DifficultyTarget);
		else if (Hash() != eng.ChainParams.Genesis) {
			HashValue hash = PowHash(eng);
			byte ar[33];
			memcpy(ar, hash.data(), 32);
			ar[32] = 0;
			if (BigInteger(ar, sizeof ar) >= BigInteger(get_DifficultyTarget()))
				Throw(E_COIN_ProofOfWorkFailed);
		}
	}

	if (Timestamp > DateTime::UtcNow()+TimeSpan::FromSeconds(MAX_FUTURE_SECONDS))
		Throw(E_COIN_BlockTimestampInTheFuture);

	if (!m_txes[0].IsCoinBase())
		Throw(E_COIN_FirstTxIsNotTheOnlyCoinbase);
	for (int i=1; i<m_txes.size(); ++i)
		if (m_txes[i].IsCoinBase())
			Throw(E_COIN_FirstTxIsNotTheOnlyCoinbase);	

	EXT_FOR (const Tx& tx, m_txes) {
		tx.Check();
	}
	if (bCheckMerkleRoot && MerkleRoot(false) != m_merkleRoot.get())
		Throw(E_COIN_MerkleRootMismatch);
	CheckSignature();
}

void BlockObj::CheckCoinbaseTx(Int64 nFees) {
	CoinEng& eng = Eng();

	if (!eng.LiteMode) {
#ifdef X_DEBUG//!!!D
		auto proofType = ProofType();
#endif
		Int64 valueOut = m_txes[0].ValueOut;
		if (valueOut > eng.GetSubsidy(Height, PrevBlockHash, eng.ToDifficulty(DifficultyTarget), true)+nFees) {
			Throw(E_COIN_SubsidyIsVeryBig);
		}
	}
}

void Block::LoadToMemory() {							//!!! Really don't loads TxIn Scripts
	EXT_FOR (const Tx& tx, get_Txes()) {
		tx.TxIns();
		EXT_FOR (const TxOut& txOut, tx.TxOuts()) {
			txOut.get_PkScript();
		}
	}
}

bool Block::HasBestChainWork() const {
	CoinEng& eng = Eng();

	if (!eng.BestBlock())
		return true;
	Block prevBlock = _self;
	BigInteger work = 0, forkWork = 0;

	Block forkBlock = eng.BestBlock();
#ifdef X_DEBUG //!!!D
	BigInteger maxTarg = eng.ChainParams.MaxTarget;
	BigInteger thisDiff = DifficultyTarget;
	BigInteger chainDiff = eng.Chain.BestBlock.DifficultyTarget;
#endif

	while (true) {
		while (prevBlock.Height > forkBlock.Height) {
			work += prevBlock.GetWork();
			prevBlock = prevBlock.GetPrevBlock();
		}
		if (Hash(forkBlock) == Hash(prevBlock))
			break;
		forkWork += forkBlock.GetWork();
		forkBlock = forkBlock.GetPrevBlock();
	}
	return work > forkWork;
}

Block Block::GetOrphanRoot() const {					// ASSERT(eng.Caches.Mtx is locked)
	CoinEng& eng = Eng();
	const Block *r = this;
	for (ChainCaches::COrphanMap::iterator it; (it=eng.Caches.OrphanBlocks.find(r->PrevBlockHash))!=eng.Caches.OrphanBlocks.end();)
		r = &it->second.first;
	return *r;
}

Int64 RunConnectTask(const CConnectJob *pjob, const ConnectTask *ptask) {
	const CConnectJob& job = *pjob;
	const ConnectTask& task = *ptask;
	CCoinEngThreadKeeper engKeeper(&job.Eng);
	t_bPayToScriptHash = (void*)(bool)job.PayToScriptHash;
	Int64 fee = 0;
	try {
		EXT_FOR (const Tx& ptx, task.Txes) {
			Int64 nValueIn = 0;
			const Tx& tx = ptx;
			if (Interlocked::Add(job.SigOps, tx.SigOpCount) > MAX_BLOCK_SIGOPS)
				Throw(E_COIN_TxTooManySigOps);
			for (int i=0; i<tx.TxIns().size(); ++i) {
				if (job.Failed)
					return fee;

				const TxIn& txIn = tx.TxIns()[i];
				const OutPoint& op = txIn.PrevOutPoint;
				const Tx& txPrev = job.TxMap.at(op.TxHash);

				const TxOut& txOut = txPrev.TxOuts()[op.Index];
				if (t_bPayToScriptHash &&  IsPayToScriptHash(txOut.PkScript)) {
					if (Interlocked::Add(job.SigOps, CalcSigOpCount(txOut.PkScript, txIn.Script())) > MAX_BLOCK_SIGOPS)
						Throw(E_COIN_TxTooManySigOps);
				}

				if (txPrev.IsCoinBase())
					job.Eng.CheckCoinbasedTxPrev(job.Height, txPrev);

				if (job.Eng.BestBlockHeight() > job.Eng.ChainParams.LastCheckpointHeight-INITIAL_BLOCK_THRESHOLD)	// Skip ECDSA signature verification when connecting blocks (fBlock=true) during initial download
					VerifySignature(txPrev, tx, i);

				job.Eng.CheckMoneyRange(nValueIn += txOut.Value);
			}
			tx.CheckInOutValue(nValueIn, fee, job.Eng.AllowFreeTxes ? 0 : tx.GetMinFee(1, false), job.DifficultyTarget); 
		}
	} catch (RCExc) {
		job.Failed = true;
	}
	return fee;
}

#if UCFG_COIN_PKSCRIPT_FUTURES
static void CalcPkScript(CoinEng *eng, const TxOut *txOut) {
	CCoinEngThreadKeeper engKeeper(eng);
	txOut->get_PkScript();
}
#endif

void CConnectJob::AsynchCheckAll(const vector<Tx>& txes) {
	vector<future<void>> futsTxOut;
	EXT_FOR (const Tx& tx, txes) {
		HashValue hashTx = Hash(tx);
		if (!tx.IsCoinBase()) {
			if (tx.IsCoinStake())
				tx.m_pimpl->GetCoinAge();		// cache CoinAge, which can't be calculated in Pooled Thread
			ptr<ConnectTask> task(new ConnectTask);
			task->Txes.push_back(tx);
			for (int i=0; i<tx.TxIns().size(); ++i) {
				const OutPoint& op = tx.TxIns()[i].PrevOutPoint;
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
				} else if (!TxMap.count(hashPrev))
					TxMap.insert(make_pair(hashPrev, Tx::FromDb(hashPrev)));
#if UCFG_COIN_PKSCRIPT_FUTURES
				futsTxOut.push_back(std::async(CalcPkScript, &Eng, &TxMap.at(hashPrev).TxOuts().at(op.Index)));
#else
				TxMap.at(hashPrev).TxOuts().at(op.Index).get_PkScript();
#endif
			}
			Tasks.insert(task);
			HashToTask.insert(make_pair(hashTx, task));
		}
		TxMap.insert(make_pair(hashTx, tx));
	}
	EXT_FOR (future<void>& fut, futsTxOut) {
		fut.wait();
	}

	vector<future<Int64>> futures;
	futures.reserve(Tasks.size());
	EXT_FOR (const ptr<ConnectTask>& task, Tasks) {
		futures.push_back(std::async(RunConnectTask, this, task.get()));
	}
	for (int i=0; i<futures.size(); ++i)
		Fee = Eng.CheckMoneyRange(Fee + futures[i].get());
}

void CConnectJob::PrepareTask(const HashValue160& hash160, const ConstBuf& pubKey) {
	if (!IsPubKey(pubKey))
		return;
#ifdef X_DEBUG//!!!D
					if (!memcmp(hash160.data(), "\x14\x7A\x95\x7A", 4)) {
						Blob compressed = ToCompressedKey(pubKey);
						Blob uncomp = ToUncompressedKey(compressed);
						ASSERT(pubKey == uncomp);
						cout << "";
					}
#endif

	Blob compressed;
	try {
		DBG_LOCAL_IGNORE_NAME(MAKE_HRESULT(SEVERITY_ERROR, FACILITY_OPENSSL, EC_R_INVALID_ENCODING), ignEC_R_INVALID_ENCODING);
		DBG_LOCAL_IGNORE_NAME(MAKE_HRESULT(SEVERITY_ERROR, FACILITY_OPENSSL, EC_R_POINT_IS_NOT_ON_CURVE), ignEC_R_POINT_IS_NOT_ON_CURVE);
		DBG_LOCAL_IGNORE_NAME(MAKE_HRESULT(SEVERITY_ERROR, FACILITY_OPENSSL, ERR_R_EC_LIB), ignERR_R_EC_LIB);
		compressed = ToCompressedKey(pubKey);
#ifdef X_DEBUG//!!!D
		if (pubKey.P[1] == 0x69 && pubKey.P[2] == 0x5A)
			compressed = compressed;
#endif
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

				ASSERT(Hash160(ToUncompressedKey(compressed)) == hash160);
			} else {
				pkd.IsTask = false;
				if (pkd.PubKey != compressed)
					pkd.PubKey = Blob(nullptr);				
			}
		}
	} else {
		Int64 id = CIdPk(hash160);
		PubKeyData pkd;
		pkd.PubKey = compressed;
		Blob pk(nullptr);
		EXT_LOCK (Eng.Caches.Mtx) {
			ChainCaches::CCachePkIdToPubKey::iterator j = Eng.Caches.m_cachePkIdToPubKey.find(id);
			if (j != Eng.Caches.m_cachePkIdToPubKey.end()) {
				PubKeyHash160& pkh = j->second.first;
				if (pkh.Hash160 != hash160)
					return;
				ASSERT(!pkh.PubKey || ConstBuf(pkh.PubKey)==pubKey);
				if (pkd.Update = !pkh.PubKey)
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
						Int64 id = CIdPk(hash160);
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
					PrepareTask(hash160, pk);
					break;
				}
			}
			EXT_FOR (const TxIn& txIn, tx.TxIns()) {
				ConstBuf pk = FindStandardHashAllSigPubKey(txIn.Script());
				if (pk.P && pk.Size) {
					ConstBuf pubKey(pk.P+1, pk.Size-1);
					PrepareTask(Hash160(pubKey), pubKey);
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
	vector<future<void>> futures;

	for (CMap::iterator it=Map.begin(), e=Map.end(); it!=e; ++it) {
		if (it->second.IsTask)
			futures.push_back(std::async(CalcPubkeyHash, &*it));   // by pointer, because async params transferred by value
	}

	EXT_FOR (future<void>& fut, futures) {
		fut.wait();
	}
}

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
			if (std::uncaught_exception()) {
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
	CBool m_bCommitted;
};


void Block::Connect() const {
	TRC(3, " " << Hash(_self) << " Height: " << Height << "  " << get_Txes().size() << " Txes");

	Check(false);		// Check Again without bCheckMerkleRoot
	Int64 nFees = 0;
	CoinEng& eng = Eng();

	t_bPayToScriptHash = (void*)(Height >= eng.ChainParams.PayToScriptHashHeight);

	CConnectJob job(eng);
	job.DifficultyTarget = DifficultyTarget;
	if (!eng.LiteMode) {
		job.PayToScriptHash = (void*)t_bPayToScriptHash;
		job.Height = Height;

		job.AsynchCheckAll(Txes);
		if (job.Failed)
			Throw(E_COIN_ProofOfWorkFailed);
		nFees = job.Fee;
	}

	job.Prepare(_self);
	job.Calculate();

	CBoolKeeper keepDisabledPkCache(eng.Caches.PubkeyCacheEnabled, false);
	{
		eng.EnsureTransactionStarted();
		CoinEngTransactionScope scopeBlockSavepoint(eng);

		for (CConnectJob::CMap::iterator it=job.Map.begin(), e=job.Map.end(); it!=e; ++it) {
			if (it->second.Insert) {
#ifdef X_DEBUG//!!!D
				Blob pk = it->second.PubKey;
				if (pk.Size == 20) {
					ASSERT(it->first == it->second.PubKey);
				}
#endif
				eng.Db->InsertPubkey(Int64(CIdPk(it->first)), it->second.PubKey);
			} else if (it->second.Update)
				eng.Db->UpdatePubkey(Int64(CIdPk(it->first)), it->second.PubKey);
		}

		EXT_LOCK (eng.Caches.Mtx) {
			for (CConnectJob::CMap::iterator it=job.Map.begin(), e=job.Map.end(); it!=e; ++it) {				//!!!? necessary to sync cache
				if (it->second.Update) {
					Int64 id = CIdPk(it->first);
					eng.Caches.m_cachePkIdToPubKey.erase(id);													// faster than call ToUncompressedKey()
/*!!!R 				ChainCaches::CCachePkIdToPubKey::iterator j = eng.Caches.m_cachePkIdToPubKey.find(id);	
					if (j != eng.Caches.m_cachePkIdToPubKey.end())
						j->second.first.PubKey = ToUncompressedKey(it->second.PubKey);					*/
				}
			}
		}

		if (eng.LiteMode) {
			CTxMap txMap;
			int nBlockSigOps = 0;
			EXT_FOR (const Tx& tx, get_Txes()) {
				tx.ConnectInputs(txMap, Height, nBlockSigOps, nFees, true, false, eng.AllowFreeTxes ? 0 : tx.GetMinFee(1, false), DifficultyTarget);
			}
		} else {
			EXT_FOR (const Tx& tx, get_Txes()) {
				if (!tx.IsCoinBase()) {
					vector<Tx> vTxPrev;
					for (int i=0; i<tx.TxIns().size(); ++i)
						vTxPrev.push_back(job.TxMap.at(tx.TxIns()[i].PrevOutPoint.TxHash));	
					eng.OnConnectInputs(tx, vTxPrev, true, false);
				}
			}
		}

		if (Height > 0)
			m_pimpl->CheckCoinbaseTx(nFees);

		MemoryStream ms;
		DbWriter wr(ms);
		wr.ConnectJob = &job;
		m_pimpl->Write(wr);
//!!!?			wr << byte(0);

		vector<HashValue> prevTxHashes;
		prevTxHashes.reserve(m_pimpl->m_txes.size());
		Coin::TxHashesOutNums txHashOutNums;
		for (int i=0; i<m_pimpl->m_txes.size(); ++i) {
			if (!eng.Runned)
				Throw(E_EXT_ThreadInterrupted);
			const Tx& tx = m_pimpl->m_txes[i];
			if (eng.ShouldBeSaved(tx)) {
				Coin::HashValue txHash = Coin::Hash(tx);
				prevTxHashes.push_back(txHash);

				MemoryStream msTx;
				DbWriter dw(msTx);
				dw.ConnectJob = &job;
				dw << tx;
				MemoryStream msTxIns;
				DbWriter wrIns(msTxIns);
				wrIns.PCurBlock = this;
				wrIns.ConnectJob = &job;
				if (!eng.LiteMode) {
					wrIns.PTxHashesOutNums = &txHashOutNums;
					tx.WriteTxIns(wrIns);
				}
				eng.Db->InsertTx(tx, txHashOutNums, txHash, Height, msTxIns, CoinEng::SpendVectorToBlob(vector<bool>(tx.TxOuts().size(), true)), msTx);
				if (!eng.LiteMode) {
					TxHashOutNum hom = { txHash, (UInt32)tx.TxOuts().size() };
					txHashOutNums.push_back(hom);
				}

				if (!eng.LiteMode) {
					if (!tx.IsCoinBase()) {
						EXT_FOR (const TxIn& txIn, tx.TxIns()) {
							vector<bool> vSpend = eng.Db->GetCoinsByTxHash(txIn.PrevOutPoint.TxHash);
							vSpend.at(txIn.PrevOutPoint.Index) = false;
							eng.Db->SaveCoinsByTxHash(txIn.PrevOutPoint.TxHash, vSpend);
						}
					}
				}
			}

//				tx.SaveCoins();
		}

		HashValue hashBlock = Hash(_self);
		eng.Db->InsertBlock(Height, hashBlock, ms, EXT_BIN(txHashOutNums));
		eng.OnConnectBlock(_self);
		eng.Caches.DtBestReceived = DateTime::UtcNow();

		EXT_LOCK (eng.Caches.Mtx) {
			eng.Caches.HeightToHashCache.insert(make_pair((ChainCaches::CHeightToHashCache::key_type)Height, hashBlock));
		}


#ifdef X_DEBUG//!!!D
	static FileStream g_fs("c:\\work\\coin\\Bitcoin-testnet2.dump", FileMode::Create, FileAccess::Write);
	BinaryWriter(g_fs) << UInt32(eng.ChainParams.ProtocolMagic) << UInt32(m_pimpl->Binary.Size);
	g_fs.WriteBuf(m_pimpl->Binary);
	g_fs.Flush();
#endif


#ifdef X_DEBUG//!!!D
		EXT_LOCK (eng.Caches.Mtx) {
			eng.Caches.HashToTxCache.clear();
		}
		int h = Height;
		vector<Blob> txblobs;
		if (h == 11116296) {
			EXT_FOR (const Tx& tx, Txes) {
				txblobs.push_back(EXT_BIN(tx));
			}
			cout << "\n\n";
			MemoryStream mst;
			BinaryWriter(mst) << Txes[11].TxOuts()[1];
			cout << Blob(mst);
		}
		Block block = eng.Db->FindBlock(h);
		block.LoadToMemory();
		EXT_FOR (const Tx& tx, block.Txes) {
			tx.m_pimpl->m_nBytesOfHash = 0;
		}	
		block.m_pimpl->m_hash.reset();
		block.m_pimpl->m_txHashesOutNums.clear();
		MemoryStream ms1;
		if (h == 11116296) {
			cout << "\n\n";
			MemoryStream mst;
			BinaryWriter(mst) << block.Txes[11].TxOuts()[1];
			cout << Blob(mst);
			h = h;
		}
		BinaryWriter(ms1) << block;
		HashValue hash = Hash(block);
		ASSERT(!hash.data()[31]);
#endif

		if (eng.m_iiEngEvents)
			eng.m_iiEngEvents->OnBlockConnectDbtx(_self);
	}
#ifdef X_DEBUG//!!!D
	TRC(1, "Before LogObjectCounters");
	LogObjectCounters();
#endif
	eng.SetBestBlock(_self);

	if (eng.m_iiEngEvents) {
		EXT_FOR (const Tx& tx, get_Txes()) {
			eng.m_iiEngEvents->OnProcessTx(tx);
		}
	}
}

void Block::Disconnect() const {
	CoinEng& eng = Eng();
	
	vector<Int64> txids;
	txids.reserve(get_Txes().size());

	eng.EnsureTransactionStarted();
	CoinEngTransactionScope scopeBlockSavepoint(eng);

	for (int i=get_Txes().size(); i--;) {		// reverse order is important
		const Tx& tx = get_Txes()[i];
		Coin::HashValue txhash = Coin::Hash(tx);
		txids.push_back(letoh(*(Int64*)txhash.data()));

		eng.OnDisconnectInputs(tx);

		if (!tx.IsCoinBase()) {
			EXT_FOR (const TxIn& txIn, tx.TxIns()) {
				vector<bool> vec = eng.Db->GetCoinsByTxHash(txIn.PrevOutPoint.TxHash);
				vec.resize(std::max(vec.size(), size_t(txIn.PrevOutPoint.Index+1)));
				vec[txIn.PrevOutPoint.Index] = true;
				eng.Db->SaveCoinsByTxHash(txIn.PrevOutPoint.TxHash, vec);
			}
		}
	}

	eng.Db->DeleteBlock(Height, txids);
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

void Block::Reorganize() {
	TRC(1, "");

	CoinEng& eng = Eng();
	Block prevBlock = _self;
	Block forkBlock = eng.BestBlock();
	vector<Block> vConnect;
	list<Tx> vResurrect;
	while (true) {
		while (prevBlock.Height > forkBlock.Height) {
			vConnect.push_back(prevBlock);
			prevBlock = prevBlock.GetPrevBlock();
		}
		if (Hash(forkBlock) == Hash(prevBlock))
			break;
		forkBlock.Disconnect();
		const CTxes& txes = forkBlock.get_Txes();
		for (CTxes::const_reverse_iterator it=txes.rbegin(); it!=txes.rend(); ++it) {
			if (!it->IsCoinBase())
				vResurrect.push_front(*it);
		}
		forkBlock = forkBlock.GetPrevBlock();
	}
	EXT_LOCK (eng.Caches.Mtx) {
		eng.Caches.HeightToHashCache.clear();
		eng.Caches.HashToAlternativeChain.clear();
	}

	for (int i=vConnect.size(); i--;) {
		Block& block = vConnect[i];
		block.Connect();
		eng.Caches.m_bestBlock = block;
		EXT_FOR (const Tx& tx, block.get_Txes()) {
			eng.RemoveFromMemory(tx);
		}
	}

	vector<HashValue> vQueue;
	EXT_FOR (const Tx& tx, vResurrect) {
		eng.AddToPool(tx, vQueue);
	}
}

void Block::Accept() {
	CoinEng& eng = Eng();

	if (PrevBlockHash == HashValue())
		m_pimpl->Height = 0;	
	else {
		Block blockPrev = eng.LookupBlock(PrevBlockHash);
		
		Target targetNext = eng.GetNextTarget(blockPrev, _self);
		if (get_DifficultyTarget() != targetNext) {
			TRC(1, "CurBlock DifficultyTarget: " << hex << BigInteger(get_DifficultyTarget()));
			TRC(1, "Should be                  " << hex << BigInteger(eng.GetNextTarget(blockPrev, _self)));
#ifdef X_DEBUG//!!!D
 			eng.GetNextTarget(blockPrev, _self);
#endif
			Throw(E_COIN_IncorrectProofOfWork);
		}

		if (Timestamp <= blockPrev.GetMedianTimePast())
			Throw(E_COIN_TooEarlyTimestamp);

		m_pimpl->Height = blockPrev.Height+1;
	}

	for (int i=0; i<m_pimpl->m_txes.size(); ++i)
		m_pimpl->m_txes[i].Height = m_pimpl->Height;

	EXT_FOR (const Tx& tx, get_Txes()) {
		if (!tx.IsFinal(Height, Timestamp))
			Throw(E_COIN_ContainsNonFinalTx);
	}

	HashValue hash = Hash(_self);

	m_pimpl->ComputeAttributes();

	ChainParams::CCheckpoints::iterator itCheckPoint = eng.ChainParams.Checkpoints.find(Height);
	if (itCheckPoint != eng.ChainParams.Checkpoints.end() && itCheckPoint->second != hash)
		Throw(E_COIN_RejectedByCheckpoint);

	EXT_LOCK (eng.Mtx) {
		if (Height <= eng.BestBlockHeight() && eng.HaveBlock(hash))
			return;		// RaceCondition check, block already accepted by parallel thread.
		if (!HasBestChainWork()) {
			EXT_LOCK (eng.Caches.Mtx) {
				eng.Caches.HashToAlternativeChain.insert(make_pair(hash, _self));
			}
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

				EXT_FOR (const Tx& tx, get_Txes()) {
					eng.RemoveFromMemory(tx);
				}
			}
		
			Inventory inv(InventoryType::MSG_BLOCK, hash);

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
	if (eng.m_iiEngEvents)
		eng.m_iiEngEvents->OnBlockchainChanged();
}

void Block::Process(P2P::Link *link) {
	CoinEng& eng = Eng();

#ifdef X_DEBUG//!!!D
	TRC(1, "Before LogObjectCounters");
	LogObjectCounters();
#endif

	HashValue hash = Hash(_self);
	
	if (eng.HaveBlock(hash)) {
		TRC(3, "Have: " << hash << ", PrevBlock: " << PrevBlockHash);
		return;
	}
	TRC(3, "    " << hash << ", PrevBlock: " << PrevBlockHash);

	try {
		DBG_LOCAL_IGNORE_NAME(E_COIN_BlockNotFound, ignE_COIN_BlockNotFound);

		Check(true);
	} catch (RCExc ex) {
		if (HResultInCatch(ex) == E_COIN_BlockNotFound)
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
		if (orphanRoot)
			link->Send(new GetBlocksMessage(eng.BestBlock(), Hash(orphanRoot)));

		TRC(2, "Added to OrphanBlocks, size()= " << EXT_LOCKED(eng.Caches.Mtx, eng.Caches.OrphanBlocks.size()));
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

