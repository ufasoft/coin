/*######   Copyright (c) 2011-2019 Ufasoft  http://ufasoft.com  mailto:support@ufasoft.com,  Sergey Pavlov  mailto:dev@ufasoft.com ####
#                                                                                                                                     #
# 		See LICENSE for licensing information                                                                                         #
#####################################################################################################################################*/

#include <el/ext.h>

#include <el/bignum.h>

#include <el/crypto/hash.h>
#include <el/crypto/ecdsa.h>
using namespace Ext::Crypto;

#include "script.h"
#include "coin-model.h"
#include "coin-protocol.h"

#if UCFG_COIN_ECC=='S'
#	include <crypto/cryp/secp256k1.h>
#else
#	include <el/crypto/ext-openssl.h>
#endif

namespace Coin {

int MAX_OP_COUNT = 201;

BigInteger VmStack::ToBigInteger(const VmStack::Value& v) {
	Blob blob = Span(v);
	uint8_t& msb = blob.data()[blob.size() - 1];
	bool bNeg = msb & 0x80;
	msb &= 0x7F;
	BigInteger r = BigInteger(blob.constData(), blob.size());
	return bNeg ? -r : r;
}

Vm::Value VmStack::FromBigInteger(const BigInteger& bi) {
	if (Sign(bi) == 0)
		return Vm::Value();
	bool bNeg = Sign(bi) < 0;
	Blob blob = (bNeg ? -bi : bi).ToBytes();
	blob.data()[blob.size() - 1] |= (bNeg ? 0x80 : 0);
	return Vm::Value(blob);
}

bool CoinEng::IsValidSignatureEncoding(RCSpan sig) {
	size_t size = sig.size();
	if (size < 9 || size > 72 || sig[0] != 0x30)
		return false;

	// Make sure the length covers the entire signature.
	if (sig[1] != size - 2)
		return false;

	// Extract the length of the R element.
	unsigned int lenR = sig[3];

	// Make sure the length of the S element is still inside the signature.
	if (5 + lenR >= size)
		return false;

	// Extract the length of the S element.
	unsigned int lenS = sig[5 + lenR];

	// Verify that the length of the signature matches the sum of the length
	// of the elements.
	if ((size_t)(lenR + lenS + 6) != size)
		return false;

	// Check whether the R element is an integer.
	if (sig[2] != 0x02)
		return false;

	// Zero-length integers are not allowed for R.
	if (lenR == 0)
		return false;

	// Negative numbers are not allowed for R.
	if (sig[4] & 0x80)
		return false;

	// Null bytes at the start of R are not allowed, unless R would
	// otherwise be interpreted as a negative number.
	if (lenR > 1 && (sig[4] == 0x00) && !(sig[5] & 0x80))
		return false;

	// Check whether the S element is an integer.
	if (sig[lenR + 4] != 0x02)
		return false;

	// Zero-length integers are not allowed for S.
	if (lenS == 0)
		return false;

	// Negative numbers are not allowed for S.
	if (sig[lenR + 6] & 0x80)
		return false;

	// Null bytes at the start of S are not allowed, unless S would otherwise be
	// interpreted as a negative number.
	if (lenS > 1 && (sig[lenR + 6] == 0x00) && !(sig[lenR + 7] & 0x80))
		return false;

	return true;
}

ScriptWriter& operator<<(ScriptWriter& wr, const Script& script) {
	Throw(E_NOTIMPL);
	return wr;
}

Script::Script(RCSpan mb) {
	Vm vm;
	vm.Init(mb);
	try {
		//		DBG_LOCAL_IGNORE_NAME(E_EXT_EndOfStream, ignE_EXT_EndOfStream);

		while (!vm.m_stm->Eof())
			push_back(vm.GetOp());
	} catch (const EndOfStreamException&) {
		Throw(CoinErr::SCRIPT_ERR_UNKNOWN_ERROR);
	}
}

void Script::FindAndDelete(RCSpan mb) {
	Throw(E_NOTIMPL);
}

Blob Script::BlobFromPubKey(const CanonicalPubKey& pubKey) {
	MemoryStream ms;
	ScriptWriter wr(ms);
	wr << Span(pubKey.Data) << Opcode::OP_CHECKSIG;
	return Span(ms);
}

bool GetInstr(const CMemReadStream& stm, LiteInstr& instr) {
	int op = stm.ReadByte();
	if (op < 0)
		return false;
	size_t remains = stm.Length - stm.Position;
	BinaryReader rd(stm);
	size_t len;
	switch (instr.Opcode = instr.OriginalOpcode = (Opcode)op) {
	case Opcode::OP_PUSHDATA1:
		if (remains < 1)
			return false;
		len = rd.ReadByte();
		if (remains < 1 + len)
			return false;
		instr.Value = Span(stm.m_mb.data() + stm.Position, len);
		stm.ReadBuffer(0, instr.Value.size());
		break;
	case Opcode::OP_PUSHDATA2:
		if (remains < 2)
			return false;
		len = rd.ReadUInt16();
		if (remains < 2 + len)
			return false;
		instr.Value = Span(stm.m_mb.data() + stm.Position, len);
		stm.ReadBuffer(0, instr.Value.size());
		instr.Opcode = Opcode::OP_PUSHDATA1;
		break;
	case Opcode::OP_PUSHDATA4:
		if (remains < 4)
			return false;
		len = rd.ReadUInt32();
		if (remains < 4 + len)
			return false;
		instr.Value = Span(stm.m_mb.data() + stm.Position, len);
		stm.ReadBuffer(0, instr.Value.size());
		instr.Opcode = Opcode::OP_PUSHDATA1;
		break;
	case Opcode::OP_NOP:
	case Opcode::OP_NOP1:
	case Opcode::OP_NOP4:
	case Opcode::OP_NOP5:
	case Opcode::OP_NOP6:
	case Opcode::OP_NOP7:
	case Opcode::OP_NOP8:
	case Opcode::OP_NOP9:
	case Opcode::OP_NOP10:
		instr.Opcode = Opcode::OP_NOP;
		break;
	case Opcode::OP_1NEGATE:
	case Opcode::OP_1:
	case Opcode::OP_2:
	case Opcode::OP_3:
	case Opcode::OP_4:
	case Opcode::OP_5:
	case Opcode::OP_6:
	case Opcode::OP_7:
	case Opcode::OP_8:
	case Opcode::OP_9:
	case Opcode::OP_10:
	case Opcode::OP_11:
	case Opcode::OP_12:
	case Opcode::OP_13:
	case Opcode::OP_14:
	case Opcode::OP_15:
	case Opcode::OP_16:
		{
			static const uint8_t s_tinyNumbers[] = { 0x81, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16 };
			instr.Opcode = Opcode::OP_PUSHDATA1;
			instr.Value = Span(&s_tinyNumbers[uint8_t(op) - uint8_t(Opcode::OP_1NEGATE)], 1);
		}
		break;
	default:
		if (instr.Opcode < Opcode::OP_PUSHDATA1) {
			if (remains < (uint8_t)instr.Opcode)
				return false;
			instr.Value = Span(stm.m_mb.data() + stm.Position, (uint8_t)instr.Opcode);
			instr.Opcode = Opcode::OP_PUSHDATA1;
			stm.ReadBuffer(0, instr.Value.size());
		} else if (instr.Opcode > Opcode::OP_NOP10) {
			TRC(7, "OP_UNKNOWN: " << hex << int(instr.Opcode));
		}
	}
	return true;
}

Instr Vm::GetOp() {
	CoinEng& eng = Eng();

	Instr instr;
	switch (instr.Opcode = instr.OriginalOpcode = (Opcode)m_rd->ReadByte()) {
	case Opcode::OP_PUSHDATA1: instr.Value = m_rd->ReadBytes(m_rd->ReadByte()); break;
	case Opcode::OP_PUSHDATA2:
		instr.Value = m_rd->ReadBytes(m_rd->ReadUInt16());
		instr.Opcode = Opcode::OP_PUSHDATA1;
		break;
	case Opcode::OP_PUSHDATA4:
		instr.Value = m_rd->ReadBytes(m_rd->ReadUInt32());
		instr.Opcode = Opcode::OP_PUSHDATA1;
		break;
	case Opcode::OP_NOP:
	case Opcode::OP_NOP1:
	case Opcode::OP_NOP4:
	case Opcode::OP_NOP5:
	case Opcode::OP_NOP6:
	case Opcode::OP_NOP7:
	case Opcode::OP_NOP8:
	case Opcode::OP_NOP9:
	case Opcode::OP_NOP10: instr.Opcode = Opcode::OP_NOP; break;
	case Opcode::OP_1NEGATE:
	case Opcode::OP_1:
	case Opcode::OP_2:
	case Opcode::OP_3:
	case Opcode::OP_4:
	case Opcode::OP_5:
	case Opcode::OP_6:
	case Opcode::OP_7:
	case Opcode::OP_8:
	case Opcode::OP_9:
	case Opcode::OP_10:
	case Opcode::OP_11:
	case Opcode::OP_12:
	case Opcode::OP_13:
	case Opcode::OP_14:
	case Opcode::OP_15:
	case Opcode::OP_16:
		instr.Value = FromBigInteger(int(instr.Opcode) - int(Opcode::OP_1) + 1);
		instr.Opcode = Opcode::OP_PUSHDATA1;
		break;
	default:
		if (instr.Opcode < Opcode::OP_PUSHDATA1) {
			if (int(instr.Opcode) > m_rd->BaseStream.Length - m_rd->BaseStream.Position)
				Throw(CoinErr::SCRIPT_ERR_UNKNOWN_ERROR); //!!!T to avoid nested EH
			instr.Value.resize((uint8_t)instr.Opcode, false);
			m_rd->Read(instr.Value.data(), (uint8_t)instr.Opcode);
			instr.Opcode = Opcode::OP_PUSHDATA1;
		} else if (instr.Opcode > eng.MaxOpcode) {
			TRC(1, "OP_UNKNOWN: " << hex << int(instr.Opcode));
		}
	}
	return instr;
}

int CalcSigOpCount1(RCSpan script, bool bAccurate) {
	CoinEng& eng = Eng();

	int r = 0;
	CMemReadStream stm(script);
	LiteInstr instr, instrPrev;
	for (int i = 0; GetInstr(stm, instr); instrPrev = instr, ++i) {
		switch (instr.Opcode) {
		case Opcode::OP_CHECKMULTISIG:
		case Opcode::OP_CHECKMULTISIGVERIFY:
			if (bAccurate && i > 0 && instrPrev.IsSmallPositiveOpcode())
				r += int(instrPrev.OriginalOpcode) - int(Opcode::OP_1) + 1;
			else
				r += MAX_PUBKEYS_PER_MULTISIG;
			break;
		default:
			r += (int)eng.IsCheckSigOp(instr.Opcode);
		}
	}
	return r;
}

/*!!!R
int Script::CalcSigOpCount(bool bAccurate) const {
	int r = 0;
	for (int i=0; i<size(); ++i) {
		switch (_self[i].Opcode) {
		case Opcode::OP_CHECKSIG:
		case Opcode::OP_CHECKSIGVERIFY:
			++r;
			break;
		case Opcode::OP_CHECKMULTISIG:
		case Opcode::OP_CHECKMULTISIGVERIFY:
			if (bAccurate && i>0 && _self[i-1].OriginalOpcode>=OP_1 && _self[i-1].OriginalOpcode<=OP_16)
				r += int(_self[i-1].OriginalOpcode) - Opcode::OP_1 + 1;
			else
				r += 20;
			break;
		}
	}
	return r;
}*/

int CalcSigOpCount(RCSpan script, RCSpan scriptSig) {
	if (!Script::IsPayToScriptHash(script)) //!!!TODO checking again
		return CalcSigOpCount1(script, true);
	Script s(scriptSig);
	for (int i = 0; i < s.size(); ++i)
		if (s[i].OriginalOpcode > Opcode::OP_16)
			return 0;
	if (s.empty() || s.back().Opcode != Opcode::OP_PUSHDATA1)
		return 0;
	return CalcSigOpCount1(s.back().Value, true);
}

static const uint8_t s_b1 = 1;

const VmStack::Value VmStack::TrueValue(Span(&s_b1, 1)), VmStack::FalseValue;

VmStack::Value& VmStack::GetStack(unsigned idx) {
	if (idx >= Stack.size())
		Throw(CoinErr::SCRIPT_ERR_INVALID_STACK_OPERATION);
	return Stack.end()[-1 - (ptrdiff_t)idx];	// cast is necessary to avoid signed/unsigned conversion error
}

VmStack::Value VmStack::Pop() {
	if (Stack.empty())
		Throw(CoinErr::SCRIPT_ERR_INVALID_STACK_OPERATION);
	Value r = Stack.back();
	Stack.pop_back();
	return r;
}

void VmStack::SkipStack(int n) {
	while (n--) {
		if (Stack.empty())
			Throw(CoinErr::SCRIPT_ERR_INVALID_STACK_OPERATION);
		Stack.pop_back();
	}
}

void VmStack::Push(const Value& v) {
	Stack.push_back(v);
}

Blob Script::DeleteSubpart(RCSpan mb, RCSpan part) {
	size_t partSize = part.size();
	if (0 == partSize)
		return mb;
	const uint8_t *partData = part.data();
	Vm vm;
	vm.Init(mb);
	MemoryStream ms(mb.size());
	while (true) {
		uint64_t pos = vm.m_stm->Position;
		for (; pos + partSize <= mb.size() && !memcmp(mb.data() + pos, partData, partSize); pos = vm.m_stm->Position)
			vm.m_stm->put_Position(pos + partSize);
		if (vm.m_stm->Eof())
			break;
		vm.GetOp();
		ms.Write(mb.subspan(pos, (int)(vm.m_stm->Position - pos)));
	}
	return ms.AsSpan();
}

static uint8_t DecodeOp(Opcode opcode) {
    if (opcode == Opcode::OP_0)
        return 0;
    if (int(opcode) >= int(Opcode::OP_1) && int(opcode) <= int(Opcode::OP_16))
        return int(opcode) - int(Opcode::OP_1) + 1;
    Throw(E_FAIL);
}

// Return <program, version>
pair<Span, uint8_t> Script::WitnessProgram(RCSpan pk) {
    return between(pk.size(), size_t(4), MAX_WITNESS_PROGRAM + 2)
				&& (pk[0] == (uint8_t)Opcode::OP_0 || pk[0] >= (uint8_t)Opcode::OP_1 && pk[0] <= (uint8_t)Opcode::OP_16) && pk[1] + 2 == pk.size()
        ? make_pair(pk.subspan(2), DecodeOp((Opcode)pk[0]))
        : pair<Span, uint8_t>();
}

bool Script::IsPayToScriptHash(RCSpan pk) {
	return pk.size() == 23 && pk[0] == (uint8_t)Opcode::OP_HASH160 && pk[1] == 20 && pk[22] == (uint8_t)Opcode::OP_EQUAL;
}

bool Script::IsPushOnly(RCSpan script) {
    CMemReadStream stm(script);
	for (LiteInstr instr; stm.Position < stm.Length;) {
		if (!GetInstr(stm, instr))
			return false;
		if (instr.OriginalOpcode > Opcode::OP_16)
			return false;
	}
	return true;
}

// binary, VC don't allow regex for binary data
//!!!O Obsolete, replace by TxOut::CheckStandardType()
uint8_t TryParseDestination(RCSpan pkScript, Span& hash160, Span& pubkey) {
	const uint8_t* p = pkScript.data();
	if (pkScript.size() < 4 || p[pkScript.size() - 1] != (uint8_t)Opcode::OP_CHECKSIG)
		return 0;
	if (pkScript.size() == 25) {
		if (p[0] == (uint8_t)Opcode::OP_DUP && p[1] == (uint8_t)Opcode::OP_HASH160 && p[2] == 20 && p[pkScript.size() - 2] == (uint8_t)Opcode::OP_EQUALVERIFY) {
			hash160 = Span(p + 3, 20);
			return 20;
		}
	} else if (p[0] == 33 || p[0] == 65) {
		uint8_t len = p[0];
		if (pkScript.size() == 1 + len + 1) {
			hash160 = Hash160(pubkey = Span(p + 1, len));
			return len;
		}
	}
	return 0;
}

Address TxOut::CheckStandardType(RCSpan scriptPubKey) {
	CoinEng& eng = Eng();
	if (Script::IsPayToScriptHash(scriptPubKey))
		return Address(eng, AddressType::P2SH, scriptPubKey.subspan(2, 20));

	auto pp = Script::WitnessProgram(scriptPubKey);
	if (pp.first.data()) {
		switch (pp.second) {
		case 0:
			return pp.first.size() == WITNESS_V0_KEYHASH_SIZE ? Address(eng, AddressType::WitnessV0KeyHash, pp.first)
				: pp.first.size() == WITNESS_V0_SCRIPTHASH_SIZE ? Address(eng, AddressType::WitnessV0ScriptHash, pp.first)
				: Address(eng, AddressType::NonStandard, Span());
		default:
			Address r(eng, AddressType::WitnessUnknown, pp.first);
			r->WitnessVer = pp.second;
			return r;
		}
	}

	if (!scriptPubKey.empty()) {
		if (scriptPubKey[0] == uint8_t(Opcode::OP_RETURN) && Script::IsPushOnly(scriptPubKey.subspan(1)))
			return Address(eng, AddressType::NullData, Span());

		Span hash160, pubkey;
		auto sz = TryParseDestination(scriptPubKey, hash160, pubkey);
		if (sz == 33 || sz == 65)
			return Address(eng, AddressType::PubKey, pubkey);
		if (sz == 20)
			return Address(eng, AddressType::P2PKH, hash160);

		CMemReadStream stm(scriptPubKey);
		LiteInstr instr;
		if (GetInstr(stm, instr)) {
			if (!instr.IsSmallPositiveOpcode())
				return Address(eng, AddressType::NonStandard, Span());
			Address r(eng, AddressType::MultiSig, Span());
			r->RequiredSigs = DecodeOp(instr.OriginalOpcode);

			while (true) {
				if (!GetInstr(stm, instr) || instr.Opcode != Opcode::OP_PUSHDATA1)
					return Address(eng, AddressType::NonStandard, Span());
				if (instr.Value.size() > CanonicalPubKey::MAX_SIZE)
					break;
				CanonicalPubKey pubkey(instr.Value);
				if (!pubkey.IsValid())
					break;
				r->Datas.push_back(instr.Value);
			}
			if (instr.IsSmallPositiveOpcode()
					&& DecodeOp(instr.OriginalOpcode) == r->Datas.size()
					&& r->Datas.size() >= r->RequiredSigs
					&& GetInstr(stm, instr)
					&& instr.Opcode == Opcode::OP_CHECKMULTISIG
					&& !GetInstr(stm, instr))
				return r;
		}
	}

	return Address(eng, AddressType::NonStandard, Span());
}

SignatureHasher::SignatureHasher(const TxObj& txoTo)
	: m_txoTo(txoTo)
	, m_amount(0)
	, HashType(SigHashType::ZERO)
	, m_bWitness(false)
{
}

void SignatureHasher::CalcWitnessCache() {
	if (m_hashPrevOuts)
		return;
	CoinEng& eng = Eng();
	MemoryStream msPO, msSeq, msOut;
	BinaryWriter wrPO(msPO), wrSeq(msSeq), wrOut(msOut);
	for (auto& txIn : m_txoTo.TxIns()) {
		txIn.PrevOutPoint.Write(wrPO);
		wrSeq << txIn.Sequence;
	}
	m_hashPrevOuts = eng.HashForSignature(msPO);
	m_hashSequence = eng.HashForSignature(msSeq);

	for (auto& txOut : m_txoTo.TxOuts)
		wrOut << txOut;
	m_hashOuts = eng.HashForSignature(msOut);
}

static const HashValue s_hashvalueOne("0000000000000000000000000000000000000000000000000000000000000001");

HashValue SignatureHasher::HashForSig(RCSpan script) {
	CoinEng& eng = Eng();

	MemoryStream stm;
	ProtocolWriter wr(stm);
	wr.HashTypeNone = (HashType & SigHashType::MASK) == SigHashType::SIGHASH_NONE;
	wr.HashTypeAnyoneCanPay = bool(HashType & SigHashType::SIGHASH_ANYONECANPAY);
	wr.HashTypeSingle = (HashType & SigHashType::MASK) == SigHashType::SIGHASH_SINGLE;

	if (m_bWitness) {		// BIP143
		wr << m_txoTo.Ver
			<< (wr.HashTypeAnyoneCanPay ? HashValue() : m_hashPrevOuts)
			<< (wr.HashTypeAnyoneCanPay || wr.HashTypeNone || wr.HashTypeSingle ? HashValue() : m_hashSequence);

		auto& txIn = m_txoTo.TxIns()[NIn];
		txIn.PrevOutPoint.Write(wr);
		CoinSerialized::WriteSpan(wr, script);
		wr << m_amount << txIn.Sequence;

		if (!wr.HashTypeNone && !wr.HashTypeSingle)
			wr << m_hashOuts;
		else if (wr.HashTypeSingle && NIn < m_txoTo.TxOuts.size()) {
			MemoryStream msOut;
			BinaryWriter wrOut(msOut);
			wrOut << m_txoTo.TxOuts[NIn];
			wr << eng.HashForSignature(msOut);
		} else
			wr << HashValue();

		wr << m_txoTo.LockBlock;
	} else if (wr.HashTypeSingle && NIn >= m_txoTo.TxOuts.size())
		return s_hashvalueOne;
	else {
		wr.WitnessAware = false;
		wr.ForSignatureHash = true;
		uint8_t opcode = (uint8_t)Opcode::OP_CODESEPARATOR;
		Blob clearedScript = Script::DeleteSubpart(script, Span(&opcode, 1));
		wr.ClearedScript = clearedScript;
		wr.NIn = NIn;
		m_txoTo.Write(wr);
	}

	wr << uint32_t(HashType);
	return eng.HashForSignature(stm);
}

// BIP141
bool SignatureHasher::VerifyWitnessProgram(Vm& vm, uint8_t witnessVer, RCSpan witnessProgram) {
	switch (witnessVer) {
	case 0: {
		auto& witness = m_txoTo.TxIns()[NIn].Witness;
		if (witness.empty())
			throw WitnessProgramException(CoinErr::SCRIPT_ERR_WITNESS_PROGRAM_WITNESS_EMPTY);
		Blob scriptPubKey;
		switch (witnessProgram.size()) {
		case WITNESS_V0_KEYHASH_SIZE: {
				if ((vm.Stack = witness).size() != 2)
					throw WitnessProgramException(CoinErr::SCRIPT_ERR_WITNESS_PROGRAM_MISMATCH);
				MemoryStream ms;
				ScriptWriter(ms) << Opcode::OP_DUP << Opcode::OP_HASH160 << witnessProgram << Opcode::OP_EQUALVERIFY << Opcode::OP_CHECKSIG;		//!!!Optimze to check without intermediate script
				scriptPubKey = ms;
			}
			break;
		case WITNESS_V0_SCRIPTHASH_SIZE:
			vm.Stack = vector<StackValue>(witness.begin(), witness.end() - 1);
			if (HashValue(SHA256().ComputeHash(scriptPubKey = witness.back())) != HashValue(witnessProgram))
				throw WitnessProgramException(CoinErr::SCRIPT_ERR_WITNESS_PROGRAM_MISMATCH);
			break;
		default:
			Throw(CoinErr::SCRIPT_ERR_WITNESS_PROGRAM_WRONG_LENGTH);
		}

		for (auto& a : vm.Stack)
			if (a.size() > MAX_SCRIPT_ELEMENT_SIZE)
				Throw(CoinErr::SCRIPT_ERR_PUSH_SIZE);

		CBoolKeeper witnessKeeper(m_bWitness, true);
		bool r = vm.Eval(scriptPubKey);
		if (r) {
			if (vm.Stack.size() != 1)
				Throw(CoinErr::SCRIPT_ERR_CLEANSTACK);
			if (!ToBool(vm.Stack.back()))
				Throw(CoinErr::SCRIPT_ERR_EVAL_FALSE);
		}
		return r;
	}
	default:
		if (t_scriptPolicy.DiscourageUpgradableWidnessProgram)
			Throw(CoinErr::SCRIPT_ERR_DISCOURAGE_UPGRADABLE_WITNESS_PROGRAM);
		return true;
	}
}

bool SignatureHasher::VerifyScript(RCSpan scriptSig, Span scriptPk) {
	Vm vm(this);
	bool r = vm.Eval(scriptSig);
	if (r) {
		bool bP2SH = t_features.PayToScriptHash && Script::IsPayToScriptHash(scriptPk);

		StackValue pubKeySerialized;
		if (bP2SH) {
			if (!Script::IsPushOnly(scriptSig))
				Throw(CoinErr::SCRIPT_ERR_SIG_PUSHONLY);
			if (!vm.FastVerifyP2SH(HashValue160(scriptPk.subspan(2, 20))))
				return false;
			pubKeySerialized = vm.Stack.back();
			vm.Stack.pop_back();
			scriptPk = pubKeySerialized;
		}
		if (r = (vm.Eval(scriptPk) && !vm.Stack.empty() && ToBool(vm.Stack.back()))) {
			auto pp = Script::WitnessProgram(scriptPk);

			if (t_features.SegWit && pp.first.data()) {
				if (!bP2SH && !scriptSig.empty())
					Throw(CoinErr::SCRIPT_ERR_WITNESS_MALLEATED);
				if (!(r = VerifyWitnessProgram(vm, pp.second, pp.first)))
					return false;
				vm.Stack.resize(1);
			}

			if (t_scriptPolicy.CleanStack && vm.Stack.size() != 1) {
				TRC(2, "SCRIPT_ERR_CLEANSTACK " << m_txoTo.m_hash);
				Throw(CoinErr::SCRIPT_ERR_CLEANSTACK);
			}
		}
	}
	return r;
}

const OutPoint& SignatureHasher::GetOutPoint() const {
	return m_txoTo.TxIns().at(NIn).PrevOutPoint;
}

void SignatureHasher::VerifySignature(RCSpan scriptPk) {
	const TxIn& txIn = m_txoTo.TxIns().at(NIn);
    const OutPoint& outPoint = txIn.PrevOutPoint;
	if (!VerifyScript(txIn.Script(), scriptPk)) {
#ifdef _DEBUG//!!!D
//		cout << m_txoTo.m_hash << endl;
//		cout << outPoint.TxHash << endl;
		//		bool bC = outPoint.TxHash == hashTxFrom;
		bool bb = VerifyScript(txIn.Script(), scriptPk);
//		bb = bb;
#endif
		Throw(CoinErr::VerifySignatureFailed);
	}
}

bool CoinEng::VerifyHash(RCSpan pubKey, const HashValue& hash, RCSpan sig) {
#if UCFG_COIN_ECC=='S'
	Sec256DsaEx dsa;
	dsa.ParsePubKey(pubKey);
#else
	ECDsa dsa(CngKey::Import(pubKey, CngKeyBlobFormat::OSslEccPublicBlob));
#endif
	return dsa.VerifyHash(hash.ToSpan(), sig);
}

bool SignatureHasher::CheckSig(Span sig, RCSpan pubKey, RCSpan script, bool bInMultiSig) {
	CoinEng& eng = Eng();

	if (sig.empty())
		return false;
	SigHashType sigHashType = SigHashType(sig[sig.size() - 1]);
	sig = sig.first(sig.size() - 1);

	if (t_features.VerifyDerEnc && !eng.IsValidSignatureEncoding(sig))
		return false;

	try {
		DBG_LOCAL_IGNORE_CONDITION(ExtErr::Crypto);
#define EC_R_INVALID_ENCODING 102 // OpenSSL
		DBG_LOCAL_IGNORE(MAKE_HRESULT(SEVERITY_ERROR, FACILITY_OPENSSL, EC_R_INVALID_ENCODING));

		eng.CheckHashType(_self, (uint32_t)sigHashType);
		if (HashType != SigHashType::ZERO && HashType != sigHashType)
			return false;
		HashType = sigHashType;
		return eng.VerifyHash(pubKey, HashForSig(script), sig);
	} catch (CryptoException& DBG_PARAM(ex)) {
		if (!bInMultiSig) {
			TRC(2, ex.what() << "    PubKey: " << pubKey);
		}
		return false;
	}
}

bool ToBool(const VmStack::Value& v) {
	Span s(v);
	for (size_t i = 0; i < s.size(); ++i)
		if (s[i])
			return i != s.size() - 1 || s[i] != 0x80;
	return false;
}

//!!! not used after disabling OP_AND
/*
static void MakeSameSize(VmStack::Value& v1, VmStack::Value& v2) {
	size_t size = std::max(v1.size(), v2.size());
	v1.resize(size);
	v2.resize(size);
}
*/

bool CoinEng::IsCheckSigOp(Opcode opcode) {
	return opcode == Opcode::OP_CHECKSIG || opcode == Opcode::OP_CHECKSIGVERIFY;
}

void CoinEng::OpCat(VmStack& stack) {
	Throw(CoinErr::SCRIPT_DISABLED_OPCODE);
}

void CoinEng::OpSubStr(VmStack& stack) {
	Throw(CoinErr::SCRIPT_DISABLED_OPCODE);
/*
	StackValue v = stack[2];
	int beg = explicit_cast<int>(ToBigInteger(stack[1])), end = beg + explicit_cast<int>(ToBigInteger(stack[0]));
	if (beg < 0 || end < beg)
		Throw(CoinErr::SCRIPT_DISABLED_OPCODE);
	beg = std::min(beg, (int)v.size());
	end = std::min(end, (int)v.size());
	stack[2] = StackValue(Span(v).subspan(beg, end - beg));
	stack.SkipStack(2);
*/
}

void CoinEng::OpLeft(VmStack& stack) {
	Throw(CoinErr::SCRIPT_DISABLED_OPCODE);
/*
	int size = explicit_cast<int>(ToBigInteger(stack.Pop()));
	StackValue& v = stack[0];
	if (size < 0)
		Throw(CoinErr::SCRIPT_INVALID_ARG);
	size = std::min(size, (int)v.size());
	v = StackValue(Span(v).first(size));
*/
}

void CoinEng::OpRight(VmStack& stack) {
	Throw(CoinErr::SCRIPT_DISABLED_OPCODE);
/*
	int size = explicit_cast<int>(ToBigInteger(stack.Pop()));
	StackValue& v = stack[0];
	if (size < 0)
		Throw(CoinErr::SCRIPT_INVALID_ARG);
	size = std::min(size, (int)v.size());
	v = StackValue(Span(v).last(size));
*/
}

void CoinEng::OpSize(VmStack& stack) {
	stack.Push(VmStack::FromBigInteger(BigInteger((int64_t)stack[0].size())));
}

void CoinEng::OpInvert(VmStack& stack) {
	Throw(CoinErr::SCRIPT_DISABLED_OPCODE);
/*
	for (uint8_t* p = stack[0].data(), *q = p + stack[0].size(); p < q; ++p)
		*p = ~*p;
*/
}

void CoinEng::OpAnd(VmStack& stack) {
	Throw(CoinErr::SCRIPT_DISABLED_OPCODE);
}

void CoinEng::OpOr(VmStack& stack) {
	Throw(CoinErr::SCRIPT_DISABLED_OPCODE);
}

void CoinEng::OpXor(VmStack& stack) {
	Throw(CoinErr::SCRIPT_DISABLED_OPCODE);
}

void CoinEng::Op2Mul(VmStack& stack) {
	Throw(CoinErr::SCRIPT_DISABLED_OPCODE);
/*
	stack[0] = FromBigInteger(ToBigInteger(stack[0]) << 1);
*/
}

void CoinEng::Op2Div(VmStack& stack) {
	Throw(CoinErr::SCRIPT_DISABLED_OPCODE);
/*
	stack[0] = FromBigInteger(ToBigInteger(stack[0]) >> 1);
*/
}

void CoinEng::OpMul(VmStack& stack) {
	Throw(CoinErr::SCRIPT_DISABLED_OPCODE);
/*
	stack[1] = FromBigInteger(ToBigInteger(stack[1]) * ToBigInteger(stack[0]));
	stack.SkipStack(1);
*/
}

void CoinEng::OpDiv(VmStack& stack) {
	Throw(CoinErr::SCRIPT_DISABLED_OPCODE);
}

void CoinEng::OpMod(VmStack& stack) {
	Throw(CoinErr::SCRIPT_DISABLED_OPCODE);
}

void CoinEng::OpLShift(VmStack& stack) {
	Throw(CoinErr::SCRIPT_DISABLED_OPCODE);
/*
	stack[1] = FromBigInteger(ToBigInteger(stack[1]) << explicit_cast<int>(ToBigInteger(stack[0])));
	stack.SkipStack(1);
*/
}

void CoinEng::OpRShift(VmStack& stack) {
	Throw(CoinErr::SCRIPT_DISABLED_OPCODE);
/*
	stack[1] = FromBigInteger(ToBigInteger(stack[1]) >> explicit_cast<int>(ToBigInteger(stack[0])));
	stack.SkipStack(1);
*/
}

void CoinEng::OpCheckDataSig(VmStack& stack) {
	Throw(CoinErr::SCRIPT_DISABLED_OPCODE);
}

void CoinEng::OpCheckDataSigVerify(VmStack& stack) {
	Throw(CoinErr::SCRIPT_DISABLED_OPCODE);
}

bool Vm::EvalImp() {
	CoinEng& eng = Eng();

	bool r = true;
	vector<bool> vExec;
	vector<Value> altStack;
	int nOpCount = 0;
	for (LiteInstr instr; GetInstr(m_rd->Stm, instr);) {
		if (instr.Value.size() > MAX_SCRIPT_ELEMENT_SIZE)
			Throw(CoinErr::SCRIPT_ERR_PUSH_SIZE);
		if (instr.Opcode > Opcode::OP_16 && ++nOpCount > MAX_OP_COUNT)
			return false;
		bool bExec = !count(vExec.begin(), vExec.end(), false);
		switch (instr.Opcode) {
		case Opcode::OP_IF: vExec.push_back(bExec ? ToBool(Pop()) : false); break;
		case Opcode::OP_NOTIF: vExec.push_back(bExec ? !ToBool(Pop()) : false); break;
		case Opcode::OP_ELSE:
			if (vExec.empty())
				return false;
			vExec.back() = !vExec.back();
			break;
		case Opcode::OP_ENDIF:
			if (vExec.empty())
				return false;
			vExec.pop_back();
			break;
		default:
			if (bExec) {
				switch (instr.Opcode) {
				case Opcode::OP_PUSHDATA1: Push(StackValue(instr.Value));
				case Opcode::OP_NOP: break;
				case Opcode::OP_DUP: Push(GetStack(0)); break;
				case Opcode::OP_2DUP:
					Push(GetStack(1));
					Push(GetStack(1));
					break;
				case Opcode::OP_3DUP:
					Push(GetStack(2));
					Push(GetStack(2));
					Push(GetStack(2));
					break;
				case Opcode::OP_2OVER:
					Push(GetStack(3));
					Push(GetStack(3));
					break;
				case Opcode::OP_DROP: SkipStack(1); break;
				case Opcode::OP_2DROP: SkipStack(2); break;
				case Opcode::OP_NIP:
					GetStack(1) = GetStack(0);
					SkipStack(1);
					break;
				case Opcode::OP_OVER: Push(GetStack(1)); break;
				case Opcode::OP_SWAP: std::swap(GetStack(0), GetStack(1)); break;
				case Opcode::OP_2SWAP:
					std::swap(GetStack(3), GetStack(1));
					std::swap(GetStack(2), GetStack(0));
					break;
				case Opcode::OP_ROT:
					std::swap(GetStack(2), GetStack(1));
					std::swap(GetStack(1), GetStack(0));
					break;
				case Opcode::OP_2ROT: {
					Value v1 = GetStack(5), v2 = GetStack(4);
					Stack.erase(Stack.end() - 6, Stack.end() - 4);
					Stack.push_back(v1);
					Stack.push_back(v2);
				} break;
				case Opcode::OP_TUCK: {
					if (Stack.size() < 2)
						Throw(CoinErr::SCRIPT_ERR_INVALID_STACK_OPERATION);
					Value v = GetStack(0);
					Stack.insert(Stack.end() - 2, v);
				} break;
				case Opcode::OP_PICK: {
					int n = explicit_cast<int>(ToBigInteger(Pop()));
					Push(GetStack(n));
				} break;
				case Opcode::OP_ROLL: {
					int n = explicit_cast<int>(ToBigInteger(Pop()));
					Value v = GetStack(n);
					Stack.erase(Stack.end() - 1 - n);
					Push(v);
				} break;
				case Opcode::OP_CAT:	eng.OpCat(_self);		break;
				case Opcode::OP_SUBSTR: eng.OpSubStr(_self);	break;
				case Opcode::OP_LEFT:	eng.OpLeft(_self);		break;
				case Opcode::OP_RIGHT:	eng.OpRight(_self);		break;
				case Opcode::OP_INVERT: eng.OpInvert(_self);	break;
				case Opcode::OP_AND:	eng.OpAnd(_self);		break;
				case Opcode::OP_OR:		eng.OpOr(_self);		break;
				case Opcode::OP_XOR:	eng.OpXor(_self);		break;
				case Opcode::OP_LSHIFT:	eng.OpLShift(_self);	break;
				case Opcode::OP_RSHIFT:	eng.OpRShift(_self);	break;
				case Opcode::OP_2MUL:	eng.Op2Mul(_self);		break;
				case Opcode::OP_2DIV:	eng.Op2Div(_self);		break;
				case Opcode::OP_MUL:	eng.OpMul(_self);		break;
				case Opcode::OP_DIV:	eng.OpDiv(_self);		break;
				case Opcode::OP_MOD:	eng.OpMod(_self);		break;
				case Opcode::OP_SIZE:	eng.OpSize(_self);		break;
				case Opcode::OP_CHECKDATASIG:		eng.OpCheckDataSig(_self);			break;
				case Opcode::OP_CHECKDATASIGVERIFY: eng.OpCheckDataSigVerify(_self);	break;					

				case Opcode::OP_RIPEMD160: GetStack(0) = RIPEMD160().ComputeHash(GetStack(0)); break;
				case Opcode::OP_SHA256: GetStack(0) = SHA256().ComputeHash(GetStack(0)); break;
				case Opcode::OP_SHA1: GetStack(0) = SHA1().ComputeHash(GetStack(0)); break;
				case Opcode::OP_HASH160: GetStack(0) = Hash160(GetStack(0)); break;
				case Opcode::OP_HASH256: GetStack(0) = Eng().HashMessage(GetStack(0)).ToSpan(); break;
				case Opcode::OP_VERIFY:
					if (!ToBool(Pop()))
						return false;
					break;
				case Opcode::OP_IFDUP:
					if (ToBool(GetStack(0)))
						Push(GetStack(0));
					break;
				case Opcode::OP_EQUAL: Push(Pop() == Pop() ? TrueValue : FalseValue); break;
				case Opcode::OP_CHECKLOCKTIMEVERIFY:
					if (t_features.CheckLocktimeVerify) {
						auto n = ToBigInteger(GetStack(0));
						if (n < 0 || Tx::LocktimeTypeOf(m_signatureHasher->m_txoTo.LockBlock) != (n >= BigInteger(LOCKTIME_THRESHOLD) ? Tx::LocktimeType::Timestamp : Tx::LocktimeType::Block) || n > BigInteger(m_signatureHasher->m_txoTo.LockBlock))
							return false;
					}
					break;
				case Opcode::OP_CHECKSEQUENCEVERIFY:
					if (t_features.CheckSequenceVerify) {
						auto bn = ToBigInteger(GetStack(0));
						if (bn < 0)
							return false;
						int64_t n;
						bn.And(BigInteger(0xFFFFFFFFL)).AsInt64(n);
						if (n & TxIn::SEQUENCE_LOCKTIME_DISABLE_FLAG)
							return false;
						auto& txIn = m_signatureHasher->m_txoTo.TxIns()[m_signatureHasher->NIn];
						if (m_signatureHasher->m_txoTo.Ver < 2 || (txIn.Sequence & TxIn::SEQUENCE_LOCKTIME_DISABLE_FLAG))
							return false;
						uint32_t nM = n & (TxIn::SEQUENCE_LOCKTIME_TYPE_FLAG | TxIn::SEQUENCE_LOCKTIME_MASK),
                            toSeqM = txIn.Sequence & (TxIn::SEQUENCE_LOCKTIME_TYPE_FLAG | TxIn::SEQUENCE_LOCKTIME_MASK);
						if (nM < TxIn::SEQUENCE_LOCKTIME_TYPE_FLAG && toSeqM >= TxIn::SEQUENCE_LOCKTIME_TYPE_FLAG
                                || nM >= TxIn::SEQUENCE_LOCKTIME_TYPE_FLAG && toSeqM < TxIn::SEQUENCE_LOCKTIME_TYPE_FLAG
                                || nM > toSeqM)
                            return false;
					}
					break;
				case Opcode::OP_EQUALVERIFY:
					if (Pop() != Pop()) {
						r = false;
						goto LAB_RET;
					}
					break;
				case Opcode::OP_CHECKSIG:
				case Opcode::OP_CHECKSIGVERIFY: {
					Value key = Pop(), sig = Pop();
					MemoryStream msSig;
					ScriptWriter(msSig) << Span(sig);

					Span spanScript = m_span.subspan(m_posCodeHash);
					Blob script;
					if (WitnessSig)
						script = spanScript;
					else {
						script = Script::DeleteSubpart(spanScript, msSig);
						//!!!TODO check
					}
					SigHashType originalHashType = m_signatureHasher->HashType;
					bool bOk = m_signatureHasher->CheckSig(sig, key, script);
					m_signatureHasher->HashType = originalHashType;	//!!!?
					if (Opcode::OP_CHECKSIG == instr.Opcode)
						Push(bOk ? TrueValue : FalseValue);
					else if (!bOk)
						return false;
				} break;
				case Opcode::OP_CHECKMULTISIG:
				case Opcode::OP_CHECKMULTISIGVERIFY: {
					vector<StackValue> vPubKey;
					vector<StackValue> vSig;
					int n = explicit_cast<int>(ToBigInteger(Pop()));
					if (n < 0 || n > 20 || (nOpCount += n) > MAX_OP_COUNT)
						return false;
					for (int i = 0; i < n; ++i)
						vPubKey.push_back(Pop());
					n = explicit_cast<int>(ToBigInteger(Pop()));
					if (n < 0 || n > vPubKey.size())
						return false;
					for (int i = 0; i < n; ++i)
						vSig.push_back(Pop());
					SkipStack(1); // one extra unused value, due to an old bug
					Blob script(m_span.subspan(m_posCodeHash));
					if (!WitnessSig) {
						EXT_FOR(const StackValue& sig, vSig) {
							MemoryStream ms;
							ScriptWriter(ms) << Span(sig);
							script = Script::DeleteSubpart(script, ms);
							//!!!TODO check
						}
					}
					bool b = true;
					SigHashType originalHashType = m_signatureHasher->HashType;
					for (int i = 0, j = 0; i < vSig.size() && (b = vSig.size() - i <= vPubKey.size() - j); ++j) {
						m_signatureHasher->HashType = originalHashType;	//!!!?
						if (m_signatureHasher->CheckSig(vSig[i], vPubKey[j], script, true))
							++i;
					}
					m_signatureHasher->HashType = originalHashType;	//!!!?
					if (Opcode::OP_CHECKMULTISIG == instr.Opcode)
						Push(b ? TrueValue : FalseValue);
					else if (!b)
						return false;
				} break;
				case Opcode::OP_ADD: {
					BigInteger a = ToBigInteger(GetStack(0)), b = ToBigInteger(GetStack(1));
					GetStack(1) = FromBigInteger(a + b);
					SkipStack(1);
				} break;
				case Opcode::OP_SUB:
					GetStack(1) = FromBigInteger(ToBigInteger(GetStack(1)) - ToBigInteger(GetStack(0)));
					SkipStack(1);
					break;
				case Opcode::OP_BOOLAND:
					GetStack(1) = FromBigInteger(!!ToBigInteger(GetStack(1)) && !!ToBigInteger(GetStack(0)));
					SkipStack(1);
					break;
				case Opcode::OP_BOOLOR:
					GetStack(1) = FromBigInteger(!!ToBigInteger(GetStack(1)) || !!ToBigInteger(GetStack(0)));
					SkipStack(1);
					break;
				case Opcode::OP_NUMEQUAL:
					GetStack(1) = FromBigInteger(ToBigInteger(GetStack(1)) == ToBigInteger(GetStack(0)));
					SkipStack(1);
					break;
				case Opcode::OP_NUMEQUALVERIFY:
					if (ToBigInteger(GetStack(1)) != ToBigInteger(GetStack(0)))
						return false;
					SkipStack(2);
					break;
				case Opcode::OP_NUMNOTEQUAL:
					GetStack(1) = FromBigInteger(ToBigInteger(GetStack(1)) != ToBigInteger(GetStack(0)));
					SkipStack(1);
					break;
				case Opcode::OP_LESSTHAN:
					GetStack(1) = FromBigInteger(ToBigInteger(GetStack(1)) < ToBigInteger(GetStack(0)));
					SkipStack(1);
					break;
				case Opcode::OP_GREATERTHAN:
					GetStack(1) = FromBigInteger(ToBigInteger(GetStack(1)) > ToBigInteger(GetStack(0)));
					SkipStack(1);
					break;
				case Opcode::OP_LESSTHANOREQUAL:
					GetStack(1) = FromBigInteger(ToBigInteger(GetStack(1)) <= ToBigInteger(GetStack(0)));
					SkipStack(1);
					break;
				case Opcode::OP_GREATERTHANOREQUAL:
					GetStack(1) = FromBigInteger(ToBigInteger(GetStack(1)) >= ToBigInteger(GetStack(0)));
					SkipStack(1);
					break;
				case Opcode::OP_WITHIN: {
					BigInteger a = ToBigInteger(GetStack(2)), b = ToBigInteger(GetStack(1)), c = ToBigInteger(GetStack(0));
					SkipStack(2);
					GetStack(0) = b <= a && a < c ? TrueValue : FalseValue;
				} break;
				case Opcode::OP_MIN:
					GetStack(1) = FromBigInteger(std::min(ToBigInteger(GetStack(1)), ToBigInteger(GetStack(0))));
					SkipStack(1);
					break;
				case Opcode::OP_MAX:
					GetStack(1) = FromBigInteger(std::max(ToBigInteger(GetStack(1)), ToBigInteger(GetStack(0))));
					SkipStack(1);
					break;
				case Opcode::OP_1ADD: GetStack(0) = FromBigInteger(ToBigInteger(GetStack(0)) + 1); break;
				case Opcode::OP_1SUB: GetStack(0) = FromBigInteger(ToBigInteger(GetStack(0)) - 1); break;
				case Opcode::OP_NEGATE: GetStack(0) = FromBigInteger(-ToBigInteger(GetStack(0))); break;
				case Opcode::OP_ABS: GetStack(0) = FromBigInteger(abs(ToBigInteger(GetStack(0)))); break;
				case Opcode::OP_NOT: GetStack(0) = FromBigInteger(!ToBigInteger(GetStack(0))); break;
				case Opcode::OP_0NOTEQUAL: GetStack(0) = FromBigInteger(!!ToBigInteger(GetStack(0))); break;
				case Opcode::OP_DEPTH: Push(FromBigInteger(BigInteger((int64_t)Stack.size()))); break;
				case Opcode::OP_CODESEPARATOR: m_posCodeHash = (int)m_stm->Position; break;
				case Opcode::OP_RETURN: return false;
				case Opcode::OP_TOALTSTACK: altStack.push_back(Pop()); break;
				case Opcode::OP_FROMALTSTACK:
					if (altStack.empty())
						return false;
					Push(altStack.back());
					altStack.pop_back();
					break;
				default: Throw(E_NOTIMPL);
				}
			}
		}
	}
LAB_RET:
	return r;
}

void Vm::Init(RCSpan mbScript) {
	m_span = mbScript;
	m_stm.reset(new CMemReadStream(m_span));
	m_rd.reset(new ScriptReader(*m_stm));
	m_pc = 0;
	m_posCodeHash = 0;
}

bool Vm::Eval(RCSpan mbScript) {
    if (mbScript.size() > MAX_SCRIPT_SIZE)
        Throw(CoinErr::SCRIPT_ERR_SCRIPT_SIZE);
	Init(mbScript);
	try {
		return EvalImp();
	} catch (RCExc) {
		return false;
	}
}

const double LN2 = log(2.), MINUS_ONE_DIV_LN2SQUARED = -1 / (LN2 * LN2);

CoinFilter::CoinFilter(int nElements, double falsePostitiveRate, uint32_t tweak, uint8_t flags)
	: Tweak(tweak)
	, Flags(flags) {
	Bitset.resize(8 * min(MAX_BLOOM_FILTER_SIZE, size_t(MINUS_ONE_DIV_LN2SQUARED * nElements * log(falsePostitiveRate)) / 8));
	ASSERT((Bitset.size() & 7) == 0);
	HashNum = min(MAX_HASH_FUNCS, int(Bitset.size() / nElements * LN2));
}

size_t CoinFilter::Hash(RCSpan cbuf, int n) const {
	return MurmurHash3_32(cbuf, n * 0xFBA4C795 + Tweak) % Bitset.size();
}

Span CoinFilter::FindScriptData(RCSpan script) const {
	CMemReadStream stm(script);
	for (LiteInstr instr; GetInstr(stm, instr);)
		if (instr.Opcode == Opcode::OP_PUSHDATA1 && instr.Value.size() > 0 && Contains(instr.Value))
			return instr.Value;
	return Span();
}

void CoinFilter::Write(BinaryWriter& wr) const {
	vector<uint8_t> v;
	to_block_range(Bitset, back_inserter(v));
	CoinSerialized::WriteSpan(wr, Blob(&v[0], v.size()));
	wr << uint32_t(HashNum) << Tweak << Flags;
}

void CoinFilter::Read(const BinaryReader& rd) {
	Blob data = CoinSerialized::ReadBlob(rd);
	Bitset.resize(data.size() * 8);
	for (int i = 0; i < data.size(); ++i)
		for (int j = 0; j < 8; ++j)
			Bitset.replace(i * 8 + j, (data.constData()[i] >> j) & 1);
	HashNum = rd.ReadUInt32();
	rd >> Tweak >> Flags;
}

bool CoinFilter::IsRelevantAndUpdate(const Tx& tx) {
	HashValue hashTx = Coin::Hash(tx);
	bool bFound = Contains(hashTx);
	for (int i = 0; i < tx.TxOuts().size(); ++i) {
		const TxOut& txOut = tx.TxOuts()[i];
		Span data = FindScriptData(txOut.get_ScriptPubKey());
		if (data.data()) {
			bFound = true;
			OutPoint oi(hashTx, i);
			switch (Flags & BLOOM_UPDATE_MASK) {
			case BLOOM_UPDATE_ALL:
				Insert(oi);
				break;
			case BLOOM_UPDATE_P2PUBKEY_ONLY:
				Address a = TxOut::CheckStandardType(txOut.ScriptPubKey);
				switch (a.Type) {
				case AddressType::PubKey:
					Insert(oi);
					break;
				case AddressType::MultiSig:
					//!!! TODO: TX_MULTISIG
					break;
				}
				break;
			}
			return true;
		}
	}
	if (bFound)
		return true;
	EXT_FOR(const TxIn& txIn, tx.TxIns()) {
		if (Contains(txIn.PrevOutPoint) || FindScriptData(txIn.Script()).data())
			return true;
	}
	return false;
}

} // namespace Coin
