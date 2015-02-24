#include <el/ext.h>

#include <el/crypto/hash.h>
using namespace Crypto;

#include "util.h"

namespace Coin {

#ifdef _AFXDLL
static struct CCoinutilInit {
	CCoinutilInit() {
		CMessageProcessor::RegisterModule((DWORD)E_COIN_BASE, (DWORD)E_COIN_UPPER, VER_INTERNALNAME_STR);
	}
} s_coinutilInit;
#endif

static const uint32_t s_sha256_hinit[8] = {
	0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a, 0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19
};

static const uint32_t g_sha256_k[64] = {
    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, //  0
    0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
    0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, //  8
    0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
    0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, // 16
    0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
    0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, // 24
    0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
    0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, // 32
    0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
    0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, // 40
    0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
    0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, // 48
    0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
    0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, // 56
    0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2
};

DECLSPEC_ALIGN(64) uint32_t g_4sha256_k[64][4];

static struct Sha256SSEInit {
	Sha256SSEInit() {
		for (int i=0; i<64; ++i)
			g_4sha256_k[i][0] = g_4sha256_k[i][1] = g_4sha256_k[i][2] = g_4sha256_k[i][3] = g_sha256_k[i];
	}
} s_sha256SSEInit;

HashAlgo StringToAlgo(RCString s) {
	String ua = s.ToUpper();
   	if (ua == "SHA256")
   		return HashAlgo::Sha256;
   	else if (ua == "SHA3" || ua=="KECCAK")
		return HashAlgo::Sha3;
   	else if (ua == "SCRYPT")
		return HashAlgo::SCrypt;
	else if (ua == "NEOSCRYPT")
		return HashAlgo::NeoSCrypt;
	else if (ua == "PRIME")
   		return HashAlgo::Prime;
   	else if (ua == "MOMENTUM")
   		return HashAlgo::Momentum;
   	else if (ua == "SOLID")
   		return HashAlgo::Solid;
   	else if (ua == "METIS")
   		return HashAlgo::Metis;
   	else
   		throw Exception(E_INVALIDARG, "Unknown hashing algorithm "+s);
}

void BitsToTargetBE(uint32_t bits, byte target[32]) {
	memset(target, 0, 32);
	int off = byte(bits>>24)-3;
	if (off < 30 && off >= -2)
		target[29-off] = byte(bits>>16);
	if (off < 31 && off >= -1)
		target[30-off] = byte(bits>>8);
	if (off < 32 && off >= 0)
		target[31-off] = byte(bits);
}

HashValue::HashValue(const hashval& hv) {
	if (hv.size() != 32)
		Throw(E_INVALIDARG);
	memcpy(data(), hv.data(), hv.size());
}

HashValue::HashValue(const ConstBuf& mb) {
	if (mb.Size != 32)
		Throw(E_INVALIDARG);
	memcpy(data(), mb.P, mb.Size);
}

HashValue HashValue::FromDifficultyBits(uint32_t bits) {
	HashValue r;
	BitsToTargetBE(bits, r.data());
	std::reverse(r.data(), r.data()+32);
	return r;
}

uint32_t HashValue::ToDifficultyBits() const {
	int i = 31;
	for (; i>=3; --i) {
		if (data()[i] || (data()[i-1] & 0x80))
			break;
	}
	return uint32_t(((i+1) << 24) | (int(data()[i]) << 16) | (int(data()[i-1]) << 8) | int(data()[i-2]));
}

HashValue HashValue::FromShareDifficulty(double difficulty, HashAlgo algo) {
	uint64_t leTarget;
	HashValue r;
	switch (algo) {
	case HashAlgo::Momentum:
		memcpy(r.data()+24, &(leTarget = htole(uint64_t(0x00000000FFFF0000ULL / difficulty))), 8);
		break;
	case HashAlgo::SCrypt:
	case HashAlgo::NeoSCrypt:
		memcpy(r.data()+23, &(leTarget = htole(uint64_t(0x00FFFF0000000000ULL / difficulty))), 8);
		break;
	case HashAlgo::Metis:
		memcpy(r.data()+22, &(leTarget = htole(uint64_t(0x00FFFF0000000000ULL / difficulty))), 8);
		break;
	default:
		memcpy(r.data()+21, &(leTarget = htole(uint64_t(0x00FFFF0000000000ULL / difficulty))), 8);
	}
	return r;
}

BlockHashValue::BlockHashValue(const ConstBuf& mb) {
	ASSERT(mb.Size>=1 && mb.Size<=32);
	memcpy(data(), mb.P, mb.Size);
}

HashValue::HashValue(RCString s) {
	Blob blob = Blob::FromHexString(s);	
	ASSERT(blob.Size == 32);
	reverse_copy(blob.constData(), blob.constData()+32, data());
}

bool HashValue::operator<(const HashValue& v) const {
	return lexicographical_compare(rbegin(), rend(), v.rbegin(), v.rend());
}

HashValue HashValue::Combine(const HashValue& h1, const HashValue& h2) {
	byte buf[64];
	memcpy(buf, h1.data(), h1.size());
	memcpy(buf+32, h2.data(), h2.size());
	SHA256 sha;
#ifdef X_DEBUG//!!!D
	return HashValue(sha.ComputeHash(ConstBuf(buf, 64)));
#endif
	return HashValue(sha.ComputeHash(sha.ComputeHash(ConstBuf(buf, 64))));
}

HashValue160::HashValue160(RCString s) {
	Blob blob = Blob::FromHexString(s);	
	ASSERT(blob.Size == 20);
	reverse_copy(blob.constData(), blob.constData()+20, data());
}

COIN_UTIL_API ostream& operator<<(ostream& os, const HashValue& hash) {
	Blob blob(&hash[0], hash.size());
	reverse(blob.data(), blob.data()+blob.Size);
	return os << blob;
}

Blob CalcSha256Midstate(const ConstBuf& mb) {
	uint32_t w[64];
	uint32_t *pw = (uint32_t*)mb.P;
	for (int i=0; i<16; ++i)
		w[i] = letoh(pw[i]);
	Blob r(s_sha256_hinit, 8*sizeof(uint32_t));
	uint32_t *pv = (uint32_t*)r.data();
	SHA256().HashBlock(pv, (const byte*)w, 0);				// BitcoinSha256().CalcRounds(w, s_sha256_hinit, pv, 16, 0, 64);
	for (int i=0; i<8; ++i)
		pv[i] = htole(pv[i]);
	return r;
}

void CoinSerialized::WriteVarInt(BinaryWriter& wr, uint64_t v) {
	if (v < 0xFD)
		wr << byte(v);
	else if (v <= 0xFFFF)
		wr << byte(0xFD) << uint16_t(v);
	else if (v <= 0xFFFFFFFFUL)
		wr << byte(0xFE) << uint32_t(v);
	else
		wr << byte(0xFF) << v;
}

uint64_t CoinSerialized::ReadVarInt(const BinaryReader& rd) {
	switch (byte pref = rd.ReadByte()) {
	case 0xFD:	return rd.ReadUInt16();
	case 0xFE:	return rd.ReadUInt32();
	case 0xFF:	return rd.ReadUInt64();
	default:
		return pref;
	}
}

void CoinSerialized::WriteString(BinaryWriter& wr, RCString s) {
	const char *p = s;
	size_t len = strlen(p);
	WriteVarInt(wr, len);
	wr.Write(p, len);
}

String CoinSerialized::ReadString(const BinaryReader& rd) {
	Blob blob(0, (size_t)ReadVarInt(rd));
	rd.Read(blob.data(), blob.Size);
	return String((const char*)blob.constData(), blob.Size);
}

void CoinSerialized::WriteBlob(BinaryWriter& wr, const ConstBuf& mb) {
	WriteVarInt(wr, mb.Size);
	wr.Write(mb.P, mb.Size);
}

Blob CoinSerialized::ReadBlob(const BinaryReader& rd) {
	size_t size = (size_t)ReadVarInt(rd);
	if (size > 100000000)	//!!!
		Throw(E_EXT_Protocol_Violation);
	Blob r(0, size);
	rd.Read(r.data(), r.Size);
	return r;
}

void BlockBase::WriteHeader(BinaryWriter& wr) const {
	wr << Ver << PrevBlockHash << MerkleRoot() << (uint32_t)to_time_t(Timestamp) << DifficultyTargetBits << Nonce;
}

HashValue BlockBase::GetHash() const {
	MemoryStream ms;
	WriteHeader(BinaryWriter(ms).Ref());
	return Coin::Hash(ms);
}

Blob Swab32(const ConstBuf& buf) {
	if (buf.Size % 4)
		Throw(E_INVALIDARG);
	Blob r(buf);
	uint32_t *p = (uint32_t*)r.data();
	for (int i=0; i<buf.Size/4; ++i)
		p[i] = _byteswap_ulong(p[i]);
	return r;
}

void FormatHashBlocks(void* pbuffer, size_t len) {
    byte* pdata = (byte*)pbuffer;
    uint32_t blocks = 1 + ((len + 8) / 64);
    pdata[len] = 0x80;
    memset(pdata + len+1, 0, 64 * blocks - len-1);
    uint32_t* pend = (uint32_t*)(pdata + 64 * blocks);
	pend[-1] = htobe(uint32_t(len * 8));
	for (uint32_t *p=(uint32_t*)pbuffer; p!=pend; ++p)
		*p = _byteswap_ulong(*p);
}

BinaryWriter& operator<<(BinaryWriter& wr, const CCoinMerkleBranch& branch) {
	CoinSerialized::Write(wr, branch.Vec);	
	return wr << int32_t(branch.Index);
}

const BinaryReader& operator>>(const BinaryReader& rd, CCoinMerkleBranch& branch) {
	CoinSerialized::Read(rd, branch.Vec);
	branch.Index = rd.ReadInt32();
	branch.m_h2 = &HashValue::Combine;
	return rd;
}


/*!!!R
void MerkleBranch::Write(BinaryWriter& wr) const {
	CoinSerialized::Write(wr, Vec);	
	wr << Index;
}

void MerkleBranch::Read(const BinaryReader& rd) {
	CoinSerialized::Read(rd, Vec);	
	rd >> Index;
}

HashValue MerkleBranch::Apply(HashValue hash) const {
	if (-1 == Index)
		return HashValue();
	int idx = Index;
	EXT_FOR (const HashValue& other, Vec) {
		byte buf[32*2];
		if (idx & 1) {
			memcpy(buf, other.data(), 32);
			memcpy(buf+32, hash.data(), 32);
		} else {
			memcpy(buf, hash.data(), 32);
			memcpy(buf+32, other.data(), 32);
		}
		hash = Hash(ConstBuf(buf, sizeof buf));
		idx >>= 1;
	}
	return hash;
}

MerkleBranch MerkleTree::GetBranch(int idx) {
	MerkleBranch r;
	r.Index = idx;
	for (int j=0, n=size(); n>1; j+=n, n=(n+1)/2, idx>>=1)
		r.Vec.push_back(_self[j + std::min(idx^1, n-1)]);
	return r;
}
*/

ShaConstants GetShaConstants() {
	ShaConstants r = { 	s_sha256_hinit, g_sha256_k, g_4sha256_k };
	return r;
}

pair<uint32_t, uint32_t> FromOptionalNonceRange(const VarValue& json) {
	if (VarValue vNonceRange = json["noncerange"]) {
		Blob blob = Blob::FromHexString(vNonceRange.ToString());
		if (blob.Size == 8)
			return make_pair(betoh(*(uint32_t*)blob.constData()), betoh(*(uint32_t*)(blob.constData()+4)));
	}
	return pair<uint32_t, uint32_t>(0, 0xFFFFFFFF);
}

EXT_THREAD_PTR(HasherEng) t_pHasherEng;

HasherEng *HasherEng::GetCurrent() {
	return t_pHasherEng;
}

void HasherEng::SetCurrent(HasherEng *heng) {
	t_pHasherEng = heng;
}

HashValue HasherEng::HashBuf(const ConstBuf& cbuf) {
	SHA256 sha;
	return ConstBuf(sha.ComputeHash(sha.ComputeHash(cbuf)));
}

CHasherEngThreadKeeper::CHasherEngThreadKeeper(HasherEng *cur) {
	m_prev = t_pHasherEng;
	t_pHasherEng = cur;
}

CHasherEngThreadKeeper::~CHasherEngThreadKeeper() {
	t_pHasherEng = m_prev;
}

HashValue Hash(const ConstBuf& mb) {
	return HasherEng::GetCurrent()->HashBuf(mb);
}

static class CoinCategory : public error_category {
	typedef error_category base;

	const char *name() const noexcept override { return "Coin"; }

	string message(int errval) const override {
		return HResultToMessage(E_COIN_BASE + errval).c_str();
	}
} s_coin_category;

const error_category& coin_category() {
	return s_coin_category;
}

static const char * const s_reasons[] = {
	"bad-cb-flag", "bad-cb-length", "bad-cb-prefix", "bad-diffbits", "bad-prevblk", "bad-txnmrklroot", "bad-txns", "bad-version", "duplicate", "high-hash", "rejected", "stale-prevblk", "stale-work",
	"time-invalid", "time-too-new", "time-too-old", "unknown-user", "unknown-work"
};

void ThrowRejectionError(RCString reason) {
	for (int i=0; i<size(s_reasons); ++i)
		if (s_reasons[i] == reason)
			Throw(E_COIN_MINER_BAD_CB_FLAG+i);
	Throw(E_FAIL);
}

int SubmitRejectionCode(RCString rej) {
	for (int i=0; i<size(s_reasons); ++i)
		if (s_reasons[i] == rej)
			return (E_COIN_MINER_BAD_CB_FLAG & 0xFFFF) + i;
	return E_COIN_MINER_REJECTED & 0xFFFF;
}


} // Coin::

