#include <el/ext.h>

#include <el/crypto/hash.h>
using namespace Crypto;

#include "util.h"

namespace Coin {

UInt32 PHI  = 0x9e3779b9;
const int BLOCKHASH_1_PADSIZE = 1024*1024*4;
static UInt32 BlockHash_1_Q[4096], BlockHash_1_c, BlockHash_1_i;

UInt32 BlockHash_1_rand() {
    UInt32 x, r = 0xfffffffe;
    UInt64 t, a = 18782LL;
    BlockHash_1_i = (BlockHash_1_i + 1) & 4095;
    t = a * BlockHash_1_Q[BlockHash_1_i] + BlockHash_1_c;
    BlockHash_1_c = (t >> 32);
    x = UInt32(t + BlockHash_1_c);
    if (x < BlockHash_1_c) {
        x++;
        BlockHash_1_c++;
    }
    return (BlockHash_1_Q[BlockHash_1_i] = r - x);
}

static const char SomeArrogantText1[] = "Back when I was born the world was different. As a kid I could run around the streets, build things in the forest, go to the beach and generally live a care free life. Sure I had video games and played them a fair amount but they didn't get in the way of living an adventurous life. The games back then were different too. They didn't require 40 hours of your life to finish. Oh the good old days, will you ever come back?",
				  SomeArrogantText2[] = "Why do most humans not understand their shortcomings? The funny thing with the human brain is it makes everyone arrogant at their core. Sure some may fight it more than others but in every brain there is something telling them, HEY YOU ARE THE MOST IMPORTANT PERSON IN THE WORLD. THE CENTER OF THE UNIVERSE. But we can't all be that, can we? Well perhaps we can, introducing GODria, take 2 pills of this daily and you can be like RealSolid, lord of the universe.",
                  SomeArrogantText3[] = "What's up with kids like artforz that think it's good to attack other's work? He spent a year in the bitcoin scene riding on the fact he took some other guys SHA256 opencl code and made a miner out of it. Bravo artforz, meanwhile all the false praise goes to his head and he thinks he actually is a programmer. Real programmers innovate and create new work, they win through being better coders with better ideas. You're not real artforz, and I hear you like furries? What's up with that? You shouldn't go on IRC when you're drunk, people remember the weird stuff.";

static byte *BlockHash_1_MemoryPAD8;
static UInt32 *BlockHash_1_MemoryPAD32;

__forceinline byte READ_PAD8(UInt64 offset) { return BlockHash_1_MemoryPAD8[offset & (BLOCKHASH_1_PADSIZE-1)]; }

__forceinline UInt32 READ_PAD32(UInt64 offset) {
	UInt32 UNALIGNED *p = (UInt32*)&BlockHash_1_MemoryPAD8[offset & (BLOCKHASH_1_PADSIZE-1)];
	return *p;
}

static byte s_READ_PAD8_BySquare[1024];

static struct SolidcoinHashInit {
	mutex Mtx;

	~SolidcoinHashInit() {
		if (BlockHash_1_MemoryPAD8)
			delete[] BlockHash_1_MemoryPAD32;
	}

	void Init() {
		BlockHash_1_MemoryPAD32 = new UInt32[BLOCKHASH_1_PADSIZE/4+2];  //need the +8 for memory overwrites 
		byte *pb = (byte*)BlockHash_1_MemoryPAD32;
		BlockHash_1_Q[0] = 0x6970F271;
		BlockHash_1_Q[1] = UInt32(0x6970F271 + PHI);
		BlockHash_1_Q[2] = UInt32(0x6970F271 + PHI + PHI);
		for (int i = 3; i < 4096; i++)
			BlockHash_1_Q[i] = BlockHash_1_Q[i - 3] ^ BlockHash_1_Q[i - 2] ^ PHI ^ i;
		BlockHash_1_c= 362436;
		BlockHash_1_i= 4095;

		int count1=0,count2=0,count3=0;
		for (int x=0; x<(BLOCKHASH_1_PADSIZE/4)+2; x++)
			BlockHash_1_MemoryPAD32[x] = BlockHash_1_rand();
		for (int x=0; x<BLOCKHASH_1_PADSIZE+8; x++) {
			switch (pb[x] & 3) {
				case 0:
					pb[x] ^= SomeArrogantText1[count1++];
					if (count1 >= sizeof(SomeArrogantText1))
						count1 = 0;
					break;
				case 1:
					pb[x] ^= SomeArrogantText2[count2++];
					if (count2 >= sizeof(SomeArrogantText2))
						count2 = 0;
					break;
				case 2:
					pb[x] ^= SomeArrogantText3[count3++];
					if (count3 >= sizeof(SomeArrogantText3))
						count3 = 0;
					break;
				case 3:
					pb[x] ^= 0xAA;
					break;
			}
		}
		BlockHash_1_MemoryPAD8 = pb;			// assign pointer only after initialization
		for (int i=0; i<_countof(s_READ_PAD8_BySquare); ++i)
			s_READ_PAD8_BySquare[i] = READ_PAD8(i*i);
	}
} s_solicoinHashInit;

class ValueRem320 {
public:
	static const int DIVISOR = 320;

