/*######   Copyright (c) 2013-2019 Ufasoft  http://ufasoft.com  mailto:support@ufasoft.com,  Sergey Pavlov  mailto:dev@ufasoft.com ####
#                                                                                                                                     #
# 		See LICENSE for licensing information                                                                                         #
#####################################################################################################################################*/

#include <el/ext.h>

#include "eng.h"
#include "coin-protocol.h"

namespace Coin {

TxPool::TxPool(CoinEng& eng)
	: Eng(eng)
{}

TxPool::TxInfo::TxInfo(const class Tx& tx)
	: Tx(tx)
	, FeeRate(tx.Fee * 1000 / tx.GetSerializeSize())
{
}

void TxPool::Add(const Tx& tx) {
	EXT_LOCK (Mtx) {
		m_hashToTxInfo[Hash(tx)] = TxInfo(tx);
		EXT_FOR (const TxIn& txIn, tx.TxIns()) {
			m_outPointToNextTx.insert(make_pair(txIn.PrevOutPoint, tx));
		}
	}
}

void TxPool::Remove(const Tx& tx) {
	EXT_LOCK (Mtx) {
		EXT_FOR (const TxIn& txIn, tx.TxIns()) {
			m_outPointToNextTx.erase(txIn.PrevOutPoint);
		}
		m_hashToTxInfo.erase(Hash(tx));
	}
}

void TxPool::EraseOrphanTx(const HashValue& hash) {
	CHashToOrphan::iterator it = m_hashToOrphan.find(hash);
	if (it != m_hashToOrphan.end()) {
		EXT_FOR(const TxIn& txIn, it->second.TxIns()) {
			pair<CHashToHash::iterator, CHashToHash::iterator> range = m_prevHashToOrphanHash.equal_range(txIn.PrevOutPoint.TxHash);
			for (CHashToHash::iterator j = range.first; j != range.second;)
				if (j->second == hash)
					m_prevHashToOrphanHash.erase(j++);
				else
					++j;
		}
		m_hashToOrphan.erase(it);
	}
}

void TxPool::AddOrphan(const Tx& tx) {
	HashValue hash = Hash(tx);
	if (m_hashToOrphan.insert(make_pair(hash, tx)).second) {
		EXT_FOR(const TxIn& txIn, tx.TxIns()) {
			m_prevHashToOrphanHash.insert(make_pair(txIn.PrevOutPoint.TxHash, hash));
		}

		if (m_hashToOrphan.size() > MAX_ORPHAN_TRANSACTIONS)
			EraseOrphanTx(m_hashToOrphan.begin()->first);		//!!!? is it random enough?
	}
}

// returns pair(added, missingInputs)
bool TxPool::AddToPool(const Tx& tx, vector<HashValue>& vQueue) {
	tx.Check();
	if (tx.IsCoinBase())
		Throw(CoinErr::Misbehaving);

	if (!g_conf.AcceptNonStdTxn)
		tx.CheckStandard();

	uint32_t serSize = tx.GetSerializeSize();
/*!!!?
	int sigOpCount = tx.SigOpCount;
	if (sigOpCount > serSize/34 || serSize < 100)
		Throw(CoinErr::Misbehaving);		*/
	HashValue hash = Hash(tx);

	if (Eng.HaveTxInDb(hash))
		return false;

	if (!tx.IsFinal(Eng.BestBlockHeight() + 1))
		Throw(CoinErr::ContainsNonFinalTx);

	Tx ptxOld(nullptr);
	EXT_LOCK (Mtx) {
		if (m_hashToTxInfo.count(hash))
			return false;

		for (int i = 0; i < tx.TxIns().size(); ++i) {
			const TxIn& txIn = tx.TxIns()[i];
			COutPointToNextTx::iterator it = m_outPointToNextTx.find(txIn.PrevOutPoint);
			if (it != m_outPointToNextTx.end()) {
				ptxOld = it->second;
				if (i != 0 || tx.IsNewerThan(ptxOld))
					return false;
				for (int j = 0; j < tx.TxIns().size(); ++j) {
					auto o = Lookup(m_outPointToNextTx, tx.TxIns()[j].PrevOutPoint);
					if (!o || o.value() != ptxOld)
						return false;
				}
				break;
			}
		}
	}
	CoinsView view(Eng);
	int nBlockSigOps = 0;
	int64_t nFees = 0;
	//!!!TODO:  accepts only transaction with inputs already in the DB. view should check inputs in the mem pool too
	try {
		DBG_LOCAL_IGNORE_CONDITION(CoinErr::SCRIPT_ERR_WITNESS_PROGRAM_MISMATCH);
		DBG_LOCAL_IGNORE_CONDITION(CoinErr::SCRIPT_ERR_CLEANSTACK);
		
		tx.ConnectInputs(view, Eng.BestBlockHeight(), nBlockSigOps, nFees, false, false, tx.GetMinFee(1, Eng.AllowFreeTxes, MinFeeMode::Relay), Eng.ChainParams.MaxTarget);	//!!!? MaxTarget
	} catch (WitnessProgramException&) {
		if (!tx->HasWitness())
			return false;
		throw;
	}

	if (nFees < tx.GetMinFee(1000, Eng.AllowFreeTxes, MinFeeMode::Relay))
		Throw(CoinErr::TxFeeIsLow);
	if (nFees < Eng.GetMinRelayTxFee()) {
		DateTime now = Clock::now();
		Eng.m_freeCount = pow(1.0 - 1.0 / 600, (int)duration_cast<seconds>(now - exchange(Eng.m_dtLastFreeTx, now)).count());
		if (Eng.m_freeCount > 15 * 10000 && !Eng.IsFromMe(tx))
			Throw(CoinErr::TxRejectedByRateLimiter);
		Eng.m_freeCount += serSize;
	}

	if (ptxOld)
		Remove(ptxOld);
	Add(tx);

	if (ptxOld)
		Eng.Events.OnEraseTx(Hash(ptxOld));
	Eng.Events.OnProcessTx(tx);
	Eng.Relay(tx);

	vQueue.push_back(hash);
	return true;
}

void TxMessage::Process(Link& link) {
	CoinEng& eng = static_cast<CoinEng&>(*link.Net);
	CCoinEngThreadKeeper keeper(&eng, &ScriptPolicy(true));

	HashValue hash = Hash(Tx);

	if (eng.Mode == EngMode::Lite) {
		auto it = find(link.m_curMatchedHashes.begin(), link.m_curMatchedHashes.end(), hash);
		if (it != link.m_curMatchedHashes.end()) {
			link.m_curMerkleBlock.m_pimpl->m_txes.push_back(Tx);
			link.m_curMatchedHashes.erase(it);
			if (link.m_curMatchedHashes.empty()) {
				Block block = exchange(link.m_curMerkleBlock, Block(nullptr));
				block.Process(&link);
			}
			return;
		}
	}

	vector<HashValue> vQueue;

	EXT_LOCK (eng.TxPool.Mtx) {
		try {
			DBG_LOCAL_IGNORE_CONDITION(CoinErr::TxNotFound);
			DBG_LOCAL_IGNORE_CONDITION(CoinErr::Misbehaving);
			DBG_LOCAL_IGNORE_CONDITION(CoinErr::TxFeeIsLow);				//!!!T
			DBG_LOCAL_IGNORE_CONDITION(CoinErr::InputsAlreadySpent);

			if (!eng.TxPool.AddToPool(Tx, vQueue))
				return;
		} catch (const TxNotFoundException&) {
			eng.TxPool.AddOrphan(Tx);
			return;
		}
		DBG_LOCAL_IGNORE_CONDITION(CoinErr::TxNotFound);	//!!!T
		for (int i = 0; i < vQueue.size(); ++i) {
			HashValue hash = vQueue[i];
			pair<TxPool::CHashToHash::iterator, TxPool::CHashToHash::iterator> range = eng.TxPool.m_prevHashToOrphanHash.equal_range(hash);
			for (TxPool::CHashToHash::iterator j = range.first; j != range.second; ++j)
				eng.TxPool.AddToPool(eng.TxPool.m_hashToOrphan[j->second], vQueue);
			eng.TxPool.EraseOrphanTx(hash);
		}
	}
}

void MemPoolMessage::Process(Link& link) {
	CoinEng& eng = static_cast<CoinEng&>(*link.Net);
	ptr<InvMessage> m(new InvMessage);
	EXT_LOCK (eng.TxPool.Mtx) {
		for (TxPool::CHashToTxInfo::iterator it = eng.TxPool.m_hashToTxInfo.begin(), e = eng.TxPool.m_hashToTxInfo.end(); it != e && m->Invs.size() < ProtocolParam::MAX_INV_SZ; ++it)
			m->Invs.push_back(Inventory(InventoryType::MSG_TX, it->first));
	}
	if (!m->Invs.empty())
		link.Send(m);
}


} // Coin::

