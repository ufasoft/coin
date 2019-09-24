/*######   Copyright (c) 2011-2019 Ufasoft  http://ufasoft.com  mailto:support@ufasoft.com,  Sergey Pavlov  mailto:dev@ufasoft.com ####
#                                                                                                                                     #
# 		See LICENSE for licensing information                                                                                         #
#####################################################################################################################################*/

#pragma once

#include <el/crypto/hash.h>
using namespace Ext::Crypto;

#include <random>

#include EXT_HEADER_OPTIONAL

#ifndef UCFG_COIN_ECC
#	define UCFG_COIN_ECC 'S' // Secp256k1
#endif

#ifndef UCFG_COIN_USE_OPENSSL
#	define UCFG_COIN_USE_OPENSSL 0
#endif

#if UCFG_COIN_ECC != 'S' && UCFG_COIN_USE_OPENSSL
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

const size_t MAX_WITNESS_PROGRAM = 40;
const size_t MAX_PUBKEY = 65;

ENUM_CLASS(HashAlgo){Sha256, Sha3, SCrypt, Prime, Momentum, Solid, Metis, NeoSCrypt, Groestl} END_ENUM_CLASS(HashAlgo);

HashAlgo StringToAlgo(RCString s);
String AlgoToString(HashAlgo algo);

class HashValue : Ext::totally_ordered<HashValue> {
	uint64_t m_data[4];
public:
	typedef uint8_t* iterator;
	typedef const uint8_t* const_iterator;

	typedef std::reverse_iterator<iterator> reverse_iterator;
	typedef std::reverse_iterator<const_iterator> const_reverse_iterator;

	HashValue() {
		memset(data(), 0, 32);
	}

	explicit HashValue(const uint8_t ar[32]) {
		memcpy(data(), ar, 32);
	}

	HashValue(const hashval& hv);
	explicit HashValue(RCSpan mb);
	explicit HashValue(RCString s);
    explicit HashValue(const char *s);

	static const HashValue& Null();

	uint8_t* data() {
		return (uint8_t*)m_data;
	}
	const uint8_t* data() const {
		return (const uint8_t*)m_data;
	}

	const_iterator begin() const {
		return data();
	}
	const_iterator end() const {
		return data() + 32;
	}
	const_reverse_iterator rbegin() const {
		return const_reverse_iterator(end());
	}
	const_reverse_iterator rend() const {
		return const_reverse_iterator(begin());
	}

	explicit operator Span() const { return Span(data(), 32); }
	Span ToSpan() const { return Span(data(), 32); }

	static HashValue FromDifficultyBits(uint32_t bits);
	uint32_t ToDifficultyBits() const;
	static HashValue FromShareDifficulty(double difficulty, HashAlgo algo);

	EXPLICIT_OPERATOR_BOOL() const {
		return *this == Null() ? 0 : EXT_CONVERTIBLE_TO_TRUE;
	}

	bool operator!() const {
		return !bool(*this);
	}

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

	void Print(ostream& os, bool bFull = true) const;

	String ToString(bool bFull = true) const {
		ostringstream os;
		Print(os, bFull);
		return os.str();
	}
};

COIN_UTIL_API ostream& operator<<(ostream& os, const HashValue& hash);
COIN_UTIL_API wostream& operator<<(wostream& os, const HashValue& hash);

/*!!!R collision with tests
inline String ToString(const HashValue& hash) {
	ostringstream os;
	os << hash;
	return os.str();
}
*/

/*!!!?
inline wostream& operator<<(wostream& os, const HashValue& hash) {
	ostringstream oss;
	oss << hash;
	return os << String(oss.str());
}*/

class BlockHashValue : public HashValue {
	typedef HashValue base;

public:
	BlockHashValue(RCSpan mb);
};

class COIN_CLASS HashValue160 : public std::array<uint8_t, 20> {
public:
	HashValue160() {
		memset(data(), 0, 20);
	}

	explicit HashValue160(RCSpan mb) {
		ASSERT(mb.size() == 20);
		memcpy(data(), mb.data(), mb.size());
	}

	explicit HashValue160(RCString s);
};

