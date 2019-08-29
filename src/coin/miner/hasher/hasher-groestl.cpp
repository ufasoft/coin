/*######   Copyright (c) 2015-2019 Ufasoft  http://ufasoft.com  mailto:support@ufasoft.com,  Sergey Pavlov  mailto:dev@ufasoft.com ####
#                                                                                                                                     #
# 		See LICENSE for licensing information                                                                                         #
#####################################################################################################################################*/

#include <el/ext.h>

#include "miner.h"

namespace Coin {

class GroestlHasher : public Hasher {
public:
	GroestlHasher()
		: Hasher("groestl", HashAlgo::Groestl)
	{}

	HashValue CalcHash(RCSpan cbuf) override {
		return GroestlHash(cbuf);
	}
} g_groestlHasher;

Hasher& GetGroestlcoinHasher() { return g_groestlHasher; }	//!!!?

} // Coin::

