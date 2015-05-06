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

struct InitSec256 {
	InitSec256() { secp256k1_start(SECP256K1_START_SIGN | SECP256K1_START_VERIFY); }
	~InitSec256() { secp256k1_stop(); }
} g_initSec256;

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
	if (secp256k1_ecdsa_sig_recover(&sig.m_sig, &q, &m, recid)) {
		r = SerializePubKey(q, bCompressed);
	}
	return r;
}

void Sec256Dsa::ParsePubKey(const ConstBuf& cbuf) {
	if (!secp256k1_eckey_pubkey_parse(&m_pubkey, cbuf.P, cbuf.Size))
		throw CryptoException(ExtErr::Crypto, "Invalid PubKey");
}

Blob Sec256Dsa::SignHash(const ConstBuf& hash) {
	Throw(E_NOTIMPL);
}

bool Sec256Dsa::VerifyHashSig(const ConstBuf& hash, const Sec256Signature& sig) {
	ASSERT(hash.Size==32);
	secp256k1_scalar_t msg;
	int over;
	secp256k1_scalar_set_b32(&msg, hash.P, &over);
	ASSERT(!over);
	return secp256k1_ecdsa_sig_verify(&sig.m_sig, &m_pubkey, &msg);
}


}} // Ext::Crypto::

