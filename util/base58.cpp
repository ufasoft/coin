/*######     Copyright (c) 1997-2013 Ufasoft  http://ufasoft.com  mailto:support@ufasoft.com,  Sergey Pavlov  mailto:dev@ufasoft.com #######################################
#                                                                                                                                                                          #
# This program is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation;  #
# either version 3, or (at your option) any later version. This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the      #
# implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details. You should have received a copy of the GNU #
# General Public License along with this program; If not, see <http://www.gnu.org/licenses/>                                                                               #
##########################################################################################################################################################################*/

#include <el/ext.h>

#include <el/bignum.h>
#include <el/crypto/hash.h>
using namespace Crypto;

#include "util.h"

namespace Coin {


HashValue160 Hash160(const ConstBuf& mb) {
	return HashValue160(RIPEMD160().ComputeHash(SHA256().ComputeHash(mb)));
}

static const char* s_pszBase58 = "123456789ABCDEFGHJKLMNPQRSTUVWXYZabcdefghijkmnopqrstuvwxyz";

String ConvertToBase58(const ConstBuf& cbuf) {
	HashValue hash = Hash(cbuf);
	Blob v = cbuf+Blob(hash.data(), 4);
	vector<char> r;

	vector<byte> tmp(v.Size+1, 0);
	std::reverse_copy(v.begin(), v.end(), tmp.begin());
	for (BigInteger n(&tmp[0], tmp.size()); Sign(n);) {
		pair<BigInteger, BigInteger> pp = div(n, 58);
		n = pp.first;
		r.insert(r.begin(), s_pszBase58[explicit_cast<int>(pp.second)]);
	}

	for (int i=0; i<v.Size && !v.constData()[i]; ++i)
		r.insert(r.begin(), s_pszBase58[0]);
	return String(&r[0], r.size());
}

Blob ConvertFromBase58(RCString s) {
	BigInteger bi = 0;
	for (const char *p=s; *p; ++p) {
		if (const char *q = strchr(s_pszBase58, *p)) {
			bi = bi*58 + BigInteger(q-s_pszBase58);
		} else
			Throw(E_INVALIDARG);
	}
	vector<byte> v((bi.Length+7)/8);
	bi.ToBytes(&v[0], v.size());
	if (v.size()>=2 && v.end()[-1]==0 && v.end()[-1]>=0x80)
		v.resize(v.size()-1);
	vector<byte> r;
	for (const char *p=s; *p==s_pszBase58[0]; ++p)
		r.push_back(0);
	r.resize(r.size()+v.size());
	std::reverse_copy(v.begin(), v.end(), r.end()-v.size());
	if (r.size() < 4)
		Throw(E_FAIL);
	HashValue hash = Hash(ConstBuf(&r[0], r.size()-4));
	if (memcmp(hash.data(), &r.end()[-4], 4))
		Throw(HRESULT_FROM_WIN32(ERROR_CRC));
	return Blob(&r[0], r.size()-4);
}


} // Coin::

