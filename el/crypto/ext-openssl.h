/*######     Copyright (c) 1997-2013 Ufasoft  http://ufasoft.com  mailto:support@ufasoft.com,  Sergey Pavlov  mailto:dev@ufasoft.com #######################################
#                                                                                                                                                                          #
# This program is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation;  #
# either version 3, or (at your option) any later version. This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the      #
# implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details. You should have received a copy of the GNU #
# General Public License along with this program; If not, see <http://www.gnu.org/licenses/>                                                                               #
##########################################################################################################################################################################*/

#pragma once 

#include <el/bignum.h>

#include <openssl/bn.h>

namespace Ext { namespace Crypto {
using namespace Ext;

void SslCheck(bool b);

class OpenSslMalloc {
public:
	OpenSslMalloc();
};

class OpenSslException : public CryptoExc {
	typedef CryptoExc base;
public:
	OpenSslException(HRESULT hr, RCString s)
		:	base(hr, s)
	{}
};

class BnCtx {
public:
	BnCtx() {
		SslCheck(m_ctx = ::BN_CTX_new());
	}

	~BnCtx() {
		::BN_CTX_free(m_ctx);
	}

	operator BN_CTX*() {
		return m_ctx;
	}
private:
	BN_CTX *m_ctx;

	BnCtx(const BnCtx& bnCtx);
	BnCtx& operator=(const BnCtx& bnCtx);
};

class OpensslBn {
public:
	OpensslBn() {
		SslCheck(m_bn = ::BN_new());
	}

	explicit OpensslBn(const BigInteger& bn);

	OpensslBn(const OpensslBn& bn) {
		SslCheck(::BN_dup(bn.m_bn));
	}


	~OpensslBn() {
		::BN_clear_free(m_bn);
	}

	OpensslBn& operator=(const OpensslBn& bn) {
		::BN_clear_free(m_bn);
		SslCheck(::BN_dup(bn.m_bn));
		return *this;
	}

	operator const BIGNUM *() const {
		return m_bn;
	}
	BIGNUM *Ref() { return m_bn; }	

private:
	BIGNUM *m_bn;

	explicit OpensslBn(BIGNUM *bn)
		:	m_bn(bn)
	{}
public:
	BigInteger ToBigInteger() const;
	static OpensslBn FromBinary(const ConstBuf& cbuf);
	void ToBinary(byte *p, size_t n) const;
};

BigInteger sqrt_mod(const BigInteger& x, const BigInteger& mod);

}} // Ext::Crypto::

