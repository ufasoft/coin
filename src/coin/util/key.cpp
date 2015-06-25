/*######   Copyright (c) 2011-2015 Ufasoft  http://ufasoft.com  mailto:support@ufasoft.com,  Sergey Pavlov  mailto:dev@ufasoft.com ####
#                                                                                                                                     #
# 		See LICENSE for licensing information                                                                                         #
#####################################################################################################################################*/

#include <el/ext.h>
#include <el/bignum.h>

#include <el/crypto/hash.h>
#include <el/crypto/cipher.h>
using namespace Crypto;

#include "util.h"

#if UCFG_COIN_ECC=='S'
#	include <crypto/cryp/secp256k1.h>
#endif


namespace Coin {


Address::Address(HasherEng& hasher)
	: Hasher(hasher)
{
	Ver = hasher.AddressVersion;
	memset(data(), 0, size());
}

Address::Address(HasherEng& hasher, const HashValue160& hash, RCString comment)
	: Hasher(hasher)
	, Comment(comment)
{
	Ver = hasher.AddressVersion;
	HashValue160::operator=(hash);
}

Address::Address(HasherEng& hasher, const HashValue160& hash, byte ver)
	: Hasher(hasher)
	, Ver(ver)
{
	HashValue160::operator=(hash);
}

Address::Address(HasherEng& hasher, RCString s)
	: Hasher(hasher)
{
	try {
		Blob blob = ConvertFromBase58(s.Trim());
		if (blob.Size < 21)
			Throw(CoinErr::InvalidAddress);
		Ver = blob.constData()[0];
		memcpy(data(), blob.constData()+1, 20);
	} catch (RCExc) {
		Throw(CoinErr::InvalidAddress);
	}
	CheckVer(hasher);
}

void Address::CheckVer(HasherEng& hasher) const {
	if (&Hasher != &hasher || Ver != hasher.AddressVersion && Ver != hasher.ScriptAddressVersion)
		Throw(CoinErr::InvalidAddress);
}

String Address::ToString() const {
	CHasherEngThreadKeeper hasherKeeper(&Hasher);

	vector<byte> v(21);
	v[0] = Ver;
	memcpy(&v[1], data(), 20);
	return ConvertToBase58(ConstBuf(&v[0], v.size()));
}

Address KeyInfoBase::ToAddress() const {
	return Address(*HasherEng::GetCurrent(), PubKey.Hash160, Comment);
}

Blob KeyInfoBase::PlainPrivKey() const {
	byte typ = 0;
	return Blob(&typ, 1) + get_PrivKey();
}

void KeyInfoBase::SetPrivData(const ConstBuf& cbuf, bool bCompressed) {
	ASSERT(cbuf.Size <= 32);
	m_privKey = cbuf.Size==32 ? cbuf : ConstBuf(Blob(0, 32-cbuf.Size)+cbuf);

#if UCFG_COIN_ECC=='S'
	Blob pubKey = Sec256Dsa::PrivKeyToPubKey(m_privKey, bCompressed);
#else
	Key = CngKey::Import(m_privKey, CngKeyBlobFormat::OSslEccPrivateBignum);
	Blob pubKey = Key.Export(bCompressed ? CngKeyBlobFormat::OSslEccPublicCompressedBlob : CngKeyBlobFormat::OSslEccPublicBlob);
#endif
	if (PubKey.Data.Size != 0 && PubKey.Data != pubKey)
		Throw(E_FAIL);
	PubKey = CanonicalPubKey(pubKey);
}

static const byte s_maxModOrder[32] ={ 0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF, 0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFE, 0xBA,0xAE,0xDC,0xE6,0xAF,0x48,0xA0,0x3B, 0xBF,0xD2,0x5E,0x8C,0xD0,0x36,0x41,0x40 };

KeyInfoBase KeyInfoBase::GenRandom(bool bCompressed) {
	KeyInfoBase ki;
	ki.Comment = nullptr;
#if UCFG_COIN_ECC=='S'
	Ext::Crypto::Random rand;
	byte privKey[32];
	do {
		rand.NextBytes(Buf(privKey, sizeof privKey));
	} while (memcmp(privKey, s_maxModOrder, 32) > 0);
	ki.SetPrivData(ConstBuf(privKey, sizeof privKey), bCompressed);
#else
	ECDsa dsa(256);
	ki.Key = dsa.Key;
	ki.SetPrivData(ki.Key.Export(CngKeyBlobFormat::OSslEccPrivateBignum), bCompressed);
#endif
	return ki;
}

Blob KeyInfoBase::ToPrivateKeyDER() const {
	ASSERT(m_privKey.Size == 32);

#if UCFG_COIN_ECC=='S'
	return Sec256Dsa::PrivKeyToDER(m_privKey, PubKey.IsCompressed());
#else
	return CngKey(Key).Export(PubKey.IsCompressed() ? CngKeyBlobFormat::OSslEccPrivateCompressedBlob : CngKeyBlobFormat::OSslEccPrivateBlob);
#endif
}

KeyInfoBase KeyInfoBase::FromDER(const ConstBuf& privKey, const ConstBuf& pubKey) {
#if UCFG_COIN_ECC=='S'
	Blob blob = Sec256Dsa::PrivKeyFromDER(privKey);
#else
	Blob blob = CngKey::Import(privKey, CngKeyBlobFormat::OSslEccPrivateBlob).Export(CngKeyBlobFormat::OSslEccPrivateBignum);
#endif
	KeyInfoBase ki;
	ki.SetPrivData(blob, pubKey.Size == 33);
	if (ConstBuf(ki.PubKey.Data) != pubKey)
		Throw(E_FAIL);
	return ki;
}

String KeyInfoBase::ToString(RCString password) const {			// non-EC-multiplied case
	ASSERT(m_privKey.Size == 32);

	byte d[39] ={ 1, 0x42, 0xC0 };
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
				
		memcpy(d+3, SHA256_SHA256(Encoding::UTF8.GetBytes(ToAddress().ToString())).data(), 4);

		Blob derived = Scrypt(Encoding::UTF8.GetBytes(password), ConstBuf(d+3, 4), 16384, 8, 8, 64);
		Blob x = m_privKey;
		VectorXor(x.data(), derived.constData(), 32);
		Aes aes;
		aes.Mode = CipherMode::ECB;
		aes.Padding = PaddingMode::None;
		aes.Key = ConstBuf(derived.constData()+32, 32);
		memcpy(d+7, aes.Encrypt(x).constData(), 32);
	}
	return ConvertToBase58(ConstBuf(d, size));
}

