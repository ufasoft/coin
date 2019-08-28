/*######   Copyright (c) 2011-2015 Ufasoft  http://ufasoft.com  mailto:support@ufasoft.com,  Sergey Pavlov  mailto:dev@ufasoft.com ####
#                                                                                                                                     #
# 		See LICENSE for licensing information                                                                                         #
#####################################################################################################################################*/

#include <el/ext.h>

#include <el/num/prime.h>
using namespace Ext::Num;

#include "../util/prime-util.h"

#include "miner.h"

#include "xpt.h"

#pragma comment(lib, EXT_GMP_LIB)


namespace Coin {

const size_t MIN_PRIME_SEQ = 4;				// Prime number 11, the first prime with unknown factor status.

const uint32_t DEFAULT_PRIMES_TO_SIEVE = 28000;
const uint32_t DEFAULT_SIEVE_PERCENTAGE = 10;
const uint32_t DEFAULT_SIEVE_EXTENSIONS = 10;

const uint32_t PRIMORIAL_HASH_FACTOR = 11, //!!!? 7,
			DEFAULT_PRIMORIAL_MULTIPLIER = 67;

const uint32_t L1_CACHE_ELEMENTS = 256000;

typedef dynamic_bitset<size_t> Bitset;

uint32_t NPrimesBefore(uint32_t upper) {
	const vector<uint32_t>& primes = PrimeTable();
	return uint32_t(lower_bound(primes.begin(), primes.end(), upper) - primes.begin());
}


#if UCFG_BITCOIN_ASM
extern "C" {
	void __cdecl BitsetPeriodicSetBts(void *bitset, size_t size, size_t idx, size_t period);
	void __cdecl BitsetPeriodicSetOr(void *bitset, size_t size, size_t idx, size_t period);
	void __cdecl BitsetOr(size_t *dst, const size_t *bitset, size_t n, size_t prime, size_t off);
}
#endif // UCFG_BITCOIN_ASM


class Sieve {
	typedef Sieve class_type;
public:
	static const uint32_t IdxInitial = 1000;

	vector<Bitset> CandidateExtensions, ExtendedCC1s, ExtendedCC2s, ExtendedBiTwins;

	BitcoinMiner& Miner;
	XptWorkData& WorkData;
	Bn FixedFactor;	
	uint32_t IdxOptimal;
	uint32_t m_seq;
	int ChainLength;
	uint32_t NPrimes,
			NLayers,
			NExtensions;
	int m_candExtension;
	uint32_t SieveSize;

	Sieve(BitcoinMiner& miner, XptWorkData& wd, uint32_t sieveSize, uint32_t nExtensions, uint32_t bits)
		:	Miner(miner)
		,	WorkData(wd)
		,	IdxOptimal(IdxInitial)
		,	m_seq(0)
		,	SieveSize(sieveSize)
		,	NExtensions(nExtensions)
		,	NPrimes(NPrimesBefore(sieveSize) / 100 * DEFAULT_SIEVE_PERCENTAGE)
		,	CandidateExtensions(nExtensions+1)
		,	ExtendedCC1s(nExtensions+1)
		,	ExtendedCC2s(nExtensions+1)
		,	ExtendedBiTwins(nExtensions+1)
		,	ChainLength(bits >> 24)
		,	m_candBlockIndex(-1)
		,	m_candExtension(0)
		,	m_curCandBlock(0)
	{
		ChainLength += int(float(bits)/0x1000000 - 0.9f >= ChainLength);
		NLayers = ChainLength  + nExtensions;

		CandidateExtensions[0].resize(sieveSize);
		ExtendedCC1s[0].resize(sieveSize);
		ExtendedCC2s[0].resize(sieveSize);
		ExtendedBiTwins[0].resize(sieveSize);
		for (size_t i=1; i<=NExtensions; ++i) {
			CandidateExtensions[i].resize(sieveSize/2);
			ExtendedCC1s[i].resize(sieveSize/2);
			ExtendedCC2s[i].resize(sieveSize/2);
			ExtendedBiTwins[i].resize(sieveSize/2);
		}
	}

	void Weave();
	
	uint64_t GetSeaveWeaveOptimalPrime() const {
		const auto& primes = PrimeTable();
		return IdxOptimal<primes.size() ? primes[IdxOptimal] : SieveSize;
	}

	uint32_t GetCandidateCount();

	struct CandidateInfo {
		PrimeChainType Type;
		uint32_t Multiplier;
		uint32_t CandidateIndex;
	};
	CandidateInfo GetNextCandidateMultiplier();

