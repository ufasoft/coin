/*######     Copyright (c) 1997-2013 Ufasoft  http://ufasoft.com  mailto:support@ufasoft.com,  Sergey Pavlov  mailto:dev@ufasoft.com #######################################
#                                                                                                                                                                          #
# This program is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation;  #
# either version 3, or (at your option) any later version. This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the      #
# implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details. You should have received a copy of the GNU #
# General Public License along with this program; If not, see <http://www.gnu.org/licenses/>                                                                               #
##########################################################################################################################################################################*/

#include <el/ext.h>

#include "prime-util.h"

namespace Coin {

 const BigInteger PRIME_MIN = BigInteger(1) << 255,
		PRIME_MAX = (BigInteger(1) << 2000) - 1;

const BigInteger& GetPrimeMax() {
	return PRIME_MAX;
}

static const Bn BN_2(2);

BigInteger HashValueToBigInteger(const HashValue& hash) {
	byte ar[33];
	memcpy(ar, hash.data(), 32);
	ar[32] = 0;
	return BigInteger(ar, 33);
}

PrimeTester::PrimeTester()
	:	FermatTest(true)
{
}

double PrimeTester::FermatProbablePrimalityTest(const Bn& n, bool bFastFail) {
	mpz_sub_ui(m_tmp_N_Minus1.get_mpz_t(), n.get_mpz_t(), 1);
	::mpz_powm(m_tmp_R.get_mpz_t(), BN_2.get_mpz_t(), m_tmp_N_Minus1.get_mpz_t(), n.get_mpz_t());
	if (m_tmp_R == 1)
		return 1;
	if (bFastFail)
		return 0;
	m_tmp_T = (n - m_tmp_R) << 24;	
	mpz_tdiv_q(m_tmp_T.get_mpz_t(), m_tmp_T.get_mpz_t(), n.get_mpz_t());
	Int64 len = m_tmp_T.get_ui();
//	if (!(((n - r) << FRACTIONAL_BITS) / n).AsInt64(len) || len >= (1<<FRACTIONAL_BITS))
//		Throw(E_FAIL);
	return double(len) / (1 << FRACTIONAL_BITS);
}

double PrimeTester::EulerLagrangeLifchitzTest(const Bn& primePrev, bool bSophieGermain) {
	((m_tmpNext = primePrev) <<= 1) += bSophieGermain ? 1 : -1;
	unsigned mod = m_tmpNext.get_ui() % 8;
	if (bSophieGermain) {
		mpz_powm(m_tmp_R.get_mpz_t(), BN_2.get_mpz_t(), primePrev.get_mpz_t(), m_tmpNext.get_mpz_t());
	} else {
		mpz_sub_ui(m_tmp_N_Minus1.get_mpz_t(), primePrev.get_mpz_t(), 1);
		mpz_powm(m_tmp_R.get_mpz_t(), BN_2.get_mpz_t(), m_tmp_N_Minus1.get_mpz_t(), m_tmpNext.get_mpz_t());
	}
	if (mod == (bSophieGermain ? 7 : 1)) {
		if (m_tmp_R==1)
			return 1;
	} else if (mod == (bSophieGermain ? 3 : 5)) {
		mpz_add_ui(m_tmp_T.get_mpz_t(), m_tmp_R.get_mpz_t(), 1);
		if (m_tmp_T == m_tmpNext)
			return 1;
	}

	mpz_mul(m_tmp_T.get_mpz_t(), m_tmp_R.get_mpz_t(), m_tmp_R.get_mpz_t());
	mpz_tdiv_r(m_tmp_R.get_mpz_t(), m_tmp_T.get_mpz_t(), m_tmpNext.get_mpz_t()); // derive Fermat test remainder

	m_tmp_T = (m_tmpNext - m_tmp_R) << 24;	
	mpz_tdiv_q(m_tmp_T.get_mpz_t(), m_tmp_T.get_mpz_t(), m_tmpNext.get_mpz_t());
	Int64 len = m_tmp_T.get_ui();
//	if (!(((n - r) << FRACTIONAL_BITS) / n).AsInt64(len) || len >= (1<<FRACTIONAL_BITS))
//		Throw(E_FAIL);
	return double(len) / (1 << FRACTIONAL_BITS);
}

double PrimeTester::ProbableCunninghamChainTest(const Bn& n, int step) {
	double r = FermatProbablePrimalityTest(n, true);
	if (r >= 1) {
		for (Bn m(n);;) {
			double q = FermatTest ? FermatProbablePrimalityTest((m += m) += step, false)
				: EulerLagrangeLifchitzTest(m, step==1);
			r += q;
			if (q < 1)
				break;
			if (!FermatTest) {
				m = m_tmpNext;
			}
		}
	}
	return r;
}

CunninghamLengths PrimeTester::ProbablePrimeChainTest(const Bn& origin, PrimeChainType chainType) {
	CunninghamLengths r;
	Bn t(origin);
	switch (chainType) {
	case PrimeChainType::Cunningham1:
		r.Cunningham1Length = ProbableCunninghamChainTest(t -= 1, 1);
		break;
	case PrimeChainType::Cunningham2:
		r.Cunningham2Length = ProbableCunninghamChainTest(t += 1, -1);
		break;
	case PrimeChainType::Twin:
		if ((r.Cunningham1Length = ProbableCunninghamChainTest(t -= 1, 1)) >= 1) {
			r.Cunningham2Length = ProbableCunninghamChainTest(t += 2, -1);
			r.TwinLength = floor(r.Cunningham1Length) > floor(r.Cunningham2Length)
				? r.Cunningham2Length + 1 + floor(r.Cunningham2Length)
				: r.Cunningham1Length + floor(r.Cunningham1Length);
		}
		break;
	default:
		r.Cunningham1Length = ProbableCunninghamChainTest(t -= 1, 1);
		r.Cunningham2Length = ProbableCunninghamChainTest(t += 2, -1);
		r.TwinLength = floor(r.Cunningham1Length) > floor(r.Cunningham2Length)
			? r.Cunningham2Length + 1 + floor(r.Cunningham2Length)
			: r.Cunningham1Length + floor(r.Cunningham1Length);
	}	
	return r;
}



} // Coin::
