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
	TRC(3, Height << "/" << Hash(_self));

	if (Height <= eng.Db->GetLastPrunedHeight())
		Throw(CoinErr::CannotReorganizeBeyondPrunedBlocks);

	const CTxes& txes = get_Txes();
	vector<int64_t> txids;
	txids.reserve(txes.size());

	if (eng.Mode != EngMode::Lite && eng.Mode != EngMode::BlockParser) {
		for (size_t i = txes.size(); i--;) {		// reverse order is important
			const Tx& tx = txes[i];
			Coin::HashValue txhash = Coin::Hash(tx);
			txids.push_back(letoh(*(int64_t*)txhash.data()));

			eng.OnDisconnectInputs(tx);

			if (!tx->IsCoinBase()) {
				EXT_FOR(const TxIn& txIn, tx.TxIns()) {
					eng.Db->UpdateCoins(txIn.PrevOutPoint, false, Height);
				}
			}
		}
	}

	eng.Db->DeleteBlock(Height, &txids);
}

void CoinEng::Reorganize(const BlockHeader& header) {
	TRC(1, "");

	BlockHeader prevBlock = header;
	Block curBest = Db->FindBlock(BestBlock().Height),
		forkBlock = curBest;
	vector<BlockHeader> vConnect;
	vector<ptr<BlockObj>> vDisconnect;
	unordered_set<HashValue> spentTxes;

	int hMaxHeader = Db->GetMaxHeaderHeight(),
		hMaxBlock = Db->GetMaxHeight();

	Tree.Add(header);
	BlockHeader lastCommon = Tree.LastCommonAncestor(Hash(header), Hash(BestHeader()));
	TRC(1, "Last Common Block: " << lastCommon.Height << "/" << Hash(lastCommon));

	for (BlockHeader h = header; h.Height > lastCommon.Height; h = h.GetPrevHeader())
		vConnect.push_back(h);

	for (int h = hMaxHeader; h > hMaxBlock && h > lastCommon.Height; --h) {
		vDisconnect.push_back(Db->FindHeader(h).m_pimpl);
	}
	for (int h = hMaxBlock; h > lastCommon.Height; --h) {
		Block b = Db->FindBlock(h);
		b.LoadToMemory();
		Tree.Add(b);
		vDisconnect.push_back(b.m_pimpl);
		if (Mode == EngMode::Bootstrap || UCFG_COIN_TXES_IN_BLOCKTABLE) {
			EXT_FOR(const Tx& tx, b.get_Txes()) {
				if (!tx->IsCoinBase()) {
					EXT_FOR(const TxIn& txIn, tx.TxIns()) {
						Tx tx;													// necessary, because without it FindTx() returns 'False Positive' (checks only 6 bytes of hash)
						if (!Db->FindTxByHash(txIn.PrevOutPoint.TxHash, &tx))
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
		unordered_map<HashValue, SpentTx> spentTxOffsets;

		EXT_LOCK(Caches.Mtx) {
			for (const auto& stx : Caches.m_cacheSpentTxes) {
				unordered_set<HashValue>::iterator it = spentTxes.find(stx.HashTx);
				if (it != spentTxes.end()) {
					spentTxOffsets[stx.HashTx] = stx;
					spentTxes.erase(it);
				}
			}
		}

		for (Block b = curBest; ; b = Tree.GetBlock(b.PrevBlockHash)) {												//!!!L   Long-time operation
			if (!Runned)
				Throw(ExtErr::ThreadInterrupted);
			const CTxes& txes = b.get_Txes();
			for (int nTx = 0; nTx < txes.size(); ++nTx) {
				HashValue hashTx = Hash(txes[nTx]);
				unordered_set<HashValue>::iterator it = spentTxes.find(hashTx);
				if (it != spentTxes.end()) {
					SpentTx stx = { hashTx, uint32_t(b.Height), b.OffsetOfTx(hashTx), (uint16_t)nTx };
					spentTxOffsets[hashTx] = stx;
					spentTxes.erase(it);
				}
			}

			if (b.Height == 0 || spentTxes.empty())
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

		for (size_t i = 0; i < vDisconnect.size(); ++i) {
			ptr<BlockObj>& bo = vDisconnect[i];
			if (bo->IsHeaderOnly()) {
				Db->DeleteBlock(bo->Height, nullptr);
			} else {
				Block b(bo.get());
				b.Disconnect();
				const CTxes& txes = b.get_Txes();
				for (CTxes::const_reverse_iterator it = txes.rbegin(); it != txes.rend(); ++it) {
					if (!it->m_pimpl->IsCoinBase())
						vResurrect.push_front(*it);
				}
			}
		}
	}

	ClearByHeightCaches();

	bool bNextAreHeaders = false;
	for (size_t i = vConnect.size(); i--;) {
		BlockHeader& headerToConnect = vConnect[i];
		if (bNextAreHeaders || (bNextAreHeaders = headerToConnect->IsHeaderOnly())) {
			headerToConnect.Connect();
		} else {
			Block block(headerToConnect.m_pimpl);
			block.Connect();
			SetBestHeader(block);
		}
	}

	if (Mode != EngMode::Bootstrap) {
		vector<HashValue> vQueue;
		EXT_FOR(const Tx& tx, vResurrect) {
			try {
				DBG_LOCAL_IGNORE_CONDITION(CoinErr::InputsAlreadySpent);
				DBG_LOCAL_IGNORE_CONDITION(CoinErr::TxNotFound);
				DBG_LOCAL_IGNORE_CONDITION(CoinErr::TxRejectedByRateLimiter);
				DBG_LOCAL_IGNORE_CONDITION(CoinErr::TxFeeIsLow);	//!!!TODO Fee calculated as Relay during Reorganize(). Should be MinFee

				TxPool.AddToPool(tx, vQueue);
			} catch (system_error& ex) {
				auto& code = ex.code();
				if (code != CoinErr::InputsAlreadySpent
					&& code != CoinErr::TxNotFound
					&& code != CoinErr::TxRejectedByRateLimiter
					&& code != CoinErr::TxFeeIsLow)
					throw;
			}
		}
	}
}

} // Coin::

