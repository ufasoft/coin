/*######   Copyright (c) 2011-2015 Ufasoft  http://ufasoft.com  mailto:support@ufasoft.com,  Sergey Pavlov  mailto:dev@ufasoft.com ####
#                                                                                                                                     #
# 		See LICENSE for licensing information                                                                                         #
#####################################################################################################################################*/

#include <el/ext.h>

#include "miner.h"

namespace Coin {

class ScryptHasher : public Hasher {
public:
	ScryptHasher()
		: Hasher("scrypt", HashAlgo::SCrypt)
	{}

	HashValue CalcHash(RCSpan cbuf) override {
		array<uint32_t, 8> res = CalcSCryptHash(cbuf);
		return HashValue(ConstBuf(res.data(), 32));
	}

#if UCFG_PLATFORM_X64 && UCFG_BITCOIN_ASM
	void MineNparNonces(BitcoinMiner& miner, BitcoinWorkData& wd, uint32_t *buf, uint32_t nonce) override {
		for (int i=0; i<UCFG_BITCOIN_NPAR; i+=3, nonce+=3) {
			SetNonce(buf, nonce);
			array<array<uint32_t, 8>, 3> res3 = CalcSCryptHash_80_3way(buf);
			for (int j=0; j<3; ++j) {
				if (HashValue(ConstBuf(res3[j].data(), 32)) <= wd.HashTarget) {
					if (!miner.TestAndSubmit(&wd, htobe(nonce+j)))
						*miner.m_pTraceStream << "Found NONCE not accepted by Target" << endl;
				}
			}
		}
	}
#endif // UCFG_PLATFORM_X64 && UCFG_BITCOIN_ASM

} g_scryptHasher;



} // Coin::
