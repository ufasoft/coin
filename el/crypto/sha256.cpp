/*######     Copyright (c) 1997-2013 Ufasoft  http://ufasoft.com  mailto:support@ufasoft.com,  Sergey Pavlov  mailto:dev@ufasoft.com #######################################
#                                                                                                                                                                          #
# This program is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation;  #
# either version 3, or (at your option) any later version. This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the      #
# implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details. You should have received a copy of the GNU #
# General Public License along with this program; If not, see <http://www.gnu.org/licenses/>                                                                               #
##########################################################################################################################################################################*/

#include <el/ext.h>

// This file should be included in each module, which uses it because problems with exporting constant data

#include "hash.h"

using namespace Ext;

namespace Ext { namespace Crypto {

const UInt32 g_sha256_hinit[8] = {
	0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a, 0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19
};

const UInt32 g_sha256_k[64] = {
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

DECLSPEC_ALIGN(64) UInt32 g_4sha256_k[64][4];

static struct Sha256SSEInit {
	Sha256SSEInit() {
		for (int i=0; i<64; ++i)
			g_4sha256_k[i][0] = g_4sha256_k[i][1] = g_4sha256_k[i][2] = g_4sha256_k[i][3] = g_sha256_k[i];
	}
} s_sha256SSEInit;


void SHA256::Init4Way(UInt32 state[8][4]) {
	for (int i=0; i<8; ++i)
		state[i][0] = state[i][1] = state[i][2] = state[i][3] = g_sha256_hinit[i];
}

void SHA256::InitHash(void *dst) {
	memcpy(dst, g_sha256_hinit, sizeof(g_sha256_hinit));
}

#pragma optimize( "t", on)

void SHA256::HashBlock(void *dst, const byte *src, UInt64 counter) noexcept {
	UInt32 *p = (UInt32*)dst;
#if UCFG_USE_MASM
	Sha256Update_x86x64(p, (const UInt32*)src);
#else
	UInt32 w[16];
	memcpy(w, src, sizeof(w));

	UInt32 a = p[0], b = p[1], c = p[2], d = p[3], e = p[4], f = p[5], g = p[6], h = p[7];

	UInt32 b_c = b ^ c;
	for (int i=0; i<64; ++i) {
		if (i >= 16) {
			UInt32 w_15 = w[(i-15) & 15], w_2 = w[(i-2) & 15];
			w[i & 15] += (Rotr32(w_15, 7) ^ Rotr32(w_15, 18) ^ (w_15 >> 3)) + w[(i-7) & 15] + (Rotr32(w_2, 17) ^ Rotr32(w_2, 19) ^ (w_2 >> 10));
		}

		UInt32 t1 = h + (Rotr32(e, 6) ^ Rotr32(e, 11) ^ Rotr32(e, 25)) + ((e & f) ^ (~e & g)) + g_sha256_k[i] + w[i & 15];
		h = g; g = f; f = e;
		e = d + t1;
		UInt32 a_b = a ^ b;
		d = c; c = b; b = a;
		a = t1 + (Rotr32(a, 2) ^ Rotr32(a, 13) ^ Rotr32(a, 22)) + ((a_b & exchange(b_c, a_b)) ^ c);											//	((a & c) ^ (a & d) ^ (c & d));
	}
	
	p[0] += a; p[1] += b; p[2] += c; p[3] += d; p[4] += e; p[5] += f; p[6] += g; p[7] += h;
#endif
}

#if UCFG_CPU_X86_X64 && !UCFG_USE_MASM
extern "C" void _cdecl Sha256Update_4way_x86x64Sse2(UInt32 state[8][4], const UInt32 data[16][4]) {
	SHA256 sha;
	UInt32 p[8], q[16];
	for (int i=0; i<4; ++i) {
		for (int j=0; j<8; ++j) {
			p[j] = state[j][i];
			q[j*2] = data[j*2][i];
			q[j*2+1] = data[j*2+1][i];
		}
		sha.HashBlock(p, (const byte*)q, 0);
		for (int j=0; j<8; ++j)
			state[j][i] = p[j];		
	}
}
#endif // UCFG_CPU_X86_X64 && !UCFG_USE_MASM



}} // Ext::Crypto::


