/*######   Copyright (c) 2013-2015 Ufasoft  http://ufasoft.com  mailto:support@ufasoft.com,  Sergey Pavlov  mailto:dev@ufasoft.com ####
#                                                                                                                                     #
# 		See LICENSE for licensing information                                                                                         #
#####################################################################################################################################*/

#pragma once

#include "eng.h"
#include "script.h"
#include "coin-model.h"

namespace Coin {

class PosTxObj : public TxObj {
	typedef TxObj base;
public:
	DateTime Timestamp;

	PosTxObj()
		:	Timestamp(Clock::now())
		,	m_coinAge(numeric_limits<uint64_t>::max())
	{}

	int64_t GetCoinAge() const override;
protected:
	PosTxObj *Clone() const override { return new PosTxObj(_self); }

	void WritePrefix(BinaryWriter& wr) const override {
		wr << (uint32_t)to_time_t(Timestamp);
	}

	void ReadPrefix(const BinaryReader& rd) override {
		Timestamp = DateTime::from_time_t(rd.ReadUInt32());
	}

	void CheckCoinStakeReward(int64_t reward, const Target& target) const override;
private:
	mutable int64_t m_coinAge;
};

class PosBlockObj : public BlockObj {
	typedef PosBlockObj class_type;
	typedef BlockObj base;
public:
	Blob Signature;
	optional<uint64_t> StakeModifier;

	const seconds STAKE_MIN_AGE;				// minimum age for coin age

	static PosBlockObj& Of(const Block& block) {
		return *dynamic_cast<PosBlockObj*>(block.m_pimpl.get());
	}

	PosBlockObj()
		: STAKE_MIN_AGE(60 * 60 * 24 * 30) {
	}

	virtual bool StakeEntropyBit() const {
		return Hash().data()[0] & 1;
	}

	using base::Write;
	void WriteSuffix(BinaryWriter& wr) const {
		base::WriteSuffix(wr);
		CoinSerialized::WriteSpan(wr, Signature);
	}

	void Read(const BinaryReader& rd) override {
		base::Read(rd);
		Signature = CoinSerialized::ReadBlob(rd);
	}

	void WriteHeaderInMessage(BinaryWriter& wr) const override {
		base::WriteHeaderInMessage(wr);
		CoinSerialized::WriteCompactSize(wr, 0);				// signature
	}

	void ReadHeaderInMessage(const BinaryReader& rd) override {
		base::ReadHeaderInMessage(rd);
		CoinSerialized::ReadCompactSize64(rd);					// signature unused
	}

	void WriteDbSuffix(BinaryWriter& wr) const override;
	void ReadDbSuffix(const BinaryReader& rd) override;

	ProofOf ProofType() const override {
		return get_Txes().size() > 1 && get_Txes()[1].IsCoinStake() ? ProofOf::Stake : ProofOf::Work;
	}

	Coin::HashValue Hash() const override;
	void Write(DbWriter& wr) const override;
	void Read(const DbReader& rd) override;

	HashValue HashProof() const {
		return ProofType()==ProofOf::Stake ? HashProofOfStake() : Hash();
	}

	BigInteger GetWork() const override {
		return ProofType() == ProofOf::Stake ? base::GetWork() : 1;
	}

	const PosTxObj& GetTxObj(int idx) const { return *(const PosTxObj*)(get_Txes()[idx].m_pimpl.get()); }

	virtual void CheckCoinStakeTimestamp() {
		DBG_LOCAL_IGNORE_CONDITION(CoinErr::TimestampViolation);

		if (GetTxObj(1).Timestamp != Timestamp)
			Throw(CoinErr::TimestampViolation);
	}

	virtual int64_t CorrectTimeWeight(DateTime dtTx, int64_t nTimeWeight) const;
	void WriteKernelStakeModifierV05(DateTime dtTx, BinaryWriter& wr, const Block& blockPrev) const;
	virtual void WriteKernelStakeModifier(BinaryWriter& wr, const Block& blockPrev) const;
	HashValue HashProofOfStake() const;
	bool VerifySignatureByTxOut(const TxOut& txOut);
	void CheckSignature() override;
	virtual void CheckCoinbaseTimestamp();
	void Check(bool bCheckMerkleRoot) override;

	void CheckProofOfStake() const {
		HashProofOfStake();
	}

	void ComputeAttributes() override {
		base::ComputeAttributes();
		ComputeStakeModifier();
	}

	virtual Target GetTargetLimit(const Block& block) { return Eng().ChainParams.MaxTarget; }
protected:
	virtual bool IsV04Protocol() const { return false; }
private:
	mutable optional<HashValue> m_hashProofOfStake;

	uint32_t StakeModifierChecksum(uint64_t modifier);
	void ComputeStakeModifier();
};

class PosEng : public CoinEng {
	typedef CoinEng base;
public:
	mutex MtxStakeModifierCache;
	struct StakeModifierItem {
		DateTime Timestamp;
		optional<uint64_t> StakeModifier;

		StakeModifierItem() {}

		StakeModifierItem(const DateTime& ts, const optional<uint64_t>& modifier)
			: Timestamp(ts)
			, StakeModifier(modifier)
		{}
	};
	LruMap<int, StakeModifierItem> StakeModifierCache;

	PosEng(CoinDb& cdb);

	StakeModifierItem GetStakeModifierItem(int height);
	StakeModifierItem GetLastStakeModifier(const HashValue& hashBlock, int height);
	virtual int64_t GetProofOfStakeReward(int64_t coinAge, const Target& target, const DateTime& dt);
protected:
	void ClearByHeightCaches() override;
	int64_t GetMaxSubsidy() { return ChainParams.InitBlockValue * ChainParams.CoinValue; }
	int64_t GetSubsidy(int height, const HashValue& prevBlockHash, double difficulty, bool bForCheck) override;

	TxObj *CreateTxObj() override { return new PosTxObj; }
	virtual TimeSpan GetTargetSpacingWorkMax(const DateTime& dt) { return TimeSpan::FromHours(2); }
	Target GetNextTargetRequired(const BlockHeader& headerLast, const Block& block) override;

	Target GetNextTarget(const BlockHeader& headerLast, const Block& block) override {
		return GetNextTargetRequired(headerLast, block);
	}

	CoinMessage *CreateCheckPointMessage() override;
};




} // Coin::
