/*######   Copyright (c) 2011-2015 Ufasoft  http://ufasoft.com  mailto:support@ufasoft.com,  Sergey Pavlov  mailto:dev@ufasoft.com ####
#                                                                                                                                     #
# 		See LICENSE for licensing information                                                                                         #
#####################################################################################################################################*/

#pragma once

#include <el/crypto/hash.h>
using namespace Ext::Crypto;

#ifndef UCFG_COIN_ECC
#	define UCFG_COIN_ECC 'S'			// Secp256k1
#endif

#ifndef UCFG_COIN_USE_OPENSSL
#	define UCFG_COIN_USE_OPENSSL 1
#endif

#if UCFG_COIN_ECC!='S' && UCFG_COIN_USE_OPENSSL
#	include <el/crypto/ecdsa.h>
#endif

#ifndef UCFG_COIN_PUBKEYID_36BITS
#	define UCFG_COIN_PUBKEYID_36BITS 0
#endif

#ifndef UCFG_COIN_LIB_DECLS
#	define UCFG_COIN_LIB_DECLS UCFG_LIB_DECLS
#endif

#if UCFG_LIB_DECLS && defined(_AFXDLL)
#	ifdef UCFG_EXPORT_COIN_UTIL
#		define COIN_UTIL_CLASS AFX_CLASS_EXPORT
#		define COIN_UTIL_EXPORT AFX_CLASS_EXPORT
#		define COIN_UTIL_API DECLSPEC_DLLEXPORT
#	else
#		define COIN_UTIL_CLASS
#		define COIN_UTIL_EXPORT
#		define COIN_UTIL_API DECLSPEC_DLLIMPORT
#		pragma comment(lib, "coinutil")
#	endif
#else
#	define COIN_UTIL_CLASS
#	define COIN_UTIL_EXPORT
#	define COIN_UTIL_API
#endif

#if UCFG_COIN_LIB_DECLS && defined(_AFXDLL)
#	ifdef UCFG_EXPORT_COIN
#		define COIN_CLASS // AFX_CLASS_EXPORT
#		define COIN_EXPORT DECLSPEC_DLLEXPORT
#	else
#		define COIN_CLASS
#		define COIN_EXPORT

#		pragma comment(lib, "coineng")
#	endif

#else
#	define COIN_CLASS
#	define COIN_EXPORT
#endif

#include "coin-err.h"

#ifndef UCFG_COIN_MOMENTUM
#	define UCFG_COIN_MOMENTUM 1
#endif

#ifndef UCFG_COIN_PRIME
#	define UCFG_COIN_PRIME 1
#endif


namespace Coin {

ENUM_CLASS(HashAlgo) {
	Sha256,
	Sha3,
	SCrypt,
	Prime,
	Momentum,
	Solid,
	Metis,
	NeoSCrypt,
	Groestl
} END_ENUM_CLASS(HashAlgo);

HashAlgo StringToAlgo(RCString s);
String AlgoToString(HashAlgo algo);

class HashValue : totally_ordered<HashValue> {
public:
	typedef byte *iterator;
	typedef const byte *const_iterator;

	typedef std::reverse_iterator<iterator> reverse_iterator;
	typedef std::reverse_iterator<const_iterator> const_reverse_iterator;

	HashValue() {
		memset(data(), 0, 32);
	}

	explicit HashValue(const byte ar[32]) {
		memcpy(data(), ar, 32);
	}

	HashValue(const hashval& hv);
	HashValue(const ConstBuf& mb);
	explicit HashValue(RCString s);

	static const HashValue& Null();

	byte *data() { return (byte*)m_data; }
	const byte *data() const { return (const byte*)m_data; }

	const_iterator begin() const { return data(); }
	const_iterator end() const { return data()+32; }
	const_reverse_iterator rbegin() const { return const_reverse_iterator(end()); }
	const_reverse_iterator rend() const { return const_reverse_iterator(begin()); }

