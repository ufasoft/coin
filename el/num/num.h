/*######     Copyright (c) 1997-2013 Ufasoft  http://ufasoft.com  mailto:support@ufasoft.com,  Sergey Pavlov  mailto:dev@ufasoft.com #######################################
#                                                                                                                                                                          #
# This program is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation;  #
# either version 3, or (at your option) any later version. This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the      #
# implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details. You should have received a copy of the GNU #
# General Public License along with this program; If not, see <http://www.gnu.org/licenses/>                                                                               #
##########################################################################################################################################################################*/

#pragma once

#ifndef UCFG_NUM
#	define UCFG_NUM 'G'
#endif

#include <el/bignum.h>

#if UCFG_NUM == 'G'
#	pragma warning(disable: 4146)
#	include <gmpxx.h>
#else
#	include <el/crypto/ext-openssl.h>
#	include <openssl/bn.h>
#endif

// Modular arithmetic


namespace Ext { namespace Num {


class Bn {
public:
	mpz_class m_z;

	Bn() {
	}

	explicit Bn(const BigInteger& bn);
public:
	BigInteger ToBigInteger() const;
	static Bn FromBinary(const ConstBuf& cbuf);
	void ToBinary(byte *p, size_t n) const;
};

}} // Ext::Num::

