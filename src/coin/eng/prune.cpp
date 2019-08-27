/*######   Copyright (c) 2019      Ufasoft  http://ufasoft.com  mailto:support@ufasoft.com,  Sergey Pavlov  mailto:dev@ufasoft.com ####
#                                                                                                                                     #
# 		See LICENSE for licensing information                                                                                         #
#####################################################################################################################################*/

#include <el/ext.h>

#include "eng.h"

namespace Coin {

void PruneDbThread::Execute() {
	Name = "PruneThread";
	CCoinEngThreadKeeper engKeeper(&Eng);
	IBlockChainDb& db = *Eng.Db;
	for (int h = From; h <= To; ++h) {
		if (m_bStop)
			return;
		Block block = db.FindBlock(h);
		const auto& txes = block.Txes;
		CoinEngTransactionScope scopeBlockSavepoint(Eng);
		for (size_t i = 1; i < txes.size(); ++i)
			for (auto& txIn : txes[i].TxIns())
				db.PruneTxo(txIn.PrevOutPoint, h);
		db.SetLastPrunedHeight(h);
	}
	TRC(1, "Pruned spent TXOs upto Block " << To)
}

void CoinEng::TryStartPruning() {
	if (Mode == EngMode::Bootstrap) {
		auto lastPrunedHeight = Db->GetLastPrunedHeight();
		auto bestHeight = BestBlockHeight();
		if (lastPrunedHeight + PRUNE_UPTO_LAST_BLOCKS < bestHeight)
			(new PruneDbThread(_self, lastPrunedHeight + 1, bestHeight - PRUNE_UPTO_LAST_BLOCKS))->Start();
	}
}

} // Coin::
