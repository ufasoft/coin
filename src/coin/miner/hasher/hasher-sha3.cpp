#include <el/ext.h>

#include "miner.h"

namespace Coin {

static SHA3<256> s_sha3;

class Sha3Hasher : public Hasher {
public:
	Sha3Hasher()
		: Hasher("keccak", HashAlgo::Sha3)
	{}

	HashValue CalcHash(RCSpan cbuf) override {
		return HashValue(s_sha3.ComputeHash(cbuf));
	}
} g_sha3Hasher;

} // Coin::
