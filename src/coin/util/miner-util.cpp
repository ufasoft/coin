/*######   Copyright (c) 2013-2015 Ufasoft  http://ufasoft.com  mailto:support@ufasoft.com,  Sergey Pavlov  mailto:dev@ufasoft.com ####
#                                                                                                                                     #
# 		See LICENSE for licensing information                                                                                         #
#####################################################################################################################################*/

#include <el/ext.h>

#include "block-template.h"
#include "prime-util.h"
#include "../miner/miner-share.h"

namespace Coin {

void MinerShare::WriteHeader(BinaryWriter& wr) const {
	base::WriteHeader(wr);
	switch (Algo) {
	case HashAlgo::Momentum:
		wr << BirthdayA << BirthdayB;
		break;
	}
}

static HashValue s_hashMax("FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF");

HashValue MinerShare::GetHash() const {
	switch (Algo) {
	case HashAlgo::Sha3:
	{
		MemoryStream ms;
		base::WriteHeader(BinaryWriter(ms).Ref());
		return HashValue(SHA3<256>().ComputeHash(ConstBuf(ms)));
	}
	case HashAlgo::Metis:
	{
		MemoryStream ms;
		base::WriteHeader(BinaryWriter(ms).Ref());
		return MetisHash(ms);
	}
	case HashAlgo::Momentum:
	{
		MemoryStream ms;
		base::WriteHeader(BinaryWriter(ms).Ref());
		if (!MomentumVerify(Coin::Hash(ms), BirthdayA, BirthdayB))
			return s_hashMax;
	}
	default:
		return base::GetHash();
	}
}

HashValue MinerShare::GetHashPow() const {
	switch (Algo) {
	case HashAlgo::SCrypt:
	{
		MemoryStream ms;
		WriteHeader(BinaryWriter(ms).Ref());
		return ScryptHash(ms);
	}
	break;
	case HashAlgo::NeoSCrypt:
	{
		MemoryStream ms;
		WriteHeader(BinaryWriter(ms).Ref());
		return NeoSCryptHash(ms, 0);
	}
	break;
	default:
		return GetHash();
	}
}

void PrimeMinerShare::WriteHeader(BinaryWriter& wr) const {
	base::WriteHeader(wr);
	CoinSerialized::WriteBlob(wr, PrimeChainMultiplier.ToBytes());
}

uint32_t PrimeMinerShare::GetDifficulty() const {
	MemoryStream ms;
	base::WriteHeader(BinaryWriter(ms).Ref());
	HashValue h = Hash(ms);
	if (h[31] < 0x80)
		Throw(CoinErr::MINER_REJECTED);
	BigInteger origin = HashValueToBigInteger(h) * PrimeChainMultiplier;
	PrimeTester tester;				//!!!TODO move to some long-time scope
	return uint32_t(tester.ProbablePrimeChainTest(Bn(origin)).BestTypeLength().second * 0x1000000);
}



} // Coin::

