/*######     Copyright (c) 1997-2013 Ufasoft  http://ufasoft.com  mailto:support@ufasoft.com,  Sergey Pavlov  mailto:dev@ufasoft.com #######################################
#                                                                                                                                                                          #
# This program is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation;  #
# either version 3, or (at your option) any later version. This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the      #
# implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details. You should have received a copy of the GNU #
# General Public License along with this program; If not, see <http://www.gnu.org/licenses/>                                                                               #
##########################################################################################################################################################################*/

#include <el/ext.h>

#include <el/crypto/hash.h>
#include <el/crypto/ext-openssl.h>

#include <el/num/mod.h>
using namespace Ext::Num;

#define OPENSSL_NO_SCTP
#include <openssl/err.h>
#include <openssl/ec.h>

#include "coin-protocol.h"
#include "script.h"
#include "eng.h"
#include "coin-msg.h"

namespace Coin {

const Blob& TxIn::Script() const {
	if (!m_script) {
		CoinEng& eng = Eng();
		Tx tx = Tx::FromDb(PrevOutPoint.TxHash);
		const TxOut& txOut = tx.TxOuts()[PrevOutPoint.Index];
		Blob pk = eng.GetPkById(txOut.m_idPk);
		byte lenPk = byte(pk.Size);
		m_script.AssignIfNull(m_sig + ConstBuf(&lenPk, 1) + pk);
	}
	return m_script;
}

void TxIn::Write(BinaryWriter& wr) const {
	wr << PrevOutPoint;
	CoinSerialized::WriteBlob(wr, Script());
	wr << Sequence;
}

void TxIn::Read(const BinaryReader& rd) {
	rd >> PrevOutPoint;
	m_script = CoinSerialized::ReadBlob(rd);
	rd >> Sequence;
}

void TxOut::Write(BinaryWriter& wr) const {
	wr << Value;
	CoinSerialized::WriteBlob(wr, get_PkScript());
}


#ifdef X_DEBUG//!!!D

}

#include "sqlite3.h"

