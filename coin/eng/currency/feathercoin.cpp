#include <el/ext.h>

#include "../eng.h"


namespace Coin {

class COIN_CLASS FeatherCoinEng : public CoinEng {
	typedef CoinEng base;
public:
	FeatherCoinEng(CoinDb& cdb)
		:	base(cdb)
	{
	}
protected:
	Target GetNextTargetRequired(const Block& blockLast, const Block& block) override {
		int h = blockLast.Height+1;
		int nInterval = h < 33000 ? 504*4
			: h < 87948 ? 504
			: 126;
		if (h % nInterval && h != 33000 && h != 87948)
			return blockLast.DifficultyTarget;
		
		Block prev = blockLast;
		for (int goback = nInterval - int(h==nInterval); goback--;)
			prev = prev.GetPrevBlock();
		int targetSpan = int((ChainParams.BlockSpan * nInterval).get_TotalSeconds()),
			spanActual = int((blockLast.get_Timestamp() - prev.get_Timestamp()).get_TotalSeconds());
		if (h >= 87948) {																					// Additional averaging over 4x nInterval window
			prev = blockLast;
			for (int goback = nInterval*4; goback--;)
				prev = prev.GetPrevBlock();
			int spanActualLong = int((blockLast.get_Timestamp() - prev.get_Timestamp()).get_TotalSeconds())/4;
	        spanActual = ((spanActual + spanActualLong)/2 + 3*targetSpan) / 4;
		}
		spanActual = h < 33000 ? clamp(spanActual, targetSpan/4, targetSpan*4)
			: h < 87948 ? clamp(spanActual, targetSpan*70/99, targetSpan*99/70)
			: clamp(spanActual, targetSpan*453/494, targetSpan*494/453);		
		return Target(BigInteger(blockLast.get_DifficultyTarget())*spanActual/targetSpan);
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

