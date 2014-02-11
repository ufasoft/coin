/*######     Copyright (c) 1997-2013 Ufasoft  http://ufasoft.com  mailto:support@ufasoft.com,  Sergey Pavlov  mailto:dev@ufasoft.com #######################################
#                                                                                                                                                                          #
# This program is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation;  #
# either version 3, or (at your option) any later version. This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the      #
# implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details. You should have received a copy of the GNU #
# General Public License along with this program; If not, see <http://www.gnu.org/licenses/>                                                                               #
##########################################################################################################################################################################*/

#include <el/ext.h>

#include "num.h"

#if UCFG_NUM == 'G'
#	pragma comment(lib, "gmp")
#endif

using namespace Ext;

namespace Ext { namespace Num {

Bn::Bn(const BigInteger& bn) {
	int n = (bn.Length+8)/8;
	byte *p = (byte*)alloca(n);
	bn.ToBytes(p, n);
	mpz_import(m_z.get_mpz_t(), n, -1, 1, -1, 0, p); //!!!Sgn
}

BigInteger Bn::ToBigInteger() const {
	size_t n = (mpz_sizeinbase(m_z.get_mpz_t(), 2) + 7) / 8 ;
	byte *p = (byte*)alloca(n+1);
	memset(p, 0, n+1);
	mpz_export(p, 0, -1, 1, -1, 0, m_z.get_mpz_t());				//!!!Sgn
	return BigInteger(p, n+1);
}

Bn Bn::FromBinary(const ConstBuf& cbuf) {
	Bn r;
	mpz_import(r.m_z.get_mpz_t(), cbuf.Size, 1, 1, -1, 0, cbuf.P); //!!!Sgn
	return r;
}

void Bn::ToBinary(byte *p, size_t n) const {
	size_t count = (mpz_sizeinbase(m_z.get_mpz_t(), 2) + 7) / 8;
	byte *buf = (byte*)alloca(count);
	mpz_export(buf, &count, 1, 1, -1, 0, m_z.get_mpz_t());				//!!!Sgn
	if (n < count)
		Throw(E_INVALIDARG);
	memset(p, 0, n-count);
	memcpy(p+n-count, buf, count);
}


}} // Ext::Num::