class ReducedHashValue {
public:
	ReducedHashValue(const HashValue& hash)
		: m_val(letoh(*(int64_t*)hash.data())) {
	}

	/*	ReducedHashValue(const HashValue160& hash)
		:	m_val(letoh(*(int64_t*)hash.data()))
	{
	} */

	bool operator==(const ReducedHashValue& v) const {
		return m_val == v.m_val;
	}

	operator int64_t() const {
		return m_val;
	}

private:
	int64_t m_val;
};

#if UCFG_COIN_PUBKEYID_36BITS
const int64_t PUBKEYID_MASK = 0xFFFFFFFFFLL;
#else
const int64_t PUBKEYID_MASK = 0x7FFFFFFFFLL;
#endif

class CIdPk {
private:
	int64_t m_val;
public:
	explicit CIdPk(const HashValue160& hash)
		: m_val(letoh(*(int64_t*)(hash.data() + 12)) & PUBKEYID_MASK) //	Use last bytes. First bytes are not random, because many generated addresses are like "1ReadableWord..."
	{
	}

	explicit CIdPk(int64_t val = -1)
		: m_val(val) {
	}

	bool operator==(const CIdPk& v) const {
		return m_val == v.m_val;
	}

	operator int64_t() const {
		if (IsNull())
			Throw(E_FAIL);
		return m_val;
	}

	bool IsNull() const {
		return m_val == -1;
	}
};

class ReducedBlockHash {
	Span m_buf;
public:
	ReducedBlockHash(const HashValue& hash)
		: m_buf(hash.ToSpan())
	{
		while (m_buf.size() > 1 && !m_buf[m_buf.size() - 1])
			m_buf = m_buf.first(m_buf.size() - 1);
	}

    const uint8_t *data() const { return m_buf.data(); }
    size_t size() const { return m_buf.size(); }
};

COIN_UTIL_EXPORT HashValue Hash(RCSpan mb);
COIN_UTIL_EXPORT HashValue160 Hash160(RCSpan mb);
COIN_UTIL_EXPORT String ConvertToBase58(RCSpan cbuf);
COIN_UTIL_EXPORT Blob ConvertFromBase58(RCString s, bool bCheckHash = true);
String ConvertToBase58ShaSquare(RCSpan cbuf);
Blob ConvertFromBase58ShaSquare(RCString s);
COIN_UTIL_EXPORT Blob CalcSha256Midstate(RCSpan mb);

COIN_UTIL_EXPORT HashValue SHA256_SHA256(RCSpan cbuf);
COIN_UTIL_EXPORT HashValue ScryptHash(RCSpan mb);
COIN_UTIL_EXPORT HashValue NeoSCryptHash(RCSpan mb, int profile);
HashValue SolidcoinHash(RCSpan cbuf);
HashValue MetisHash(RCSpan cbuf);
COIN_UTIL_EXPORT HashValue GroestlHash(RCSpan mb);

bool MomentumVerify(const HashValue& hash, uint32_t a, uint32_t b);

struct SCoinMessageHeader {
	uint32_t Magic;
	char Command[12];
	uint32_t PayloadSize;
};

class ProtocolWriter : public BinaryWriter {
    typedef BinaryWriter base;
public:
	// Used in SignatureHash only
	Span ClearedScript;
	int NIn;
	CBool ForSignatureHash, HashTypeSingle, HashTypeNone, HashTypeAnyoneCanPay;
	CBool WitnessAware;

    explicit ProtocolWriter(Stream& stm, bool bWitnessAware = true)
        : base(stm)
		, WitnessAware(bWitnessAware)
	{
    }

    ProtocolWriter& Ref() { return *this; }
};

class ProtocolReader : public BinaryReader {
    typedef BinaryReader base;
public:
    CBool WitnessAware, MayBeHeader;

    explicit ProtocolReader(const Stream& stm, bool witnessAware = false)
		: base(stm)
		, WitnessAware(witnessAware)
	{
    }
};

struct ShortTxId {
	uint8_t Data[6];
};

class CoinSerialized {
public:
	static const uint32_t MAX_SIZE = 32 * 1024 * 1024;

	uint32_t Ver;

	CoinSerialized()
		: Ver(1) {
	}