	ValueRem320(UInt64 v) {
		operator=(v);
	}

	operator UInt64() const { return m_val; }
	int Rem() const { return m_rem; }

	int HalfRem() const {
		return m_rem < DIVISOR/2 ? m_rem : m_rem-DIVISOR/2;
	}

	ValueRem320& operator=(UInt64 v) {
		m_val = v;
		m_rem = m_val % DIVISOR;
		return _self;
	}

	ValueRem320& operator+=(byte v) {
		m_val += v;
		m_rem = (m_rem+v) % DIVISOR;
		return _self;
	}

	ValueRem320& operator-=(byte v) {
		m_val -= v;
		m_rem = (m_rem+320-v) % DIVISOR;
		return _self;
	}

	ValueRem320& operator+=(UInt32 v) {
		m_val += v;
		m_rem = (m_rem + (v % DIVISOR)) % DIVISOR;
		return _self;
	}

private:
	UInt64 m_val;
	int m_rem;
};

HashValue SolidcoinHash(const ConstBuf& cbuf) {
	ASSERT(cbuf.Size == 128);

	if (!BlockHash_1_MemoryPAD8) {
		EXT_LOCK (s_solicoinHashInit.Mtx) {
			if (!BlockHash_1_MemoryPAD8)
				s_solicoinHashInit.Init();
		}
	}

	byte work1[512];
	memcpy(work1, cbuf.P, cbuf.Size);			// 0->127   is the block header      (128)
    byte *work2=work1+128;						// 128->191 is blake(blockheader)    (64)
    byte *work3=work1+192;						// 192->511 is scratch work area     (320)

	Blob blakeHash = Blake512().ComputeHash(ConstBuf(work1, 128));
	ASSERT(blakeHash.Size == 64);
	memcpy(work2, blakeHash.constData(), 64);
    
    work3[0] = work2[15];
    for(int x=1;x<320;x++) {						//setup the 320 scratch with some base values
        work3[x-1] ^= work2[x&63];
		work3[x]= work3[x-1]<0x80 ? work2[(x+work3[x-1])&63] : work1[(x+work3[x-1]) & 127];
    }

    UInt64 qCount = *((UInt64*)&work3[310]);
    int nExtra = READ_PAD8(qCount+work3[300])>>3;
    for (UInt16 x=1; x<512+nExtra; x++) {
        qCount += READ_PAD32(qCount);
        if (UInt32(qCount) & 0x87878700)
			work3[qCount % 320]++;

        qCount -= READ_PAD8(qCount + work3[qCount % 160]);
        if (qCount & 0x80000000)
			qCount += READ_PAD8(qCount & 0x8080FFFF);
        else
			qCount += READ_PAD32(qCount & 0x7F60FAFB);

        qCount += READ_PAD32(qCount + work3[qCount % 160]);
        if (UInt32(qCount) & 0xF0000000)
			work3[qCount % 320]++;

		UInt32 UNALIGNED *p = (UInt32*)&work3[qCount & 0xFF];
        qCount += READ_PAD32(*p);
        work3[x % 320] = byte(work2[x & 63] ^ qCount);

        qCount += READ_PAD32((qCount>>32) + work3[x % 200]);
		p = (UInt32*)&work3[qCount % 316];
        *p ^= (qCount>>24) & 0xFFFFFFFF;
        x += ((qCount & 7) == 3);
        qCount -= s_READ_PAD8_BySquare[x];
		x += ((qCount & 7) == 1);
    }
	return SHA256().ComputeHash(ConstBuf(work1, 512));
}


} // Coin::

