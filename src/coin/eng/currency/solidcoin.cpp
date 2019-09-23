#include <el/ext.h>

#include <el/bignum.h>
#include <el/crypto/hash.h>

#include "../coin-protocol.h"
#include "../script.h"
#include "../eng.h"
#include "coin-msg.h"
#include "util.h"

namespace Coin {

const double CPF_PERCENT = 0.05;

class SolidcoinBlockObj : public BlockObj {
	typedef BlockObj base;
public:
    uint64_t Nonce1, Nonce2, Nonce3;
    char MinerId[12];
protected:
	SolidcoinBlockObj *Clone() override { return new SolidcoinBlockObj(*this); }

	void WriteHeader(BinaryWriter& wr) const override {
		wr << Ver << PrevBlockHash << MerkleRoot() << int64_t(Height) << (uint64_t)to_time_t(Timestamp) << Nonce1 << Nonce2 << Nonce3 << Nonce;
		wr.WriteStruct(MinerId);
		wr << DifficultyTarget;
	}

	void ReadHeader(const BinaryReader& rd, bool bParent) override {
		HashValue h;
		rd >> Ver >> PrevBlockHash >> h;
		m_merkleRoot = h;
		Height = (uint32_t)rd.ReadInt64();
		Timestamp = DateTime::from_time_t(rd.ReadUInt64());
		rd >> Nonce1 >> Nonce2 >> Nonce3 >> Nonce;
		rd.ReadStruct(MinerId);
		DifficultyTargetBits = rd.ReadUInt32();
	}

	using base::Write;

	void Write(DbWriter& wr) const override {
		base::Write(wr);
		wr << MerkleRoot() << Nonce1 << Nonce2 << Nonce3;
		wr.WriteStruct(MinerId);
	}

	using base::Read;

	void Read(const DbReader& rd) override {
		base::Read(rd);
		HashValue h;
		rd >> h >> Nonce1 >> Nonce2 >> Nonce3;
		m_merkleRoot = h;
		rd.ReadStruct(MinerId);
	}

	Coin::HashValue Hash() const override {
		if (!m_hash) {
			MemoryStream ms;
			WriteHeader(BinaryWriter(ms));
			m_hash = SolidcoinHash(ms);
		}
		return m_hash.get();
	}

	HashValue HashFromTx(const Tx& tx, int n) const override {
		if (n > 0 || Height<91500 || (Height % 2))
			return base::HashFromTx(tx, n);
		MemoryStream ms;
		ScriptWriter(ms) << BigInteger(Height);
		Tx dtx(tx->Clone());
		dtx->m_txIns.at(0).put_Script(ms);
		dtx->m_nBytesOfHash = 0;
		return Coin::Hash(dtx);
	}

	void Check(bool bCheckMerkleRoot) override {
		if (m_txes.empty())
			Throw(E_FAIL);
		if (!m_txes[0].IsCoinBase())
			Throw(E_COIN_FirstTxIsNotTheOnlyCoinbase);
		CoinEng& eng = Eng();
		if (0 == Height) {
			for (int i=0; i<m_txes.size(); ++i)
				if (!m_txes[i].IsCoinBase())
					Throw(E_COIN_FirstTxIsNotTheOnlyCoinbase);
		} else if (Height % 2) {
			for (int i=1; i<m_txes.size(); ++i)
				if (m_txes[i].IsCoinBase())
					Throw(E_COIN_FirstTxIsNotTheOnlyCoinbase);
		} else {
			if (m_txes.size() < 2)
				Throw(E_FAIL);
			//!!!TODO

		}

		EXT_FOR (const Tx& tx, m_txes) {
			tx.Check();
		}
		if (bCheckMerkleRoot && MerkleRoot(false) != m_merkleRoot.get())
			Throw(E_COIN_MerkleRootMismatch);
	}

	bool IsPower() const {
		CoinEng& eng = Eng();

		HashValue h1 = Coin::Hash(eng.GetBlockByHeight(Height-1)),
				h2 = Coin::Hash(eng.GetBlockByHeight(Height-2));
		byte sum1 = 0, sum2 = 0;
		for (int i=0; i<h1.size(); ++i) {
			sum1 += h1[i];
			sum2 += h2[i];
		}
		return sum1>=128 && sum2>=239;
	}

	void CheckCoinbaseTx(int64_t nFees) override {
		CoinEng& eng = Eng();

		int64_t val = eng.GetSubsidy(Height, PrevBlockHash, eng.ToDifficulty(DifficultyTarget));
		if (Height % 2) {
			if (Height >= 50000 && IsPower())
				val *= 2;
		} else if (Height >= 42000)
			val = eng.GetSubsidy(Height, PrevBlockHash, eng.ToDifficulty(eng.GetBlockByHeight(Height-1).DifficultyTarget));

		if (m_txes[0].ValueOut != val+nFees) {
			Throw(E_COIN_SubsidyIsVeryBig);
		}
	}
};

class SolidcoinVersionMessage : public VersionMessage {
	typedef VersionMessage base;
protected:
	void Write(BinaryWriter& wr) const override {
		base::Write(wr);
		wr << uint32_t(0);
	}

	void Read(const BinaryReader& rd) override {
		base::Read(rd);
		uint32_t highHeight = rd.ReadUInt32();
		if (highHeight != 0)
			Throw(E_FAIL);
	}
};

class SolidcoinEng : public CoinEng {
	typedef CoinEng base;
public:
	SolidcoinEng(CoinDb& cdb)
		:	base(cdb)
	{
	}
protected:
	BlockObj *CreateBlockObj() override { return new SolidcoinBlockObj; }
	SolidcoinVersionMessage *CreateVersionMessage() override { return new SolidcoinVersionMessage; }


