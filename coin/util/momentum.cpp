#include <el/ext.h>

#include <el/crypto/hash.h>
using namespace Ext::Crypto;

#include "util.h"

namespace Coin {

static const UInt32 MAX_MOMENTUM_NONCE = 1<<26;
static int SEARCH_SPACE_BITS = 50,
	BIRTHDAYS_PER_HASH = 8;

UInt64 MomentumGetBirthdayHash(const HashValue& hash, UInt32 a) {
	UInt32 buf[8+1];
	buf[0] = htole(UInt32(a & ~7));
	memcpy(buf+1, hash.data(), 32);
	return letoh(*(UInt64*)(SHA512().ComputeHash(ConstBuf(buf, sizeof buf)).constData()+(a & 7)*8)) >> (64-SEARCH_SPACE_BITS);
}

bool MomentumVerify(const HashValue& hash, UInt32 a, UInt32 b) {
	return a!=b && a<=MAX_MOMENTUM_NONCE && b<=MAX_MOMENTUM_NONCE && MomentumGetBirthdayHash(hash, a)==MomentumGetBirthdayHash(hash, b);
}

} // Coin::
