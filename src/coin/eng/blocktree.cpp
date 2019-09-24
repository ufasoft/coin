/*######   Copyright (c) 2018-2019 Ufasoft  http://ufasoft.com  mailto:support@ufasoft.com,  Sergey Pavlov  mailto:dev@ufasoft.com ####
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

void BlockTree::Clear() {
	HeightLastCheckpointed = -1;
	EXT_LOCK(Mtx) {
		Map.clear();
	}
}

BlockTreeItem BlockTree::FindInMap(const HashValue& hashBlock) const {
	return EXT_LOCKED(Mtx, Lookup(Map, hashBlock).value_or(BlockTreeItem()));
}

BlockHeader BlockTree::FindHeader(const HashValue& hashBlock) const {
	if (BlockTreeItem item = FindInMap(hashBlock))
		return item;
	return Eng.Db->FindHeader(hashBlock);
}

BlockHeader BlockTree::FindHeader(const BlockRef& bref) const {
	if (BlockTreeItem item = FindInMap(bref.Hash))
		return item;
	return Eng.Db->FindHeader(bref);
}

BlockTreeItem BlockTree::GetHeader(const HashValue& hashBlock) const {
	if (BlockTreeItem item = FindHeader(hashBlock))
		return item;
	Throw(E_FAIL);
}

BlockTreeItem BlockTree::GetHeader(const BlockRef& bref) const {
	if (BlockTreeItem item = FindHeader(bref))
		return item;
	Throw(E_FAIL);
}

Block BlockTree::FindBlock(const HashValue& hashBlock) const {
	if (BlockTreeItem item = FindInMap(hashBlock)) {
		if (!item->IsHeaderOnly())
			return Block(item.m_pimpl);
	}
	return Eng.LookupBlock(hashBlock);
}

Block BlockTree::GetBlock(const HashValue& hashBlock) const {
	if (Block b = FindBlock(hashBlock))
		return b;
	Throw(E_FAIL);
}

BlockHeader BlockTree::GetAncestor(const HashValue& hashBlock, int height) const {
	BlockTreeItem item = GetHeader(hashBlock);
	if (height == item.Height)
		return item;
	if (height > item.Height)
		return BlockHeader(nullptr);
	EXT_LOCK(Mtx) {
		while (height < item.Height - 1) {
			if (auto o = Lookup(Map, item.PrevBlockHash))
				item = o.value();
			else
				goto LAB_TRY_MAIN_CHAIN;
		}
	}
	return GetHeader(item.PrevBlockHash);
LAB_TRY_MAIN_CHAIN:
	return Eng.Db->FindHeader(height);
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
			if (pp.second.PrevBlockHash == hashBlock && !pp.second->IsHeaderOnly())
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


} // Coin::
