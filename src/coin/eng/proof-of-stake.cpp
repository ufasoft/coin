/*######   Copyright (c) 2013-2015 Ufasoft  http://ufasoft.com  mailto:support@ufasoft.com,  Sergey Pavlov  mailto:dev@ufasoft.com ####
#                                                                                                                                     #
# 		See LICENSE for licensing information                                                                                         #
#####################################################################################################################################*/

#include <el/ext.h>

#include "proof-of-stake.h"

namespace Coin {

static const TimeSpan STAKE_TARGET_SPACING = TimeSpan::FromMinutes(10);

static const seconds MODIFIER_INTERVAL_SECONDS(6 * 3600);

const int MODIFIER_INTERVAL_RATIO = 3;

static TimeSpan GetStakeModifierSelectionIntervalSection(int nSection) {
	return TimeSpan::FromSeconds(double((MODIFIER_INTERVAL_SECONDS.count() * 63 / (63 + ((63 - nSection) * (MODIFIER_INTERVAL_RATIO - 1))))));
}

static TimeSpan GetStakeModifierSelectionInterval() {
    TimeSpan r(0);
    for (int nSection=0; nSection<64; nSection++)
        r += GetStakeModifierSelectionIntervalSection(nSection);
    return r;
}

static const TimeSpan STAKE_MODIFIER_SELECTION_INTERVAL = GetStakeModifierSelectionInterval();

int64_t PosTxObj::GetCoinAge() const {
	if (m_coinAge == numeric_limits<uint64_t>::max()) {
		CoinEng& eng = Eng();

		BigInteger centSecond = 0;
		if (Tx((PosTxObj*)this).IsCoinBase())
			m_coinAge = 0;
		else {
			EXT_FOR (const TxIn& txIn, TxIns()) {
				Tx txPrev = Tx::FromDb(txIn.PrevOutPoint.TxHash);
				DateTime dtPrev = ((PosTxObj*)txPrev.m_pimpl.get())->Timestamp;
				if (Timestamp < dtPrev)
					Throw(CoinErr::TimestampViolation);
				if (eng.GetBlockByHeight(txPrev.Height).get_Timestamp()+TimeSpan::FromDays(30) <= Timestamp)
					centSecond += BigInteger(txPrev.TxOuts()[txIn.PrevOutPoint.Index].Value) * duration_cast<seconds>(Timestamp-dtPrev).count() / (eng.ChainParams.CoinValue / 100);
			}
			m_coinAge = explicit_cast<int64_t>(centSecond / (100 * 24*60*60));
		}
	}
	return m_coinAge;
}

int64_t GetProofOfStakeReward(int64_t coinAge) {
	CoinEng& eng = Eng();
	return eng.ChainParams.CoinValue * eng.ChainParams.AnnualPercentageRate * coinAge * 33 / (100*(365*33 + 8));
}

void PosTxObj::CheckCoinStakeReward(int64_t reward, const Target& target) const {
	PosEng& eng = (PosEng&)Eng();
	Tx tx((TxObj*)this);
	int64_t posReward = eng.GetProofOfStakeReward(GetCoinAge(), target, Timestamp);
	if (reward > posReward - tx.GetMinFee(1, eng.AllowFreeTxes) + eng.ChainParams.MinTxFee)
		Throw(CoinErr::StakeRewardExceeded);
}

HashValue PosBlockObj::Hash() const {
	if (!m_hash) {
		MemoryStream ms;
		BinaryWriter(ms).Ref() << Ver << PrevBlockHash << MerkleRoot() << (uint32_t)to_time_t(Timestamp) << get_DifficultyTarget() << Nonce;
		Span mb = ms;
		switch (Eng().ChainParams.HashAlgo) {
		case HashAlgo::Sha256:
			m_hash = Coin::Hash(mb);
			break;
		case HashAlgo::SCrypt:
			m_hash = ScryptHash(mb);
			break;
		default:
			Throw(E_NOTIMPL);
		}
	}
	return m_hash.get();
}

void PosBlockObj::WriteDbSuffix(BinaryWriter& wr) const {
	uint8_t flags = (ProofType() == ProofOf::Stake ? 1 : 0) | (!!StakeModifier ? 4 : 0);
	wr << flags;
	if (ProofType() == ProofOf::Stake)
		wr << HashProofOfStake();
	if (!!StakeModifier)
		wr << StakeModifier.get();
}

void PosBlockObj::Write(DbWriter& wr) const {
	base::Write(wr);
	CoinSerialized::WriteSpan(wr, Signature);
	WriteDbSuffix(wr);
}

void PosBlockObj::ReadDbSuffix(const BinaryReader& rd) {
	uint8_t flags = rd.ReadByte();
	if (flags & 1) {
		HashValue h;
		rd >> h;
		m_hashProofOfStake = h;
	}
	//		m_stakeEntropyBit = flags & 2;
	if (flags & 4)
		StakeModifier = rd.ReadUInt64();
}

void PosBlockObj::Read(const DbReader& rd) {
	base::Read(rd);
	Signature = CoinSerialized::ReadBlob(rd);
	ReadDbSuffix(rd);
}

PosEng::StakeModifierItem PosEng::GetStakeModifierItem(int height) {
	StakeModifierItem r;
	EXT_LOCK(MtxStakeModifierCache) {
		if (!Lookup(StakeModifierCache, height, r)) {
//			TRC(4, "H: " << height);	//!!!T
			Block b = Db->FindBlockPrefixSuffix(height);
			PosBlockObj& pos = PosBlockObj::Of(b);
			r = StakeModifierItem(pos.Timestamp, pos.StakeModifier);
			StakeModifierCache.insert(make_pair(height, r));
		}
	}
	return r;
}

PosEng::StakeModifierItem PosEng::GetLastStakeModifier(const HashValue& hashBlock, int height) {
	EXT_LOCK(Caches.Mtx) {
		Block b(nullptr);
		for (HashValue hash=hashBlock; ; hash=b.PrevBlockHash, --height) {
			if (!Lookup(Caches.HashToBlockCache, hash, b)) {
				BlockTreeItem bti = Tree.FindInMap(hash);
				if (!bti || bti.IsHeaderOnly)
					break;
				b = Block(bti.m_pimpl);
			}
			PosBlockObj& pos = PosBlockObj::Of(b);
			if (!!pos.StakeModifier)
				return StakeModifierItem(pos.Timestamp, pos.StakeModifier);
		}
	}
	for (; height>=0; --height) {
		StakeModifierItem item = GetStakeModifierItem(height);
		if (!!item.StakeModifier)
			return item;
	}
	Throw(E_FAIL);
}

int64_t PosBlockObj::CorrectTimeWeight(DateTime dtTx, int64_t nTimeWeight) const {
	return nTimeWeight;
}

void PosBlockObj::WriteKernelStakeModifierV05(DateTime dtTx, BinaryWriter& wr, const Block& blockPrev) const {	//!!!TODO test
	PosEng& eng = static_cast<PosEng&>(Eng());

	DateTime dtCompare = dtTx - STAKE_MIN_AGE + STAKE_MODIFIER_SELECTION_INTERVAL;
	DateTime dt = blockPrev.Timestamp;
	if (dt <= dtCompare) {
		Throw(CoinErr::CoinstakeCheckTargetFailed);
	}
	PosEng::StakeModifierItem item ={ blockPrev.Timestamp, PosBlockObj::Of(blockPrev).StakeModifier };
	for (Block b = blockPrev; dt > dtCompare; b = eng.Tree.GetBlock(b.PrevBlockHash)) {
		if (!!(item = eng.GetStakeModifierItem(b.Height-1)).StakeModifier)
			dt = item.Timestamp;
	}
	wr << item.StakeModifier.get();
}

void PosBlockObj::WriteKernelStakeModifier(BinaryWriter& wr, const Block& blockPrev) const {
	PosEng& eng = static_cast<PosEng&>(Eng());

	PosEng::StakeModifierItem item ={ blockPrev.Timestamp, PosBlockObj::Of(blockPrev).StakeModifier };
	int height = blockPrev.Height;
	for (DateTime dt=item.Timestamp, dtTarget=dt+STAKE_MODIFIER_SELECTION_INTERVAL; dt<dtTarget;) {
		if (height == eng.BestBlock().Height)
			Throw(CoinErr::CoinstakeCheckTargetFailed);
		if (!!(item = eng.GetStakeModifierItem(++height)).StakeModifier)
			dt = item.Timestamp;
	}
//	TRC(2, hex << item.StakeModifier.get() << " at height " << dec << b.Height << " timestamp " << dt);
	wr << item.StakeModifier.get();
}

HashValue PosBlockObj::HashProofOfStake() const {
	if (!m_hashProofOfStake) {
		CoinEng& eng = Eng();
		DBG_LOCAL_IGNORE_CONDITION(CoinErr::TxNotFound);

		const Tx& tx = get_Txes()[1];
		const TxIn& txIn = tx.TxIns()[0];
		Tx txPrev = Tx::FromDb(txIn.PrevOutPoint.TxHash);
		DateTime dtTx = ((PosTxObj*)tx.m_pimpl.get())->Timestamp,
			   dtPrev = ((PosTxObj*)txPrev.m_pimpl.get())->Timestamp;
		if (dtTx < dtPrev)
			Throw(CoinErr::TimestampViolation);
		VerifySignature(txPrev, tx, 0);

		Block blockPrev = eng.GetBlockByHeight(txPrev.Height);
		if (blockPrev.get_Timestamp()+TimeSpan::FromDays(30) > dtTx)
			Throw(CoinErr::CoinsAreTooRecent);

		int64_t val = txPrev.TxOuts()[txIn.PrevOutPoint.Index].Value;
		int64_t nTimeWeight = CorrectTimeWeight(dtTx, duration_cast<seconds>(std::min(dtTx-dtPrev, TimeSpan::FromDays(90))).count());
		BigInteger cdays = BigInteger(val) * nTimeWeight / (24 * 60 * 60 * eng.ChainParams.CoinValue);

		DBG_LOCAL_IGNORE_CONDITION(CoinErr::CoinstakeCheckTargetFailed);

		MemoryStream ms;
		BinaryWriter wr(ms);
		WriteKernelStakeModifier(wr, blockPrev);

		wr << (uint32_t)to_time_t(blockPrev.get_Timestamp());
		MemoryStream msBlock;
		ProtocolWriter wrPrev(msBlock);
		blockPrev.WriteHeader(wrPrev);
		CoinSerialized::WriteCompactSize(wrPrev, blockPrev.get_Txes().size());
		EXT_FOR (const Tx& t, blockPrev.get_Txes()) {
			if (Coin::Hash(t) == txIn.PrevOutPoint.TxHash) {
				wr << uint32_t(Span(msBlock).size());
				break;
			}
			wrPrev << t;
		}
		wr << (uint32_t)to_time_t(dtPrev) << uint32_t(txIn.PrevOutPoint.Index) << (uint32_t)to_time_t(dtTx);
		HashValue h = Coin::Hash(ms);

		uint8_t ar[33];
		memcpy(ar, h.data(), 32);
		ar[32] = 0;
		if (BigInteger(ar, sizeof ar) > BigInteger(get_DifficultyTarget())*cdays) {
	#ifdef _DEBUG//!!!D
			TRC(2, hex << BigInteger(ar, sizeof ar));
			TRC(2, hex << BigInteger(get_DifficultyTarget())*cdays);
	#endif
			Throw(CoinErr::CoinstakeCheckTargetFailed);
		}
		m_hashProofOfStake = h;
	}
	return m_hashProofOfStake.value();
}

Target PosEng::GetNextTargetRequired(const BlockHeader& headerLast, const Block& block) {
	ProofOf proofType = block.ProofType;
	Block prev = Tree.GetBlock(Hash(headerLast));
	while (prev && prev.ProofType != proofType)
		prev = prev.PrevBlockHash==HashValue::Null() ? Block(nullptr) : Tree.GetBlock(prev.PrevBlockHash);
	if (!prev)
		return ChainParams.InitTarget;
	Block prevPrev = prev.PrevBlockHash==HashValue::Null() ? Block(nullptr) : Tree.GetBlock(prev.PrevBlockHash);
	while (prevPrev && prevPrev.ProofType != proofType)
		prevPrev = prevPrev.PrevBlockHash==HashValue::Null() ? Block(nullptr) : Tree.GetBlock(prevPrev.PrevBlockHash);
	if (!prevPrev || prevPrev.PrevBlockHash==HashValue::Null())
		return ChainParams.InitTarget;

	TimeSpan nTargetSpacing = proofType==ProofOf::Stake ? STAKE_TARGET_SPACING
													: std::min(GetTargetSpacingWorkMax(headerLast.Timestamp), TimeSpan(TimeSpan::FromMinutes(10)*(headerLast.Height - prev.Height + 1)));
	int64_t interval = TimeSpan::FromDays(7).count() / nTargetSpacing.count();
	BigInteger r = BigInteger(prev.get_DifficultyTarget()) * (nTargetSpacing.count() * (interval-1) + (prev.get_Timestamp() - prevPrev.get_Timestamp()).count()*2) / (nTargetSpacing.count()*(interval+1));

//		TRC(2, hex << r << "  " << int((prev.Timestamp - prevPrev.Timestamp).TotalSeconds) << " s");

	return std::min(dynamic_cast<PosBlockObj*>(headerLast.m_pimpl.get())->GetTargetLimit(block), Target(r));
}

bool PosBlockObj::VerifySignatureByTxOut(const TxOut& txOut) {
	Address a = TxOut::CheckStandardType(txOut.ScriptPubKey);
	return a.Type == AddressType::PubKey && KeyInfoBase::VerifyHash(a.Data(), Hash(), Signature);
}

void PosBlockObj::CheckSignature() {
	CoinEng& eng = Eng();
	const vector<Tx>& txes = get_Txes();

	if (0 == Signature.size()) {
		if (Hash() == eng.ChainParams.Genesis)
			return;
	} else if (VerifySignatureByTxOut(ProofType() == ProofOf::Stake ? txes[1].TxOuts()[1] : txes[0].TxOuts()[0]))
		return;
	Throw(CoinErr::BadBlockSignature);
}

void PosBlockObj::CheckCoinbaseTimestamp() {
	if (Timestamp > GetTxObj(0).Timestamp + seconds(MAX_FUTURE_SECONDS))
		Throw(CoinErr::CoinbaseTimestampIsTooEarly);
}

void PosBlockObj::Check(bool bCheckMerkleRoot) {
	CoinEng& eng = Eng();
	const vector<Tx>& txes = get_Txes();

	base::Check(bCheckMerkleRoot);

	for (int i = 2; i < txes.size(); ++i)
		if (txes[i].IsCoinStake())
			Throw(CoinErr::CoinstakeInWrongPos);

	CheckCoinbaseTimestamp();
	if (ProofType() == ProofOf::Stake) {
		CheckCoinStakeTimestamp();
		CheckProofOfStake();
	}
}

#ifdef X_DEBUG

unordered_map<int, uint32_t> g_h2modsum;

uint32_t PosBlockObj::StakeModifierChecksum(uint64_t modifier) {
	MemoryStream ms;
	BinaryWriter wr(ms);
	if (Height != 0) {
		ASSERT(g_h2modsum.count(Height-1));
		wr << g_h2modsum[Height-1];
	}
	uint32_t flags = 0;
	if (m_hashProofOfStake)
		flags |= 1;
	if (StakeEntropyBit())
		flags |= 2;
	if (!!StakeModifier)
		flags |= 4;

	wr << flags;
	if (m_hashProofOfStake)
		wr << m_hashProofOfStake.value();
	else
		wr << HashValue::Null();
	if (!!StakeModifier)
		wr << StakeModifier.get();
	else
		wr << modifier;
	HashValue h = Coin::Hash(ms);
	uint32_t r =  *(uint32_t*)(h.data()+28);
	g_h2modsum[Height] = r;
	TRC(2, Height << "  " << hex << r);
/*!!! Novacoin Specific
	switch (Height) {
	case 0:
		if (r != 0x0e00670b)
			Throw(E_FAIL);
		break;
	case 6000:
		if (r != 0xb7cbc5d3)
			Throw(E_FAIL);
		break;
	case 12661:
		if (r != 0x5d84115d)
			Throw(E_FAIL);
		break;
	}
	*/
	return r;
}


#endif // _DEBUG


void PosBlockObj::ComputeStakeModifier() {
	if (0 == Height)
		StakeModifier = 0;
	else {
		PosEng& eng = static_cast<PosEng&>(Eng());
		Block blockPrev = eng.Tree.GetBlock(PrevBlockHash);
		PosEng::StakeModifierItem itemModifier = eng.GetLastStakeModifier(Coin::Hash(blockPrev), blockPrev.Height);
		DateTime dtStartOfPrevInterval = DateTime::from_time_t(to_time_t(blockPrev.get_Timestamp()) / MODIFIER_INTERVAL_SECONDS.count() * MODIFIER_INTERVAL_SECONDS.count());
		if (itemModifier.Timestamp < dtStartOfPrevInterval) {
			DateTime dtStartOfCurInterval = DateTime::from_time_t(to_time_t(Timestamp) / MODIFIER_INTERVAL_SECONDS.count() * MODIFIER_INTERVAL_SECONDS.count());
			if (!IsV04Protocol() || itemModifier.Timestamp < dtStartOfCurInterval) {
				typedef map<pair<DateTime, HashValue>, Block> CCandidates;
				CCandidates candidates;
				DateTime dtStart = dtStartOfPrevInterval - STAKE_MODIFIER_SELECTION_INTERVAL,
					dtStop = dtStart;
				for (Block b=blockPrev; b.Timestamp >= dtStart; b=eng.Tree.GetBlock(b.PrevBlockHash)) {
					candidates.insert(make_pair(make_pair(b.Timestamp, Coin::Hash(b)), b));
					if (b.Height == 0)
						break;
				}

				uint64_t r = 0;
				unordered_set<int> selectedBlocks;
				for (int i=0, n=std::min(64, (int)candidates.size()); i<n; ++i) {
					dtStop += GetStakeModifierSelectionIntervalSection(i);
					Block b(nullptr);
					HashValue hashBest;
					EXT_FOR(const CCandidates::value_type& cd, candidates) {
						if (b && cd.second.Timestamp > dtStop)
							break;
						if (!selectedBlocks.count(cd.second.Height)) {
							PosBlockObj& bo = PosBlockObj::Of(cd.second);
							MemoryStream ms(64);
							BinaryWriter(ms).Ref() << bo.HashProof() << itemModifier.StakeModifier.get();
							HashValue hash = Coin::Hash(ms);
							if (bo.ProofType()==ProofOf::Stake)
								memset((uint8_t*)memmove(hash.data(), hash.data()+4, 28) + 28, 0, 4);
							if (!b || hash < hashBest) {
								hashBest = hash;
								b = cd.second;
							}
						}
					}
					if (!b)
						Throw(E_FAIL);
					r |= uint64_t(PosBlockObj::Of(b).StakeEntropyBit()) << i;
					selectedBlocks.insert(b.Height);
				}

				TRC(5, "H: " << Height << "         Modifier: "<< hex << r);
				StakeModifier = r;
			}
		}
	}
#ifdef X_DEBUG//!!!D
	StakeModifierChecksum(modifier);
#endif
}

class CheckPointMessage : public CoinMessage {
	typedef CoinMessage base;
public:
	int32_t Ver;
	HashValue Hash;
	Blob Msg, Sig;

