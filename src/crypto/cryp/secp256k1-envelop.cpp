/*######   Copyright (c) 2015 Ufasoft  http://ufasoft.com  mailto:support@ufasoft.com,  Sergey Pavlov  mailto:dev@ufasoft.com ####
#                                                                                                                                     #
# 		See LICENSE for licensing information                                                                                         #
#####################################################################################################################################*/

#include <el/ext.h>

#include "cryp-config.h"

#include "secp256k1.h"

#include <secp256k1/secp256k1.h>
#include <secp256k1/util.h>

#pragma warning(disable: 4319)

#pragma comment(lib, "mpir")


#include <secp256k1/secp256k1.c>


namespace Ext { namespace Crypto {

struct Sec256Ctx {
	Sec256Ctx() {
		Ctx = ::secp256k1_context_create(SECP256K1_CONTEXT_SIGN | SECP256K1_CONTEXT_VERIFY);
	}
	~Sec256Ctx() {
		::secp256k1_context_destroy(Ctx);
	}

	operator const secp256k1_context_t*() { return Ctx; }
	const secp256k1_ecmult_context_t *MultCtx() { return &Ctx->ecmult_ctx; }
private:
	secp256k1_context_t* Ctx;
} g_sec256Ctx;

void Sec256Check(int rc) {
	if (!rc)
		Throw(ExtErr::Crypto);
}

Sec256Signature::Sec256Signature() {
//	secp256k1_ecdsa_sig_init(&m_sig);
}

Sec256Signature::Sec256Signature(const ConstBuf& cbuf) {
	Parse(cbuf);
}

Sec256Signature::~Sec256Signature() {
//	secp256k1_ecdsa_sig_free(&m_sig);
}

void Sec256Signature::Parse(const ConstBuf& cbuf) {
	if (!secp256k1_ecdsa_sig_parse(&m_sig, cbuf.P, cbuf.Size))
		throw CryptoException(ExtErr::Crypto, "Invalid Signature");
}

void Sec256Signature::AssignCompact(const array<byte, 64>& ar) {
	int over1, over2;
	secp256k1_scalar_set_b32(&m_sig.r, ar.data(), &over1);
	secp256k1_scalar_set_b32(&m_sig.s, ar.data()+32, &over2);
	ASSERT(!over1 && !over2);
}


array<byte, 64> Sec256Signature::ToCompact() const {
	array<byte, 64> r;
	secp256k1_scalar_get_b32(r.data(), &m_sig.r);
	secp256k1_scalar_get_b32(r.data()+32, &m_sig.s);
	return r;
}

Blob Sec256Signature::Serialize() const {
	byte buf[100];
	int size = sizeof buf;
	if (!secp256k1_ecdsa_sig_serialize(buf, &size, &m_sig))
		Throw(E_INVALIDARG);
	return Blob(buf, size);
}

static Blob SerializePubKey(secp256k1_ge_t& q, bool bCompressed = false) {
	byte buf[100];
	int pubkeylen;
	secp256k1_eckey_pubkey_serialize(&q, buf, &pubkeylen, bCompressed);
	return Blob(buf, pubkeylen);
}

Blob Sec256Dsa::RecoverPubKey(const ConstBuf& hash, const Sec256Signature& sig, byte recid, bool bCompressed) {
	ASSERT(hash.Size==32 && recid < 3);

	secp256k1_scalar_t m;
	int over;
	secp256k1_scalar_set_b32(&m, hash.P, &over);
	ASSERT(!over);
    secp256k1_ge_t q;
	Blob r(nullptr);
	if (secp256k1_ecdsa_sig_recover(g_sec256Ctx.MultCtx(), &sig.m_sig, &q, &m, recid)) {
		r = SerializePubKey(q, bCompressed);
	}
	return r;
}

void Sec256Dsa::ParsePubKey(const ConstBuf& cbuf) {
	if (!secp256k1_eckey_pubkey_parse(&m_pubkey, cbuf.P, cbuf.Size))
		throw CryptoException(ExtErr::Crypto, "Invalid PubKey");
}

Blob Sec256Dsa::SignHash(const ConstBuf& hash) {
	if (hash.Size != 32)
		Throw(errc::invalid_argument);
	byte sig[72];
	int nSigLen = sizeof sig;
	Sec256Check(::secp256k1_ecdsa_sign(g_sec256Ctx, hash.P, sig, &nSigLen, m_privKey.constData(), secp256k1_nonce_function_rfc6979, nullptr));
	return Blob(sig, nSigLen);
}

bool Sec256Dsa::VerifyHashSig(const ConstBuf& hash, const Sec256Signature& sig) {
	ASSERT(hash.Size==32);
	secp256k1_scalar_t msg;
	int over;
	secp256k1_scalar_set_b32(&msg, hash.P, &over);
	ASSERT(!over);
	return ::secp256k1_ecdsa_sig_verify(g_sec256Ctx.MultCtx(), &sig.m_sig, &m_pubkey, &msg);
}

vararray<byte, 65> Sec256Dsa::PrivKeyToPubKey(const ConstBuf& privKey, bool bCompressed) {
	ASSERT(privKey.Size == 32);
	vararray<byte, 65> r;
	int clen = 65;
	Sec256Check(::secp256k1_ec_pubkey_create(g_sec256Ctx, r.data(), &clen, privKey.P, bCompressed));
	r.resize(clen);
	return r;	
}

Blob Sec256Dsa::PrivKeyToDER(const ConstBuf& privKey, bool bCompressed) {
	ASSERT(privKey.Size == 32);
	byte ar[279];
	int privkeylen = sizeof ar;
	Sec256Check(::secp256k1_ec_privkey_export(g_sec256Ctx, privKey.P, ar, &privkeylen, bCompressed));
	return Blob(ar, privkeylen);
}

Blob Sec256Dsa::PrivKeyFromDER(const ConstBuf& der) {
	byte ar[32];
	Sec256Check(::secp256k1_ec_privkey_import(g_sec256Ctx, ar, der.P, der.Size));
	return Blob(ar, 32);
}

Blob Sec256Dsa::DecompressPubKey(const ConstBuf& cbuf) {
	byte buf[65];
	int pubkeylen = 33;
	Sec256Check(::secp256k1_ec_pubkey_decompress(g_sec256Ctx, cbuf.P, buf, &pubkeylen));
	return Blob(buf, pubkeylen);
}




}} // Ext::Crypto::