	static void WriteCompactSize(BinaryWriter& wr, uint64_t v);
	static uint64_t ReadCompactSize64(const BinaryReader& rd);
	static uint32_t ReadCompactSize(const BinaryReader& rd);
	static String ReadString(const BinaryReader& rd);
	static void WriteString(BinaryWriter& wr, RCString s);
	static void WriteSpan(BinaryWriter& wr, RCSpan mb);
	static Blob ReadBlob(const BinaryReader& rd);

    template <int N>
    static void WriteEl(ProtocolWriter& wr, const array<uint8_t, N>& ar) {
        wr.Write(Span(ar));
    }

	static void WriteEl(ProtocolWriter& wr, const ShortTxId& txId) {
		wr.Write(txId.Data, sizeof(txId.Data));
	}

    template <class T>
    static void WriteEl(ProtocolWriter& wr, const T& v) { v.Write(wr); }

	template <class T>
    static void Write(ProtocolWriter& wr, const vector<T>& ar) {
		WriteCompactSize(wr, ar.size());
        for (size_t i = 0; i < ar.size(); ++i)
            WriteEl(wr, ar[i]);
	}

    template <int N>
    static void ReadEl(const ProtocolReader& rd, array<uint8_t, N>& ar) {
        rd.Read(ar.data(), N);
    }

	static void ReadEl(const ProtocolReader& rd, ShortTxId txId) {
		rd.Read(txId.Data, sizeof(txId.Data));
	}

    template <class T>
    static void ReadEl(const ProtocolReader& rd, T& v) { v.Read(rd); }

	template <class T> static void Read(const ProtocolReader& rd, vector<T>& ar, size_t maxSize = MAX_SIZE) {
        uint32_t size = ReadCompactSize(rd);
        if (size > maxSize)
            Throw(ExtErr::Protocol_Violation);
		ar.resize(size);
		for (size_t i = 0; i < size; ++i)
			ReadEl(rd, ar[i]);
	}
};

struct BlockHeaderBinary {
	int32_t Ver;
	uint32_t
		PrevBlockHash[8],
		MerkleRoot[8],
		Timestamp, DifficultyTargetBits, Nonce;
};

class BlockRef {
public:
	const HashValue Hash;
	const int HeightInTrunk;

	BlockRef(int h)
		: HeightInTrunk(h)
	{}

	BlockRef(int h, const HashValue& hash)
		: Hash(hash)
		, HeightInTrunk(h)
	{}

	BlockRef(const HashValue& hash)
		: Hash(hash)
		, HeightInTrunk(-1)
	{}

	explicit operator bool() const { return HeightInTrunk >= 0 || Hash; }
	bool IsInTrunk() const { return HeightInTrunk >= 0; }
};

class COIN_UTIL_CLASS BlockBase : public InterlockedObject {
public:
	HashValue PrevBlockHash;
	mutable optional<HashValue> m_merkleRoot;
	DateTime Timestamp;
	uint32_t DifficultyTargetBits;
	int32_t Ver;						// Important to be signed
	uint32_t Height;
	uint32_t Nonce;
	CBool IsInTrunk;

	BlockBase()
		: Ver(4)
		, Height(uint32_t(-1)) {
	}

	BlockRef Prev() const {
		return IsInTrunk ? BlockRef(Height - 1, PrevBlockHash) : BlockRef(PrevBlockHash);
	}

	virtual void WriteHeader(ProtocolWriter& wr) const;
	virtual Coin::HashValue MerkleRoot(bool bSave = true) const = 0;
	virtual Coin::HashValue GetHash() const;
};

COIN_UTIL_EXPORT Blob Swab32(RCSpan buf);
COIN_UTIL_EXPORT void FormatHashBlocks(void* pbuffer, size_t len);

typedef HashValue (*PFN_Combine)(const HashValue& h1, const HashValue& h2);

typedef MerkleBranch<HashValue, PFN_Combine> CCoinMerkleBranch;
typedef MerkleTree<HashValue, PFN_Combine> CCoinMerkleTree;

ProtocolWriter& operator<<(ProtocolWriter& wr, const CCoinMerkleBranch& branch);
const ProtocolReader& operator>>(const ProtocolReader& rd, CCoinMerkleBranch& branch);

