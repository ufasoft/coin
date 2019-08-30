/*######   Copyright (c) 2015-2019 Ufasoft  http://ufasoft.com  mailto:support@ufasoft.com,  Sergey Pavlov  mailto:dev@ufasoft.com ####
#                                                                                                                                     #
# 		See LICENSE for licensing information                                                                                         #
#####################################################################################################################################*/

// Groestlcoin Crypto-currency
// http://groestlcoin.org


#include <el/ext.h>

#include "../eng.h"

namespace Coin {

class GroestlBlockObj : public BlockObj {
	typedef BlockObj base;
public:
	GroestlBlockObj() {
	}
};


class COIN_CLASS GroestlcoinEng : public CoinEng {
	typedef CoinEng base;
public:
	GroestlcoinEng(CoinDb& cdb)
		: base(cdb)
	{
		MaxBlockVersion = 112;
	}
protected:
	BlockObj *CreateBlockObj() override { return new GroestlBlockObj; }

	int64_t GetSubsidy(int height, const HashValue& prevBlockHash, double difficulty, bool bForCheck) override {
		static const double logBase6percent = log(0.94),
			logBase10percent = log(0.9),
			logBase1percent = log(0.99);
		int64_t r =
			height >= 1772880 ? 5 * ChainParams.CoinValue
			: height >= 150000 ? int64_t(25 * ChainParams.CoinValue * exp(logBase1percent * ((height - 150000) / 10080)))
			: height >= 120000 ? int64_t(250 * ChainParams.CoinValue * exp(logBase10percent * ((height - 120000) / 1440)))
			: height > 1 ? (max)(5 * ChainParams.CoinValue, int64_t(512 * ChainParams.CoinValue * exp(logBase6percent * (height / 10080)))) : height == 1 ? 240640 * ChainParams.CoinValue : ChainParams.CoinValue;
		return (bForCheck ? 1 : -44) + r;			// max difference with reference implementation is 43 until Block #10,000,000
	}

	HashValue HashFromTx(const Tx& tx, bool widnessAware) override {
        MemoryStream stm;
		ProtocolWriter wr(stm);
		wr.WitnessAware = widnessAware;
        tx.Write(wr);
		return SHA256().ComputeHash(stm);
	}

	HashValue HashForSignature(RCSpan cbuf) override {
		return SHA256().ComputeHash(cbuf);
	}

	HashValue HashMessage(RCSpan cbuf) override {
		return GroestlHash(cbuf);		// OP_HASH256 implementation
	}

	HashValue HashForWallet(RCSpan s) override {
		return GroestlHash(s);
	}

	HashValue HashForAddress(RCSpan cbuf) override {
		return GroestlHash(cbuf);
	}

	Target DarkGravityWave(const BlockHeader& headerLast, const Block& block, bool bWave3) {
		if (!bWave3)
			return block->get_DifficultyTarget();			// workaround of DarkGravityWave() v1 FP-non-determinism

		const int minBlocks = bWave3 ? 24 : 12,
			maxBlocks = bWave3 ? 24 : 120;

		if (headerLast.Height < minBlocks)
			return ChainParams.MaxTarget;
		int actualSeconds = 0;
		BigInteger average;
		BlockHeader b = headerLast;
		int64_t secAvg = 0, sum2 = 0;
		int mass = 1;
		DateTime dtPrev = b.Timestamp;
		for (; b.Height > 0 && (maxBlocks <= 0 || mass <= maxBlocks); ++mass, b = b.GetPrevHeader()) {
			DateTime dt = b.Timestamp;
			if (bWave3) {
				if (mass <= minBlocks) {
					average = mass==1 ? BigInteger(b->get_DifficultyTarget())
						: (BigInteger(b->get_DifficultyTarget()) + average*mass) / (mass + 1);
				}
			} else {
				if (mass <= minBlocks)
					average += (BigInteger(b->get_DifficultyTarget()) - average) / mass;
				int diff = (max)(0, (int)duration_cast<seconds>(dtPrev - dt).count());
				if (mass > 1 && mass <= minBlocks + 2)
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
			actualSeconds = int(duration_cast<seconds>(headerLast.Timestamp - dtPrev).count());
		else if (min(minBlocks+2, count)!=0 && count2!=0) {
			bAdjustAverage = true;
			double shift = ChainParams.BlockSpan.count() / (max)(1., secAvg * 0.7 + sum2/count2 * 0.3);
			actualSeconds = int(secTarget/shift);
		}
		if (bAdjustAverage)
			average = average * clamp(actualSeconds, secTarget / 3, secTarget * 3) / secTarget;

		return Target(average);
	}

	Target GetNextTargetRequired(const BlockHeader& headerLast, const Block& block) override {
		if (ChainParams.IsTestNet) {
			if (block.get_Timestamp() - headerLast.Timestamp > ChainParams.BlockSpan * 2) // negative block timestamp delta is allowed
				return ChainParams.MaxTarget;
		}
		return DarkGravityWave(headerLast, block, headerLast.Height >= 99999);
	}

};

static CurrencyFactory<GroestlcoinEng> s_groestlcoin("Groestlcoin"), s_groestlcoinTestnet("Groestlcoin-testnet");

#if UCFG_COIN_GENERATE
	class GroestlHasher;
	extern GroestlHasher g_groestlHasher;
	static GroestlHasher* s_pGroestlHasher = &g_groestlHasher;		// Reference to link GroestlHasher into static build
#endif // UCFG_COIN_GENERATE

} // Coin::
