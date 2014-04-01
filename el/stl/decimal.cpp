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

static const double _M_LN10 = log(10.);

decimal64_::decimal64_(double v) {
	if (v == 0) {
		m_val = 0;
		m_exp = 0;
	} else {
		bool bNeg = v < 0;
		double exp10 = log10(bNeg ? -v : v);
		m_exp = int(floor(exp10)) - (numeric_limits<Int64>::digits10 - 1);
		m_val = (Int64)pow(10., exp10 - m_exp);
		if (bNeg)
			m_val = -m_val;	
	}
}

double decimal_to_double(const decimal64_& v) {
	return double(v.m_val) * exp (v.m_exp * _M_LN10);
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
		Int64 val = v.m_val;
		for (lldiv_t qr; nexp>0 && (qr=div(val, 10LL)).rem == 0; --nexp)
			val = qr.quot;
		Int64 d = 1;
		for (int i=0; i<nexp; ++i)
			d *= 10;
		os << val/d << use_facet<numpunct<char>>(os.getloc()).decimal_point() << setw(nexp) << setfill('0') << val % d;
	}
	return os;
}

static regex s_reDecimal("([-+])?(\\d*)(\\.(\\d*))?([eE](([-+])?(\\d+)))?");

EXT_API istream& operator>>(istream& is, decimal64_& v) {		//!!! TODO read exponent
	string s;
	is >> s;
	smatch m;
	if (regex_match(s, m, s_reDecimal)) {
		string sign = m[1].str(),
				intpart = m[2].str(),
				fppart = m[4].str();

		v = make_decimal64(stoll(sign+intpart+fppart), -(int)fppart.size());
		if (m[5].matched) {
			v.m_exp += atoi(m[6].str().c_str());
		}
	} else
		is.setstate(ios::failbit);
	return is;
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

