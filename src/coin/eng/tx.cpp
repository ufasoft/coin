/*######   Copyright (c) 2011-2019 Ufasoft  http://ufasoft.com  mailto:support@ufasoft.com,  Sergey Pavlov  mailto:dev@ufasoft.com ####
#                                                                                                                                     #
# 		See LICENSE for licensing information                                                                                         #
#####################################################################################################################################*/

#include <el/ext.h>

using namespace std::placeholders;

#include <el/crypto/hash.h>
#include <el/crypto/ext-openssl.h>

#include <el/num/mod.h>
using namespace Ext::Num;

#include "coin-protocol.h"
#include "script.h"
#include "crypter.h"
#include "eng.h"
#include "coin-model.h"

#include <crypto/cryp/secp256k1.h>

#if UCFG_COIN_ECC == 'S'
#else
#	define OPENSSL_NO_SCTP
#	include <openssl/err.h>
#	include <openssl/ec.h>
#endif

namespace Coin {

unsigned WitnessSigOps(uint8_t ver, RCSpan program, const vector<StackValue>& witness) {
    return ver != 0
        ? 0
        : program.size() == WITNESS_V0_KEYHASH_SIZE ? 1
        : program.size() == WITNESS_V0_SCRIPTHASH_SIZE && !witness.empty() ? CalcSigOpCount1(witness.back(), true)
        : 0;
}

unsigned TxIn::CountWitnessSigOps() const {
    if (!t_features.SegWit)
        return 0;

    Span scriptPubKey = PrevTxOut.ScriptPubKey;
    auto pp = Script::WitnessProgram(scriptPubKey);
    if (pp.first.data())
        return WitnessSigOps(pp.second, pp.first, Witness);

    Span scriptSig = Script();
    if (Script::IsPayToScriptHash(scriptPubKey) && Script::IsPushOnly(scriptSig)) {
        LiteInstr lastInstr;
        for (CMemReadStream stm(scriptSig); GetInstr(stm, lastInstr);)
            ;
        pp = Script::WitnessProgram(lastInstr.Value);
        if (pp.first.data())
            return WitnessSigOps(pp.second, pp.first, Witness);
    }
    return 0;
}

void TxIn::Write(ProtocolWriter& wr, bool writeForSigHash) const {
	PrevOutPoint.Write(wr);
	CoinSerialized::WriteSpan(wr, !wr.ForSignatureHash ? Script() : writeForSigHash ? wr.ClearedScript : Span());
	wr << (!wr.HashTypeSingle && !wr.HashTypeNone || writeForSigHash ? Sequence : 0);
}

void TxIn::Read(const BinaryReader& rd) {
	PrevOutPoint.Read(rd);
	m_script = CoinSerialized::ReadBlob(rd);
	rd >> Sequence;
}

void TxOut::Write(BinaryWriter& wr) const {
	CoinSerialized::WriteSpan(wr << Value, get_ScriptPubKey());
}

void TxOut::Read(const BinaryReader& rd) {
	Value = rd.ReadInt64();
	uint32_t size = CoinSerialized::ReadCompactSize(rd);
	if (size > MAX_BLOCK_WEIGHT)	//!!!?
		Throw(ExtErr::Protocol_Violation);
	m_scriptPubKey.resize(size, false);
	rd.Read(m_scriptPubKey.data(), size);
}

Tx CTxMap::Find(const HashValue& hash) const {
	return Lookup(m_map, hash).value_or(Tx());
}

const Tx& CTxMap::Get(const HashValue& hash) const {
	return m_map.at(hash);
}

const TxOut& CTxMap::GetOutputFor(const OutPoint& prev) const {
	auto it = m_map.find(prev.TxHash);
	if (it == m_map.end())
		throw TxNotFoundException(prev.TxHash);
	return it->second.TxOuts().at(prev.Index);
}

void CTxMap::Add(const Tx& tx) {
	if (!m_map.insert(make_pair(Hash(tx), tx)).second)
		Throw(E_FAIL);
}

bool CFutureTxMap::Contains(const HashValue& hash) const {
	return EXT_LOCKED(m_mtx, m_map.count(hash));
}

Tx CFutureTxMap::Find(const HashValue& hash) const {
	shared_future<TxFeeTuple> ft;
	EXT_LOCK(m_mtx) {
		if (auto o = Lookup(m_map, hash))
			ft = o.value();
		else
			return nullptr;
	}
	return ft.get().Tx;
}

const Tx& CFutureTxMap::Get(const HashValue& hash) const {
	shared_future<TxFeeTuple> ft = EXT_LOCKED(m_mtx, m_map.at(hash)); // not reference to prevend deadlock
	return ft.get().Tx;
}

const TxOut& CFutureTxMap::GetOutputFor(const OutPoint& prev) const {
	return Get(prev.TxHash).TxOuts().at(prev.Index);
}

static TxFeeTuple LoadTxFromDb(HashValue hash) {
	return Tx::FromDb(hash);
}

void CFutureTxMap::Add(const Tx& tx) {
	HashValue hash = Hash(tx);
	promise<TxFeeTuple> pr;
	pr.set_value(TxFeeTuple(tx));
	EXT_LOCK(m_mtx) {
		if (!m_map.insert(make_pair(hash, pr.get_future().share())).second)
			Throw(E_FAIL);
	}
}

void CFutureTxMap::Add(const HashValue& hash, const shared_future<TxFeeTuple>& ft) {
	EXT_LOCK(m_mtx) {
		if (!m_map.insert(make_pair(hash, ft)).second)
			Throw(E_FAIL);
	}
}

void CFutureTxMap::Ensure(const HashValue& hash) {
	EXT_LOCK(m_mtx) {
		if (!m_map.count(hash))
			m_map.insert(make_pair(hash, std::async(LoadTxFromDb, hash).share()));
	}
}

static Txo LoadTxoFromDbAsync(CoinEng* eng, OutPoint op, int height) {
	Tx tx;
	if (eng->Db->FindTx(op.TxHash, &tx)) {		// Don't use the cache as it is usually one-time operation
		if (tx->IsCoinBase())
			eng->CheckCoinbasedTxPrev(height, tx.Height);
		try {
			return tx.TxOuts().at(op.Index);
		} catch (out_of_range&) {
		}
	}
	TRC(2, "NotFound Txo: " << op);
	throw TxNotFoundException(CoinErr::TxMissingInputs, op.TxHash);	//!!!TODO throw OutPointNotFoundException
}

void TxoMap::Add(const OutPoint& op, int height) {
	auto launchType = UCFG_COIN_TX_CONNECT_FUTURES ? launch::async : launch::deferred;
	EXT_LOCK(m_mtx) {
		auto pp = m_map.insert(make_pair(op, Entry()));
		Entry& entry = pp.first->second;
		if (pp.second)
			entry.Ft = std::async(launchType, LoadTxoFromDbAsync, &m_eng, op, height);
		else if (entry.InCurrentBlock)
			entry.InCurrentBlock = false;
		else
			Throw(E_FAIL);
	}
}

void TxoMap::AddAllOuts(const HashValue& hashTx, const Tx& tx) {
	auto& txOuts = tx.TxOuts();
	OutPoint op(hashTx);
	EXT_LOCK(m_mtx) {
		for (int i = 0; i < txOuts.size(); ++i) {
			Txo toh = { txOuts[op.Index = i] };
			promise<Txo> pr;
			pr.set_value(toh);
			if (!m_map.insert(make_pair(op, Entry(pr.get_future(), true))).second)
				Throw(E_FAIL);
		}
	}
}

Txo TxoMap::Get(const OutPoint& op) const {
	shared_future<Txo> *pft;
	EXT_LOCK(m_mtx) {
		auto it = m_map.find(op);
		if (it == m_map.end())
			Throw(CoinErr::TxMissingInputs);
		pft = &it->second.Ft;
	}
	return pft->get();
}

bool CoinsView::HasInput(const OutPoint& op) const {
	if (auto o = Lookup(m_outPoints, op))
		return o.value();
	if (!Eng.Db->FindTx(op.TxHash, 0)) //!!! Double find. Should be optimized. But we need this check during Reorganize()
		Throw(CoinErr::TxNotFound);
	vector<bool> vSpend = Eng.Db->GetCoinsByTxHash(op.TxHash);
	return m_outPoints[op] = op.Index < vSpend.size() && vSpend[op.Index];
}

void CoinsView::SpendInput(const OutPoint& op) {
	m_outPoints[op] = false;
}

Tx CoinsView::Find(const HashValue& hash) const {
	Tx r;
	Eng.Db->FindTx(hash, &r);
	return r;
}

const Tx& CoinsView::Get(const HashValue& hash) const {
	auto it = TxMap.m_map.find(hash);
	if (it != TxMap.m_map.end())
		return it->second;
	Tx tx;
	if (!Eng.Db->FindTx(hash, &tx))
		Throw(CoinErr::TxNotFound);
	TxMap.Add(tx);
	return TxMap.Get(hash);
}

const TxOut& CoinsView::GetOutputFor(const OutPoint& prev) const {
	return Get(prev.TxHash).TxOuts().at(prev.Index);
}

Txo CoinsView::Get(const OutPoint& op) const {
	return GetOutputFor(op);
}

void CoinsView::Add(const Tx& tx) {
	Throw(E_NOTIMPL);
}

TxObj::TxObj()
	: Height(-1)
	, LockBlock(0)
	, m_nBytesOfHash(0)
	, m_bLoadedIns(false) {
}

void Tx::WriteTxIns(DbWriter& wr) const {
	CoinEng& eng = Eng();
	int nIn = 0;
	EXT_FOR(const TxIn& txIn, TxIns()) {
		uint8_t typ = uint32_t(-1) == txIn.Sequence ? 0x80 : 0;
		if (txIn.PrevOutPoint.IsNull())
			wr.Write7BitEncoded(0);
		else {
			ASSERT(txIn.PrevOutPoint.Index >= 0);

			pair<int, int> pp = wr.PTxHashesOutNums->StartingTxOutIdx(txIn.PrevOutPoint.TxHash);
			if (pp.second >= 0) {
				wr.Write7BitEncoded(1); // current block
			} else {
				pp = eng.Db->FindPrevTxCoords(wr, Height, txIn.PrevOutPoint.TxHash);
			}
			wr.Write7BitEncoded(pp.second + txIn.PrevOutPoint.Index);

			Span scriptSig = txIn.Script();
			Span pk = FindStandardHashAllSigPubKey(scriptSig);
			if (txIn.RecoverPubKeyType) {
				typ |= txIn.RecoverPubKeyType;
				(wr << typ).BaseStream.Write(Sec256Signature(scriptSig.subspan(1, pk.data() - scriptSig.data() - 2)).ToCompact());
				goto LAB_TXIN_WROTE;
			}
#if 0 //!!!R
			if (pk.P && !pk.Size) {

					/*!!!
					ConstBuf sig(txIn.Script().constData()+1, pk.P-txIn.Script().constData()-2);
					Blob blobPk(pk.P+1, pk.Size-1);		//!!!O
					SignatureHasher hasher(*m_pimpl);
					HashValue hashSig = hasher.Hash(Tx::FromDb(txIn.PrevOutPoint.TxHash).TxOuts()[txIn.PrevOutPoint.Index].PkScript, nIn, 1);
					for (uint8_t recid=0; recid<3; ++recid) {
						if (Sec256Dsa::RecoverPubKey(hashSig, sig, recid, pk.Size<35) == blobPk) {
							typ |= 8;
							subType = uint8_t((pk.Size<35 ? 4 : 0) | recid);
							goto LAB_RECOVER_OK;
						}
					}
					{
						CConnectJob::CMap::iterator it = wr.ConnectJob->Map.find(Hash160(ConstBuf(pk.P+1, pk.Size-1)));
						if (it == wr.ConnectJob->Map.end() || !it->second.PubKey)
							break;
						const TxOut& txOut = wr.ConnectJob->TxMap.GetOutputFor(txIn.PrevOutPoint);
						if (txOut.m_idPk.IsNull())
							break;
					}
					typ |= 0x40;
					*/
				ConstBuf sig(txIn.Script().constData()+5, pk.P-txIn.Script().constData()-6);
				ASSERT(sig.Size>=63 && sig.Size<71);
				int len1 = txIn.Script().constData()[4];
				typ |= 0x10;							// 0.58 bug workaround: typ should not be zero
				typ |= (len1 & 1) << 5;
				typ += uint8_t(sig.Size-63);
				(wr << typ).Write(sig.P, len1);
				wr.Write(sig.P+len1+2, sig.Size-len1-2);		// skip "0x02, len2" fields
				goto LAB_TXIN_WROTE;
			}
#endif
		}
		wr << uint8_t(typ); //  | SCRIPT_PKSCRIPT_GEN
		CoinSerialized::WriteSpan(wr, txIn.Script());
	LAB_TXIN_WROTE:
		if (txIn.Sequence != uint32_t(-1))
			wr.Write7BitEncoded(uint32_t(txIn.Sequence));
		++nIn;
	}
}

void TxObj::ReadTxIns(const DbReader& rd) const {
	CoinEng& eng = Eng();
	for (int nIn = 0; !rd.BaseStream.Eof(); ++nIn) {
		int64_t blockordBack = rd.Read7BitEncoded(), blockHeight = -1;
		//		pair<int, OutPoint> pp(-1, OutPoint());
		TxIn txIn;
		if (0 == blockordBack)
			txIn.PrevOutPoint = OutPoint();
		else {
			int idx = (int)rd.Read7BitEncoded();
			if (rd.ReadTxHashes) { //!!!? always true
				blockHeight = Height - (blockordBack - 1);
				//!!!R				txIn.PrevOutPoint = (pp = rd.Eng->Db->GetTxHashesOutNums((int)blockHeight).OutPointByIdx(idx)).second;
				pair<OutPoint, TxOut> pp = eng.Db->GetOutPointTxOut((int)blockHeight, idx);
				txIn.PrevOutPoint = pp.first;
				txIn.PrevTxOut = pp.second;
			}
		}
		uint8_t typ = rd.ReadByte();
		switch (typ & 0x7F) {
		case 0: txIn.m_script = CoinSerialized::ReadBlob(rd);
#ifdef X_DEBUG //!!!D
			if (txIn.m_script.Size > 80) {
				ConstBuf pk = FindStandardHashAllSigPubKey(txIn.Script());
				if (pk.P) {
					ConstBuf sig(txIn.Script().constData() + 1, pk.P - txIn.Script().constData() - 2);
					Sec256Signature sigObj(sig), sigObj2;
					sigObj2.AssignCompact(sigObj.ToCompact());
					Blob ser = sigObj2.Serialize();

					if (!(ser == sig)) {
						ser = sigObj2.Serialize();
						ser = ser;
					}
				}
			}
#endif
			break;
		case 0x7F:
			if (eng.Mode == EngMode::Lite)
				break;
			//!!!?			Throw(E_FAIL);
		default:
			if (typ & 8) {
				array<uint8_t, 64> ar;
				rd.BaseStream.ReadBuffer(ar.data(), ar.size());
				Sec256Signature sigObj;
				sigObj.AssignCompact(ar);
				Blob ser = sigObj.Serialize();
				Blob sig(0, ser.size() + 2);
				uint8_t* data = sig.data();
				data[0] = uint8_t(ser.size() + 1);
				memcpy(data + 1, ser.constData(), ser.size());
				data[sig.size() - 1] = 1;
				if (typ & 0x40) {
					txIn.m_pTxo = this;
					txIn.RecoverPubKeyType = uint8_t(typ & 0x4F);
					txIn.m_sig = sig;
				} else
					txIn.m_script = sig;
			} else {
				int len = (typ & 0x07) + 63;
				ASSERT(len > 63 || (typ & 0x10)); // WriteTxIns bug workaround
				Blob sig(0, len + 6);
				uint8_t* data = sig.data();

				data[0] = uint8_t(len + 5);
				data[1] = 0x30;
				data[2] = uint8_t(len + 2);
				data[3] = 2;
				data[4] = 0x20 | ((typ & 0x20) >> 5);
				rd.Read(data + 5, data[4]);
				data[5 + data[4]] = 2;
				uint8_t len2 = uint8_t(len - data[4] - 2);
				data[5 + data[4] + 1] = len2;
				rd.Read(data + 5 + data[4] + 2, len2);
				data[5 + len] = 1;
				if (typ & 0x40) {
					txIn.m_sig = sig;
				} else
					txIn.m_script = sig;
			}
		}
		txIn.Sequence = (typ & 0x80) ? uint32_t(-1) : uint32_t(rd.Read7BitEncoded());
		m_txIns.push_back(txIn);
	}
}

static mutex s_mtxTx;

const vector<TxIn>& TxObj::TxIns() const {
	if (!m_bLoadedIns) {
		EXT_LOCK(s_mtxTx) {
			if (!m_bLoadedIns) {
				ASSERT(m_nBytesOfHash && m_txIns.empty());
				Eng().Db->ReadTxIns(m_hash, _self);
				m_bLoadedIns = true;
			}
		}
	}
	return m_txIns;
}

void TxObj::Write(ProtocolWriter& wr) const {
	wr << Ver;
	WritePrefix(wr);
	auto hasWitness = wr.WitnessAware && HasWitness();
	if (hasWitness)
		wr << uint8_t(0) << uint8_t(1);

	auto& txIns = TxIns();
	if (wr.HashTypeAnyoneCanPay) {
		WriteCompactSize(wr, 1);
		txIns[wr.NIn].Write(wr, true);
	} else {
		size_t nTxIn = txIns.size();
		WriteCompactSize(wr, nTxIn);
		for (size_t i = 0; i < nTxIn; ++i)
			txIns[i].Write(wr, i == wr.NIn);
	}

	auto nOuts = wr.HashTypeNone ? 0 : wr.HashTypeSingle ? wr.NIn + 1 : (int)TxOuts.size();
	WriteCompactSize(wr, nOuts);
	for (size_t i = 0; i < nOuts; ++i)
		if (wr.HashTypeSingle && i != wr.NIn)
			CoinSerialized::WriteSpan(wr << int64_t(-1), Span());
		else
			TxOuts[i].Write(wr);

	if (hasWitness)
		for (auto& txIn : txIns) {
			WriteCompactSize(wr, txIn.Witness.size());
			for (auto& chunk : txIn.Witness)
				CoinSerialized::WriteSpan(wr, chunk);
    	}
	wr << LockBlock;
	WriteSuffix(wr);
}

void TxObj::Read(const ProtocolReader& rd) {
	ASSERT(!m_bLoadedIns);
	DBG_LOCAL_IGNORE_CONDITION(CoinErr::Misbehaving);

	Ver = rd.ReadUInt32();
	ReadPrefix(rd);
	CoinSerialized::Read(rd, m_txIns);
	if (!m_txIns.empty() || !rd.WitnessAware) {
		m_bLoadedIns = true;
		CoinSerialized::Read(rd, TxOuts);
	} else {
		uint8_t flag = rd.ReadByte();
		if (!flag)
			throw PeerMisbehavingException(10);
		CoinSerialized::Read(rd, m_txIns);
		m_bLoadedIns = true;
		CoinSerialized::Read(rd, TxOuts);
		if (flag & 1)
			for (auto& txIn : m_txIns) {
				txIn.Witness.resize(CoinSerialized::ReadCompactSize(rd));
				for (auto& chunk : txIn.Witness)
					chunk = CoinSerialized::ReadBlob(rd);
			}
	}
	if (m_txIns.empty() || TxOuts.empty())
		throw PeerMisbehavingException(10);
	LockBlock = rd.ReadUInt32();
	if (Tx::LocktimeTypeOf(LockBlock) == Tx::LocktimeType::Timestamp)
		LockTimestamp = DateTime::from_time_t(LockBlock);
	ReadSuffix(rd);
}

bool TxObj::HasWitness() const {
	for (auto& txIn : TxIns())
		if (!txIn.Witness.empty())
			return true;
	return false;
}

bool TxObj::IsFinal(int height, const DateTime dt) const {
	if (LockTimestamp.Ticks == 0)
		return true;
	/*!!!        if (height == 0)
        nBlockHeight = nBestHeight;
    if (nBlockTime == 0)
        nBlockTime = GetAdjustedTime(); */
	if (Tx::LocktimeTypeOf(LockBlock) == Tx::LocktimeType::Block) {
		if (LockBlock < height)
			return true;
	} else if (LockTimestamp < dt)
		return true;
	for (auto& txIn : TxIns())
        if (!txIn.IsFinal())
            return false;
    return true;
}

bool Tx::AllowFree(double priority) {
	CoinEng& eng = Eng();
	return eng.AllowFreeTxes && (priority > eng.ChainParams.CoinValue * 144 / 250);
}

bool Tx::TryFromDb(const HashValue& hash, Tx* ptx) {
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

		EXT_LOCKED(eng.Caches.Mtx, eng.Caches.HashToTxCache.insert(make_pair(hash, *ptx)));
	}
	return true;
}

