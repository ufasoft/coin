/*######     Copyright (c) 1997-2013 Ufasoft  http://ufasoft.com  mailto:support@ufasoft.com,  Sergey Pavlov  mailto:dev@ufasoft.com #######################################
#                                                                                                                                                                          #
# This program is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation;  #
# either version 3, or (at your option) any later version. This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the      #
# implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details. You should have received a copy of the GNU #
# General Public License along with this program; If not, see <http://www.gnu.org/licenses/>                                                                               #
##########################################################################################################################################################################*/

#include <el/ext.h>

#include "hash.h"
#include "salsa20.h"

extern "C" {

#if UCFG_CPU_X86_X64
	static bool s_bHasSse2 = CpuInfo().HasSse2;
#endif

#ifdef _M_X64
	void _cdecl ScryptCore_x64_3way(UInt32 x[16*3*2], UInt32 alignedScratch[3*1024*32+32]);
#endif

} // "C"

namespace Ext { namespace Crypto {


void ShuffleForSalsa(UInt32 w[32], const UInt32 src[32]) {
	w[0]	= src[0	];
	w[1]	= src[5	];
	w[2]	= src[10];
	w[3]	= src[15];
	w[4]	= src[12];
	w[5]	= src[1	];
	w[6]	= src[6	];
	w[7]	= src[11];
	w[8]	= src[8	];
	w[9]	= src[13];
	w[10]	= src[2	];
	w[11]	= src[7	];
	w[12]	= src[4	];
	w[13]	= src[9	];
	w[14]	= src[14];
	w[15]	= src[3	];

	w[16+0]	 = src[16+0	];
	w[16+1]	 = src[16+5	];
	w[16+2]	 = src[16+10];
	w[16+3]	 = src[16+15];
	w[16+4]	 = src[16+12];
	w[16+5]	 = src[16+1	];
	w[16+6]	 = src[16+6	];
	w[16+7]	 = src[16+11];
	w[16+8]	 = src[16+8	];
	w[16+9]  = src[16+13];
	w[16+10] = src[16+2	];
	w[16+11] = src[16+7	];
	w[16+12] = src[16+4	];
	w[16+13] = src[16+9	];
	w[16+14] = src[16+14];
	w[16+15] = src[16+3	];
}

void UnShuffleForSalsa(UInt32 dst[32], const UInt32 w[32]) {
	dst[0]	= w[0];
	dst[5]	= w[1];
	dst[10] = w[2];
	dst[15] = w[3];
	dst[12]	= w[4];
	dst[1]	= w[5];
	dst[6]	= w[6];
	dst[11]	= w[7];
	dst[8]	= w[8];
	dst[13]	= w[9];
	dst[2]	= w[10];
	dst[7]	= w[11];
	dst[4]	= w[12];
	dst[9]	= w[13];
	dst[14]	= w[14];
	dst[3]	= w[15];		

	dst[16+0]	= w[16+0];
	dst[16+5]	= w[16+1];
	dst[16+10]  = w[16+2];
	dst[16+15]  = w[16+3];
	dst[16+12]	= w[16+4];
	dst[16+1]	= w[16+5];
	dst[16+6]	= w[16+6];
	dst[16+11]	= w[16+7];
	dst[16+8]	= w[16+8];
	dst[16+13]	= w[16+9];
	dst[16+2]	= w[16+10];
	dst[16+7]	= w[16+11];
	dst[16+4]	= w[16+12];
	dst[16+9]	= w[16+13];
	dst[16+14]	= w[16+14];
	dst[16+3]	= w[16+15];
}

void ScryptCore(UInt32 x[32], UInt32 alignedScratch[1024*32+32]) {
	DECLSPEC_ALIGN(128) UInt32 w[32];

	ShuffleForSalsa(w, x);

#if UCFG_USE_MASM && !defined(_M_X64)
	if (s_bHasSse2)
		ScryptCore_x86x64(w, alignedScratch);
	else
#endif
	{
		for (int i=0; i<1024; ++i) {
			memcpy(alignedScratch+i*32, w, 32*sizeof(UInt32));
			Salsa20Core(w, w+16, 8);
			Salsa20Core(w+16, w, 8);
		}
		for (int i=0; i<1024; ++i) {
			int j = w[16] & 1023;		// w[16] is fixed point
			UInt32 *p = alignedScratch+j*32;
			for (int k=0; k<32; ++k)
				w[k] ^= p[k];
			Salsa20Core(w, w+16, 8);
			Salsa20Core(w+16, w, 8);
		}
	}
	UnShuffleForSalsa(x, w);
}

hashval CalcPbkdf2Hash(const UInt32 *pIhash, const ConstBuf& data, int idx) {
	SHA256 sha;

	size_t size = 16*4+data.Size+4;
	UInt32 *buf = (UInt32*)alloca(size);
	memset(buf, 0x36, 16*4);
	for (int i=0; i<8; ++i)
		buf[i] ^= pIhash[i];
	memcpy(buf+16, data.P, data.Size);
	buf[16+data.Size/4] = htobe32(idx);

	hashval hv = sha.ComputeHash(ConstBuf(buf, size));
	memset(buf, 0x5C, 16*4);
	for (int i=0; i<8; ++i)
		buf[i] ^= pIhash[i];
	memcpy(buf+16, hv.constData(), hv.size());
	return sha.ComputeHash(ConstBuf(buf, 64+hv.size()));
}

CArray8UInt32 CalcSCryptHash(const ConstBuf& password) {
	ASSERT(password.Size == 80);

	SHA256 sha;

	hashval ihash = sha.ComputeHash(password);
	const UInt32 *pIhash = (UInt32*)ihash.constData();

	UInt32 x[32];
	for (int i=0; i<4; ++i) {
		hashval hv = CalcPbkdf2Hash(pIhash, password, i+1);
		memcpy(x+8*i, hv.constData(), 32);
	}
	AlignedMem am((1025*32)*sizeof(UInt32), 128);
	ScryptCore(x, (UInt32*)am.get());
	hashval hv = CalcPbkdf2Hash(pIhash, ConstBuf(x, sizeof x), 1);
	const UInt32 *p = (UInt32*)hv.constData();
	CArray8UInt32 r;
	for (int i=0; i<8; ++i)
		r[i] = p[i];
	return r;
}

static const UInt32
	s_InputPad[12][4] = {
		{ 0x80000000, 0x80000000, 0x80000000, 0x80000000 },
		{ 0, 0, 0, 0 },
		{ 0, 0, 0, 0 },
		{ 0, 0, 0, 0 },
		{ 0, 0, 0, 0 },
		{ 0, 0, 0, 0 },
		{ 0, 0, 0, 0 },
		{ 0, 0, 0, 0 },
		{ 0, 0, 0, 0 },
		{ 0, 0, 0, 0 },
		{ 0, 0, 0, 0 },
		{ 0x00000280, 0x00000280, 0x00000280, 0x00000280 }
	},