	double EstimateCandidatePrimeProbability(uint32_t nPrimorialMultiplier, uint32_t nChainPrimeNum) const;
private:
	int m_candBlockIndex;
	Bitset::block_type m_curCandBlock;
};

uint32_t Sieve::GetCandidateCount() {
	uint32_t r = 0;
	for (size_t i=0; i<CandidateExtensions.size(); ++i)
		r += CandidateExtensions[i].count();
	return r;
}

const size_t BITS_IN_SIZE_T = sizeof(size_t)*8;

void Sieve::Weave() {
	const uint32_t *primes = &PrimeTable()[0];

	vector<uint32_t> multipliers(NPrimes * NLayers);

	uint32_t nCombinedEndSeq = MIN_PRIME_SEQ, nFixedFactorCombinedMod = 0;
	for (uint32_t seq=MIN_PRIME_SEQ; seq<NPrimes; ++seq) {
		uint32_t prime = primes[seq];
		if (seq >= nCombinedEndSeq) {
			uint32_t nPrimeCombined = 1;
			for (uint64_t n; (n=uint64_t(nPrimeCombined) * primes[nCombinedEndSeq]) < UINT_MAX; ++nCombinedEndSeq)
				nPrimeCombined = (uint32_t)n;
			nFixedFactorCombinedMod = uint32_t(FixedFactor % nPrimeCombined);
		}
		if (uint32_t nFixedFactorMod = nFixedFactorCombinedMod % prime) {
			uint32_t fixedInverse = inverse(nFixedFactorMod, prime),
				twoInverse = inverse(2, prime);
			for (uint32_t chainSeq=0; chainSeq<NLayers; ++chainSeq)
				multipliers[chainSeq*NPrimes + seq] = exchange(fixedInverse, uint32_t(uint64_t(fixedInverse) * twoInverse % prime));
		}
	}

	uint32_t nRound = RoundUpToMultiple(SieveSize, L1_CACHE_ELEMENTS) / L1_CACHE_ELEMENTS;
	uint32_t nBiTwinCC1Layers = (ChainLength + 1) / 2,
			nBiTwinCC2Layers = ChainLength / 2;
    const uint32_t nExtensionsMinMultiplier = SieveSize / 2;
	Bitset bs1(L1_CACHE_ELEMENTS), bs2(L1_CACHE_ELEMENTS);
	for (uint32_t j=0; j<nRound; ++j) {
		const uint32_t minMultiplier = L1_CACHE_ELEMENTS * j,
			maxMultiplier = min(minMultiplier+L1_CACHE_ELEMENTS, SieveSize),
			nExtMinMultiplier = max(minMultiplier, nExtensionsMinMultiplier);
		for (uint32_t nLayer=0; nLayer<NLayers; ++nLayer) {
			if (WorkData.Height != Miner.MaxHeight)
				return;
			uint32_t offset = nLayer<ChainLength ? minMultiplier : nExtMinMultiplier;
			if (offset < maxMultiplier) {
				bs1.reset();
				bs2.reset();
				uint32_t size = maxMultiplier - offset;			
				const int32_t *pMultipliers = (const int32_t*)&multipliers[nLayer*NPrimes + MIN_PRIME_SEQ];
				for (uint32_t seq=MIN_PRIME_SEQ; seq<NPrimes; ++seq, ++pMultipliers) {
					if (int mul1 = *pMultipliers) {
						uint32_t prime = primes[seq];
						int diff = int(prime) -  2 * mul1;
						mul1 = (mul1>=offset ? mul1 : mul1 + RoundUpToMultiple(offset - mul1, prime)) - offset;
						int mul2 = mul1 + diff;
						mul2 = mul2<0 ? mul2 + prime :
								mul2>=prime ? mul2-prime : mul2;
#if UCFG_BITCOIN_ASM
						BitsetPeriodicSetOr(bs1.data(), size, mul1, prime);
						BitsetPeriodicSetOr(bs2.data(), size, mul2, prime);
#else
						for (; mul1 < size; mul1 += prime, mul2 += prime) {
							bs1.set(mul1);
							if (mul2 < size)
								bs2.set(mul2);
						}
#endif
					}
				}

				for (uint32_t nExt=0; nExt<=NExtensions; ++nExt) {
					if (nLayer>=nExt && nLayer<ChainLength+nExt) {
						Bitset &extendedCC1 = ExtendedCC1s[nExt],
							&extendedCC2 = ExtendedCC2s[nExt],
							&extendedBiTwin = ExtendedBiTwins[nExt];
						int offsetOr = offset - int(nExt ? nExtensionsMinMultiplier : 0);
						extendedCC1.Or(bs1, offsetOr);
						extendedCC2.Or(bs2, offsetOr);
						if (nLayer-nExt < nBiTwinCC1Layers) {
							extendedBiTwin.Or(bs1, offsetOr);
							if (nLayer-nExt < nBiTwinCC2Layers)
								extendedBiTwin.Or(bs2, offsetOr);
						}
					}
				}
			}
		}
	}

	for (int i=0; i<=NExtensions; ++i)
		CandidateExtensions[i] = ~(ExtendedCC1s[i] & ExtendedCC2s[i] & ExtendedBiTwins[i]);

	TRC(2, "Sieve: " << GetCandidateCount() << "/" << SieveSize << " @ " << DEFAULT_SIEVE_PERCENTAGE << "%");
}

Sieve::CandidateInfo Sieve::GetNextCandidateMultiplier() {
	const uint32_t SizeSizeHalf = SieveSize / 2;
	Bitset *bs = &CandidateExtensions[m_candExtension];
	while (true) {
		if (int n = BitOps::Scan(m_curCandBlock)) {
			m_curCandBlock &= ~(Bitset::block_type(1) << --n);
			int idx = m_candBlockIndex*Bitset::bits_per_block  + n;
			CandidateInfo ci = { !ExtendedBiTwins[m_candExtension][idx] ? PrimeChainType::Twin :
				!ExtendedCC1s[m_candExtension][idx] ? PrimeChainType::Cunningham1 :
				PrimeChainType::Cunningham2,
				(m_candExtension ? SizeSizeHalf+idx : idx) << m_candExtension,
				m_candExtension ? SizeSizeHalf+idx : idx
			};
			return ci;
		}
		if (++m_candBlockIndex < bs->num_blocks()) {
			m_curCandBlock = bs->data()[m_candBlockIndex];
			if (!m_candBlockIndex)
				m_curCandBlock &= ~Bitset::block_type(1);
		} else {
			m_candBlockIndex = -1;
			if (++m_candExtension > NExtensions) {
				m_candExtension = 0;
				CandidateInfo ci = { PrimeChainType::Twin, 0, 0 };
				return ci;
			}
			bs = &CandidateExtensions[m_candExtension];
		}
	}
}

#ifndef M_LN2
	static const double M_LN2 = log(2.);
#endif

static const double M_LN_1_5 = log(1.5);

double Sieve::EstimateCandidatePrimeProbability(uint32_t nPrimorialMultiplier, uint32_t nChainPrimeNum) const {
	const vector<uint32_t>& primes = PrimeTable();	
    const uint32_t nSieveWeaveOptimalPrime = primes[NPrimes - 1];
    const double nAverageCandidateMultiplier = SieveSize / 2;
    double dFixedMultiplier = Primorial(nPrimorialMultiplier).AsDouble() / Primorial(PRIMORIAL_HASH_FACTOR).AsDouble();

    double dExtendedSieveWeightedSum = 0.5 * SieveSize;
    for (unsigned int i = 0; i < NExtensions; i++)
        dExtendedSieveWeightedSum += 0.75 * (SieveSize * (2 << i));
	const double dExtendedSieveAverageMultiplier = dExtendedSieveWeightedSum / (SieveSize + NExtensions * (SieveSize / 2));
    return (1.781072 * log((double)std::max(1u, nSieveWeaveOptimalPrime)) / ((255.0 + nChainPrimeNum) * M_LN2 + M_LN_1_5 + log(dFixedMultiplier) + log(nAverageCandidateMultiplier) + log(dExtendedSieveAverageMultiplier)));
}


class PrimeHasher : public Hasher {
public:
	PrimeHasher()
		:	Hasher("prime", HashAlgo::Prime)
	{}

