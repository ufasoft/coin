/*######   Copyright (c) 2013-2015 Ufasoft  http://ufasoft.com  mailto:support@ufasoft.com,  Sergey Pavlov  mailto:dev@ufasoft.com ####
#                                                                                                                                     #
# 		See LICENSE for licensing information                                                                                         #
#####################################################################################################################################*/

#include <el/ext.h>

#include <el/crypto/hash.h>
using namespace Crypto;

#include "util.h"

namespace Coin {

HashValue SHA256_SHA256(const ConstBuf& cbuf) {
	SHA256 sha;
	return ConstBuf(sha.ComputeHash(sha.ComputeHash(cbuf)));
}

HashValue160 Hash160(const ConstBuf& mb) {
	return HashValue160(RIPEMD160().ComputeHash(SHA256().ComputeHash(mb)));
}

HashValue ScryptHash(const ConstBuf& mb) {
	CArray8UInt32 ar = CalcSCryptHash(mb);
	return HashValue(ConstBuf(ar.data(), 32));
}

HashValue NeoSCryptHash(const ConstBuf& mb, int profile) {
	return HashValue(ConstBuf(CalcNeoSCryptHash(mb, profile).data(), 32));
}

HashValue GroestlHash(const ConstBuf& mb) {
	Groestl512Hash hf;
	return HashValue(ConstBuf(hf.ComputeHash(hf.ComputeHash(mb)).data(), 32));
}


} // Coin::