namespace Coin {
volatile Int32 s_nObjectCount; //!!!D

TxCounter::TxCounter() {
	Interlocked::Increment(s_nObjectCount);
	if (!(s_nObjectCount & 0xFF)) {
		Int64 mem = ::sqlite3_memory_used();
		Int64 lim = sqlite3_soft_heap_limit64(-1);
		mem = mem;
	}
}

TxCounter::TxCounter(const TxCounter&) {
	Interlocked::Increment(s_nObjectCount);
}

TxCounter::~TxCounter() {
	Interlocked::Decrement(s_nObjectCount);
	if (s_nObjectCount < 0)
		s_nObjectCount = s_nObjectCount;
}
#endif

void TxOut::Read(const BinaryReader& rd) {
	Value = rd.ReadUInt64();
	m_pkScript = CoinSerialized::ReadBlob(rd);
}

const TxOut& CTxMap::GetOutputFor(const OutPoint& prev) const {
	const_iterator it = find(prev.TxHash);
	if (it == end())
		Throw(E_FAIL);
	return it->second.TxOuts().at(prev.Index);
}

//!!! static regex s_rePkScriptDestination("(\\x76\\xA9\\x14([^]{20,20})\\x88\\xAC|(\\x21|\\x41)([^]+)\\xAC)");		// binary, VC don't allows regex for binary data

byte TryParseDestination(const ConstBuf& pkScript, HashValue160& hash160, Blob& pubkey) {
	const byte *p = pkScript.P;
	if (pkScript.Size < 4 || p[pkScript.Size-1] != OP_CHECKSIG)
		return 0;
	if (pkScript.Size == 25) {
		if (p[0]==OP_DUP && p[1]==OP_HASH160 && p[2]==20 && p[pkScript.Size-2]==OP_EQUALVERIFY) {
			hash160 = HashValue160(ConstBuf(p+3, 20));
			return 20;
		}
	} else if (p[0]==33 || p[0]==65) {
		byte len = p[0];
		if (pkScript.Size == 1+len+1) {
			hash160 = Hash160(pubkey = Blob(p+1, len));
			return len;
		}
	}
	return 0;
}

TxObj::TxObj()
	:	Height(-1)
	,	LockBlock(0)
	,	m_nBytesOfHash(0)
	,	m_bLoadedIns(false)
{
}

void TxObj::ReadTxIns(const DbReader& rd) const {
	CoinEng& eng = Eng();
	while (!rd.BaseStream.Eof()) {
		Int64 blockordBack = rd.Read7BitEncoded(),
			 blockHeight = -1;
		pair<int, OutPoint> pp(-1, OutPoint());
		TxIn txIn;
		if (0 == blockordBack)
			txIn.PrevOutPoint = OutPoint();
		else {
			int idx = (int)rd.Read7BitEncoded();
			if (rd.ReadTxHashes) {																	//!!!? always true
				blockHeight = Height-(blockordBack-1);
				txIn.PrevOutPoint = (pp = rd.Eng->Db->GetTxHashesOutNums((int)blockHeight).OutPointByIdx(idx)).second;
			}
		}
		byte typ = rd.ReadByte();
		switch (typ & 0x7F) {
		case 0:
			txIn.m_script = CoinSerialized::ReadBlob(rd);
			break;
		case 0x7F:
			if (eng.LiteMode)
				break;
//!!!?			Throw(E_FAIL);
		default:
			int len = (typ & 0x07)+63;
			ASSERT(len>63 || (typ & 0x10));   // WriteTxIns bug workaround
			Blob sig(0, len+6);
			byte *data = sig.data();			
			
			data[0] = byte(len+5);
			data[1] = 0x30;
			data[2] = byte(len+2);
			data[3] = 2;
			data[4] = 0x20 | ((typ & 0x20)>>5);
			rd.Read(data+5, data[4]);
			data[5+data[4]] = 2;
			byte len2 = byte(len-data[4]-2);
			data[5+data[4]+1] = len2;
			rd.Read(data+5+data[4]+2, len2);
			data[5+len] = 1;
			if (typ & 0x40) {
				txIn.m_sig = sig;
			} else
				txIn.m_script = sig;
		}
		txIn.Sequence = (typ & 0x80) ? UInt32(-1) : UInt32(rd.Read7BitEncoded());
		m_txIns.push_back(txIn);
	}
}

static mutex s_mtxTx;

const vector<TxIn>& TxObj::TxIns() const {
	if (!m_bLoadedIns) {
		EXT_LOCK (s_mtxTx) {
			if (!m_bLoadedIns) {
				ASSERT(m_nBytesOfHash && m_txIns.empty());
				Eng().Db->ReadTxIns(m_hash, _self);
				m_bLoadedIns = true;
			}
		}
	}
	return m_txIns;
}

void TxObj::Write(BinaryWriter& wr) const {
	wr << Ver;
	WritePrefix(wr);
	CoinSerialized::Write(wr, TxIns());
	CoinSerialized::Write(wr, TxOuts);
	wr << LockBlock;
}

void TxObj::Read(const BinaryReader& rd) {
	ASSERT(!m_bLoadedIns);
	DBG_LOCAL_IGNORE_NAME(E_COIN_Misbehaving, ignE_COIN_Misbehaving);

	Ver = rd.ReadUInt32();
	ReadPrefix(rd);
	CoinSerialized::Read(rd, m_txIns);
	m_bLoadedIns = true;
	CoinSerialized::Read(rd, TxOuts);
	if (m_txIns.empty() || TxOuts.empty())
		throw PeerMisbehavingExc(10);
	LockBlock = rd.ReadUInt32();
	if (LockBlock >= 500000000)
		LockTimestamp = DateTime::FromUnix(LockBlock);
}

bool TxObj::IsFinal(int height, const DateTime dt) const {
    // Time based nLockTime implemented in 0.1.6
    if (LockTimestamp.Ticks == 0)
        return true;
/*!!!        if (height == 0)
        nBlockHeight = nBestHeight;
    if (nBlockTime == 0)
        nBlockTime = GetAdjustedTime(); */
    if (LockTimestamp < (LockTimestamp < DateTime::FromUnix(LOCKTIME_THRESHOLD) ? height : dt))
        return true;
	EXT_FOR (const TxIn& txIn, TxIns()) {
        if (!txIn.IsFinal())
            return false;
	}
    return true;
}


bool Tx::AllowFree(double priority) {
	CoinEng& eng = Eng();
	return eng.AllowFreeTxes && (priority > eng.ChainParams.CoinValue * 144 / 250);
}

bool Tx::TryFromDb(const HashValue& hash, Tx *ptx) {
	CoinEng& eng = Eng();

	EXT_LOCK(eng.Caches.Mtx) {
		ChainCaches::CHashToTxCache::iterator it = eng.Caches.HashToTxCache.find(hash);
		if (it != eng.Caches.HashToTxCache.end()) {
			if (ptx)
				*ptx = it->second.first;
			return true;
		}
	}
	if (!eng.Db->FindTx(hash, ptx))
		return false;
	if (ptx) {
		// ASSERT(ReducedHashValue(Hash(*ptx)) == ReducedHashValue(hash));

		EXT_LOCK (eng.Caches.Mtx) {
			eng.Caches.HashToTxCache.insert(make_pair(hash, *ptx));
		}
	}
	return true;
}

Tx Tx::FromDb(const HashValue& hash) {
	Coin::Tx r;
	if (!TryFromDb(hash, &r)) {
		TRC(2, "NotFound Tx: " << hash);
		throw TxNotFoundExc(hash);
	}
	return r;
}

void Tx::EnsureCreate() {
	if (!m_pimpl)
		m_pimpl = Eng().CreateTxObj();
}

void Tx::Write(BinaryWriter& wr) const {
	m_pimpl->Write(wr);
}

void Tx::Read(const BinaryReader& rd) {
	EnsureCreate();
	m_pimpl->Read(rd);
}

bool Tx::IsNewerThan(const Tx& txOld) const {
	if (TxIns().size() != txOld.TxIns().size())
		return false;
	bool r = false;
	UInt32 lowest = UINT_MAX;
	for (int i=0; i<TxIns().size(); ++i) {
		const TxIn &txIn = TxIns()[i],
			&oldIn = txOld.TxIns()[i];
		if (txIn.PrevOutPoint != oldIn.PrevOutPoint)
			return false;
		if (txIn.Sequence != oldIn.Sequence) {
			if (txIn.Sequence <= lowest)
				r = false;
			if (oldIn.Sequence <= lowest)
				r = true;
			lowest = std::min(lowest, std::min(txIn.Sequence, oldIn.Sequence));
		}
	}
	return r;
}

Blob ToCompressedKey(const ConstBuf& cbuf) {
	ASSERT(cbuf.Size==33 || cbuf.Size==65);

	if (cbuf.Size == 33)
		return cbuf;
	CngKey key = CngKey::Import(cbuf, CngKeyBlobFormat::OSslEccPublicBlob);
	Blob r = key.Export(CngKeyBlobFormat::OSslEccPublicCompressedBlob);
	r.data()[0] |= 0x80;
	if (cbuf.P[0] & 2)
		r.data()[0] |= 4; // POINT_CONVERSION_HYBRID
#ifdef X_DEBUG//!!!D
	Blob test(r);
	test.data()[0] &= 0x7F;
	ASSERT(ConstBuf(test) == ConstBuf(cbuf.P, 33));
#endif
#ifdef X_DEBUG//!!!D
	ASSERT(ToUncompressedKey(r) == Blob(cbuf));
#endif
	return r;
}


/*!!!!ERROR
Blob ToCompressedKeyWithoutCheck(const ConstBuf& cbuf) {							// Just truncates last 32 bytes. Use only after checking hashes!
	ASSERT(cbuf.Size==33 || cbuf.Size==65);

	if (cbuf.Size == 33)
		return cbuf;
	Blob r(cbuf.P, 33);
	r.data()[0] |= 0x80;
	return r;
}
*/

static const BigInteger s_ec_p("FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFEFFFFFC2F", 16);

Blob ToUncompressedKey(const ConstBuf& cbuf) {
	if (cbuf.Size != 33 || !(cbuf.P[0] & 0x80))
		return cbuf;
	BigInteger x = OpensslBn::FromBinary(ConstBuf(cbuf.P+1, 32)).ToBigInteger();
	BigInteger xr = (x*x*x+7) % s_ec_p;
	BigInteger y = sqrt_mod(xr, s_ec_p);
	if (y.TestBit(0) != bool(cbuf.P[0] & 1))
		y = s_ec_p-y;
	Blob r(0, 65);
	r.data()[0] = 4;									//	POINT_CONVERSION_UNCOMPRESSED
	if (cbuf.P[0] & 4) {
		r.data()[0] |= 2 | (cbuf.P[0] & 1);				//  POINT_CONVERSION_HYBRID
	}
	memcpy(r.data()+1, cbuf.P+1, 32);
	OpensslBn(y).ToBinary(r.data()+33, 32);
	return r;
}

PubKeyHash160 DbPubKeyToHashValue160(const ConstBuf& mb) {
	return mb.Size == 20
		? PubKeyHash160(nullptr, HashValue160(mb))
		: PubKeyHash160(ToUncompressedKey(mb));
}

HashValue160 CoinEng::GetHash160ById(Int64 id) {
	EXT_LOCK (Caches.Mtx) {
		ChainCaches::CCachePkIdToPubKey::iterator it = Caches.m_cachePkIdToPubKey.find(id);	
		if (it != Caches.m_cachePkIdToPubKey.end())
			return it->second.first.Hash160;
	}
	Blob pk = Db->FindPubkey(id);
	if (!!pk) {
		PubKeyHash160 pkh = DbPubKeyToHashValue160(pk);
#ifdef X_DEBUG//!!!D
		CIdPk idpk(pkh.Hash160);
		ASSERT(idpk == id);
#endif
		EXT_LOCK (Caches.Mtx) {
			if (Caches.PubkeyCacheEnabled)
				Caches.m_cachePkIdToPubKey.insert(make_pair(id, pkh));
		}
		return pkh.Hash160;
	} else
		Throw(E_EXT_DB_NoRecord);
}

Blob CoinEng::GetPkById(Int64 id) {
	EXT_LOCK (Caches.Mtx) {
		ChainCaches::CCachePkIdToPubKey::iterator it = Caches.m_cachePkIdToPubKey.find(id);	
		if (it != Caches.m_cachePkIdToPubKey.end())
			return it->second.first.PubKey;
	}
	Blob pk = Db->FindPubkey(id);
	if (!!pk) {
		if (pk.Size == 20)
			Throw(E_COIN_InconsistentDatabase);
		PubKeyHash160 pkh = PubKeyHash160(ToUncompressedKey(pk));
		EXT_LOCK (Caches.Mtx) {
			if (Caches.PubkeyCacheEnabled)
				Caches.m_cachePkIdToPubKey.insert(make_pair(id, pkh));
		}
		return pkh.PubKey;
	} else
		Throw(E_EXT_DB_NoRecord);
}

bool CoinEng::GetPkId(const HashValue160& hash160, CIdPk& id) {
	id = CIdPk(hash160);
	EXT_LOCK (Caches.Mtx) {
		ChainCaches::CCachePkIdToPubKey::iterator it = Caches.m_cachePkIdToPubKey.find(id);	
		if (it != Caches.m_cachePkIdToPubKey.end())
			return hash160 == it->second.first.Hash160;
	}

	PubKeyHash160 pkh;
	Blob pk = Db->FindPubkey((Int64)id);
	if (!!pk) {
		if ((pkh = DbPubKeyToHashValue160(pk)).Hash160 != hash160)
			return false;
	} else {
		Throw(E_EXT_CodeNotReachable); //!!!
		Db->InsertPubkey((Int64)id, hash160);
		pkh = PubKeyHash160(nullptr, hash160);
	}
	EXT_LOCK (Caches.Mtx) {
		if (Caches.PubkeyCacheEnabled)
			Caches.m_cachePkIdToPubKey.insert(make_pair(id, pkh));
	}
	return true;
}

bool CoinEng::GetPkId(const ConstBuf& cbuf, CIdPk& id) {
	ASSERT(cbuf.Size != 20);

	if (!IsPubKey(cbuf))
		return false;

	HashValue160 hash160 = Hash160(cbuf);
	id = CIdPk(hash160);
	Blob compressed;
	try {
		DBG_LOCAL_IGNORE_NAME(MAKE_HRESULT(SEVERITY_ERROR, FACILITY_OPENSSL, EC_R_POINT_IS_NOT_ON_CURVE), ignEC_R_POINT_IS_NOT_ON_CURVE);
		DBG_LOCAL_IGNORE_NAME(MAKE_HRESULT(SEVERITY_ERROR, FACILITY_OPENSSL, ERR_R_EC_LIB), ignERR_R_EC_LIB);

		compressed = ToCompressedKey(cbuf);
	} catch (RCExc) {
		return false;
	}
	Blob pk = Db->FindPubkey((Int64)id);
	if (!!pk) {
		ConstBuf mb = pk;
		if (mb.Size != 20) {
			return mb == compressed;
		}
		if (hash160 != HashValue160(mb))
			return false;
		Throw(E_EXT_CodeNotReachable); //!!!
		Db->UpdatePubkey((Int64)id, compressed);
		EXT_LOCK (Caches.Mtx) {
			Caches.m_cachePkIdToPubKey[id] = PubKeyHash160(cbuf, hash160);
		}
		return true;
	}
	Throw(E_EXT_CodeNotReachable); //!!!
	Db->InsertPubkey((Int64)id, compressed);
	return true;
}

const Blob& TxOut::get_PkScript() const {
	if (!m_pkScript) {
#ifdef X_DEBUG//!!!D
		if (Value == 1014505)
			cout << "";
#endif

		CoinEng& eng = Eng();
		switch (m_typ) {
		case 20:
			{
				byte ar[25] = { OP_DUP, OP_HASH160,
								20,
								0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
								OP_EQUALVERIFY, OP_CHECKSIG };
				HashValue160 hash160 = eng.GetHash160ById(m_idPk);
				memcpy(ar+3, hash160.data(), 20);
				m_pkScript.AssignIfNull(Blob(ar, sizeof ar));
			}
			break;
		case 33:
		case 65:
			{
				Blob pk = eng.GetPkById(m_idPk);
				ASSERT(pk.Size == 33 || pk.Size == 65);
				Blob pkScript(0, pk.Size+2);
				pkScript.data()[0] = byte(pk.Size);
				memcpy(pkScript.data()+1, pk.constData(), pk.Size);
				pkScript.data()[pk.Size+1] = OP_CHECKSIG;
				m_pkScript.AssignIfNull(pkScript);
			}
			break;
		default:
			Throw(E_EXT_CodeNotReachable);
		}
	}
	return m_pkScript;
}


ConstBuf FindStandardHashAllSigPubKey(const ConstBuf& cbuf) {
	ConstBuf r(0, 0);
	if (cbuf.Size > 64) {
		int len = cbuf.P[0];
		if (len>64 && len<64+32 && cbuf.P[1]==0x30 && cbuf.P[2]==len-3 && cbuf.P[3]==2 && (cbuf.P[4]==0x20 || cbuf.P[4]==0x21) && cbuf.P[5+cbuf.P[4]] == 2 && cbuf.P[5+cbuf.P[4]+1]+cbuf.P[4]+4 == len-3 && cbuf.P[len]==1) {
			if (cbuf.Size == len+1)
				r = ConstBuf(cbuf.P+len+1, 0);
			else if (cbuf.Size > len+2) {
				int len2 = cbuf.P[1+len];
				if ((len2==33 || len2==65) && cbuf.Size == len+len2+2)
					r = ConstBuf(cbuf.P+len+1, len2+1);
			}
		}
	}
	return r;
}

void Tx::WriteTxIns(DbWriter& wr) const {
	CoinEng& eng = Eng();

	EXT_FOR (const TxIn& txIn, TxIns()) {
		byte typ = UInt32(-1)==txIn.Sequence ? 0x80 : 0;

#ifdef X_DEBUG//!!!D
		if (Height == 387 && TxIns().size() == 4) {
			static int s_n = 0;
			s_n = s_n;
			if (s_n++ == 3) {
				s_n = s_n;
			}
		}
#endif

		if (txIn.PrevOutPoint.IsNull())
			wr.Write7BitEncoded(0);
		else {
			ASSERT(txIn.PrevOutPoint.Index >= 0);

			pair<int, int> pp = wr.PTxHashesOutNums->StartingTxOutIdx(txIn.PrevOutPoint.TxHash);
			if (pp.second >= 0) {
				wr.Write7BitEncoded(1);			// current block
			} else {
				pp = eng.Db->FindPrevTxCoords(wr, Height, txIn.PrevOutPoint.TxHash);
			}
			wr.Write7BitEncoded(pp.second + txIn.PrevOutPoint.Index);

			ConstBuf pk = FindStandardHashAllSigPubKey(txIn.Script());
			while (pk.P) {
				if (pk.Size) {
					CConnectJob::CMap::iterator it = wr.ConnectJob->Map.find(Hash160(ConstBuf(pk.P+1, pk.Size-1)));
					if (it == wr.ConnectJob->Map.end() || !it->second.PubKey) {
#ifdef X_DEBUG//!!!D
						if (it != wr.ConnectJob->Map.end()) {
							Blob blo = it->second.PubKey;
						}
#endif
						break;
					}
					const TxOut& txOut = wr.ConnectJob->TxMap.GetOutputFor(txIn.PrevOutPoint);
					if (txOut.m_idPk.IsNull())
						break;
					typ |= 0x40;
				}
				ConstBuf sig(txIn.Script().constData()+5, pk.P-txIn.Script().constData()-6);
				ASSERT(sig.Size>=63 && sig.Size<71);
				int len1 = txIn.Script().constData()[4];
				typ |= 0x10;							// 0.58 bug workaround: typ should not be zero
				typ |= (len1 & 1) << 5;
				typ += byte(sig.Size-63);			
				(wr << typ).Write(sig.P, len1);
				wr.Write(sig.P+len1+2, sig.Size-len1-2);		// skip "0x02, len2" fields
//!!!R				if (pk.Size)
//					wr << UInt32(Int64(idPk));
				goto LAB_TXIN_WROTE;
			}
		}
		wr << byte(typ);		//  | SCRIPT_PKSCRIPT_GEN
		CoinSerialized::WriteBlob(wr, txIn.Script());
LAB_TXIN_WROTE:
		if (txIn.Sequence != UInt32(-1))
			wr.Write7BitEncoded(UInt32(txIn.Sequence));
	}
}

DbWriter& operator<<(DbWriter& wr, const Tx& tx) {
	CoinEng& eng = Eng();
	
	if (!wr.BlockchainDb) {
		wr.Write7BitEncoded(tx.m_pimpl->Ver);
		tx.m_pimpl->WritePrefix(wr);
		wr << tx.TxIns() << tx.TxOuts();
		wr.Write7BitEncoded(tx.LockBlock);
	} else {
		UInt64 v = UInt64(tx.m_pimpl->Ver) << 2;
		if (tx.IsCoinBase())
			v |= 1;
		if (tx.LockBlock)
			v |= 2;
		wr.Write7BitEncoded(v);
		tx.m_pimpl->WritePrefix(wr);
		if (tx.LockBlock)
			wr.Write7BitEncoded(tx.LockBlock);

		for (int i=0; i<tx.TxOuts().size(); ++i) {
			const TxOut& txOut = tx.TxOuts()[i];
#if UCFG_COIN_PUBKEYID_36BITS
			UInt64 valtyp = UInt64(txOut.Value) << 6;
#else
			UInt64 valtyp = UInt64(txOut.Value) << 5;
#endif
			HashValue160 hash160;
			Blob pk;
			CIdPk idPk;
			switch (byte typ = txOut.TryParseDestination(hash160, pk)) {
			case 20:
			case 33:
			case 65:
				if (eng.LiteMode) {
					wr.Write7BitEncoded(valtyp | 3);
					break;
				} else {
					CConnectJob::CMap::iterator it = wr.ConnectJob->Map.find(hash160);
					if (it != wr.ConnectJob->Map.end() && !!it->second.PubKey) {
#ifdef X_DEBUG//!!!D
						if (tx.Height == 16296 && i==1 && tx.m_pimpl->m_hash.data()[31] == 0xEE) {
							Blob fpk = it->second.PubKey;
							HashValue160 hv = Hash160(ToUncompressedKey(fpk));
							ASSERT(hash160 == hv);
							i = 1;
						}
#endif

						idPk = CIdPk(hash160);
#if UCFG_COIN_PUBKEYID_36BITS
						wr.Write7BitEncoded(valtyp | (20==typ ? 1 : 2) | ((Int64(idPk) & 0xF00000000)>>30));
#else
						wr.Write7BitEncoded(valtyp | (20==typ ? 1 : 2) | ((Int64(idPk) & 0x700000000)>>30));
#endif
						wr << UInt32(Int64(txOut.m_idPk = idPk));
						break;
					}
				}
			default:
				wr.Write7BitEncoded(valtyp);						// SCRIPT_PKSCRIPT_GEN
				CoinSerialized::WriteBlob(wr, txOut.get_PkScript());
			}
		}
	}	
	return wr;
}

const DbReader& operator>>(const DbReader& rd, Tx& tx) {
	CoinEng& eng = Eng();

	UInt64 v;
//!!!?	if (tx.Ver != 1)
//		Throw(E_EXT_New_Protocol_Version);

	tx.EnsureCreate();

	if (!rd.BlockchainDb) {
		v = rd.Read7BitEncoded();
		tx.m_pimpl->Ver = (UInt32)v;
		tx.m_pimpl->ReadPrefix(rd);
		rd >> tx.m_pimpl->m_txIns >> tx.m_pimpl->TxOuts;
		tx.m_pimpl->m_bLoadedIns = true;
		tx.m_pimpl->LockBlock = (UInt32)rd.Read7BitEncoded();
	} else {
		v = rd.Read7BitEncoded();
		tx.m_pimpl->Ver = (UInt32)(v >> 2);
		tx.m_pimpl->ReadPrefix(rd);
		tx.m_pimpl->m_bIsCoinBase = v & 1;
		if (v & 2)
			tx.LockBlock = (UInt32)rd.Read7BitEncoded();

		while (!rd.BaseStream.Eof()) {
			UInt64 valtyp = rd.Read7BitEncoded();
#if UCFG_COIN_PUBKEYID_36BITS
			UInt64 value = valtyp>>6;
#else
			UInt64 value = valtyp>>5;
#endif
			tx.TxOuts().push_back(TxOut(value, Blob(nullptr)));
			TxOut& txOut = tx.TxOuts().back();
			static const byte s_types[] = { 0, 20, 33, 0x7F };
			switch (byte typ = s_types[valtyp & 3]) {
			case 0:
				txOut.m_pkScript = CoinSerialized::ReadBlob(rd);
				break;
			case 20:
			case 33:
			case 65:
				txOut.m_typ = typ;
				txOut.m_pkScript = nullptr;
#if UCFG_COIN_PUBKEYID_36BITS
				txOut.m_idPk = CIdPk(Int64(rd.ReadUInt32()) | ((valtyp & 0x3C) << 30));
#else
				txOut.m_idPk = CIdPk(Int64(rd.ReadUInt32()) | ((valtyp & 0x1C) << 30));
#endif
				break;
			default:
				Throw(E_FAIL);
			}		
		}
	}	
	if (tx.LockBlock >= 500000000)
		tx.m_pimpl->LockTimestamp = DateTime::FromUnix(tx.LockBlock);
	return rd;
}

void Tx::Check() const {
	CoinEng& eng = Eng();

	if (TxIns().empty() || TxOuts().empty())
		Throw(E_FAIL);
	Int64 nOut = 0;
	EXT_FOR(const TxOut& txOut, TxOuts()) {
        if (!txOut.IsEmpty()) {
			if (txOut.Value < eng.ChainParams.MinTxOutAmount)
				Throw(E_COIN_TxOutBelowMinimum);
			if (txOut.Value > eng.ChainParams.MaxMoney)
				throw PeerMisbehavingExc(100);
		}
		eng.CheckMoneyRange(nOut += eng.CheckMoneyRange(txOut.Value));
	}
	eng.OnCheck(_self);
}

void CoinEng::UpdateMinFeeForTxOuts(Int64& minFee, const Int64& baseFee, const Tx& tx) {
    if (minFee < baseFee) {                   // To limit dust spam, require MIN_TX_FEE/MIN_RELAY_TX_FEE if any output is less than 0.01
		const Int64 cent = ChainParams.CoinValue / 100;
		EXT_FOR (const TxOut txOut, tx.TxOuts()) {
            if (txOut.Value < cent) {
                minFee = baseFee;
				break;
			}
		}
	}
}

UInt32 Tx::GetSerializeSize() const {
	return EXT_BIN(_self).Size;
}

Int64 Tx::GetMinFee(UInt32 blockSize, bool bAllowFree, MinFeeMode mode) const {
	CoinEng& eng = Eng();
    Int64 nBaseFee = mode==MinFeeMode::Relay ? eng.GetMinRelayTxFee() : eng.ChainParams.MinTxFee;

    UInt32 nBytes = GetSerializeSize();
    UInt32 nNewBlockSize = blockSize + nBytes;
    Int64 nMinFee = (1 + (Int64)nBytes / 1000) * nBaseFee;

    if (bAllowFree) {
        if (blockSize == 1) {          
            if (nBytes < 10000)					// Transactions under 10K are free
                nMinFee = 0;					// (about 4500bc if made of 50bc inputs)
        } else if (nNewBlockSize < 27000)		// Free transaction area
			nMinFee = 0;
    }
    
	eng.UpdateMinFeeForTxOuts(nMinFee, nBaseFee, _self);
    
    if (blockSize != 1 && nNewBlockSize >= MAX_BLOCK_SIZE_GEN/2) {					// Raise the price as the block approaches full
        if (nNewBlockSize >= MAX_BLOCK_SIZE_GEN)
            return eng.ChainParams.MaxMoney;
        nMinFee *= MAX_BLOCK_SIZE_GEN / (MAX_BLOCK_SIZE_GEN - nNewBlockSize);
    }

	try {
		eng.CheckMoneyRange(nMinFee);
	} catch (RCExc) {
		return eng.ChainParams.MaxMoney;
	}
    return nMinFee;
}

Int64 Tx::get_ValueOut() const {
	CoinEng& eng = Eng();

	Int64 r = 0;
	EXT_FOR (const TxOut& txOut, TxOuts()) {
		eng.CheckMoneyRange(r += eng.CheckMoneyRange(txOut.Value));
	}
	return r;
}

Int64 Tx::get_Fee() const {
	if (IsCoinBase())
		return 0;
	Int64 sum = 0;
	EXT_FOR (const TxIn& txIn, TxIns()) {
		sum += Tx::FromDb(txIn.PrevOutPoint.TxHash).TxOuts().at(txIn.PrevOutPoint.Index).Value;
	}
	return sum - ValueOut;
}

decimal64 Tx::GetDecimalFee() const {
	return make_decimal64(Fee, -Coin::Eng().ChainParams.CoinValueExp());
}


int Tx::get_DepthInMainChain() const {
	CoinEng& eng = Eng();
	Block bestBlock = eng.BestBlock();
	return Height >= 0 && bestBlock ? bestBlock.Height-Height+1 : 0;
}

int Tx::get_SigOpCount() const {
	int r = 0;
	EXT_FOR (const TxIn& txIn, TxIns()) {
		if (txIn.PrevOutPoint.Index >= 0)
			r += CalcSigOpCount1(txIn.Script());
	}
	EXT_FOR (const TxOut& txOut, TxOuts()) {
		try {																		//!!! should be more careful checking
			DBG_LOCAL_IGNORE_NAME(E_EXT_EndOfStream, ignE_EXT_EndOfStream);
			
			r += CalcSigOpCount1(txOut.get_PkScript());
		} catch (RCExc) {			
		}
	}
	return r;
}

int Tx::GetP2SHSigOpCount(const CTxMap& txMap) const {
	if (IsCoinBase())
		return 0;
	int r = 0;
	EXT_FOR (const TxIn& txIn, TxIns()) {
		const TxOut& txOut = txMap.GetOutputFor(txIn.PrevOutPoint);
		if (IsPayToScriptHash(txOut.get_PkScript()))
			r += CalcSigOpCount(txOut.get_PkScript(), txIn.Script());
	}
	return r;
}

void Tx::CheckInOutValue(Int64 nValueIn, Int64& nFees, Int64 minFee, const Target& target) const {
	Int64 valOut = ValueOut;
	if (IsCoinStake())
		m_pimpl->CheckCoinStakeReward(valOut - nValueIn, target);
	else {
		if (nValueIn < valOut)
			Throw(E_COIN_ValueInLessThanValueOut);
		Int64 nTxFee = nValueIn-valOut;
		if (nTxFee < minFee) {
			Throw(E_COIN_TxFeeIsLow);
		}
		nFees = Eng().CheckMoneyRange(nFees + nTxFee);
	}
}

void Tx::ConnectInputs(CTxMap& txMap, Int32 height, int& nBlockSigOps, Int64& nFees, bool bBlock, bool bMiner, Int64 minFee, const Target& target) const {
	CoinEng& eng = Eng();

	HashValue hashTx = Hash(_self);
	int nSigOp = nBlockSigOps + SigOpCount;
	if (!IsCoinBase()) {
		vector<Tx> vTxPrev;
		Int64 nValueIn = 0;
		if (!eng.LiteMode) {
//			TRC(3, TxIns.size() << " TxIns");

			for (int i=0; i<TxIns().size(); ++i) {
				const TxIn& txIn = TxIns()[i];
				OutPoint op = txIn.PrevOutPoint;
		
				Tx txPrev;
				if (bBlock || bMiner) {
					CTxMap::iterator it = txMap.find(op.TxHash);
					txPrev = it!=txMap.end() ? it->second : FromDb(op.TxHash);
				} else {
					EXT_LOCK (eng.TxPool.Mtx) {
						CoinEng::CTxPool::CHashToTx::iterator it = eng.TxPool.m_hashToTx.find(op.TxHash);
						if (it != eng.TxPool.m_hashToTx.end())
							txPrev = it->second;
					}
					if (!txPrev)
						txPrev = FromDb(op.TxHash);
				}
				if (op.Index >= txPrev.TxOuts().size())
					Throw(E_FAIL);
				const TxOut& txOut = txPrev.TxOuts()[op.Index];
				if (t_bPayToScriptHash) {
					if ((nSigOp += CalcSigOpCount(txOut.PkScript, txIn.Script())) > MAX_BLOCK_SIGOPS)
						Throw(E_COIN_TxTooManySigOps);
				}

				if (txPrev.IsCoinBase())
					eng.CheckCoinbasedTxPrev(height, txPrev);

				if (!bBlock || eng.BestBlockHeight() > eng.ChainParams.LastCheckpointHeight-INITIAL_BLOCK_THRESHOLD)	// Skip ECDSA signature verification when connecting blocks (fBlock=true) during initial download
					VerifySignature(txPrev, _self, i);

				eng.CheckMoneyRange(nValueIn += txOut.Value);
				vTxPrev.push_back(txPrev);
			}
		}

		if (bBlock)
			eng.OnConnectInputs(_self, vTxPrev, bBlock, bMiner);

		if (!eng.LiteMode)
			CheckInOutValue(nValueIn, nFees, minFee, target);
	}
	if (nSigOp > MAX_BLOCK_SIGOPS)
		Throw(E_COIN_TxTooManySigOps);
	nBlockSigOps = nSigOp;
	txMap[hashTx] = _self;
}

DbWriter& operator<<(DbWriter& wr, const CTxes& txes) {
	if (!wr.BlockchainDb)
		Throw(E_NOTIMPL);	
	EXT_FOR (const Tx& tx, txes) {
		wr << tx;
	}
	return wr;
}

const DbReader& operator>>(const DbReader& rd, CTxes& txes) {
	if (!rd.BlockchainDb)
		Throw(E_NOTIMPL);
	ASSERT(txes.empty());
	while (!rd.BaseStream.Eof()) {
		Tx tx;
		rd >> tx;
//!!!R		tx.m_pimpl->N = txes.size();
		tx.Height = rd.PCurBlock->Height;
		txes.push_back(tx);
	}
	return rd;
}


} // Coin::

