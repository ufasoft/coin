/*######   Copyright (c) 2011-2015 Ufasoft  http://ufasoft.com  mailto:support@ufasoft.com,  Sergey Pavlov  mailto:dev@ufasoft.com ####
#                                                                                                                                     #
# 		See LICENSE for licensing information                                                                                         #
#####################################################################################################################################*/

#include <el/ext.h>

#include "miner.h"

#include "bitcoin-sha256sse.h"

namespace Coin {

static SHA256 s_sha256;

class Sha256Hasher : public Hasher {
public:
	Sha256Hasher()
		: Hasher("sha256", HashAlgo::Sha256)
	{}

	HashValue CalcHash(RCSpan cbuf) override {
//!!!?			BitcoinSha256 sha256;
//!!!?			sha256.PrepareData(wd.Midstate.constData(), wd.Data.constData()+64, wd.Hash1.constData());
//!!!?			r = HashValue(sha256.FullCalc());


		return HashValue(s_sha256.ComputeHash(s_sha256.ComputeHash(cbuf)));
	}

	uint32_t MineOnCpu(BitcoinMiner& miner, BitcoinWorkData& wd) override {
		BitcoinSha256 *bcSha;

#if UCFG_BITCOIN_ASM
		DECLSPEC_ALIGN(64) uint8_t bufShaAlgo[sizeof(SseBitcoinSha256) + (16*(32*UCFG_BITCOIN_WAY+8)) + 256];		// max possible size with SSE buffers
		if (miner.UseSse2())
			bcSha = new(bufShaAlgo) SseBitcoinSha256;
		else
#else
		DECLSPEC_ALIGN(64) uint8_t bufShaAlgo[sizeof(BitcoinSha256) + (16*(32*UCFG_BITCOIN_WAY+8)) + 256];		// max possible size
#endif
			bcSha = new(bufShaAlgo) BitcoinSha256;

		bcSha->PrepareData(wd.Midstate.constData(), wd.Data.constData()+64, wd.Hash1.constData());
		uint32_t nHashes = 0;
		for (uint32_t nonce=wd.FirstNonce, end=wd.LastNonce+1; nonce!=end;) {
			if (bcSha->FindNonce(nonce)) {
				BitcoinSha256 sha256;
				sha256.PrepareData(wd.Midstate.constData(), wd.Data.constData()+64, wd.Hash1.constData());
				while (true) {
					miner.TestAndSubmit(&wd, nonce);
					if (!(++nonce % UCFG_BITCOIN_NPAR) || !sha256.FindNonce(nonce))
						break;
				}
			}				
			nHashes += UCFG_BITCOIN_NPAR;
			++miner.aHashCount;
			if (Ext::this_thread::interruption_requested())
				break;
		}
		return nHashes;
	}
} g_sha256Hasher;



} // Coin::
