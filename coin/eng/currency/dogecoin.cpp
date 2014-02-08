/*######     Copyright (c) 1997-2014 Ufasoft  http://ufasoft.com  mailto:support@ufasoft.com,  Sergey Pavlov  mailto:dev@ufasoft.com #######################################
#                                                                                                                                                                          #
# This program is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation;  #
# either version 3, or (at your option) any later version. This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the      #
# implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details. You should have received a copy of the GNU #
# General Public License along with this program; If not, see <http://www.gnu.org/licenses/>                                                                               #
##########################################################################################################################################################################*/

#include <el/ext.h>

#include <boost/random/mersenne_twister.hpp>
#include <boost/random/uniform_int_distribution.hpp>

#include "../eng.h"
#include "../script.h"
#include "../coin-model.h"


namespace Coin {

static int GenerateMTRandom(unsigned int s, int range) {
	return boost::random::uniform_int_distribution<>(1, range)(boost::random::mt19937(s)) + 1;				//!!! VC std:: <random> gives different results
}

class COIN_CLASS DogeCoinEng : public CoinEng {
	typedef CoinEng base;
public:
	DogeCoinEng(CoinDb& cdb)
		:	base(cdb)
	{
	}
protected:
	bool MiningAllowed() override { return false; }

	Int64 GetSubsidy(int height, const HashValue& prevBlockHash, double difficulty, bool bForCheck) override {
		long seed7 = int(letoh(*(UInt64*)(prevBlockHash.data()+24)) >> 8) & 0xFFFFFFF,		//!!!TODO O
			seed6 = int(letoh(*(UInt64*)(prevBlockHash.data()+24)) >> 12) & 0xFFFFFFF;
		Int64 nSubsidy =
			  height < 100000 ? GenerateMTRandom(seed7, 999999)
			: height < 200000 ? GenerateMTRandom(seed7, 499999)
			: height < 300000 ? GenerateMTRandom(seed6, 249999)
			: height < 400000 ? GenerateMTRandom(seed7, 124999)
			: height < 500000 ? GenerateMTRandom(seed7, 62499)
			: height < 600000 ? GenerateMTRandom(seed6, 31249)
			: 10000;
		return nSubsidy  * ChainParams.CoinValue;
	}

	TimeSpan AdjustSpan(int height, const TimeSpan& span, const TimeSpan& targetSpan) override {
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

protected:
	DogeCoinEng *CreateObject(CoinDb& cdb, IBlockChainDb *db) override {
		DogeCoinEng *r = new DogeCoinEng(cdb);
		r->SetChainParams(_self, db);
		return r;
	}

} s_dogeCoinParams(true);


} // Coin::


