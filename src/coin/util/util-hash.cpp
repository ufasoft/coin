/*######   Copyright (c) 2013-2019 Ufasoft  http://ufasoft.com  mailto:support@ufasoft.com,  Sergey Pavlov  mailto:dev@ufasoft.com ####
#                                                                                                                                     #
# 		See LICENSE for licensing information                                                                                         #
#####################################################################################################################################*/

#include <el/ext.h>

#include <el/crypto/hash.h>
using namespace Crypto;

#include "util.h"

namespace Coin {

static SHA256 s_sha256;
static RIPEMD160 s_ripemd160;
static Groestl512Hash s_groestl512hasher;

HashValue SHA256_SHA256(RCSpan cbuf) {
	return s_sha256.ComputeHash(s_sha256.ComputeHash(cbuf));
}

HashValue160 Hash160(RCSpan mb) {
	return HashValue160(s_ripemd160.ComputeHash(s_sha256.ComputeHash(mb)));
}

HashValue ScryptHash(RCSpan mb) {
	CArray8UInt32 ar = CalcSCryptHash(mb);
	return HashValue(ConstBuf(ar.data(), 32));
}

HashValue NeoSCryptHash(RCSpan mb, int profile) {
	return HashValue(ConstBuf(CalcNeoSCryptHash(mb, profile).data(), 32));
}

HashValue GroestlHash(RCSpan mb) {
	return HashValue(Span(s_groestl512hasher.ComputeHash(s_groestl512hasher.ComputeHash(mb)).data(), 32));
}


} // Coin::

