/*######     Copyright (c) 1997-2013 Ufasoft  http://ufasoft.com  mailto:support@ufasoft.com,  Sergey Pavlov  mailto:dev@ufasoft.com #######################################
#                                                                                                                                                                          #
# This program is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation;  #
# either version 3, or (at your option) any later version. This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the      #
# implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details. You should have received a copy of the GNU #
# General Public License along with this program; If not, see <http://www.gnu.org/licenses/>                                                                               #
##########################################################################################################################################################################*/

#include <el/ext.h>

#include "proof-of-stake.h"

namespace Coin {

const Version VER_NOVACOIN_STAKE_MODIFIER(0, 53);

const Target NOVACOIN_2013_PROOF_OF_STAKE_LIMIT(0x1D1FFFFF),
			NOVACOIN_PROOF_OF_STAKE_LIMIT(0x1E00FFFF),
			NOVACOIN_PROOF_OF_STAKE_HARD_LIMIT(0x1D03FFFF);

const DateTime DATE_CHECK_BLOCK_SIGNATURE_OUTS(2013, 2, 24),
			DATE_STAKE_SWITCH(2013, 6, 20),
			DATE_2013_PROOF_OF_STAKE_SWITCH(2013, 7, 20);

class NovaCoinBlockObj : public PosBlockObj {
	typedef PosBlockObj base;

public:
	NovaCoinBlockObj() {
		Ver = 6;
	}
protected:
	Target GetTargetLimit(const Block& block) override {
		return block.ProofType!=ProofOf::Stake || Height <= 14061 ? base::GetTargetLimit(block)
			: Height <= 15001 ? NOVACOIN_PROOF_OF_STAKE_HARD_LIMIT
			: Height > 32672 ? NOVACOIN_2013_PROOF_OF_STAKE_LIMIT
			: NOVACOIN_PROOF_OF_STAKE_LIMIT;
	}

	bool StakeEntropyBit() const override {
		if (Height >= 9689)
			return Hash().data()[0] & 1;
		return base::StakeEntropyBit();
	}

	void CheckSignature() override {
		if (Timestamp < DATE_CHECK_BLOCK_SIGNATURE_OUTS || ProofType()==ProofOf::Stake)
			return base::CheckSignature();
		EXT_FOR (const TxOut& txOut, get_Txes()[0].TxOuts()) {
			if (VerifySignatureByTxOut(txOut))
				return;
		}
		Throw(E_COIN_BadBlockSignature);
	}
};

class COIN_CLASS NovaCoinEng : public PosEng {
	typedef PosEng base;
public:
	NovaCoinEng(CoinDb& cdb)
		:	base(cdb)
	{
		MaxBlockVersion = 6;
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

	Int64 GetProofOfStakeReward(Int64 coinAge, const Target& target, const DateTime& dt) override {
		if (dt <= DATE_STAKE_SWITCH)
			return base::GetProofOfStakeReward(coinAge, target, dt);
		const Int64 MAX_MINT_PROOF_OF_STAKE = ChainParams.CoinValue;
		BigInteger lim = GetProofOfStakeLimit(dt);
		const Int64 cent = ChainParams.CoinValue / 100;
		Int64 lower = cent,
			upper = MAX_MINT_PROOF_OF_STAKE;
		for (BigInteger u=pow(BigInteger(MAX_MINT_PROOF_OF_STAKE), 6) * BigInteger(target); lower+cent <= upper;) {
			Int64 mid = (lower + upper) / 2;
			if (pow(BigInteger(mid), 6) * lim > u)
				upper = mid;
			else
				lower = mid;
        }
		return coinAge * 33 / (365 * 33 + 8) * min(upper/cent*cent, MAX_MINT_PROOF_OF_STAKE);
	}
	
	BlockObj *CreateBlockObj() override { return new NovaCoinBlockObj; }
};

static class NovaCoinChainParams : public ChainParams {
	typedef ChainParams base;
public:
	NovaCoinChainParams(bool)
		:	base("NovaCoin", false)
	{	
		ChainParams::Add(_self);
	}

protected:
	NovaCoinEng *CreateObject(CoinDb& cdb, IBlockChainDb *db) override {
		NovaCoinEng *r = new NovaCoinEng(cdb);
		r->SetChainParams(_self, db);
		return r;
	}

} s_novacoinParams(true);


} // Coin::

