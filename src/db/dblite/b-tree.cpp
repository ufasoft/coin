/*######   Copyright (c) 2014-2019 Ufasoft  http://ufasoft.com  mailto:support@ufasoft.com,  Sergey Pavlov  mailto:dev@ufasoft.com ####
#                                                                                                                                     #
# 		See LICENSE for licensing information                                                                                         #
#####################################################################################################################################*/

#include <el/ext.h>

#include "dblite.h"
#include "b-tree.h"

namespace Ext { namespace DB { namespace KV {

int NumKeys(const Page& page) {
	return page.Header().Num;
}

size_t CalculateLocalDataSize(uint64_t dataSize, size_t cbExtendedPrefix, uint32_t pageSize) {
	size_t cbMaxData = pageSize - 3 - cbExtendedPrefix;
	return dataSize <= cbMaxData || (dataSize %= pageSize - 4) + 4 <= cbMaxData
		? (size_t)dataSize
		: 0;
}

LiteEntry *BuildLiteEntryIndex(PageHeader& h, void *raw, int n, uint32_t pageSize, uint8_t keySize) {
	LiteEntry *entries = (LiteEntry*)raw;
	uint8_t *p = h.Data;
	uint8_t keyOffset = h.KeyOffset();
	if (h.Flags & PageHeader::FLAG_BRANCH) {
		p += 4;
		for (int i = 0; i < n; ++i) {
			entries[i].P = p;
			p += (keySize ? keySize - keyOffset : 1 + p[0]) + 4;
		}
	} else {
		for (LiteEntry *entry = entries, *end = entries + n; entry != end; ++entry) {
			entry->P = p;
			pair<Span, uint64_t> qq = entry->LocalData(pageSize, keySize, keyOffset);
			size_t cbLocal = qq.first.size();
			p = (uint8_t*)qq.first.data() + cbLocal + (cbLocal != qq.second ? 4 : 0);
		}
	}
	entries[n].P = p;
	ASSERT(p - (uint8_t*)&h <= pageSize);
	return entries;
}

PageDesc PageObj::Entries(uint8_t keySize) {
#ifdef X_DEBUG//!!!D
	if (Entries) {
		PageHeader& hh = Header();
		LiteEntry *le;
		BuildLiteEntryIndex(hh, le = (LiteEntry*)alloca(sizeof(LiteEntry) * (hh.Num+1)), hh.Num);
		if (memcmp(le, Entries, sizeof(LiteEntry) * (hh.Num+1)))
			cout << "Error";
	}
#endif

	PageHeader& h = Header();
	if (!aEntries.load()) {
		LiteEntry* p = BuildLiteEntryIndex(h, Malloc(sizeof(LiteEntry) * (h.Num + 1)), h.Num, View->Storage.PageSize, keySize);
		for (LiteEntry* prev = 0; !aEntries.compare_exchange_weak(prev, p);)
			if (prev) {
				free(p);
				break;
			}
	}
	return PageDesc(h, aEntries);
}

#ifdef X_DEBUG//!!!D

void CheckPage(Page& page) {
	PageHeader& h = page.Header();
	LiteEntry *entries = BuildLiteEntryIndex(h, (LiteEntry*)alloca(sizeof(LiteEntry) * (h.Num+1)), h.Num);
	for (int i=0; i<h.Num; ++i) {
		ASSERT(entries[i].PgNo() < 100000);
	}
}

#endif

size_t Page::SizeLeft(uint8_t keySize) const {
	PageDesc pd = Entries(keySize);
	return m_pimpl->View->Storage.PageSize - (pd[pd.Header.Num].P - (uint8_t*)&pd.Header);
}

LiteEntry GetLiteEntry(const PagePos& pp, uint8_t keySize) {
	PageDesc pd = pp.Page.Entries(keySize);
	ASSERT(pp.Pos <= pd.Header.Num);

	return pd[pp.Pos];
}

size_t GetEntrySize(const EntrySize& es, size_t ksize, uint64_t dsize) {
	uint8_t buf[10], *p = buf;
	Write7BitEncoded(p, dsize);
	return es.Size + ksize + (p - buf) + (es.IsBigData ? 4 : 0);
}

int Page::FillPercent(uint8_t keySize) {
	PageDesc pd = Entries(keySize);
	int pageSize = m_pimpl->View->Storage.PageSize;
	return pd.Header.Num ? int(pageSize - (pd[pd.Header.Num].P - pd.Header.Data)) * 100 / pageSize : 0;
}

bool Page::IsUnderflowed(uint8_t keySize) {
	return FillPercent(keySize) < FILL_THRESHOLD_PERCENTS;
}

void InsertCell(const PagePos& pagePos, RCSpan cell, uint8_t keySize) {
	Page page = pagePos.Page;
	PageObj& po = *page.m_pimpl;
	if (po.Overflows || page.SizeLeft(keySize) < cell.size()) {
		ASSERT(po.Overflows < size(po.OverflowCells));
		po.OverflowCells[po.Overflows++] = IndexedBuf(cell.data(), (uint16_t)cell.size(), (uint16_t)pagePos.Pos);
	} else {
		PageDesc pd = page.Entries(keySize);
		bool bBranch = pd.Header.Flags & PageHeader::FLAG_BRANCH;
		int off = bBranch ? 4 : 0;
		LiteEntry& e = pd[pagePos.Pos];
		memmove(e.P + cell.size() - off, e.P - off, pd[pd.Header.Num++].P - e.P + off);
		memcpy(e.P - off, cell.data(), cell.size());
	}
	page.ClearEntries();
}

// Keeps Entry Index valid on the same memory place
// Returns first overflow page or 0
uint32_t DeleteEntry(const PagePos& pp, uint8_t keySize) {
	PageDesc pd = pp.Page.Entries(keySize);
	ASSERT(pp.Pos < pd.Header.Num);
	LiteEntry& e = pd[pp.Pos];
	bool bBranch = pp.Page.IsBranch;
	int r = DB_EOF_PGNO;
	if (!bBranch) {
		pair<Span, uint64_t> qq = e.LocalData(pp.Page->View->Storage.PageSize, keySize, pd.Header.KeyOffset());
		if (qq.first.size() != qq.second)
			r = e.FirstBigdataPage();
	}
	ssize_t off = e.Size();
	uint8_t* p = e.P - (int(bBranch) * 4);
	memmove(p, p + off, pd[pd.Header.Num--].P - p - off);
	for (int i = pp.Pos + 1, numKeys = pd.Header.Num; i <= numKeys; ++i)
		pd[i].P = pd[i + 1].P - off;
	return r;
}

// Returns pointer to the RightPtr of the Branch Page
static uint8_t *AssemblePage(Page& page, const Span cells[], uint16_t n) {
	ASSERT(!page.Overflows);

	PageHeader& h = page.Header();
	h.Num = htole(n);
	uint8_t *p = h.Data;
	for (int i=0; i<n; ++i) {
		RCSpan cell = cells[i];
		memcpy(exchange(p, p+cell.size()), cell.data(), cell.size());
	}
	return p;
}

void BTree::SetRoot(const Page& page) {
	Root = page;
	Dirty = true;
	if (Name == DbTable::Main().Name)
		Tx.MainTableRoot = page;
}

// second arg not used, but present to ensure it exists in memory when reopened by OpenPage()
void BTree::BalanceNonRoot(PagePos& ppParent, Page&, uint8_t *tmpPage) {
	DbTransaction& tx = dynamic_cast<DbTransaction&>(Tx);
	KVStorage& stg = tx.Storage;
	const uint32_t pageSize = stg.PageSize;
	Page pageParent = ppParent.Page;

	PageDesc pdParent = pageParent.Entries(KeySize);
	int n = pdParent.Header.Num;
	int nxDiv = std::max(0, std::min(ppParent.Pos - 1, n - 2 + int(tx.Bulk)));
	n = std::min(2 - int(tx.Bulk), n);
	vector<Page> oldPages(n+1);
	vector<Span> dividers(n);
	uint8_t *tmp = tmpPage;
	int nEstimatedCells = 0;
	for (int i = oldPages.size() - 1; i >= 0; --i) {
		LiteEntry& e = pdParent[nxDiv + i];
		Page& page = oldPages[i] = tx.OpenPage(e.PgNo());
		nEstimatedCells += NumKeys(page) + page.Overflows + 1;
		if (i != oldPages.size() - 1) {
			size_t cbEntry = e.Size();
			memcpy(tmp, e.P - 4, cbEntry);
			dividers[i] = Span(tmp, cbEntry);
			tmp += cbEntry;
			DeleteEntry(PagePos(pageParent, nxDiv + i), KeySize);
		}
	}
	bool bBranch = oldPages.front().IsBranch;

	struct PageInfo {
		size_t UsedSize;
		int Count;
		Ext::DB::KV::Page Page;

		PageInfo(size_t usedSize = 0, int count = 0)
			:	UsedSize(usedSize)
			,	Count(count)
		{}
	};
	AlignedMem memTmp((oldPages.size()+1)*pageSize, 64);
	vector<Span> cells;
	cells.reserve(nEstimatedCells);
	uint8_t *pTemp = (uint8_t*)memTmp.get()+oldPages.size()*pageSize;
	uint32_t pgnoRight = 0;
	uint8_t keyOffset = pageParent.Header().KeyOffset();
	for (int i=0; i<oldPages.size(); ++i) {
		Page& page = oldPages[i];
		PageDesc pd = page.Entries(KeySize);
		PageObj& po = *page.m_pimpl;
		ASSERT(po.Dirty || !po.Overflows);

		if (po.Dirty) {
			PageHeader& hd = *(PageHeader*)((uint8_t*)memTmp.get()+i*stg.PageSize);
			MemcpyAligned32(&hd, &pd.Header, pd[pd.Header.Num].P - (uint8_t*)&pd.Header);
			ssize_t off = (uint8_t*)&hd - (uint8_t*)&pd.Header;
			for (int i = 0, n = pd.Header.Num; i <= n; ++i)
				pd[i].P += off;
		}
		for (int k = 0, n = NumKeys(page) + po.Overflows, idx; (idx = k) < n; ++k) {
			Span cell;
			for (int j = int(po.Overflows) - 1; j >= 0; --j) {
				IndexedBuf& ib = po.OverflowCells[j];
				if (idx == ib.Index) {
					cell = Span(ib.P, ib.Size);
					goto LAB_FOUND;
				} else if (idx > ib.Index)
					--idx;
			}
			{
				LiteEntry& e = pd[idx];
				cell = Span(e.P - int(bBranch) * 4, e.Size());
			}
LAB_FOUND:
			cells.push_back(cell);
		}
		po.Overflows = 0;
		if (bBranch) {
			uint32_t pgno = pd[pd.Header.Num].PgNo();
			if (i == oldPages.size()-1)
				pgnoRight = pgno;
			else {
				PutLeUInt32(pTemp, pgno);
				Span& div = dividers[i];
				memcpy(pTemp + 4, div.data() + 4, div.size() - 4);
				cells.push_back(Span(pTemp, div.size()));
				pTemp += div.size();
			}
		}
		if (po.Dirty)
			page.ClearEntries();
	}
	ASSERT(!cells.empty());

	vector<PageInfo> newPages;
	const size_t cbUsable = stg.PageSize-3-int(bBranch)*4;
	size_t cbPage = 0;
	int iCell = 0;
	for (; iCell<cells.size(); ++iCell) {
		size_t cb = cells[iCell].size();
		if (cbPage+cb > cbUsable) {
			newPages.push_back(PageInfo(exchange(cbPage, 0), iCell));
			iCell -= (int)(!bBranch);
		} else
			cbPage += cb;
	}
	newPages.push_back(PageInfo(exchange(cbPage, 0), iCell));

	for (int i=newPages.size()-1; i>0; --i) {
		size_t &cbRight = newPages[i].UsedSize,
			&cbLeft = newPages[i-1].UsedSize;
		int& cnt = newPages[i-1].Count;
		while (true) {
			int r = cnt - 1,
				d = r + 1 - (bBranch ? 0 : 1);
			if (cbRight!=0 && (tx.Bulk || cbRight+cells[d].size() > cbLeft-cells[r].size()))
				break;
			cbRight += cells[d].size();
			cbLeft -= cells[r].size();
			--cnt;
		}
	}

	for (int i=0; i<newPages.size(); ++i) {
		for (vector<Page>::iterator it=oldPages.begin(); it!=oldPages.end(); ++it) {
			if (it->Dirty) {
				ASSERT(!it->m_pimpl->aEntries.load());

				newPages[i].Page = *it;
				oldPages.erase(it);
				break;
			}
		}
		if (!newPages[i].Page) {
			newPages[i].Page = tx.Allocate(bBranch ? PageAlloc::Branch : PageAlloc::Leaf);
			newPages[i].Page.Header().SetKeyOffset(keyOffset);
		}
	}

	//!!!TODO Sort pgnos

	pdParent[nxDiv].PutPgNo(newPages.back().Page.N);

	int j = 0;
	tmp = tmpPage;
	for (int i=0; i<newPages.size(); ++i) {
		Page& page = newPages[i].Page;
		ASSERT(!page.Overflows);

		int nj = newPages[i].Count;
		uint8_t *pRight = AssemblePage(page, &cells[j], uint16_t(nj-j));
		j = nj;

		ASSERT(i<newPages.size()-1 || j==cells.size());
		if (j < cells.size()) {
			Span div = cells[j];
			if (bBranch) {
				memcpy(pRight, div.data(), 4);
				++j;
			} else {
                div = Span(div.data() - 4, 4 + (KeySize ? KeySize - keyOffset : 1 + div[0]));
			}
			PutLeUInt32(tmp, page.N);
			memcpy(tmp + 4, div.data() + 4, div.size() - 4);
			InsertCell(PagePos(pageParent, nxDiv++), Span(tmp, div.size()), KeySize);
			tmp += div.size();
		}
		if (bBranch && i==newPages.size()-1)
			PutLeUInt32(pRight, pgnoRight);
	}

	for (int i=0; i<oldPages.size(); ++i)						// non-Dirty oldPages contain used Data, so free them only after copying to newPages
		tx.FreePage(oldPages[i]);

	if (pageParent==Root && 0==NumKeys(pageParent)) {
		ASSERT(newPages.size() == 1);
		tx.FreePage(Root);
		SetRoot(newPages[0].Page);
	}
}

void BTreeCursor::Balance() {
	if (Path.empty())
		return;
	Page& page = Top().Page;
	if (Path.size() == 1) {
		if (!page.Overflows)
			return;
		Tree->SetRoot(dynamic_cast<DbTransaction&>(Tree->Tx).Allocate(PageAlloc::Branch));
		Tree->Root.Header().SetKeyOffset(page.Header().KeyOffset());
		PutLeUInt32(Tree->Root.Header().Data, page.N);
		Path.insert(Path.begin(), PagePos(Tree->Root, 0));
	} else if (!page.Overflows && !page.IsUnderflowed(Tree->KeySize))
		return;
	Initialized = false;
	Blob blobDivs(0, Tree->Tx.Storage.PageSize);										// should be in dynamic scope during one nested call of Balance()
	Tree->BalanceNonRoot(Path[Path.size()-2], Path.back().Page, blobDivs.data());
	Path.back().Page->Overflows = 0;
	Path.pop_back();
	Balance();
}

void BTreeCursor::Delete() {
	base::Delete();
	Tree->Tx.m_bError = true;
	Balance();
	Tree->Tx.m_bError = false;
}

int PagedMap::Compare(const void *p1, const void *p2, size_t cb2) {
	size_t cb1 = ((const uint8_t*)p1)[-1];
	int r = memcmp(p1, p2, std::min(cb1, cb2));
	return r
		? r
		: (cb1 < cb2 ? -1 : cb1 == cb2 ? 0 : 1);
}

EntrySize PagedMap::GetDataEntrySize(RCSpan k, uint64_t dsize) const {
	uint32_t pageSize = Tx.Storage.PageSize;
	uint8_t buf[10], *p = buf;
	Write7BitEncoded(p, dsize);
	size_t cbExtendedPrefix = (KeySize ? KeySize : 1 + k.size()) + (p - buf);
	return EntrySize(CalculateLocalDataSize(dsize, cbExtendedPrefix, pageSize), cbExtendedPrefix + dsize > pageSize - 3); //!!!?
}

pair<int, bool> PagedMap::EntrySearch(const PageDesc& pd, RCSpan k) {
	Span kk = k.subspan(pd.Header.KeyOffset());
	int rc = 0;
	int b = 0;
	for (int count = pd.Header.Num, m, half; count;) {
		Span nk = pd[m = b + (half = count / 2)].Key(KeySize);
		if (!(rc = m_pfnCompare(nk.data(), kk.data(), kk.size())))
			return make_pair(m, true);
		else if (rc < 0) {
			b = m + 1;
			count -= half + 1;
		} else
			count = half;
	}
	return make_pair(b, false);
}

EntryDesc PagedMap::GetEntryDesc(const PagePos& pp) {
	PageDesc pd = pp.Page.Entries(KeySize);
	ASSERT(!(pd.Header.Flags & PageHeader::FLAG_BRANCH) && pp.Pos < pd.Header.Num);
	LiteEntry& e = pd[pp.Pos];

	EntryDesc r;
	r.P = e.P;
	r.Size = e.Size();
	uint32_t pageSize = pp.Page->View->Storage.PageSize;
	pair<Span, uint64_t> qq = e.LocalData(pageSize, KeySize, pd.Header.KeyOffset());
	r.LocalData = qq.first;
	r.DataSize = qq.second;
	r.PgNo = (r.Overflowed = r.DataSize >= pageSize || r.LocalData.data() + r.DataSize != e.Upper()) ? e.FirstBigdataPage() : 0;
	return r;
}

void CursorObj::DeepFreePage(const Page& page) {
	DbTransaction& tx = dynamic_cast<DbTransaction&>(Map->Tx);
	if (page.IsBranch) {
		PageDesc pd = page.Entries(Map->KeySize);
		for (int i = 0, n = pd.Header.Num; i <= n; ++i)
			DeepFreePage(tx.OpenPage(pd[i].PgNo()));
	} else {
		for (int i = 0, n = NumKeys(page); i < n; ++i)
			FreeBigdataPages(Map->GetEntryDesc(PagePos(page, i)).PgNo);
	}
	tx.FreePage(page);
}

#pragma intrinsic(memcmp)


BTreeCursor::BTreeCursor()
	:	Path(4)
{
}

int BTreeCursor::CompareEntry(LiteEntry *entries, int idx, RCSpan kk) {
	int rc = Tree->m_pfnCompare(entries[idx].Key(Tree->KeySize).data(), kk.data(), kk.size());
	if (!rc)
		ReturnFromSeekKey(idx);
	return rc;
}

bool BTreeCursor::SeekToKey(RCSpan k) {
	uint8_t keySize = Tree->KeySize;
	if (keySize && keySize != k.size())
		Throw(errc::invalid_argument);
	ASSERT(k.size() > 0 && k.size() <= KVStorage::MAX_KEY_SIZE);

	if (Initialized) {
		Page& page = Top().Page;
		PageHeader& h = page.Header();
		if (0 == h.Num) {
			Top().Pos = 0;
			return false;
		}
		Span kk(k.data() + h.KeyOffset(), k.size() - h.KeyOffset());
		PageDesc pd = page.Entries(keySize);
		int rc = CompareEntry(pd.Entries, 0, kk);
		if (!rc)
			return true;
		if (rc < 0) {
			if (h.Num > 1) {
				if (!(rc = CompareEntry(pd.Entries, h.Num-1, kk)))
					return true;
				if (rc > 0) {
					if (Top().Pos<h.Num && !CompareEntry(pd.Entries, Top().Pos, kk))
						return true;
					goto LAB_ENTRY_SEARCH;
				}
			}
			for (int i=0; i<Path.size()-1; ++i)
				if (Path[i].Pos < NumKeys(Path[i].Page)-1)
					goto LAB_SOME_PARENT_HAVE_RIGHT_SIB;
			Top().Pos = h.Num;
			return false;
		}
LAB_SOME_PARENT_HAVE_RIGHT_SIB:
		if (Path.size() < 2) {
			Top().Pos = 0;
			return false;
		}
	}

	if (!PageSearch(k))
		return false;
LAB_ENTRY_SEARCH:
	Page& page = Top().Page;
	ASSERT(!page.IsBranch);

	pair<int, bool> pp = Tree->EntrySearch(page.Entries(keySize), k);	// bBranch=false
	Top().Pos = pp.first;
	if (!pp.second)
		return false;
	Eof = false;

#ifdef X_DEBUG//!!!D
	ASSERT(Top().Pos < NumKeys(Top().Page));
#endif
	return Initialized = ClearKeyData();
}

bool BTreeCursor::SeekToSibling(bool bToRight) {
	if (Path.size() <= 1)
		return false;
	Path.pop_back();
	PagePos& pp = Top();
	if (bToRight ? pp.Pos+1<=NumKeys(pp.Page) : pp.Pos>0)
		pp.Pos += bToRight ? 1 : -1;
	else if (!SeekToSibling(bToRight))
		return false;
	ASSERT(Top().Page.IsBranch);
	Path.push_back(PagePos(Tree->Tx.OpenPage(Top().Page.Entries(Tree->KeySize)[Top().Pos].PgNo()), 0));
	return true;
}

void BTreeCursor::InsertImp(Span k, RCSpan d) {
	InsertImpHeadTail(Tree->GetDataEntrySize(k, d.size()), k, d, d.size(), DB_EOF_PGNO);
}

void BTreeCursor::Put(Span k, RCSpan d, bool bInsert) {
	bool bExists = false;
	if (!Tree->Root) {
		Page pageRoot = dynamic_cast<DbTransaction&>(Tree->Tx).Allocate(PageAlloc::Leaf);
		Tree->SetRoot(pageRoot);
		Path.push_back(PagePos(pageRoot, 0));
		Initialized = true;
	} else {
		bExists = SeekToKey(k);
		if (bExists && bInsert)
			throw DbException(make_error_code(ExtErr::DB_DupKey), nullptr);
		ASSERT(!bExists || Top().Pos < NumKeys(Top().Page));
		Touch();
		if (bExists) {
			PagePos& pp = Top();
			EntryDesc e = Tree->GetEntryDesc(pp);
			if (!e.Overflowed && d.size() == e.DataSize) {
				memcpy((uint8_t*)e.LocalData.data(), d.data(), d.size());
				return;
			}
			FreeBigdataPages(DeleteEntry(pp, Tree->KeySize));
		}
	}
	InsertImp(k, d);
}

void BTreeCursor::PushFront(Span k, RCSpan d) {
	if (Tree->Tx.ReadOnly)
		Throw(errc::permission_denied);
	ASSERT(k.size() <= KVStorage::MAX_KEY_SIZE && (!Tree->KeySize || Tree->KeySize == k.size()));
	bool bExists = false;
	if (!Tree->Root) {
		Page pageRoot = dynamic_cast<DbTransaction&>(Tree->Tx).Allocate(PageAlloc::Leaf);
		Tree->SetRoot(pageRoot);
		Path.push_back(PagePos(pageRoot, 0));
		Initialized = true;
	} else {
		bExists = SeekToKey(k);
		ASSERT(!bExists || Top().Pos < NumKeys(Top().Page));
		Touch();
		if (bExists) {
			PagePos& pp = Top();
			EntryDesc e = Tree->GetEntryDesc(pp);
			uint32_t pageSize = Tree->Tx.Storage.PageSize;
			EntrySize ppEntry;
			if (!e.Overflowed ||
				e.LocalData.size() != e.DataSize % (pageSize-4) ||
				(ppEntry = Tree->GetDataEntrySize(k, d.size() + e.DataSize)).Size == 0)
			{
				Blob newdata = d + get_Data();
				FreeBigdataPages(DeleteEntry(pp, Tree->KeySize));
				InsertImp(k, newdata);
				return;
			}
			Blob head = d + e.LocalData;
			ASSERT(ppEntry.IsBigData && ppEntry.Size == head.size() % (pageSize - 4) && ppEntry.Size == (d.size() + e.DataSize) % (pageSize - 4));
			DeleteEntry(pp, Tree->KeySize);
			InsertImpHeadTail(ppEntry, k, head, d.size() + e.DataSize, e.PgNo);
			return;
		}
	}
	InsertImp(k, d);
}

void BTreeCursor::Drop() {
	if (Tree->Root) {
		DeepFreePage(Tree->Root);
		Tree->SetRoot(nullptr);
	}
}

void static CopyPage(Page& pageFrom, Page& pageTo, uint8_t keySize) {
	PageDesc pd = pageFrom.Entries(keySize);

	size_t size = pd[pd.Header.Num].P - (uint8_t*)&pd.Header;
	MemcpyAligned32(pageTo.Address, pageFrom.Address, size);
#ifdef X_DEBUG//!!!D
	memset((uint8_t*)pageTo.Address+size, 0xFE, pageFrom->Storage.PageSize-size);
#endif

	LiteEntry* dEntries = pageTo->aEntries = (LiteEntry*)Ext::Malloc(sizeof(LiteEntry) * (pd.Header.Num + 1));
	ssize_t off = (uint8_t*)pageTo.get_Address() - (uint8_t*)pageFrom.get_Address();
	for (int i = 0, n = pd.Header.Num; i <= n; ++i)
		dEntries[i].P = pd[i].P + off;
}

void BTreeCursor::PageTouch(int height) {
	Page& page = Path[height].Page;
	DbTransaction& tx = dynamic_cast<DbTransaction&>(Tree->Tx);
	if (!page.Dirty) {
		Page np = tx.Allocate(page.IsBranch ? PageAlloc::Branch : PageAlloc::Leaf);
		CopyPage(page, np, Tree->KeySize);
		tx.FreePage(page);

		int t = Path.size() - 1;
		EXT_FOR (CursorObj& co, Tree->Cursors) {
			BTreeCursor& c = dynamic_cast<BTreeCursor&>(co);
			if (&c != this && c.Path.size() > t && c.Path[t].Page == page)
				c.Path[t].Page = np;
		}
		page = np;
		if (height)
			GetLiteEntry(Path[height-1], Tree->KeySize).PutPgNo(page.N);
		else
			Tree->SetRoot(page);
	}
}

void BTreeCursor::Touch() {
	if (Tree->Name != DbTable::Main().Name) {
		DbCursor cM(Tree->Tx, DbTable::Main());
		BTreeCursor *btreeCursor = dynamic_cast<BTreeCursor*>(cM.m_pimpl.get());
		if (!btreeCursor->PageSearch(Span((const uint8_t*)Tree->Name.c_str(), Tree->Name.length()), true))
			Throw(E_FAIL);
		Tree->Dirty = true;
	}
	for (int i = 0; i < Path.size(); ++i)
		PageTouch(i);
}

bool BTreeCursor::PageSearchRoot(RCSpan k, bool bModify) {
	while (true) {
		Page& page = Path.back().Page;
		PageHeader& h = page.Header();
		if (!(h.Flags & PageHeader::FLAG_BRANCH))
			return true;
		PageDesc pd = page.Entries(Tree->KeySize);

		int i = 0;
		if (k.size() > MAX_KEY_SIZE)
			i = h.Num;
		else if (k.data()) {
			pair<int, bool> pp = Tree->EntrySearch(pd, k);		// bBranch=true
			i = pp.second ? pp.first+1 : pp.first;
		}
		Path.back().Pos = i;
		Path.push_back(PagePos(Tree->Tx.OpenPage(pd[i].PgNo()), 0));
		ASSERT(Path.size() < 10);
		if (bModify)
			PageTouch(Path.size()-1);
	}
}

bool BTreeCursor::PageSearch(RCSpan k, bool bModify) {
	if (Tree->Tx.m_bError)
		Throw(E_FAIL);

	if (!Tree->Root)
		return false;

	if (Tree->Name != DbTable::Main().Name && bModify && !IsDbDirty) {
		DbCursor cMain(Tree->Tx, DbTable::Main());
		BTreeCursor *btreeCursor = dynamic_cast<BTreeCursor*>(cMain.m_pimpl.get());
		const char *name = Tree->Name.c_str();
		if (!btreeCursor->PageSearch(Span((const uint8_t*)name, Tree->Name.length()), bModify))
			Throw(E_FAIL);
		IsDbDirty = bModify;
	}

	Path.resize(1);
	Path[0] = PagePos(Tree->Root, 0);
	if (bModify)
		PageTouch(0);
	return PageSearchRoot(k, bModify);
}

bool BTreeCursor::SeekToFirst() {
	if (!Initialized || Path.size() > 1) {
		if (!PageSearch(Span()))
			return false;
	}
	ASSERT(!Top().Page.IsBranch);
	Top().Pos = 0;
	return Initialized = ClearKeyData();
}

bool BTreeCursor::SeekToLast() {
	if (!Eof) {
		if (!Initialized || Path.size() > 1) {
			if (!PageSearch(Span(0, INT_MAX)))
				return false;
		}
		ASSERT(!Top().Page.IsBranch);
		Top().Pos = NumKeys(Top().Page)-1;
		Initialized = Eof = true;
	}
	return ClearKeyData();
}

}}} // Ext::DB::KV::
