/*######     Copyright (c) 1997-2015 Ufasoft  http://ufasoft.com  mailto:support@ufasoft.com,  Sergey Pavlov  mailto:dev@ufasoft.com #########################################################################################################
#                                                                                                                                                                                                                                            #
# This program is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation;  either version 3, or (at your option) any later version.          #
# This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.   #
# You should have received a copy of the GNU General Public License along with this program; If not, see <http://www.gnu.org/licenses/>                                                                                                      #
############################################################################################################################################################################################################################################*/

#include <el/ext.h>

#include "miner.h"

#include "bitcoin-sha256sse.h"

namespace Coin {

class Sha256Hasher : public Hasher {
public:
	Sha256Hasher()
		:	Hasher("sha256", HashAlgo::Sha256)
	{}

	HashValue CalcHash(const ConstBuf& cbuf) override {
//!!!?			BitcoinSha256 sha256;
//!!!?			sha256.PrepareData(wd.Midstate.constData(), wd.Data.constData()+64, wd.Hash1.constData());
//!!!?			r = HashValue(sha256.FullCalc());


		SHA256 sha;
		return HashValue(sha.ComputeHash(sha.ComputeHash(cbuf)));
	}

	uint32_t MineOnCpu(BitcoinMiner& miner, BitcoinWorkData& wd) override {
		BitcoinSha256 *bcSha;

#if UCFG_BITCOIN_ASM
		DECLSPEC_ALIGN(64) byte bufShaAlgo[sizeof(SseBitcoinSha256) + (16*(32*UCFG_BITCOIN_WAY+8)) + 256];		// max possible size with SSE buffers
		if (miner.UseSse2())
			bcSha = new(bufShaAlgo) SseBitcoinSha256;
		else
#else
		DECLSPEC_ALIGN(64) byte bufShaAlgo[sizeof(BitcoinSha256) + (16*(32*UCFG_BITCOIN_WAY+8)) + 256];		// max possible size
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
			Interlocked::Increment(miner.HashCount);
			if (Ext::this_thread::interruption_requested())
				break;
		}
		return nHashes;
	}
} g_sha256Hasher;



} // Coin::
