/*######     Copyright (c) 1997-2015 Ufasoft  http://ufasoft.com  mailto:support@ufasoft.com,  Sergey Pavlov  mailto:dev@ufasoft.com #########################################################################################################
#                                                                                                                                                                                                                                            #
# This program is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation;  either version 3, or (at your option) any later version.          #
# This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.   #
# You should have received a copy of the GNU General Public License along with this program; If not, see <http://www.gnu.org/licenses/>                                                                                                      #
############################################################################################################################################################################################################################################*/

#include <el/ext.h>

#include "../eng.h"


namespace Coin {

static const DateTime DATE_SWITCH_V2(2014, 10, 22);
static const Target FEATHER_NEO_SCRYPT_LIMIT(0x1D3FFFFF);

class FeatherCoinBlockObj : public BlockObj {
	typedef BlockObj base;
protected:
	HashValue PowHash() const override {
		return NeoScryptHash(EXT_BIN(Ver << PrevBlockHash << MerkleRoot() << (uint32_t)to_time_t(Timestamp) << get_DifficultyTarget() << Nonce),
			Timestamp>=DATE_SWITCH_V2 && int(Height != uint32_t(-1) ? Height : GetBlockHeightFromCoinbase())>=432000 ? 0 : 3);
	}
};

class COIN_CLASS FeatherCoinEng : public CoinEng {
	typedef CoinEng base;
public:
	FeatherCoinEng(CoinDb& cdb)
		:	base(cdb)
	{
		Caches.HashToBlockCache.SetMaxSize(512);
	}
protected:
	BlockObj *CreateBlockObj() override { return new FeatherCoinBlockObj; }


	int64_t GetSubsidy(int height, const HashValue& prevBlockHash, double difficulty, bool bForCheck) override {
		return (ChainParams.CoinValue * (height >= 204639 ? 80 : 200))
			>> ((height - 306960)/ChainParams.HalfLife);
	}

	Target GetNextTargetRequired(const Block& blockLast, const Block& block) override {
		int h = blockLast.Height+1;
		
		if (h == 432000)
			return FEATHER_NEO_SCRYPT_LIMIT;

		int nInterval = h >= 204639 ? 1
			: h >= 87948 ? 126
			: h >= 33000 ? 504
			: 2016;
		
		if (h % nInterval && h != 33000 && h != 87948)
			return blockLast.DifficultyTarget;
		
		Block prev = blockLast;
		for (int goback = nInterval - int(h==nInterval); goback--;)
			prev = prev.GetPrevBlock();
		int targetSpan = int(((h >= 204639 ? ChainParams.BlockSpan : TimeSpan::FromSeconds(150)) * nInterval).get_TotalSeconds()),
			spanActual = int((blockLast.get_Timestamp() - prev.get_Timestamp()).get_TotalSeconds());
		if (h >= 204639) {
			prev = blockLast;
			DateTime dtShortTime, dtMediumTime;
			for (int goback=nInterval*480, i=0; i<goback && i<h-1; ++i) {
				prev = prev.GetPrevBlock();
				if (i == 14)
					dtShortTime = prev.Timestamp;
				else if (i == 119)
					dtMediumTime = prev.Timestamp;
			}
			int spanShort = int((blockLast.get_Timestamp() - dtShortTime).get_TotalSeconds()) / 15,
				spanMedium = int((blockLast.get_Timestamp() - dtMediumTime).get_TotalSeconds()) / 120,
				spanLong = int((blockLast.get_Timestamp() - prev.get_Timestamp()).get_TotalSeconds()) / 480;
			spanActual = ((spanShort + spanMedium + spanLong)/3 + 3*targetSpan) / 4;
		} else if (h >= 87948) {																					// Additional averaging over 4x nInterval window
			prev = blockLast;
			for (int goback = nInterval*4; goback--;)
				prev = prev.GetPrevBlock();
			int spanActualLong = int((blockLast.get_Timestamp() - prev.get_Timestamp()).get_TotalSeconds()) / 4;
	        spanActual = ((spanActual + spanActualLong)/2 + 3*targetSpan) / 4;
		}
		pair<int, int> md = h >= 87948 ? make_pair(453, 494)
			: h >= 33000 ? make_pair(70, 99)
			: make_pair(1, 4);
		return Target(BigInteger(blockLast.get_DifficultyTarget()) * clamp(spanActual, targetSpan*md.first/md.second, targetSpan*md.second/md.first) / targetSpan);
	}
};


static class FeatherCoinChainParams : public ChainParams {
	typedef ChainParams base;
public:
	FeatherCoinChainParams(bool)
		:	base("FeatherCoin", false)
	{	
		ChainParams::Add(_self);
	}

	FeatherCoinEng *CreateEng(CoinDb& cdb) override { return new FeatherCoinEng(cdb); }
} s_feathercoinParams(true);


} // Coin::


