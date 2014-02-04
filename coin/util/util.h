#pragma once

#include <el/crypto/hash.h>
using namespace Ext::Crypto;

#include "coin-msg.h"

#ifndef UCFG_COIN_PUBKEYID_36BITS
#	define UCFG_COIN_PUBKEYID_36BITS 0
#endif

#ifndef UCFG_COIN_LIB_DECLS
#	define UCFG_COIN_LIB_DECLS UCFG_LIB_DECLS
#endif

#if UCFG_LIB_DECLS
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

#if UCFG_COIN_LIB_DECLS
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


namespace Coin {

ENUM_CLASS(HashAlgo) {
	Sha256,
	SCrypt,
	Prime,
	Momentum,
	Solid,
	Metis
} END_ENUM_CLASS(HashAlgo);

HashAlgo StringToAlgo(RCString s);

class HashValue : public std::array<byte, 32> {
public:
	HashValue() {
		memset(data(), 0, 32);
	}

	explicit HashValue(const byte ar[32]) {
		memcpy(data(), ar, 32);
	}

	HashValue(const ConstBuf& mb);
	explicit HashValue(RCString s);
	static HashValue FromDifficultyBits(UInt32 bits);
	UInt32 ToDifficultyBits() const;
	static HashValue FromShareDifficulty(double difficulty, HashAlgo algo);

	bool operator<(const HashValue& v) const;
	bool operator>(const HashValue& v) const { return v < *this; }
	bool operator<=(const HashValue& v) const { return !(v < *this); }
	bool operator>=(const HashValue& v) const { return !operator<(v); }

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
		:	m_val(letoh(*(Int64*)hash.data()))
	{
	}

/*	ReducedHashValue(const HashValue160& hash)
		:	m_val(letoh(*(Int64*)hash.data()))
	{
	} */

	bool operator==(const ReducedHashValue& v) const {
		return m_val == v.m_val;
	}

	operator Int64() const { return m_val; }
private:
	Int64 m_val;
};

#if UCFG_COIN_PUBKEYID_36BITS
	const Int64 PUBKEYID_MASK = 0xFFFFFFFFFLL;
#else
	const Int64 PUBKEYID_MASK = 0x7FFFFFFFFLL;
#endif

class CIdPk {
public:
	explicit CIdPk(const HashValue160& hash)
		:	m_val(letoh(*(Int64*)(hash.data()+12)) & PUBKEYID_MASK)				//	Use last bytes. First bytes are not random, because many generated addresses are like "1ReadableWord..."
	{
	}

	explicit CIdPk(Int64 val = -1)
		:	m_val(val)
	{
	}

	bool operator==(const CIdPk& v) const {
		return m_val == v.m_val;
	}

	operator Int64() const { 
		if (IsNull())
			Throw(E_FAIL);
		return m_val;
	}

	bool IsNull() const { return m_val == -1; }
private:
	Int64 m_val;
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
COIN_UTIL_EXPORT Blob ConvertFromBase58(RCString s);
COIN_UTIL_EXPORT Blob CalcSha256Midstate(const ConstBuf& mb);

COIN_UTIL_EXPORT  HashValue ScryptHash(const ConstBuf& mb);
HashValue SolidcoinHash(const ConstBuf& cbuf);
HashValue MetisHash(const ConstBuf& cbuf);

bool MomentumVerify(const HashValue& hash, UInt32 a, UInt32 b);

struct SCoinMessageHeader {
	UInt32 Magic;
	char Command[12];
	UInt32 PayloadSize;
};


#pragma pack(push, 1)
struct SolidcoinBlock {
    Int32 nVersion;
    byte hashPrevBlock[32];
    byte hashMerkleRoot[32];
    Int64 nBlockNum;
    Int64 nTime;
    UInt64 nNonce1;
    UInt64 nNonce2;
    UInt64 nNonce3;
    UInt32 nNonce4;
    char miner_id[12];
    UInt32 dwBits;
};
#pragma pack(pop)


class CoinSerialized {
public:
	UInt32 Ver;

	CoinSerialized()
		:	Ver(1)
	{}

	static void WriteVarInt(BinaryWriter& wr, UInt64 v);
	static UInt64 ReadVarInt(const BinaryReader& rd);
	static String ReadString(const BinaryReader& rd);
	static void WriteString(BinaryWriter& wr, RCString s);
	static void WriteBlob(BinaryWriter& wr, const ConstBuf& mb);
	static Blob ReadBlob(const BinaryReader& rd);

	template <class T>
	static void Write(BinaryWriter& wr, const vector<T>& ar) {
		WriteVarInt(wr, ar.size());
		for (int i=0; i<ar.size(); ++i)
			wr << ar[i];
	}

	template <class T>
	static void Read(const BinaryReader& rd, vector<T>& ar) {
		ar.resize((size_t)ReadVarInt(rd));
		for (int i=0; i<ar.size(); ++i)
			ar[i].Read(rd);
	}
};

class COIN_UTIL_CLASS BlockBase : public Object {
public:
	typedef Interlocked interlocked_policy;

	HashValue PrevBlockHash;
	DateTime Timestamp;
	UInt32 DifficultyTargetBits;
	UInt32 Ver;
	UInt32 Height;
	UInt32 Nonce;
	mutable optional<Coin::HashValue> m_merkleRoot;
	
//!!!R	mutable CBool m_bMerkleCalculated;

	BlockBase()
		:	Ver(2)
		,	Height(UInt32(-1))
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
	const UInt32 *pg_sha256_hinit,
		*pg_sha256_k;
	const UInt32 (*pg_4sha256_k)[4];
};

ShaConstants GetShaConstants();

void BitsToTargetBE(UInt32 bits, byte target[32]);

pair<UInt32, UInt32> FromOptionalNonceRange(const VarValue& json);

static const int PASSWORD_ENCRYPT_ROUNDS_A = 1000,
			PASSWORD_ENCRYPT_ROUNDS_B = 100*1000;

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




