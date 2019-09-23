/*######   Copyright (c) 2018-2019 Ufasoft  http://ufasoft.com  mailto:support@ufasoft.com,  Sergey Pavlov  mailto:dev@ufasoft.com ####
#                                                                                                                                     #
# 		See LICENSE for licensing information                                                                                         #
#####################################################################################################################################*/

#include <el/ext.h>

#include "coin-protocol.h"

namespace Coin {

bool CoinEng::HaveAllBlocksUntil(const HashValue& hash) {
	BlockTreeItem bti = Tree.FindInMap(hash);
	if (bti && !bti->IsHeaderOnly())
		return true;
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
	if (bti && !bti->IsHeaderOnly())
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
		if (EXT_LOCKED(TxPool.Mtx, TxPool.m_hashToTxInfo.count(inv.HashValue)))			// don't combine with next expr, because lock's scope will be wider
			return true;
		return HaveTxInDb(inv.HashValue);
	default:
		return false; //!!!TODO
	}
}

bool CoinEng::IsInitialBlockDownload() {
	if (UpgradingDatabaseHeight || Rescanning)
		return true;
	BlockHeader bestBlock = BestBlock();
	if (!bestBlock || bestBlock.Height < ChainParams.LastCheckpointHeight - INITIAL_BLOCK_THRESHOLD)
		return true;
	DateTime now = Clock::now();
	HashValue hash = Hash(bestBlock);
	EXT_LOCK(MtxBestUpdate) {
		if (m_hashPrevBestUpdate != hash) {
			m_hashPrevBestUpdate = hash;
			m_dtPrevBestUpdate = now;
		}
		return now - m_dtPrevBestUpdate < TimeSpan::FromSeconds(20) && bestBlock.Timestamp < now - hours(24);
	}
}

bool CoinEng::MarkBlockAsReceived(const HashValue& hashBlock) {
	EXT_LOCK(Mtx) {
		auto it = MapBlocksInFlight.find(hashBlock);
		if (it != MapBlocksInFlight.end()) {
			BlocksInFlightList::iterator itInFlight = it->second;
			Coin::Link& link = itInFlight->Link;
			link.DtStallingSince = DateTime();
			link.BlocksInFlight.erase(itInFlight);
			MapBlocksInFlight.erase(it);
			return true;
		}
	}
	return false;
}

void CoinEng::MarkBlockAsInFlight(GetDataMessage& mGetData, Link& link, const Inventory& inv) {
	MarkBlockAsReceived(inv.HashValue);
	MapBlocksInFlight[inv.HashValue] = link.BlocksInFlight.insert(link.BlocksInFlight.end(), QueuedBlockItem(link, inv.HashValue, Clock::now()));

	InventoryType invType = Mode == EngMode::Lite
		? invType = InventoryType::MSG_FILTERED_BLOCK		// MSG_FILTERED_WITNESS_BLOCK is not used
		: inv.Type| (link.HasWitness ? InventoryType::MSG_WITNESS_FLAG : (InventoryType)0);
	mGetData.Invs.push_back(Inventory(invType, inv.HashValue));
}

void Link::SetHashBlockBestKnown(const HashValue& hash) {
	CoinEng& eng = static_cast<CoinEng&>(*Net);

	HashBlockBestKnown = hash;

	TRC(4, Peer->get_EndPoint().Address << " " << eng.BlockStringId(HashBlockBestKnown));
}

void Link::UpdateBlockAvailability(const HashValue& hashBlock) {
	CoinEng& eng = static_cast<CoinEng&>(*Net);

	if (eng.Tree.FindHeader(HashBlockLastUnknown)) {
		if (!HashBlockBestKnown)
			SetHashBlockBestKnown(HashBlockLastUnknown);
		HashBlockLastUnknown = HashValue::Null();
	}

	if (BlockTreeItem bti = eng.Tree.FindHeader(hashBlock)) {
		BlockTreeItem btiPrevBest = eng.Tree.FindHeader(HashBlockBestKnown);
		if (!btiPrevBest || btiPrevBest.Height < bti.Height)
			SetHashBlockBestKnown(hashBlock);
	} else {
		HashBlockLastUnknown = hashBlock;

		TRC(4, Peer->get_EndPoint().Address << " let HashBlockLastUnknown = " << eng.BlockStringId(hashBlock));
	}
}

