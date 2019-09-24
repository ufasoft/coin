/*######   Copyright (c) 2014-2015 Ufasoft  http://ufasoft.com  mailto:support@ufasoft.com,  Sergey Pavlov  mailto:dev@ufasoft.com ####
#                                                                                                                                     #
# 		See LICENSE for licensing information                                                                                         #
#####################################################################################################################################*/

#include <el/ext.h>

#include "filet.h"

namespace Ext { namespace DB { namespace KV {

uint64_t Filet::GetPagesForLength(uint64_t len) const {
	const uint32_t pageSize = m_tx.Storage.PageSize;
	return (len + pageSize - 1) >> PageSizeBits;
}

uint32_t Filet::GetPgNo(Page& page, int idx) const {
	return letoh(((uint32_t*)page.get_Address())[idx]);
}

void Filet::PutPgNo(Page& page, int idx, uint32_t pgno) const {
	ASSERT(page.Dirty);
	((uint32_t*)page.get_Address())[idx] = htole(pgno);
}

Page Filet::FindPath(uint64_t offset, PathVisitor& visitor) const {
	Page page = PageRoot, pagePrev;
	uint32_t pgno = 0;
	int prevIdx = 0;
	for (int levels=IndirectLevels, level=levels; ;) {
		--level;
		int bits = PageSizeBits + level*(PageSizeBits-2);
		if (!visitor.OnPathLevel(levels - level-1, page))
			return nullptr;
		if (pagePrev && page.N != pgno)
			PutPgNo(pagePrev, prevIdx, page.N);
		if (level == -1)
			break;
		pagePrev = exchange(page, (pgno = GetPgNo(page, prevIdx = int(offset >> bits))) ? m_tx.OpenPage(pgno) : nullptr);
		offset &= (1LL<<bits) - 1;
	}
	return page;
}

uint8_t *Filet::FindPathFlat(uint64_t offset, PathVisitor& visitor) const {
	uint32_t pn = PageRoot.N, pnPrev = 0;
	uint32_t pgno = 0;
	int prevIdx = 0;
	for (int levels=IndirectLevels, level=levels; ;) {
		--level;
		int bits = PageSizeBits + level*(PageSizeBits-2);
		if (!visitor.OnPathLevelFlat(levels - level-1, pn))
			return nullptr;
		if (pnPrev && pn != pgno) {
			Throw(E_NOTIMPL);		//!!!TODO
		}
		if (level == -1)
			break;
		pnPrev = pn;
		int idx = int (offset >> bits);
		prevIdx = idx;
		pgno = GetPgNo(pn, idx);
		pn = pgno;
		offset &= (1LL<<bits) - 1;
	}
	return pn ? (uint8_t*)m_tx.Storage.ViewAddress + uint64_t(pn) * m_tx.Storage.PageSize : nullptr;
}

bool Filet::TouchPage(Page& page) {
	if (page.Dirty)
		return false;
	page = dynamic_cast<DbTransaction&>(m_tx).Allocate(PageAlloc::Move, &page);
	return true;
}

void Filet::TouchPage(Page& pageData, PagedPath& path) {
	TouchPage(pageData);
	uint32_t pgno = pageData.N;
	bool bDirty = false;
	for (size_t level=path.size(); !bDirty && level--;) {
		Page& page = path[level].first;
		bDirty = page.Dirty;
		TouchPage(page);
		if (level == 0)
			SetRoot(page);
		PutPgNo(page, path[level].second, exchange(pgno, page.N));
	}
}

Page Filet::GetPageToModify(uint64_t offset, bool bOptional) {
#if UCFG_DB_TLB
	ClearTlbs();							//!!!?
#endif
	struct PutVisitor : PathVisitor {
		Filet *pFilet;
		bool Optional;

		bool OnPathLevel(int level, Page& page) override {
			if (!page) {
				if (Optional)
					return false;
				page = dynamic_cast<DbTransaction&>(pFilet->m_tx).Allocate(PageAlloc::Zero);
				if (0 == level)
					pFilet->SetRoot(page);
			} else if (pFilet->TouchPage(page) && 0==level)
				pFilet->SetRoot(page);
			return true;
		}
	} visitor;
	visitor.Optional = bOptional;
	visitor.pFilet = this;
	return FindPath(offset, visitor);
}

uint32_t Filet::RemoveRange(int level, Page& page, uint32_t first, uint32_t last) {
	int bits = level*(PageSizeBits-2);
	uint32_t from = first >> bits,
			to = last >> bits;
	bool bHasData = false;
	uint32_t *p = (uint32_t*)page.get_Address();
	DbTransaction& tx = dynamic_cast<DbTransaction&>(m_tx);
	for (int i=0, n=int(tx.Storage.PageSize/4); i<n; ++i) {
		if (uint32_t pgno = letoh(p[i])) {
			if (i>=from && i<=to) {
				if (0 == level) {
					tx.FreePage(exchange(pgno, 0));
				} else {
					Page subPage = tx.OpenPage(pgno);
					uint32_t offset = (i << bits);
					if (pgno = RemoveRange(level-1, subPage, first-offset, last-offset)) {
						if (TouchPage(page))
							p = (uint32_t*)page.get_Address();
						*p = htole(pgno);
					}
				}
			}
			bHasData |= bool(pgno);
		}
	}
	if (bHasData)
		return page.N;
	tx.FreePage(page);
	page = nullptr;
	return 0;
}

void Filet::SetLength(uint64_t v) {
	m_length = v;
	IndirectLevels = !m_length ? 0 : max(0, BitOps::ScanReverse(uint32_t((Length - 1) >> PageSizeBits)) - 1) / (PageSizeBits - 2) + 1;
}

void Filet::put_Length(uint64_t v) {
	uint64_t prevLen = Length;
	int levels = IndirectLevels;

	uint64_t nPages = GetPagesForLength(v),
		nHasPages = GetPagesForLength(Length);
	if (nPages < nHasPages) {
		RemoveRange(levels, PageRoot, uint32_t(nPages), uint32_t(nHasPages - 1));
	}
	SetLength(v);
	int levelsNew = IndirectLevels;
	DbTransaction& tx = dynamic_cast<DbTransaction&>(m_tx);
	for (; levelsNew>levels; --levelsNew) {
		Page newRoot = tx.Allocate(PageAlloc::Zero);
		if (PageRoot)
			PutPgNo(newRoot, 0, PageRoot.N);
		SetRoot(newRoot);
	}
	for (; levelsNew<levels; ++levelsNew) {
		uint32_t pgno = PageRoot ? GetPgNo(PageRoot, 0) : 0;
		if (PageRoot)
			tx.FreePage(PageRoot);
		SetRoot(pgno ? tx.OpenPage(pgno) : nullptr);
	}

	if (v > prevLen) {
		if (Page page = GetPageToModify(prevLen, true)) {
			size_t off = size_t(prevLen & (tx.Storage.PageSize-1));
			memset((uint8_t*)page.get_Address() + off, 0, m_tx.Storage.PageSize - off);
		}
	}
}

uint32_t Filet::GetUInt32(uint64_t offset) const {
	KVStorage& storage = m_tx.Storage;
	if (offset + sizeof(uint32_t) > Length)
		Throw(errc::invalid_argument);
	if (offset & 3)
		Throw(errc::invalid_argument);
	uint32_t vpage = uint32_t(offset >> PageSizeBits);
	size_t off = size_t(offset & (storage.PageSize - 1));

	if (storage.m_accessViewMode == ViewMode::Full) {
#if UCFG_DB_TLB
		CTlbAddress::iterator it = TlbAddress.find(vpage);
		if (it != TlbAddress.end())
			return it->second.first ? letoh(*(uint32_t*)(it->second.first + off)) : 0;
#endif

		struct GetPathOffVisitor : PathVisitor {
			bool OnPathLevelFlat(int level, uint32_t& pgno) override {
				return bool(pgno);
			}
		} visitor;
		uint8_t *p = FindPathFlat(offset, visitor);
#if UCFG_DB_TLB
		TlbAddress.insert(make_pair(vpage, p));
#endif
		return p ? letoh(*(uint32_t*)(p + off)) : 0;
	} else {
#if UCFG_DB_TLB
		CTlbPage::iterator it = TlbPage.find(vpage);
		if (it != TlbPage.end())
			return it->second.first ? letoh(*(uint32_t*)((uint8_t*)it->second.first.get_Address() + off)) : 0;
#endif

#if 1
		uint32_t pgno = PageRoot.N;
		void *pageAddress = PageRoot.get_Address();
		ptr<ViewBase> view;
		EXT_LOCK(g_lruViewCache->Mtx) {
			EXT_LOCK(storage.MtxViews) {
				for (int levels = IndirectLevels, level = levels;;) {
					--level;
					int bits = PageSizeBits + level * (PageSizeBits - 2);
					storage.OnOpenPage(pgno);
					if (level == -1)
						return letoh(*(uint32_t*)((uint8_t*)pageAddress + off));
					if (!(pgno = letoh(((uint32_t*)pageAddress)[int(offset >> bits)])))
						return 0;
					PageObj *po = storage.OpenedPages.at(pgno);
					pageAddress = po ? po->GetAddress()
						: storage.GetPageAddress(storage.GetViewNoLock(pgno >> storage.m_bitsViewPageRatio), pgno);
					offset &= (1LL<<bits) - 1;
				}
			}
		}
#else //!!!T

		struct GetPathOffVisitor : PathVisitor {
			bool OnPathLevel(int level, Page& page) override {
				return bool(page);
			}
		} visitor;
		Page page = FindPath(offset, visitor);
#if UCFG_DB_TLB
		TlbPage.insert(make_pair(vpage, page));
#endif
		uint32_t r = page ? letoh(*(uint32_t*)((uint8_t*)page.get_Address() + off)) : 0;
		return r;
#endif
	}
}

void Filet::PutUInt32(uint64_t offset, uint32_t v) {
	if (offset + sizeof(uint32_t) > Length)
		Length = offset + sizeof(uint32_t);
	if (offset & 3)
		Throw(errc::invalid_argument);
	*(uint32_t*)((uint8_t*)GetPageToModify(offset, false).get_Address() + size_t(offset & (m_tx.Storage.PageSize - 1))) = htole(v);
}


}}} // Ext::DB::KV::
