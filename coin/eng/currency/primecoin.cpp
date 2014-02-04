#include <el/ext.h>

#include "../util/prime-util.h"
#include "eng.h"

namespace Coin {

double GetFractionalDifficulty(double length) {
	return 1/(1-(length - floor(length)));
}

double DifficultyToFractional(double diff) {
	return 1 - 1/diff;
}


class PrimeCoinBlockObj : public BlockObj {
	typedef BlockObj base;
public:
	BigInteger PrimeChainMultiplier;

	double GetTargetLength() const {
		return double(DifficultyTargetBits)/0x1000000;
	}
protected:
	void WriteHeader(BinaryWriter& wr) const override {
		base::WriteHeader(wr);
		CoinSerialized::WriteBlob(wr, PrimeChainMultiplier.ToBytes());
	}

	void ReadHeader(const BinaryReader& rd, bool bParent) override {
		base::ReadHeader(rd, bParent);
		Blob blob = CoinSerialized::ReadBlob(rd);
		PrimeChainMultiplier = BigInteger(blob.constData(), blob.Size);

		TRC(4, "PrimeChainMultiplier: " << PrimeChainMultiplier);
	}

	using base::Write;
	using base::Read;

	void Write(DbWriter& wr) const override {
		base::Write(wr);
		wr << PrimeChainMultiplier;
	}

	void Read(const DbReader& rd) override {
		base::Read(rd);
		rd >> PrimeChainMultiplier;
	}

	BigInteger GetWork() const override {
		return 1;
	}

	Coin::HashValue Hash() const override {
		if (!m_hash) {
			MemoryStream ms;
			WriteHeader(BinaryWriter(ms));
			m_hash = Coin::Hash(ms);
		}
		return m_hash.get();
	}

	HashValue PowHash(CoinEng& eng) const override {
		return Coin::Hash(EXT_BIN(Ver << PrevBlockHash << MerkleRoot() << (UInt32)to_time_t(Timestamp) << get_DifficultyTarget() << Nonce));
	}

	void CheckPow(const Target& target) override {
		HashValue hashPow = PowHash(Eng());
		BigInteger bnHash = HashValueToBigInteger(hashPow);
		BigInteger origin = bnHash * PrimeChainMultiplier;

		if (hashPow[31]<0x80 || origin>GetPrimeMax())
			Throw(E_COIN_ProofOfWorkFailed);

		double targ = GetTargetLength();
		PrimeTester tester;
		CunninghamLengths cl = tester.ProbablePrimeChainTest(Bn(origin));
		if (!cl.TargetAchieved(targ)) {
#ifdef _DEBUG//!!!D
			tester.ProbablePrimeChainTest(Bn(origin));
#endif
			Throw(E_COIN_ProofOfWorkFailed);
		}
		pair<PrimeChainType, double> tl = cl.BestTypeLength();
		if (!PrimeChainMultiplier.TestBit(0) && !origin.TestBit(1) && tester.ProbablePrimeChainTest(Bn(origin/2)).BestTypeLength().second > tl.second) {
#ifdef X_DEBUG//!!!D
			double le = ProbablePrimeChainTest(origin/2, targ).BestTypeLength().second;
#endif
			Throw(E_COIN_ProofOfWorkFailed);
		}
	}
};

class PrimeCoinEng : public CoinEng {
	typedef CoinEng base;
public:
	PrimeCoinEng(CoinDb& cdb)
		:	base(cdb)
	{
	}

	void SetChainParams(const Coin::ChainParams& p, IBlockChainDb *db) override {
		base::SetChainParams(p, db);
		ChainParams.MedianTimeSpan = 99;
	}
protected:
	bool MiningAllowed() override { return false; }
	BlockObj *CreateBlockObj() override { return new PrimeCoinBlockObj; }

	double ToDifficulty(const Target& target) override {
		return double(target.m_value)/(1 << FRACTIONAL_BITS);
	}

	Int64 GetSubsidy(int height, const HashValue& prevBlockHash, double difficulty, bool bForCheck) override {
		Int64 cents = (Int64)floor(999 * 100 / (difficulty*difficulty));
		return cents * (ChainParams.CoinValue/100);
	}

	Target GetNextTargetRequired(const Block& blockLast, const Block& block) override {
		double r = 7;
		if (blockLast.Height > 1) {
			const int INTERVAL = 7*24*60;
			int nActualSpacing = (int)(blockLast.get_Timestamp() - blockLast.GetPrevBlock().get_Timestamp()).get_TotalSeconds();
			double targ = ((PrimeCoinBlockObj*)blockLast.m_pimpl.get())->GetTargetLength();
			double d = GetFractionalDifficulty(targ);
			int nTargetSpacing = (int)ChainParams.BlockSpan.get_TotalSeconds();
			
			BigInteger bn = BigInteger((Int64)floor(d * (1ULL << 32))) * (INTERVAL+1) * nTargetSpacing / ((INTERVAL-1) * nTargetSpacing + 2 * nActualSpacing);
			d = double(explicit_cast<Int64>(bn)) / (1ULL << 32); 

			d = max(1., min(double(1 << FRACTIONAL_BITS), d));
			r = targ;
			if (d > 256)
				r = floor(targ)+1;
			else if (d == 1 && floor(targ) > 6)
				r = floor(targ) - 1 + DifficultyToFractional(256);
			else
				r = floor(targ) + DifficultyToFractional(d);
		}
		return Target((UInt32)ceil(r * 0x1000000));
	}

	Target GetNextTarget(const Block& blockLast, const Block& block) override {
		return GetNextTargetRequired(blockLast, block);
	}
};


static class PrimeCoinChainParams : public ChainParams {
	typedef ChainParams base;
public:
	PrimeCoinChainParams(bool)
		:	base("Primecoin", false)
	{	
		ChainParams::Add(_self);		
	}

protected:
	PrimeCoinEng *CreateObject(CoinDb& cdb, IBlockChainDb *db) override {
		PrimeCoinEng *r = new PrimeCoinEng(cdb);
		r->SetChainParams(_self, db);
		return r;
	}


} s_primecoinParams(true);


} // Coin::