	uint32_t MineOnCpu(BitcoinMiner& miner, BitcoinWorkData& rwd) override;
} g_primeHasher;

uint32_t PrimeHasher::MineOnCpu(BitcoinMiner& miner, BitcoinWorkData& rwd) {
	XptWorkData& wd = dynamic_cast<XptWorkData&>(rwd);

	wd.FixedPrimorialMultiplier = wd.FixedPrimorialMultiplier != 0 ? wd.FixedPrimorialMultiplier : DEFAULT_PRIMORIAL_MULTIPLIER;
	wd.FixedHashFactor = wd.FixedHashFactor != 0 ? wd.FixedHashFactor : PRIMORIAL_HASH_FACTOR;

	const uint32_t hashFactor = explicit_cast<uint32_t>(Primorial(wd.FixedHashFactor));
	const double target = double(wd.Bits) / 0x1000000;
	const Bn fixedMultiplier = Bn(Primorial(wd.FixedPrimorialMultiplier)) / hashFactor;
	const uint32_t nPrimesToSeave = min(wd.PrimesToSieveMax, max(wd.PrimesToSieveMin, DEFAULT_PRIMES_TO_SIEVE));

	NonceRange powNonceRange;

	wd.Nonce = wd.FirstNonce;
	PrimeTester tester;
	uint32_t nHashes = 0;
	while (!Ext::this_thread::interruption_requested() && (!wd.Interruptible || wd.Height == miner.MaxHeight)) {
		HashValue hashPow = Hasher::Find(wd.HashAlgo).CalcWorkDataHash(wd);
		Bn bnHash;
		if (hashPow.data()[31] < 0x80 || !(bnHash = Bn::FromBinary(hashPow.ToSpan(), Endian::Little)).DivisibleP(hashFactor))
			goto LAB_NEXT_NONCE;
		{
			Bn hashMultiplier = bnHash * fixedMultiplier;
			int nTests = 0;

			Sieve sieve(miner, wd, wd.SieveSize, DEFAULT_SIEVE_EXTENSIONS, wd.Bits);
			sieve.FixedFactor = hashMultiplier;
			sieve.Weave();
			double probability = 1;
			for (int len=wd.Bits>>24; len--;)
				probability *= sieve.EstimateCandidatePrimeProbability(wd.FixedPrimorialMultiplier, len);

			PrimeTester tester;
			tester.FermatTest = false;
			while (!Ext::this_thread::interruption_requested() && (!wd.Interruptible || wd.Height == miner.MaxHeight)) {
				Sieve::CandidateInfo ci = sieve.GetNextCandidateMultiplier();
				if (!ci.Multiplier)
					break;
				++nTests;
				miner.ChainsExpectedCount += float(probability);		//!!!RC

				Bn biTriedMultiplier((mp_ui_t)ci.Multiplier);
				Bn origin = hashMultiplier * biTriedMultiplier;
				
				CunninghamLengths cl = tester.ProbablePrimeChainTest(Bn(origin), ci.Type);
				pair<PrimeChainType, double> btl = cl.BestTypeLength();
				uint32_t diff = uint32_t(btl.second * 0x1000000),
					len = diff >> 24;
				if (diff > powNonceRange.ChainLength) {
					powNonceRange.ChainType = btl.first;
					powNonceRange.ChainLength = diff;
					powNonceRange.Nonce = wd.Nonce;
					powNonceRange.Multiplier = ci.Multiplier;
				}
				if (len >= 1) {
					++nHashes;
					++miner.aHashCount;
					if (len >= 5) {
						TRC(3, "Prime FOUND, len: " << len);
					}
					if (len < wd.Client->aLenStats.size())
						++(wd.Client->aLenStats[len]);
					else {
						TRC(1, "Unexpected Chain Length " << len);
					}
					if (diff >= wd.BitsForShare) {
						ptr<XptWorkData> share = wd.Clone();
						share->SieveSize = sieve.SieveSize;
						share->SieveCandidate = ci.CandidateIndex;
						share->ChainMultiplier = (share->FixedMultiplier = fixedMultiplier.ToBigInteger()) * biTriedMultiplier.ToBigInteger();
						try {
							wd.Client->Submit(share);
						} catch (RCExc ex) {
							*miner.m_pTraceStream << ex.what() << endl;
						}
					}
				}
			}
		}
LAB_NEXT_NONCE:
		if (wd.Nonce != wd.LastNonce)
			wd.Nonce = wd.Nonce + 1;
		else {		
			EXT_LOCK (wd.Client->MtxData) {
				Pow& pow = wd.Client->m_powMap[make_pair(wd.PrevBlockHash, wd.MerkleRoot)];
				powNonceRange.NonceEnd = 0xFFFFFFFF;
				pow.Ranges.push_back(powNonceRange);
			}
			powNonceRange = NonceRange();

			if (!wd.TimestampRoll)
				break;
			wd.BlockTimestamp = wd.get_BlockTimestamp() + TimeSpan::FromSeconds(1);
			wd.Nonce = wd.FirstNonce;
		}			
	}
	EXT_LOCK (wd.Client->MtxData) {
		powNonceRange.NonceEnd = wd.Nonce;

		Pow& pow = wd.Client->m_powMap[make_pair(wd.PrevBlockHash, wd.MerkleRoot)];

		pow.PrevBlockHash = wd.PrevBlockHash;
		pow.MerkleRoot = wd.MerkleRoot;
		pow.DtStart = wd.Timestamp;
		pow.SieveSize = wd.SieveSize;
		pow.PrimesToSieve = nPrimesToSeave;
		pow.Ranges.push_back(powNonceRange);
	}
	return nHashes;
}




} // Coin::