	/*!!! base::  works
	double ToDifficulty(const Target& target) override {
		int nShift = (target.m_value >> 24) & 0xff;
		uint32_t dwDiff1 = ChainParams.MaxPossibleTarget.m_value;
		double dDiff =  (double)(dwDiff1&0x00FFFFFF) / (double)(target.m_value & 0x00ffffff);
		for (; nShift < ((dwDiff1>>24)&0xFF); nShift++)
			dDiff *= 256.0;
		for (; nShift > ((dwDiff1>>24)&0xFF); nShift--)
			dDiff /= 256.0;
		return dDiff;
	}*/

	int64_t GetSubsidy(int height, const HashValue& prevBlockHash, double difficulty, bool bForCheck) override {
		double dCoinInflationModifier = difficulty / 100000.0;
		if (height == 0)
			return 1200000 * ChainParams.CoinValue;
		else if (height < 50000) {
			int64_t qBaseValue = 32;
			qBaseValue -= (height/1000000);
			qBaseValue = std::max(int64_t(4), qBaseValue) * ChainParams.CoinValue;
			qBaseValue += int64_t(dCoinInflationModifier*qBaseValue);
			return height % 2 ? qBaseValue : int64_t(qBaseValue*CPF_PERCENT)+1;
		} else if (height < 180000) {
			double dBaseValue = 5;
			dBaseValue -=  ((int64_t)(height/1000000)) *0.1;
			dBaseValue = std::max(1., dBaseValue) * ChainParams.CoinValue;
			dBaseValue += dCoinInflationModifier*dBaseValue;
			if ((height % 2) == 0) {
				return ((int64_t)(dBaseValue * CPF_PERCENT))+1;
			}
			return (int64_t)dBaseValue;
		}

		double dYearsPassed = ( (double)(height-179999) /(60*24*365));							// We need to work out how many years it has been running to calculate estimated electricity price increases and hardware improvements
		double dKH_Per_Watt = 3.0 * pow(1.50, dYearsPassed);										// moores law type adjustment. Assume 50% better efficiency per year
		double dCostPerKiloWattHour = 0.08*pow(1.10,dYearsPassed);								// We start at 8c per kilowatt hour, compound increase it 10% per year
		double dTotalKWh = ((difficulty*131)/dKH_Per_Watt) / (3600*1000);						// this is the estimated increased cost of electricity per year
		double dBaseValue = dTotalKWh*dCostPerKiloWattHour;
		if ((height%2) == 0) {
			dBaseValue = (dBaseValue*CPF_PERCENT);												// trust blocks are only worth 5% of normal blocks
		}
		return int64_t((std::max(0.0001, dBaseValue) + 0.05)*ChainParams.CoinValue);			// 0.0001 SC is the minimum amount and each block needs at least that to be worth something, also add a minimum amount to cover the existing fee
	}

	void CheckCoinbasedTxPrev(int height, const Tx& txPrev) override {
		if (txPrev.Height > ChainParams.CoinbaseMaturity)
			base::CheckCoinbasedTxPrev(height, txPrev);
	}


	Target GetNextTargetRequired(const Block& blockLast, const Block& block) override {
		int nTargetTimespan = 12 * 60 * 60;			// 12 hours, but it's really 6 due to trusted blocks not being taken into account
		TimeSpan targetSpan = TimeSpan::FromSeconds(nTargetTimespan);
		int64_t nTargetSpacing = 2 * 60;    //2 minute blocks
		int64_t nInterval = nTargetTimespan / nTargetSpacing;
		TimeSpan span(0);

		if (0 == blockLast.Height)
			return blockLast.DifficultyTarget;
		if (blockLast.Height % 2 == 1)
			return ChainParams.MaxTarget;
		Block prev = GetBlockByHeight(blockLast.Height-1);
		if (blockLast.Height % nInterval) {
			if (prev)		//!!!?
				if (GetBlockByHeight(blockLast.Height-2))
					return prev.DifficultyTarget;
			return ChainParams.MaxTarget;
		}

		for (int i=0; i<nInterval-1; i+=2) {						// Go back by what we want to be 6 hours worth of blocks
			int h = blockLast.Height - i - 2;
			if (h < 0)
				break;
			span += GetBlockByHeight(h+1).Timestamp - GetBlockByHeight(h).Timestamp;
		}

		targetSpan = targetSpan /2;					// Limit adjustment step
		TimeSpan onePercent = targetSpan / 100;        //divide by 2 since each OTHER block shouldnt be counted

		if (span < targetSpan) {			//is time taken for a block less than 2 minutes?
			if (span < (onePercent*33))
				span = (onePercent*88);		//pretend it was 88% of our desired to limit increase
			else if (span < (onePercent*66))
				span = (onePercent*92);		//pretend it was 92% of our desired to limit increase
			else
				span = (onePercent*97);		//pretend it was 97% of our desired to limit increase
		} else
			span = std::min(span, targetSpan*4);
		return Target(BigInteger(prev.DifficultyTarget)*span.Ticks/targetSpan.Ticks);
	}

	void SetChainParams(const Coin::ChainParams& p) override {
		base::SetChainParams(p);
		ChainParams.MaxPossibleTarget = Target(0x1E7FFFFF);
	}
};

static CurrencyFactory<SolidcoinEng> s_solidcoin("SolidCoin");

} // Coin::
