/*######   Copyright (c) 2015 Ufasoft  http://ufasoft.com  mailto:support@ufasoft.com,  Sergey Pavlov  mailto:dev@ufasoft.com ####
#                                                                                                                                     #
# 		See LICENSE for licensing information                                                                                         #
#####################################################################################################################################*/

// GroestlCoin Crypto-currency
// http://groestlcoin.org


#include <el/ext.h>

#include "../eng.h"

namespace Coin {

class GroestlBlockObj : public BlockObj {
	typedef BlockObj base;
public:
	GroestlBlockObj() {
		Ver = 112;
	}
protected:
};


class COIN_CLASS GroestlCoinEng : public CoinEng {
	typedef CoinEng base;
public:
	GroestlCoinEng(CoinDb& cdb)
		:	base(cdb)
	{
		MaxBlockVersion = 112;
	}
protected:
	BlockObj *CreateBlockObj() override { return new GroestlBlockObj; }

	int64_t GetSubsidy(int height, const HashValue& prevBlockHash, double difficulty, bool bForCheck) override {
		static const double logBase6percent = log(0.94),
			logBase10percent = log(0.9),
			logBase1percent = log(0.99);
		int64_t r = height>=150000 ? int64_t(25 * ChainParams.CoinValue * exp(logBase1percent * ((height-150000) / 10080)))
			: height>=120000 ? int64_t(250 * ChainParams.CoinValue * exp(logBase10percent * ((height-120000) / 1440)))
			: height>1 ? (max)(5*ChainParams.CoinValue, int64_t(512 * ChainParams.CoinValue * exp(logBase6percent * (height / 10080))))
			: height==1 ? 240640 * ChainParams.CoinValue
			: ChainParams.CoinValue;
		return (bForCheck ? 1 : -1) + r;
	}

	HashValue HashFromTx(const Tx& tx) override {
		return SHA256().ComputeHash(EXT_BIN(tx));
	}

	HashValue HashForSignature(const ConstBuf& cbuf) override {
		return SHA256().ComputeHash(cbuf);
	}

	HashValue HashMessage(const ConstBuf& cbuf) override {
		return GroestlHash(cbuf);		// OP_HASH256 implementation
	}

	Target DarkGravityWave(const Block& blockLast, const Block& block, bool bWave3) {
		const int minBlocks = bWave3 ? 24 : 12,
			maxBlocks = bWave3 ? 24 : 120;

		if (blockLast.Height < minBlocks)
			return ChainParams.MaxTarget;
		int actualSeconds = 0;
		BigInteger average;
		Block b = blockLast;
		int64_t secAvg = 0, sum2 = 0;
		int mass = 1;
		DateTime dtPrev = b.Timestamp;
		for (; b.Height>0 && (maxBlocks<=0 || mass<=maxBlocks); ++mass, b=GetPrevBlockPrefixSuffixFromMainTree(b)) {
			DateTime dt = b.Timestamp;
			if (bWave3) {
				if (mass <= minBlocks) {
					average = mass==1 ? BigInteger(b.get_DifficultyTarget())
						: (BigInteger(b.get_DifficultyTarget()) + average*mass)/(mass + 1);
				}
			} else {
				if (mass <= minBlocks)
					average += (BigInteger(b.get_DifficultyTarget()) - average)/mass;
				int diff = (max)(0, (int)duration_cast<seconds>(dtPrev - dt).count());
				if (mass>1 && mass<=minBlocks+2)
					secAvg += (diff - secAvg)/(mass-1);
				sum2 += diff;
			}
			dtPrev = dt;
		}
		
		int count = mass - 1,
			count2 = count - 1;
		int secTarget = int(ChainParams.BlockSpan.count() * count);
		bool bAdjustAverage = bWave3;

		if (bWave3)
			actualSeconds = int(duration_cast<seconds>(blockLast.Timestamp - dtPrev).count());
		else if (min(minBlocks+2, count)!=0 && count2!=0) {
			bAdjustAverage = true;
			double shift = ChainParams.BlockSpan.count() / (max)(1., secAvg * 0.7 + sum2/count2 * 0.3);
			actualSeconds = int(secTarget/shift);
		}
		if (bAdjustAverage)
			average = average * clamp(actualSeconds, secTarget/3, secTarget*3) / secTarget;

		return Target(average);
	}

	Target GetNextTargetRequired(const Block& blockLast, const Block& block) override {
		return DarkGravityWave(blockLast, block, blockLast.Height >= 99999);
	}

};



static CurrencyFactory<GroestlCoinEng> s_groestlcoin("GroestlCoin");

} // Coin::



