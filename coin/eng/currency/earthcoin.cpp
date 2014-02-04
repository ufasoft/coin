#include <el/ext.h>

#include "eng.h"


namespace Coin {

static const double MATH_M_PI = 3.14159265358979323846;

class EarthBlockObj : public BlockObj {
	typedef BlockObj base;
public:
	EarthBlockObj() {
		Ver = 1;
	}
protected:
};

class EarthTxObj : public TxObj {
	typedef TxObj base;
public:
	String Comment;

protected:
	EarthTxObj *Clone() const override { return new EarthTxObj(_self); }
	String GetComment() const override { return Comment; }

	void WriteSuffix(BinaryWriter& wr) const {
		if (Ver >= 2)
			WriteString(wr, Comment);
	}

	void ReadSuffix(const BinaryReader& rd) {
		if (Ver >= 2)
			Comment = ReadString(rd);
	}
};

class EarthCoinEng : public CoinEng {
	typedef CoinEng base;
public:
	EarthCoinEng(CoinDb& cdb)
		:	base(cdb)
	{
	}
protected:
	bool MiningAllowed() override { return false; }

	Int64 GetSubsidy(int height, const HashValue& prevBlockHash, double difficulty, bool bForCheck) override {
		if (height == 1)
			return ChainParams.MaxMoney / 50;
		const int blocksPerDay = int(TimeSpan::FromDays(1).get_TotalSeconds() / ChainParams.BlockSpan.get_TotalSeconds());
		Int64 r = ChainParams.InitBlockValue + int(2000 * sin(double(height) / (365 * blocksPerDay) * 2 * MATH_M_PI)) * ChainParams.CoinValue;
		switch (int day = height / blocksPerDay + 1) {
		case 1:	r *= 5; break;
		case 2:	r *= 3; break;
		case 3:	r *= 2; break;
		default:
			if (day % 31 == 0)
				r *= 5;
			else if (day % 14 == 0)
				r *= 2;
		}
		return std::max(ChainParams.CoinValue, r >> (height / ChainParams.HalfLife));
	}

	Target GetNextTargetRequired(const Block& blockLast, const Block& block) override {
		if (blockLast.Height < 2)
			return ChainParams.MaxTarget;
		const int blockSpanSeconds = int(ChainParams.BlockSpan.get_TotalSeconds());
		int seconds = int((blockLast.Timestamp - blockLast.GetPrevBlock().Timestamp).get_TotalSeconds());
		TimeSpan span = TimeSpan::FromSeconds(clamp(seconds, blockSpanSeconds/16, blockSpanSeconds*16));
		return Target(BigInteger(blockLast.get_DifficultyTarget()) * ((ChainParams.TargetInterval - 1) * ChainParams.BlockSpan.get_Ticks() + 2 * span.get_Ticks()) / ((ChainParams.TargetInterval + 1) * ChainParams.BlockSpan.get_Ticks()));
	}

	BlockObj *CreateBlockObj() override { return new EarthBlockObj; }
	TxObj *CreateTxObj() override { return new EarthTxObj; }
};

static class EarthCoinChainParams : public ChainParams {
	typedef ChainParams base;
public:
	EarthCoinChainParams(bool)
		:	base("EarthCoin", false)
	{	
		ChainParams::Add(_self);
	}
protected:
	EarthCoinEng *CreateObject(CoinDb& cdb, IBlockChainDb *db) override {
		EarthCoinEng *r = new EarthCoinEng(cdb);
		r->SetChainParams(_self, db);
		return r;
	}
} s_earthCoinParams(true);


} // Coin::


