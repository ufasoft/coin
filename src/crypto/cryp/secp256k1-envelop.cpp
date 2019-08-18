/*######   Copyright (c) 2015 Ufasoft  http://ufasoft.com  mailto:support@ufasoft.com,  Sergey Pavlov  mailto:dev@ufasoft.com      ####
#                                                                                                                                     #
# 		See LICENSE for licensing information                                                                                         #
#####################################################################################################################################*/

#include <el/ext.h>

#include "cryp-config.h"

#define HAVE_CONFIG_H
#include "secp256k1.h"

#include <secp256k1/secp256k1.h>
#include <secp256k1/util.h>

#include <secp256k1/include/secp256k1_recovery.h>

#pragma warning(disable: 4319)

#pragma comment(lib, "mpir")


#include <secp256k1/secp256k1.c>
#include <secp256k1/modules/recovery/main_impl.h>

#include <secp256k1/lax_der_parsing.c>
#include <secp256k1/lax_der_privatekey_parsing.c>


#include <secp256k1/modules/schnorr/main_impl.h>

namespace Ext { namespace Crypto {

struct Sec256Ctx {
	Sec256Ctx() {
		Ctx = ::secp256k1_context_create(SECP256K1_CONTEXT_SIGN | SECP256K1_CONTEXT_VERIFY);
	}
	~Sec256Ctx() {
		::secp256k1_context_destroy(Ctx);
	}

