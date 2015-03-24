/*######   Copyright (c) 2015 Ufasoft  http://ufasoft.com  mailto:support@ufasoft.com,  Sergey Pavlov  mailto:dev@ufasoft.com ####
#                                                                                                                                     #
# 		See LICENSE for licensing information                                                                                         #
#####################################################################################################################################*/

#pragma once

// Non-standard AES backward implementation for compatibility with old Coin version

#include <el/crypto/cipher.h>

namespace Coin {
using namespace Ext::Crypto;


class BuggyAes : public Aes {
	typedef Aes base;
public:
	Blob Encrypt(const ConstBuf& cbuf) override;
	Blob Decrypt(const ConstBuf& cbuf) override;
};



} // Coin::





