/*######   Copyright (c) 2015 Ufasoft  http://ufasoft.com  mailto:support@ufasoft.com,  Sergey Pavlov  mailto:dev@ufasoft.com ####
#                                                                                                                                     #
# 		See LICENSE for licensing information                                                                                         #
#####################################################################################################################################*/

#include <el/ext.h>

#include "eng.h"

namespace Coin {


void InvMessage::Process(P2P::Link& link) {
	CoinEng& eng = static_cast<CoinEng&>(*link.Net);

	base::Process(link);
	eng.m_bSomeInvReceived = true;

	ptr<GetDataMessage> m = new GetDataMessage;
	if (!eng.BestBlock()) {
		TRC(1, "Requesting for block " << eng.ChainParams.Genesis);

		m->Invs.push_back(Inventory(eng.Mode==EngMode::Lite ? InventoryType::MSG_FILTERED_BLOCK : InventoryType::MSG_BLOCK, eng.ChainParams.Genesis));
	}

	Inventory invLastBlock;
	EXT_FOR(const Inventory& inv, Invs) {
#ifdef X_DEBUG//!!!D
		if (inv.Type==InventoryType::MSG_BLOCK) {
			String s = " ";
			if (Block block = eng.LookupBlock(inv.HashValue))
				s += Convert::ToString(block.Height);
			TRC(3, "Inv block " << inv.HashValue << s);
		}
#endif

		if (eng.Have(inv)) {
			if (inv.Type==InventoryType::MSG_BLOCK || eng.Mode==EngMode::Lite && inv.Type==InventoryType::MSG_FILTERED_BLOCK) {
				Block orphanRoot(nullptr);
				EXT_LOCK(eng.Caches.Mtx) {
					Block block(nullptr);
					if (Lookup(eng.Caches.OrphanBlocks, inv.HashValue, block))
						orphanRoot = block.GetOrphanRoot();
				}
				if (orphanRoot)
					link.Send(new GetBlocksMessage(eng.BestBlock(), Hash(orphanRoot)));
				else
					invLastBlock = inv;							// Always request the last block in an inv bundle (even if we already have it), as it is the trigger for the other side to send further invs.
																// If we are stuck on a (very long) side chain, this is necessary to connect earlier received orphan blocks to the chain again.
			}
		} else {
			if (inv.Type==InventoryType::MSG_BLOCK || eng.Mode==EngMode::Lite && inv.Type==InventoryType::MSG_FILTERED_BLOCK) {
				m->Invs.push_back(eng.Mode==EngMode::Lite ? Inventory(InventoryType::MSG_FILTERED_BLOCK, inv.HashValue) : inv);
			} else {
				switch (eng.Mode) {
				case EngMode::BlockExplorer:
					break;
				default:
					m->Invs.push_back(inv);
				}
			}
		}
	}
	if (invLastBlock.HashValue != HashValue()) {
		TRC(2, "InvLastBlock: " << invLastBlock.HashValue);

		m->Invs.push_back(invLastBlock);
		link.Send(new GetBlocksMessage(eng.LookupBlock(invLastBlock.HashValue), HashValue()));
	}

	if (!m->Invs.empty()) {
		m->Invs.resize(std::min(m->Invs.size(), MAX_INV_SZ));
		link.Send(m);
	}
}

} // Coin::

