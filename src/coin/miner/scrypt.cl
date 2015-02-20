// scrypt implementation for LiteCoin mining

#define MODIFIER

void Salsa20(MODIFIER uint4 *dst, MODIFIER const uint4 *x) {
	uint4 v0 = dst[0] ^ x[0];
	uint4 v1 = dst[1] ^ x[1];
	uint4 v2 = dst[2] ^ x[2];
	uint4 v3 = dst[3] ^ x[3];

	uint4 w0 = v0;
	uint4 w1 = v1;
	uint4 w2 = v2;
	uint4 w3 = v3;

	for (int i=0; i<8; ++i) {
		w3 ^= rotate(w0 + w1, 7);
		w2 ^= rotate(w3 + w0, 9);
		w1 ^= rotate(w2 + w3, 13);
		w0 ^= rotate(w1 + w2, 18);

		uint4 w = w1;
		w1 = w3.s3012;
		w2 = w2.s2301;
		w3 =  w.s1230;
	}

	dst[0] = v0 + w0;
	dst[1] = v1 + w1;
	dst[2] = v2 + w2;
	dst[3] = v3 + w3;
}

void DoubleSalsa20(MODIFIER uint4 x[8]) {
	Salsa20(x, x+4);
	Salsa20(x+4, x);
}

void memcpyToGlobal8(__global uint4 *d, MODIFIER const uint4 *s) {
	for (int i=0; i<8; ++i)
		d[i] = s[i];
}

__kernel void ScryptCore(__global uint16 *iobuf, __global uint16 *scratch) {
	uint id = get_global_id(0);
	uint lid = get_local_id(0);
	__global uint4 *io = (__global uint4 *)(iobuf + id*2);
	__global uint4 *p = (__global uint4 *)(scratch + id*1024*2);

    uint4 x[8];
	for (int i=0; i<8; ++i)
		x[i] = io[i];

	__global uint4 *pp = p;
	for (int i=0; i<1024; ++i, pp+=8) {
		memcpyToGlobal8(pp, x);
		DoubleSalsa20(x);
	}	
	for (int i=0; i<1024; ++i) {
		__global uint4 *q = p + (x[4].s0 & 1023)*8;
		for (int i=0; i<8; ++i)
			x[i] ^= q[i];
		DoubleSalsa20(x);
	}

	memcpyToGlobal8(io, x);
} 

/*!!!T
void ScryptCoreInternal(uint8 x[4]) {

}

__constant uint K[64] = { 
    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
    0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
    0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
    0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
    0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
    0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
    0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
    0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2
};

const uint8 H = (0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a, 0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19);

uint8 SHA256(uint8 state, uint8 block0, uint8 block1) {
	uint w[64+9] = {  state.s7,  state.s6,   state.s5,	 state.s4,	 state.s3,	state.s2,	 state.s1,	 state.s0,
					  0, 
					  block0.s0, block0.s1,  block0.s2,  block0.s3,  block0.s4,  block0.s5,  block0.s6,  block0.s7,
				      block1.s0, block1.s1,  block1.s2,  block1.s3,  block1.s4,  block1.s5,  block1.s6,  block1.s7 };

#	define a w[i+7]
#	define b w[i+6]
#	define c w[i+5]
#	define d w[i+4]
#	define e w[i+3]
#	define f w[i+2]
#	define g w[i+1]
#	define h w[i+0]

	for (int i=0; i<64; ++i) {
		if (i >= 16) {
			uint w15 = w[i-15+9],
				w2 = w[i-2+9];
			w[i+9] = w[i-16+9] + w[i-7+9] + (rotate(w15, 25) ^ rotate(w15, 14) ^ (w15 >> 3)) + (rotate(w2, 15) ^ rotate(w2, 13) ^ (w2 >> 10));
		}
		uint e1 = e,
			a1 = a,
			t1 = h + (rotate(e1, 26) ^ rotate(e1, 21) ^ rotate(e1, 7)) + bitselect(g, f, e1) + K[i] + w[i+9];
		d += t1;
		w[i+8] = t1 + (rotate(a1, 30) ^ rotate(a1, 19) ^ rotate(a1, 10)) + bitselect(b, c, a1^b);
	}

#	undef a
#	undef b
#	undef c
#	undef d
#	undef e
#	undef f
#	undef g
#	undef h

	return state + (uint8)(w[71], w[70], w[69], w[68], w[67], w[66], w[65], w[64]);
}

uint8 SHA256_fresh(uint8 block0, uint8 block1) {
	return SHA256(H, block0, block1);
}

uint Swab(uint x) {
	return (uint)(((uchar4)x).wzyx);
}

__kernel void Scrypt(const uint8 * restrict input, volatile __global uint * restrict output, const uint8 midstate, const uint target) {
	uint gid = get_global_id(0);
	uint8 data = (input[2].x, input[2].y, input[2].z, gid, 0x80000000, 0, 0, 0),
		pad = SHA256(midstate, data, (uint8)(0, 0, 0, 0, 0, 0, 0, 0x280)),
		ostate = SHA256_fresh(pad ^ 0x5C5C5C5C, 0x5C5C5C5C),
		tmp = SHA256_fresh(pad ^ 0x36363636, 0x36363636),
		tstate = SHA256(tmp, input[0], input[1]);
	uint8 x[4];
	for (uint i=0; i<4; i++) {
		x[i] = SHA256(ostate, SHA256(tstate, (uint8)(data.x, data,.y, data.z, data.w, i+1, 0x80000000, 0, 0),
											 (uint8)(0, 0, 0, 0, 0, 0, 0, 0x4A0)),
    						  (uint8)(0x80000000, 0, 0, 0, 0, 0, 0, 0x300));
	}
	ScyptCoreInternal(x);


    tstate = SHA256(tstate, x[0], x[1]);
    tstate = SHA256(tstate, x[2], x[3]);
	tstate = SHA256(tstate, (uint8)(1, 0, 0, 0, 0, 0, 0, 0), (uint8)(0x80000000, 0, 0, 0, 0, 0, 0, 0x520));
	uint8 r = SHA256(tstate, tmp, (uint8)(0x80000000, 0, 0, 0, 0, 0, 0, 0x300));
	if (Swab(r.s7) <= target)
		output[output[255]++] = gid;
}

*/






