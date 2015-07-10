/*######   Copyright (c) 2015 Ufasoft  http://ufasoft.com  mailto:support@ufasoft.com,  Sergey Pavlov  mailto:dev@ufasoft.com      ####
#                                                                                                                                     #
# 		See LICENSE for licensing information                                                                                         #
#####################################################################################################################################*/

#include <el/ext.h>

#include "coin-protocol.h"

namespace Coin {

BlockTreeItem::BlockTreeItem(const BlockHeader& header)
	: base(header)
{
}

BlockTreeItem BlockTree::FindInMap(const HashValue& hashBlock) const {
	BlockTreeItem item;
	EXT_LOCKED(Mtx, Lookup(Map, hashBlock, item));
	return item;
}

BlockHeader BlockTree::FindHeader(const HashValue& hashBlock) const {
	if (BlockTreeItem item = FindInMap(hashBlock))
		return item;
	return Eng().Db->FindHeader(hashBlock);
}

BlockTreeItem BlockTree::GetHeader(const HashValue& hashBlock) const {
	if (BlockTreeItem item = FindHeader(hashBlock))
		return item;
	Throw(E_FAIL);
}

Block BlockTree::FindBlock(const HashValue& hashBlock) const {
	if (BlockTreeItem item = FindInMap(hashBlock)) {
		if (!item.IsHeaderOnly)
			return Block(item.m_pimpl);
	}
	return Eng().LookupBlock(hashBlock);
}

Block BlockTree::GetBlock(const HashValue& hashBlock) const {
	if (Block b = FindBlock(hashBlock))
		return b;
	Throw(E_FAIL);
}

BlockHeader BlockTree::GetAncestor(const HashValue& hashBlock, int height) const {
	CoinEng& eng = Eng();
	BlockTreeItem item = GetHeader(hashBlock);
	if (height == item.Height)
		return item;
	if (height > item.Height)
		return BlockHeader(nullptr);
	EXT_LOCK(Mtx) {
		while (height < item.Height-1) {
			if (!Lookup(Map, item.PrevBlockHash, item))
				goto LAB_TRY_MAIN_CHAIN;
		}
	}
	return GetHeader(item.PrevBlockHash);
LAB_TRY_MAIN_CHAIN:
	return eng.Db->FindHeader(height);
}

BlockHeader BlockTree::LastCommonAncestor(const HashValue& ha, const HashValue& hb) const {
	int h = (min)(GetHeader(ha).Height, GetHeader(hb).Height);
	ASSERT(h >= 0);
	BlockHeader a = GetAncestor(ha, h), b = GetAncestor(hb, h);
	while (Hash(a) != Hash(b)) {
		a = a.GetPrevHeader();
		b = b.GetPrevHeader();
	}
	return a;
}

vector<Block> BlockTree::FindNextBlocks(const HashValue& hashBlock) const {
	vector<Block> r;
	EXT_LOCK(Mtx) {
		EXT_FOR(CMap::value_type pp, Map) {
			if (pp.second.PrevBlockHash == hashBlock && !pp.second.IsHeaderOnly)
				r.push_back(Block(pp.second.m_pimpl));
		}
	}
	return r;
}

void BlockTree::Add(const BlockHeader& header) {	
	ASSERT(header.Height >= 0);
	EXT_LOCKED(Mtx, Map[Hash(header)] = BlockTreeItem(header));
}

void BlockTree::RemovePersistentBlock(const HashValue& hashBlock) {
	EXT_LOCKED(Mtx, Map.erase(hashBlock));
}

bool CoinEng::HaveAllBlocksUntil(const HashValue& hash) {
	EXT_LOCK(Caches.Mtx) {
		if (Caches.HashToBlockCache.count(hash))
			return true;
	}
	return Db->HaveBlock(hash);
}

bool CoinEng::HaveBlock(const HashValue& hash) {
	EXT_LOCK(Caches.Mtx) {
		if (Caches.OrphanBlocks.count(hash))
			return true;
	}
	BlockTreeItem bti = Tree.FindInMap(hash);
	if (bti && !bti.IsHeaderOnly)
		return true;
	return HaveAllBlocksUntil(hash);
}

bool CoinEng::HaveTxInDb(const HashValue& hashTx) {
	return Tx::TryFromDb(hashTx, nullptr);
}

bool CoinEng::AlreadyHave(const Inventory& inv) {
	switch (inv.Type) {
	case InventoryType::MSG_BLOCK:
	case InventoryType::MSG_FILTERED_BLOCK:
		return HaveBlock(inv.HashValue);
	case InventoryType::MSG_TX:
		if (EXT_LOCKED(TxPool.Mtx, TxPool.m_hashToTx.count(inv.HashValue)))			// don't combine with next expr, because lock's scope will be wider
			return true;
		return HaveTxInDb(inv.HashValue);
	default:
		return false; //!!!TODO
	}
}

bool CoinEng::IsInitialBlockDownload() {
	if (UpgradingDatabaseHeight)
		return true;
	Block bestBlock = BestBlock();
	if (!bestBlock || bestBlock.Height < ChainParams.LastCheckpointHeight-INITIAL_BLOCK_THRESHOLD)
		return true;
	DateTime now = Clock::now();
	HashValue hash = Hash(bestBlock);
	EXT_LOCK(MtxBestUpdate) {
		if (m_hashPrevBestUpdate != hash) {
			m_hashPrevBestUpdate = hash;
			m_dtPrevBestUpdate = now;
		}
		return now-m_dtPrevBestUpdate < TimeSpan::FromSeconds(20) && bestBlock.Timestamp < now-hours(24);
	}
}

bool CoinEng::MarkBlockAsReceived(const HashValue& hashBlock) {
	EXT_LOCK(Mtx) {
		auto it = MapBlocksInFlight.find(hashBlock);
		if (it != MapBlocksInFlight.end()) {
			it->second->Link.BlocksInFlight.erase(it->second);
			it->second->Link.DtStallingSince = DateTime();
			MapBlocksInFlight.erase(it);
			return true;
		}
	}
	return false;
}

// GetDataMessage defined after CoinEng. So this function cannot be member of class CoinEng
static void MarkBlockAsInFlight(CoinEng& eng, ptr<GetDataMessage>& mGetData, CoinLink& link, const Inventory& inv) {
	eng.MarkBlockAsReceived(inv.HashValue);
	eng.MapBlocksInFlight[inv.HashValue] = link.BlocksInFlight.insert(link.BlocksInFlight.end(), QueuedBlockItem(link, inv.HashValue, Clock::now()));
	if (!mGetData)
		mGetData = new GetDataMessage;
	mGetData->Invs.push_back(eng.Mode==EngMode::Lite ? Inventory(InventoryType::MSG_FILTERED_BLOCK, inv.HashValue) : inv);
}

void CoinLink::UpdateBlockAvailability(const HashValue& hashBlock) {
	CoinEng& eng = static_cast<CoinEng&>(*Net);

	if (eng.Tree.FindHeader(HashBlockLastUnknown)) {
		if (!HashBlockBestKnown)
			HashBlockBestKnown = HashBlockLastUnknown;
		HashBlockLastUnknown = HashValue::Null();
	}

	if (BlockTreeItem bti = eng.Tree.FindHeader(hashBlock)) {
		BlockTreeItem btiPrevBest = eng.Tree.FindHeader(HashBlockBestKnown);
		if (!btiPrevBest || btiPrevBest.Height < bti.Height)
			HashBlockBestKnown = hashBlock;
	} else
		HashBlockLastUnknown = hashBlock;
}

void InvMessage::Process(P2P::Link& link) {
	CoinEng& eng = static_cast<CoinEng&>(*link.Net);
	CoinLink& clink = static_cast<CoinLink&>(link);

	base::Process(link);

	ptr<GetDataMessage> m = new GetDataMessage;
	if (!eng.BestBlock()) {
		TRC(1, "Requesting for block " << eng.ChainParams.Genesis);

		m->Invs.push_back(Inventory(InventoryType::MSG_BLOCK, eng.ChainParams.Genesis));
	}

	bool bCloseToBeSync = false;
	if (Block bestBlock = eng.BestBlock())
		bCloseToBeSync = Clock::now() < bestBlock.Timestamp + eng.ChainParams.BlockSpan*20;

	HashValue hashLastInvBlock;
	

	EXT_FOR(const Inventory& inv, Invs) {

		bool bIsBlockInv = inv.Type==InventoryType::MSG_BLOCK || eng.Mode==EngMode::Lite && inv.Type==InventoryType::MSG_FILTERED_BLOCK;
		if (bIsBlockInv)
			clink.UpdateBlockAvailability(inv.HashValue);

		if (eng.AlreadyHave(inv)) {
			/*!!!R
			if (bIsBlockInv) {
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
			*/
		} else {
			if (bIsBlockInv) {
				if (!EXT_LOCKED(eng.Mtx, eng.MapBlocksInFlight.count(inv.HashValue))) {
					hashLastInvBlock = inv.HashValue;

					if (bCloseToBeSync && clink.BlocksInFlight.size() < MAX_BLOCKS_IN_TRANSIT_PER_PEER) {
						MarkBlockAsInFlight(eng, m, clink, inv);
					}
				}
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

	if (hashLastInvBlock) {
		HashValue hashBest;
		if (BlockHeader bh = eng.BestHeader())
			hashBest = Hash(bh);
		link.Send(new GetHeadersMessage(hashBest, hashLastInvBlock));
	}

	if (!m->Invs.empty()) {
		m->Invs.resize(std::min(m->Invs.size(), MAX_INV_SZ));
		link.Send(m);
	}
}

void HeadersMessage::Process(P2P::Link& link) {
	CoinEng& eng = Eng();
	CoinLink& clink = static_cast<CoinLink&>(link);

	if (Headers.size() > MAX_HEADERS_RESULTS)
		throw PeerMisbehavingException(20);

	if (eng.UpgradingDatabaseHeight)
		return;

	BlockHeader headerLast;
	for (size_t i=0; i<Headers.size(); ++i) {
		if (BlockHeader headerPrev = exchange(headerLast, Headers[i]))
			if (Hash(headerPrev) != headerLast.PrevBlockHash)
				throw PeerMisbehavingException(20);
		headerLast.Accept();
	}
	if (headerLast) {
		HashValue hashLast = Hash(headerLast);
		clink.UpdateBlockAvailability(hashLast);
		if (Headers.size() == MAX_HEADERS_RESULTS) {
			TRC(4, "Last Header: " << headerLast.Height << " " << hashLast);
			link.Send(new GetHeadersMessage(hashLast));
		}
	}
}

void CoinLink::RequestHeaders() {
	CoinEng& eng = static_cast<CoinEng&>(*Net);

	if (!IsSyncStarted && !IsClient && !eng.UpgradingDatabaseHeight) {
		if (Block bestBlock = eng.BestBlock()) {
			bool bFetch = IsPreferredDownload || (0==eng.aPreferredDownloadPeers && !IsClient && !IsOneShot);
			IsSyncStarted = eng.aSyncStartedPeers.load()==0 && bFetch || bestBlock.Timestamp > Clock::now()-TimeSpan::FromHours(24);
			if (IsSyncStarted) {
				++eng.aSyncStartedPeers;

				HashValue hashBest;
				if (BlockHeader bh = eng.BestHeader())
					hashBest = Hash(bh);
				Send(new GetHeadersMessage(hashBest));
			}
		} else {
			InventoryType invType = eng.Mode==EngMode::Lite ? InventoryType::MSG_FILTERED_BLOCK : InventoryType::MSG_BLOCK;
			ptr<GetDataMessage> m = new GetDataMessage;
			m->Invs.push_back(Inventory(invType, eng.ChainParams.Genesis));
			Send(m);
		}
	}
}

void CoinLink::RequestBlocks() {
	CoinEng& eng = static_cast<CoinEng&>(*Net);

	bool bFetch = IsPreferredDownload || (0==eng.aPreferredDownloadPeers && !IsClient && !IsOneShot);

	int count = MAX_BLOCKS_IN_TRANSIT_PER_PEER - (int)EXT_LOCKED(eng.Mtx, BlocksInFlight.size());
	if (IsClient || (!bFetch && eng.IsInitialBlockDownload()) || count<=0 || !HashBlockBestKnown)
		return;

	if (!HashBlockLastCommon) {
		BlockTreeItem btiBestKnown = eng.Tree.GetHeader(HashBlockBestKnown);
		int h = (min)(eng.BestBlockHeight(), btiBestKnown.Height);
		ASSERT(h >= 0);
		HashBlockLastCommon = Hash(eng.Tree.GetAncestor(HashBlockBestKnown, h));
	}
	HashBlockLastCommon = Hash(eng.Tree.LastCommonAncestor(HashBlockLastCommon, HashBlockBestKnown));

	if (HashBlockLastCommon == HashBlockBestKnown)
		return;
	
	BlockTreeItem bti = eng.Tree.GetHeader(HashBlockLastCommon),
		btiBestKnown = eng.Tree.GetHeader(HashBlockBestKnown);
	HashValue prevHashBlockLastCommon = HashBlockLastCommon;
	TRC(4, "HashBlockLastCommon: " << bti.Height << " " << HashBlockLastCommon << "     HashBlockBestKnown: " << btiBestKnown.Height << " " << HashBlockBestKnown);

	int nWindowEnd = bti.Height + BLOCK_DOWNLOAD_WINDOW,
		nMaxHeight = (min)(btiBestKnown.Height, nWindowEnd + 1);

	CoinLink *staller = 0, *waitingFor = 0;
	ptr<GetDataMessage> mGetData;
	while (bti.Height < nMaxHeight) {
		typedef map<int, HashValue> CHeight2Value;
		CHeight2Value height2hash;

		int nToFetch = (min)(nMaxHeight-bti.Height, (max)(128, count - (mGetData ? (int)mGetData->Invs.size() : 0) ));
		BlockHeader cur = bti = eng.Tree.GetAncestor(HashBlockBestKnown, bti.Height+nToFetch);
		height2hash.insert(make_pair(cur.Height, Hash(cur)));
		for (int i=1; i<nToFetch; ++i) {
			height2hash.insert(make_pair(cur.Height-1, cur.PrevBlockHash));
			cur = cur.GetPrevHeader();
		}

		EXT_FOR(const CHeight2Value::value_type& pp, height2hash) {
			if (eng.HaveBlock(pp.second)) {
				if (eng.HaveAllBlocksUntil(pp.second))
					HashBlockLastCommon = pp.second;
			} else {
				EXT_LOCK(eng.Mtx) {
					CoinEng::CMapBlocksInFlight::iterator it = eng.MapBlocksInFlight.find(pp.second);
					if (it != eng.MapBlocksInFlight.end()) {
						if (!waitingFor) {
							waitingFor = &it->second->Link;
							TRC(4, "Stall waiting for " << pp.first << " " << pp.second << " from " << waitingFor->Peer->EndPoint.Address);
						}
					} else if (pp.first > nWindowEnd) {
						if (!mGetData && waitingFor != this)
							staller = waitingFor;
						break;
					} else {
						MarkBlockAsInFlight(eng, mGetData, _self, Inventory(InventoryType::MSG_BLOCK, pp.second));
						if (mGetData && mGetData->Invs.size() == count)
							goto LAB_OUT_LOOP;
					}
				}
			}
		}

		if (EXT_LOCKED(eng.Mtx, BlocksInFlight.empty()) && staller) {
			EXT_LOCK(staller->Mtx) {
				if (staller->DtStallingSince == DateTime()) {
					staller->DtStallingSince = Clock::now();
					TRC(3, "Stall started " << staller->Peer->get_EndPoint());
				}
			}
		}
	}
LAB_OUT_LOOP:
	if (prevHashBlockLastCommon != HashBlockLastCommon) {
		TRC(4, "Updated HashBlockLastCommon: " << HashBlockLastCommon);
	}


	if (mGetData) {
		mGetData->Invs.resize(std::min(mGetData->Invs.size(), MAX_INV_SZ));
		Send(mGetData);
	}
}

void BlockMessage::ProcessContent(P2P::Link& link, bool bRequested) {
	Block.Process(&link, bRequested);
}

void BlockMessage::Process(P2P::Link& link) {
	CoinEng& eng = Eng();
	bool bRequested = eng.MarkBlockAsReceived(Hash(Block));

	CoinLink& clink = static_cast<CoinLink&>(link);
	HashValue hash = Hash(Block);
	EXT_LOCKED(clink.Mtx, clink.KnownInvertorySet.insert(Inventory(InventoryType::MSG_BLOCK, hash)));
	if (eng.UpgradingDatabaseHeight)
		return;
	ProcessContent(link, bRequested);

	if (Eng().Mode!=EngMode::Lite) {																	// for LiteMode we wait until txes downloaded
		if (EXT_LOCKED(eng.Mtx, clink.BlocksInFlight.size() <= MAX_BLOCKS_IN_TRANSIT_PER_PEER/2))
			clink.RequestBlocks();
	}
}

void MerkleBlockMessage::ProcessContent(P2P::Link& link, bool bRequested) {
	pair<HashValue, vector<HashValue>> pp = PartialMT.ExtractMatches();
	if (pp.first != Block.get_MerkleRoot())
		Throw(CoinErr::MerkleRootMismatch);
	if (pp.second.empty())
		Block.Process(&link, bRequested);
	else {
		CoinLink& clink = static_cast<CoinLink&>(link);
		clink.m_curMerkleBlock = Block;					// Defer processing until all txes will be received
		clink.m_curMatchedHashes = pp.second;
	}
}

void MerkleBlockMessage::Process(P2P::Link& link) {
	if (Eng().Mode==EngMode::Lite)
		base::Process(link);
}

} // Coin::