	operator const secp256k1_context*() { return Ctx; }
	const secp256k1_ecmult_context *MultCtx() { return &Ctx->ecmult_ctx; }
private:
	secp256k1_context* Ctx;
} g_sec256Ctx;

void Sec256Check(int rc) {
	if (!rc)
		Throw(ExtErr::Crypto);
}

Sec256Signature::Sec256Signature() {
//	secp256k1_ecdsa_sig_init(&m_sig);
}

Sec256Signature::Sec256Signature(RCSpan cbuf) {
	Parse(cbuf);
}

Sec256Signature::~Sec256Signature() {
//	secp256k1_ecdsa_sig_free(&m_sig);
}

void Sec256Signature::Parse(RCSpan cbuf) {
	if (!secp256k1_ecdsa_sig_parse(&m_r, &m_s, cbuf.data(), cbuf.size()))
		throw CryptoException(make_error_code(ExtErr::Crypto), "Invalid Signature");
}

void Sec256Signature::AssignCompact(const array<uint8_t, 64>& ar) {
	int over1, over2;
	secp256k1_scalar_set_b32(&m_r, ar.data(), &over1);
	secp256k1_scalar_set_b32(&m_s, ar.data()+32, &over2);
	ASSERT(!over1 && !over2);
}

array<uint8_t, 64> Sec256Signature::ToCompact() const {
	array<uint8_t, 64> r;
	secp256k1_scalar_get_b32(r.data(), &m_r);
	secp256k1_scalar_get_b32(r.data()+32, &m_s);
	return r;
}

Blob Sec256Signature::Serialize() const {
	uint8_t buf[100];
	size_t size = sizeof buf;
	if (!secp256k1_ecdsa_sig_serialize(buf, &size, &m_r, &m_s))
		Throw(errc::invalid_argument);
	return Blob(buf, size);
}

void Sec256SignatureEx::Parse(RCSpan cbuf) {
	if (!ecdsa_signature_parse_der_lax(g_sec256Ctx, &m_sig, cbuf.data(), cbuf.size()))
		throw CryptoException(make_error_code(ExtErr::Crypto), "Invalid Signature");
	secp256k1_ecdsa_signature_normalize(g_sec256Ctx, &m_sig, &m_sig);
}

static vararray<uint8_t, 65> SerializePubKey(secp256k1_ge& q, bool bCompressed = false) {
	uint8_t buf[100];
	size_t pubkeylen;
	secp256k1_eckey_pubkey_serialize(&q, buf, &pubkeylen, bCompressed);
	return vararray<uint8_t, 65>(buf, pubkeylen);
}

vararray<uint8_t, 65> Sec256Dsa::RecoverPubKey(RCSpan hash, const Sec256Signature& sig, uint8_t recid, bool bCompressed) {
	ASSERT(hash.size() == 32 && recid < 3);

	secp256k1_scalar m;
	secp256k1_scalar_set_b32(&m, hash.data(), nullptr);
	secp256k1_ge q;
	if (::secp256k1_ecdsa_sig_recover(g_sec256Ctx.MultCtx(), &sig.m_r, &sig.m_s, &q, &m, recid)) {
		return SerializePubKey(q, bCompressed);
	}
	return vararray<uint8_t, 65>();
}

void Sec256Dsa::ParsePubKey(RCSpan cbuf) {
	if (!secp256k1_eckey_pubkey_parse(&m_pubkey, cbuf.data(), cbuf.size()))
		throw CryptoException(make_error_code(ExtErr::Crypto), "Invalid PubKey");
}

Blob Sec256Dsa::SignHash(RCSpan hash) {
	if (hash.size() != 32)
		Throw(errc::invalid_argument);
	secp256k1_ecdsa_signature sig;
	Sec256Check(::secp256k1_ecdsa_sign(g_sec256Ctx, &sig, hash.data(), m_privKey.constData(), secp256k1_nonce_function_rfc6979, nullptr));
	uint8_t buf[72];
	size_t nSiglen = sizeof buf;
	Sec256Check(::secp256k1_ecdsa_signature_serialize_der(g_sec256Ctx, buf, &nSiglen, &sig));
	return Blob(buf, nSiglen);
}

bool Sec256Dsa::VerifyHashSig(RCSpan hash, const Sec256Signature& sig) {
	ASSERT(hash.size() == 32);
	secp256k1_scalar msg;
	int over;
	secp256k1_scalar_set_b32(&msg, hash.data(), &over);
	ASSERT(!over);
	return ::secp256k1_ecdsa_sig_verify(g_sec256Ctx.MultCtx(), &sig.m_r, &sig.m_s, &m_pubkey, &msg);
}

vararray<uint8_t, 65> Sec256Dsa::PrivKeyToPubKey(RCSpan privKey, bool bCompressed) {
	ASSERT(privKey.size() == 32);
	secp256k1_pubkey pubkey;
	Sec256Check(::secp256k1_ec_pubkey_create(g_sec256Ctx, &pubkey, privKey.data()));
	vararray<uint8_t, 65> r;
	size_t clen = 65;
	Sec256Check(::secp256k1_ec_pubkey_serialize(g_sec256Ctx, r.data(), &clen, &pubkey, bCompressed ? SECP256K1_EC_COMPRESSED : SECP256K1_EC_UNCOMPRESSED));
	r.resize(clen);
	return r;
}

Blob Sec256Dsa::PrivKeyToDER(RCSpan privKey, bool bCompressed) {
	ASSERT(privKey.size() == 32);
	uint8_t ar[279];
	size_t privkeylen = sizeof ar;
	Sec256Check(::ec_privkey_export_der(g_sec256Ctx, ar, &privkeylen, privKey.data(), bCompressed));
	return Blob(ar, privkeylen);
}

Blob Sec256Dsa::PrivKeyFromDER(RCSpan der) {
	uint8_t ar[32];
	Sec256Check(::ec_privkey_import_der(g_sec256Ctx, ar, der.data(), der.size()));
	return Blob(ar, 32);
}

bool Sec256Dsa::VerifyPubKey(RCSpan cbuf) {
	secp256k1_pubkey pubkey;
	return ::secp256k1_ec_pubkey_parse(g_sec256Ctx, &pubkey, cbuf.data(), cbuf.size());
}

Blob Sec256Dsa::DecompressPubKey(RCSpan cbuf) {
	uint8_t buf[65];
	size_t pubkeylen = 65;

	secp256k1_ge Q;
	Sec256Check(::secp256k1_eckey_pubkey_parse(&Q, cbuf.data(), cbuf.size()) &&
				::secp256k1_eckey_pubkey_serialize(&Q, buf, &pubkeylen, false));
	return Blob(buf, pubkeylen);
}

bool Sec256Dsa::VerifyKey(const array<uint8_t, 32>& key) {
	return ::secp256k1_ec_seckey_verify(g_sec256Ctx, key.data());
}

void Sec256DsaEx::ParsePubKey(RCSpan cbuf) {
	if (!secp256k1_ec_pubkey_parse(g_sec256Ctx, &m_pubkey, cbuf.data(), cbuf.size()))
		throw CryptoException(make_error_code(ExtErr::Crypto), "Invalid PubKey");
}

bool Sec256DsaEx::VerifyHashSig(RCSpan hash, const Sec256SignatureEx& sig) {
	return secp256k1_ecdsa_verify(g_sec256Ctx, &sig.m_sig, hash.data(), &m_pubkey);
}

bool SchnorrDsa::VerifyHash(RCSpan hash, RCSpan bufSig) {
	return secp256k1_schnorr_verify(g_sec256Ctx, bufSig.data(), hash.data(), &m_pubkey);
}

}} // Ext::Crypto::
