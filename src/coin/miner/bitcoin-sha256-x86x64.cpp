/*######     Copyright (c) 1997-2013 Ufasoft  http://ufasoft.com  mailto:support@ufasoft.com,  Sergey Pavlov  mailto:dev@ufasoft.com #######################################
#                                                                                                                                                                          #
# This program is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation;  #
# either version 3, or (at your option) any later version. This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the      #
# implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details. You should have received a copy of the GNU #
# General Public License along with this program; If not, see <http://www.gnu.org/licenses/>                                                                               #
##########################################################################################################################################################################*/

#include <el/ext.h>

#include "bitcoin-sha256sse.h"

/*!!!R
#if defined(_WIN64) || defined(_M_IX86_FP) && _M_IX86_FP >= 2

__m128i g_4sha256_k[64];

#endif */

namespace Coin {

#if defined(_WIN64) || defined(_M_IX86_FP) && _M_IX86_FP >= 2 && UCFG_BITCOIN_ASM

void SseBitcoinSha256::PrepareData(const void *midstate, const void *data, const void *hash1) {
	base::PrepareData(midstate, data, hash1);

	for (int i=0; i<8; ++i)
		m_4midstate[i] = _mm_set1_epi32(m_midstate[i]);

	for (int i=0; i<=17; ++i)
		m_4w[i] = _mm_set1_epi32(m_w[i]);

	for (int i=0; i<64; ++i)
		m_4w[i+64] = _mm_set1_epi32(m_w1[i]);
}


#if UCFG_BITCOIN_ASM

bool SseBitcoinSha256::FindNonce(UInt32& nonce) {

#if UCFG_BITCOIN_ASM
	return CalcSha256Sse(m_4w, m_4midstate, m_midstate_after_3, UCFG_BITCOIN_NPAR, nonce);

#else
	__m128i *m_4w1 = &m_4w[16*UCFG_BITCOIN_WAY];	//!!!?

	__m128i offset = _mm_set_epi32(3, 2, 1, 0);
	for (int i=0; i<UCFG_BITCOIN_NPAR; i+=UCFG_BITCOIN_WAY, nonce+=UCFG_BITCOIN_WAY) {
		m_4w[3] = _mm_set1_epi32(nonce)+offset;
		for (int j=0; j<8; ++j)
			m_4w1[j] = _mm_set1_epi32(m_midstate_after_3[j]);
#if UCFG_BITCOIN_ASM
		CalcSha256Sse(m_4w, m_4midstate, m_4w1, 18, 3, 64);
#else
		CalcRounds(m_4w, m_4midstate, m_4w1, 18, 3, 64);
#endif

#if UCFG_BITCOIN_WAY==6 || UCFG_BITCOIN_WAY==8
		__m128i v[16];
		for (int j=0; j<8; ++j)
			v[j*2] = v[j*2+1] = _mm_set1_epi32(g_sha256_hinit[j]);
#else
		__m128i v[8];
		for (int j=0; j<8; ++j)
			v[j] = _mm_set1_epi32(g_sha256_hinit[j]);
#endif
#if UCFG_BITCOIN_ASM
		__m128i e = CalcSha256Sse(m_4w1, v, v, 16, 0, 61);
#else
		__m128i e = CalcRounds(m_4w1, v, v, 16, 0, 61);				// We enough this
#endif
		__m128i p = _mm_cmpeq_epi32(e + _mm_set1_epi32(g_sha256_hinit[7]), _mm_setzero_si128());
		UInt64 *p64 = (UInt64*)&p;
		if (p64[0] | p64[1]) {
			if (_mm_extract_epi16(p, 0) != 0)
				return true;
			if (_mm_extract_epi16(p, 2) != 0) {
				nonce += 1;
				return true;
			}
			if (_mm_extract_epi16(p, 4) != 0) {
				nonce += 2;
				return true;
			}
			if (_mm_extract_epi16(p, 6) != 0) {
				nonce += 3;
				return true;
			}
		}

	}
	return false;
#endif
}

#endif // UCFG_BITCOIN_ASM

#endif // M_IX86_FP > 2

} // Coin::


