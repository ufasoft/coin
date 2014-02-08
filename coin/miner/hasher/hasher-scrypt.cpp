/*######     Copyright (c) 1997-2013 Ufasoft  http://ufasoft.com  mailto:support@ufasoft.com,  Sergey Pavlov  mailto:dev@ufasoft.com #######################################
#                                                                                                                                                                          #
# This program is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation;  #
# either version 3, or (at your option) any later version. This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the      #
# implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details. You should have received a copy of the GNU #
# General Public License along with this program; If not, see <http://www.gnu.org/licenses/>                                                                               #
##########################################################################################################################################################################*/

#include <el/ext.h>

#include "miner.h"

namespace Coin {

class ScryptHasher : public Hasher {
public:
	ScryptHasher()
		:	Hasher("scrypt", HashAlgo::SCrypt)
	{}

	HashValue CalcHash(const ConstBuf& cbuf) override {
		array<UInt32, 8> res = CalcSCryptHash(cbuf);
		return HashValue(ConstBuf(res.data(), 32));
	}

#if UCFG_PLATFORM_X64 && UCFG_BITCOIN_ASM
	void MineNparNonces(BitcoinMiner& miner, BitcoinWorkData& wd, UInt32 *buf, UInt32 nonce) override {
		for (int i=0; i<UCFG_BITCOIN_NPAR; i+=3, nonce+=3) {
			SetNonce(buf, nonce);
			array<array<UInt32, 8>, 3> res3 = CalcSCryptHash_80_3way(buf);
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
