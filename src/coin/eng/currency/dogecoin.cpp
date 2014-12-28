/*######     Copyright (c) 1997-2015 Ufasoft  http://ufasoft.com  mailto:support@ufasoft.com,  Sergey Pavlov  mailto:dev@ufasoft.com #########################################################################################################
#                                                                                                                                                                                                                                            #
# This program is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation;  either version 3, or (at your option) any later version.          #
# This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.   #
# You should have received a copy of the GNU General Public License along with this program; If not, see <http://www.gnu.org/licenses/>                                                                                                      #
############################################################################################################################################################################################################################################*/

#include <el/ext.h>

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
			int secTarget = (int)targetSpan.get_TotalSeconds(),
				secActual = (int)span.get_TotalSeconds();
			TimeSpan spanAct = TimeSpan::FromSeconds(secTarget + (secActual-secTarget)/8);
			return clamp(spanAct, targetSpan - targetSpan/4, targetSpan + targetSpan/2);
		}
		return height+1 > 10000 ? base::AdjustSpan(height, span, targetSpan)
			: height+1 > 5000 ? clamp(span, targetSpan/8, targetSpan*4)
			: clamp(span, targetSpan/16, targetSpan*4);
	}
};

static class DogeCoinChainParams : public ChainParams {
	typedef ChainParams base;
public:
	DogeCoinChainParams(bool)
		:	base("DogeCoin", false)
	{	
		ChainParams::Add(_self);
	}

	DogeCoinEng *CreateEng(CoinDb& cdb) override { return new DogeCoinEng(cdb); }
} s_dogeCoinParams(true);


} // Coin::


