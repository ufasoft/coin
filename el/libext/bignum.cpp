/*######     Copyright (c) 1997-2013 Ufasoft  http://ufasoft.com  mailto:support@ufasoft.com,  Sergey Pavlov  mailto:dev@ufasoft.com #######################################
#                                                                                                                                                                          #
# This program is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation;  #
# either version 3, or (at your option) any later version. This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the      #
# implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details. You should have received a copy of the GNU #
# General Public License along with this program; If not, see <http://www.gnu.org/licenses/>                                                                               #
##########################################################################################################################################################################*/

#include <el/ext.h>

using namespace std;
using namespace rel_ops;

#if UCFG_BIGNUM=='G'
#	pragma comment(lib, EXT_GMP_LIB)
#endif

#include <el/bignum.h>

namespace Ext {

BigInteger::BigInteger()
#if UCFG_BIGNUM=='A'
	:	m_blob(nullptr)
	,	m_count(1)
#else
	:	m_zz(0)
#endif
{
#if UCFG_BIGNUM=='A'
	m_data[0] = 0;
#endif
}

BigInteger::BigInteger(UInt32 n)
#if UCFG_BIGNUM=='A'
	:	m_blob(nullptr)
#endif
{
#if UCFG_BIGNUM=='A'
	if (n >= 0) {
		m_count = 1;
		m_data[0] = n;
		return;
	}
#endif
	Int64 v = n;
	Init((byte*)&v, sizeof(v));			//!!!LE
}

BigInteger::BigInteger(Int64 n)
#if UCFG_BIGNUM=='N'
	:	m_zz(ZZFromSignedBytes((byte*)&n, sizeof(n)))
#elif UCFG_BIGNUM=='A'
	:	m_blob(nullptr)
#endif
{
#if UCFG_BIGNUM=='A' && INTPTR_MAX > 0x7fffffff
	m_count = 1;
	m_data[0] = n;
#elif UCFG_BIGNUM!='N'
	Init((const byte*)&n, sizeof n);			//!!! Only Little-Endian
#endif
}


#if UCFG_BIGNUM!='A'

BigInteger::~BigInteger() {
}

BigInteger::BigInteger(const BigInteger& bi)					// here to hide GMP/NTL API from users
	:	m_zz(bi.m_zz)
{
}

BigInteger& BigInteger::operator=(const BigInteger& bi) {
	m_zz = bi.m_zz;
	return _self;
}

size_t BigInteger::GetBaseWords() const {
	size_t n = ToBytes(0, 0);
	return (n + sizeof(BASEWORD)-1)/sizeof(BASEWORD);
}


#endif

#if UCFG_BIGNUM=='G'
BigInteger::BigInteger(int n)
	:	m_zz(n)
{
}

void BigInteger::Init(const byte *p, size_t count) {
	if (p[count-1] < 0x80)
		::mpz_import(m_zz.get_mpz_t(), count, -1, 1, -1, 0, p);
	else {
		byte *q = (byte*)alloca(count);
		for (int i=0; i<count; ++i)
			q[i] = ~p[i];
		::mpz_import(m_zz.get_mpz_t(), count, -1, 1, -1, 0, q);
		m_zz = -m_zz - 1;
	}
}

#endif


#if UCFG_BIGNUM=='N'

using namespace NTL;

BigInteger::BigInteger(int n)
	:	m_zz(NTL::INIT_VAL, n)
{
}



static ZZ ZZFromSignedBytes(const byte *p, size_t n) {
	if ((p[n-1] & 0x80) == 0)
		return ZZFromBytes(p, n);
	byte *buf = (byte*)alloca(n);
	for (int i=0; i<n; ++i)
		buf[i] = ~p[i];
	return -(ZZFromBytes(buf, n)+1);
}

#	if LONG_MAX==0x7fffffff
BigInteger::BigInteger(long n)
	:	m_zz(ZZFromSignedBytes((byte*)&n, sizeof(n)))
{
}
#	endif



BigInteger::BigInteger(const NTL::ZZ& zz)
	:	m_zz(zz)
{
}

#endif

BigInteger::BigInteger(RCString s, int bas)
#if UCFG_BIGNUM=='A'
	:	m_blob(nullptr)
#endif
{
#if UCFG_BIGNUM=='A'
	byte zero = 0;
	Init(&zero, 1);
#endif

	if (s.Length == 0)
		Throw(E_FAIL);
	bool bMinus = false;
	size_t i=0;
	if (s[0] == '-') {
		bMinus = true;
		++i;
		if (s.Length == 1)
			Throw(E_FAIL);
	}	
	for (; i<s.Length; ++i) {
		wchar_t ch = s[i];
		int n;
		if (ch>='0' && ch <='9')
			n = ch-'0';
		else if (ch>='A' && ch <='Z')
			n = ch-'A'+10;
		else if (ch>='a' && ch <='z')
			n = ch-'a'+10;
		else
			Throw(E_FAIL);
		if (n >= bas)
			Throw(E_FAIL);
		_self = _self*bas+n;
	}
	if (bMinus)
		_self = -_self;
}

BigInteger::BigInteger(const byte *p, size_t count)
#if UCFG_BIGNUM=='N'
	:	m_zz(ZZFromSignedBytes(p, count))
#elif UCFG_BIGNUM=='A'
	:	m_blob(nullptr)
#endif
{
#if UCFG_BIGNUM!='N'
	Init(p, count);
#endif
}

int AFXAPI Sign(const BigInteger& v) {
#if UCFG_BIGNUM=='G'
	return sgn(v.m_zz);
#elif UCFG_BIGNUM=='N'
	return sign(v.m_zz);
#else
	if (v.m_count==1 && !v.m_data[0])
		return 0;
	if (*((byte*)v.get_Data()+v.m_count*sizeof(BASEWORD)-1) & 0x80)
		return -1;
	return 1;
#endif
}

bool BigInteger::operator!() const {
#if UCFG_BIGNUM!='A'
		return m_zz == 0;
#else
		return m_count==1 && !m_data[0];
#endif
}

static const char s_radixChars[37] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";

String BigInteger::ToString(int bas) const {
	if (bas < 2 || bas > 36)
		Throw(E_INVALIDARG);
	size_t size = Length+2;
	String::Char *e = (String::Char*)alloca(size*sizeof(String::Char)) + size,									// to avoid char -> String::Char conversion
			*q = e;
	bool bMinusP = Sign(_self)<0;
	BigInteger v = bMinusP ? -_self : _self,
		   divider(bas);
	do {
		pair<BigInteger, BigInteger> qr = div(v, divider);
		*--q = s_radixChars[explicit_cast<int>(qr.second)];
		v = qr.first;
	} while (Sign(v) != 0);
	if (bMinusP)
		*--q = '-';
	return String(q, e-q);
}

static int GetBas(ostream& os) {
	switch (os.flags() & ios::basefield) {
	case ios::oct: return 8;
	case ios::hex: return 16;
	default: return 10;
	}
}

ostream& AFXAPI operator<<(ostream& os, const BigInteger& n) {
	return os << n.ToString(GetBas(os));
}

#if UCFG_BIGNUM=='G'
size_t NumBits(const mpz_class& z) {
	return ::mpz_sizeinbase(z.get_mpz_t(), 2);
}

size_t NumBytes(const mpz_class& z) {
	size_t bits = ::mpz_sizeinbase(z.get_mpz_t(), 2);
	if (sgn(z) >= 0) {												//!!!?
		return bits/8 + 1;
	} else {
		return (bits+7)/8;
	}
}
#endif //  UCFG_BIGNUM=='G'

bool BigInteger::AsInt64(Int64& n) const {
#if UCFG_BIGNUM!='A'
	int count = NumBytes(m_zz);
	if (count > sizeof(Int64))
		return false;
	ToBytes((byte*)&n, sizeof(Int64));
	return true;
#else
	if (m_count > 64/BASEWORD_BITS)
		return false;
	memcpy(&n, Data, m_count*sizeof(BASEWORD));
	if (m_count < 64/BASEWORD_BITS)
		n = Int64(((UInt64)n) << 32) >> 32;
	return true;
#endif
}

bool BigInteger::operator==(const BigInteger& v) const {
#if UCFG_BIGNUM!='A'
	return m_zz == v.m_zz;
#else
	return m_count==v.m_count && !memcmp(Data, v.Data, m_count*sizeof(BASEWORD));
#endif
}

BigInteger BigInteger::operator-() const {
#if UCFG_BIGNUM!='A'
	BigInteger r;
	r.m_zz = -m_zz;
	return r;
#else
	BASEWORD *p = (BASEWORD*)alloca((m_count+1)*sizeof(BASEWORD));
	ImpNegBignum(Data, m_count, p);
	return BigInteger(p, m_count+1);
#endif
}

BigInteger BigInteger::Mul(const BigInteger& y) const {
#if UCFG_BIGNUM!='A'
	BigInteger r;
	r.m_zz = m_zz * y.m_zz;
	return r;
#else
	bool signX = Sign(_self) == -1,
		signY = Sign(y) == -1;
	BASEWORD *px = (BASEWORD*)Data,
		*py = (BASEWORD*)y.Data;
	if (signX)
		ImpNegBignum(Data, m_count, px = (BASEWORD*)alloca((m_count+1)*sizeof(BASEWORD)));
	if (signY)
		ImpNegBignum(y.Data, y.m_count, py = (BASEWORD*)alloca((y.m_count+1)*sizeof(BASEWORD)));
	size_t size = m_count + y.m_count;
	BASEWORD *p = (BASEWORD*)alloca((size+1)*sizeof(BASEWORD));
	ImpMulBignums(px, m_count, py, y.m_count, p);
	if (signX^signY)
		ImpNegBignum(p, size, p);
	return BigInteger(p, size);
#endif
}

unsigned int BigInteger::NMod(unsigned int d) const {
	if (Sign(_self) < 0)
		Throw(E_INVALIDARG);
#if UCFG_BIGNUM=='G'
	return ::mpz_fdiv_ui(m_zz.get_mpz_t(), d);
#elif UCFG_BIGNUM=='N'
	return m_zz % d;
#else
	return (unsigned int)ImpShortDiv(ExtendTo((BASEWORD*)alloca((m_count+1)*sizeof(BASEWORD)), m_count+1), (BASEWORD*)alloca(m_count*sizeof(BASEWORD)), m_count, d);
#endif
}

pair<BigInteger, BigInteger> AFXAPI div(const BigInteger& x, const BigInteger& y) {
	if (!y)
		Throw(E_EXT_DivideByZero);
#if UCFG_BIGNUM!='A'
	pair<BigInteger, BigInteger> r;
	r.first.m_zz = x.m_zz/y.m_zz;
	r.second.m_zz = x.m_zz%y.m_zz;
	return r;
#else
	if (x.m_count==1 && y.m_count==1 && *x.m_data != BASEWORD_MIN) {
		S_BASEWORD num = *x.m_data,
			den = *y.m_data;
		return pair<BigInteger, BigInteger>(num/den, num%den);
	}
	BigInteger nx(x),
		ny(y);
	bool signX = Sign(nx) == -1,
		signY = Sign(ny) == -1;
	if (signX)
		nx = -nx;
	if (signY)
		ny = -ny;
	BigInteger q, r;
	if (ny.m_count == 1) {
		BASEWORD v = *ny.m_data;
		size_t size = nx.m_count;
		BASEWORD *pq = (BASEWORD*)alloca(size*sizeof(BASEWORD));
#if UCFG_BIGNUM!='A'
		const BASEWORD *px = nx.get_Data();
		BASEWORD rem = 0;
		for (int i=size; i--;) {
			D_BASEWORD u = (D_BASEWORD(rem)<<BASEWORD_BITS) | px[i];
			pq[i] = BASEWORD(u / v);
			rem = u % v;
		}
#else
		BASEWORD *px = (BASEWORD*)alloca((size+1)*sizeof(BASEWORD));
		nx.ExtendTo(px, size+1);
		BASEWORD rem = ImpShortDiv(px, pq, size, v);
#endif
		q = BigInteger(pq, size);
		r = BigInteger(&rem, 1);
	} else if (nx < ny) {
		q = 0;
		r = nx;
	} else {
		int len = ny.Length;
		int offset = (-len) & (BASEWORD_BITS-1);		// Normalize
		nx <<= offset;
		ny <<= offset;
		int n = ny.m_count-1;
		int m = ((nx.Length+BASEWORD_BITS-1) / BASEWORD_BITS)-n;
		const BASEWORD *pv = ny.get_Data();
		BASEWORD v_n_1 = pv[n-1],
			     v_n_2 = pv[n-2];
		int size = m+n+1;
		BASEWORD *pu = (BASEWORD*)alloca(size*sizeof(BASEWORD)),
		         *pq = (BASEWORD*)alloca((m+2)*sizeof(BASEWORD));
		pq[m+1] = 0;
		nx.ExtendTo(pu, size);
		for (int j=m; j>=0; --j) {
			BASEWORD qh;
			BASEWORD rh;

			if (pu[j+n] >= v_n_1) {
				qh = BASEWORD(S_BASEWORD(-1)); 			// all ones
				D_BASEWORD rr = D_BASEWORD(pu[j+n-1])+v_n_1;
				if (rr >= D_BASEWORD(1)<<BASEWORD_BITS)
					goto LAB_Q_FOUND;
				rh = int_cast<BASEWORD>(rr);
			} else {
				BASEWORD u[2] = { pu[j+n-1], pu[j+n] };
				rh = ImpShortDiv(u, &qh, 1, v_n_1);
			}
			while (D_BASEWORD(qh)*v_n_2 > ((D_BASEWORD(rh)<<BASEWORD_BITS) | pu[j+n-2])) {
				--qh;
				D_BASEWORD rr = D_BASEWORD(rh)+v_n_1;
				if (rr >= D_BASEWORD(1)<<BASEWORD_BITS)
					break;
				rh = int_cast<BASEWORD>(rr);
			}
LAB_Q_FOUND:
			if (ImpMulSubBignums(pu+j, pv, n, qh)) {
				--qh;										// Test Case: 0x7fffffff800000010000000000000000 / 0x800000008000000200000005
				ImpAddBignumsEx(pu+j, pv, n);
			}
			pq[j] = qh;
		}
		q = BigInteger(pq, m+2);

		pu[n] = 0;
		r = BigInteger(pu, n+1)>>offset;


		/*!!!R
		size_t size = nx.m_count+ny.m_count+1;
		BASEWORD *px = (BASEWORD*)alloca(size*sizeof(BASEWORD)),
			*py = (BASEWORD*)alloca(size*sizeof(BASEWORD)),
			*pr = (BASEWORD*)alloca(size*sizeof(BASEWORD));
		nx.ExtendTo(px, size);
		ny.ExtendTo(py, size);
		ImpDivBignums(px, py, pr, size);
		q = BigInteger(px, size);
		r = BigInteger(pr, size);
		*/
	}
	if (signX ^ signY)
		q = -q;
	if (signX)
		r = -r;
	return make_pair(q, r);
#endif
}

size_t BigInteger::get_Length() const {
#if UCFG_BIGNUM!='A'
	return NumBits(m_zz);
#else
	S_BASEWORD w = get_Data()[m_count-1];
	return (m_count-1)*BASEWORD_BITS+1+ImpBSR(w ^ (w>>(BASEWORD_BITS-1)));
#endif
}

bool BigInteger::TestBit(size_t idx) {
#if UCFG_BIGNUM=='G'
	return ::mpz_tstbit(m_zz.get_mpz_t(), idx);
#elif UCFG_BIGNUM=='N'
	return bit(m_zz, idx);
#else
	return BitOps::BitTest(Data, min(idx, m_count*BASEWORD_BITS-1));
#endif
}

size_t BigInteger::ToBytes(byte *p, size_t size) const {
#if UCFG_BIGNUM=='G'
	mpz_class v = sgn(m_zz)==-1 ? m_zz+1 : m_zz;
	size_t r = NumBytes(v);
	if (p) {
		memset(p, 0, size);
		mpz_export(p, 0, -1, 1, -1, 0, v.get_mpz_t());
		if (sgn(m_zz) == -1)
			for (int i=0; i<size; ++i)
				p[i] = ~p[i];
	}
	return r;
#elif UCFG_BIGNUM=='N'
	ZZ v = sign(m_zz)==-1 ? m_zz+1 : m_zz;
	size_t r = NumBytes(v);
	if (p) {
		memset(p, 0, size);
		BytesFromZZ(p, v, size);
		if (sign(m_zz)==-1)
			for (int i=0; i<size; ++i)
				p[i] = ~p[i];
	}
	return r;
#else
	size_t r = DWORD((Length+8)/8);
	if (p)
		memcpy(p, Data, std::min(size, r));
	return r;
#endif
}

Blob BigInteger::ToBytes() const {
#if UCFG_BIGNUM!='A'
	Blob r(0, ToBytes(0, 0));
	ToBytes(r.data(), r.Size);
#else
	Blob r(0, DWORD((Length+8)/8));
	memcpy(r.data(), Data, r.Size);
#endif
	return r;
}

BinaryWriter& AFXAPI operator<<(BinaryWriter& wr, const BigInteger& n) {
	DWORD nbytes = DWORD((n.Length+8)/8);
#if UCFG_BIGNUM!='A'
	byte *p = (byte*)alloca(nbytes);
	n.ToBytes(p, nbytes);
#else
	const BASEWORD *p = n.Data;
#endif
	wr.WriteSize(nbytes);
	wr.Write(p, nbytes);
	return wr;
}

const BinaryReader& AFXAPI operator>>(const BinaryReader& rd, BigInteger& n) {
	size_t nbytes = rd.ReadSize();
	byte *p = (byte*)alloca(nbytes);
	rd.Read(p, nbytes);
	n = BigInteger(p, nbytes);
	return rd;
}

istream& AFXAPI operator>>(istream& is, BigInteger& n) {
	n = 0;
	bool bMinus = false;
	int ch = is.get();
	if (bMinus = (ch == '-'))
		ch = is.get();
	do {
		if (!isdigit(ch)) {
			is.putback((char)ch);
			is.clear(ios::failbit);
			return is;
		}
		n = n*10+(ch-'0');
		ch = is.get();
	} while (is && isdigit(ch));
	if (bMinus)
		n = -n;
	if (ch != EOF)
		is.putback((char)ch);
	is.clear();
	return is;
}

bool BigInteger::AsBaseWord(S_BASEWORD& n) const {
#if UCFG_BIGNUM!='A'
	int count = NumBytes(m_zz);
	if (count > sizeof(BASEWORD))
		return false;
	ToBytes((byte*)&n, sizeof(S_BASEWORD));
	return true;
#else
	if (m_count > 1)
		return false;
	S_BASEWORD esign = *(S_BASEWORD*)m_data >> (BASEWORD_BITS-1);
	for (int i=1; i<_countof(m_data); i++)
		if (m_data[i] != esign)
			return false;
	n = m_data[0];
	return true;
#endif
}

double BigInteger::AsDouble() const {
	int len = Length;
	if (len < DBL_MANT_DIG) {
		Int64 i;
		if (!AsInt64(i))
			Throw(E_FAIL);
		return (double)i;
	} else {
		int exp = len-DBL_MANT_DIG;
		BigInteger n = _self >> exp;
		Int64 i;
		if (!n.AsInt64(i))
			Throw(E_FAIL);
		return ::ldexp((double)i, exp);
	}
}

BigInteger BinaryBignums(PFNImpBinary pfn, const BigInteger& x, const BigInteger& y, int incsize) {
	size_t size = std::max(x.GetBaseWords(), y.GetBaseWords());
	BASEWORD *px = (BASEWORD*)alloca(size*sizeof(BASEWORD)),
		*py = (BASEWORD*)alloca(size*sizeof(BASEWORD)),
		*pr = (BASEWORD*)alloca((size+incsize)*sizeof(BASEWORD));
	x.ExtendTo(px, size);
	y.ExtendTo(py, size);
	pfn(px, py, pr, size);
#if UCFG_BIGNUM!='A'
	return BigInteger((const byte*)pr, (size+incsize)*sizeof(BASEWORD));
#else
	return BigInteger(pr, size+incsize);
#endif
}

int BigInteger::Compare(const BigInteger& x) const {
#if UCFG_BIGNUM=='G'
	return cmp(m_zz, x.m_zz);
#elif UCFG_BIGNUM=='N'
	return NTL::compare(m_zz, x.m_zz);
#else
	if (m_count != x.m_count) {
		int signMe = Sign(_self),
			signX = Sign(x);
		if (signMe != signX)
			return signMe - signX;
		ASSERT(signMe != 0 );
		return (m_count - x.m_count) * signMe;
	} else {
		const BASEWORD *p = Data,
			           *px = x.Data;
		for (size_t i=m_count; i--;)
			if (p[i] != px[i])
				return (i == m_count-1 ? (S_BASEWORD)p[i] < (S_BASEWORD)px[i] : p[i] < px[i]) ? -1 : 1;
		return 0;
	}
#endif
}

BigInteger BigInteger::Add(const BigInteger& x) const {
#if UCFG_BIGNUM!='A'
	return BigInteger(m_zz + x.m_zz);
#else
	return BinaryBignums(ImpAddBignums, _self, x, 1);
#endif
}

BigInteger BigInteger::Sub(const BigInteger& x) const {
#if UCFG_BIGNUM!='A'
	return BigInteger(m_zz - x.m_zz);
#else
	return BinaryBignums(ImpSubBignums, _self, x, 1);
#endif
}

BigInteger BigInteger::operator~() const {
#if UCFG_BIGNUM!='A'
	size_t count = std::max<size_t>(GetBaseWords(), 1);
	BASEWORD *p = (BASEWORD*)alloca(count*sizeof(BASEWORD));
	ExtendTo(p, count);
	for (int i=0; i<count; ++i)
		p[i] = ~p[i];
	return BigInteger((const byte*)p, (count)*sizeof(BASEWORD));
#else
	BASEWORD *p = (BASEWORD*)alloca(m_count*sizeof(BASEWORD));
	for (size_t i=0; i<m_count; i++)
		p[i] = ~get_Data()[i];
	return BigInteger(p, m_count);
#endif
}

BigInteger BigInteger::And(const BigInteger& x) const {
	return BinaryBignums(ImpAndBignums, _self, x);
}

BigInteger BigInteger::Or(const BigInteger& x) const {
	return BinaryBignums(ImpOrBignums, _self, x);
}

BigInteger BigInteger::Xor(const BigInteger& x) const {
	return BinaryBignums(ImpXorBignums, _self, x);
}

BigInteger AFXAPI operator>>(const BigInteger& x, size_t v) {
	size_t len = x.Length;
#if UCFG_BIGNUM!='A'
	int sgn = Sign(x);
	if (v < len) {
		DWORD nbytes = DWORD((len+8)/8+1);
		byte *p = (byte*)alloca(nbytes);
		x.ToBytes(p, nbytes);
		int rn = std::max(1, int((len-v+7)/8));
		int off = v & 7;
		int byteoff = v/8;
		for (int i=0; i<rn; ++i)
			p[i] = (p[i+byteoff] >> off) | byte(p[i+byteoff+1] << (8-off));
		return BigInteger(p, rn);
	} else
		return sgn==-1 ? -1 : 0;
#else
	size_t size = max(int(len)-int(v)+int(BASEWORD_BITS), int(BASEWORD_BITS))/BASEWORD_BITS;
	BASEWORD *r = (BASEWORD*)alloca(size*sizeof(BASEWORD));
	BASEWORD prev = Sign(x)==-1 ? -1 : 0;
	if (v < len) {
		const BASEWORD *p = x.Data+len/BASEWORD_BITS;
		int off = int(v & (BASEWORD_BITS-1));
		if (off > int(len & (BASEWORD_BITS-1)))
			prev = *p--;
		ImpShrd(p-size+1, r, size, off, prev);

		/*!!!
		for (size_t i=size; i--;)
		{
			BASEWORD w = *p--;
			r[i] = BASEWORD(((DWORDLONG(exchange(prev, w)) << BASEWORD_BITS) | w) >> off);
		}*/
	} else
		*r = prev;
	return BigInteger(r, size);
#endif
}

BigInteger AFXAPI operator<<(const BigInteger& x, size_t v) {
#if UCFG_BIGNUM!='A'
	return BigInteger(x.m_zz << (long)v);
#else
	size_t size = v/BASEWORD_BITS+1+x.m_count;
	BASEWORD *r = (BASEWORD*)alloca(size*sizeof(BASEWORD));
	memset(r, 0, size*sizeof(BASEWORD));
	BASEWORD *p = r+v/BASEWORD_BITS;
	ImpShld(x.Data, p, x.m_count, v & (BASEWORD_BITS-1));
	return BigInteger(r, size);
#endif
}

BigInteger BigInteger::Random(const BigInteger& maxValue, Ext::Random *random) {
	auto_ptr<Ext::Random> r;
	if (!random) {
		r.reset(new Ext::Random);
		random = r.get();
	}
	size_t nbytes = maxValue.Length/8+1;
	byte *p = (byte*)alloca(nbytes);
	random->NextBytes(Buf(p, nbytes-1));
#if UCFG_BIGNUM!='A'
	byte *ar = (byte*)alloca(nbytes);
	maxValue.ToBytes(ar, nbytes);
	byte hiByte = ar[nbytes-1];	
#else
	byte hiByte = ((byte*)maxValue.get_Data())[nbytes-1];
#endif
	if (hiByte)
		p[nbytes-1] = byte(random->Next(hiByte));
	else
		p[nbytes-1] = 0;
	return BigInteger(p, nbytes);
}

BASEWORD *BigInteger::ExtendTo(BASEWORD *p, size_t size) const {
	size_t count = GetBaseWords();
	if (count > size)
		Throw(E_InvalidExtendOfNumber);
#if UCFG_BIGNUM!='A'
	ToBytes((byte*)p, size*sizeof(BASEWORD));
#else
	memcpy(p, Data, count*sizeof(BASEWORD));
	if (count != size)
		memset(p+count, Sign(_self)==-1 ? -1 : 0, (size-count)*sizeof(BASEWORD));
#endif
	return p;
}

/*!!!R
void ImpMulBignums(const BASEWORD *a, size_t m, const BASEWORD *b, size_t n, BASEWORD *c)
{
	memset(c, 0, (m+n)*sizeof(BASEWORD));
	for (int j=0; j<n; j++)
	{
		DWORD k = 0;
		for (int i=0; i<m; i++)
		{
			DWORDLONG t = DWORDLONG(a[i])*DWORDLONG(b[j])+c[i+j]+k;
			c[i+j] = DWORD(t);
			k = DWORD(t>>32);
		}
		c[j+m] = k;
	}
}
*/


#if UCFG_BIGNUM=='A'




#if (!defined(_MSC_VER) || !defined(_M_IX86)) && !defined(_M_X64)

void ImpShld(const BASEWORD *s, BASEWORD *d, size_t siz, size_t amt) {
	int off = BASEWORD_BITS-amt;
	BASEWORD prev = 0;
	for (int i=0; i<siz; i++) {
		BASEWORD w = s[i];
		*d++ = BASEWORD(((Ext::UInt64(w) << BASEWORD_BITS) | exchange(prev, w)) >> off);
	}
	*d = S_BASEWORD(prev) >> min(off, int(BASEWORD_BITS-1));
}

#endif



/*!!!
Int64 BigInteger::ToInt64() const
{
	if (_self < BigInteger(LLONG_MIN) || _self > BigInteger(LLONG_MAX))
		Throw(E_EXT_BigNumConversion);
	BigInteger v = _self;
	v.Extend(2);
	return *(Int64*)v.m_p;
}
*/

/*!!!
BigInteger& BigInteger::operator=(const BigInteger& v)
{
	this->~BigInteger();
	m_count = v.m_count;
	if (v.m_p)
	{
    m_p = new DWORD[m_count];
    memcpy(m_p, v.m_p, m_count*4);
	}
	else
	{
		m_p = 0;
		*m_data = *v.m_data; //!!! memcpy(this, &v, sizeof(BigInteger));
	}
	return _self;
}*/

void BigInteger::Init(const byte *p, size_t count) {
	byte bhigh = (signed char)p[count-1] >> 7;
	size_t n = count;
	for (; n>0 && p[n-1]==bhigh; n--)
		;
	if (!n || byte(((signed char)p[n-1]>>7))!=bhigh)
		n++;
	BASEWORD *data;
	if ((m_count=(n+sizeof(BASEWORD)-1)/sizeof(BASEWORD)) > _countof(m_data)) {
		size_t size = m_count*sizeof(BASEWORD);
		m_blob.m_pData = new(size) CStringBlobBuf(0, size);
		data = (BASEWORD*)m_blob.data();
	} else
		data = m_data;
	data[m_count-1] = (S_BASEWORD)(signed char)bhigh;
	memcpy(data, p, n);

/*!!!
	if (count != 1)
	{
		int i;
		for (i=count-1; i>0; i--)
		{
			DWORD high = p[i],
						prev = p[i-1];
			if ((high || (prev & 0x80000000)) && (high!=0xFFFFFFFF || !(prev & 0x80000000)))
				break;
		}
		count = i!=count-1 ? i+1 : count;
	}
	if ((m_count=count) > _countof(m_data))
	{
		size_t size = m_count*sizeof(BASEWORD);
		m_blob.m_pData = new(size) CStringBlobBuf(p, size);
	}
	else
		*m_data = *p;
		*/
}


#endif // UCFG_BIGNUM=='A'

BigInteger AFXAPI sqrt(const BigInteger& bi) {
	if (Sign(bi) < 0)
		Throw(E_INVALIDARG);
	for (BigInteger x=(BigInteger(1) << (bi.Length/2)); ;) {													// Newton method
		BigInteger d = bi / x;
		bool bRet = abs(x - d) < 2;
		x = (x+d) >> 1;
		if (bRet)
			return x;
	}
}

BigInteger pow(const BigInteger& x, int n) {
	if (n < 0)
		Throw(E_INVALIDARG);
	if (n == 0)
		return 1;
	BigInteger r = x;
	while (--n)
		r *= x;
	return r;
}

} // Ext::

void __stdcall ImpAndBignums(const BASEWORD *a, const BASEWORD *b, BASEWORD *c, size_t siz) {
	while (siz--)
		*c++ = *a++ & *b++;
}

void __stdcall ImpOrBignums(const BASEWORD *a, const BASEWORD *b, BASEWORD *c, size_t siz) {
	while (siz--)
		*c++ = *a++ | *b++;
}

void __stdcall ImpXorBignums(const BASEWORD *a, const BASEWORD *b, BASEWORD *c, size_t siz) {
	while (siz--)
		*c++ = *a++ ^ *b++;
}


