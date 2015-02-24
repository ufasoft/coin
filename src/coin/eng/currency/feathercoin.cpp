/*######   Copyright (c) 2011-2015 Ufasoft  http://ufasoft.com  mailto:support@ufasoft.com,  Sergey Pavlov  mailto:dev@ufasoft.com ####
#                                                                                                                                     #
# 		See LICENSE for licensing information                                                                                         #
#####################################################################################################################################*/

#include <el/ext.h>

#include "../eng.h"


namespace Coin {

static const DateTime DATE_SWITCH_V2(2014, 10, 22);
static const Target FEATHER_NEO_SCRYPT_LIMIT(0x1D3FFFFF);

class FeatherCoinBlockObj : public BlockObj {
	typedef BlockObj base;
protected:
	HashValue PowHash() const override {
		return NeoSCryptHash(EXT_BIN(Ver << PrevBlockHash << MerkleRoot() << (uint32_t)to_time_t(Timestamp) << get_DifficultyTarget() << Nonce),
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
		int targetSpan = (int)duration_cast<seconds>(((h >= 204639 ? ChainParams.BlockSpan : TimeSpan::FromSeconds(150)) * nInterval)).count(),
			spanActual = (int)duration_cast<seconds>(blockLast.get_Timestamp() - prev.get_Timestamp()).count();
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
			int spanShort = (int)(duration_cast<seconds>(blockLast.get_Timestamp() - dtShortTime) / 15).count(),
				spanMedium = (int)(duration_cast<seconds>(blockLast.get_Timestamp() - dtMediumTime) / 120).count(),
				spanLong = (int)(duration_cast<seconds>(blockLast.get_Timestamp() - prev.get_Timestamp()) / 480).count();
			spanActual = ((spanShort + spanMedium + spanLong)/3 + 3*targetSpan) / 4;
		} else if (h >= 87948) {																					// Additional averaging over 4x nInterval window
			prev = blockLast;
			for (int goback = nInterval*4; goback--;)
				prev = prev.GetPrevBlock();
			int spanActualLong = (int)(duration_cast<seconds>(blockLast.get_Timestamp() - prev.get_Timestamp()) / 4).count();
	        spanActual = ((spanActual + spanActualLong)/2 + 3*targetSpan) / 4;
		}
		pair<int, int> md = h >= 87948 ? make_pair(453, 494)
			: h >= 33000 ? make_pair(70, 99)
			: make_pair(1, 4);
		return Target(BigInteger(blockLast.get_DifficultyTarget()) * clamp(spanActual, targetSpan*md.first/md.second, targetSpan*md.second/md.first) / targetSpan);
	}
};

static CurrencyFactory<FeatherCoinEng> s_feathercoin("FeatherCoin");

} // Coin::


