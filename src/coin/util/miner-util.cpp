/*######   Copyright (c) 2013-2015 Ufasoft  http://ufasoft.com  mailto:support@ufasoft.com,  Sergey Pavlov  mailto:dev@ufasoft.com ####
#                                                                                                                                     #
# 		See LICENSE for licensing information                                                                                         #
#####################################################################################################################################*/

#include <el/ext.h>

#include "block-template.h"
#include "prime-util.h"
#include "../miner/miner-share.h"

namespace Coin {

void MinerShare::WriteHeader(ProtocolWriter& wr) const {
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
		base::WriteHeader(ProtocolWriter(ms).Ref());
		return HashValue(SHA3<256>().ComputeHash(ms));
	}
	case HashAlgo::Metis:
	{
		MemoryStream ms;
		base::WriteHeader(ProtocolWriter(ms).Ref());
		return MetisHash(ms);
	}
#if UCFG_COIN_MOMENTUM
	case HashAlgo::Momentum:
	{
		MemoryStream ms;
		base::WriteHeader(ProtocolWriter(ms).Ref());
		if (!MomentumVerify(Coin::Hash(ms), BirthdayA, BirthdayB))
			return s_hashMax;
	}
#endif
	default:
		return base::GetHash();
	}
}

HashValue MinerShare::GetHashPow() const {
	switch (Algo) {
	case HashAlgo::SCrypt:
	{
		MemoryStream ms;
		WriteHeader(ProtocolWriter(ms).Ref());
		return ScryptHash(ms);
	}
	break;
	case HashAlgo::NeoSCrypt:
	{
		MemoryStream ms;
		WriteHeader(ProtocolWriter(ms).Ref());
		return NeoSCryptHash(ms, 0);
	}
	break;
	default:
		return GetHash();
	}
}

#if UCFG_COIN_PRIME
void PrimeMinerShare::WriteHeader(ProtocolWriter& wr) const {
	base::WriteHeader(wr);
	CoinSerialized::WriteSpan(wr, PrimeChainMultiplier.ToBytes());
}

uint32_t PrimeMinerShare::GetDifficulty() const {
	MemoryStream ms;
	base::WriteHeader(ProtocolWriter(ms).Ref());
	HashValue h = Hash(ms);
	if (h.data()[31] < 0x80)
		Throw(CoinErr::MINER_REJECTED);
	BigInteger origin = HashValueToBigInteger(h) * PrimeChainMultiplier;
	PrimeTester tester;				//!!!TODO move to some long-time scope
	return uint32_t(tester.ProbablePrimeChainTest(Bn(origin)).BestTypeLength().second * 0x1000000);
}
#endif // UCFG_COIN_PRIME


} // Coin::

