#include <el/ext.h>

#include "miner.h"

namespace Coin {

class MetisHasher : public Hasher {
public:
	MetisHasher()
		:	Hasher("metis", HashAlgo::Metis) 
	{}

	HashValue CalcHash(const ConstBuf& cbuf) override {
		return MetisHash(cbuf);
	}
} g_metisHasher;



} // Coin::
