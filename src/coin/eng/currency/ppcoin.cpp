/*######   Copyright (c) 2012-2015 Ufasoft  http://ufasoft.com  mailto:support@ufasoft.com,  Sergey Pavlov  mailto:dev@ufasoft.com ####
#                                                                                                                                     #
# 		See LICENSE for licensing information                                                                                         #
#####################################################################################################################################*/

#include <el/ext.h>

#include "../proof-of-stake.h"


namespace Coin {

const Version VER_PPCOIN_STAKE_MODIFIER(0, 50),
			VER_PPCOIN_V04_STAKE_MODIFIER(0, 93);

static DateTime DtV03Switch(2013, 3, 20, 17, 20),
			DtV04Switch(2014, 5, 5, 14, 26, 40);


class PPCoinBlockObj : public PosBlockObj {
	typedef PPCoinBlockObj class_type;
	typedef PosBlockObj base;
public:

	PPCoinBlockObj() {
	}

	bool IsV02Protocol(const DateTime& dt) const {
		return dt < DtV03Switch;
	}

	bool IsV04Protocol() const override {
		return Timestamp >= DtV04Switch;
	}

	void CheckCoinStakeTimestamp() override {
		DateTime dtTx = GetTxObj(1).Timestamp;
		if (!IsV02Protocol(dtTx))
			base::CheckCoinStakeTimestamp();
		else if (dtTx > Timestamp || Timestamp > dtTx + seconds(MAX_FUTURE_SECONDS)) 
			Throw(CoinErr::TimestampViolation);
	}

	void WriteKernelStakeModifier(BinaryWriter& wr, const Block& blockPrev) const override {
		DateTime dtTx = GetTxObj(1).Timestamp;
		if (!IsV02Protocol(dtTx))
			base::WriteKernelStakeModifier(wr, blockPrev);
		else
			wr << DifficultyTargetBits;		
	}
protected:
	bool StakeEntropyBit() const override {
		return IsV04Protocol() ? base::StakeEntropyBit()
			:	Hash160(Signature)[19] & 0x80;
	}
};


class COIN_CLASS PPCoinEng : public PosEng {
	typedef PosEng base;
public:
	PPCoinEng(CoinDb& cdb)
		:	base(cdb)
	{
	}

protected:
	void TryUpgradeDb() override {
		if (Db->CheckUserVersion() < VER_PPCOIN_V04_STAKE_MODIFIER)
			Db->Recreate();
		base::TryUpgradeDb();
	}

	BlockObj *CreateBlockObj() override { return new PPCoinBlockObj; }
};

static CurrencyFactory<PPCoinEng> s_ppcoin("PPCoin");

} // Coin::