void InvMessage::Process(Link& link) {
	CoinEng& eng = static_cast<CoinEng&>(*link.Net);

	base::Process(link);

	ptr<GetDataMessage> m = new GetDataMessage;
	if (!eng.BestBlock()) {
		TRC(1, "Requesting for block " << eng.ChainParams.Genesis);

		InventoryType invType = eng.Mode == EngMode::Lite
			? invType = InventoryType::MSG_FILTERED_BLOCK		// MSG_FILTERED_WITNESS_BLOCK is not used
			: InventoryType::MSG_BLOCK | (link.HasWitness ? InventoryType::MSG_WITNESS_FLAG : (InventoryType)0);
		m->Invs.push_back(Inventory(invType, eng.ChainParams.Genesis));
	}

	bool bCloseToBeSync = false;
	if (BlockHeader bestBlock = eng.BestBlock())
		bCloseToBeSync = Timestamp < bestBlock.Timestamp + eng.ChainParams.BlockSpan * 20;

	HashValue hashLastInvBlock;


	EXT_FOR(const Inventory& inv, Invs) {
#ifdef _DEBUG
		if (inv.Type == InventoryType::MSG_WITNESS_BLOCK) {
			int aa = 1;
		}
#endif

		bool bIsBlockInv = inv.Type == InventoryType::MSG_BLOCK || eng.Mode == EngMode::Lite && inv.Type == InventoryType::MSG_FILTERED_BLOCK;
		if (bIsBlockInv && (link.HasWitness || eng.BestBlockHeight() < eng.ChainParams.SegwitHeight))	//!!!?
			link.UpdateBlockAvailability(inv.HashValue);

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

					if (bCloseToBeSync && link.BlocksInFlight.size() < MAX_BLOCKS_IN_TRANSIT_PER_PEER) {
						EXT_LOCK(eng.Mtx) {
							eng.MarkBlockAsInFlight(*m, link, inv);
						}
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
		BlockHeader bh = eng.BestHeader();
		link.Send(new GetHeadersMessage(bh ? Hash(bh) : HashValue(), hashLastInvBlock));
	}

	if (!m->Invs.empty()) {
		m->Invs.resize(std::min(m->Invs.size(), (size_t)ProtocolParam::MAX_INV_SZ));
		link.Send(m);
	}
}

BlockHeader CoinEng::ProcessNewBlockHeaders(const vector<BlockHeader>& headers, Link* link) {
	BlockHeader headerLast;
	for (size_t i = 0; i < headers.size(); ++i) {
		if (BlockHeader headerPrev = exchange(headerLast, headers[i]))
			if (Hash(headerPrev) != headerLast.PrevBlockHash)
				throw PeerMisbehavingException(20);
		headerLast.Accept(link);
	}
	return headerLast;
}

void HeadersMessage::Process(Link& link) {
	CoinEng& eng = Eng();

	if (Headers.size() > ProtocolParam::MAX_HEADERS_RESULTS)
		throw PeerMisbehavingException(20);

	if (eng.UpgradingDatabaseHeight)
		return;

	if (BlockHeader headerLast = eng.ProcessNewBlockHeaders(Headers, &link)) {
		HashValue hashLast = Hash(headerLast);
		link.UpdateBlockAvailability(hashLast);
		if (Headers.size() == ProtocolParam::MAX_HEADERS_RESULTS) {
			TRC(4, "Last Header: " << headerLast.Height << "/" << hashLast);
			link.Send(new GetHeadersMessage(hashLast));
		}
	}
}

void Link::RequestHeaders() {
	CoinEng& eng = static_cast<CoinEng&>(*Net);

	if (!IsSyncStarted && !IsClient && !eng.UpgradingDatabaseHeight) {
		if (BlockHeader bestBlock = eng.BestBlock()) {
			bool bFetch = IsPreferredDownload || (0 == eng.aPreferredDownloadPeers && !IsOneShot);
			IsSyncStarted = eng.aSyncStartedPeers.load() == 0 && bFetch || bestBlock.Timestamp > Clock::now() - TimeSpan::FromHours(24);
			if (IsSyncStarted) {
				++eng.aSyncStartedPeers;

				BlockHeader bh = eng.BestHeader();
				HashValue hashBest =
					!bh ? HashValue()
					: bh.PrevBlockHash ? bh.PrevBlockHash			// Start with previous of best known header to avoid getting empty Headers list.
					: Hash(bh);
				Send(new GetHeadersMessage(hashBest));
			}
		} else {
			InventoryType invType = eng.Mode == EngMode::Lite
				? invType = InventoryType::MSG_FILTERED_BLOCK		// MSG_FILTERED_WITNESS_BLOCK is not used
				: InventoryType::MSG_BLOCK | (HasWitness ? InventoryType::MSG_WITNESS_FLAG : (InventoryType)0);
			ptr<GetDataMessage> m = new GetDataMessage;
			m->Invs.push_back(Inventory(invType, eng.ChainParams.Genesis));
			Send(m);
		}
	}
}

void Link::RequestBlocks() {
	CoinEng& eng = static_cast<CoinEng&>(*Net);

	bool bFetch = IsPreferredDownload || (0 == eng.aPreferredDownloadPeers.load() && !IsClient && !IsOneShot);

	int count = MAX_BLOCKS_IN_TRANSIT_PER_PEER - (int)EXT_LOCKED(eng.Mtx, BlocksInFlight.size());
	if (IsClient || ((!bFetch || IsLimitedNode) && eng.IsInitialBlockDownload()) || count <= 0 || !HashBlockBestKnown)
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
	TRC(4, "HashBlockLastCommon: " << bti.Height << "/" << HashBlockLastCommon << "     HashBlockBestKnown: " << btiBestKnown.Height << "/" << HashBlockBestKnown);

	int nWindowEnd = bti.Height + BLOCK_DOWNLOAD_WINDOW,
		nMaxHeight = (min)(btiBestKnown.Height, nWindowEnd + 1);

	Link *staller = 0, *waitingFor = 0;
	ptr<GetDataMessage> mGetData;
	while (bti.Height < nMaxHeight) {
		typedef map<int, HashValue> CHeight2Value;
		CHeight2Value height2hash;

		int nToFetch = (min)(nMaxHeight - bti.Height, (max)(128, count - (mGetData ? (int)mGetData->Invs.size() : 0)));
		BlockHeader cur = bti = eng.Tree.GetAncestor(HashBlockBestKnown, bti.Height+nToFetch);
		height2hash.insert(make_pair(cur.Height, Hash(cur)));
		for (int i = 1; i < nToFetch; ++i) {
			height2hash.insert(make_pair(cur.Height - 1, cur.PrevBlockHash));
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
						if (!mGetData)
							mGetData = new GetDataMessage;
						eng.MarkBlockAsInFlight(*mGetData, _self, Inventory(InventoryType::MSG_BLOCK, pp.second));
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
		TRC(4, "Updated HashBlockLastCommon: " << eng.BlockStringId(HashBlockLastCommon));
	}

	if (mGetData) {
		mGetData->Invs.resize(std::min(mGetData->Invs.size(), (size_t)ProtocolParam::MAX_INV_SZ));
		Send(mGetData);
	}
}

void BlockMessage::ProcessContent(Link& link, bool bRequested) {
	Block.Process(&link, bRequested);
}

void BlockMessage::Process(Link& link) {
	CoinEng& eng = Eng();
	const HashValue& hash = Hash(Block);
	bool bRequested = eng.MarkBlockAsReceived(hash);

	EXT_LOCKED(link.Mtx, link.KnownInvertorySet.insert(Inventory(InventoryType::MSG_BLOCK, hash)));
	if (eng.UpgradingDatabaseHeight)
		return;
	ProcessContent(link, bRequested);

	if (eng.Mode != EngMode::Lite || !link.m_curMerkleBlock) { // for LiteMode we wait until txes are downloaded
		if (EXT_LOCKED(eng.Mtx, link.BlocksInFlight.size() <= MAX_BLOCKS_IN_TRANSIT_PER_PEER / 2))
			link.RequestBlocks();
	}

#ifdef X_DEBUG//!!!D
	if (link.HasWitness) {
		if (auto o = eng.Db->FindBlockOffset(hash)) {
			ptr<BlockMessage> m = new BlockMessage(Coin::Block(nullptr));
			m->Offset = o.value().first;
			m->Size = o.value().second;
			m->WitnessAware = true;
			link.Send(m);
		}
	}
#endif
}

void MerkleBlockMessage::ProcessContent(Link& link, bool bRequested) {
	pair<HashValue, vector<HashValue>> pp = PartialMT.ExtractMatches();
	if (pp.first != Block.get_MerkleRoot())
		Throw(CoinErr::MerkleRootMismatch);
	if (pp.second.empty())
		Block.Process(&link, bRequested);
	else {
		link.m_curMerkleBlock = Block;					// Defer processing until all txes will be received
		link.m_curMatchedHashes = pp.second;
	}
}

void MerkleBlockMessage::Process(Link& link) {
	if (Eng().Mode == EngMode::Lite)
		base::Process(link);
}

} // Coin::
