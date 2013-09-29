/*######     Copyright (c) 1997-2013 Ufasoft  http://ufasoft.com  mailto:support@ufasoft.com,  Sergey Pavlov  mailto:dev@ufasoft.com #######################################
#                                                                                                                                                                          #
# This program is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation;  #
# either version 3, or (at your option) any later version. This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the      #
# implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details. You should have received a copy of the GNU #
# General Public License along with this program; If not, see <http://www.gnu.org/licenses/>                                                                               #
##########################################################################################################################################################################*/

#pragma once 

#include EXT_HEADER_DYNAMIC_BITSET

#if UCFG_LIB_DECLS
#	ifdef UCFG_HASH_API
#		define EXT_HASH_API DECLSPEC_DLLEXPORT
#	else
#		define EXT_HASH_API DECLSPEC_DLLIMPORT
#	endif
#else
#	define EXT_HASH_API
#endif

namespace Ext { namespace Crypto {
using namespace std;

extern "C" {
	extern const UInt32 g_sha256_hinit[8];
	extern const UInt64 g_sha512_hinit[8];
	extern const UInt32 g_sha256_k[64];	
	extern UInt32 g_4sha256_k[64][4];			// __m128i

	extern const byte g_blake_sigma[10][16];
	extern const UInt32 g_blake256_c[16];
	extern const UInt64 g_blake512_c[16];

} // "C"


__forceinline UInt32 Rotr32(UInt32 v, int n) {
	return _rotr(v, n);
}



class SHA1 : public HashAlgorithm {
public:
	hashval ComputeHash(Stream& stm) override;
	hashval ComputeHash(const ConstBuf& mb) override;
};

class SHA256 : public HashAlgorithm {
public:
	static void Init4Way(UInt32 state[8][4]);

	void InitHash(void *dst) override;
	void HashBlock(void *dst, const byte *src, UInt64 counter) override;
};

class Blake256 : public HashAlgorithm {
public:
	UInt32 Salt[4];

	Blake256() {
		IsHaifa = true;
		ZeroStruct(Salt);
	}
protected:
	void InitHash(void *dst) override;
	void HashBlock(void *dst, const byte *src, UInt64 counter) override;
};

class Blake512 : public HashAlgorithm {
public:
	UInt64 Salt[4];

	Blake512() {
		IsHaifa = true;
		Is64Bit = true;
		ZeroStruct(Salt);
	}
protected:
	void InitHash(void *dst) override;
	void HashBlock(void *dst, const byte *src, UInt64 counter) override;
};

class RIPEMD160 : public HashAlgorithm {
public:
	hashval ComputeHash(Stream& stm) override;
	hashval ComputeHash(const ConstBuf& mb) override;
};

class Aes {
public:
	Blob Key;
	Blob IV;

	static std::pair<Blob, Blob> GetKeyAndIVFromPassword(RCString password, const byte salt[8], int nRounds);		// don't set default value for nRound
	Blob Encrypt(const ConstBuf& cbuf);
	Blob Decrypt(const ConstBuf& cbuf);
};


class Random : public Ext::Random {
public:
	Random();
	void NextBytes(const Buf& mb) override;
};


Random& RandomRef();

typedef std::array<UInt32, 8> CArray8UInt32;
hashval CalcPbkdf2Hash(const UInt32 *pIhash, const ConstBuf& data, int idx);
void CalcPbkdf2Hash_80_4way(UInt32 dst[32], const UInt32 *pIhash, const ConstBuf& data);
void ShuffleForSalsa(UInt32 w[32], const UInt32 src[32]);
void UnShuffleForSalsa(UInt32 w[32], const UInt32 src[32]);
void ScryptCore(UInt32 x[32], UInt32 alignedScratch[1024*32+32]);
EXT_HASH_API CArray8UInt32 CalcSCryptHash(const ConstBuf& password);
std::array<CArray8UInt32, 3> CalcSCryptHash_80_3way(const UInt32 input[20]);

#if UCFG_CPU_X86_X64
extern "C" void _cdecl ScryptCore_x86x64(UInt32 x[32], UInt32 alignedScratch[1024*32+32]);
#endif

template <class T, class H2>
class MerkleBranch {
public:
	std::vector<T> Vec;
	int Index;
	H2 m_h2;

	MerkleBranch()
		:	Index(-1)
	{}

	T Apply(T hash) const {
		if (Index < 0)
			Throw(E_FAIL);
		for (int i=0, idx=Index; i<Vec.size(); ++i, idx >>= 1)
			hash = idx & 1 ? m_h2(Vec[i], hash) : m_h2(hash, Vec[i]);
		return hash;
	}
};

template <class T, class H2>
class MerkleTree : public vector<T> {
	typedef vector<T> base;
public:
	H2 m_h2;
	int SourceSize;

