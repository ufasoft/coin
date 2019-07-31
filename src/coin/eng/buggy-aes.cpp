/*######   Copyright (c) 2015 Ufasoft  http://ufasoft.com  mailto:support@ufasoft.com,  Sergey Pavlov  mailto:dev@ufasoft.com      ####
#                                                                                                                                     #
# 		See LICENSE for licensing information                                                                                         #
#####################################################################################################################################*/


// Non-standard AES backward implementation for compatibility with old Coin version

#include <el/ext.h>

#include "buggy-aes.h"

#if UCFG_USE_OPENSSL
#	define OPENSSL_NO_SCTP
#	include <openssl/aes.h>
#	include <openssl/evp.h>

#	include <el/crypto/ext-openssl.h>
#endif


using namespace Ext;

namespace Coin {
using namespace std;


#if UCFG_USE_OPENSSL

#	pragma comment(lib, "openssl")

class CipherCtx {
	EVP_CIPHER_CTX *m_ctx;
public:
	CipherCtx()
		: m_ctx(::EVP_CIPHER_CTX_new())
	{}

	~CipherCtx() {
	    ::EVP_CIPHER_CTX_free(m_ctx);
	}

	operator EVP_CIPHER_CTX*() { return m_ctx; }
};



#endif // UCFG_USE_OPENSSL



Blob BuggyAes::Encrypt(RCSpan cbuf) {
#if UCFG_USE_OPENSSL
	int rlen = cbuf.size() + AES_BLOCK_SIZE, flen = 0;
	Blob r(0, rlen);

	CipherCtx ctx;
    SslCheck(::EVP_DecryptInit_ex(ctx, EVP_aes_256_cbc(), 0, m_key.constData(), IV.constData()));		//!!! should be EVP_EncryptInit_ex() in normal AES
    SslCheck(::EVP_EncryptUpdate(ctx, r.data(), &rlen, cbuf.data(), cbuf.size()));
    SslCheck(::EVP_EncryptFinal_ex(ctx, r.data()+rlen, &flen));
	r.resize(rlen + flen);
	return r;
#else
	InitParams();
	const int cbBlock = BlockSize / 8;
	MemoryStream ms;
	Blob block(0, cbBlock);
	uint8_t *tdata = (uint8_t*)alloca(cbBlock);
	Span state = IV;
	Blob ekey = CalcInvExpandedKey();
	for (size_t pos = 0;; pos += block.size()) {
		size_t size = (min)(cbuf.size() - pos, size_t(cbBlock));
		memcpy(tdata, cbuf.data() + pos, size);
		if (size < cbBlock)
			Pad(tdata, cbBlock - size);

		DecryptBlock(ekey, tdata);
		VectorXor(tdata, state.data(), cbBlock);
		state = Span(cbuf.data() + pos, cbBlock);
		ms.WriteBuffer(tdata, cbBlock);
		if (size < cbBlock)
			break;
	}
	return Span(ms);
#endif
}

Blob BuggyAes::Decrypt(RCSpan cbuf) {
#if UCFG_USE_OPENSSL
	int rlen = cbuf.size(), flen = 0;
	Blob r(0, rlen);

	CipherCtx ctx;
    SslCheck(::EVP_EncryptInit_ex(ctx, EVP_aes_256_cbc(), 0, m_key.constData(), IV.constData()));		//!!! should be EVP_DecryptInit_ex() in normal AES
    SslCheck(::EVP_DecryptUpdate(ctx, r.data(), &rlen, cbuf.data(), cbuf.size()));
    SslCheck(::EVP_DecryptFinal_ex(ctx, r.data()+rlen, &flen));
	r.resize(rlen + flen);
	return r;
#else
	InitParams();
	const int cbBlock = BlockSize / 8;
	if (cbuf.size() % cbBlock)
		Throw(errc::invalid_argument);
	MemoryStream ms;
	Blob block = IV;
	Span state = IV;
	Blob ekey = CalcExpandedKey();
	for (size_t pos = 0; pos < cbuf.size(); pos += block.size()) {
		VectorXor(block.data(), cbuf.data() + pos, cbBlock);
		EncryptBlock(ekey, block.data());
		if (Padding != PaddingMode::None && pos + cbBlock == cbuf.size()) {
			uint8_t nPad = block.constData()[cbBlock-1];
			for (int i = 0; i < nPad; ++i)
				if (block.constData()[cbBlock-1-i] != nPad)
					Throw(ExtErr::Crypto);									//!!!TODO Must be EXT_Crypto_DecryptFailed
			ms.WriteBuffer(block.constData(), cbBlock-nPad);
		} else
			ms.Write(block);
	}
	return Span(ms);
#endif
}




} // Coin::


