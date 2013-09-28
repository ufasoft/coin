/*######     Copyright (c) 1997-2013 Ufasoft  http://ufasoft.com  mailto:support@ufasoft.com,  Sergey Pavlov  mailto:dev@ufasoft.com #######################################
#                                                                                                                                                                          #
# This program is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation;  #
# either version 3, or (at your option) any later version. This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the      #
# implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details. You should have received a copy of the GNU #
# General Public License along with this program; If not, see <http://www.gnu.org/licenses/>                                                                               #
##########################################################################################################################################################################*/

#pragma once

#include <el/bignum.h>

#include "coin-model.h"

namespace Coin {

class Tx;
class Script;

enum Opcode {
    // push value
    OP_0=0,
    OP_FALSE=OP_0,
    OP_PUSHDATA1=76,
    OP_PUSHDATA2,
    OP_PUSHDATA4,
    OP_1NEGATE,
    OP_RESERVED,
    OP_1,	OP_TRUE=OP_1,	OP_2,	OP_3,	OP_4,	OP_5,	OP_6,	OP_7,	OP_8,	OP_9,	OP_10,	OP_11,	OP_12,	OP_13,	OP_14,	OP_15,	OP_16,
	
	// control
    OP_NOP,	OP_VER,	OP_IF,	OP_NOTIF,	OP_VERIF,	OP_VERNOTIF,	OP_ELSE,	OP_ENDIF,	OP_VERIFY,	OP_RETURN,	

    // stack ops
    OP_TOALTSTACK,	OP_FROMALTSTACK,	OP_2DROP,	OP_2DUP,	OP_3DUP,	OP_2OVER,	OP_2ROT,	OP_2SWAP,	OP_IFDUP,	OP_DEPTH,	OP_DROP,	OP_DUP,	OP_NIP,	OP_OVER,	OP_PICK,	OP_ROLL,	OP_ROT,	OP_SWAP,	OP_TUCK,
	
	// splice ops
    OP_CAT,	OP_SUBSTR,	OP_LEFT,	OP_RIGHT,	OP_SIZE,

    // bit logic
    OP_INVERT,	OP_AND,	OP_OR,	OP_XOR,	OP_EQUAL,	OP_EQUALVERIFY,	OP_RESERVED1,	OP_RESERVED2,

    // numeric
    OP_1ADD,	OP_1SUB,	OP_2MUL,	OP_2DIV,	OP_NEGATE,	OP_ABS,	OP_NOT,	OP_0NOTEQUAL,	

    OP_ADD,	OP_SUB,	OP_MUL,	OP_DIV,	OP_MOD,	OP_LSHIFT,	OP_RSHIFT,

    OP_BOOLAND,	OP_BOOLOR,	OP_NUMEQUAL,	OP_NUMEQUALVERIFY,	OP_NUMNOTEQUAL,	OP_LESSTHAN,	OP_GREATERTHAN,	OP_LESSTHANOREQUAL,	OP_GREATERTHANOREQUAL,	OP_MIN,	OP_MAX,

    OP_WITHIN,

    // crypto
    OP_RIPEMD160,	OP_SHA1,	OP_SHA256,	OP_HASH160,	OP_HASH256,	OP_CODESEPARATOR,	OP_CHECKSIG,	OP_CHECKSIGVERIFY,	OP_CHECKMULTISIG,	OP_CHECKMULTISIGVERIFY,

    // expansion
    OP_NOP1,	OP_NOP2,	OP_NOP3,	OP_NOP4,	OP_NOP5,	OP_NOP6,	OP_NOP7,	OP_NOP8,	OP_NOP9,	OP_NOP10,
	
    // template matching params
    OP_PUBKEYHASH = 0xfd,	OP_PUBKEY = 0xfe,

