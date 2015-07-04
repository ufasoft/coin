/*######   Copyright (c) 2013-2015 Ufasoft  http://ufasoft.com  mailto:support@ufasoft.com,  Sergey Pavlov  mailto:dev@ufasoft.com ####
#                                                                                                                                     #
# 		See LICENSE for licensing information                                                                                         #
#####################################################################################################################################*/

#include <el/ext.h>

#include "../eng.h"


namespace Coin {

const Target TERRACOIN_FIVE_THOUSAND_LIMIT(0x1B0C7898);

class COIN_CLASS TerraCoinEng : public CoinEng {
	typedef CoinEng base;
public:
	TerraCoinEng(CoinDb& cdb)
		:	base(cdb)
	{
		Caches.HashToBlockCache.SetMaxSize(2165);
	}
protected:
	int GetIntervalForModDivision(int height) override {
		return height > 192237 ? base::GetIntervalForModDivision(height)
			: height > 181200 ? 2160
			: 30;
	}

	int GetIntervalForCalculatingTarget(int height) override {
		int r = base::GetIntervalForCalculatingTarget(height);
		return height > 192237 ? r
			: height > 181200 ? 2160
			: height > 101908 ? 90
			: height > 99988 ? 720
			: 30; 
	}

	TimeSpan GetActualSpanForCalculatingTarget(const BlockObj& bo, int nInterval) override {
		seconds r = duration_cast<seconds>(base::GetActualSpanForCalculatingTarget(bo, nInterval + int(bo.Height>99988)));
		return bo.Height > 181200 ? r
			: bo.Height > 101908 ? r / 3 * 3
			: bo.Height > 99988 ? r / 24 * 24
			: r;
	}

	TimeSpan AdjustSpan(int height, const TimeSpan& span, const TimeSpan& targetSpan) override {
		if (height <= 192237)
			return base::AdjustSpan(height, span, targetSpan);
		int targetSeconds = (int)duration_cast<seconds>(targetSpan).count();
		return clamp(duration_cast<seconds>(span), seconds(int(targetSeconds/1.25)), seconds(int(targetSeconds*1.25)));
	}

	Target GetNextTargetRequired(const BlockHeader& headerLast, const Block& block) override {
		Target r;
		if (headerLast.Height > 181200) {
			r = base::GetNextTargetRequired(headerLast, block);
			if (headerLast.Height < 220000)
				r = std::min(r, TERRACOIN_FIVE_THOUSAND_LIMIT);
			return r;
		}

		if (headerLast.Height <= 101631)
			return base::GetNextTargetRequired(headerLast, block);

		if (block.get_Timestamp()-headerLast.Timestamp > ChainParams.BlockSpan*10 && headerLast.Height <= 175000)
			r = Target(BigInteger(headerLast.get_DifficultyTarget()) * (headerLast.Height>101631 && headerLast.Height<103791 ? 10 : 2));
		else {
			const seconds PER_BLOCK_SPAN = duration_cast<seconds>(ChainParams.BlockSpan);
			vector<seconds> durations(2160);
			BlockHeader b = headerLast;
			for (vector<seconds>::reverse_iterator it=durations.rbegin(), e=durations.rend(); it!=e; ++it) {
				BlockHeader prev = b.GetPrevHeader();
				seconds dur = duration_cast<seconds>(exchange(b, prev).get_Timestamp()-prev.get_Timestamp());
				if (headerLast.Height > 110322) {
					dur = std::min(PER_BLOCK_SPAN*3/2, dur);
					if (dur.count() >= 0)
						dur = std::max(PER_BLOCK_SPAN/2, dur);
				}
				*it = dur.count()<0 && headerLast.Height>104290 ? PER_BLOCK_SPAN : dur;
			}
			float acc = (float)PER_BLOCK_SPAN.count();
			for (int i=0; i<durations.size(); ++i)
				acc = headerLast.Height>110322 ? 0.06f*durations[i].count() + (1-0.06f)*acc				// float/double conversions are not well defined. Another way to calculate diverges with reference TerraCoin implementation
									: 0.09f*durations[i].count() + (1-0.09f)*acc;
			seconds actualSpan = clamp(seconds((int)acc), PER_BLOCK_SPAN/2, PER_BLOCK_SPAN * (headerLast.Height>110322 ? 2 : 4));
			r = Target(BigInteger(headerLast.get_DifficultyTarget()) * actualSpan.count() / PER_BLOCK_SPAN.count());
		}
		return headerLast.Height>104290 ? std::min(r, TERRACOIN_FIVE_THOUSAND_LIMIT) : r;
	}
};

static CurrencyFactory<TerraCoinEng> s_terracoin("TerraCoin");

} // Coin::

