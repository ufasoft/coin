/*######     Copyright (c) 1997-2013 Ufasoft  http://ufasoft.com  mailto:support@ufasoft.com,  Sergey Pavlov  mailto:dev@ufasoft.com #######################################
#                                                                                                                                                                          #
# This program is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation;  #
# either version 3, or (at your option) any later version. This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the      #
# implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details. You should have received a copy of the GNU #
# General Public License along with this program; If not, see <http://www.gnu.org/licenses/>                                                                               #
##########################################################################################################################################################################*/

// (c) Ufasoft 2011 http://ufasoft.com mailto:support@ufasoft.com
// Version 2011
// This software is Public Domain

#pragma once

#include <el/crypto/hash.h>
using namespace Ext::Crypto;

#include "bitcoin-common.h"

/*!!!R
extern "C" {
	extern const UInt32 g_sha256_k[64];	
	extern const UInt32 g_sha256_hinit[8];
} // "C"
*/

namespace Coin {


template <typename T> T Expand32(UInt32 a) {
	return (T)a;
}

template<> __forceinline UInt32 Expand32<UInt32>(UInt32 a) {
	return a;
}

__forceinline UInt32 AndNot(UInt32 a, UInt32 b) {
	return ~a & b;
}

__forceinline UInt32 Rotr32(UInt32 v, int n) {
	return _rotr(v, n);
}

class MINER_CLASS BitcoinSha256 : public Object {
public:
	UInt32 m_midstate[8], m_midstate_after_3[8];
	UInt32 m_w[64], m_w1[64];
	UInt32 m_t1, m_preVal4;
 	
	virtual void PrepareData(const void *midstate, const void *data, const void *hash1);
  	virtual bool FindNonce(UInt32& nonce);
	Blob FullCalc();

  	static ptr<BitcoinSha256> CreateObject();

#ifdef _DEBUG//!!!D
	template <typename T>
	T CalcRoundsTest(T *w, const T init[8], T v[8], int startCalc, int nFrom, int n) {
		for (int j=startCalc; j<n; ++j)
			CalcW(w, j);

		T a = v[0], b = v[1], c = v[2], d = v[3], e = v[4], f = v[5], g = v[6], h = v[7];

		for (int j=nFrom; j<n; ++j) {
			T t1 = h + (Rotr32(e, 6) ^ Rotr32(e, 11) ^ Rotr32(e, 25)) + ((e & f) ^ AndNot(e, g)) + Expand32<T>(g_sha256_k[j]) + w[j];
			h = g; g = f; f = e;
			e = d+t1;
			d = c; c = b; b = a;
			a = t1 + (Rotr32(a, 2) ^ Rotr32(a, 13) ^ Rotr32(a, 22)) + ((a & c) ^ (a & d) ^ (c & d));
		}
		if (n == 64) {
			v[0] = init[0]+a; v[1] = init[1]+b, v[2] = init[2]+c; v[3] = init[3]+d, v[4] = init[4]+e; v[5] = init[5]+f, v[6] = init[6]+g; v[7] = init[7]+h;
		} else
			v[0] = a; v[1] = b, v[2] = c; v[3] = d, v[4] = e; v[5] = f, v[6] = g; v[7] = h;
		return e;	// To skip last 3 rounds
	}
#endif

	template <typename T>
	__forceinline void CalcW(T *w, int i) {
		T w_15 = w[i-15], w_2 = w[i-2];
		w[i] = w[i-16] + (Rotr32(w_15, 7) ^ Rotr32(w_15, 18) ^ (w_15 >> 3)) + w[i-7] + (Rotr32(w_2, 17) ^ Rotr32(w_2, 19) ^ (w_2 >> 10));
	}

	template <typename T>
	__forceinline T CalcRounds(T *w, const T init[8], T v[8], int startCalc, int nFrom, int n) {
		for (int j=startCalc; j<n; ++j)
			CalcW(w, j);

		T a = v[0], b = v[1], c = v[2], d = v[3], e = v[4], f = v[5], g = v[6], h = v[7];

		for (int j=nFrom; j<n; ++j) {
//!!!			T t2 = (Rotr32(a, 2) ^ Rotr32(a, 13) ^ Rotr32(a, 22)) + ((a & b) ^ (a & c) ^ (b & c));
			T t1 = h + (Rotr32(e, 6) ^ Rotr32(e, 11) ^ Rotr32(e, 25)) + ((e & f) ^ AndNot(e, g)) + Expand32<T>(::g_sha256_k[j]) + w[j];
			h = g; g = f; f = e;
			e = d+t1;
			d = c; c = b; b = a;
			a = t1 + (Rotr32(a, 2) ^ Rotr32(a, 13) ^ Rotr32(a, 22)) + ((a & c) ^ (a & d) ^ (c & d));
		}
		if (n == 64) {
			v[0] = init[0]+a; v[1] = init[1]+b, v[2] = init[2]+c; v[3] = init[3]+d, v[4] = init[4]+e; v[5] = init[5]+f, v[6] = init[6]+g; v[7] = init[7]+h;
		} else if (n == 3) {
			v[0] = a; v[1] = b, v[2] = c; v[3] = d, v[4] = e; v[5] = f, v[6] = g; v[7] = h;
		}
		return e;	// To skip last 3 rounds
	}
};

} // Coin::

