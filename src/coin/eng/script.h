/*######   Copyright (c) 2013-2019 Ufasoft  http://ufasoft.com  mailto:support@ufasoft.com,  Sergey Pavlov  mailto:dev@ufasoft.com ####
#                                                                                                                                     #
# 		See LICENSE for licensing information                                                                                         #
#####################################################################################################################################*/

#pragma once

#include <el/bignum.h>

#include "coin-model.h"
#include "../util/opcode.h"

namespace Coin {

class Tx;
class Script;
class Address;

const unsigned
    WITNESS_V0_SCRIPTHASH_SIZE = 32,
    WITNESS_V0_KEYHASH_SIZE = 20;

class ScriptWriter : public BinaryWriter {
	typedef BinaryWriter base;
public:
	ScriptWriter(Stream& stm)
		: base(stm)
	{}
};

inline ScriptWriter& operator<<(ScriptWriter& wr, uint8_t v) {
	(BinaryWriter&)wr << v;
	return wr;
}

inline ScriptWriter& operator<<(ScriptWriter& wr, Opcode op) {
	(BinaryWriter&)wr << uint8_t(op);
	return wr;
}

inline ScriptWriter& operator<<(ScriptWriter& wr, uint16_t v) {
	(BinaryWriter&)wr << v;
	return wr;
}

inline ScriptWriter& operator<<(ScriptWriter& wr, int32_t v) {
	(BinaryWriter&)wr << v;
	return wr;
}

inline ScriptWriter& operator<<(ScriptWriter& wr, uint32_t v) {
	(BinaryWriter&)wr << v;
	return wr;
}

inline ScriptWriter& operator<<(ScriptWriter& wr, uint64_t v) {
	(BinaryWriter&)wr << v;
	return wr;
}

inline ScriptWriter& operator<<(ScriptWriter& wr, RCSpan mb) {
	size_t size = mb.size();
	if (size < (int)Opcode::OP_PUSHDATA1)
		wr << uint8_t(size);
	else if (size <= 0xFF)
		wr << Opcode::OP_PUSHDATA1 << uint8_t(size);
	else if (size <= 0xFFFF)
		wr << Opcode::OP_PUSHDATA2 << uint16_t(size);
	else
		wr << Opcode::OP_PUSHDATA4 << uint32_t(size);
	wr.Write(mb.data(), size);
	return wr;
}

inline ScriptWriter& operator<<(ScriptWriter& wr, const Blob& blob) {
	return wr << Span(blob);
}

inline ScriptWriter& operator<<(ScriptWriter& wr, const BigInteger& bi) {
	return wr << Span(bi.ToBytes());
}

inline ScriptWriter& operator<<(ScriptWriter& wr, const HashValue160& hash) {
	return wr << Span(hash);
}

inline ScriptWriter& operator<<(ScriptWriter& wr, const HashValue& hash) {
	return wr << hash.ToSpan();
}

inline ScriptWriter& operator<<(ScriptWriter& wr, int64_t v) {
	BinaryWriter& bwr = wr;
	if (v==-1 || v>=1 && v<=16)
		bwr << uint8_t(v + uint8_t(Opcode::OP_1) - 1);
	else
		wr << BigInteger(v);
	return wr;
}

ScriptWriter& operator<<(ScriptWriter& wr, const Script& script);


class ScriptReader : public BinaryReader {
	typedef BinaryReader base;
public:
	const CMemReadStream& Stm;

	ScriptReader(const CMemReadStream& stm)
		: base(stm)
		, Stm(stm)
	{}

	Blob ReadBlob(int size = -1) const {
		Blob r = CoinSerialized::ReadBlob(_self);
		if (size != -1 && r.size() != size)
			Throw(E_FAIL);
		return r;
	}
};

struct LiteInstr {
	Span Value;
	Coin::Opcode Opcode;
	Coin::Opcode OriginalOpcode;

	bool IsSmallPositiveOpcode() const { return OriginalOpcode >= Opcode::OP_1 && OriginalOpcode <= Opcode::OP_16; }
};

struct Instr {
	Coin::Opcode Opcode;
	Coin::Opcode OriginalOpcode;
	StackValue Value;

	Instr()
	{}

	explicit Instr(RCSpan mb)
		: Opcode(Opcode::OP_PUSHDATA1)
		, Value(mb)
	{}
};

class Vm : public VmStack {
	typedef VmStack base;
public:
	Span m_span;
	unique_ptr<CMemReadStream> m_stm;
	unique_ptr<ScriptReader> m_rd;
	int m_pc;
	int m_posCodeHash;
	CBool WitnessSig;	//!!!? never assigned
	//	Coin::Script Script;

	Vm(SignatureHasher* signatureHasher = nullptr) : base(signatureHasher) {
	}

	void Init(RCSpan mbScript);
	bool Eval(RCSpan mbScript);
	Instr GetOp();
	bool FastVerifyP2SH(const HashValue160& hash160) { return Hash160(GetStack(0)) == hash160; }
private:
	bool EvalImp();
};


class Script : public vector<Instr> {
	typedef Script class_type;
public:
	static Blob DeleteSubpart(RCSpan mb, RCSpan part);
    static bool IsPayToScriptHash(RCSpan scriptPubKey);
    static bool IsPushOnly(RCSpan script);

    static bool IsSpendable(RCSpan s) { return s.size() > 0 && s.size() <= MAX_SCRIPT_SIZE && s[0] != (uint8_t)Opcode::OP_RETURN; }

    static pair<Span, uint8_t> WitnessProgram(RCSpan scriptPubKey);

	Script() {}

	Script(RCSpan mb);
	void FindAndDelete(RCSpan mb);

	Blob ToBytes() const {
		MemoryStream ms;
		ScriptWriter w(ms);
		w << _self;
		return Span(ms);
	}

	static Blob BlobFromPubKey(const CanonicalPubKey& pubKey);
};

int CalcSigOpCount(RCSpan script, RCSpan scriptSig);
int CalcSigOpCount1(RCSpan script, bool bAccurate = false);

bool GetInstr(const CMemReadStream& stm, LiteInstr& instr);

bool ToBool(const Vm::Value& v);
BigInteger ToBigInteger(const Vm::Value& v);

} // Coin::