	ConstBuf ToConstBuf() const {
		return ConstBuf(data(), 32);
	}

	static HashValue FromDifficultyBits(uint32_t bits);
	uint32_t ToDifficultyBits() const;
	static HashValue FromShareDifficulty(double difficulty, HashAlgo algo);

	EXPLICIT_OPERATOR_BOOL() const {
		return *this == Null() ? 0 : EXT_CONVERTIBLE_TO_TRUE;
	}

	bool operator!() const { return !bool(*this); }

	bool operator==(const HashValue& v) const {
		return !memcmp(data(), v.data(), 32);
	}

	bool operator<(const HashValue& v) const;

	static HashValue Combine(const HashValue& h1, const HashValue& h2);

	void Write(BinaryWriter& wr) const {
		wr.Write(data(), 32);
	}

	void Read(const BinaryReader& rd) {
		rd.Read(data(), 32);
	}
private:
	uint64_t m_data[4];
};


COIN_UTIL_API ostream& operator<<(ostream& os, const HashValue& hash);

class BlockHashValue : public HashValue {
	typedef HashValue base;
public:
	BlockHashValue(const ConstBuf& mb);
};

class COIN_CLASS HashValue160 : public std::array<byte, 20> {
public:
	HashValue160() {
		memset(data(), 0, 20);
	}

	explicit HashValue160(const ConstBuf& mb) {
		ASSERT(mb.Size == 20);
		memcpy(data(), mb.P, mb.Size);
	}

	explicit HashValue160(RCString s);
};


class ReducedHashValue {
public:
	ReducedHashValue(const HashValue& hash)
		:	m_val(letoh(*(int64_t*)hash.data()))
	{
	}

/*	ReducedHashValue(const HashValue160& hash)
		:	m_val(letoh(*(int64_t*)hash.data()))
	{
	} */

	bool operator==(const ReducedHashValue& v) const {
		return m_val == v.m_val;
	}

	operator int64_t() const { return m_val; }
private:
	int64_t m_val;
};

#if UCFG_COIN_PUBKEYID_36BITS
	const int64_t PUBKEYID_MASK = 0xFFFFFFFFFLL;
#else
	const int64_t PUBKEYID_MASK = 0x7FFFFFFFFLL;
#endif

class CIdPk {
public:
	explicit CIdPk(const HashValue160& hash)
		:	m_val(letoh(*(int64_t*)(hash.data()+12)) & PUBKEYID_MASK)				//	Use last bytes. First bytes are not random, because many generated addresses are like "1ReadableWord..."
	{
	}

	explicit CIdPk(int64_t val = -1)
		:	m_val(val)
	{
	}

	bool operator==(const CIdPk& v) const {
		return m_val == v.m_val;
	}

	operator int64_t() const { 
		if (IsNull())
			Throw(E_FAIL);
		return m_val;
	}

	bool IsNull() const { return m_val == -1; }
private:
	int64_t m_val;
};


class ReducedBlockHash {
public:
	ReducedBlockHash(const HashValue& hash)
		:	m_buf(hash.ToConstBuf())
	{
		while (m_buf.Size > 1 && !m_buf.P[m_buf.Size-1])
			m_buf.Size--;
	}

