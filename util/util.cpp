/*######     Copyright (c) 1997-2013 Ufasoft  http://ufasoft.com  mailto:support@ufasoft.com,  Sergey Pavlov  mailto:dev@ufasoft.com #######################################
#                                                                                                                                                                          #
# This program is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation;  #
# either version 3, or (at your option) any later version. This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the      #
# implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details. You should have received a copy of the GNU #
# General Public License along with this program; If not, see <http://www.gnu.org/licenses/>                                                                               #
##########################################################################################################################################################################*/

#include <el/ext.h>

#include <el/crypto/hash.h>
using namespace Crypto;

#include "util.h"

namespace Coin {

static struct CCoinutilInit {
	CCoinutilInit() {
		CMessageProcessor::RegisterModule((DWORD)E_COIN_BASE, (DWORD)E_COIN_UPPER, VER_INTERNALNAME_STR);
	}
} s_coinutilInit;

HashValue::HashValue(const ConstBuf& mb) {
	if (mb.Size != 32)
		Throw(E_INVALIDARG);
	memcpy(data(), mb.P, mb.Size);
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
	return Hash(ConstBuf(buf, 64));
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

HashValue Hash(const ConstBuf& mb) {
	SHA256 sha;
	return ConstBuf(sha.ComputeHash(sha.ComputeHash(mb)));
}

Blob CalcSha256Midstate(const ConstBuf& mb) {
	UInt32 w[64];
	UInt32 *pw = (UInt32*)mb.P;
	for (int i=0; i<16; ++i)
		w[i] = letoh(pw[i]);
	Blob r(Ext::Crypto::g_sha256_hinit, 8*sizeof(UInt32));
	UInt32 *pv = (UInt32*)r.data();
	SHA256().HashBlock(pv, (const byte*)w, 0);				// BitcoinSha256().CalcRounds(w, Ext::Crypto::g_sha256_hinit, pv, 16, 0, 64);
	for (int i=0; i<8; ++i)
		pv[i] = htole(pv[i]);
	return r;
}

HashValue ScryptHash(const ConstBuf& mb) {
	array<UInt32, 8> ar = CalcSCryptHash(mb);
	HashValue r;
	memcpy(r.data(), ar.data(), 32);
	return r;
}

void CoinSerialized::WriteVarInt(BinaryWriter& wr, UInt64 v) {
	if (v < 0xFD)
		wr << byte(v);
	else if (v <= 0xFFFF)
		wr << byte(0xFD) << UInt16(v);
	else if (v <= 0xFFFFFFFFUL)
		wr << byte(0xFE) << UInt32(v);
	else
		wr << byte(0xFF) << v;
}

UInt64 CoinSerialized::ReadVarInt(const BinaryReader& rd) {
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
	Blob r(0, (size_t)ReadVarInt(rd));
	rd.Read(r.data(), r.Size);
	return r;
}

void BlockBase::WriteHeader(BinaryWriter& wr) const {
	wr << Ver << PrevBlockHash << MerkleRoot() << UInt32(Timestamp.UnixEpoch) << DifficultyTargetBits << Nonce;
}

Blob Swab32(const ConstBuf& buf) {
	if (buf.Size % 4)
		Throw(E_INVALIDARG);
	Blob r(buf);
	UInt32 *p = (UInt32*)r.data();
	for (int i=0; i<buf.Size/4; ++i)
		p[i] = _byteswap_ulong(p[i]);
	return r;
}

void FormatHashBlocks(void* pbuffer, size_t len) {
    byte* pdata = (byte*)pbuffer;
    UInt32 blocks = 1 + ((len + 8) / 64);
    pdata[len] = 0x80;
    memset(pdata + len+1, 0, 64 * blocks - len-1);
    UInt32* pend = (UInt32*)(pdata + 64 * blocks);
	pend[-1] = htobe(UInt32(len * 8));
	for (UInt32 *p=(UInt32*)pbuffer; p!=pend; ++p)
		*p = _byteswap_ulong(*p);
}

BinaryWriter& operator<<(BinaryWriter& wr, const CCoinMerkleBranch& branch) {
	CoinSerialized::Write(wr, branch.Vec);	
	return wr << Int32(branch.Index);
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
	ShaConstants r = { 	g_sha256_hinit, g_sha256_k, g_4sha256_k };
	return r;
}

void BitsToTargetBE(UInt32 bits, byte target[32]) {
	memset(target, 0, 32);
	int off = byte(bits>>24)-3;
	if (off < 30 && off >= -2)
		target[29-off] = byte(bits>>16);
	if (off < 31 && off >= -1)
		target[30-off] = byte(bits>>8);
	if (off < 32 && off >= 0)
		target[31-off] = byte(bits);
}



} // Coin::

