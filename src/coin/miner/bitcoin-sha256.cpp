/*######   Copyright (c) 2013-2015 Ufasoft  http://ufasoft.com  mailto:support@ufasoft.com,  Sergey Pavlov  mailto:dev@ufasoft.com ####
#                                                                                                                                     #
# 		See LICENSE for licensing information                                                                                         #
#####################################################################################################################################*/

#include <el/ext.h>

#include <el/libext/ext-net.h>


#include "bitcoin-sha256sse.h"

extern "C" {

extern const uint32_t g_sha256_hinit[8];
extern const uint32_t g_sha256_k[64];
extern uint32_t g_4sha256_k[64][4];			// __m128i

DECLSPEC_ALIGN(32) const uint32_t g_sha256_hinit[8] = {
	0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a, 0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19
};

DECLSPEC_ALIGN(32) const uint32_t g_sha256_k[64] = {
	0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, //  0
	0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
	0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, //  8
	0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
	0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, // 16
	0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
	0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, // 24
	0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
	0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, // 32
	0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
	0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, // 40
	0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
	0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, // 48
	0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
	0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, // 56
	0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2
};

DECLSPEC_ALIGN(64) uint32_t g_4sha256_k[64][4];

static struct Sha256SSEInit {
	Sha256SSEInit() {
		for (int i = 0; i < 64; ++i)
			::g_4sha256_k[i][0] = ::g_4sha256_k[i][1] = ::g_4sha256_k[i][2] = ::g_4sha256_k[i][3] = ::g_sha256_k[i];
	}
} s_sha256SSEInit;


} // "C"

namespace Coin {


ptr<BitcoinSha256> BitcoinSha256::CreateObject() {
#if UCFG_BITCOIN_ASM
	if (CpuInfo().Features.SSE2)
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
		m_preVal4 = h + (Rotr32(e, 6) ^ Rotr32(e, 11) ^ Rotr32(e, 25)) + ((e & f) ^ AndNot(e, g)) + ::g_sha256_k[3];
		m_t1 = (Rotr32(a, 2) ^ Rotr32(a, 13) ^ Rotr32(a, 22)) + ((a & b) ^ (b & c) ^ (a & c));
	}

	memcpy(m_w1, hash1, 16*sizeof(uint32_t));

	/*!!!
	uint32_t bits = 256;
	*(uint8_t*)&m_w1[8] = 0x80;
	uint8_t *p = (uint8_t*)&m_w1[63]+3;			// swap for LE-processing. Original SHA-2 is BE
	p[3] = uint8_t(bits >> 24);
	p[2] = uint8_t(bits >> 16);
	p[1] = uint8_t(bits >> 8);
	p[0] = uint8_t(bits);
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
	memcpy(v, ::g_sha256_hinit, 8*sizeof(uint32_t));
	CalcRounds(m_w1, v, v, 16, 0, 64);

	for (int i=0; i<8; ++i)
		v[i] = Fast_htonl(v[i]);
	return Blob(v, 8*sizeof(uint32_t));
}

} // Coin::

