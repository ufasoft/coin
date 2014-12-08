#include <el/ext.h>

#include "filet.h"

namespace Ext { namespace DB { namespace KV {

int Filet::IndirectLevels() const {
	if (Length == 0)
		return 0;
	int bits = max(0, BitOps::ScanReverse(UInt32((Length-1) >> PageSizeBits))-1);
	return bits/(PageSizeBits-2) + 1;
}

UInt64 Filet::GetPagesForLength(UInt64 len) const {
	const size_t pageSize = m_tx.Storage.PageSize;
	return (len + pageSize - 1) >> PageSizeBits;
}

UInt32 Filet::GetPgNo(Page& page, int idx) const {
	return letoh(((UInt32*)page.get_Address())[idx]);
}

void Filet::PutPgNo(Page& page, int idx, UInt32 pgno) const {
	ASSERT(page.Dirty);
	((UInt32*)page.get_Address())[idx] = htole(pgno);
}

Page Filet::FindPath(UInt64 offset, PathVisitor& visitor) const {
	Page page = PageRoot, pagePrev;
	UInt32 pgno = 0;
	int prevIdx = 0;
	for (int levels=IndirectLevels(), level=levels; ;) {
		--level;
		int bits = PageSizeBits + level*(PageSizeBits-2);
		if (!visitor.OnPathLevel(levels - level-1, page))
			return nullptr;
		if (pagePrev && page.N != pgno)
			PutPgNo(pagePrev, prevIdx, page.N);
		if (level == -1)
			break;
		pagePrev = page;
		int idx = int (offset >> bits);
		prevIdx = idx;
		pgno = GetPgNo(page, idx);
		page = pgno ? m_tx.OpenPage(pgno) : nullptr;
		offset &= (1LL<<bits) - 1;
	}
	return page;
}

byte *Filet::FindPathFlat(UInt64 offset, PathVisitor& visitor) const {
	UInt32 pn = PageRoot.N, pnPrev = 0;
	UInt32 pgno = 0;
	int prevIdx = 0;
	for (int levels=IndirectLevels(), level=levels; ;) {
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
	return pn ? (byte*)m_tx.Storage.ViewAddress + UInt64(pn) * m_tx.Storage.PageSize : nullptr;
}

bool Filet::TouchPage(Page& page) {
	if (page.Dirty)
		return false;
	page = m_tx.Allocate(PageAlloc::Move, &page);
	return true;
}

void Filet::TouchPage(Page& pageData, PagedPath& path) {
	TouchPage(pageData);
	UInt32 pgno = pageData.N;
	bool bDirty = false;
	for (size_t level=path.size(); !bDirty && level--;) {
		Page& page = path[level].first;
		bDirty = page.Dirty;
		TouchPage(page);
		if (level == 0)
			PageRoot = page;
		PutPgNo(page, path[level].second, exchange(pgno, page.N));
	}
}

Page Filet::GetPageToModify(UInt64 offset, bool bOptional) {
	struct PutVisitor : PathVisitor {
		Filet *pFilet;
		bool Optional;

		bool OnPathLevel(int level, Page& page) override {
			if (!page) {
				if (Optional)
					return false;
				page = pFilet->m_tx.Allocate(PageAlloc::Zero);
				if (0 == level)
					pFilet->PageRoot = page;
			} else if (pFilet->TouchPage(page) && 0==level)
				pFilet->PageRoot = page;
			return true;
		}
	} visitor;
	visitor.Optional = bOptional;
	visitor.pFilet = this;
	return FindPath(offset, visitor);
}

UInt32 Filet::RemoveRange(int level, Page& page, UInt32 first, UInt32 last) {
	int bits = level*(PageSizeBits-2);
	UInt32 from = first >> bits,
			to = last >> bits;
	bool bHasData = false;
	UInt32 *p = (UInt32*)page.get_Address();
	for (int i=0, n=int(m_tx.Storage.PageSize/4); i<n; ++i) {
		if (UInt32 pgno = letoh(p[i])) {
			if (i>=from && i<=to) {
				if (0 == level) {
					m_tx.FreePage(exchange(pgno, 0));				
				} else {
					Page subPage = m_tx.OpenPage(pgno);
					UInt32 offset = (i << bits);
					if (pgno = RemoveRange(level-1, subPage, first-offset, last-offset)) {
						if (TouchPage(page))
							p = (UInt32*)page.get_Address();
						*p = htole(pgno);
					}
				}
			}
			bHasData |= bool(pgno);
		}
	}
	if (bHasData)
		return page.N;
	m_tx.FreePage(page);
	page = nullptr;
	return 0;
}

void Filet::put_Length(UInt64 v) {
	UInt64 prevLen = Length;
	int levels = IndirectLevels();

	UInt64 nPages = GetPagesForLength(v),
		nHasPages = GetPagesForLength(Length);
	if (nPages < nHasPages) {
		RemoveRange(levels, PageRoot, UInt32(nPages), UInt32(nHasPages-1));
	}
	m_length = v;
	int levelsNew=IndirectLevels();
	for (; levelsNew>levels; --levelsNew) {
		Page newRoot = m_tx.Allocate(PageAlloc::Zero);
		if (PageRoot)
			PutPgNo(newRoot, 0, PageRoot.N);
		PageRoot = newRoot;
	}
	for (; levelsNew<levels; ++levelsNew) {
		UInt32 pgno = PageRoot ? GetPgNo(PageRoot, 0) : 0;
		if (PageRoot)
			m_tx.FreePage(PageRoot);
		PageRoot = pgno ? m_tx.OpenPage(pgno) : nullptr;
	}

	if (v > prevLen) {
		if (Page page = GetPageToModify(prevLen, true)) {
			size_t off = size_t(prevLen & (m_tx.Storage.PageSize-1));
			memset((byte*)page.get_Address() + off, 0, m_tx.Storage.PageSize-off);
		}
	}
}

UInt32 Filet::GetUInt32(UInt64 offset) const {
	if (offset+sizeof(UInt32) > Length)
		Throw(E_INVALIDARG);
	if (offset & 3)
		Throw(E_INVALIDARG);
	if (m_tx.Storage.m_viewMode == KVStorage::ViewMode::Full) {
		struct GetPathOffVisitor : PathVisitor {
			bool OnPathLevelFlat(int level, UInt32& pgno) override {
				return bool(pgno);
			}
		} visitor;
		byte *p = FindPathFlat(offset, visitor);
		return p ? letoh(*(UInt32*)(p + size_t(offset & (m_tx.Storage.PageSize-1)))) : 0;
	} else {
		struct GetPathOffVisitor : PathVisitor {
			bool OnPathLevel(int level, Page& page) override {
				return bool(page);
			}
		} visitor;
		Page page = FindPath(offset, visitor);
		UInt32 r = page ? letoh(*(UInt32*)((byte*)page.get_Address() + size_t(offset & (m_tx.Storage.PageSize-1)))) : 0;
		return r;
	}
}

void Filet::PutUInt32(UInt64 offset, UInt32 v) {
	if (offset+sizeof(UInt32) > Length)
		Length = offset+sizeof(UInt32);
	if (offset & 3)
		Throw(E_INVALIDARG);
	*(UInt32*)((byte*)GetPageToModify(offset, false).get_Address() + size_t(offset & (m_tx.Storage.PageSize-1))) = htole(v);
}


}}} // Ext::DB::KV::


