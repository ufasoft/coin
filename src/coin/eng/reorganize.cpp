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
	TRC(3,  Height << " " << Hash(_self));

	vector<int64_t> txids;
	txids.reserve(get_Txes().size());

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

	eng.Db->DeleteBlock(Height, &txids);
}

void CoinEng::Reorganize(const BlockHeader& header) {
	TRC(1, "");

	BlockHeader prevBlock = header;
	Block curBest = BestBlock(),
		forkBlock = curBest;
	vector<BlockHeader> vConnect;
	vector<ptr<BlockObj>> vDisconnect;
	unordered_set<HashValue> spentTxes;

	int hMaxHeader = Db->GetMaxHeaderHeight(),
		hMaxBlock = Db->GetMaxHeight();

	Tree.Add(header);
	BlockHeader lastCommon = Tree.LastCommonAncestor(Hash(header), Hash(BestHeader()));
	TRC(1, "Last Common Block: " << lastCommon.Height << " " << Hash(lastCommon));

	for (BlockHeader h=header; h.Height>lastCommon.Height; h=h.GetPrevHeader())
		vConnect.push_back(h);

	for (int h=hMaxHeader; h>hMaxBlock && h>lastCommon.Height; --h) {
		vDisconnect.push_back(Db->FindHeader(h).m_pimpl);
	}
	for (int h=hMaxBlock; h>lastCommon.Height; --h) {
		Block b = Db->FindBlock(h);
		b.LoadToMemory();
		Tree.Add(b);
		vDisconnect.push_back(b.m_pimpl);
		if (Mode == EngMode::Bootstrap) {
			EXT_FOR(const Tx& tx, b.get_Txes()) {
				if (!tx.IsCoinBase()) {
					EXT_FOR(const TxIn& txIn, tx.TxIns()) {
						Tx tx;													// necessary, because without it FindTx() returns 'False Positive' (checks only 6 bytes of hash)
						if (!Db->FindTxById(ConstBuf(txIn.PrevOutPoint.TxHash.data(), 8), &tx) || Hash(tx) != txIn.PrevOutPoint.TxHash)
							spentTxes.insert(txIn.PrevOutPoint.TxHash);
					}
				}
			}
		}
	}
	

	/*!!!R
	for (;; forkBlock = forkBlock.GetPrevBlock()) {
		if (Hash(forkBlock) == Hash(prevBlock))
			break;
		forkBlock.LoadToMemory();
		vDisconnect.push_back(forkBlock);
	} */
	

	if (!spentTxes.empty()) {
		unordered_map<HashValue, pair<uint32_t, uint32_t>> spentTxOffsets;

		EXT_LOCK (Caches.Mtx) {
			for (const auto& stx : Caches.m_cacheSpentTxes) {
				unordered_set<HashValue>::iterator it = spentTxes.find(stx.HashTx);
				if (it != spentTxes.end()) {
					spentTxOffsets[stx.HashTx] = make_pair(stx.Height, stx.Offset);
					spentTxes.erase(it);
				}		
			}
		}

		for (Block b=curBest; ; b=Tree.GetBlock(b.PrevBlockHash)) {												//!!!L   Long-time operation
			if (!Runned)
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
		CoinEngTransactionScope scopeBlockSavepoint(_self);
		Db->InsertSpentTxOffsets(spentTxOffsets);
	}

	list<Tx> vResurrect;
	{
		EnsureTransactionStarted();
		CoinEngTransactionScope scopeBlockSavepoint(_self);

		for (size_t i=0; i<vDisconnect.size(); ++i) {
			ptr<BlockObj>& bo = vDisconnect[i];
			if (bo->IsHeaderOnly()) {
				Db->DeleteBlock(bo->Height, nullptr);
			} else {
				Block b(bo.get());
				b.Disconnect();
				const CTxes& txes = b.get_Txes();
				for (CTxes::const_reverse_iterator it=txes.rbegin(); it!=txes.rend(); ++it) {
					if (!it->IsCoinBase())
						vResurrect.push_front(*it);
				}
			}
		}
	}

	ClearByHeightCaches();

	bool bNextAreHeaders = false;
	for (size_t i=vConnect.size(); i--;) {
		BlockHeader& headerToConnect = vConnect[i];
		if (bNextAreHeaders || (bNextAreHeaders = headerToConnect.IsHeaderOnly)) {
			headerToConnect.Connect();
		} else {
			Block block(headerToConnect.m_pimpl);
			block.Connect();
			SetBestHeader(block);
		}
	}
	
	vector<HashValue> vQueue;
	EXT_FOR (const Tx& tx, vResurrect) {
		try {
			DBG_LOCAL_IGNORE_CONDITION(CoinErr::InputsAlreadySpent);
			DBG_LOCAL_IGNORE_CONDITION(CoinErr::TxNotFound);
			DBG_LOCAL_IGNORE_CONDITION(CoinErr::TxFeeIsLow);	//!!!TODO Fee calculated as Relay during Reorganize(). Should be MinFee

			AddToPool(tx, vQueue);
		} catch (system_error& ex) {
			if (ex.code() != CoinErr::InputsAlreadySpent
				&& ex.code() != CoinErr::TxNotFound
				&& ex.code() != CoinErr::TxFeeIsLow)
				throw;
		}
	}
}

} // Coin::

