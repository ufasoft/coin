/*######     Copyright (c) 1997-2013 Ufasoft  http://ufasoft.com  mailto:support@ufasoft.com,  Sergey Pavlov  mailto:dev@ufasoft.com #######################################
#                                                                                                                                                                          #
# This program is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation;  #
# either version 3, or (at your option) any later version. This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the      #
# implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details. You should have received a copy of the GNU #
# General Public License along with this program; If not, see <http://www.gnu.org/licenses/>                                                                               #
##########################################################################################################################################################################*/

#include <el/ext.h>

#include "hash.h"

#define OPENSSL_NO_SCTP
#include <openssl/aes.h>
#include <openssl/evp.h>

#include <el/crypto/ext-openssl.h>


using namespace Ext;

namespace Ext { namespace Crypto {



class CipherCtx {
public:
	CipherCtx() {
		::EVP_CIPHER_CTX_init(&m_ctx);
	}

	~CipherCtx() {
	    SslCheck(::EVP_CIPHER_CTX_cleanup(&m_ctx));
	}

	operator EVP_CIPHER_CTX*() {
		return &m_ctx;
	}
private:
	EVP_CIPHER_CTX m_ctx;
};



std::pair<Blob, Blob> Aes::GetKeyAndIVFromPassword(RCString password, const byte salt[8], int nRounds) {
	pair<Blob, Blob> r(Blob(0, 32), Blob(0, 32));

	const unsigned char *psz = (const unsigned char*)(const char*)password;
	int rc = ::EVP_BytesToKey(EVP_aes_256_cbc(), EVP_sha512(), salt, psz, strlen(password), nRounds, r.first.data(), r.second.data());
	if (rc != 32)
		Throw(E_FAIL);
	return r;
}

Blob Aes::Encrypt(const ConstBuf& cbuf) {
	int rlen = cbuf.Size+AES_BLOCK_SIZE, flen = 0;
	Blob r(0, rlen);
    
	CipherCtx ctx;
    SslCheck(::EVP_DecryptInit_ex(ctx, EVP_aes_256_cbc(), 0, Key.constData(), IV.constData()));
    SslCheck(::EVP_EncryptUpdate(ctx, r.data(), &rlen, cbuf.P, cbuf.Size));
    SslCheck(::EVP_EncryptFinal_ex(ctx, r.data()+rlen, &flen));

	r.Size = rlen+flen;
	return r;
}

Blob Aes::Decrypt(const ConstBuf& cbuf) {
	int rlen = cbuf.Size, flen = 0;
	Blob r(0, rlen);
    
	CipherCtx ctx;
    SslCheck(::EVP_EncryptInit_ex(ctx, EVP_aes_256_cbc(), 0, Key.constData(), IV.constData()));
    SslCheck(::EVP_DecryptUpdate(ctx, r.data(), &rlen, cbuf.P, cbuf.Size));
    SslCheck(::EVP_DecryptFinal_ex(ctx, r.data()+rlen, &flen));

	r.Size = rlen+flen;
	return r;
}



}} // Ext::Crypto


