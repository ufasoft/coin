/*######   Copyright (c) 2015-2019 Ufasoft  http://ufasoft.com  mailto:support@ufasoft.com,  Sergey Pavlov  mailto:dev@ufasoft.com ####
#                                                                                                                                     #
# 		See LICENSE for licensing information                                                                                         #
#####################################################################################################################################*/

#pragma once

#include <el/crypto/sign.h>

#define USE_NUM_GMP
#define USE_FIELD_GMP
#define USE_FIELD_INV_BUILTIN

#if UCFG_USE_MASM && UCFG_CPU_X86_X64
#	define USE_ASM_X86_64
#endif

#if UCFG_PLATFORM_X64 && defined USE_ASM_X86_64
#	define USE_FIELD_4X64 1
#	if UCFG_MSC_VERSION
#		define USE_SCALAR_8X32 1
#	else
#		define HAVE___INT128 1
#		define USE_SCALAR_4X64 1
#	endif
#elif UCFG_64 && (!UCFG_MSC_VERSION || defined(USE_ASM_X86_64))
#	define USE_FIELD_5X52 1
#	if UCFG_MSC_VERSION
#		define USE_SCALAR_8X32 1
#	else
#		define HAVE___INT128 1
#		define USE_SCALAR_4X64 1
#	endif
#else
#	define USE_FIELD_10X26 1
#	define USE_SCALAR_8X32 1
#endif


#include <secp256k1/group.h>
#include <secp256k1/ecdsa.h>
#include <secp256k1/secp256k1.h>

namespace Ext { namespace Crypto {

class Sec256Signature {
	secp256k1_scalar m_r, m_s;
public:
	Sec256Signature();
	Sec256Signature(RCSpan cbuf);
	~Sec256Signature();
	void Parse(RCSpan cbuf);
	void AssignCompact(const array<uint8_t, 64>& ar);
	array<uint8_t, 64> ToCompact() const;
	Blob Serialize() const;
private:
	friend class Sec256Dsa;
};

class Sec256SignatureEx {	// High-level signature
	secp256k1_ecdsa_signature m_sig;
public:
	Sec256SignatureEx(RCSpan cbuf) {
		Parse(cbuf);
	}

	void Parse(RCSpan cbuf);
private:
	friend class Sec256DsaEx;
};

class Sec256Dsa : public DsaBase {
	secp256k1_ge m_pubkey;
public:
	Blob m_privKey;

	static vararray<uint8_t, 65> RecoverPubKey(RCSpan hash, const Sec256Signature& sig, uint8_t recid, bool bCompressed = false);

	void ParsePubKey(RCSpan cbuf);
	Blob SignHash(RCSpan hash) override;
	bool VerifyHashSig(RCSpan hash, const Sec256Signature& sig);

	bool VerifyHash(RCSpan hash, RCSpan bufSig) override {
		return VerifyHashSig(hash, bufSig);
	}

	static vararray<uint8_t, 65> PrivKeyToPubKey(RCSpan privKey, bool bCompressed);
	static Blob PrivKeyToDER(RCSpan privKey, bool bCompressed);
	static Blob PrivKeyFromDER(RCSpan der);
	static bool VerifyPubKey(RCSpan cbuf);
	static Blob DecompressPubKey(RCSpan cbuf);
	static bool VerifyKey(const std::array<uint8_t, 32>& key);
};

class Sec256DsaEx : public DsaBase {
protected:
	secp256k1_pubkey m_pubkey;
public:
	void ParsePubKey(RCSpan cbuf);
	bool VerifyHashSig(RCSpan hash, const Sec256SignatureEx& sig);
	Blob SignHash(RCSpan hash) override { Throw(E_NOTIMPL); }

	bool VerifyHash(RCSpan hash, RCSpan bufSig) override {
		return VerifyHashSig(hash, Sec256SignatureEx(bufSig));		//!!!C Use Sec256SignatureEx for compatibility with old incorrect signature format
	}
};

class SchnorrDsa : public Sec256DsaEx {
public:
	bool VerifyHash(RCSpan hash, RCSpan bufSig) override;
};

}} // Ext::Crypto::
