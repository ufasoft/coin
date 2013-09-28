/*######     Copyright (c) 1997-2013 Ufasoft  http://ufasoft.com  mailto:support@ufasoft.com,  Sergey Pavlov  mailto:dev@ufasoft.com #######################################
#                                                                                                                                                                          #
# This program is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation;  #
# either version 3, or (at your option) any later version. This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the      #
# implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details. You should have received a copy of the GNU #
# General Public License along with this program; If not, see <http://www.gnu.org/licenses/>                                                                               #
##########################################################################################################################################################################*/

#include <el/ext.h>

#include "eng.h"
#include "coin-protocol.h"

namespace Coin {

void CoinEng::AddToMemory(const Tx& tx) {
	EXT_LOCK (TxPool.Mtx) {
		TxPool.m_hashToTx[Hash(tx)] = tx;
		EXT_FOR (const TxIn& txIn, tx.TxIns()) {
			TxPool.m_outPointToNextTx.insert(make_pair(txIn.PrevOutPoint, tx));
		}
	}
}

void CoinEng::RemoveFromMemory(const Tx& tx) {
	EXT_LOCK (TxPool.Mtx) {
		EXT_FOR (const TxIn& txIn, tx.TxIns()) {
			TxPool.m_outPointToNextTx.erase(txIn.PrevOutPoint);
		}
		TxPool.m_hashToTx.erase(Hash(tx));
	}
}

// returns pair(added, missingInputs)
bool CoinEng::AddToPool(const Tx& tx, vector<HashValue>& vQueue) {
	tx.Check();
	if (tx.IsCoinBase())
		Throw(E_COIN_Misbehaving);
	UInt32 serSize = tx.GetSerializeSize();
	int sigOpCount = tx.SigOpCount;
	if (sigOpCount > serSize/34 || serSize < 100)
		Throw(E_COIN_Misbehaving);
	HashValue hash = Hash(tx);

	if (HaveTxInDb(hash))
		return false;
	Tx ptxOld(nullptr);
	EXT_LOCK (TxPool.Mtx) {
		if (TxPool.m_hashToTx.count(hash))
			return false;
	
		for (int i=0; i<tx.TxIns().size(); ++i) {
			const TxIn& txIn = tx.TxIns()[i];
			CTxPool::COutPointToNextTx::iterator it = TxPool.m_outPointToNextTx.find(txIn.PrevOutPoint);
			if (it != TxPool.m_outPointToNextTx.end()) {
				ptxOld = it->second;
				if (i!=0 || tx.IsNewerThan(ptxOld))
					return false;
				for (int j=0; j<tx.TxIns().size(); ++j) {
					Tx pTx;
					if (!Lookup(TxPool.m_outPointToNextTx, tx.TxIns()[j].PrevOutPoint, pTx) || pTx != ptxOld)
						return false;
				}
				break;
			}
		}
	}
	CTxMap txMap;
	int nBlockSigOps = 0;
	Int64 nFees = 0;
	tx.ConnectInputs(txMap, BestBlockHeight(), nBlockSigOps, nFees, false, false, tx.GetMinFee(1, AllowFreeTxes, MinFeeMode::Relay), ChainParams.MaxTarget);	//!!!? MaxTarget
	if (nFees < tx.GetMinFee(1000, AllowFreeTxes, MinFeeMode::Relay))
		Throw(E_COIN_TxFeeIsLow);
	if (nFees < GetMinRelayTxFee()) {
		DateTime now = DateTime::UtcNow();
		m_freeCount = pow(1.0 - 1.0/600, (now - exchange(m_dtLastFreeTx, now)).TotalSeconds);
		if (m_freeCount > 15*10000 && !IsFromMe(tx))
			Throw(E_COIN_TxRejectedByRateLimiter);
		m_freeCount += serSize;
	}

	if (ptxOld)
		RemoveFromMemory(ptxOld);
	AddToMemory(tx);
	
	if (m_iiEngEvents) {
		if (ptxOld)
			m_iiEngEvents->OnEraseTx(Hash(ptxOld));	
		m_iiEngEvents->OnProcessTx(tx);
	}
	Relay(tx);	

	vQueue.push_back(hash);
	return true;
}

void CoinEng::EraseOrphanTx(const HashValue& hash) {
	CTxPool::CHashToOrphan::iterator it = TxPool.m_hashToOrphan.find(hash);
	if (it != TxPool.m_hashToOrphan.end()) {
		EXT_FOR (const TxIn& txIn, it->second.TxIns()) {
			pair<CTxPool::CHashToHash::iterator, CTxPool::CHashToHash::iterator> range = TxPool.m_prevHashToOrphanHash.equal_range(txIn.PrevOutPoint.TxHash);
			for (CTxPool::CHashToHash::iterator j=range.first; j!=range.second;)
				if (j->second == hash)
					TxPool.m_prevHashToOrphanHash.erase(j++);
				else
					++j;
		}	
		TxPool.m_hashToOrphan.erase(it);
	}
}

void CoinEng::AddOrphan(const Tx& tx) {
	HashValue hash = Hash(tx);
	if (TxPool.m_hashToOrphan.insert(make_pair(hash, tx)).second) {
		EXT_FOR (const TxIn& txIn, tx.TxIns()) {
			TxPool.m_prevHashToOrphanHash.insert(make_pair(txIn.PrevOutPoint.TxHash, hash));
		}

		if (TxPool.m_hashToOrphan.size() > MAX_ORPHAN_TRANSACTIONS)
			EraseOrphanTx(TxPool.m_hashToOrphan.begin()->first);		//!!!? is it random enough?
	}
}

void TxMessage::Process(P2P::Link& link) {
	CoinEng& eng = static_cast<CoinEng&>(*link.Net);

	HashValue hash = Hash(Tx);
		
	if (eng.LiteMode) {
		EXT_LOCK (eng.Mtx) {
			if (!eng.CheckedFilteredTxHashes.count(hash))
				return;
		}
		if (eng.m_iiEngEvents)
			eng.m_iiEngEvents->OnProcessTx(Tx);
		return;
	}

	vector<HashValue> vQueue;

	EXT_LOCK (eng.TxPool.Mtx) {
		try {
			DBG_LOCAL_IGNORE_NAME(E_COIN_TxNotFound, ignE_COIN_TxNotFound);
			DBG_LOCAL_IGNORE_NAME(E_COIN_Misbehaving, ignE_COIN_Misbehaving);
			DBG_LOCAL_IGNORE_NAME(E_COIN_TxFeeIsLow, ignE_COIN_TxFeeIsLow);				//!!!T

			if (!eng.AddToPool(Tx, vQueue))
				return;
		} catch (const TxNotFoundExc&) {
			eng.AddOrphan(Tx);
			return;
		}
		DBG_LOCAL_IGNORE_NAME(E_COIN_TxNotFound, ignE_COIN_TxNotFound);	//!!!T
		for (int i=0; i<vQueue.size(); ++i) {
			HashValue hash = vQueue[i];
			pair<CoinEng::CTxPool::CHashToHash::iterator, CoinEng::CTxPool::CHashToHash::iterator> range = eng.TxPool.m_prevHashToOrphanHash.equal_range(hash);
			for (CoinEng::CTxPool::CHashToHash::iterator j=range.first; j!=range.second; ++j)
				eng.AddToPool(eng.TxPool.m_hashToOrphan[j->second], vQueue);
			eng.EraseOrphanTx(hash);
		}
	}
}

void MemPoolMessage::Process(P2P::Link& link) {
	CoinEng& eng = static_cast<CoinEng&>(*link.Net);
	ptr<InvMessage> m(new InvMessage);
	EXT_LOCK (eng.TxPool.Mtx) {
		for (CoinEng::CTxPool::CHashToTx::iterator it=eng.TxPool.m_hashToTx.begin(), e=eng.TxPool.m_hashToTx.end(); it!=e && m->Invs.size()<MAX_INV_SZ; ++it)
			m->Invs.push_back(Inventory(InventoryType::MSG_TX, it->first));
	}
	if (!m->Invs.empty())
		link.Send(m);
}


} // Coin::

