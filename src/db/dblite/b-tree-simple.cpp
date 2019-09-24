/*######   Copyright (c) 2014-2015 Ufasoft  http://ufasoft.com  mailto:support@ufasoft.com,  Sergey Pavlov  mailto:dev@ufasoft.com ####
#                                                                                                                                     #
# 		See LICENSE for licensing information                                                                                         #
#####################################################################################################################################*/


#include <el/ext.h>

#include "dblite.h"
#include "b-tree.h"

namespace Ext { namespace DB { namespace KV {

EntryDesc *BuildEntryIndex(PageHeader& h, void *raw, int n) {
	EntryDesc *entries = (EntryDesc*)raw;
	byte *p = h.Data;
	bool bIsBranch = h.Flags & PAGE_FLAG_BRANCH;
	for (int i=0; i<n; ++i) {
		EntryDesc& entry = entries[i];
		entry.P = p;
		entry.Key = ConstBuf(p+5, p[4]);
		UInt32 pgnoOrDataSize = GetLeUInt32(p);
		p += 5 + entry.Key.Size;
		if (bIsBranch) {
			entry.PgNo = pgnoOrDataSize;
			entry.Data = Buf();
		} else {
			entry.Flags = *p++;
			if (entry.Flags & ENTRY_FLAG_BIGDATA) {
				entry.Data = Buf(0, pgnoOrDataSize);
				entry.PgNo = GetLeUInt32(p);
				p += 4;
			} else {
				entry.Data = Buf(p, pgnoOrDataSize);
				entry.PgNo = 0;
				p += entry.Data.Size;
			}
		}
		entry.Size = p-entry.P;
	}
	ASSERT(p-(byte*)&h <= 8192);	//!!!T
	return entries;
}

EntryDesc BTree::SetOverflowPages(bool bIsBranch, const ConstBuf& k, const ConstBuf& d, UInt32& pgno, byte& flags) {
	EntryDesc e;
	e.PgNo = pgno;
	e.Key = k;
	e.Data = d;
	if (bIsBranch) {
		e.Size = GetBranchSize(k);
		return e;
	}
	pair<size_t, bool> pp = GetDataEntrySize(k, d, Tx.Storage.PageSize);
	e.Size = pp.first;
	if (pp.second) {
		e.Flags = ENTRY_FLAG_BIGDATA;
		size_t pageSize = Tx.Storage.PageSize;
		int n = d.Size / (pageSize-4);
		vector<UInt32> pages = Tx.AllocatePages(n);
		size_t full = n*(pageSize-4);
		const byte *p = d.P;
		size_t size = d.Size;
		size_t off = 0;
		if (d.Size > full) {
			p += (off = d.Size - full);
			size -= off;
		}
		for (size_t cbCopy, i=0; size; p+=cbCopy, size-=cbCopy, ++i) {
			cbCopy = std::min(size, pageSize-4);
			Page page = Tx.OpenPage(pages[i]);
			*(UInt32*)page.Address = htole(i==pages.size()-1 ? 0 : pages[i+1]);
			memcpy((byte*)page.Address+4, p, cbCopy);
		}
		e.PgNo = pages[0];
		e.Data = ConstBuf(d.P, off);
	} else
		e.Flags = 0;
	return e;
}


void BTree::Split(DbCursor& c, const ConstBuf& k, const ConstBuf& d, UInt32 pgno, int flags, bool bReplace) {
	bool bRootChanged = false;
	PagePos pp = c.Top();
	ASSERT(NumKeys(pp.Page));
	Page rp = Tx.Allocate(pp.Page.IsBranch);
	if (c.Path.size() < 2) {
		c.Path.resize(2);
		c.Path[1] = exchange(c.Path[0], PagePos(Root = Tx.Allocate(true), 0));
		bRootChanged = true;
		if (Name == DbTable::Main.Name)
			Tx.MainTableRoot = Root;
		AddEntry(c.Path[0], ConstBuf(), ConstBuf(), c.Path[1].Page.N, 0);
	}
	int ptop = c.Path.size()-2;
	int nKey = NumKeys(pp.Page),
		posSplit = nKey / 2,
		posNew = pp.Pos;
	bool bNewPos = posNew >= posSplit;
	PageHeader& h = *(PageHeader*)pp.Page.Address,
		      &rh = *(PageHeader*)rp.Address;
	ASSERT(h.Num);
	LiteEntry *entries = pp.Page.Entries(); //BuildEntryIndex(h, alloca(sizeof(EntryDesc) * h.Num), h.Num);

	DbCursor cn(c);
	cn.Top().Page = rp;
	cn.Path[ptop].Pos++;
	bool bDidSplit = false;
	EntryDesc e = SetOverflowPages(pp.Page.IsBranch, k, d, pgno, flags);
	size_t cbNewEntry = e.Size;
	{
		CBoolKeeper bk(c.IsSplitting.Ref(), true);

		if (!pp.Page.IsBranch) {
			size_t pmax = Tx.Storage.PageSize - 3;
			if (h.Num<20 || cbNewEntry>pmax/16) {
				size_t psize = cbNewEntry;
				if (posNew <= posSplit) {
					bNewPos = false;
					for (int i=0; i<posSplit; ++i) {
						if ((psize += entries[i].Size()) > pmax) {
							bNewPos = i < posNew;
							posSplit = i;
							break;
						}
					}
				} else {
					for (int i=h.Num-1; i>=posSplit; --i) {
						if ((psize += entries[i].Size()) > pmax) {
							bNewPos = i < posNew;
							posSplit = i+1;
							break;
						}
					}
				}
			}
		}

		ConstBuf sepKey = pp.Pos==posSplit && bNewPos ? k : GetEntryDesc(PagePos(pp.Page, posSplit)).Key;
		Page parent = c.Path[ptop].Page;
		PageHeader& h = *(PageHeader*)parent.Address;
		if (parent.SizeLeft() >= GetBranchSize(sepKey))
			AddEntry(cn.Path[cn.Path.size()-2], sepKey, ConstBuf(), rp.N);
		else {
			cn.Path.pop_back();
			bDidSplit = true;
			Split(cn, sepKey, ConstBuf(), rp.N, 0);
			if (cn.Path.size() == c.Path.size()) {
				PagePos ppt = c.Path[ptop];
				c.Path.insert(c.Path.begin()+ptop, ppt);
			}
			if (cn.Path[ptop].Page == c.Path[ptop].Page && cn.Path[ptop].Pos >= NumKeys(c.Path[ptop].Page)) {
				std::copy(cn.Path.begin(), cn.Path.begin()+ptop+1, c.Path.begin());
				c.Path[ptop].Pos--;
			}
		}
	}

	byte *pPart2 = posNew>=h.Num ? entries[h.Num-1].Upper() : entries[posNew].P;
	if (bNewPos) {
		size_t size1 = pPart2-entries[posSplit].P;
		memcpy(rh.Data, entries[posSplit].P, size1);
		AddEntry(rh.Data+size1, pp.Page.IsBranch, k, d, pgno, (byte)flags);
		memcpy(rh.Data+size1+cbNewEntry, pPart2, entries[h.Num-1].Upper()-pPart2);
		rh.Num = UInt16(exchange(h.Num, (UInt16)posSplit) - posSplit + 1);
	} else {
		memcpy(rh.Data, entries[posSplit].P, entries[h.Num-1].Upper()-entries[posSplit].P);
		rh.Num = UInt16(exchange(h.Num, UInt16(posSplit+1)) - posSplit);
		memmove(pPart2+cbNewEntry, pPart2, entries[posSplit].P-pPart2);
		AddEntry(pPart2, pp.Page.IsBranch, k, d, pgno, (byte)flags);
	}
#ifdef _DEBUG//!!!D
	rp.ClearEntries();
	rp.Entries();
#endif
	pp.Page.ClearEntries();
	rp.ClearEntries();

	int height = c.Path.size()-1;
	EXT_FOR (DbCursor& c1, Cursors) {
		if (&c1 != &c && c1.Initialized && !c1.IsSplitting) {
			if (bRootChanged)
				c1.Path.insert(c1.Path.begin(), PagePos(c.Path[0].Page, c1.Path.at(0).Pos >= posSplit ? 1 : 0));
			if (c1.Path.at(height).Page == pp.Page) {
				if (c1.Path[height].Pos >= posNew && !bReplace)
					c1.Path[height].Pos++;
				if (c1.Path[height].Pos >= h.Num) {
					c1.Path[height] = PagePos(rp, c1.Path[height].Pos - h.Num);
					c1.Path[ptop].Pos = cn.Path[ptop].Pos;
				}
			} else if (!bDidSplit && c1.Path[ptop].Page == c.Path[ptop].Page && c1.Path[ptop].Pos >= c.Path[ptop].Pos)
				c1.Path[ptop].Pos++;
		}
	}
}

void DbCursor::UpdateKey(const ConstBuf& key) {
	PagePos& pp = Top();
	PageHeader& h = *(PageHeader*)pp.Page.Address;
	ASSERT(pp.Pos < h.Num);

	LiteEntry *entries = pp.Page.Entries();
	ConstBuf oldKey = entries[pp.Pos].Key();
	if (ssize_t(key.Size - oldKey.Size) <= ssize_t(pp.Page.SizeLeft())) {                       // signed comparison
		byte* p = (byte*)oldKey.P;
		const byte *pRest = p + oldKey.Size;
		memmove(p+key.Size, pRest, entries[h.Num].P - pRest);
		memcpy(p, key.P, p[-1] = (byte)key.Size);
	} else {
		UInt32 pgno = entries[pp.Pos].PgNo();
		Tree->DeleteEntry(pp);
		Tree->Split(_self, key, ConstBuf(), pgno, 0, true);
	}
	pp.Page.ClearEntries();
}

void BTree::Move(DbCursor& cSrc, DbCursor& cDst) {
	cSrc.PageTouch(cSrc.Path.size()-1);
	cDst.PageTouch(cDst.Path.size()-1);

	PagePos ppSrc = cSrc.Top(),
		ppDst = cDst.Top();
	EntryDesc eSrc = GetEntryDesc(ppSrc);
	ConstBuf key = eSrc.Key;

	{
		DbCursor cs(cSrc),
			cd(cDst);

		if (ppSrc.Pos==0 && ppSrc.Page.IsBranch) {
			cs.PageSearchRoot(ConstBuf(), false);
			cs.Top().Pos = 0;
			key = GetEntryDesc(cs.Top()).Key;
		}
		if (ppDst.Pos==0 && ppDst.Page.IsBranch) {
			cd.PageSearchRoot(ConstBuf(), false);
			cd.Top().Pos = 0;
			cDst.UpdateKey(GetEntryDesc(cs.Top()).Key);
		}
		AddEntry(cDst.Top(), key, eSrc.Data, eSrc.PgNo, eSrc.Flags);
		DeleteEntry(cSrc.Top());
	}
	int h = cSrc.Path.size()-1;
	EXT_FOR (DbCursor& c1, Cursors) {
		if (&c1 != &cSrc && c1.Path[h] == cSrc.Path[h])
			c1.Path[h] = cDst.Top();
	}
	if (ppSrc.Pos == 0) {
		PagePos& ppParent = cSrc.Path[cSrc.Path.size()-2];
		if (ppParent.Pos != 0) {
			DbCursor c1(cSrc);
			c1.Path.pop_back();
			c1.UpdateKey(GetEntryDesc(cSrc.Top()).Key);
		}
		if (cSrc.Top().Page.IsBranch)
			cSrc.UpdateKey(ConstBuf());
	}
	if (ppDst.Pos == 0) {
		PagePos& ppParent = cDst.Path[cDst.Path.size()-2];
		if (ppParent.Pos != 0) {
			DbCursor c1(cDst);
			c1.Path.pop_back();
			c1.UpdateKey(GetEntryDesc(cDst.Top()).Key);
		}
		if (cDst.Top().Page.IsBranch)
			cDst.UpdateKey(ConstBuf());
	}
}

void BTree::Merge(DbCursor& cSrc, DbCursor& cDst) {
	cDst.PageTouch(cDst.Path.size()-1);

	int nKey = NumKeys(cDst.Top().Page);

	PageHeader& h = *(PageHeader*)cSrc.Top().Page.Address;
	EntryDesc *entries = BuildEntryIndex(h, alloca(sizeof(EntryDesc) * h.Num), h.Num);
	for (int i=0, j=nKey; i<NumKeys(cSrc.Top().Page); ++i, ++j) {
		EntryDesc& e = entries[i];
		ConstBuf key = e.Key;
		Page page1;
		if (0==i && cSrc.Top().Page.IsBranch) {
			DbCursor c(cSrc);
			c.PageSearchRoot(ConstBuf(), false);
			key = GetEntryDesc(PagePos(page1 = c.Top().Page, 0)).Key;
		}
		AddEntry(PagePos(cDst.Top().Page, j), key, e.Data, e.PgNo, e.Flags);
	}

	PagePos pp = *(cSrc.Path.end()-2);
	DeleteEntry(pp);
	if (0 == pp.Pos) {
		DbCursor c1(cSrc);
		c1.Path.pop_back();
		c1.UpdateKey(ConstBuf());
	}

	Page page = cSrc.Path.back().Page;
	Tx.FreePage(page);

	int t = cSrc.Path.size()-1;
	EXT_FOR (DbCursor& c, Cursors) {
		if (&c != &cSrc && c.Path.size() > t && c.Path[t].Page == page) {
			c.Path[t].Page = cDst.Top().Page;
			c.Path[t].Pos += nKey;
		}
	}

	cSrc.Path.pop_back();
	cSrc.Balance();
}

Page DbCursor::FindLeftSibling() {
	if (Path.size() <= 1)
		return Page(nullptr);
	PagePos& pp = Path[Path.size()-2];
	return pp.Pos > 0 ? Tree->Tx.OpenPage(pp.Page.Entries()[pp.Pos-1].PgNo()) : Page(nullptr);
}

Page DbCursor::FindRightSibling() {
	if (Path.size() <= 1)
		return Page(nullptr);
	PagePos& pp = Path[Path.size()-2];
	return pp.Pos < NumKeys(pp.Page)-1 ? Tree->Tx.OpenPage(pp.Page.Entries()[pp.Pos+1].PgNo()) : Page(nullptr);
}


void DbCuresor::Put() {
	size_t cbFree = pp.Page.SizeLeft();
	if (cbFree < cbEntry) {
		if (Page pageSibling = FindRightSibling()) {
			size_t cbMy = cbFree;
			size_t cbSiblingFree = pageSibling.SizeLeft();
			for (int i=h.Num-1; i>pp.Pos+1; --i) {						// resulting insert point should not be 0 or h.Num, because parent index can be incorrect
				size_t cb = entries[i].Size();
				if (cb > cbSiblingFree)
					break;
				cbSiblingFree -= cb;
				cbMy += cb;
				if (cbMy >= cbEntry) {
					int curPos = Top().Pos;
					for (int j=h.Num-1; j>=i; --j) {
						DbCursor cDst(_self, true, pageSibling);
						Top().Pos = j;
						Tree->Move(_self, cDst);
						pageSibling = cDst.Top().Page;
					}
					Top().Pos = curPos;
					goto LAB_ADD;
				}
			}
		}
		if (Page pageSibling = FindLeftSibling()) {
			size_t cbMy = cbFree;
			size_t cbSiblingFree = pageSibling.SizeLeft();
			for (int i=0; i<pp.Pos-1; ++i) {
				size_t cb = entries[i].Size();
				if (cb > cbSiblingFree)
					break;
				cbSiblingFree -= cb;
				cbMy += cb;
				if (cbMy >= cbEntry) {
					int curPos = exchange(Top().Pos, 0);
					for (int j=0; j<=i; ++j) {
						DbCursor cDst(_self, false, pageSibling);
						Tree->Move(_self, cDst);
						pageSibling = cDst.Top().Page;
					}
					Top().Pos = curPos - i - 1;
					goto LAB_ADD;
				}
			}
		}

		Tree->Split(_self, k, d, pp.Page.N, 0, bExists);
		goto LAB_RET;
	}
LAB_ADD:
	Tree->AddEntry(Top(), k, d, 0);
	if (!bExists) {
		int height = Path.size()-1;
		EXT_FOR (DbCursor& c1, Tree->Cursors) {
			if (&c1 != this && c1.Path.size() >= Path.size()) {
				PagePos& pp1 = c1.Path[height];
				if (pp1.Page==Top().Page && pp1.Pos>=Top().Pos)
					pp1.Pos++;
			}
		}
	}
LAB_RET:
	Tree->Tx.m_bError = false;
}


void Balance() {
	if (Path.size() < 2) {
		switch (h.Num) {
		case 0:
			Root = Page(nullptr);

			EXT_FOR (DbCursor& c1, Cursors) {
				if (&c1 != this && Path.size() > 0 && c1.Path[0].Page == page)
					c1.Path.clear();
			}
			Tx.FreePage(page);
			Path.clear();
			break;
		case 1:
			if (page.IsBranch) {
				EntryDesc *entries = BuildEntryIndex(h, alloca(sizeof(EntryDesc) * 2), 2);
				Root = Tx.OpenPage(entries[0].PgNo);

				EXT_FOR (DbCursor& c1, Cursors) {
					if (&c1 != this && Path.size() > 0 && c1.Path[0].Page == page)
						c1.Path[0].Page = Root;
				}
				Tx.FreePage(page);
			}
			break;
		}
	} else {
		int top = Path.size()-2;
		Page pageLeft = FindLeftSibling();
		bool bHaveLeftNeighbor = pageLeft;
		DbCursor c1(_self, !bHaveLeftNeighbor, bHaveLeftNeighbor ? pageLeft : FindRightSibling());
		if (bHaveLeftNeighbor) {
			c1.Path.back().Pos--;
			Path.back().Pos = 0;
		} else
			Path.back().Pos = h.Num;

		PageHeader& h1 = *(PageHeader*)c1.Top().Page.Address;
		if (!h1.IsUnderflowed()	&& h1.Num >= 2)
			Move(c1, _self);
		else {
			bHaveLeftNeighbor ? Merge(_self, c1) : Merge(c1, _self);
		}
	}

}

void BTree::AddEntry(void *p, bool bIsBranch, const ConstBuf& key, const ConstBuf& data, UInt32 pgno, byte flags) {
	PutLeUInt32(p, bIsBranch ? pgno : data.Size);
	memcpy((byte*)p+5, key.P, (((byte*)p)[4] = (byte)key.Size));
	if (!bIsBranch) {
		byte *q = (byte*)p+5+key.Size;
		if ((q[0] = flags) & ENTRY_FLAG_BIGDATA)
			PutLeUInt32(q+1, pgno);
		else
			memcpy(q+1, data.P, data.Size);
	}
}

void BTree::AddEntry(const PagePos& pagePos, const ConstBuf& key, const ConstBuf& data, UInt32 pgno, byte flags) {
	PageHeader& h = *(PageHeader*)pagePos.Page.Address;
	bool bIsBranch = pagePos.Page.IsBranch;
	ASSERT(pagePos.Pos <= h.Num);
	LiteEntry *entries = pagePos.Page.Entries(); //BuildEntryIndex(h, alloca(sizeof(EntryDesc) * h.Num), h.Num);
	if (data.P)
		SetOverflowPages(bIsBranch, key, data, pgno, flags);

	if (pagePos.Pos < h.Num) {
		LiteEntry& e = entries[pagePos.Pos];
		memmove(e.P + key.Size + (bIsBranch ? 5 : 6 + (flags & ENTRY_FLAG_BIGDATA ? 4 : data.Size)), e.P, entries[h.Num-1].Upper() - e.P);
		AddEntry(e.P, bIsBranch, key, data, pgno, flags);
	} else
		AddEntry(h.Num ? entries[h.Num-1].Upper() : h.Data, bIsBranch, key, data, pgno, flags);
	++h.Num;
	pagePos.Page.ClearEntries();

#ifdef _DEBUG//!!!D
	LiteEntry *en = BuildLiteEntryIndex(h, alloca(sizeof(LiteEntry) * (h.Num+1)), h.Num);
	memset(en[h.Num].P, 0xFE, Tx.Storage.PageSize-(en[h.Num].P - (byte*)&h));
#endif
}

void BTree::AddCell(const PagePos& pagePos, const ConstBuf& cell, byte *tmp, UInt32 pgno) {
	const Page& page = pagePos.Page;
	ASSERT(!page.Overflows);
	PageHeader& h = *(PageHeader*)page.Address;
	LiteEntry *entries = page.Entries();

	if (cell.Size > page.SizeLeft()) {
		IndexedEntryDesc *oe = page->OverflowEntry = new IndexedEntryDesc;
		oe->Index = pagePos.Pos;
		if (tmp) {
			memcpy(tmp, cell.P, cell.Size);
			oe->P = tmp;
		} else
			oe->P = cell.P;
		oe->Size = cell.Size;
		if (pgno)
			PutLeUInt32(oe->P, pgno);
	} else {
		LiteEntry& e = entries[pagePos.Pos];
		memmove(e.P + cell.Size, e.P, entries[h.Num].P - e.P);
		memcpy(e.P, cell.P, cell.Size);
		if (pgno)
			PutLeUInt32(e.P, pgno);
		++h.Num;
	}
	page.ClearEntries();
}



}}} // Ext::DB::KV::
