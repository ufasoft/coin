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

const unsigned int WALLET_CRYPTO_KEY_SIZE = 32;
const unsigned int WALLET_CRYPTO_SALT_SIZE = 8;

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


class COIN_CLASS Address : public HashValue160, public CPrintable {
public:
	CoinEng& Eng;
	String Comment;
	byte Ver;

	Address(CoinEng& eng);
	explicit Address(CoinEng& eng, const HashValue160& hash, RCString comment = "");
	explicit Address(CoinEng& eng, const HashValue160& hash, byte ver);
	explicit Address(CoinEng& eng, RCString s);

	Address& operator=(const Address& a) {
		if (&Eng != &a.Eng)
			Throw(E_INVALIDARG);
		HashValue160::operator=(a);
		Comment = a.Comment;
		Ver = a.Ver;
		return *this;
	}

	void CheckVer(CoinEng& eng) const;
	String ToString() const override;

	bool operator<(const Address& a) const {
		return Ver < a.Ver ||
			(Ver == a.Ver && memcmp(data(), a.data(), 20) < 0);
	}
};

class PrivateKey : public CPrintable {
public:
	PrivateKey() {}
	PrivateKey(const ConstBuf& cbuf, bool bCompressed);
	explicit PrivateKey(RCString s);
	pair<Blob, bool> GetPrivdataCompressed() const;
	String ToString() const override;
private:
	Blob m_blob;
};

class MyKeyInfo {
	typedef MyKeyInfo class_type;
public:
	int64_t KeyRowid;
	Blob PubKey;

	CngKey Key;
	DateTime Timestamp;
	String Comment;

	~MyKeyInfo();
	Address ToAddress() const;
	Blob PlainPrivKey() const;
	Blob EncryptedPrivKey(BuggyAes& aes) const;

	Blob get_PrivKey() const { return m_privKey; }
	//	void put_PrivKey(const Blob& v);
	DEFPROP_GET(Blob, PrivKey);

	HashValue160 get_Hash160() const {
		return Coin::Hash160(PubKey);
	}
	DEFPROP_GET(HashValue160, Hash160);

	void SetPrivData(const ConstBuf& cbuf, bool bCompressed);
	void SetPrivData(const PrivateKey& privKey);

	bool IsCompressed() const { return PubKey.Size == 33; }
private:
	Blob m_privKey;
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

	static MyKeyInfo GenRandomKey();
	static Blob PublicKeyBlobToCompressedBlob(const ConstBuf& cbuf);
};

bool EncryptSecret(CKeyingMaterial& vMasterKey, const Blob& vchPlaintext, const HashValue& nIV, std::vector<unsigned char> &vchCiphertext);
bool DecryptSecret(const CKeyingMaterial& vMasterKey, const ConstBuf& vchCiphertext, const HashValue& nIV, Blob &vchPlaintext);


} // Coin::

