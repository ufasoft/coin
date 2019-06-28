/*######   Copyright (c) 2019      Ufasoft  http://ufasoft.com  mailto:support@ufasoft.com,  Sergey Pavlov  mailto:dev@ufasoft.com ####
#                                                                                                                                     #
# 		See LICENSE for licensing information                                                                                         #
#####################################################################################################################################*/

#include <el/ext.h>

#include "eng.h"

namespace Coin {

void PruneDbThread::Execute() {
	CCoinEngThreadKeeper engKeeper(&Eng);
	for (int h = From; h <= To; Sleep(10), ++h) {
		CoinEngTransactionScope scopeBlockSavepoint(Eng);
		Block block = Eng.Db->FindBlock(h);
		const CTxes& txes = block.Txes;
		for (int i = 1; i < txes.size(); ++i) {
			for (auto& txIn : txes[i].TxIns())
				Eng.Db->PruneTxo(txIn.PrevOutPoint, h);
		}
		Eng.Db->SetLastPrunedHeight(h);
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
