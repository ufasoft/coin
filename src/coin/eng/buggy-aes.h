/*######   Copyright (c) 2015 Ufasoft  http://ufasoft.com  mailto:support@ufasoft.com,  Sergey Pavlov  mailto:dev@ufasoft.com ####
#                                                                                                                                     #
# 		See LICENSE for licensing information                                                                                         #
#####################################################################################################################################*/

#pragma once

// Non-standard AES implementation for backward-compatibility with old Coin version

#include <el/crypto/cipher.h>

namespace Coin {
using namespace Ext::Crypto;


class BuggyAes : public Aes {
	typedef Aes base;
public:
	Blob Encrypt(RCSpan cbuf) override;
	Blob Decrypt(RCSpan cbuf) override;
};



} // Coin::





