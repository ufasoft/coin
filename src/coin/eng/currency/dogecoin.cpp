#include <el/ext.h>

#ifndef UCFG_COIN_DOGECOIN
#	define UCFG_COIN_DOGECOIN 1
#endif

#if UCFG_COIN_DOGECOIN

#define UUID_AA15E74A856F11E08B8D93F24824019B	//!!!T
#include <boost/random/mersenne_twister.hpp>
#include <boost/random/uniform_int_distribution.hpp>

#include "../eng.h"
#include "../script.h"
#include "../coin-model.h"


namespace Coin {

static int GenerateMTRandom(unsigned int s, int range) {
	boost::random::uniform_int_distribution<> dist(1, range);
	boost::random::mt19937 eng(s);
	return dist(eng) + 1;				//!!! VC std:: <random> gives different results
}

class COIN_CLASS DogeCoinEng : public CoinEng {
	typedef CoinEng base;
public:
	DogeCoinEng(CoinDb& cdb)
		:	base(cdb)
	{
		MaxBlockVersion = 6;
	}
protected:
	bool MiningAllowed() override { return false; }

	int64_t GetSubsidy(int height, const HashValue& prevBlockHash, double difficulty, bool bForCheck) override {
		return ChainParams.CoinValue * (
			height >= 600000 ? 10000
			: height >=145000 ? 500000 >> (height / 100000)
			: GenerateMTRandom(int(letoh(*(uint64_t*)(prevBlockHash.data()+24)) >> 8) & 0xFFFFFFF, height>=100000 ? 499999 : 999999));
	}

	int GetIntervalForModDivision(int height) override {
		return height<144999 ? 240 : base::GetIntervalForModDivision(height);
	}

	int GetIntervalForCalculatingTarget(int height) override {
		return GetIntervalForModDivision(height);
	}

	TimeSpan AdjustSpan(int height, const TimeSpan& span, const TimeSpan& targetSpan) override {
		if (height > 144998) {
			seconds secTarget = duration_cast<seconds>(targetSpan),
				secActual = duration_cast<seconds>(span);
			TimeSpan spanAct = secTarget + (secActual-secTarget)/8;
			return clamp(spanAct, targetSpan - targetSpan/4, targetSpan + targetSpan/2);
		}
		return height+1 > 10000 ? base::AdjustSpan(height, span, targetSpan)
			: height+1 > 5000 ? clamp(span, targetSpan/8, targetSpan*4)
			: clamp(span, targetSpan/16, targetSpan*4);
	}
};

static CurrencyFactory<DogeCoinEng> s_dogecoin("DogeCoin");

} // Coin::

#endif // UCFG_COIN_DOGECOIN
