/*######   Copyright (c) 2013-2015 Ufasoft  http://ufasoft.com  mailto:support@ufasoft.com,  Sergey Pavlov  mailto:dev@ufasoft.com ####
#                                                                                                                                     #
# 		See LICENSE for licensing information                                                                                         #
#####################################################################################################################################*/

#pragma once

#include "param.h"
#include "../util/util.h"

#if UCFG_COIN_USE_OPENSSL
#	include <el/crypto/ecdsa.h>
#endif

#include "buggy-aes.h"

namespace Coin {

class CoinEng;

const unsigned
	WALLET_CRYPTO_KEY_SIZE = 32,
	WALLET_CRYPTO_SALT_SIZE = 8,
	WALLET_CRYPTO_IV_SIZE = 16;

class CMasterKey : public CPersistent {
public:
    vector<unsigned char> vchCryptedKey;
    vector<unsigned char> vchSalt;
    // 0 = EVP_sha512()
    // 1 = scrypt()
    uint32_t nDerivationMethod;
    uint32_t nDeriveIterations;
    // Use this for more parameters to key derivation,
    // such as the various parameters to scrypt
    vector<unsigned char> vchOtherDerivationParameters;

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

class KeyInfoObj : public KeyInfoBase {
	typedef KeyInfoBase base;
public:
	int64_t KeyRowId;

	KeyInfoObj()
		: KeyRowId(-1) {
	}

	KeyInfoObj(const base& ki)
		: base(ki)
		, KeyRowId(-1)
	{}
};

class KeyInfo : public Pimpl<KeyInfoObj> {
	typedef KeyInfo class_type;
	typedef Pimpl<KeyInfoObj> base;
public:
	KeyInfo() {
		m_pimpl = new KeyInfoObj;
	}

	KeyInfo(nullptr_t) {
	}

	CanonicalPubKey get_PubKey() const { return m_pimpl->PubKey; }
	void put_PubKey(const CanonicalPubKey& v) { m_pimpl->PubKey = v; }
	DEFPROP(CanonicalPubKey, PubKey);

	const PrivateKey& get_PrivKey() const { return m_pimpl->PrivKey; }
	DEFPROP_GET(const PrivateKey&, PrivKey);
		
	Blob get_PlainPrivKey() const { return m_pimpl->PlainPrivKey(); }
	DEFPROP_GET(Blob, PlainPrivKey);

	AddressType get_AddressType() const { return m_pimpl->AddressType; }
	void put_AddressType(AddressType v) { m_pimpl->AddressType = v; }
	DEFPROP(AddressType, AddressType);
};

Blob EncryptedPrivKey(BuggyAes& aes, const KeyInfo& key);

class CCrypter {
public:
	unsigned char chKey[WALLET_CRYPTO_KEY_SIZE];
	unsigned char chIV[WALLET_CRYPTO_IV_SIZE];
private:
    bool fKeySet;
public:

	bool SetKeyFromPassphrase(const std::string &strKeyData, RCSpan salt, unsigned nRounds, unsigned nDerivationMethod);
    bool Encrypt(const CKeyingMaterial& vchPlaintext, std::vector<unsigned char> &vchCiphertext);
    bool Decrypt(RCSpan vchCiphertext, CKeyingMaterial& vchPlaintext);
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
bool DecryptSecret(const CKeyingMaterial& vMasterKey, RCSpan vchCiphertext, const HashValue& nIV, Blob &vchPlaintext);





} // Coin::

