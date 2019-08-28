#include <el/ext.h>

#include "miner.h"

namespace Coin {

class NeoSCryptHasher : public Hasher {
public:
	NeoSCryptHasher()
		: Hasher("neoscrypt", HashAlgo::NeoSCrypt)
	{}

	HashValue CalcHash(RCSpan cbuf) override {
		array<uint32_t, 8> res = CalcNeoSCryptHash(cbuf);
		return HashValue(ConstBuf(res.data(), 32));
	}
} g_neoscryptHasher;

} // Coin::
