/*######     Copyright (c) 1997-2013 Ufasoft  http://ufasoft.com  mailto:support@ufasoft.com,  Sergey Pavlov  mailto:dev@ufasoft.com #######################################
#                                                                                                                                                                          #
# This program is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation;  #
# either version 3, or (at your option) any later version. This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the      #
# implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details. You should have received a copy of the GNU #
# General Public License along with this program; If not, see <http://www.gnu.org/licenses/>                                                                               #
##########################################################################################################################################################################*/

#include <el/ext.h>
#include <el/bignum.h>

#include "decimal"

namespace ExtSTL {
namespace decimal {
using namespace std;
using namespace Ext;

long long decimal_to_long_long(const decimal64& v) {
	Int64 maxPrev = numeric_limits<long long>::max()/10;
	Int64 r = v.m_val;
	if (v.m_exp >= 0) {
		for (int i=0; i<v.m_exp; ++i) {
			if (std::abs(r) > maxPrev )
				Throw(E_INVALIDARG);
			r *= 10;
		}
	} else
		for (int i=v.m_exp; i<0; ++i)
			r /= 10;
	return r;
}

decimal64 make_decimal64(Int64 val, int exp) {
	decimal64 r;
	r.m_val = val;
	r.m_exp = exp;
	return r;
}

EXT_API ostream& operator<<(ostream& os, const decimal64& v) {
	if (v.m_exp >= 0) {
		os << v.m_val << string(v.m_exp, '0');
	} else {
		int nexp = -v.m_exp;
		Int64 div = 1;
		for (int i=0; i<nexp; ++i)
			div *= 10;
		os << v.m_val/div << use_facet<numpunct<char>>(os.getloc()).decimal_point() << setw(nexp) << setfill('0') << v.m_val % div;
	}
	return os;
}

decimal64 operator*(const decimal64& a, const decimal64& b) {
	BigInteger bi = BigInteger(a.m_val) * BigInteger(b.m_val);
	int off = 0;
	while (bi.Length >= 63) {
		bi /= 10;
		++off;
	}
	return make_decimal64(explicit_cast<Int64>(bi), a.m_exp+b.m_exp+off);
}

static const UInt64 MAX_DECIMAL_M = 10000000000000000000ULL,
					MAX_DECIMAL_M_DIV_10 = MAX_DECIMAL_M/10;

NormalizedDecimal AFXAPI Normalize(const decimal64& a) {
	NormalizedDecimal r = { UInt64(a.m_val < 0 ? -a.m_val : a.m_val), a.m_exp, a.m_val < 0 };
	if (r.M == 0)
		r.Exp = INT_MIN;
	else if (r.M >= MAX_DECIMAL_M) {
		r.M /= 10;
		++r.Exp;
	} else
		for (; r.M < MAX_DECIMAL_M_DIV_10; --r.Exp)
			r.M *= 10;
	return r;
};

bool operator==(const decimal64& a, const decimal64& b) {
	NormalizedDecimal na = Normalize(a),
						nb = Normalize(b);
	return na.Minus==nb.Minus && na.M==nb.M && na.Exp==nb.Exp;
}

bool operator<(const decimal64& a, const decimal64& b) {
	NormalizedDecimal na = Normalize(a),
						nb = Normalize(b);
	if (na.Minus != nb.Minus)
		return na.Minus;
	if (na.Exp != nb.Exp)
		return (na.Exp < nb.Exp) ^ na.Minus;
	return (na.M < nb.M) ^ na.Minus;
}

} // decimal::
}  // ExtSTL::

