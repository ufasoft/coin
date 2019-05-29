/*######   Copyright (c) 2011-2019 Ufasoft  http://ufasoft.com  mailto:support@ufasoft.com,  Sergey Pavlov  mailto:dev@ufasoft.com ####
#                                                                                                                                     #
# 		See LICENSE for licensing information                                                                                         #
#####################################################################################################################################*/

#include <el/ext.h>

#include <el/bignum.h>

#include <el/crypto/hash.h>
#include <el/crypto/ecdsa.h>
using namespace Ext::Crypto;

#include "coin-model.h"
#include "eng.h"
#include "script.h"
#include "crypter.h"

namespace Coin {

Blob Signer::SignHash(const HashValue& hash) {
	return Key.m_pimpl->SignHash(hash.ToSpan());
}

void Signer::Sign(const KeyInfo& randomKey, RCSpan pkScript, uint32_t nIn, SigHashType hashType, Span scriptPrereq) {
	Address parsed = TxOut::CheckStandardType(pkScript);
	TxIn& txIn = m_tx.m_pimpl->m_txIns.at(nIn);
	Hasher.NIn = nIn;
	Hasher.HashType = hashType;
	HashValue hash = Hasher.Hash(scriptPrereq + pkScript);
	KeyInfo ki(nullptr);
	MemoryStream msSig;
	ScriptWriter wrSig(msSig);
	HashValue160 hash160;
	switch (parsed.Type) {
	case AddressType::PubKey: {
		hash160 = Hash160(parsed.Data());
		ASSERT(Equal(GetMyKeyInfo(hash160).PubKey.Data, parsed.Data()));
		ki = GetMyKeyInfo(hash160);
		uint8_t byHashType = uint8_t(hashType);
		Key = randomKey ? randomKey : ki;
		Blob sig = SignHash(hash) + Span(&byHashType, 1);
		wrSig << sig;
	} break;
	case AddressType::P2SH: {
		HashValue160 hashRedeem = HashValue160(parsed);
		ki = GetMyKeyInfoByScriptHash(hashRedeem);
		auto redeemScript = ki->ToPubScript();
		uint8_t byHashType = uint8_t(hashType);
		Key = randomKey ? randomKey : ki;
		hash = Hasher.Hash(scriptPrereq + redeemScript);
		Blob sig = SignHash(hash) + Span(&byHashType, 1);
		wrSig << sig << Span(ki.PubKey.Data) << Span(redeemScript);
	} break;
	case AddressType::P2PKH: {
		ki = GetMyKeyInfo(HashValue160(parsed));
		uint8_t byHashType = uint8_t(hashType);
		Key = randomKey ? randomKey : ki;
		Blob sig = SignHash(hash) + Span(&byHashType, 1);
		wrSig << sig << Span(ki.PubKey.Data);
	} break;
	default:
		Throw(E_FAIL);
	}
	txIn.put_Script(msSig);

	if (!randomKey && 0 == scriptPrereq.size() && !Hasher.VerifyScript(txIn.Script(), pkScript))
		Throw(E_FAIL);
}

} // namespace Coin