	operator ConstBuf() const { return m_buf; }
private:
	ConstBuf m_buf;
};



COIN_UTIL_EXPORT  HashValue Hash(const ConstBuf& mb);
COIN_UTIL_EXPORT  HashValue160 Hash160(const ConstBuf& mb);
COIN_UTIL_EXPORT String ConvertToBase58(const ConstBuf& cbuf);
COIN_UTIL_EXPORT Blob ConvertFromBase58(RCString s, bool bCheckHash = true);
String ConvertToBase58ShaSquare(const ConstBuf& cbuf);
Blob ConvertFromBase58ShaSquare(RCString s);
COIN_UTIL_EXPORT Blob CalcSha256Midstate(const ConstBuf& mb);

COIN_UTIL_EXPORT HashValue SHA256_SHA256(const ConstBuf& cbuf);
COIN_UTIL_EXPORT HashValue ScryptHash(const ConstBuf& mb);
COIN_UTIL_EXPORT HashValue NeoSCryptHash(const ConstBuf& mb, int profile);
HashValue SolidcoinHash(const ConstBuf& cbuf);
HashValue MetisHash(const ConstBuf& cbuf);
COIN_UTIL_EXPORT HashValue GroestlHash(const ConstBuf& mb);

bool MomentumVerify(const HashValue& hash, uint32_t a, uint32_t b);

struct SCoinMessageHeader {
	uint32_t Magic;
	char Command[12];
	uint32_t PayloadSize;
};



class CoinSerialized {
public:
	uint32_t Ver;

	CoinSerialized()
		:	Ver(1)
	{}

	static void WriteVarInt(BinaryWriter& wr, uint64_t v);
	static uint64_t ReadVarInt(const BinaryReader& rd);
	static String ReadString(const BinaryReader& rd);
	static void WriteString(BinaryWriter& wr, RCString s);
	static void WriteBlob(BinaryWriter& wr, const ConstBuf& mb);
	static Blob ReadBlob(const BinaryReader& rd);

	template <class T>
	static void Write(BinaryWriter& wr, const vector<T>& ar) {
		WriteVarInt(wr, ar.size());
		for (size_t i=0; i<ar.size(); ++i)
			wr << ar[i];
	}

	template <class T>
	static void Read(const BinaryReader& rd, vector<T>& ar) {
		ar.resize((size_t)ReadVarInt(rd));
		for (size_t i=0; i<ar.size(); ++i)
			ar[i].Read(rd);
	}
};

class COIN_UTIL_CLASS BlockBase : public Object {
public:
	typedef InterlockedPolicy interlocked_policy;

	HashValue PrevBlockHash;
	DateTime Timestamp;
	uint32_t DifficultyTargetBits;
	uint32_t Ver;
	uint32_t Height;
	uint32_t Nonce;
	mutable optional<Coin::HashValue> m_merkleRoot;
	
//!!!R	mutable CBool m_bMerkleCalculated;

	BlockBase()
		:	Ver(2)
		,	Height(uint32_t(-1))
	{}

	virtual void WriteHeader(BinaryWriter& wr) const;
	virtual Coin::HashValue MerkleRoot(bool bSave = true) const =0;
	virtual Coin::HashValue GetHash() const;
};

COIN_UTIL_EXPORT Blob Swab32(const ConstBuf& buf);
COIN_UTIL_EXPORT void FormatHashBlocks(void* pbuffer, size_t len);

typedef HashValue (*PFN_Combine)(const HashValue& h1, const HashValue& h2);

typedef MerkleBranch<HashValue, PFN_Combine> CCoinMerkleBranch;
typedef MerkleTree<HashValue, PFN_Combine> CCoinMerkleTree;

BinaryWriter& operator<<(BinaryWriter& wr, const CCoinMerkleBranch& branch);
const BinaryReader& operator>>(const BinaryReader& rd, CCoinMerkleBranch& branch);

struct ShaConstants {
	const uint32_t *pg_sha256_hinit,
		*pg_sha256_k;
	const uint32_t (*pg_4sha256_k)[4];
};

ShaConstants GetShaConstants();

void BitsToTargetBE(uint32_t bits, byte target[32]);

pair<uint32_t, uint32_t> FromOptionalNonceRange(const VarValue& json);

const int PASSWORD_ENCRYPT_ROUNDS_A = 1000,
			PASSWORD_ENCRYPT_ROUNDS_B = 100*1000;

const char DEFAULT_PASSWORD_ENCRYPT_METHOD = 'B';


class HasherEng : public Object {
public:
	byte AddressVersion, ScriptAddressVersion;