	CheckPointMessage()
		:	base("checkpoint")
		,	Ver(1)
	{}

	void Write(BinaryWriter& wr) const override {
		WriteSpan(wr, Msg);
		WriteSpan(wr, Sig);
	}

	void Read(const BinaryReader& rd) override {
		Msg = ReadBlob(rd);
		Sig = ReadBlob(rd);
		CMemReadStream stm(Msg);
		BinaryReader(stm) >> Ver >> Hash;
	}
protected:
	void Process(P2P::Link& link) override;
};

void CheckPointMessage::Process(P2P::Link& link) {
	CoinEng& eng = Eng();
	if (!KeyInfoBase::VerifyHash(eng.ChainParams.CheckpointMasterPubKey, Coin::Hash(Msg), Sig))
		Throw(CoinErr::CheckpointVerifySignatureFailed);
	//!!! TODO
}

PosEng::PosEng(CoinDb& cdb)
	: base(cdb)
	, StakeModifierCache(20000)
{
	AllowFreeTxes = false;
}

void PosEng::ClearByHeightCaches() {
	base::ClearByHeightCaches();
	EXT_LOCKED(MtxStakeModifierCache, StakeModifierCache.clear());
}

int64_t PosEng::GetSubsidy(int height, const HashValue& prevBlockHash, double difficulty, bool bForCheck) {
	int64_t centValue = ChainParams.CoinValue / 100;
	double dr = GetMaxSubsidy() / (pow(difficulty, ChainParams.PowOfDifficultyToHalfSubsidy) * centValue);
	int64_t r = int64_t(ceil(dr)) * centValue;
	return r;
}

CoinMessage *PosEng::CreateCheckPointMessage() {
	return new CheckPointMessage;
}

int64_t PosEng::GetProofOfStakeReward(int64_t coinAge, const Target& target, const DateTime& dt) {
	return ChainParams.CoinValue * ChainParams.AnnualPercentageRate * coinAge * 33 / (100*(365*33 + 8));
}

} // Coin::