Tx Tx::FromDb(const HashValue& hash) {
	Coin::Tx r;
	if (TryFromDb(hash, &r))
		return r;
	TRC(2, "NotFound Tx: " << hash);
	throw TxNotFoundException(hash);
}

void Tx::EnsureCreate(CoinEng& eng) {
	if (!m_pimpl)
		m_pimpl = eng.CreateTxObj();
}

void Tx::Write(ProtocolWriter& wr) const {
	m_pimpl->Write(wr);
}

void Tx::Read(const ProtocolReader& rd) {
	EnsureCreate(Eng());
	m_pimpl->Read(rd);
}

bool Tx::IsNewerThan(const Tx& txOld) const {
	if (TxIns().size() != txOld.TxIns().size())
		return false;
	bool r = false;
	uint32_t lowest = UINT_MAX;
	for (int i = 0; i < TxIns().size(); ++i) {
		const TxIn &txIn = TxIns()[i], &oldIn = txOld.TxIns()[i];
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

PubKeyHash160 DbPubKeyToHashValue160(RCSpan mb) {
	return mb.size() == 20 ? PubKeyHash160(nullptr, HashValue160(mb)) : PubKeyHash160(CanonicalPubKey::FromCompressed(mb));
}

HashValue160 CoinEng::GetHash160ById(int64_t id) {
	EXT_LOCK(Caches.Mtx) {
		ChainCaches::CCachePkIdToPubKey::iterator it = Caches.m_cachePkIdToPubKey.find(id);
		if (it != Caches.m_cachePkIdToPubKey.end())
			return it->second.first.Hash160;
	}
	Blob pk = Db->FindPubkey(id);
	if (!!pk) {
		PubKeyHash160 pkh = DbPubKeyToHashValue160(pk);
#ifdef X_DEBUG //!!!D
		CIdPk idpk(pkh.Hash160);
		ASSERT(idpk == id);
#endif
		EXT_LOCK(Caches.Mtx) {
			if (Caches.PubkeyCacheEnabled)
				Caches.m_cachePkIdToPubKey.insert(make_pair(id, pkh));
		}
		return pkh.Hash160;
	} else
		Throw(ExtErr::DB_NoRecord);
}


int64_t TxOut::DustThreshold(int64_t feeRate) const
{
    Span s = ScriptPubKey;
    if (!Script::IsSpendable(s))
        return 0;
	return Tx::CalcMinFee(EXT_BIN(_self).size() + 32 + 4 + 1 + 4 + 107 / (Script::WitnessProgram(s).first.data() ? WITNESS_SCALE_FACTOR : 1), feeRate);
}

void TxOut::CheckForDust() const {
    if (IsDust(Eng().ChainParams.DustRelayFee))
		Throw(CoinErr::TxAmountTooSmall);
}

Span FindStandardHashAllSigPubKey(RCSpan cbuf) {
	Span r(0, (size_t)0);
	if (cbuf.size() > 64) {
		int len = cbuf[0];
		if (len > 64 && len < 64 + 32 && cbuf[1] == 0x30 && cbuf[2] == len - 3 && cbuf[3] == 2) {
			switch (int len1 = cbuf[4]) {
			case 0x1F:
			case 0x20:
			case 0x21:
				if (cbuf[5 + len1] == 2 && cbuf[5 + len1 + 1] + len1 + 4 == len - 3 && cbuf[len] == 1) {
					if (cbuf.size() == len + 1)
						r = Span(cbuf.data() + len + 1, (size_t)0);
					else if (cbuf.size() > len + 2) {
						int len2 = cbuf[1 + len];
						if ((len2 == 33 || len2 == 65) && cbuf.size() == len + len2 + 2)
							r = Span(cbuf.data() + len + 1, len2 + 1);
					}
				}
			}
		}
	}
	return r;
}


void Tx::Check() const {
	CoinEng& eng = Eng();
	auto& txIns = TxIns();

	if (txIns.empty())
		Throw(CoinErr::BadTxnsVinEmpty);
	if (TxOuts().empty())
		Throw(CoinErr::BadTxnsVoutEmpty);

	bool bIsCoinBase = m_pimpl->IsCoinBase();
	int64_t nOut = 0;
	EXT_FOR(const TxOut& txOut, TxOuts()) {
		if (!txOut.IsEmpty()) {
			if (txOut.Value < 0)
				Throw(CoinErr::BadTxnsVoutNegative);
			if (!bIsCoinBase && txOut.Value < eng.ChainParams.MinTxOutAmount)
				Throw(CoinErr::TxOutBelowMinimum);
		}
		eng.CheckMoneyRange(nOut += eng.CheckMoneyRange(txOut.Value));
	}
	unordered_set<OutPoint> outPoints;
	EXT_FOR(const TxIn& txIn, txIns) { // Check for duplicate inputs
		if (!outPoints.insert(txIn.PrevOutPoint).second)
			Throw(CoinErr::DupTxInputs);
		if (!bIsCoinBase && txIn.PrevOutPoint.IsNull())
			Throw(CoinErr::BadTxnsPrevoutNull);
	}
	if (bIsCoinBase && !between(int(txIns[0].Script().size()), 2, 100))
		Throw(CoinErr::BadCbLength);

	eng.OnCheck(_self);
}

void CoinEng::UpdateMinFeeForTxOuts(int64_t& minFee, const int64_t& baseFee, const Tx& tx) {
	if (minFee < baseFee) { // To limit dust spam, require MIN_TX_FEE/MIN_RELAY_TX_FEE if any output is less than 0.001		//!!! Bitcoin requires min 0.01
		const int64_t cent = ChainParams.CoinValue / 1000;
		EXT_FOR(const TxOut txOut, tx.TxOuts()) {
			if (txOut.Value < cent) {
				minFee = baseFee;
				break;
			}
		}
	}
}

int64_t Tx::CalcMinFee(uint32_t size, int64_t feeRate) {
    auto r = size * feeRate / 1000;
    return !feeRate || r || !size ? r
        : feeRate > 0 ? 1 : -1;    //!!!?
}

int64_t Tx::GetMinFee(uint32_t blockSize, bool bAllowFree, MinFeeMode mode, uint32_t nBytes) const {
	CoinEng& eng = Eng();
	int64_t nBaseFee = mode == MinFeeMode::Relay ? g_conf.MinRelayTxFee : eng.ChainParams.MinTxFee;

	if (uint32_t(-1) == nBytes)
		nBytes = GetSerializeSize();
	uint32_t nNewBlockSize = blockSize + nBytes;
	int64_t nMinFee = (1 + (int64_t)nBytes / 1000) * nBaseFee;

	if (bAllowFree) {
		if (blockSize == 1) {
			if (nBytes < MAX_FREE_TRANSACTION_CREATE_SIZE)
				nMinFee = 0;			  // (about 4500bc if made of 50bc inputs)
		} else if (nNewBlockSize < 27000) // Free transaction area
			nMinFee = 0;
	}

	eng.UpdateMinFeeForTxOuts(nMinFee, nBaseFee, _self);

	if (blockSize != 1 && nNewBlockSize >= MAX_BLOCK_SIZE_GEN / 2) { // Raise the price as the block approaches full
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

uint32_t Tx::GetSerializeSize(bool witnessAware) const {
    MemoryStream stm;
    ProtocolWriter wr(stm);
    wr.WitnessAware = witnessAware;
    Write(wr);
    return (uint32_t)stm.Position;
}

uint32_t Tx::get_Weight() const {
    return GetSerializeSize(false) * (WITNESS_SCALE_FACTOR - 1) + GetSerializeSize(true);
}

Tx::StandardStatus Tx::CheckStandard() const {
    int32_t ver = (int32_t)m_pimpl->Ver;    //!!!
    if (ver < 1 || ver > MAX_STANDARD_VERSION)
        return StandardStatus::Version;
    if (Weight > MAX_STANDARD_TX_WEIGHT)
        return StandardStatus::TxSize;
    for (auto& txIn : TxIns()) {
        Span script = txIn.Script();
        if (script.size() > MAX_STANDARD_SIGSCRIPT_SIZE)
            return StandardStatus::ScriptSigSize;
        if (!Script::IsPushOnly(script))
            return StandardStatus::ScriptSigNotPushOnly;
    }

    int nDataOut = 0;
    auto dustFee = Eng().ChainParams.DustRelayFee;
    for (auto& txOut : TxOuts()) {
        auto ps = TxOut::CheckStandardType(txOut.ScriptPubKey);
		switch (ps.Type) {
		case AddressType::NonStandard:
			Throw(CoinErr::NonStandardTx);
		case AddressType::NullData:
			++nDataOut;
			break;
		default:
			if (txOut.IsDust(dustFee))
				return StandardStatus::Dust;
		}
    }
    return nDataOut <= 1 ? StandardStatus::Standard : StandardStatus::MultiOpReturn;
}

int64_t Tx::get_ValueOut() const {
	CoinEng& eng = Eng();

	int64_t r = 0;
	EXT_FOR(const TxOut& txOut, TxOuts()) {
		eng.CheckMoneyRange(r += eng.CheckMoneyRange(txOut.Value));
	}
	return r;
}

int64_t Tx::get_Fee() const {
	if (m_pimpl->IsCoinBase())
		return 0;
	int64_t sum = 0;
	EXT_FOR(const TxIn& txIn, TxIns()) {
		sum += Tx::FromDb(txIn.PrevOutPoint.TxHash).TxOuts().at(txIn.PrevOutPoint.Index).Value;
	}
	return sum - ValueOut;
}

int Tx::get_DepthInMainChain() const {
	CoinEng& eng = Eng();
	BlockHeader bestBlock = eng.BestBlock();
	return Height >= 0 && bestBlock ? bestBlock.Height - Height + 1 : 0;
}

unsigned Tx::GetP2SHSigOpCount(const ITxoMap& txoMap) const {
	if (m_pimpl->IsCoinBase())
		return 0;
	int r = 0;
	EXT_FOR(const TxIn& txIn, TxIns()) {
		const Txo txo = txoMap.Get(txIn.PrevOutPoint);
		Span scriptPubKey = txo.get_ScriptPubKey();
		if (Script::IsPayToScriptHash(scriptPubKey))
			r += CalcSigOpCount(scriptPubKey, txIn.Script());
	}
	return r;
}

pair<int, DateTime> Tx::CalculateSequenceLocks(vector<int>& inHeights, const HashValue& hashBlock) const {
	auto r = make_pair(0, DateTime(0));
	if (m_pimpl->Ver >= 2 && t_features.CheckSequenceVerify) {
		CoinEng& eng = Eng();
		int i = 0;
		EXT_FOR(const TxIn& txIn, TxIns()) {
			if (!(txIn.Sequence & TxIn::SEQUENCE_LOCKTIME_DISABLE_FLAG)) {
				auto coinHeight = inHeights[i];
				int val = txIn.Sequence & TxIn::SEQUENCE_LOCKTIME_MASK;
				if (txIn.Sequence & TxIn::SEQUENCE_LOCKTIME_TYPE_FLAG)
					r.second = (max)(r.second, eng.Tree.GetAncestor(hashBlock, (max)(0, coinHeight - 1)).GetMedianTimePast() + TimeSpan::FromSeconds(val << TxIn::SEQUENCE_LOCKTIME_GRANULARITY));
				else
					r.first = (max)(r.first, coinHeight + val);
			}
			++i;
		}
	}
	return r;
}

void Tx::CheckInOutValue(int64_t nValueIn, int64_t& nFees, int64_t minFee, const Target& target) const {
	int64_t valOut = ValueOut;
	if (m_pimpl->IsCoinStake())
		m_pimpl->CheckCoinStakeReward(valOut - nValueIn, target);
	else {
		if (nValueIn < valOut)
			Throw(CoinErr::ValueInLessThanValueOut);
		int64_t nTxFee = nValueIn - valOut;
		if (nTxFee < minFee)
			Throw(CoinErr::TxFeeIsLow);
		nFees = Eng().CheckMoneyRange(nFees + nTxFee);
	}
}

void Tx::ConnectInputs(CoinsView& view, int32_t height, int& nBlockSigOps, int64_t& nFees, bool bBlock, bool bMiner, int64_t minFee, const Target& target) const {
	CoinEng& eng = Eng();

	HashValue hashTx = Hash(_self);
	int nSigOp = nBlockSigOps + SigOpCost(view);
    if (nSigOp > MAX_BLOCK_SIGOPS_COST)
        Throw(CoinErr::TxTooManySigOps);

	if (!m_pimpl->IsCoinBase()) {
		vector<Txo> vTxo;
		int64_t nValueIn = 0;
		if (eng.Mode != EngMode::Lite) {
			//			TRC(3, TxIns.size() << " TxIns");
			bool bVerifySignature = !bBlock || eng.BestBlockHeight() > eng.ChainParams.LastCheckpointHeight - INITIAL_BLOCK_THRESHOLD;
			SignatureHasher sigHasher(*m_pimpl);
			if (bVerifySignature && m_pimpl->HasWitness())
				sigHasher.CalcWitnessCache();
			eng.PatchSigHasher(sigHasher);

			auto& txIns = TxIns();
			for (uint32_t i = 0; i < txIns.size(); ++i) {
				const TxIn& txIn = txIns[i];
				const OutPoint& op = txIn.PrevOutPoint;

				Tx txPrev;
				if (bBlock || bMiner) {
					txPrev = view.TxMap.Find(op.TxHash);
					if (!txPrev)
						txPrev = FromDb(op.TxHash);
				} else {
					EXT_LOCK(eng.TxPool.Mtx) {
						TxPool::CHashToTxInfo::iterator it = eng.TxPool.m_hashToTxInfo.find(op.TxHash);
						if (it != eng.TxPool.m_hashToTxInfo.end())
							txPrev = it->second.Tx;
					}
					if (!txPrev)
						txPrev = FromDb(op.TxHash);
				}
				const TxOut& txOut = txPrev.TxOuts().at(op.Index);

				if (txPrev->IsCoinBase())
					eng.CheckCoinbasedTxPrev(height, txPrev.Height);

				if (!view.HasInput(op))
					Throw(CoinErr::InputsAlreadySpent);
				view.SpendInput(op);

				if (bVerifySignature) {
					sigHasher.m_bWitness = false;
					sigHasher.NIn = i;
					sigHasher.m_amount = txOut.Value;
					sigHasher.HashType = SigHashType::ZERO;
					sigHasher.VerifySignature(txOut.ScriptPubKey);
				}

				eng.CheckMoneyRange(nValueIn += txOut.Value);
				vTxo.push_back(txOut);
			}
		}

		if (bBlock)
			eng.OnConnectInputs(_self, vTxo, bBlock, bMiner);

		if (eng.Mode != EngMode::Lite)
			CheckInOutValue(nValueIn, nFees, minFee, target);
	}
	nBlockSigOps = nSigOp;
	view.TxMap.Add(_self);
}

unsigned Tx::SigOpCost(const ITxoMap& txoMap) const {
    unsigned r = 0;
	auto& txIns = TxIns();
	EXT_FOR(const TxIn& txIn, txIns) {
		if (txIn.PrevOutPoint.Index >= 0)
			r += CalcSigOpCount1(txIn.Script());
	}
	{
		DBG_LOCAL_IGNORE_CONDITION(ExtErr::EndOfStream);

		EXT_FOR(const TxOut & txOut, TxOuts()) {
			try { //!!! should be more careful checking
				r += CalcSigOpCount1(txOut.get_ScriptPubKey());
			} catch (RCExc) {
			}
		}
	}
    r *= WITNESS_SCALE_FACTOR;
    if (!m_pimpl->IsCoinBase()) {
        r += GetP2SHSigOpCount(txoMap) * WITNESS_SCALE_FACTOR;

        for (auto& txIn : txIns)
            r += txIn.CountWitnessSigOps();
    }
	return r;
}

DbWriter& operator<<(DbWriter& wr, const Tx& tx) {
	CoinEng& eng = Eng();

	if (!wr.BlockchainDb) {
		wr.Write7BitEncoded(tx->Ver);
		tx->WritePrefix(wr);
		auto hasWitness = tx->HasWitness();
		if (hasWitness)
			(ProtocolWriter&)wr << uint8_t(0) << uint8_t(1);

		const vector<TxIn>& txIns = tx.TxIns();
		size_t nIns = txIns.size();
		wr.WriteSize(nIns);
		for (auto& txIn : txIns)
			txIn.Write(wr);

		Write(wr, tx.TxOuts());
		if (hasWitness)
			for (auto& txIn : txIns) {
				CoinSerialized::WriteCompactSize(wr, txIn.Witness.size());
				for (auto& chunk : txIn.Witness)
					CoinSerialized::WriteSpan(wr, chunk);
			}

		wr.Write7BitEncoded(tx.LockBlock);
		tx->WriteSuffix(wr);
	} else {
#if UCFG_COIN_USE_NORMAL_MODE
		uint64_t v = uint64_t(tx->Ver) << 2;
		if (tx.IsCoinBase())
			v |= 1;
		if (tx.LockBlock)
			v |= 2;
		wr.Write7BitEncoded(v);
		tx->WritePrefix(wr);
		if (tx.LockBlock)
			wr.Write7BitEncoded(tx.LockBlock);
		tx->WriteSuffix(wr);

		auto& txOuts = tx.TxOuts();
		for (int i = 0; i < txOuts.size(); ++i) {
			const TxOut& txOut = txOuts[i];
#if UCFG_COIN_PUBKEYID_36BITS
			uint64_t valtyp = uint64_t(txOut.Value) << 6;
#else
			uint64_t valtyp = uint64_t(txOut.Value) << 5;
#endif
			Span span160;
			Span pk;
			CIdPk idPk;
			switch (uint8_t typ = TryParseDestination(txOut.ScriptPubKey, span160, pk)) {
			case 20:
			case 33:
			case 65:
				if (eng.Mode == EngMode::Lite) {
					wr.Write7BitEncoded(valtyp | 3);
					break;
				} else {
					HashValue160 hash160(span160);
					CConnectJob::CMap::iterator it = wr.ConnectJob->Map.find(hash160);
					if (it != wr.ConnectJob->Map.end() && !!it->second.PubKey) {
						idPk = CIdPk(hash160);
#if UCFG_COIN_PUBKEYID_36BITS
						wr.Write7BitEncoded(valtyp | (20 == typ ? 1 : 2) | ((int64_t(idPk) & 0xF00000000) >> 30));
#else
						wr.Write7BitEncoded(valtyp | (20 == typ ? 1 : 2) | ((int64_t(idPk) & 0x700000000) >> 30));
#endif
						wr << uint32_t(int64_t(txOut.m_idPk = idPk));
						break;
					}
				}
			default:
				wr.Write7BitEncoded(valtyp); // SCRIPT_PKSCRIPT_GEN
				CoinSerialized::WriteSpan(wr, txOut.get_ScriptPubKey());
			}
		}
#else
		Throw(E_FAIL);
#endif // UCFG_COIN_USE_NORMAL_MODE
	}
	return wr;
}

const DbReader& operator>>(const DbReader& rd, Tx& tx) {
	CoinEng& eng = Eng();

	uint64_t v;
	//!!!?	if (tx.Ver != 1)
	//		Throw(ExtErr::New_Protocol_Version);

	tx.EnsureCreate(eng);

	if (!rd.BlockchainDb) {
		v = rd.Read7BitEncoded();
		tx->Ver = (uint32_t)v;
		tx->ReadPrefix(rd);
		Read(rd, tx->m_txIns);
		if (!tx->m_txIns.empty())
			Read(rd, tx->TxOuts);
		else {
			uint8_t flag = rd.ReadByte();
			if (!flag)
				Throw(CoinErr::InconsistentDatabase);
			Read(rd, tx->m_txIns);
			Read(rd, tx->TxOuts);
			if (flag & 1)
				for (auto& txIn : tx->m_txIns) {
					txIn.Witness.resize(CoinSerialized::ReadCompactSize(rd));
					for (auto& chunk : txIn.Witness)
						chunk = CoinSerialized::ReadBlob(rd);
				}
		}
		tx->m_bLoadedIns = true;
		tx->LockBlock = (uint32_t)rd.Read7BitEncoded();
		tx->ReadSuffix(rd);
	} else {
#if UCFG_COIN_USE_NORMAL_MODE
		v = rd.Read7BitEncoded();
		tx->Ver = (uint32_t)(v >> 2);
		tx->ReadPrefix(rd);
		tx->m_bIsCoinBase = v & 1;
		if (v & 2)
			tx.LockBlock = (uint32_t)rd.Read7BitEncoded();
		tx->ReadSuffix(rd);

#if UCFG_COIN_TXES_IN_BLOCKTABLE
		for (int i = 0; i < rd.NOut; ++i) {
#else
		while (!rd.BaseStream.Eof()) {
#endif
			uint64_t valtyp = rd.Read7BitEncoded();
#if UCFG_COIN_PUBKEYID_36BITS
			uint64_t value = valtyp >> 6;
#else
			uint64_t value = valtyp >> 5;
#endif
			tx.TxOuts().push_back(TxOut(value, Blob(nullptr)));
			TxOut& txOut = tx.TxOuts().back();
			static const uint8_t s_types[] = { 0, 20, 33, 0x7F };
			switch (uint8_t typ = s_types[valtyp & 3]) {
			case 0: txOut.m_scriptPubKey = CoinSerialized::ReadBlob(rd); break;
			case 20:
			case 33:
			case 65:
				txOut.m_typ = typ;
				txOut.m_scriptPubKey = nullptr;
#if UCFG_COIN_PUBKEYID_36BITS
				txOut.m_idPk = CIdPk(int64_t(rd.ReadUInt32()) | ((valtyp & 0x3C) << 30));
#else
				txOut.m_idPk = CIdPk(int64_t(rd.ReadUInt32()) | ((valtyp & 0x1C) << 30));
#endif
				break;
			default: Throw(E_FAIL);
			}
		}
#else
		Throw(E_FAIL);
#endif // UCFG_COIN_USE_NORMAL_MODE
	}
	if (Tx::LocktimeTypeOf(tx.LockBlock) == Tx::LocktimeType::Timestamp)
		tx->LockTimestamp = DateTime::from_time_t(tx.LockBlock);
	return rd;
}


} // namespace Coin
