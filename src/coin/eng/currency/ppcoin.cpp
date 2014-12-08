#include <el/ext.h>

#include "../proof-of-stake.h"


namespace Coin {

const Version VER_PPCOIN_STAKE_MODIFIER(0, 50);

static DateTime DtV30Switch(2013, 3, 20, 17, 20),
			DtV40Switch(2014, 5, 5, 14, 26, 40);


class PPCoinBlockObj : public PosBlockObj {
	typedef PPCoinBlockObj class_type;
	typedef PosBlockObj base;
public:

	PPCoinBlockObj() {
	}

	bool IsV02Protocol(const DateTime& dt) const {
		return dt < DtV30Switch;
	}

	void CheckCoinStakeTimestamp() override {
		DateTime dtTx = GetTxObj(1).Timestamp;
		if (!IsV02Protocol(dtTx))
			base::CheckCoinStakeTimestamp();
		else if (dtTx > Timestamp || Timestamp > dtTx+TimeSpan::FromSeconds(MAX_FUTURE_SECONDS)) 
			Throw(E_COIN_TimestampViolation);
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
		return Timestamp>=DtV40Switch ? base::StakeEntropyBit()
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
		if (Db->CheckUserVersion() < VER_PPCOIN_STAKE_MODIFIER)
			Db->Recreate();
		base::TryUpgradeDb();
	}

	BlockObj *CreateBlockObj() override { return new PPCoinBlockObj; }
};

static class PPCoinChainParams : public ChainParams {
	typedef ChainParams base;
public:
	PPCoinChainParams(bool)
		:	base("PPCoin", false)
	{	
//!!!R		MaxPossibleTarget = Target(0x1D00FFFF);
		ChainParams::Add(_self);
	}

	PPCoinEng *CreateEng(CoinDb& cdb) override { return new PPCoinEng(cdb); }
} s_ppcoinParams(true);




} // Coin::

