#include <el/ext.h>

// MurmurHash3, MurmurHashAligned2 were written by Austin Appleby, and is placed in the PUBLIC DOMAIN.
// The author hereby disclaims copyright to this source code.

namespace Ext {

__forceinline uint32_t fmix32(uint32_t h) {
	h ^= h >> 16;
	h *= 0x85ebca6b;
	h ^= h >> 13;
	h *= 0xc2b2ae35;
	h ^= h >> 16;
	return h;
}

#define MIX(h,k,m) { k *= m; k ^= k >> r; k *= m; h *= m; h ^= k; }

unsigned int MurmurHashAligned2(const ConstBuf& cbuf, UInt32 seed) {
	const unsigned int m = 0x5bd1e995;
	const int r = 24;
	size_t len = cbuf.Size;

	const byte *data = cbuf.P;

	unsigned int h = seed ^ len;

	const size_t align = (int)(UInt64)data & 3;

	if (align && (len >= 4)) {
		// Pre-load the temp registers

		unsigned int t = 0, d = 0;

		switch (align) {
		case 1: t |= data[2] << 16;
		case 2: t |= data[1] << 8;
		case 3: t |= data[0];
		}

		t <<= (8 * align);

		data += 4-align;
		len -= 4-align;

		int sl = 8 * (4-align);
		int sr = 8 * align;

		// Mix

		while(len >= 4) {
			d = *(unsigned int *)data;
			t = (t >> sr) | (d << sl);

			unsigned int k = t;

			MIX(h,k,m);

			t = d;

			data += 4;
			len -= 4;
		}

		// Handle leftover data in temp registers

		d = 0;

		if(len >= align) {
			switch(align) {
			case 3: d |= data[2] << 16;
			case 2: d |= data[1] << 8;
			case 1: d |= data[0];
			}

			unsigned int k = (t >> sr) | (d << sl);
			MIX(h,k,m);

			data += align;
			len -= align;

			//----------
			// Handle tail bytes

			switch(len) {
			case 3: h ^= data[2] << 16;
			case 2: h ^= data[1] << 8;
			case 1: h ^= data[0];
				h *= m;
			};
		}
		else {
			switch(len) {
			case 3: d |= data[2] << 16;
			case 2: d |= data[1] << 8;
			case 1: d |= data[0];
			case 0: h ^= (t >> sr) | (d << sl);
				h *= m;
			}
		}

		h ^= h >> 13;
		h *= m;
		h ^= h >> 15;

		return h;
	} else {
		while(len >= 4) {
			unsigned int k = *(unsigned int *)data;

			MIX(h,k,m);

			data += 4;
			len -= 4;
		}

		//----------
		// Handle tail bytes

		switch(len) {
		case 3: h ^= data[2] << 16;
		case 2: h ^= data[1] << 8;
		case 1: h ^= data[0];
			h *= m;
		};

		h ^= h >> 13;
		h *= m;
		h ^= h >> 15;

		return h;
	}
}

UInt32 MurmurHash3_32(const ConstBuf& cbuf, UInt32 seed) {
	const byte *data = cbuf.P;
	size_t len = cbuf.Size,
		nblocks = len / 4;
	UInt32 h1 = seed;
	const UInt32 c1 = 0xcc9e2d51,
			c2 = 0x1b873593,
			*blocks = (const UInt32 *)(data + nblocks*4);

	for (int i = -int(nblocks); i; i++) {
		uint32_t k1 = GetLeUInt32(blocks + i);
		k1 *= c1;
		k1 = _rotl(k1, 15);
		k1 *= c2;
		h1 ^= k1;
		h1 = _rotl(h1, 13); 
		h1 = h1*5+0xe6546b64;
	}

	data += nblocks*4;
	UInt32 k1 = 0;
	switch(len & 3) {
	case 3: k1 ^= data[2] << 16;
	case 2: k1 ^= data[1] << 8;
	case 1: k1 ^= data[0];
		k1 *= c1; k1 = _rotl(k1,15); k1 *= c2; h1 ^= k1;
	}
	return fmix32(h1 ^ len);
} 

} // Ext::
