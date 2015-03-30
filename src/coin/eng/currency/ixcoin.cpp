/*######   Copyright (c) 2012-2015 Ufasoft  http://ufasoft.com  mailto:support@ufasoft.com,  Sergey Pavlov  mailto:dev@ufasoft.com ####
#                                                                                                                                     #
# 		See LICENSE for licensing information                                                                                         #
#####################################################################################################################################*/

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
		seconds ts = duration_cast<seconds>(span),
			twoPercent = duration_cast<seconds>(targetSpan / 50);
		return base::AdjustSpan(height,
			height<20055 || span>=targetSpan ? span : twoPercent * (ts<twoPercent*16 ? 45 : ts<twoPercent*32 ? 47 : 49),
			targetSpan);
	}
};

static CurrencyFactory<IXCoinEng> s_ixcoin("iXcoin");

} // Coin::

