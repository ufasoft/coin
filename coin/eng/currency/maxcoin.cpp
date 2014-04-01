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
	bool MiningAllowed() override { return false; }
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
		TimeSpan minPast = TimeSpan::FromDays(1) / 100,
			maxPast = TimeSpan::FromDays(1) / 100 * 14;
		return KimotoGravityWell(blockLast, block, int(minPast.get_TotalSeconds()/ChainParams.BlockSpan.get_TotalSeconds()), int(maxPast.get_TotalSeconds()/ChainParams.BlockSpan.get_TotalSeconds()));
	}
};

static class MaxCoinChainParams : public ChainParams {
	typedef ChainParams base;
public:
	MaxCoinChainParams(bool)
		:	base("MaxCoin", false)
	{	
		ChainParams::Add(_self);
	}

	MaxCoinEng *CreateEng(CoinDb& cdb) override { return new MaxCoinEng(cdb); }
} s_maxCoinParams(true);


} // Coin::


