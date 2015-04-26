#include <el/ext.h>

#include "miner.h"

namespace Coin {

class Sha3Hasher : public Hasher {
public:
	Sha3Hasher()
		:	Hasher("keccak", HashAlgo::Sha3)
	{}

	HashValue CalcHash(const ConstBuf& cbuf) override {
		return HashValue(SHA3<256>().ComputeHash(cbuf));
	}
} g_sha3Hasher;

} // Coin::