struct ShaConstants {
	const uint32_t *pg_sha256_hinit, *pg_sha256_k;
	const uint32_t (*pg_4sha256_k)[4];
};

ShaConstants GetShaConstants();

void BitsToTargetBE(uint32_t bits, uint8_t target[32]);

pair<uint32_t, uint32_t> FromOptionalNonceRange(const VarValue& json);

const int PASSWORD_ENCRYPT_ROUNDS_A = 1000, PASSWORD_ENCRYPT_ROUNDS_B = 100 * 1000;

const char DEFAULT_PASSWORD_ENCRYPT_METHOD = 'B';

class HasherEng : public InterlockedObject {
public:
	String Hrp;
	char HrpSeparator;
	uint8_t AddressVersion, ScriptAddressVersion;

	HasherEng()
		: AddressVersion(0)
		, ScriptAddressVersion(5)
		, HrpSeparator('1')
    {
		Hrp = "bc";
    }

	virtual HashValue HashBuf(RCSpan cbuf);
	virtual HashValue HashForAddress(RCSpan cbuf);
	static HasherEng* GetCurrent();

protected:
	static void SetCurrent(HasherEng* heng);
};

class CHasherEngThreadKeeper {
	HasherEng* m_prev;

public:
	CHasherEngThreadKeeper(HasherEng* cur);
	~CHasherEngThreadKeeper();
};

class PrivateKey : public array<uint8_t, 32>, public CPrintable{
public:
	bool Compressed;

	PrivateKey()
		: Compressed(false)
	{
	}
	PrivateKey(RCSpan cbuf, bool bCompressed);
	explicit PrivateKey(RCString s);
	String ToString() const override;
};

class CanonicalPubKey {
	typedef CanonicalPubKey class_type;
public:
	const static size_t MAX_SIZE = 65;

	typedef vararray<uint8_t, MAX_SIZE> CData;
	CData Data;

	CanonicalPubKey() {
	}

	CanonicalPubKey(nullptr_t)
		: Data(0) {
	}

	CanonicalPubKey(RCSpan cbuf)
		: Data(cbuf.data(), CheckArgSize(cbuf.size())) {
	}

	bool IsCompressed() const {
		return Data.size() == 33;
	}

	HashValue160 get_Hash160() const {
		return Coin::Hash160(Data);
	}
	DEFPROP_GET(HashValue160, Hash160);

	HashValue160 get_ScriptHash() const;
	DEFPROP_GET(HashValue160, ScriptHash);

	bool IsValid() const {
		uint8_t b0 = Data.constData()[0];
		return (Data.size() == 33 && (b0 == 2 || b0 == 3)) || (Data.size() == 65 || b0 == 4);
	}

	Blob ToCompressed() const;
	static CanonicalPubKey FromCompressed(RCSpan cbuf);
private:
	static size_t CheckArgSize(size_t size) {
		if (size > MAX_SIZE)
			Throw(E_INVALIDARG);
		return size;
	}
};

enum class AddressType : uint8_t {				// Used in WalletDb in pubkeys.type field
	P2PKH = 0
	, Legacy = P2PKH //!!!R
	, P2SH = 1
	, Bech32 = 2
	, PubKey = 3
	, MultiSig = 4
	, NullData = 5
	, WitnessV0ScriptHash = 6
	, WitnessV0KeyHash = 7
	, WitnessUnknown = 8
	, NonStandard = 9
	, P2WPKH_IN_P2SH = 10
	, P2WSH_IN_P2SH = 11
};

class COIN_CLASS AddressObj : public InterlockedObject {
public:
	HasherEng& Hasher;
	String Comment;

	typedef vararray<uint8_t, MAX_PUBKEY> CData;
	CData Data;
	vector<Blob> Datas;
	uint8_t WitnessVer;
	uint8_t RequiredSigs;
	AddressType Type;

	AddressObj(HasherEng& hasher);
	AddressObj(HasherEng& hasher, AddressType typ, RCSpan data, uint8_t witnessVer, RCString comment);
	AddressObj(HasherEng& hasher, RCString s, RCString comment);
	String ToString() const;
	Blob ToScriptPubKey() const;
	void DecodeBech32(RCString hrp, RCString s);
	void CheckVer(HasherEng& hasher) const;
};

