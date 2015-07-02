/*######   Copyright (c) 2013-2015 Ufasoft  http://ufasoft.com  mailto:support@ufasoft.com,  Sergey Pavlov  mailto:dev@ufasoft.com ####
#                                                                                                                                     #
# 		See LICENSE for licensing information                                                                                         #
#####################################################################################################################################*/

#include <el/ext.h>

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


	Aes aes;
	aes.Key = ConstBuf(chKey, sizeof chKey);
	aes.IV = ConstBuf(chIV, sizeof chIV);
	Blob r = aes.Encrypt(vchPlaintext);
	vchCiphertext = vector<unsigned char>(r.constData(), r.constData()+r.Size);

    return true;
}

bool CCrypter::Decrypt(const ConstBuf& vchCiphertext, CKeyingMaterial& vchPlaintext) {
    if (!fKeySet)
        return false;

	Aes aes;
	aes.Key = ConstBuf(chKey, sizeof chKey);
	aes.IV = ConstBuf(chIV, sizeof chIV);
	vchPlaintext = aes.Decrypt(vchCiphertext);

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


} // Coin::

