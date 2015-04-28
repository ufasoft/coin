#pragma once

#include <el/crypto/sign.h>

#define USE_NUM_GMP
#define USE_FIELD_GMP
#define USE_FIELD_INV_BUILTIN

#if UCFG_64 && !UCFG_PLATFORM_X64
#	define HAVE___INT128 1
#	define USE_SCALAR_4X64 1
#	define USE_FIELD_5X52 1
#else
#	define USE_SCALAR_8X32 1
#	define USE_FIELD_10X26 1
#endif


#include <secp256k1/group.h>
#include <secp256k1/ecdsa.h>

#ifndef UCFG_MODULE_CRYP
#	pragma comment(lib, "cryp")
#endif

namespace Ext { namespace Crypto {

class Sec256Signature {
public:
	Sec256Signature();
	Sec256Signature(const ConstBuf& cbuf);
	~Sec256Signature();
	void Parse(const ConstBuf& cbuf);
	void AssignCompact(const array<byte, 64>& ar);
	array<byte, 64> ToCompact() const;
	Blob Serialize() const;
private:
	secp256k1_ecdsa_sig_t m_sig;

	friend class Sec256Dsa;
};

class Sec256Dsa : public DsaBase {
public:
	static Blob RecoverPubKey(const ConstBuf& hash, const Sec256Signature& sig, byte recid, bool bCompressed = false);

	void ParsePubKey(const ConstBuf& cbuf);
	Blob SignHash(const ConstBuf& hash) override;
	bool VerifyHashSig(const ConstBuf& hash, const Sec256Signature& sig);

	bool VerifyHash(const ConstBuf& hash, const ConstBuf& bufSig) override {
		return VerifyHashSig(hash, bufSig);
	}
private:
	secp256k1_ge_t m_pubkey;
};

}} // Ext::Crypto::