Blob KeyInfoBase::SignHash(const ConstBuf& cbuf) {
#if UCFG_COIN_ECC=='S'
	Sec256Dsa dsa;
	dsa.m_privKey = m_privKey;
	return dsa.SignHash(cbuf);
#else
	ECDsa dsa(Key);
	return dsa.SignHash(cbuf);
#endif
}

bool KeyInfoBase::VerifyHash(const ConstBuf& pubKey, const HashValue& hash, const ConstBuf& sig) {
#if UCFG_COIN_ECC=='S'
	Sec256Dsa dsa;
	dsa.ParsePubKey(pubKey);
#else
	ECDsa dsa(CngKey::Import(pubKey, CngKeyBlobFormat::OSslEccPublicBlob));
#endif
	return dsa.VerifyHash(hash.ToConstBuf(), sig);
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
	Key = CngKey::Import(PubKey, CngKeyBlobFormat::OSslEccPublicBlob);
#endif
}

pair<Blob, bool> PrivateKey::GetPrivdataCompressed() const {
	return pair<Blob, bool>(ConstBuf(m_blob.constData(), 32), m_blob.Size == 33);
}

Blob CanonicalPubKey::ToCompressed() const {
	ASSERT(Data.Size==33 || Data.Size==65);
	if (Data.Size == 33)
		return Data;
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

CanonicalPubKey CanonicalPubKey::FromCompressed(const ConstBuf& cbuf) {
	CanonicalPubKey r;
	if (cbuf.Size != 33 || !(cbuf.P[0] & 0x80))
		r.Data = cbuf;
	else {
#if UCFG_COIN_ECC=='S'
		byte buf[33];
		memcpy(buf, cbuf.P, 33);
		buf[0] &= 0x7F;
		r.Data = Sec256Dsa::DecompressPubKey(ConstBuf(buf, 33));
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