	s_InnerPad[12][4] = {
		{ 1, 2, 3, 4 },
		{ 0x80000000, 0x80000000, 0x80000000, 0x80000000 },
		{ 0, 0, 0, 0 },
		{ 0, 0, 0, 0 },
		{ 0, 0, 0, 0 },
		{ 0, 0, 0, 0 },
		{ 0, 0, 0, 0 },
		{ 0, 0, 0, 0 },
		{ 0, 0, 0, 0 },
		{ 0, 0, 0, 0 },
		{ 0, 0, 0, 0 },
		{ 0x000004A0, 0x000004A0, 0x000004A0, 0x000004A0 }
	},

	s_OuterPad[8][4] = {
		{ 0x80000000, 0x80000000, 0x80000000, 0x80000000 },
		{ 0, 0, 0, 0 },
		{ 0, 0, 0, 0 },
		{ 0, 0, 0, 0 },
		{ 0, 0, 0, 0 },
		{ 0, 0, 0, 0 },
		{ 0, 0, 0, 0 },
		{ 0x00000300, 0x00000300, 0x00000300, 0x00000300 }
	};

static DECLSPEC_ALIGN(128) const UInt32 s_IhashFinal_4way[16][4] = {
	{ 0x00000001, 0x00000001, 0x00000001, 0x00000001 },
	{ 0x80000000, 0x80000000, 0x80000000, 0x80000000 },
	{ 0, 0, 0, 0 },
	{ 0, 0, 0, 0 },
	{ 0, 0, 0, 0 },
	{ 0, 0, 0, 0 },
	{ 0, 0, 0, 0 },
	{ 0, 0, 0, 0 },
	{ 0, 0, 0, 0 },
	{ 0, 0, 0, 0 },
	{ 0, 0, 0, 0 },
	{ 0, 0, 0, 0 },
	{ 0, 0, 0, 0 },
	{ 0, 0, 0, 0 },
	{ 0, 0, 0, 0 },
	{ 0x00000620, 0x00000620, 0x00000620, 0x00000620 }
};

#if UCFG_CPU_X86_X64

void CalcPbkdf2Hash_80_4way(UInt32 dst[32], const UInt32 *pIhash, const ConstBuf& data) {
	SHA256 sha;
	DECLSPEC_ALIGN(128) UInt32
		ist[16][4], ost[8][4],
		buf4[32][4];

	UInt32 buf[32], ostate[8], istate[8], bufPasswd[20];
	for (int i=0; i<20; ++i)
		bufPasswd[i] = be32toh(((const UInt32*)data.P)[i]);

	for (int i=0; i<8; ++i)
		buf[i] = be32toh(pIhash[i] ^ 0x5C5C5C5C);
	memset(buf+8, 0x5C, 8*4);
	sha.InitHash(ostate);
	sha.HashBlock(ostate, (const byte*)buf, 0);

	for (int i=0; i<8; ++i)
		buf[i] = be32toh(pIhash[i] ^ 0x36363636);
	memset(buf+8, 0x36, 8*4);
	sha.InitHash(istate);
	sha.HashBlock(istate, (const byte*)buf, 0);
	sha.HashBlock(istate, (const byte*)bufPasswd, 0);

	for (int i=0; i<8; ++i) {
		ist[i][0] = ist[i][1] = ist[i][2] = ist[i][3] = istate[i];
		ost[i][0] = ost[i][1] = ost[i][2] = ost[i][3] = ostate[i];
	}
	for (int i=0; i<4; ++i)
		buf4[i][0] = buf4[i][1] = buf4[i][2] = buf4[i][3] = bufPasswd[16+i];
	memcpy(buf4[4], s_InnerPad, sizeof s_InnerPad);
	Sha256Update_4way_x86x64Sse2(ist, buf4);

	memcpy(ist[8], s_OuterPad, sizeof s_OuterPad);
	Sha256Update_4way_x86x64Sse2(ost, ist);
	for (int i=0; i<8; ++i)
		for (int j=0; j<4; ++j)
			dst[j*8+i] = swap32(ost[i][j]);
}

#endif // UCFG_CPU_X86_X64


#if defined(_M_X64) && UCFG_USE_MASM

array<CArray8UInt32, 3> CalcSCryptHash_80_3way(const UInt32 input[20]) {
	DECLSPEC_ALIGN(128) UInt32
		tstate[16][4],
		ostate[8][4],
		istate[8][4],
		bufPasswd[32][4],
		buf[32][4],
		w[3*16*2];

	for (int i=0; i<19; ++i)
		bufPasswd[i][0] = bufPasswd[i][1] = bufPasswd[i][2] = be32toh(input[i]);
	bufPasswd[19][0] = be32toh(input[19]);
	bufPasswd[19][1] = be32toh(input[19]+1);
	bufPasswd[19][2] = be32toh(input[19]+2);

	SHA256::Init4Way(tstate);
	memcpy(bufPasswd[20], s_InputPad, sizeof s_InputPad);
	Sha256Update_4way_x86x64Sse2(tstate, bufPasswd);
	Sha256Update_4way_x86x64Sse2(tstate, bufPasswd+16);

	SHA256::Init4Way(ostate);
	for (int i=0; i<8; ++i) {
		buf[i][0] = tstate[i][0] ^ 0x5C5C5C5C;
		buf[i][1] = tstate[i][1] ^ 0x5C5C5C5C;
		buf[i][2] = tstate[i][2] ^ 0x5C5C5C5C;
		buf[i][3] = tstate[i][3] ^ 0x5C5C5C5C;
	}
	memset(buf+8, 0x5C, 8*16);
	Sha256Update_4way_x86x64Sse2(ostate, buf);
	
	for (int i=0; i<8; ++i) {
		buf[i][0] = tstate[i][0] ^ 0x36363636;
		buf[i][1] = tstate[i][1] ^ 0x36363636;
		buf[i][2] = tstate[i][2] ^ 0x36363636;
		buf[i][3] = tstate[i][3] ^ 0x36363636;
	}
	memset(buf+8, 0x36, 8*16);
	SHA256::Init4Way(tstate);
	Sha256Update_4way_x86x64Sse2(tstate, buf);

	memcpy(istate, tstate, sizeof istate);
	Sha256Update_4way_x86x64Sse2(istate, bufPasswd);

	for (int k=0; k<3; ++k) {
		DECLSPEC_ALIGN(128) UInt32 ist[16][4], ost[8][4];

		for (int i=0; i<8; ++i) {
			ist[i][0] = ist[i][1] = ist[i][2] = ist[i][3] = istate[i][k];
			ost[i][0] = ost[i][1] = ost[i][2] = ost[i][3] = ostate[i][k];
		}
		for (int i=0; i<4; ++i)
			buf[i][0] = buf[i][1] = buf[i][2] = buf[i][3] = bufPasswd[16+i][k];
		memcpy(buf[4], s_InnerPad, sizeof s_InnerPad);
		Sha256Update_4way_x86x64Sse2(ist, buf);

		memcpy(ist[8], s_OuterPad, sizeof s_OuterPad);
		Sha256Update_4way_x86x64Sse2(ost, ist);

		UInt32 *d = w+32*k;
		d[0]		= swap32(ost[0][0]);
		d[1]		= swap32(ost[5][0]);
		d[2]		= swap32(ost[2][1]);
		d[3]		= swap32(ost[7][1]);
		d[4]		= swap32(ost[4][1]);
		d[5]		= swap32(ost[1][0]);
		d[6]		= swap32(ost[6][0]);
		d[7]		= swap32(ost[3][1]);
		d[8]		= swap32(ost[0][1]);
		d[9]		= swap32(ost[5][1]);
		d[10]		= swap32(ost[2][0]);
		d[11]		= swap32(ost[7][0]);
		d[12]		= swap32(ost[4][0]);
		d[13]		= swap32(ost[1][1]);
		d[14]		= swap32(ost[6][1]);
		d[15]		= swap32(ost[3][0]);

		d[16+0]		= swap32(ost[0][2]);
		d[16+1]		= swap32(ost[5][2]);
		d[16+2]		= swap32(ost[2][3]);
		d[16+3]		= swap32(ost[7][3]);
		d[16+4]		= swap32(ost[4][3]);
		d[16+5]		= swap32(ost[1][2]);
		d[16+6]		= swap32(ost[6][2]);
		d[16+7]		= swap32(ost[3][3]);
		d[16+8]		= swap32(ost[0][3]);
		d[16+9]		= swap32(ost[5][3]);
		d[16+10]	= swap32(ost[2][2]);
		d[16+11]	= swap32(ost[7][2]);
		d[16+12]	= swap32(ost[4][2]);
		d[16+13]	= swap32(ost[1][3]);
		d[16+14]	= swap32(ost[6][3]);
		d[16+15]	= swap32(ost[3][2]);
	}
	AlignedMem am((3*1025*32)*sizeof(UInt32), 128);
	ScryptCore_x64_3way(w, (UInt32*)am.get());
	UInt32 *s = w;
	
	for (int k=0; k<3; ++k) {	
		buf[0  ][k] = swap32(s[0]);
		buf[5  ][k] = swap32(s[1]);
		buf[10 ][k] = swap32(s[2]);
		buf[15 ][k] = swap32(s[3]);
		buf[12 ][k] = swap32(s[4]);
		buf[1  ][k] = swap32(s[5]);
		buf[6  ][k] = swap32(s[6]);
		buf[11 ][k] = swap32(s[7]);
		buf[8  ][k] = swap32(s[8]);
		buf[13 ][k] = swap32(s[9]);
		buf[2  ][k] = swap32(s[10]);
		buf[7  ][k] = swap32(s[11]);
		buf[4  ][k] = swap32(s[12]);
		buf[9  ][k] = swap32(s[13]);
		buf[14 ][k] = swap32(s[14]);
		buf[3  ][k] = swap32(s[15]);
		s += 32;
	}
	Sha256Update_4way_x86x64Sse2(tstate, buf);

	s = w+16;
	
	for (int k=0; k<3; ++k) {	
		buf[0  ][k] = swap32(s[0]);
		buf[5  ][k] = swap32(s[1]);
		buf[10 ][k] = swap32(s[2]);
		buf[15 ][k] = swap32(s[3]);
		buf[12 ][k] = swap32(s[4]);
		buf[1  ][k] = swap32(s[5]);
		buf[6  ][k] = swap32(s[6]);
		buf[11 ][k] = swap32(s[7]);
		buf[8  ][k] = swap32(s[8]);
		buf[13 ][k] = swap32(s[9]);
		buf[2  ][k] = swap32(s[10]);
		buf[7  ][k] = swap32(s[11]);
		buf[4  ][k] = swap32(s[12]);
		buf[9  ][k] = swap32(s[13]);
		buf[14 ][k] = swap32(s[14]);
		buf[3  ][k] = swap32(s[15]);
		s += 32;
	}
	Sha256Update_4way_x86x64Sse2(tstate, buf);
	
	Sha256Update_4way_x86x64Sse2(tstate, s_IhashFinal_4way);

	memcpy(tstate[8], s_OuterPad, sizeof s_OuterPad);
	Sha256Update_4way_x86x64Sse2(ostate, tstate);
	
	array<CArray8UInt32, 3> r;
	for (int i=0; i<8; ++i) {
		r[0][i] = htobe32(ostate[i][0]);
		r[1][i] = htobe32(ostate[i][1]);
		r[2][i] = htobe32(ostate[i][2]);
	}
	return r;
}

#endif // _M_X64

}} // Ext::Crypto::

