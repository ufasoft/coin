/*######   Copyright (c) 2012-2015 Ufasoft  http://ufasoft.com  mailto:support@ufasoft.com,  Sergey Pavlov  mailto:dev@ufasoft.com ####
#                                                                                                                                     #
# 		See LICENSE for licensing information                                                                                         #
#####################################################################################################################################*/

#include <el/ext.h>

#include "../eng.h"
#include "../script.h"
#include "../coin-model.h"

namespace Coin {

class COIN_CLASS DevCoinEng : public CoinEng {
	typedef CoinEng base;
public:
	DevCoinEng(CoinDb& cdb)
		:	base(cdb)
	{}
protected:
	int64_t GetMinRelayTxFee() override { return ChainParams.MinTxFee; }

	void UpdateMinFeeForTxOuts(int64_t& minFee, const int64_t& baseFee, const Tx& tx) override {
		minFee += baseFee / 10 * tx.TxOuts().size();
	}

	Target GetNextTargetRequired(const BlockHeader& headerLast, const Block& block)  override {
		seconds targetSpan = hours(24 * (headerLast.Height < 10700 ? 14 : 1));
		int nInterval = int(targetSpan / ChainParams.BlockSpan);
		if (headerLast.Height < 10 || headerLast.Height < 10700 && ((headerLast.Height+1) % nInterval != 0))
			return headerLast.DifficultyTarget;
		BigInteger avg;
		vector<DateTime> vTimes;
		BlockHeader prev = headerLast;
		for (int i=0; i<nInterval-1; ++i, prev=prev.GetPrevHeader()) {
			avg += BigInteger(prev.get_DifficultyTarget());
			vTimes.push_back(prev.Timestamp);
		}
		TimeSpan spanActual = headerLast.get_Timestamp() - prev.get_Timestamp();
		if (headerLast.Height <= 10800)
			avg = headerLast.get_DifficultyTarget();
		else {
			avg /= (nInterval-1);
			sort(vTimes.begin(), vTimes.end());
			spanActual = (vTimes[vTimes.size()-6]-vTimes[6]) * double((nInterval-1)/(vTimes.size()-12));
		}			
		spanActual = std::min(std::max(spanActual, TimeSpan(targetSpan/4)), TimeSpan(targetSpan*4));
		return Target(avg * spanActual.count() / TimeSpan(targetSpan).count());
	}
};

static CurrencyFactory<DevCoinEng> s_devcoin("DevCoin");

} // Coin::

