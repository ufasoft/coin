/*######   Copyright (c) 2013-2015 Ufasoft  http://ufasoft.com  mailto:support@ufasoft.com,  Sergey Pavlov  mailto:dev@ufasoft.com ####
#                                                                                                                                     #
# 		See LICENSE for licensing information                                                                                         #
#####################################################################################################################################*/

#include <el/ext.h>

#include "../proof-of-stake.h"

namespace Coin {

const Version VER_NOVACOIN_STAKE_MODIFIER(0, 53);

const Target NOVACOIN_2013_PROOF_OF_STAKE_LIMIT(0x1D1FFFFF),
			NOVACOIN_PROOF_OF_STAKE_LIMIT(0x1E00FFFF),
			NOVACOIN_PROOF_OF_STAKE_HARD_LIMIT(0x1D03FFFF);

const DateTime DATE_CHECK_BLOCK_SIGNATURE_OUTS(2013, 2, 24),
			DATE_ENTROPY_SWITCH_TIME(2013, 3, 9),
			DATE_STAKE_SWITCH(2013, 6, 20),
			DATE_2013_PROOF_OF_STAKE_SWITCH(2013, 7, 20),
			DATE_STAKECURVE_SWITCH(2013, 10, 20),
			DtV04Switch(2014, 10, 20);

class NovaCoinBlockObj : public PosBlockObj {
	typedef PosBlockObj base;

public:
	NovaCoinBlockObj() {
		Ver = 6;
	}

	bool IsV04Protocol() const override {
		return Timestamp >= DtV04Switch;
	}
protected:
	Target GetTargetLimit(const Block& block) override {
		return block.ProofType!=ProofOf::Stake || Height <= 14061 ? base::GetTargetLimit(block)
			: Height <= 15001 ? NOVACOIN_PROOF_OF_STAKE_HARD_LIMIT
			: Height > 32672 ? NOVACOIN_2013_PROOF_OF_STAKE_LIMIT
			: NOVACOIN_PROOF_OF_STAKE_LIMIT;
	}

	bool StakeEntropyBit() const override {
		return Height >= 9689 ? base::StakeEntropyBit()
			:	Hash160(Signature)[19] & 0x80;
	}

	void CheckSignature() override {
		if (Timestamp < DATE_ENTROPY_SWITCH_TIME) {
			if (Timestamp < DATE_CHECK_BLOCK_SIGNATURE_OUTS || ProofType()==ProofOf::Stake)
				return base::CheckSignature();
			EXT_FOR (const TxOut& txOut, get_Txes()[0].TxOuts()) {
				if (VerifySignatureByTxOut(txOut))
					return;
			}
			Throw(E_COIN_BadBlockSignature);
		}
	}
};

class NovaCoinTxObj : public PosTxObj {
	typedef PosTxObj base;
protected:
	NovaCoinTxObj *Clone() const override { return new NovaCoinTxObj(_self); }

	void CheckCoinStakeReward(int64_t reward, const Target& target) const override {
		PosEng& eng = (PosEng&)Eng();
		Tx tx((TxObj*)this);
		int64_t posReward = eng.GetProofOfStakeReward(GetCoinAge(), target, Timestamp);
		if (reward > posReward - tx.GetMinFee(1, eng.AllowFreeTxes, MinFeeMode::Block, 0) + eng.ChainParams.MinTxFee)
			Throw(E_COIN_StakeRewardExceeded);
	}
};

class COIN_CLASS NovaCoinEng : public PosEng {
	typedef PosEng base;
public:
	NovaCoinEng(CoinDb& cdb)
		:	base(cdb)
	{
		MaxBlockVersion = 6;
		AllowFreeTxes = true;
	}
protected:
	void TryUpgradeDb() override {
		if (Db->CheckUserVersion() < VER_NOVACOIN_STAKE_MODIFIER)
			Db->Recreate();
		base::TryUpgradeDb();
	}

	BigInteger GetProofOfStakeLimit(const DateTime& dt) {
		if (dt > DATE_2013_PROOF_OF_STAKE_SWITCH)
			return NOVACOIN_2013_PROOF_OF_STAKE_LIMIT;
		if (dt > DATE_STAKE_SWITCH)									//!!!
			return NOVACOIN_PROOF_OF_STAKE_LIMIT;
		return NOVACOIN_PROOF_OF_STAKE_HARD_LIMIT;
	}

	TimeSpan GetTargetSpacingWorkMax(const DateTime& dt) override {
		return dt > DATE_2013_PROOF_OF_STAKE_SWITCH ? TimeSpan::FromMinutes(30) : base::GetTargetSpacingWorkMax(dt);
	}

	int64_t GetProofOfStakeReward(int64_t coinAge, const Target& target, const DateTime& dt) override {
		if (dt <= DATE_STAKE_SWITCH)
			return base::GetProofOfStakeReward(coinAge, target, dt);
		const int64_t MAX_MINT_PROOF_OF_STAKE = ChainParams.CoinValue;
		BigInteger lim = GetProofOfStakeLimit(dt);
		const int64_t cent = ChainParams.CoinValue / 100;
		int64_t lower = cent,
			upper = MAX_MINT_PROOF_OF_STAKE;
		int exp = dt >= DATE_STAKECURVE_SWITCH ? 3 : 6;
		for (BigInteger u=pow(BigInteger(MAX_MINT_PROOF_OF_STAKE), exp) * BigInteger(target); lower+cent <= upper;) {
			int64_t mid = (lower + upper) / 2;
			if (pow(BigInteger(mid), exp) * lim > u)
				upper = mid;
			else
				lower = mid;
        }
		return coinAge * 33 * min(upper/cent*cent, MAX_MINT_PROOF_OF_STAKE) / (365 * 33 + 8);
	}

	void UpdateMinFeeForTxOuts(int64_t& minFee, const int64_t& baseFee, const Tx& tx) override {
	}
	
	BlockObj *CreateBlockObj() override { return new NovaCoinBlockObj; }
	NovaCoinTxObj *CreateTxObj() override { return new NovaCoinTxObj; }
};

static CurrencyFactory<NovaCoinEng> s_novacoin("NovaCoin");

} // Coin::