    OP_INVALIDOPCODE = 0xff,
};


class ScriptWriter : public BinaryWriter {
	typedef BinaryWriter base;
public:
	ScriptWriter(Stream& stm)
		:	base(stm)
	{}
};

inline ScriptWriter& operator<<(ScriptWriter& wr, byte v) {
	(BinaryWriter&)wr << v;
	return wr;
}

inline ScriptWriter& operator<<(ScriptWriter& wr, UInt16 v) {
	(BinaryWriter&)wr << v;
	return wr;
}

inline ScriptWriter& operator<<(ScriptWriter& wr, Int32 v) {
	(BinaryWriter&)wr << v;
	return wr;
}

inline ScriptWriter& operator<<(ScriptWriter& wr, UInt32 v) {
	(BinaryWriter&)wr << v;
	return wr;
}

inline ScriptWriter& operator<<(ScriptWriter& wr, UInt64 v) {
	(BinaryWriter&)wr << v;
	return wr;
}

inline ScriptWriter& operator<<(ScriptWriter& wr, Opcode opcode) {
	wr << byte(opcode);
	return wr;
}

inline ScriptWriter& operator<<(ScriptWriter& wr, const ConstBuf& mb) {
	size_t size = mb.Size;
	if (size < OP_PUSHDATA1)
		wr << byte(size);
	else if (size <= 0xFF)
		wr << byte(OP_PUSHDATA1) << byte(size);
	else if (size <= 0xFFFF)
		wr << byte(OP_PUSHDATA2) << UInt16(size);
	else
		wr << byte(OP_PUSHDATA4) << UInt32(size);
	wr.Write(mb.P, size);
	return wr;
}

inline ScriptWriter& operator<<(ScriptWriter& wr, const Blob& blob) {
	return wr << ConstBuf(blob);
}

inline ScriptWriter& operator<<(ScriptWriter& wr, const BigInteger& bi) {
	return wr << bi.ToBytes();
}

inline ScriptWriter& operator<<(ScriptWriter& wr, const HashValue& hash) {
	return wr << ConstBuf(hash);
}

inline ScriptWriter& operator<<(ScriptWriter& wr, const HashValue160& hash) {
	return wr << ConstBuf(hash);
}

inline ScriptWriter& operator<<(ScriptWriter& wr, Int64 v) {
	BinaryWriter& bwr = wr;
	if (v==-1 || v>=1 && v<=16)
		bwr << byte(v+OP_1-1);
	else
		wr << BigInteger(v);
	return wr;
}

ScriptWriter& operator<<(ScriptWriter& wr, const Script& script);


class ScriptReader : public BinaryReader {
	typedef BinaryReader base;
public:
	ScriptReader(const Stream& stm)
		:	base(stm)
	{}

	Blob ReadBlob(int size = -1) const {
		Blob r = CoinSerialized::ReadBlob(_self);
		if (size != -1 && r.Size != size)
			Throw(E_FAIL);
		return r;
	}
};

struct LiteInstr {
	Coin::Opcode Opcode;
	Coin::Opcode OriginalOpcode;
	ConstBuf Buf;
};

struct Instr {
	Coin::Opcode Opcode;
	Coin::Opcode OriginalOpcode;
	Blob Value;

	Instr()
	{}

	explicit Instr(const ConstBuf& mb)
		:	Opcode(OP_PUSHDATA1)
		,	Value(mb)
	{}

};


class Script : public vector<Instr> {
	typedef Script class_type;
public:
	Script() {}

	Script(const ConstBuf& mb);

	void FindAndDelete(const ConstBuf& mb);

	bool IsAddress();

	Blob ToBytes() const {
		MemoryStream ms;
		ScriptWriter w(ms);
		w << _self;
		return ms;
	}

	static Blob BlobFromAddress(const HashValue160& hash160);
	static Blob BlobFromPubKey(const ConstBuf& mb);
//!!!R	int CalcSigOpCount(bool bAccurate = false) const;
};

int CalcSigOpCount(const Blob& script, const Blob& scriptSig = nullptr);
int CalcSigOpCount1(const ConstBuf& script, bool bAccurate = false);

bool GetOp(CMemReadStream& stm, LiteInstr& instr);

class Vm {
public:
	typedef Blob Value;

	static const Value TrueValue,
						FalseValue;

	Blob m_blob;
	auto_ptr<CMemReadStream> m_stm;
	auto_ptr<ScriptReader> m_rd;
	vector<Value> Stack;
//	Coin::Script Script;

	void Init(const ConstBuf& mbScript);
	bool Eval(const ConstBuf& mbScript, const Tx& txTo, UInt32 nIn, Int32 nHashType);
	Instr GetOp();
private:
	int m_pc;
	int m_posCodeHash;

	Value& GetStack(int idx);
	Value Pop();
	void SkipStack(int n);
	void Push(const Value& v);
	bool EvalImp(const Tx& txTo, UInt32 nIn, Int32 nHashType);
};

bool IsPayToScriptHash(const Blob& script);
HashValue SignatureHash(const ConstBuf& script, const Tx& txTo, int nIn, Int32 nHashType);
void VerifySignature(const Tx& txFrom, const Tx& txTo, UInt32 nIn, Int32 nHashType = 0);
bool ToBool(const Vm::Value& v);
BigInteger ToBigInteger(const Vm::Value& v);

} // Coin::

