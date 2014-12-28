/*######     Copyright (c) 1997-2015 Ufasoft  http://ufasoft.com  mailto:support@ufasoft.com,  Sergey Pavlov  mailto:dev@ufasoft.com #########################################################################################################
#                                                                                                                                                                                                                                            #
# This program is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation;  either version 3, or (at your option) any later version.          #
# This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.   #
# You should have received a copy of the GNU General Public License along with this program; If not, see <http://www.gnu.org/licenses/>                                                                                                      #
############################################################################################################################################################################################################################################*/

#include <el/ext.h>

#include <el/bignum.h>

#include <el/crypto/hash.h>
#include <el/crypto/ecdsa.h>
using namespace Ext::Crypto;

#include "script.h"
#include "coin-model.h"
#include "coin-protocol.h"


namespace Coin {

int MAX_OP_COUNT = 201;

BigInteger ToBigInteger(const Vm::Value& v) {
	Blob blob = v;
	byte& msb = blob.data()[blob.Size-1];
	bool bNeg = msb & 0x80;
	msb &= 0x7F;
	BigInteger r = BigInteger(blob.constData(), blob.Size);
	return bNeg ? -r : r;
}

Vm::Value FromBigInteger(const BigInteger& bi) {
	if (Sign(bi) == 0)
		return Blob();
	bool bNeg = Sign(bi) < 0;
	Blob blob = (bNeg ? -bi : bi).ToBytes();
	blob.data()[blob.Size-1] |= (bNeg ? 0x80 : 0);
	return blob;
}

ScriptWriter& operator<<(ScriptWriter& wr, const Script& script) {
	Throw(E_NOTIMPL);
	return wr;
}

Script::Script(const ConstBuf& mb) {
	Vm vm;
	vm.Init(mb);
	try {
//		DBG_LOCAL_IGNORE_NAME(E_EXT_EndOfStream, ignE_EXT_EndOfStream);

		while (!vm.m_stm->Eof())
			push_back(vm.GetOp());
	} catch (const EndOfStreamException&) {
		Throw(E_COIN_InvalidScript);
	}
}

void Script::FindAndDelete(const ConstBuf& mb) {
	Throw(E_NOTIMPL);
}

bool Script::IsAddress() {
	return size() == 5
		&& _self[0].Opcode == OP_DUP
		&& _self[1].Opcode == OP_HASH160
		&& _self[2].Value.Size == 20
		&& _self[3].Opcode == OP_EQUALVERIFY
		&& _self[4].Opcode == OP_CHECKSIG;
}

Blob Script::BlobFromAddress(const Address& addr) {
	MemoryStream ms;
	ScriptWriter wr(ms);
	if (addr.Ver == Eng().ChainParams.AddressVersion)
		wr << OP_DUP << OP_HASH160 << HashValue160(addr) << OP_EQUALVERIFY << OP_CHECKSIG;
	else
		wr << OP_HASH160 << HashValue160(addr) << OP_EQUAL;
	return ms;
}

Blob Script::BlobFromPubKey(const ConstBuf& mb) {
	MemoryStream ms;
	ScriptWriter wr(ms);
	wr << mb << OP_CHECKSIG;
	return ms;
}

bool GetOp(CMemReadStream& stm, LiteInstr& instr) {
	int op = stm.ReadByte();
	if (op < 0)
		return false;
	BinaryReader rd(stm);
	size_t len;
	switch (instr.Opcode = instr.OriginalOpcode = (Opcode)op) {
	case OP_PUSHDATA1:
		len = rd.ReadByte();
		instr.Buf = ConstBuf(stm.m_mb.P+stm.Position, len);
		stm.ReadBuffer(0, instr.Buf.Size);
		break;
	case OP_PUSHDATA2:
		len = rd.ReadUInt16();
		instr.Buf = ConstBuf(stm.m_mb.P+stm.Position, len);
		stm.ReadBuffer(0, instr.Buf.Size);
		instr.Opcode = OP_PUSHDATA1;
		break;
	case OP_PUSHDATA4:
		len = rd.ReadUInt32();
		instr.Buf = ConstBuf(stm.m_mb.P+stm.Position, len);
		stm.ReadBuffer(0, instr.Buf.Size);
		instr.Opcode = OP_PUSHDATA1;
		break;
	case OP_NOP: case OP_NOP1: case OP_NOP2: case OP_NOP3: case OP_NOP4: case OP_NOP5: case OP_NOP6: case OP_NOP7: case OP_NOP8: case OP_NOP9: case OP_NOP10:
		instr.Opcode = OP_NOP;
		break;
    case OP_1NEGATE:
    case OP_1:	case OP_2:	case OP_3:	case OP_4:	case OP_5:	case OP_6:	case OP_7:	case OP_8:	case OP_9:	case OP_10:	case OP_11:	case OP_12:	case OP_13:	case OP_14:	case OP_15:	case OP_16:
		instr.Opcode = OP_PUSHDATA1;
		break;
	default:
		if (instr.Opcode < OP_PUSHDATA1) {
			instr.Buf = ConstBuf(stm.m_mb.P+stm.Position, instr.Opcode);
			instr.Opcode = OP_PUSHDATA1;
			stm.ReadBuffer(0, instr.Buf.Size);
		} else if (instr.Opcode > OP_NOP10) {
			TRC(1, "OP_UNKNOWN: " << hex << instr.Opcode);
		}
	}
	return true;
}

Instr Vm::GetOp() {
	Instr instr;
	switch (instr.Opcode = instr.OriginalOpcode = (Opcode)m_rd->ReadByte()) {
	case OP_PUSHDATA1:
		instr.Value = m_rd->ReadBytes(m_rd->ReadByte());
		break;
	case OP_PUSHDATA2:
		instr.Value = m_rd->ReadBytes(m_rd->ReadUInt16());
		instr.Opcode = OP_PUSHDATA1;
		break;
	case OP_PUSHDATA4:
		instr.Value = m_rd->ReadBytes(m_rd->ReadUInt32());
		instr.Opcode = OP_PUSHDATA1;
		break;
	case OP_NOP: case OP_NOP1: case OP_NOP2: case OP_NOP3: case OP_NOP4: case OP_NOP5: case OP_NOP6: case OP_NOP7: case OP_NOP8: case OP_NOP9: case OP_NOP10:
		instr.Opcode = OP_NOP;
		break;
    case OP_1NEGATE:
    case OP_1:	case OP_2:	case OP_3:	case OP_4:	case OP_5:	case OP_6:	case OP_7:	case OP_8:	case OP_9:	case OP_10:	case OP_11:	case OP_12:	case OP_13:	case OP_14:	case OP_15:	case OP_16:
		instr.Value = FromBigInteger(int(instr.Opcode)-OP_1+1);
		instr.Opcode = OP_PUSHDATA1;
		break;
	default:
		if (instr.Opcode < OP_PUSHDATA1) {
			if (instr.Opcode > m_rd->BaseStream.Length-m_rd->BaseStream.Position)
				Throw(E_COIN_InvalidScript);											//!!!T to avoid nested EH
			instr.Value = m_rd->ReadBytes(instr.Opcode);
			instr.Opcode = OP_PUSHDATA1;
		} else if (instr.Opcode > OP_NOP10) {
			TRC(1, "OP_UNKNOWN: " << hex << instr.Opcode);
		}
	}
	return instr;
}

int CalcSigOpCount1(const ConstBuf& script, bool bAccurate) {
	int r = 0;
	CMemReadStream stm(script);
	LiteInstr instr, instrPrev;
	for (int i=0; GetOp(stm, instr); instrPrev=instr, ++i) {
		switch (instr.Opcode) {
		case OP_CHECKSIG:
		case OP_CHECKSIGVERIFY:
			++r;
			break;
		case OP_CHECKMULTISIG:
		case OP_CHECKMULTISIGVERIFY:
			if (bAccurate && i>0 && instrPrev.OriginalOpcode>=OP_1 && instrPrev.OriginalOpcode<=OP_16)
				r += int(instrPrev.OriginalOpcode) - OP_1 + 1;
			else
				r += 20;
			break;
		}
	}
	return r;
}

/*!!!R
int Script::CalcSigOpCount(bool bAccurate) const {
	int r = 0;
	for (int i=0; i<size(); ++i) {
		switch (_self[i].Opcode) {
		case OP_CHECKSIG:
		case OP_CHECKSIGVERIFY:
			++r;
			break;
		case OP_CHECKMULTISIG:
		case OP_CHECKMULTISIGVERIFY:
			if (bAccurate && i>0 && _self[i-1].OriginalOpcode>=OP_1 && _self[i-1].OriginalOpcode<=OP_16)
				r += int(_self[i-1].OriginalOpcode) - OP_1 + 1;
			else
				r += 20;
			break;
		}
	}
	return r;
}*/

int CalcSigOpCount(const Blob& script, const Blob& scriptSig) {
	if (!IsPayToScriptHash(script))							//!!!TODO checking again
		return CalcSigOpCount1(script, true);
	Script s(scriptSig);
	for (int i=0; i<s.size(); ++i)
		if (s[i].OriginalOpcode > OP_16)
			return 0;
	if (s.empty() || s.back().Opcode != OP_PUSHDATA1)
		return 0;
	return CalcSigOpCount1(s.back().Value, true);
}

static const byte s_b1 = 1;

const Vm::Value Vm::TrueValue(&s_b1, 1),
	Vm::FalseValue;

Vm::Value& Vm::GetStack(int idx) {
	if (idx < 0 || idx >= Stack.size())
		Throw(E_FAIL);
	return Stack.end()[-1-idx];
}

Vm::Value Vm::Pop() {
	if (Stack.empty())
		Throw(E_FAIL);
	Value r = Stack.back();
	Stack.pop_back();
	return r;
}

void Vm::SkipStack(int n) {
	while (n--) {
		if (Stack.empty())
			Throw(E_FAIL);
		Stack.pop_back();
	}
}

void Vm::Push(const Value& v) {
	Stack.push_back(v);
}

Blob Script::DeleteSubpart(const ConstBuf& mb, const ConstBuf& part) {
	if (0 == part.Size)
		return mb;
	Vm vm;
	vm.Init(mb);
	Blob r;	
	while (true) {
		int pos = (int)vm.m_stm->Position;
		while (pos+part.Size <= mb.Size && !memcmp(mb.P+pos, part.P, part.Size)) {
			vm.m_stm->put_Position(vm.m_stm->get_Position() + part.Size);
			pos = (int)vm.m_stm->Position;
		}
		if (vm.m_stm->Eof())
			break;
		vm.GetOp();
		r = r + Blob(mb.P+pos, (int)vm.m_stm->Position-pos);
	}
	return r;
}

HashValue SignatureHash(const ConstBuf& script, const TxObj& txoTo, int nIn, int32_t nHashType) {
	Tx txTmp(txoTo.Clone());
	txTmp.m_pimpl->m_nBytesOfHash = 0;
	byte opcode = OP_CODESEPARATOR;
	Blob script1 = Script::DeleteSubpart(script, ConstBuf(&opcode, 1));
	for (int i=0; i<txTmp.TxIns().size(); ++i)
		txTmp.m_pimpl->m_txIns.at(i).put_Script(Blob());
	txTmp.m_pimpl->m_txIns.at(nIn).put_Script(script1);					// .at() checks 0<=nIn<size()

	int nOut = -1;
	switch (nHashType & 0x1f) {
	case SIGHASH_SINGLE:        
        if ((nOut = nIn) >= txTmp.TxOuts().size()) {				// Only lockin the txout payee at same index as txin
			byte one[32] = { 1 };
			return HashValue(one);				//!!!? probably a security BUG
			//!!! Throw(E_FAIL);
		}
	case SIGHASH_NONE:											// Wildcard payee
        txTmp.TxOuts().resize(nOut+1);
        for (int i = 0; i < nOut; i++)
            txTmp.TxOuts()[i] = TxOut();        
        for (int i = 0; i < txTmp.TxIns().size(); i++)			// Let the others update at will
            if (i != nIn)
                txTmp.m_pimpl->m_txIns.at(i).Sequence = 0;
		break;
    }
    
    if (nHashType & SIGHASH_ANYONECANPAY) {						// Blank out other inputs completely, not recommended for open transactions
        txTmp.m_pimpl->m_txIns.at(0) = txTmp.TxIns()[nIn];
        txTmp.m_pimpl->m_txIns.resize(1);
    }
	return Hash(EXT_BIN(txTmp << nHashType));
}

bool CheckSig(ConstBuf sig, const ConstBuf& pubKey, const ConstBuf& script, const Tx& txTo, int nIn, int32_t nHashType) {
	if (IsCanonicalSignature(sig)) {
		try {
	#define EC_R_INVALID_ENCODING				 102	// OpenSSL
			DBG_LOCAL_IGNORE(MAKE_HRESULT(SEVERITY_ERROR, FACILITY_OPENSSL, EC_R_INVALID_ENCODING));

	#if UCFG_COIN_ECC=='S'
			Sec256Dsa dsa;
			dsa.ParsePubKey(pubKey);
	#else
			ECDsa dsa(CngKey::Import(pubKey, CngKeyBlobFormat::OSslEccPublicBlob));
	#endif
			if (0 == sig.Size || (nHashType = nHashType ? nHashType : sig.P[sig.Size-1]) != sig.P[sig.Size-1])
				return false;
			sig.Size--;
			return dsa.VerifyHash(SignatureHash(script, *txTo.m_pimpl, nIn, nHashType), sig);
		} catch (CryptoException& DBG_PARAM(ex)) {
			TRC(2, ex.Message);
		}
	}
	return false;
}

bool ToBool(const Vm::Value& v) {
	for (int i=0; i<v.Size; ++i)
		if (v.constData()[i])
			return i!=v.Size-1 || v.constData()[i]!=0x80;
	return false;
}

static void MakeSameSize(Vm::Value& v1, Vm::Value& v2) {
	size_t size = std::max(v1.Size, v2.Size);
	v1.Size = size;
	v2.Size = size;
}

bool Vm::EvalImp(const Tx& txTo, uint32_t nIn, int32_t nHashType) {
	bool r = true;
	vector<bool> vExec;
	vector<Value> altStack;
	int nOpCount = 0;
	for (; !m_rd->BaseStream.Eof();) {
		const Instr instr = GetOp();
        if (instr.Opcode > OP_16 && ++nOpCount > MAX_OP_COUNT)
            return false;
        bool bExec = !count(vExec.begin(), vExec.end(), false);
		switch (instr.Opcode) {
		case OP_IF:
			vExec.push_back(bExec ? ToBool(Pop()) : false);
			break;
		case OP_NOTIF:
			vExec.push_back(bExec ? !ToBool(Pop()) : false);
			break;		
		case OP_ELSE:
			if (vExec.empty())
				return false;
			vExec.back() = !vExec.back();
			break;
		case OP_ENDIF:
			if (vExec.empty())
				return false;
			vExec.pop_back();
			break;
		default:
			if (bExec) {
				switch (instr.Opcode) {
				case OP_PUSHDATA1:
					Push(instr.Value);
				case OP_NOP:
					break;
				case OP_DUP:
					Push(GetStack(0));
					break;
				case OP_2DUP:
					Push(GetStack(1));
					Push(GetStack(1));
					break;
				case OP_3DUP:
					Push(GetStack(2));
					Push(GetStack(2));
					Push(GetStack(2));
					break;
				case OP_2OVER:
					Push(GetStack(3));
					Push(GetStack(3));
					break;
				case OP_DROP:
					SkipStack(1);
					break;
				case OP_2DROP:
					SkipStack(2);
					break;
				case OP_NIP:
					GetStack(1) = GetStack(0);
					SkipStack(1);
					break;
				case OP_OVER:
					Push(GetStack(1));
					break;
				case OP_SWAP:
					std::swap(GetStack(0), GetStack(1));
					break;
				case OP_2SWAP:
					std::swap(GetStack(3), GetStack(1));
					std::swap(GetStack(2), GetStack(0));
					break;
				case OP_ROT:
					std::swap(GetStack(2), GetStack(1));
					std::swap(GetStack(1), GetStack(0));
					break;
				case OP_2ROT:
					{
						Value v1 = GetStack(5),
							v2 = GetStack(4);
						Stack.erase(Stack.end()-6, Stack.end()-4);
						Stack.push_back(v1);
						Stack.push_back(v2);
					}
					break;
				case OP_TUCK:
					{
						if (Stack.size() < 2)
							Throw(E_FAIL);
						Value v = GetStack(0);
						Stack.insert(Stack.end()-2, v);
					}
					break;
                case OP_PICK:
					{
						int n = explicit_cast<int>(ToBigInteger(Pop()));
						Push(GetStack(n));
					}
					break;
                case OP_ROLL:
					{
						int n = explicit_cast<int>(ToBigInteger(Pop()));
						Value v = GetStack(n);
						Stack.erase(Stack.end()-1-n);
						Push(v);
					}
					break;
				case OP_CAT:			
					{
						Value& v = GetStack(1);
						v.Replace(v.Size, 0, GetStack(0));
						if (v.Size > 520)
							return false;
						SkipStack(1);
					}
					break;
				case OP_SUBSTR:
					{
						Value v = GetStack(2);
						int beg = explicit_cast<int>(ToBigInteger(GetStack(1))),
							end = beg+explicit_cast<int>(ToBigInteger(GetStack(0)));
						if (beg<0 || end<beg)
							return false;
						beg = std::min(beg, (int)v.Size);
						end = std::min(end, (int)v.Size);
						GetStack(2) = Blob(v.constData()+beg, end-beg);
						SkipStack(2);
					}
					break;
				case OP_LEFT:
					{
						int size = explicit_cast<int>(ToBigInteger(Pop()));
						Value& v = GetStack(0);
						if (size < 0)
							return false;
						size = std::min(size, (int)v.Size);
						v.Replace(size, v.Size-size, Blob());
					}
					break;
				case OP_RIGHT:
					{
						int size = explicit_cast<int>(ToBigInteger(Pop()));
						Value& v = GetStack(0);
						if (size < 0)
							return false;
						size = std::min(size, (int)v.Size);
						v.Replace(0, v.Size-size, Blob());
					}
					break;
				case OP_RIPEMD160:
					GetStack(0) = RIPEMD160().ComputeHash(GetStack(0));
					break;
				case OP_SHA256:
					GetStack(0) = SHA256().ComputeHash(GetStack(0));
					break;
				case OP_SHA1:
					GetStack(0) = SHA1().ComputeHash(GetStack(0));
					break;
				case OP_HASH160:
					GetStack(0) = Hash160(GetStack(0));
					break;
				case OP_HASH256:				
					GetStack(0) = Eng().HashMessage(GetStack(0));
					break;
				case OP_VERIFY:
					if (!ToBool(Pop()))
						return false;
					break;
				case OP_IFDUP:
					if (ToBool(GetStack(0)))
						Push(GetStack(0));
					break;
				case OP_EQUAL:
					Push(Pop() == Pop() ? TrueValue : FalseValue);
					break;
				case OP_EQUALVERIFY:
					if (Pop() != Pop()) {
						r = false;
						goto LAB_RET;
					}
					break;
				case OP_CHECKSIG:
				case OP_CHECKSIGVERIFY:
					{
						Value key = Pop(),
							sig = Pop();
						Blob script = Script::DeleteSubpart(Blob(m_blob.constData()+m_posCodeHash, m_blob.Size-m_posCodeHash), sig);
						bool bOk = CheckSig(sig, key, script, txTo, nIn, nHashType);
						if (OP_CHECKSIG == instr.Opcode)
							Push(bOk ? TrueValue : FalseValue);
						else if (!bOk)
							return false;
					}
					break;
                case OP_CHECKMULTISIG:
                case OP_CHECKMULTISIGVERIFY:
					{
						vector<Blob> vPubKey;
						vector<Blob> vSig;
						int n = explicit_cast<int>(ToBigInteger(Pop()));
						if (n < 0 || n > 20 || (nOpCount += n) > MAX_OP_COUNT)
							return false;
						for (int i=0; i<n; ++i)
							vPubKey.push_back(Pop());
						n = explicit_cast<int>(ToBigInteger(Pop()));
						if (n < 0 || n > vPubKey.size())
							return false;
						for (int i=0; i<n; ++i)
							vSig.push_back(Pop());
						SkipStack(1);						// one extra unused value, dut to an old bug
						Blob script(m_blob.constData()+m_posCodeHash, m_blob.Size-m_posCodeHash);
						EXT_FOR (const Blob& sig, vSig) {
							script = Script::DeleteSubpart(script, sig);
						}
						bool b = true;
						for (int i=0, j=0; i<vSig.size() && (b = vSig.size()-i <= vPubKey.size()-j); ++j) {
							if (CheckSig(vSig[i], vPubKey[j], script, txTo, nIn, nHashType))
								++i;
						}
						if (OP_CHECKMULTISIG == instr.Opcode)
							Push(b ? TrueValue : FalseValue);
						else if (!b)
							return false;
					}
					break;
				case OP_INVERT:
					for (byte *p=GetStack(0).data(), *q=p+GetStack(0).Size; p<q; ++p)
						*p = ~*p;
					break;
				case OP_AND:
					{
						Value& vr = GetStack(1),
							 &v = GetStack(0);
						MakeSameSize(vr, v);
						byte *pr = vr.data();
						const byte *p = v.constData();
						for (int i=0, n=vr.Size; i<n; ++i)
							pr[i] &= p[i];
						SkipStack(1);
					}
					break;
				case OP_OR:
					{
						Value& vr = GetStack(1),
							 &v = GetStack(0);
						MakeSameSize(vr, v);
						byte *pr = vr.data();
						const byte *p = v.constData();
						for (int i=0, n=vr.Size; i<n; ++i)
							pr[i] |= p[i];
						SkipStack(1);
					}
					break;
				case OP_XOR:
					{
						Value& vr = GetStack(1),
							 &v = GetStack(0);
						MakeSameSize(vr, v);
						byte *pr = vr.data();
						const byte *p = v.constData();
						for (int i=0, n=vr.Size; i<n; ++i)
							pr[i] ^= p[i];
						SkipStack(1);
					}
					break;
				case OP_ADD:
					{
						BigInteger a = ToBigInteger(GetStack(0)),
								b = ToBigInteger(GetStack(1));
						GetStack(1) = FromBigInteger(a + b);
					}
					SkipStack(1);
					break;
				case OP_SUB:
					GetStack(1) = FromBigInteger(ToBigInteger(GetStack(1)) - ToBigInteger(GetStack(0)));
					SkipStack(1);
					break;
				case OP_MUL:
					GetStack(1) = FromBigInteger(ToBigInteger(GetStack(1)) * ToBigInteger(GetStack(0)));
					SkipStack(1);
					break;
				case OP_DIV:
					GetStack(1) = FromBigInteger(ToBigInteger(GetStack(1)) / ToBigInteger(GetStack(0)));
					SkipStack(1);
					break;
				case OP_MOD:
					GetStack(1) = FromBigInteger(ToBigInteger(GetStack(1)) % ToBigInteger(GetStack(0)));
					SkipStack(1);
					break;
				case OP_LSHIFT:
					GetStack(1) = FromBigInteger(ToBigInteger(GetStack(1)) << explicit_cast<int>(ToBigInteger(GetStack(0))));
					SkipStack(1);
					break;
				case OP_RSHIFT:
					GetStack(1) = FromBigInteger(ToBigInteger(GetStack(1)) >> explicit_cast<int>(ToBigInteger(GetStack(0))));
					SkipStack(1);
					break;
				case OP_BOOLAND:
					GetStack(1) = FromBigInteger(!!ToBigInteger(GetStack(1)) && !!ToBigInteger(GetStack(0)));
					SkipStack(1);
					break;
				case OP_BOOLOR:
					GetStack(1) = FromBigInteger(!!ToBigInteger(GetStack(1)) || !!ToBigInteger(GetStack(0)));
					SkipStack(1);
					break;
				case OP_NUMEQUAL:
					GetStack(1) = FromBigInteger(ToBigInteger(GetStack(1)) == ToBigInteger(GetStack(0)));
					SkipStack(1);
					break;
				case OP_NUMEQUALVERIFY:
					if (ToBigInteger(GetStack(1)) != ToBigInteger(GetStack(0)))
						return false;
					SkipStack(2);
					break;
				case OP_NUMNOTEQUAL:
					GetStack(1) = FromBigInteger(ToBigInteger(GetStack(1)) != ToBigInteger(GetStack(0)));
					SkipStack(1);
					break;
				case OP_LESSTHAN:
					GetStack(1) = FromBigInteger(ToBigInteger(GetStack(1)) < ToBigInteger(GetStack(0)));
					SkipStack(1);
					break;
				case OP_GREATERTHAN:
					GetStack(1) = FromBigInteger(ToBigInteger(GetStack(1)) > ToBigInteger(GetStack(0)));
					SkipStack(1);
					break;
				case OP_LESSTHANOREQUAL:
					GetStack(1) = FromBigInteger(ToBigInteger(GetStack(1)) <= ToBigInteger(GetStack(0)));
					SkipStack(1);
					break;
				case OP_GREATERTHANOREQUAL:
					GetStack(1) = FromBigInteger(ToBigInteger(GetStack(1)) >= ToBigInteger(GetStack(0)));
					SkipStack(1);
					break;
				case OP_WITHIN:
					{
						BigInteger a = ToBigInteger(GetStack(2)),
								b = ToBigInteger(GetStack(1)),
								c = ToBigInteger(GetStack(0));
						SkipStack(2);
						GetStack(0) = b <= a && a < c ? TrueValue : FalseValue;
					}
					break;
				case OP_MIN:
					GetStack(1) = FromBigInteger(std::min(ToBigInteger(GetStack(1)), ToBigInteger(GetStack(0))));
					SkipStack(1);
					break;
				case OP_MAX:
					GetStack(1) = FromBigInteger(std::max(ToBigInteger(GetStack(1)), ToBigInteger(GetStack(0))));
					SkipStack(1);
					break;
				case OP_1ADD:
					GetStack(0) = FromBigInteger(ToBigInteger(GetStack(0)) + 1);
					break;
				case OP_1SUB:
					GetStack(0) = FromBigInteger(ToBigInteger(GetStack(0)) - 1);
					break;
				case OP_2MUL:
					GetStack(0) = FromBigInteger(ToBigInteger(GetStack(0)) << 1);
					break;
				case OP_2DIV:
					GetStack(0) = FromBigInteger(ToBigInteger(GetStack(0)) >> 1);
					break;
				case OP_NEGATE:
					GetStack(0) = FromBigInteger(-ToBigInteger(GetStack(0)));
					break;
				case OP_ABS:
					GetStack(0) = FromBigInteger(abs(ToBigInteger(GetStack(0))));
					break;
				case OP_NOT:
					GetStack(0) = FromBigInteger(!ToBigInteger(GetStack(0)));
					break;
				case OP_0NOTEQUAL:
					GetStack(0) = FromBigInteger(!!ToBigInteger(GetStack(0)));
					break;
				case OP_SIZE:
					Push(FromBigInteger(BigInteger((int64_t)GetStack(0).Size)));
					break;
				case OP_DEPTH:
					Push(FromBigInteger(BigInteger((int64_t)Stack.size())));
					break;
				case OP_CODESEPARATOR:
					m_posCodeHash = (int)m_stm->Position;
					break;
				case OP_RETURN:
					return false;
                case OP_TOALTSTACK:
					altStack.push_back(Pop());
	                break;
                case OP_FROMALTSTACK:
					if (altStack.empty())
						return false;
					Push(altStack.back());
					altStack.pop_back();
					break;
				default:
					Throw(E_NOTIMPL);
				}
			}
		}
	}
LAB_RET:
	return r;
}

void Vm::Init(const ConstBuf& mbScript) {
	m_blob = mbScript;
	m_stm.reset(new CMemReadStream(m_blob));
	m_rd.reset(new ScriptReader(*m_stm));
	m_pc = 0;
	m_posCodeHash = 0;
}

bool Vm::Eval(const ConstBuf& mbScript, const Tx& txTo, uint32_t nIn, int32_t nHashType) {
	Init(mbScript);
	try {
		return EvalImp(txTo, nIn, nHashType);
	} catch (RCExc) {
		return false;
	}
}

const double LN2 = log(2.),
	MINUS_ONE_DIV_LN2SQUARED = -1 / (LN2*LN2);

CoinFilter::CoinFilter(int nElements, double falsePostitiveRate, uint32_t tweak, byte flags)
	:	Tweak(tweak)
	,	Flags(flags)
{
	Bitset.resize(8 * min(MAX_BLOOM_FILTER_SIZE, size_t(MINUS_ONE_DIV_LN2SQUARED * nElements * log(falsePostitiveRate))/8));
	ASSERT((Bitset.size() & 7) == 0);
	HashNum = min(MAX_HASH_FUNCS, int(Bitset.size()/nElements * LN2));
}

size_t CoinFilter::Hash(const ConstBuf& cbuf, int n) const {
	return MurmurHash3_32(cbuf, n * 0xFBA4C795 + Tweak) % Bitset.size();
}

ConstBuf CoinFilter::FindScriptData(const ConstBuf& script) const {
	CMemReadStream stm(script);
	for (LiteInstr instr; GetOp(stm, instr); )
		if (instr.Opcode==OP_PUSHDATA1 && instr.Buf.Size>0 && Contains(instr.Buf))
			return instr.Buf;
	return ConstBuf();
}

void CoinFilter::Write(BinaryWriter& wr) const {
	vector<byte> v;
	to_block_range(Bitset, back_inserter(v));
	CoinSerialized::WriteBlob(wr, Blob(&v[0], v.size()));
	wr << uint32_t(HashNum) << Tweak << Flags;
}

void CoinFilter::Read(const BinaryReader& rd) {
	Blob data = CoinSerialized::ReadBlob(rd);
	Bitset.resize(data.Size*8);
	for (int i=0; i<data.Size; ++i)
		for (int j=0; j<8; ++j)
			Bitset.replace(i*8+j, (data.constData()[i] >> j) & 1);
	HashNum = rd.ReadUInt32();
	rd >> Tweak >> Flags;
}

bool CoinFilter::IsRelevantAndUpdate(const Tx& tx) {
	HashValue hashTx = Coin::Hash(tx);
	bool bFound = Contains(hashTx);
	for (int i=0; i<tx.TxOuts().size(); ++i) {
		const TxOut& txOut = tx.TxOuts()[i];
		ConstBuf data = FindScriptData(txOut.get_PkScript());
		if (data.P) {
			bFound = true;
			if ((Flags & BLOOM_UPDATE_MASK) == BLOOM_UPDATE_ALL)
				Insert(OutPoint(hashTx, i));
			else if ((Flags & BLOOM_UPDATE_MASK) == BLOOM_UPDATE_P2PUBKEY_ONLY) {
				HashValue160 hash160;
				Blob pk;
				int cbPk = txOut.TryParseDestination(hash160, pk);
				if (cbPk == 33 || cbPk == 65)				//!!! TODO: TX_MULTISIG
					Insert(OutPoint(hashTx, i));
			}
			return true;
		}
	}
	if (bFound)
		return true;
	EXT_FOR (const TxIn& txIn, tx.TxIns()) {
		if (Contains(txIn.PrevOutPoint) || FindScriptData(txIn.Script()).P)
			return true;
	}
    return false;
}

} // Coin::

