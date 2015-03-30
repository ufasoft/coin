/*######   Copyright (c) 2015 Ufasoft  http://ufasoft.com  mailto:support@ufasoft.com,  Sergey Pavlov  mailto:dev@ufasoft.com ####
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



#endif // UCFG_USE_OPENSSL



Blob BuggyAes::Encrypt(const ConstBuf& cbuf) {
#if UCFG_USE_OPENSSL
	int rlen = cbuf.Size+AES_BLOCK_SIZE, flen = 0;
	Blob r(0, rlen);
    
	CipherCtx ctx;
    SslCheck(::EVP_DecryptInit_ex(ctx, EVP_aes_256_cbc(), 0, m_key.constData(), IV.constData()));		//!!! should be EVP_EncryptInit_ex() in normal AES
    SslCheck(::EVP_EncryptUpdate(ctx, r.data(), &rlen, cbuf.P, cbuf.Size));
    SslCheck(::EVP_EncryptFinal_ex(ctx, r.data()+rlen, &flen));
	r.Size = rlen+flen;
	return r;
#else
	InitParams();
	const int cbBlock = BlockSize / 8;
	MemoryStream ms;
	Blob block(0, cbBlock);
	byte *tdata = (byte*)alloca(cbBlock);
	ConstBuf state = IV;
	Blob ekey = CalcInvExpandedKey();
	for (size_t pos = 0;; pos += block.Size) {
		size_t size = (min)(cbuf.Size - pos, size_t(cbBlock));
		memcpy(tdata, cbuf.P + pos, size);
		if (size < cbBlock)
			Pad(tdata, cbBlock - size);

		DecryptBlock(ekey, tdata);
		VectorXor(tdata, state.P, cbBlock);
		state = ConstBuf(cbuf.P+pos, cbBlock);
		ms.WriteBuffer(tdata, cbBlock);
		if (size < cbBlock)
			break;
	}
	return ms;
#endif
}

Blob BuggyAes::Decrypt(const ConstBuf& cbuf) {
#if UCFG_USE_OPENSSL
	int rlen = cbuf.Size, flen = 0;
	Blob r(0, rlen);

	CipherCtx ctx;
    SslCheck(::EVP_EncryptInit_ex(ctx, EVP_aes_256_cbc(), 0, m_key.constData(), IV.constData()));		//!!! should be EVP_DecryptInit_ex() in normal AES
    SslCheck(::EVP_DecryptUpdate(ctx, r.data(), &rlen, cbuf.P, cbuf.Size));
    SslCheck(::EVP_DecryptFinal_ex(ctx, r.data()+rlen, &flen));
	r.Size = rlen+flen;
	return r;
#else
	InitParams();
	const int cbBlock = BlockSize/8;
	if (cbuf.Size % cbBlock)
		Throw(errc::invalid_argument);
	MemoryStream ms;
	Blob block = IV;
	ConstBuf state = IV;
	Blob ekey = CalcExpandedKey();
	for (size_t pos=0;  pos<cbuf.Size; pos+=block.Size) {
		VectorXor(block.data(), cbuf.P+pos, cbBlock);
		EncryptBlock(ekey, block.data());		
		if (pos+cbBlock==cbuf.Size) {
			byte nPad = block.constData()[cbBlock-1];
			for (int i=0; i<nPad; ++i)
				if (block.constData()[cbBlock-1-i] != nPad)
					Throw(E_EXT_Crypto);									//!!!TODO Must be EXT_Crypto_DecryptFailed
			ms.WriteBuffer(block.constData(), cbBlock-nPad);		
		} else
			ms.WriteBuf(block);
	}
	return ms;
#endif
}




} // Coin::


