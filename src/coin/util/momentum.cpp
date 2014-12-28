/*######     Copyright (c) 1997-2015 Ufasoft  http://ufasoft.com  mailto:support@ufasoft.com,  Sergey Pavlov  mailto:dev@ufasoft.com #########################################################################################################
#                                                                                                                                                                                                                                            #
# This program is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation;  either version 3, or (at your option) any later version.          #
# This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.   #
# You should have received a copy of the GNU General Public License along with this program; If not, see <http://www.gnu.org/licenses/>                                                                                                      #
############################################################################################################################################################################################################################################*/

#include <el/ext.h>

#include <el/crypto/hash.h>
using namespace Ext::Crypto;

#include "util.h"

namespace Coin {

static const uint32_t MAX_MOMENTUM_NONCE = 1<<26;
static int SEARCH_SPACE_BITS = 50,
	BIRTHDAYS_PER_HASH = 8;

uint64_t MomentumGetBirthdayHash(const HashValue& hash, uint32_t a) {
	uint32_t buf[8+1];
	buf[0] = htole(uint32_t(a & ~7));
	memcpy(buf+1, hash.data(), 32);
	return letoh(*(uint64_t*)(SHA512().ComputeHash(ConstBuf(buf, sizeof buf)).constData()+(a & 7)*8)) >> (64-SEARCH_SPACE_BITS);
}

bool MomentumVerify(const HashValue& hash, uint32_t a, uint32_t b) {
	return a!=b && a<=MAX_MOMENTUM_NONCE && b<=MAX_MOMENTUM_NONCE && MomentumGetBirthdayHash(hash, a)==MomentumGetBirthdayHash(hash, b);
}

} // Coin::
