/*######     Copyright (c) 1997-2013 Ufasoft  http://ufasoft.com  mailto:support@ufasoft.com,  Sergey Pavlov  mailto:dev@ufasoft.com #######################################
#                                                                                                                                                                          #
# This program is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation;  #
# either version 3, or (at your option) any later version. This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the      #
# implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details. You should have received a copy of the GNU #
# General Public License along with this program; If not, see <http://www.gnu.org/licenses/>                                                                               #
##########################################################################################################################################################################*/

#include <el/ext.h>

#include "mod.h"


using namespace Ext;

namespace Ext { namespace Num {

ModNum inverse(const ModNum& mn) {
	Bn v(mn.m_val),
		mod(mn.m_mod);
#if UCFG_NUM == 'G'
	Bn r;
	int rc = mpz_invert(r.m_z.get_mpz_t(), v.m_z.get_mpz_t(), mod.m_z.get_mpz_t());
	ASSERT(rc);
	return ModNum(r.ToBigInteger(), mn.m_mod);
#else
	Bn r;
	BnCtx ctx;
	SslCheck(::BN_mod_inverse(r.Ref(), v, mod, ctx));
	return ModNum(r.ToBigInteger(), mn.m_mod);
#endif
}

ModNum pow(const ModNum& mn, const BigInteger& p) {
	Bn v(mn.m_val),
		mod(mn.m_mod);
	Bn r;
#if UCFG_NUM == 'G'
	mpz_powm(r.m_z.get_mpz_t(), v.m_z.get_mpz_t(), Bn(p).m_z.get_mpz_t(), mod.m_z.get_mpz_t());
	return ModNum(r.ToBigInteger(), mn.m_mod);
#else
	BnCtx ctx;
	SslCheck(::BN_mod_exp(r.Ref(), v, Bn(p), mod, ctx));
	return ModNum(r.ToBigInteger(), mn.m_mod);
#endif
}


}} // Ext::Num::


