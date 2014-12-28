/*######     Copyright (c) 1997-2015 Ufasoft  http://ufasoft.com  mailto:support@ufasoft.com,  Sergey Pavlov  mailto:dev@ufasoft.com #########################################################################################################
#                                                                                                                                                                                                                                            #
# This program is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation;  either version 3, or (at your option) any later version.          #
# This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.   #
# You should have received a copy of the GNU General Public License along with this program; If not, see <http://www.gnu.org/licenses/>                                                                                                      #
############################################################################################################################################################################################################################################*/

#pragma once

#include "../util/util.h"

namespace Coin {

const unsigned int WALLET_CRYPTO_KEY_SIZE = 32;
const unsigned int WALLET_CRYPTO_SALT_SIZE = 8;

class CMasterKey : public CPersistent {
public:
    std::vector<unsigned char> vchCryptedKey;
    std::vector<unsigned char> vchSalt;
    // 0 = EVP_sha512()
    // 1 = scrypt()
    uint32_t nDerivationMethod;
    uint32_t nDeriveIterations;
    // Use this for more parameters to key derivation,
    // such as the various parameters to scrypt
    std::vector<unsigned char> vchOtherDerivationParameters;

	CMasterKey() {
        // 25000 rounds is just under 0.1 seconds on a 1.86 GHz Pentium M
        // ie slightly lower than the lowest hardware we need bother supporting
        nDeriveIterations = 25000;
        nDerivationMethod = 0;
        vchOtherDerivationParameters = std::vector<unsigned char>(0);
    }

	void Write(BinaryWriter& wr) const override;
	void Read(const BinaryReader& rd) override;
};

typedef Blob CKeyingMaterial;


class CWalletKey : public CPersistent {
public:
	Blob vchPrivKey;
    int64_t nTimeCreated;
    int64_t nTimeExpires;
    String strComment;

	void Write(BinaryWriter& wr) const override;
	void Read(const BinaryReader& rd) override;
};


class CCrypter {
private:
    unsigned char chKey[WALLET_CRYPTO_KEY_SIZE];
    unsigned char chIV[WALLET_CRYPTO_KEY_SIZE];
    bool fKeySet;

public:
    bool SetKeyFromPassphrase(const std::string &strKeyData, const std::vector<unsigned char>& chSalt, const unsigned int nRounds, const unsigned int nDerivationMethod);
    bool Encrypt(const CKeyingMaterial& vchPlaintext, std::vector<unsigned char> &vchCiphertext);
    bool Decrypt(const ConstBuf& vchCiphertext, CKeyingMaterial& vchPlaintext);
    bool SetKey(const CKeyingMaterial& chNewKey, const std::vector<unsigned char>& chNewIV);

    void CleanKey() {
        memset(&chKey, 0, sizeof chKey);
        memset(&chIV, 0, sizeof chIV);        
        fKeySet = false;
    }

    CCrypter() {
        fKeySet = false;
    }

    ~CCrypter() {
        CleanKey();
    }
};

bool EncryptSecret(CKeyingMaterial& vMasterKey, const Blob& vchPlaintext, const HashValue& nIV, std::vector<unsigned char> &vchCiphertext);
bool DecryptSecret(const CKeyingMaterial& vMasterKey, const ConstBuf& vchCiphertext, const HashValue& nIV, Blob &vchPlaintext);


} // Coin::

