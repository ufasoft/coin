/*######   Copyright (c) 2011-2019 Ufasoft  http://ufasoft.com  mailto:support@ufasoft.com,  Sergey Pavlov  mailto:dev@ufasoft.com ####
#                                                                                                                                     #
# 		See LICENSE for licensing information                                                                                         #
#####################################################################################################################################*/

#include <el/ext.h>
#include <el/bignum.h>

#include <el/crypto/hash.h>
#include <el/crypto/cipher.h>
using namespace Crypto;

#include "util.h"
#include "opcode.h"

#pragma warning(disable: 4146 4244)	// for MPIR

#if UCFG_COIN_ECC=='S'
#	include <crypto/cryp/secp256k1.h>
#else
#	include <el/crypto/ext-openssl.h>
#endif


namespace Coin {

vararray<uint8_t, 25> KeyInfoBase::ToPubScript() const {
	HashValue160 hash160 = PubKey.Hash160;
	uint8_t script[25] = { (uint8_t)Opcode::OP_DUP, (uint8_t)Opcode::OP_HASH160, 20,
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		(uint8_t)Opcode::OP_EQUALVERIFY, (uint8_t)Opcode::OP_CHECKSIG
	};
	memcpy(script + 3, hash160.data(), 20);
	return vararray<uint8_t, 25>(script, sizeof(script));
}

Address KeyInfoBase::ToAddress() const {
	HashValue160 hash160 = PubKey.Hash160;
	if (AddressType == AddressType::P2SH) {
		hash160 = Hash160(ToPubScript());
	}
	return Address(*HasherEng::GetCurrent(), AddressType, hash160, 0, Comment);
}

Blob KeyInfoBase::PlainPrivKey() const {
	uint8_t typ = 0;
	return Blob(&typ, 1) + get_PrivKey();
}

void KeyInfoBase::SetPrivData(RCSpan cbuf, bool bCompressed) {
	ASSERT(cbuf.size() <= 32);
	m_privKey = cbuf.size() == 32 ? cbuf : Span(Blob(0, 32 - cbuf.size()) + cbuf);

#if UCFG_COIN_ECC=='S'
	Blob pubKey = Span(Sec256Dsa::PrivKeyToPubKey(m_privKey, bCompressed));
#else
	Key = CngKey::Import(m_privKey, CngKeyBlobFormat::OSslEccPrivateBignum);
	Blob pubKey = Key.Export(bCompressed ? CngKeyBlobFormat::OSslEccPublicCompressedBlob : CngKeyBlobFormat::OSslEccPublicBlob);
#endif
	if (!PubKey.Data.empty() && !Equal(Span(PubKey.Data), pubKey))
		Throw(E_FAIL);
	PubKey = CanonicalPubKey(pubKey);
}

static const uint8_t s_maxModOrder[32] ={ 0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF, 0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFE, 0xBA,0xAE,0xDC,0xE6,0xAF,0x48,0xA0,0x3B, 0xBF,0xD2,0x5E,0x8C,0xD0,0x36,0x41,0x40 };

void KeyInfoBase::GenRandom(bool bCompressed) {
	Comment = nullptr;
#if UCFG_COIN_ECC=='S'
	Ext::Crypto::Random rand;
	uint8_t privKey[32];
	do {
		rand.NextBytes(span<uint8_t>(privKey, sizeof privKey));
	} while (memcmp(privKey, s_maxModOrder, 32) > 0);
	SetPrivData(Span(privKey, sizeof privKey), bCompressed);
#else
	ECDsa dsa(256);
	Key = dsa.Key;
	SetPrivData(ki.Key.Export(CngKeyBlobFormat::OSslEccPrivateBignum), bCompressed);
#endif
}

Blob KeyInfoBase::ToPrivateKeyDER() const {
	ASSERT(m_privKey.size() == 32);

#if UCFG_COIN_ECC=='S'
	return Sec256Dsa::PrivKeyToDER(m_privKey, PubKey.IsCompressed());
#else
	return CngKey(Key).Export(PubKey.IsCompressed() ? CngKeyBlobFormat::OSslEccPrivateCompressedBlob : CngKeyBlobFormat::OSslEccPrivateBlob);
#endif
}

void KeyInfoBase::FromDER(RCSpan privKey, RCSpan pubKey) {
#if UCFG_COIN_ECC=='S'
	Blob blob = Sec256Dsa::PrivKeyFromDER(privKey);
#else
	Blob blob = CngKey::Import(privKey, CngKeyBlobFormat::OSslEccPrivateBlob).Export(CngKeyBlobFormat::OSslEccPrivateBignum);
#endif
	SetPrivData(blob, pubKey.size() == 33);
	if (!Equal(PubKey.Data, pubKey))
		Throw(E_FAIL);
}

static Blob CreateAesBip38(Aes& aes, RCString password, RCSpan salt) {
	Blob derived = Scrypt(Encoding::UTF8.GetBytes(password), salt, 16384, 8, 8, 64);
	aes.Mode = CipherMode::ECB;
	aes.Padding = PaddingMode::None;
	aes.Key = ConstBuf(derived.constData()+32, 32);
	return derived;
}

String KeyInfoBase::ToString(RCString password) const {			// non-EC-multiplied case
	ASSERT(m_privKey.size() == 32);

	uint8_t d[39] ={ 1, 0x42, 0xC0 };
	size_t size = 39;
	if (password == nullptr) {
		d[0] = 0x80;
		memcpy(d+1, m_privKey.constData(), 32);
		size = 33;
		if (PubKey.IsCompressed())
			d[size++] = 1;
	} else {
		if (PubKey.IsCompressed())
			d[2] |= 0x20;

		memcpy(d + 3, SHA256_SHA256(Encoding::UTF8.GetBytes(ToAddress().ToString())).data(), 4);
		Aes aes;
		Blob derived = CreateAesBip38(aes, password, ConstBuf(d+3, 4));
		Blob x = m_privKey;
		VectorXor(x.data(), derived.constData(), 32);
		memcpy(d+7, aes.Encrypt(x).constData(), 32);
	}
	return ConvertToBase58(ConstBuf(d, size));
}

void KeyInfoBase::FromBIP38(RCString bip38, RCString password) {
	if (password.empty())
		Throw(ExtErr::InvalidPassword);

	Blob blob = ConvertFromBase58(bip38);
	const uint8_t *d = blob.constData();
	if (blob.size() != 39 || d[0]!=1 || d[1] != 0x42)
		Throw(CoinErr::InvalidPrivateKey);
	Aes aes;
	Blob derived = CreateAesBip38(aes, password, ConstBuf(d+3, 4));
	Blob x = aes.Decrypt(ConstBuf(d+7, 32));
	VectorXor(x.data(), derived.constData(), 32);
	SetPrivData(x, d[2] & 0x20);
	if (memcmp(d + 3, SHA256_SHA256(Encoding::UTF8.GetBytes(ToAddress().ToString())).data(), 4))
		Throw(ExtErr::InvalidPassword);
}

Blob KeyInfoBase::SignHash(RCSpan cbuf) {
#if UCFG_COIN_ECC=='S'
	Sec256Dsa dsa;
	dsa.m_privKey = m_privKey;
	return dsa.SignHash(cbuf);
#else
	ECDsa dsa(Key);
	return dsa.SignHash(cbuf);
#endif
}

bool KeyInfoBase::VerifyHash(RCSpan pubKey, const HashValue& hash, RCSpan sig) {
#if UCFG_COIN_ECC=='S'
	Sec256DsaEx dsa;
	dsa.ParsePubKey(pubKey);
#else
	ECDsa dsa(CngKey::Import(pubKey, CngKeyBlobFormat::OSslEccPublicBlob));
#endif
	return dsa.VerifyHash(hash.ToSpan(), sig);
}


/*
void KeyInfoBase::put_PrivKey(const Blob& v) {
if (v.Size > 32)
Throw(E_FAIL);
Key = CngKey::Import(m_privKey = v, CngKeyBlobFormat::OSslEccPrivateBignum);
Blob pubKey = Key.Export(CngKeyBlobFormat::OSslEccPublicBlob);
if (PubKey.Size != 0 && PubKey != pubKey)
Throw(E_FAIL);
Hash160 = Coin::Hash160(PubKey = pubKey);
}*/


void KeyInfoBase::SetPrivData(const PrivateKey& privKey) {
	pair<Blob, bool> pp = privKey.GetPrivdataCompressed();
	SetPrivData(pp.first, pp.second);
}

void KeyInfoBase::SetKeyFromPubKey() {
#if UCFG_COIN_ECC!='S'
	Key = CngKey::Import(PubKey.Data, CngKeyBlobFormat::OSslEccPublicBlob);
#endif
}

pair<Blob, bool> PrivateKey::GetPrivdataCompressed() const {
	return pair<Blob, bool>(ConstBuf(m_data.data(), 32), m_data.size() == 33);
}

Blob CanonicalPubKey::ToCompressed() const {
	ASSERT(Data.size() == 33 || Data.size() == 65);
	if (Data.size() == 33)
		return Span(Data);
#if UCFG_COIN_ECC=='S'
	if (!Sec256Dsa::VerifyPubKey(Data))
		Throw(ExtErr::Crypto);
	Blob r(Data.constData(), 33);
	r.data()[0] = 2 | Data.constData()[64] & 1;
#else
	Blob r = CngKey::Import(Data, CngKeyBlobFormat::OSslEccPublicBlob).Export(CngKeyBlobFormat::OSslEccPublicCompressedBlob);
#endif
	r.data()[0] |= 0x80;
	if (Data.constData()[0] & 2)
		r.data()[0] |= 4; // POINT_CONVERSION_HYBRID
#ifdef	X_DEBUG//!!!D
	Blob decompressed = CanonicalPubKey::FromCompressed(r).Data;
	if (decompressed != Data) {
		auto a = CngKey::Import(Data, CngKeyBlobFormat::OSslEccPublicBlob);
		Blob rref = a.Export(CngKeyBlobFormat::OSslEccPublicCompressedBlob);
		rref.data()[0] |= 0x80;
		if (Data.constData()[0] & 2)
			rref.data()[0] |= 4; // POINT_CONVERSION_HYBRID
		ASSERT(rref == r);
		ASSERT(decompressed == Data);
	}
#	endif
	return r;
}


static const BigInteger s_ec_p("FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFEFFFFFC2F", 16);

CanonicalPubKey CanonicalPubKey::FromCompressed(RCSpan cbuf) {
	CanonicalPubKey r;
	if (cbuf.size() != 33 || !(cbuf[0] & 0x80))
		r.Data = CData(cbuf.data(), cbuf.size());
	else {
#if UCFG_COIN_ECC=='S'
		uint8_t buf[33];
		memcpy(buf, cbuf.data(), 33);
		buf[0] &= 0x7F;
		Blob blob = Sec256Dsa::DecompressPubKey(ConstBuf(buf, 33));
		r.Data = CData(blob.constData(), blob.size());
#else
		BigInteger x = OpensslBn::FromBinary(ConstBuf(cbuf.P+1, 32)).ToBigInteger();
		BigInteger xr = (x*x*x+7) % s_ec_p;
		BigInteger y = sqrt_mod(xr, s_ec_p);
		if (y.TestBit(0) != bool(cbuf.P[0] & 1))
			y = s_ec_p-y;
		r.Data.Size = 65;
		r.Data.data()[0] = 4;									//	POINT_CONVERSION_UNCOMPRESSED
		if (cbuf.P[0] & 4) {
			r.Data.data()[0] |= 2 | (cbuf.P[0] & 1);				//  POINT_CONVERSION_HYBRID
		}
		memcpy(r.Data.data()+1, cbuf.P+1, 32);
		OpensslBn(y).ToBinary(r.Data.data()+33, 32);
#endif
	}
	return r;
}



} // Coin::

