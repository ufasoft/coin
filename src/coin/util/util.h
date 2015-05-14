/*######   Copyright (c) 2011-2015 Ufasoft  http://ufasoft.com  mailto:support@ufasoft.com,  Sergey Pavlov  mailto:dev@ufasoft.com ####
#                                                                                                                                     #
# 		See LICENSE for licensing information                                                                                         #
#####################################################################################################################################*/

#pragma once

#include <el/crypto/hash.h>
using namespace Ext::Crypto;

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

class HashValue : public std::array<byte, 32>, totally_ordered<HashValue> {
public:
	HashValue() {
		memset(data(), 0, 32);
	}

	explicit HashValue(const byte ar[32]) {
		memcpy(data(), ar, 32);
	}

	HashValue(const hashval& hv);
	HashValue(const ConstBuf& mb);
	explicit HashValue(RCString s);
	static HashValue FromDifficultyBits(uint32_t bits);
	uint32_t ToDifficultyBits() const;
	static HashValue FromShareDifficulty(double difficulty, HashAlgo algo);

	bool operator<(const HashValue& v) const;

	static HashValue Combine(const HashValue& h1, const HashValue& h2);

	void Write(BinaryWriter& wr) const {
		wr.Write(&_self[0], size());
	}

	void Read(const BinaryReader& rd) {
		rd.Read(&_self[0], size());
	}
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
		:	m_buf(hash)
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

COIN_UTIL_EXPORT  HashValue ScryptHash(const ConstBuf& mb);
COIN_UTIL_EXPORT  HashValue NeoSCryptHash(const ConstBuf& mb, int profile);
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




