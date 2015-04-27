/*######   Copyright (c) 2013-2015 Ufasoft  http://ufasoft.com  mailto:support@ufasoft.com,  Sergey Pavlov  mailto:dev@ufasoft.com ####
#                                                                                                                                     #
# 		See LICENSE for licensing information                                                                                         #
#####################################################################################################################################*/

#include <el/ext.h>

#include <el/crypto/hash.h>
using namespace Crypto;

#include "util.h"

namespace Coin {

HashValue ScryptHash(const ConstBuf& mb) {
	array<uint32_t, 8> ar = CalcSCryptHash(mb);
	HashValue r;
	memcpy(r.data(), ar.data(), 32);
	return r;
}

HashValue NeoSCryptHash(const ConstBuf& mb, int profile) {
	array<uint32_t, 8> ar = CalcNeoSCryptHash(mb, profile);
	HashValue r;
	memcpy(r.data(), ar.data(), 32);
	return r;
}

HashValue GroestlHash(const ConstBuf& mb) {
	Groestl512Hash hf;
	hashval hv = hf.ComputeHash(hf.ComputeHash(mb));
	return HashValue(ConstBuf(hv.data(), 32));
}


} // Coin::

