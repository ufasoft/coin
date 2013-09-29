/*######     Copyright (c) 1997-2013 Ufasoft  http://ufasoft.com  mailto:support@ufasoft.com,  Sergey Pavlov  mailto:dev@ufasoft.com #######################################
#                                                                                                                                                                          #
# This program is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation;  #
# either version 3, or (at your option) any later version. This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the      #
# implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details. You should have received a copy of the GNU #
# General Public License along with this program; If not, see <http://www.gnu.org/licenses/>                                                                               #
##########################################################################################################################################################################*/

#include <el/ext.h>

#include "salsa20.h"


using namespace Ext;

extern "C" {

const byte g_salsaIndices[32][4] = {
	{ 4,  0,  12, 7 },
	{ 9,  5,  1,  7 },
	{ 14, 10, 6,  7 },
	{ 3,  15, 11, 7 },
	
	{ 8,  4,  0,  9 },
	{ 13, 9,  5,  9 },
	{ 2,  14, 10, 9 },
	{ 7,  3,  15, 9 },
	
	{ 12, 8,  4,  13 },
	{ 1,  13, 9,  13 },
	{ 6,  2,  14, 13 },
	{ 11, 7,  3,  13 },
	
	{ 0,  12, 8,  18 },
	{ 5,  1,  13, 18 },
	{ 10, 6,  2,  18 },
	{ 15, 11, 7,  18 },
 
	
	{ 1,  0,  3,  7 },
	{ 6,  5,  4,  7 },
	{ 11, 10, 9,  7 },
	{ 12, 15, 14, 7 },
	
	{ 2,  1,  0,  9 },
	{ 7,  6,  5,  9 },
	{ 8,  11, 10, 9 },
	{ 13, 12, 15, 9 },
	
	{ 3,  2,  1,  13 },
	{ 4,  7,  6,  13 },
	{ 9,  8,  11, 13 },
	{ 14, 13, 12, 13 },
	
	{ 0,  3,  2,  18 },
	{ 5,  4,  7,  18 },
	{ 10, 9,  8,  18 },
	{ 15, 14, 13, 18 },
};


} // "C"


namespace Ext { namespace Crypto {


void Salsa20Core(UInt32 dst[16], const UInt32 src[16], int rounds) {
	
	UInt32 
		w0  = dst[0]  ^= src[0],
		w5  = dst[1]  ^= src[1],
		w10 = dst[2]  ^= src[2],
		w15 = dst[3]  ^= src[3],
		w12 = dst[4]  ^= src[4],
		w1  = dst[5]  ^= src[5],
		w6  = dst[6]  ^= src[6],
		w11 = dst[7]  ^= src[7],
		w8  = dst[8]  ^= src[8],
		w13 = dst[9]  ^= src[9],
		w2  = dst[10] ^= src[10],
		w7  = dst[11] ^= src[11],
		w4  = dst[12] ^= src[12],
		w9  = dst[13] ^= src[13],
		w14 = dst[14] ^= src[14],
		w3  = dst[15] ^= src[15];

	for (int i=0; i<rounds; ++i) {
		w8  ^= _rotl((w4  ^= _rotl(w0  + w12, 7))  + w0,  9);
		w13 ^= _rotl((w9  ^= _rotl(w5  + w1,  7))  + w5,  9);
		w2  ^= _rotl((w14 ^= _rotl(w10 + w6,  7))  + w10, 9);
		w7  ^= _rotl((w3  ^= _rotl(w15 + w11, 7))  + w15, 9);

		w0  ^= _rotl((w12 ^= _rotl(w8  + w4,  13)) + w8,  18);
		w5	^= _rotl((w1  ^= _rotl(w13 + w9,  13)) + w13, 18);
		w10 ^= _rotl((w6  ^= _rotl(w2  + w14, 13)) + w2,  18);
		w15 ^= _rotl((w11 ^= _rotl(w7  + w3,  13)) + w7,  18);

		std::swap(w2, w8);
		std::swap(w7, w13);
		std::swap(w1, w4);
		std::swap(w12, w3);
		std::swap(w6, w9);
		std::swap(w11, w14);
	}

	dst[0]  += w0;
	dst[1]  += w5;
	dst[2]  += w10;
	dst[3]  += w15;
	dst[4]  += w12;
	dst[5]  += w1;
	dst[6]  += w6;
	dst[7]  += w11;
	dst[8]  += w8;
	dst[9]  += w13;
	dst[10] += w2;
	dst[11] += w7;
	dst[12] += w4;
	dst[13] += w9;
	dst[14] += w14;
	dst[15] += w3;
}

}} // Ext::Crypto::


