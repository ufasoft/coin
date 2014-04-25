#include <el/ext.h>

#include "../eng.h"

namespace Coin {

class IXCoinEng : public CoinEng {
	typedef CoinEng base;
public:
	IXCoinEng(CoinDb& cdb)
		:	base(cdb)
	{}
protected:
	int GetIntervalForModDivision(int height) override {
		return height<20055 ? 2016 : base::GetIntervalForModDivision(height);
	}

	int GetIntervalForCalculatingTarget(int height) override {
		return GetIntervalForModDivision(height);
	}

	TimeSpan GetActualSpanForCalculatingTarget(const BlockObj& bo, int nInterval) override {
		return bo.Timestamp - GetBlockByHeight(bo.Height - nInterval + int(bo.Height < 43000)).Timestamp;
	}

	TimeSpan AdjustSpan(int height, const TimeSpan& span, const TimeSpan& targetSpan) override {
		int ts = int(span.get_TotalSeconds()),
			twoPercent = int(targetSpan.get_TotalSeconds()) / 50;
		return base::AdjustSpan(height,
			height<20055 || span>=targetSpan ? span : TimeSpan::FromSeconds(twoPercent * (ts<twoPercent*16 ? 45 : ts<twoPercent*32 ? 47 : 49)),
			targetSpan);
	}
};

static class IXCoinChainParams : public ChainParams {
	typedef ChainParams base;
public:
	IXCoinChainParams(bool)
		:	base("iXcoin", false)
	{	
		ChainParams::Add(_self);
	}

	IXCoinEng *CreateEng(CoinDb& cdb) override { return new IXCoinEng(cdb); }
} s_ixcoinParams(true);

} // Coin::

