/*######   Copyright (c) 2011-2015 Ufasoft  http://ufasoft.com  mailto:support@ufasoft.com,  Sergey Pavlov  mailto:dev@ufasoft.com ####
#                                                                                                                                     #
# 		See LICENSE for licensing information                                                                                         #
#####################################################################################################################################*/

#include <el/ext.h>

#include "coin-protocol.h"
#include "eng.h"


namespace Coin {

void Block::Disconnect() const {
	CoinEng& eng = Eng();
	TRC(3, " " << Hash(_self) << " Height: " << Height);

	vector<int64_t> txids;
	txids.reserve(get_Txes().size());

	eng.EnsureTransactionStarted();
	CoinEngTransactionScope scopeBlockSavepoint(eng);

	if (eng.Mode!=EngMode::Lite && eng.Mode!=EngMode::BlockParser) {
		for (size_t i=get_Txes().size(); i--;) {		// reverse order is important
			const Tx& tx = get_Txes()[i];
			Coin::HashValue txhash = Coin::Hash(tx);
			txids.push_back(letoh(*(int64_t*)txhash.data()));

			eng.OnDisconnectInputs(tx);

			if (!tx.IsCoinBase()) {
				EXT_FOR (const TxIn& txIn, tx.TxIns()) {
					eng.Db->UpdateCoins(txIn.PrevOutPoint, false);
				}
			}
		}
	}

	eng.Db->DeleteBlock(Height, txids);
}

void Block::Reorganize() {
	TRC(1, "");

	CoinEng& eng = Eng();
	Block prevBlock = _self;
	Block curBest = eng.BestBlock(),
		forkBlock = curBest;
	vector<Block> vConnect, vDisconnect;

	unordered_set<HashValue> spentTxes;

	for (;; forkBlock = forkBlock.GetPrevBlock()) {
		while (prevBlock.Height > forkBlock.Height)
			vConnect.push_back(exchange(prevBlock, prevBlock.GetPrevBlock()));
		if (Hash(forkBlock) == Hash(prevBlock))
			break;
		forkBlock.LoadToMemory();
		vDisconnect.push_back(forkBlock);
		if (eng.Mode == EngMode::Bootstrap) {
			EXT_FOR (const Tx& tx, forkBlock.get_Txes()) {
				if (!tx.IsCoinBase()) {
					EXT_FOR (const TxIn& txIn, tx.TxIns()) {
						Tx tx;													// necessary, because without it FindTx() returns 'False Positive' (checks only 6 bytes of hash)
						if (!eng.Db->FindTxById(ConstBuf(txIn.PrevOutPoint.TxHash.data(), 8), &tx) || Hash(tx) != txIn.PrevOutPoint.TxHash)
							spentTxes.insert(txIn.PrevOutPoint.TxHash);
					}
				}
			}
		}
	}
	TRC(1, "Fork Block: " << Hash(forkBlock) << "  H: " << forkBlock.Height);

	if (!spentTxes.empty()) {
		unordered_map<HashValue, pair<uint32_t, uint32_t>> spentTxOffsets;

		EXT_LOCK (eng.Caches.Mtx) {
			for (const auto& stx : eng.Caches.m_cacheSpentTxes) {
				unordered_set<HashValue>::iterator it = spentTxes.find(stx.HashTx);
				if (it != spentTxes.end()) {
					spentTxOffsets[stx.HashTx] = make_pair(stx.Height, stx.Offset);
					spentTxes.erase(it);
				}		
			}
		}

		for (Block b=curBest; ; b=b.GetPrevBlock()) {												//!!!L   Long-time operation
			if (!eng.Runned)
				Throw(ExtErr::ThreadInterrupted);
			EXT_FOR (const Tx& tx, b.get_Txes()) {
				unordered_set<HashValue>::iterator it = spentTxes.find(Hash(tx));
				if (it != spentTxes.end()) {
					spentTxOffsets[*it] = make_pair(uint32_t(b.Height), b.OffsetOfTx(*it));
					spentTxes.erase(it);
				}		
			}

			if (b.Height==0 || spentTxes.empty())
				break;
		}
		ASSERT(spentTxes.empty());
		CoinEngTransactionScope scopeBlockSavepoint(eng);
		eng.Db->InsertSpentTxOffsets(spentTxOffsets);
	}

	list<Tx> vResurrect;
	for (size_t i=0; i<vDisconnect.size(); ++i) {
		forkBlock = vDisconnect[i];
		forkBlock.Disconnect();
		const CTxes& txes = forkBlock.get_Txes();
		for (CTxes::const_reverse_iterator it=txes.rbegin(); it!=txes.rend(); ++it) {
			if (!it->IsCoinBase())
				vResurrect.push_front(*it);
		}
		EXT_LOCKED(eng.Caches.Mtx, eng.Caches.HashToAlternativeChain.insert(make_pair(Hash(forkBlock), forkBlock)));
	}


	EXT_LOCKED(eng.Caches.Mtx, eng.Caches.HeightToHashCache.clear());

	for (size_t i=vConnect.size(); i--;) {
		Block& block = vConnect[i];
		block.Connect();
		eng.Caches.m_bestBlock = block;
		EXT_LOCKED(eng.Caches.Mtx, eng.Caches.HashToAlternativeChain.erase(Hash(block)));
		EXT_FOR (const Tx& tx, block.get_Txes()) {
			eng.RemoveFromMemory(tx);
		}
	}

	vector<HashValue> vQueue;
	EXT_FOR (const Tx& tx, vResurrect) {
		try {
			DBG_LOCAL_IGNORE_CONDITION(CoinErr::InputsAlreadySpent);
			DBG_LOCAL_IGNORE_CONDITION(CoinErr::TxNotFound);
			DBG_LOCAL_IGNORE_CONDITION(CoinErr::TxFeeIsLow);	//!!!TODO Fee calculated as Relay during Reorganize(). Should be MinFee

			eng.AddToPool(tx, vQueue);
		} catch (system_error& ex) {
			if (ex.code() != CoinErr::InputsAlreadySpent
				&& ex.code() != CoinErr::TxNotFound
				&& ex.code() != CoinErr::TxFeeIsLow)
				throw;
		}
	}
}

} // Coin::

