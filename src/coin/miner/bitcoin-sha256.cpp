/*######     Copyright (c) 1997-2015 Ufasoft  http://ufasoft.com  mailto:support@ufasoft.com,  Sergey Pavlov  mailto:dev@ufasoft.com #########################################################################################################
#                                                                                                                                                                                                                                            #
# This program is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation;  either version 3, or (at your option) any later version.          #
# This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.   #
# You should have received a copy of the GNU General Public License along with this program; If not, see <http://www.gnu.org/licenses/>                                                                                                      #
############################################################################################################################################################################################################################################*/

#include <el/ext.h>

#include <el/libext/ext-net.h>


#include "bitcoin-sha256sse.h"

namespace Coin {


ptr<BitcoinSha256> BitcoinSha256::CreateObject() {
#if UCFG_BITCOIN_ASM
	if (CpuInfo().HasSse2)
		return new SseBitcoinSha256;
	else
#endif
		return new BitcoinSha256;
}

void BitcoinSha256::PrepareData(const void *midstate, const void *data, const void *hash1) {
	memcpy(m_midstate, midstate, sizeof m_midstate);
	memcpy(m_w, data, 16*sizeof(uint32_t));
	CalcW(m_w, 16);
	CalcW(m_w, 17);

	memcpy(m_midstate_after_3, m_midstate, sizeof m_midstate);
	CalcRounds(m_w, m_midstate, m_midstate_after_3, 18, 0, 3);

	{
		uint32_t a = m_midstate_after_3[0], b = m_midstate_after_3[1], c = m_midstate_after_3[2], d = m_midstate_after_3[3], e = m_midstate_after_3[4], f = m_midstate_after_3[5], g = m_midstate_after_3[6], h = m_midstate_after_3[7];
		m_preVal4 = h + (Rotr32(e, 6) ^ Rotr32(e, 11) ^ Rotr32(e, 25)) + ((e & f) ^ AndNot(e, g)) + g_sha256_k[3];
		m_t1 = (Rotr32(a, 2) ^ Rotr32(a, 13) ^ Rotr32(a, 22)) + ((a & b) ^ (b & c) ^ (a & c));
	}

	memcpy(m_w1, hash1, 16*sizeof(uint32_t));

	/*!!!
	uint32_t bits = 256;
	*(byte*)&m_w1[8] = 0x80;
	byte *p = (byte*)&m_w1[63]+3;			// swap for LE-processing. Original SHA-2 is BE
	p[3] = byte(bits >> 24);
	p[2] = byte(bits >> 16);
	p[1] = byte(bits >> 8);
	p[0] = byte(bits);
	*/
}

bool BitcoinSha256::FindNonce(uint32_t& nonce) {
	int n = UCFG_BITCOIN_NPAR - (nonce & (UCFG_BITCOIN_NPAR-1));
#if UCFG_BITCOIN_ASM

#	ifdef X_DEBUG//!!!D
	memcpy(m_w1, m_midstate_after_3, 8*sizeof(uint32_t));		
	m_w[3] = nonce;
	CalcRounds(m_w, m_midstate, m_w1, 18, 3, 64);
	uint32_t v[8];
	memcpy(v, g_sha256_hinit, 8*sizeof(uint32_t));
	uint32_t e = CalcRounds(m_w1, v, v, 16, 0, 61);				// We enough this
	if (g_sha256_hinit[7]+e == 0)
		cout << "AAA";
#	endif

	return CalcSha256(m_w, m_midstate, m_midstate_after_3, n, nonce);
#else
	for (int i=0; i<n; ++i, ++nonce) {
		m_w[3] = nonce;
		memcpy(m_w1, m_midstate_after_3, 8*sizeof(uint32_t));		
		CalcRounds(m_w, m_midstate, m_w1, 18, 3, 64);

		uint32_t v[8];
		memcpy(v, g_sha256_hinit, 8*sizeof(uint32_t));
		uint32_t e = CalcRounds(m_w1, v, v, 16, 0, 61);				// We enough this
		if (g_sha256_hinit[7]+e == 0)
			return true;
#	ifdef X_DEBUG//!!!D
			TRC(0, "Last Hash word: "<< hex << HInit[7]+e);
			return true;
#	endif

	}
	return false;
#endif
}

Blob BitcoinSha256::FullCalc() {
	memcpy(m_w1, m_midstate_after_3, 8*sizeof(uint32_t));		
	CalcRounds(m_w, m_midstate, m_w1, 18, 3, 64);

	uint32_t v[8];
	memcpy(v, g_sha256_hinit, 8*sizeof(uint32_t));
	CalcRounds(m_w1, v, v, 16, 0, 64);

	for (int i=0; i<8; ++i)
		v[i] = Fast_htonl(v[i]);
	return Blob(v, 8*sizeof(uint32_t));
}

} // Coin::

