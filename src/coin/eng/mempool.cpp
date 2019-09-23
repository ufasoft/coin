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

TxInfo::TxInfo(const class Tx& tx, uint32_t serializationSize)
	: Tx(tx)
	, FeeRatePerKB(serializationSize ? tx.Fee * 1000 / serializationSize : INT_MAX)
{
}

void TxPool::Add(const TxInfo& txInfo) {
	EXT_LOCK (Mtx) {
		m_hashToTxInfo[Hash(txInfo.Tx)] = txInfo;
		EXT_FOR (const TxIn& txIn, txInfo.Tx.TxIns()) {
			m_outPointToNextTx.insert(make_pair(txIn.PrevOutPoint, txInfo.Tx));
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
	if (tx->IsCoinBase())
		Throw(CoinErr::Misbehaving);

	if (!g_conf.AcceptNonStdTxn)
		tx.CheckStandard();

	DateTime now = Clock::now();

	if (!tx->IsFinal(Eng.BestBlockHeight() + 1, now))
		Throw(CoinErr::ContainsNonFinalTx);


/*!!!?
	int sigOpCount = tx.SigOpCount;
	if (sigOpCount > serSize/34 || serSize < 100)
		Throw(CoinErr::Misbehaving);		*/
	HashValue hash = Hash(tx);

	if (Eng.HaveTxInDb(hash))
		return false;

//!!!?	if (!tx.IsFinal(Eng.BestBlockHeight() + 1))
//			Throw(CoinErr::ContainsNonFinalTx);

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
				for (auto& txIn : tx.TxIns()) {
					auto o = Lookup(m_outPointToNextTx, txIn.PrevOutPoint);
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
		DBG_LOCAL_IGNORE_CONDITION(CoinErr::SCRIPT_ERR_WITNESS_PROGRAM_WITNESS_EMPTY);
		DBG_LOCAL_IGNORE_CONDITION(CoinErr::SCRIPT_ERR_CLEANSTACK);
		
		tx.ConnectInputs(view, Eng.BestBlockHeight(), nBlockSigOps, nFees, false, false, tx.GetMinFee(1, Eng.AllowFreeTxes, MinFeeMode::Relay), Eng.ChainParams.MaxTarget);	//!!!? MaxTarget
	} catch (WitnessProgramException&) {
		if (!tx->HasWitness())
			return false;
		throw;
	}	 

	uint32_t serializationSize = tx.GetSerializeSize();
	int64_t feeLimit = serializationSize * g_conf.MinRelayTxFee / 1000;
	if (nFees < feeLimit) {
		Throw(CoinErr::TxFeeIsLow);

		/*!!! Disable free txes temporarily
		DateTime now = Clock::now();
		Eng.m_freeCount = pow(1.0 - 1.0 / 600, (int)duration_cast<seconds>(now - exchange(Eng.m_dtLastFreeTx, now)).count());
		if (Eng.m_freeCount > 15 * 10000 && !Eng.IsFromMe(tx))
			Throw(CoinErr::TxRejectedByRateLimiter);
		Eng.m_freeCount += serSize;
		*/
	}

	if (ptxOld)
		Remove(ptxOld);
	TxInfo txInfo(tx, serializationSize);
	Add(txInfo);

	if (ptxOld)
		Eng.Events.OnEraseTx(Hash(ptxOld));
	Eng.Events.OnProcessTx(tx);
	Eng.Relay(txInfo);

	vQueue.push_back(hash);
	return true;
}

void TxPool::OnTxMessage(const Tx& tx) {
	vector<HashValue> vQueue;
	EXT_LOCK(Mtx) {
		try {
			DBG_LOCAL_IGNORE_CONDITION(CoinErr::TxNotFound);
			DBG_LOCAL_IGNORE_CONDITION(CoinErr::Misbehaving);
			DBG_LOCAL_IGNORE_CONDITION(CoinErr::TxFeeIsLow);				//!!!T
			DBG_LOCAL_IGNORE_CONDITION(CoinErr::InputsAlreadySpent);

			if (!AddToPool(tx, vQueue))
				return;
		} catch (const TxNotFoundException&) {
			AddOrphan(tx);
			return;
		}
		DBG_LOCAL_IGNORE_CONDITION(CoinErr::TxNotFound);	//!!!T
		for (int i = 0; i < vQueue.size(); ++i) {
			HashValue hash = vQueue[i];
			pair<TxPool::CHashToHash::iterator, TxPool::CHashToHash::iterator> range = m_prevHashToOrphanHash.equal_range(hash);
			for (TxPool::CHashToHash::iterator j = range.first; j != range.second; ++j)
				AddToPool(m_hashToOrphan[j->second], vQueue);
			EraseOrphanTx(hash);
		}
	}
}

void TxMessage::Trace(Link& link, bool bSend) const {
	if (CTrace::s_nLevel & (1 << TRC_LEVEL_TX_MESSAGE))
		base::Trace(link, bSend);
}

void TxMessage::Process(Link& link) {
	CoinEng& eng = static_cast<CoinEng&>(*link.Net);
	CCoinEngThreadKeeper keeper(&eng, &ScriptPolicy(true), true);

	HashValue hash = Hash(Tx);

	if (eng.Mode == EngMode::Lite) {
		auto it = find(link.m_curMatchedHashes.begin(), link.m_curMatchedHashes.end(), hash);
		if (it != link.m_curMatchedHashes.end()) {
			link.m_curMerkleBlock->m_txes.push_back(Tx);
			link.m_curMatchedHashes.erase(it);
			if (link.m_curMatchedHashes.empty()) {
				Block block = exchange(link.m_curMerkleBlock, Block(nullptr));
				block.Process(&link);
			}
			return;
		}
	}

	eng.TxPool.OnTxMessage(Tx);
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