class COIN_CLASS Address : public Pimpl<AddressObj>, public CPrintable {
	typedef Address class_type;
public:
	static const size_t MAX_LEN = 90;

	Address(HasherEng& hasher) {
		m_pimpl = new AddressObj(hasher);
	}

	explicit Address(HasherEng& hasher, AddressType typ, RCSpan data, uint8_t witnessVer = 0, RCString comment = "") {
		m_pimpl = new AddressObj(hasher, typ, data, witnessVer, comment);
	}

	explicit Address(HasherEng& hasher, RCString s, RCString comment = "") {
		m_pimpl = new AddressObj(hasher, s, comment);
	}

	Address& operator=(const Address& a);

	AddressType get_Type() const { return m_pimpl->Type; }
	DEFPROP_GET(AddressType, Type);

	uint8_t WitnessVer() const { return m_pimpl->WitnessVer; }
	String ToString() const override { return m_pimpl->ToString(); }
	explicit operator HashValue160() const;
	explicit operator HashValue() const;

	String get_Comment() const { return m_pimpl->Comment; }
	void put_Comment(RCString comment) { m_pimpl->Comment = comment; }
	DEFPROP(String, Comment);

	Span Data() const { return m_pimpl->Data; }

	bool operator==(const Address& a) const {
		return m_pimpl->Type == a->Type && Equal(m_pimpl->Data, a->Data);
	}

	bool operator<(const Address& a) const {
		return m_pimpl->Type < a->Type || (m_pimpl->Type == a->Type && memcmp(m_pimpl->Data.data(), a->Data.data(), m_pimpl->Data.size()) < 0); //!!!TODO: use WitnessVer & maybe Comment
	}
};

class Rng : noncopyable {
	mt19937 m_rng;
	uniform_int_distribution<> m_ud;;
public:
	Rng();
	uint8_t operator()() { return (uint8_t)m_ud(m_rng); }
};

class KeyInfoBase : public InterlockedObject {
	typedef KeyInfoBase class_type;

	PrivateKey m_privKey;
public:
	CanonicalPubKey PubKey;

#if UCFG_COIN_ECC != 'S'
	CngKey Key;
#endif

	DateTime Timestamp;
	String Comment;					// nullptr for change or reserved
	AddressType AddressType;
	bool Reserved;

	KeyInfoBase()
		: AddressType(AddressType::Legacy)
		, Reserved(false)
	{
	}

	void GenRandom(Rng& rng, bool bCompressed = true);

	Blob PlainPrivKey() const;

	const PrivateKey& get_PrivKey() const {
		return m_privKey;
	}
	DEFPROP_GET(const PrivateKey&, PrivKey);

	vararray<uint8_t, 25> ToPubScript() const;
	Address ToAddress() const;
	void SetPrivData(const PrivateKey& privKey, bool bCompressed);
	void SetPrivData(const PrivateKey& privKey) { SetPrivData(privKey, privKey.Compressed); }
	void SetKeyFromPubKey();
	String ToString(RCString password) const; // To WIF / BIP0038 formats
	Blob SignHash(RCSpan cbuf);

	Blob ToPrivateKeyDER() const;
	void FromDER(RCSpan privKey, RCSpan pubKey);
	void FromBIP38(RCString bip38, RCString password);
};

} // namespace Coin

namespace std {

template <> struct hash<Coin::HashValue> {
	size_t operator()(const Coin::HashValue& v) const {
		return *(size_t*)v.data();
		//		return hash<std::array<uint8_t, 32>>()(v);
	}
};

template <> struct hash<Coin::HashValue160> {
	size_t operator()(const Coin::HashValue160& v) const {
		return *(size_t*)v.data();
		//		return hash<std::array<uint8_t, 20>>()(v);
	}
};

template <> struct hash<Coin::Address> {
	size_t operator()(const Coin::Address& a) const {
		return hash<String>()(a.ToString());		//!!!TODO Optimize
	}
};

} // namespace std
