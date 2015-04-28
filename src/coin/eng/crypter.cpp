/*######   Copyright (c) 2013-2015 Ufasoft  http://ufasoft.com  mailto:support@ufasoft.com,  Sergey Pavlov  mailto:dev@ufasoft.com ####
#                                                                                                                                     #
# 		See LICENSE for licensing information                                                                                         #
#####################################################################################################################################*/

#include <el/ext.h>

#include <openssl/evp.h>
#include <openssl/aes.h>

#include "crypter.h"

#include <el/crypto/cipher.h>
using namespace Ext::Crypto;

namespace Coin {

void CMasterKey::Write(BinaryWriter& wr) const {
	wr << vchCryptedKey << vchSalt << nDerivationMethod << nDeriveIterations << vchOtherDerivationParameters;
}

void CMasterKey::Read(const BinaryReader& rd) {
	rd >> vchCryptedKey >> vchSalt >> nDerivationMethod >> nDeriveIterations >> vchOtherDerivationParameters;
}

void CWalletKey::Write(BinaryWriter& wr) const {
	wr << vchPrivKey << nTimeCreated << nTimeExpires << strComment;
}

void CWalletKey::Read(const BinaryReader& rd) {
	rd >> vchPrivKey >> nTimeCreated >> nTimeExpires >> strComment;
}

bool CCrypter::SetKeyFromPassphrase(const string& strKeyData, const std::vector<unsigned char>& chSalt, const unsigned int nRounds, const unsigned int nDerivationMethod) {
    if (nRounds < 1 || chSalt.size() != WALLET_CRYPTO_SALT_SIZE)
        return false;

    int i = 0;
    if (nDerivationMethod == 0) {
		pair<Blob, Blob> pp = Aes::GetKeyAndIVFromPassword(strKeyData, &chSalt[0], nRounds);
		memcpy(chKey, pp.first.constData(), 32);
		memcpy(chIV, pp.second.constData(), 16);
		i = WALLET_CRYPTO_KEY_SIZE;
	}

    if (i != WALLET_CRYPTO_KEY_SIZE) {
        memset(&chKey, 0, sizeof chKey);
        memset(&chIV, 0, sizeof chIV);
        return false;
    }

    fKeySet = true;
    return true;
}

bool CCrypter::SetKey(const CKeyingMaterial& chNewKey, const std::vector<unsigned char>& chNewIV) {
    if (chNewKey.Size != WALLET_CRYPTO_KEY_SIZE || chNewIV.size() != WALLET_CRYPTO_KEY_SIZE)
        return false;

	memcpy(&chKey[0], chNewKey.data(), sizeof chKey);
    memcpy(&chIV[0], &chNewIV[0], sizeof chIV);

    fKeySet = true;
    return true;
}

bool CCrypter::Encrypt(const CKeyingMaterial& vchPlaintext, std::vector<unsigned char> &vchCiphertext) {
    if (!fKeySet)
        return false;

    // max ciphertext len for a n bytes of plaintext is
    // n + AES_BLOCK_SIZE - 1 bytes
    int nLen = vchPlaintext.Size;
    int nCLen = nLen + AES_BLOCK_SIZE, nFLen = 0;
    vchCiphertext = std::vector<unsigned char> (nCLen);

    EVP_CIPHER_CTX ctx;

    EVP_CIPHER_CTX_init(&ctx);
    EVP_EncryptInit_ex(&ctx, EVP_aes_256_cbc(), NULL, chKey, chIV);

    EVP_EncryptUpdate(&ctx, &vchCiphertext[0], &nCLen, vchPlaintext.data(), nLen);
    EVP_EncryptFinal_ex(&ctx, (&vchCiphertext[0])+nCLen, &nFLen);

    EVP_CIPHER_CTX_cleanup(&ctx);

    vchCiphertext.resize(nCLen + nFLen);
    return true;
}

bool CCrypter::Decrypt(const ConstBuf& vchCiphertext, CKeyingMaterial& vchPlaintext) {
    if (!fKeySet)
        return false;

    // plaintext will always be equal to or lesser than length of ciphertext
    int nLen = vchCiphertext.Size;
    int nPLen = nLen, nFLen = 0;

    vchPlaintext = CKeyingMaterial(0, nPLen);

    EVP_CIPHER_CTX ctx;

    EVP_CIPHER_CTX_init(&ctx);
    EVP_DecryptInit_ex(&ctx, EVP_aes_256_cbc(), NULL, chKey, chIV);

    EVP_DecryptUpdate(&ctx, vchPlaintext.data(), &nPLen, vchCiphertext.P, nLen);
    EVP_DecryptFinal_ex(&ctx, vchPlaintext.data()+nPLen, &nFLen);

    EVP_CIPHER_CTX_cleanup(&ctx);

    vchPlaintext.Size = nPLen + nFLen;
    return true;
}


bool EncryptSecret(CKeyingMaterial& vMasterKey, const Blob& vchPlaintext, const HashValue& nIV, std::vector<unsigned char> &vchCiphertext) {
    CCrypter cKeyCrypter;
    std::vector<unsigned char> chIV(WALLET_CRYPTO_KEY_SIZE);
    memcpy(&chIV[0], &nIV, WALLET_CRYPTO_KEY_SIZE);
    if(!cKeyCrypter.SetKey(vMasterKey, chIV))
        return false;
    return cKeyCrypter.Encrypt((CKeyingMaterial)vchPlaintext, vchCiphertext);
}

bool DecryptSecret(const CKeyingMaterial& vMasterKey, const ConstBuf& vchCiphertext, const HashValue& nIV, Blob& vchPlaintext) {
    CCrypter cKeyCrypter;
    std::vector<unsigned char> chIV(WALLET_CRYPTO_KEY_SIZE);
    memcpy(&chIV[0], nIV.data(), WALLET_CRYPTO_KEY_SIZE);
    if(!cKeyCrypter.SetKey(vMasterKey, chIV))
        return false;
    return cKeyCrypter.Decrypt(vchCiphertext, vchPlaintext);
}

MyKeyInfo CCrypter::GenRandomKey() {
#if UCFG_COIN_USE_OPENSSL
	ECDsa dsa(256);
	MyKeyInfo ki;
	ki.Comment = nullptr;
	ki.Key = dsa.Key;
	Blob privData = ki.Key.Export(CngKeyBlobFormat::OSslEccPrivateBignum);
	ki.SetPrivData(privData, true);
	ki.PubKey = ki.Key.Export(CngKeyBlobFormat::OSslEccPublicCompressedBlob);
	return ki;
#else
	!!!TODO
#endif
}

Blob CCrypter::PublicKeyBlobToCompressedBlob(const ConstBuf& cbuf) {
#if UCFG_COIN_USE_OPENSSL
	CngKey key = CngKey::Import(cbuf, CngKeyBlobFormat::OSslEccPublicBlob);
	return key.Export(CngKeyBlobFormat::OSslEccPublicCompressedBlob);
#else
	!!!TODO
#endif
}

} // Coin::