	MerkleTree()
		:	SourceSize(-1)
	{}

	template <class U, class H1>
	MerkleTree(const vector<U>& ar, H1 h1, H2 h2)
		:	base(ar.size())
		,	SourceSize(int(ar.size()))
		,	m_h2(h2)
	{
		for (int i=0; i<ar.size(); ++i)
			(*this)[i] = h1(ar[i], i);
		for (int j=0, nSize = ar.size(); nSize > 1; j += exchange(nSize, (nSize + 1) / 2))
			for (int i = 0; i < nSize; i += 2)
				base::push_back(h2((*this)[j+i], (*this)[j+std::min(i+1, nSize-1)]));
	}

	MerkleBranch<T, H2> GetBranch(int idx) {
		MerkleBranch<T, H2> r;
		r.m_h2 = m_h2;
		r.Index = idx;
		for (int j=0, n=SourceSize; n>1; j+=n, n=(n+1)/2, idx>>=1)
			r.Vec.push_back((*this)[j + std::min(idx^1, n-1)]);
		return r;
	}
};

template <class T, class U, class H1, class H2>
MerkleTree<T, H2> BuildMerkleTree(const vector<U>& ar, H1 h1, H2 h2) {
	return MerkleTree<T, H2>(ar, h1, h2);
}

class PartialMerkleTreeBase {
public:
	dynamic_bitset<byte> Bitset;
	size_t NItems;

    size_t CalcTreeWidth(int height) const {
        return (NItems + size_t(1 << height)-1) >> height;
    }

	virtual void AddHash(int height, size_t pos, const void *ar) =0;
protected:
	void TraverseAndBuild(int height, size_t pos, const void *ar, const dynamic_bitset<byte>& vMatch);
};

template <class T, class H2>
class PartialMerkleTree : public PartialMerkleTreeBase {
	typedef PartialMerkleTreeBase base;
public:
	H2 m_h2;
	vector<T> Items;

	PartialMerkleTree(H2 h2)
		:	m_h2(h2)
	{}

	T CalcHash(int height, size_t pos, const T *ar) const {
		if (0 == height)
			return ar[pos];
		T left = CalcHash(height-1, pos*2, ar),
			right = pos*2+1 < CalcTreeWidth(height-1) ? CalcHash(height-1, pos*2+1, ar) : left;
		return m_h2(left, right);
	}

	void AddHash(int height, size_t pos, const void *ar) override {
		Items.push_back(CalcHash(height, pos, (const T*)ar));
	} 

	T TraverseAndExtract(int height, size_t pos, size_t& nBitsUsed, int nHashUsed, vector<T>& vMatch) {
		if (nBitsUsed > Bitset.size())
			Throw(E_FAIL);
		bool fParentOfMatch = Bitset[nBitsUsed++];
		if (height==0 || !fParentOfMatch) {
	        if (nHashUsed >= Items.size())
				Throw(E_FAIL);
	        const T &hash = Items[nHashUsed++];
	        if (height==0 && fParentOfMatch) // in case of height 0, we have a matched txid
    	        vMatch.push_back(hash);
 	       return hash;
    	} else {
			T left = TraverseAndExtract(height-1, pos*2, nBitsUsed, nHashUsed, vMatch),
			   right = pos*2+1 < CalcTreeWidth(height-1) ? TraverseAndExtract(height-1, pos*2+1, nBitsUsed, nHashUsed, vMatch) : left;
			return m_h2(left, right);
		}
	}

};



/*!!!R
template <class T, class U, class H1, class H2>
std::vector<T> BuildMerkleTree(const vector<U>& ar, H1 h1, H2 h2) {
	std::vector<T> r(ar.size());
	std::transform(ar.begin(), ar.end(), r.begin(), h1);
    for (int j=0, nSize = ar.size(); nSize > 1; j += exchange(nSize, (nSize + 1) / 2))
        for (int i = 0; i < nSize; i += 2)
			r.push_back(h2(r[j+i], r[j+std::min(i+1, nSize-1)]));
	return r;
}
*/


}} // Ext::Crypto::

extern "C" {

#if UCFG_CPU_X86_X64
	void _cdecl Sha256Update_4way_x86x64Sse2(UInt32 state[8][4], const UInt32 data[16][4]);
	void _cdecl Sha256Update_x86x64(UInt32 state[8], const UInt32 data[16]);
	void _cdecl Blake512Round(int sigma0, int sigma1, UInt64& pa, UInt64& pb, UInt64& pc, UInt64& pd, const UInt64 m[16], const UInt64 blakeC[]);
#endif

} // "C"
