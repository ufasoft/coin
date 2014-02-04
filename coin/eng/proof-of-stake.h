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
		:	Timestamp(DateTime::UtcNow())
		,	m_coinAge(numeric_limits<UInt64>::max())
	{}

	Int64 GetCoinAge() const override;
protected:
	PosTxObj *Clone() const override { return new PosTxObj(_self); }

	void WritePrefix(BinaryWriter& wr) const override {
		wr << (UInt32)to_time_t(Timestamp);
	}

	void ReadPrefix(const BinaryReader& rd) override {
		Timestamp = DateTime::from_time_t(rd.ReadUInt32());
	}

	void CheckCoinStakeReward(Int64 reward, const Target& target) const override;
private:
	mutable Int64 m_coinAge;
};

class PosBlockObj : public BlockObj {
	typedef PosBlockObj class_type;
	typedef BlockObj base;
public:
	Blob Signature;
	optional<UInt64> StakeModifier;	

	static PosBlockObj& Of(const Block& block) {
		return *dynamic_cast<PosBlockObj*>(block.m_pimpl.get());
	}

	virtual bool StakeEntropyBit() const {
		return Hash160(Signature)[19] & 0x80;
	}

	void Write(BinaryWriter& wr) const override {
		base::Write(wr);
		CoinSerialized::WriteBlob(wr, Signature);
	}

	void Read(const BinaryReader& rd) override {
		base::Read(rd);
		Signature = CoinSerialized::ReadBlob(rd);
	}

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
		return ProofType()==ProofOf::Stake ? base::GetWork() : 1;
	}

	const PosTxObj& GetTxObj(int idx) const { return *(const PosTxObj*)(get_Txes()[idx].m_pimpl.get()); }

	virtual void CheckCoinStakeTimestamp() {
		DBG_LOCAL_IGNORE_NAME(E_COIN_TimestampViolation, E_COIN_TimestampViolation);

		if (GetTxObj(1).Timestamp != Timestamp)
			Throw(E_COIN_TimestampViolation);
	}

	virtual void WriteKernelStakeModifier(BinaryWriter& wr, const Block& blockPrev) const;
	HashValue HashProofOfStake() const;
	bool VerifySignatureByTxOut(const TxOut& txOut);
	void CheckSignature() override;
	void Check(bool bCheckMerkleRoot) override;

	void CheckProofOfStake() const {
		HashProofOfStake();
	}

	void ComputeAttributes() override {
		base::ComputeAttributes();
		ComputeStakeModifier();
	}

	virtual Target GetTargetLimit(const Block& block) { return Eng().ChainParams.MaxTarget; }
private:
	mutable optional<HashValue> m_hashProofOfStake;

	UInt32 StakeModifierChecksum(UInt64 modifier);
	void ComputeStakeModifier();
};

class PosEng : public CoinEng {
	typedef CoinEng base;
public:
	PosEng(CoinDb& cdb);

	virtual Int64 GetProofOfStakeReward(Int64 coinAge, const Target& target, const DateTime& dt);
protected:
	Int64 GetMinRelayTxFee() override { return ChainParams.MinTxFee; }
	bool MiningAllowed() override { return false; }
	Int64 GetMaxSubsidy() { return ChainParams.InitBlockValue * ChainParams.CoinValue; }
	Int64 GetSubsidy(int height, const HashValue& prevBlockHash, double difficulty, bool bForCheck) override;

	TxObj *CreateTxObj() override { return new PosTxObj; }
	virtual TimeSpan GetTargetSpacingWorkMax(const DateTime& dt) { return TimeSpan::FromHours(2); }
	Target GetNextTargetRequired(const Block& blockLast, const Block& block) override;
	
	Target GetNextTarget(const Block& blockLast, const Block& block) override {
		return GetNextTargetRequired(blockLast, block);
	}

	CoinMessage *CreateCheckPointMessage() override;
};




} // Coin::