	HasherEng()
		: AddressVersion(0)
		, ScriptAddressVersion(5) {
	}

	virtual HashValue HashBuf(const ConstBuf& cbuf);
	virtual HashValue HashForAddress(const ConstBuf& cbuf);
	static HasherEng *GetCurrent();
protected:
	static void SetCurrent(HasherEng *heng);
};


class CHasherEngThreadKeeper {
	HasherEng *m_prev;
public:
	CHasherEngThreadKeeper(HasherEng *cur);
	~CHasherEngThreadKeeper();
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

class CanonicalPubKey {
	typedef CanonicalPubKey class_type;
public:
	Blob Data;

	CanonicalPubKey() {
	}

	CanonicalPubKey(nullptr_t)
		: Data(nullptr) {
	}

	CanonicalPubKey(const ConstBuf& cbuf)
		: Data(cbuf) {
	}

	bool IsCompressed() const { return Data.Size == 33; }

	HashValue160 get_Hash160() const {
		return Coin::Hash160(Data);
	}
	DEFPROP_GET(HashValue160, Hash160);
	
	bool IsValid() const {
		byte b0 = Data.constData()[0];
		return (Data.Size == 33 && (b0==2 || b0==3)) ||
			(Data.Size == 65 || b0==4);
	}

	Blob ToCompressed() const;
	static CanonicalPubKey FromCompressed(const ConstBuf& cbuf);
};

class COIN_CLASS Address : public HashValue160, public CPrintable {
public:
	HasherEng& Hasher;
	String Comment;
	byte Ver;

	Address(HasherEng& hasher);
	explicit Address(HasherEng& hasher, const HashValue160& hash, RCString comment = "");
	explicit Address(HasherEng& hasher, const HashValue160& hash, byte ver);
	explicit Address(HasherEng& hasher, RCString s);

	Address& operator=(const Address& a) {
		if (&Hasher != &a.Hasher)
			Throw(errc::invalid_argument);
		HashValue160::operator=(a);
		Comment = a.Comment;
		Ver = a.Ver;
		return *this;
	}

	void CheckVer(HasherEng& hasher) const;
	String ToString() const override;

	bool operator<(const Address& a) const {
		return Ver < a.Ver ||
			(Ver == a.Ver && memcmp(data(), a.data(), 20) < 0);
	}
};

class KeyInfoBase {
	typedef KeyInfoBase class_type;
public:
	CanonicalPubKey PubKey;

#if UCFG_COIN_ECC!='S'
	CngKey Key;
#endif

	DateTime Timestamp;
	String Comment;

	static KeyInfoBase GenRandom(bool bCompressed = true);

	Blob PlainPrivKey() const;

	Blob get_PrivKey() const { return m_privKey; }
	DEFPROP_GET(Blob, PrivKey);


	Address ToAddress() const;
	void SetPrivData(const ConstBuf& cbuf, bool bCompressed);
	void SetPrivData(const PrivateKey& privKey);
	void SetKeyFromPubKey();
	String ToString(RCString password) const;					// To WIF / BIP0038 formats
	Blob SignHash(const ConstBuf& cbuf);
	static bool VerifyHash(const ConstBuf& pubKey, const HashValue& hash, const ConstBuf& sig);
	
	Blob ToPrivateKeyDER() const;
	static KeyInfoBase FromDER(const ConstBuf& privKey, const ConstBuf& pubKey);
private:
	Blob m_privKey;
};


} // Coin::

namespace std {

template<> struct hash<Coin::HashValue> {
	size_t operator()(const Coin::HashValue& v) const {
		return *(size_t*)v.data();
//		return hash<std::array<byte, 32>>()(v);
	}
};

template<> struct hash<Coin::HashValue160> {
	size_t operator()(const Coin::HashValue160& v) const {
		return *(size_t*)v.data();
//		return hash<std::array<byte, 20>>()(v);
	}
};

} // std::




