/*######   Copyright (c) 2013-2015 Ufasoft  http://ufasoft.com  mailto:support@ufasoft.com,  Sergey Pavlov  mailto:dev@ufasoft.com ####
#                                                                                                                                     #
# 		See LICENSE for licensing information                                                                                         #
#####################################################################################################################################*/

#include <el/ext.h>

#include "../eng.h"


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
//	String Comment;
	Blob CommentBlob;

protected:
	EarthTxObj *Clone() const override { return new EarthTxObj(_self); }

	String GetComment() const override { 
		try {
			return String((const char*)CommentBlob.constData(), CommentBlob.Size);
		} catch (RCExc) {
			return "<#INVALID-STRING>";
		}
	}

	void WriteSuffix(BinaryWriter& wr) const {
		if (Ver >= 2)
			WriteBlob(wr, CommentBlob);
	}

	void ReadSuffix(const BinaryReader& rd) {
		if (Ver >= 2)
			CommentBlob = ReadBlob(rd);
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
	int64_t GetSubsidy(int height, const HashValue& prevBlockHash, double difficulty, bool bForCheck) override {
		if (height == 1)
			return ChainParams.MaxMoney / 50;
		const int blocksPerDay = int(seconds(hours(24)) / ChainParams.BlockSpan);
		int64_t r = ChainParams.InitBlockValue + int(2000 * sin(double(height) / (365 * blocksPerDay) * 2 * MATH_M_PI)) * ChainParams.CoinValue;
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

	Target GetNextTargetRequired(const BlockHeader& headerLast, const Block& block) override {
		if (headerLast.Height < 2)
			return ChainParams.MaxTarget;
		seconds secs = duration_cast<seconds>(headerLast.get_Timestamp() - headerLast.GetPrevHeader().get_Timestamp());
		TimeSpan span = clamp(secs, ChainParams.BlockSpan/16, ChainParams.BlockSpan*16);
		return Target(BigInteger(headerLast.get_DifficultyTarget()) * ((ChainParams.TargetInterval - 1) * TimeSpan(ChainParams.BlockSpan).count() + 2 * span.count()) / ((ChainParams.TargetInterval + 1) * TimeSpan(ChainParams.BlockSpan).count()));
	}

	BlockObj *CreateBlockObj() override { return new EarthBlockObj; }
	TxObj *CreateTxObj() override { return new EarthTxObj; }
};

static CurrencyFactory<EarthCoinEng> s_earthcoin("EarthCoin");

} // Coin::


