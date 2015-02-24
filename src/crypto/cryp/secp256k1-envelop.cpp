#include <el/ext.h>

#include "cryp-config.h"
#include "secp256k1.h"

#pragma comment(lib, "mpir")


#include <secp256k1/secp256k1.c>


namespace Ext { namespace Crypto {

static struct InitSec256 {
	InitSec256() {
		secp256k1_start();
	}

	~InitSec256() {
		secp256k1_stop();
	}
} s_initSec256;

Sec256Signature::Sec256Signature() {
	secp256k1_ecdsa_sig_init(&m_sig);
}

Sec256Signature::Sec256Signature(const ConstBuf& cbuf) {
	Parse(cbuf);
}

Sec256Signature::~Sec256Signature() {
	secp256k1_ecdsa_sig_free(&m_sig);
}

void Sec256Signature::Parse(const ConstBuf& cbuf) {
	if (!secp256k1_ecdsa_sig_parse(&m_sig, cbuf.P, cbuf.Size))
		throw CryptoException(E_EXT_Crypto, "Invalid Signature");
}

void Sec256Signature::AssignCompact(const array<byte, 64>& ar) {
    secp256k1_num_set_bin(&m_sig.r, ar.data(), 32);
    secp256k1_num_set_bin(&m_sig.s, ar.data()+32, 32);
}


array<byte, 64> Sec256Signature::ToCompact() const {
	array<byte, 64> r;
	secp256k1_num_get_bin(r.data(), 32, &m_sig.r);
    secp256k1_num_get_bin(r.data()+32, 32, &m_sig.s);
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
	secp256k1_ecdsa_pubkey_serialize(&q, buf, &pubkeylen, bCompressed);
	return Blob(buf, pubkeylen);
}

Blob Sec256Dsa::RecoverPubKey(const ConstBuf& hash, const Sec256Signature& sig, byte recid, bool bCompressed) {
	ASSERT(recid < 3);

	secp256k1_num_t m;
    secp256k1_num_set_bin(&m, hash.P, hash.Size);
    secp256k1_ge_t q;
	Blob r(nullptr);
	if (secp256k1_ecdsa_sig_recover(&sig.m_sig, &q, &m, recid)) {
		r = SerializePubKey(q, bCompressed);
		secp256k1_num_free(&m);
	}
	return r;
}

void Sec256Dsa::ParsePubKey(const ConstBuf& cbuf) {
	if (!secp256k1_ecdsa_pubkey_parse(&m_pubkey, cbuf.P, cbuf.Size))
		throw CryptoException(E_EXT_Crypto, "Invalid PubKey");
}

Blob Sec256Dsa::SignHash(const ConstBuf& hash) {
	Throw(E_NOTIMPL);
}

bool Sec256Dsa::VerifyHashSig(const ConstBuf& hash, const Sec256Signature& sig) {
	secp256k1_num_t msg;
    secp256k1_num_set_bin(&msg, hash.P, hash.Size);
	int r = secp256k1_ecdsa_sig_verify(&sig.m_sig, &m_pubkey, &msg);
    secp256k1_num_free(&msg);
	return r;
}


}} // Ext::Crypto::

