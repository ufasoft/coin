/*######   Copyright (c) 2013-2015 Ufasoft  http://ufasoft.com  mailto:support@ufasoft.com,  Sergey Pavlov  mailto:dev@ufasoft.com ####
#                                                                                                                                     #
# 		See LICENSE for licensing information                                                                                         #
#####################################################################################################################################*/

#include <el/ext.h>

#include "eng.h"

namespace Coin {

class MaxBlockObj : public BlockObj {
	typedef BlockObj base;
public:
	MaxBlockObj() {
		Ver = 112;
	}
protected:
};

class MaxCoinEng : public CoinEng {
	typedef CoinEng base;
public:
	MaxCoinEng(CoinDb& cdb)
		:	base(cdb)
	{
		MaxBlockVersion = 112;
	}
protected:
	BlockObj *CreateBlockObj() override { return new MaxBlockObj; }

	HashValue HashMessage(const ConstBuf& cbuf) override {
		return SHA3<256>().ComputeHash(cbuf);
	}

	HashValue HashBuf(const ConstBuf& cbuf) override {
		return SHA256().ComputeHash(cbuf);
	}

	Target GetNextTargetRequired(const Block& blockLast, const Block& block) override {
		if (blockLast.Height < 199)
			return base::GetNextTargetRequired(blockLast, block);
		seconds minPast = seconds(hours(24)) / 100,
			maxPast = seconds(hours(1)) / 100 * 14;
		return KimotoGravityWell(blockLast, block, int(minPast / ChainParams.BlockSpan), int(maxPast / ChainParams.BlockSpan));
	}
};

static CurrencyFactory<MaxCoinEng> s_maxcoin("MaxCoin");

} // Coin::


