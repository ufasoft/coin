#include <el/ext.h>

#include "dblite.h"
#include "b-tree.h"
#include "hash-table.h"

namespace Ext { namespace DB { namespace KV {

HashTable::HashTable(DbTransaction& tx)
	:	base(tx)
	,	PageMap(tx)	
	,	HtType(HashType::MurmurHash3)
	,	MaxLevel(32)
{
	ASSERT(MaxLevel <= 32);
}

int HashTable::BitsOfHash() const {
	int r = 0;
	if (UInt64 nPages = PageMap.Length/4)
		r = BitOps::ScanReverse(UInt32(nPages - 1));
	return r;
}

Page HashTable::TouchBucket(UInt32 nPage) {
	Dirty = true;
	Page page = Tx.OpenPage(PageMap.GetUInt32(UInt64(nPage)*4));
	if (!page.Dirty) {
		Page np = Tx.Allocate(PageAlloc::Move, &page);
		PageMap.PutUInt32(UInt64(nPage)*4, np.N);
		return np;
	}
	return page;
}

TableData HashTable::GetTableData() {
	TableData r = base::GetTableData();
	r.HtType = (byte)HtType;
	r.RootPgNo = PageMap.PageRoot ? htole(PageMap.PageRoot.N) : 0;
	r.PageMapLength = htole(PageMap.Length);
	return r;
}

UInt32 HashTable::GetPgno(UInt32 nPage) {
	UInt64 offset = UInt64(nPage) * 4;	
	return offset<PageMap.Length ? PageMap.GetUInt32(offset) : 0;
}

BTreeSubCursor::BTreeSubCursor(HtCursor& cHT)
	:	m_btree(cHT.Ht->Tx)
{
	Map = &m_btree;
	m_btree.SetKeySize(cHT.Ht->KeySize);
	m_btree.Root = cHT.m_pagePos.Page;
}

HtCursor::HtCursor(DbTransaction& tx, DbTable& table)
	:	NPage(0xFFFFFFFF)
{
}

bool HtCursor::SeekToFirst() {
	for (UInt64 nPages=Ht->PageMap.Length/4, nPage=0; nPage<nPages; ++nPage) {
		if (UInt32 pgno = Ht->GetPgno(NPage = (UInt32)nPage)) {
			Page page = Map->Tx.OpenPage(pgno);
			if (page.Header().Num) {
				m_pagePos = PagePos(page, 0);
				Eof = false;
				return Initialized = ClearKeyData();
			}
		}
	}
	return false;
}

bool HtCursor::SeekToLast() {
	for (UInt64 nPage=Ht->PageMap.Length/4; nPage--;) {
		if (UInt32 pgno = Ht->GetPgno(NPage = (UInt32)nPage)) {
			Page page = Map->Tx.OpenPage(pgno);
			PageHeader& h = page.Header();
			if (h.Num) {
				m_pagePos = PagePos(page, h.Num-1);
				Eof = true;
				return Initialized = ClearKeyData();
			}
		}
	}
	return false;
}

bool HtCursor::SeekToSibling(bool bToRight) {
	if (Ht->PageMap.Length == 0)
		return false;
	if (bToRight) {
		for (UInt64 nPages=Ht->PageMap.Length/4; ++NPage < nPages;) {
			if (UInt32 pgno = Ht->GetPgno(NPage)) {
				Page page = Map->Tx.OpenPage(pgno);
				if (NumKeys(page)) {
					m_pagePos = PagePos(page, 0);
					return true;
				}
			}
		}
	} else {
		while (NPage-- > 0) {
			if (UInt32 pgno = Ht->GetPgno(NPage)) {
				Page page = Map->Tx.OpenPage(pgno);
				if (NumKeys(page)) {
					m_pagePos = PagePos(page, 0);
					return true;
				}
			}
		}
	}
	return false;
}

bool HtCursor::SeekToNext() {
	if (SubCursor) {
		if (SubCursor->SeekToNext())
			return true;
		SubCursor = nullptr;
		m_pagePos.Pos = NumKeys(m_pagePos.Page);
	}	
	return base::SeekToNext();
}

bool HtCursor::SeekToPrev() {
	if (SubCursor) {
		if (SubCursor->SeekToPrev())
			return true;
		SubCursor = nullptr;
		m_pagePos.Pos = 0;
	}
	return base::SeekToPrev();
}

bool HtCursor::SeekToKeyHash(const ConstBuf& k, UInt32 hash) {
	SubCursor = nullptr;
	for (int level=Ht->BitsOfHash(); level>=0; --level) {
		if (UInt32 pgno = Ht->GetPgno(NPage = hash & UInt32((1LL << level)-1))) {				// converting to 1LL because int(1) << 32 == 1
			Initialized = ClearKeyData();
			m_pagePos.Page = Ht->Tx.OpenPage(pgno);
			if (m_pagePos.Page.IsBranch) {
				return (SubCursor = new BTreeSubCursor(_self))->SeekToKey(k);
			} else {
				LiteEntry *entries = m_pagePos.Page.Entries(Ht->KeySize);
				PageHeader& h = m_pagePos.Page.Header();
				pair<int, bool> pp = Map->EntrySearch(entries, h.Num, k);
				m_pagePos.Pos = pp.first;
				return pp.second;
			}
		}
	}
	return false;
}

bool HtCursor::SeekToKey(const ConstBuf& k) {
	return SeekToKeyHash(k, Ht->Hash(k));
}

void HtCursor::UpdateFromSubCursor() {
	Ht->PageMap.PutUInt32(UInt64(NPage)*4, (m_pagePos.Page = SubCursor->m_btree.Root).N);
	Map->Dirty = true;
}

void HtCursor::Touch() {
	if (SubCursor) {
		SubCursor->Touch();
		UpdateFromSubCursor();
	} else {
		m_pagePos.Page = Ht->TouchBucket(NPage);
	}
}

void HashTable::Split(UInt32 nPage, int level) {
	UInt32 nPageNew = (1 << level) | nPage;
	ASSERT(nPageNew != nPage);
	Page page = TouchBucket(nPage);
	Page pageNew = Tx.Allocate(PageAlloc::Leaf);
	PageMap.PutUInt32(UInt64(nPageNew)*4, pageNew.N);
	UInt32 mask = UInt32(1LL << (level+1))-1;
	LiteEntry *entries = page.Entries(KeySize);
	PageHeader& h = page.Header();
	PageHeader& dh = pageNew.Header();
	byte *ps = h.Data,
		 *pd = dh.Data;
	for (int i=0; i<h.Num; ++i) {
		LiteEntry& e = entries[i];
		size_t size = e.Size();
		if ((Hash(e.Key(KeySize)) & mask) == nPage) {
			memmove(ps, e.P, size);
			ps += size;
		} else {
			memcpy(pd, e.P, size);
			pd += size;
			dh.Num++;
		}
	}
	h.Num -= dh.Num;
	page.ClearEntries();
}

void HtCursor::Put(ConstBuf k, const ConstBuf& d, bool bInsert) {
	if (Ht->PageMap.Length == 0) {
		Ht->PageMap.PutUInt32(0, Ht->Tx.Allocate(PageAlloc::Leaf).N);
		Map->Dirty = true;
	}
	const size_t pageSize = Map->Tx.Storage.PageSize;
	UInt32 hash = Ht->Hash(k);
	while (true) {
LAB_AGAIN:
		bool bExists = SeekToKeyHash(k, hash);
		if (SubCursor)
			break;
		if (bExists && bInsert)
			throw DbException(E_EXT_DB_DupKey, nullptr);
		ASSERT(!bExists || Top().Pos < NumKeys(Top().Page));
		Touch();
		if (bExists)
			Delete();
		size_t ksize = Map->KeySize ? Map->KeySize : 1+k.Size;
		pair<size_t, bool> ppEntry = GetDataEntrySize(ksize, d.Size, pageSize);
		size_t entrySize = GetEntrySize(ppEntry, ksize, d.Size);
		if (m_pagePos.Page.SizeLeft(Map->KeySize) < entrySize) {
			for (int level = BitOps::ScanReverse(NPage); level<Ht->MaxLevel; ++level) {
				if (!Ht->GetPgno((1 << level) | NPage)) {
					Ht->Split(NPage, level);
					goto LAB_AGAIN;
				}
			}
			SubCursor = new BTreeSubCursor(_self);
			break;
		}
		InsertImpHeadTail(ppEntry, k, d, d.Size, 0, pageSize);
		return;
	}
	SubCursor->Put(k, d, bInsert);
	UpdateFromSubCursor();
}

void HtCursor::Delete() {
	if (SubCursor) {
		SubCursor->Delete();
		UpdateFromSubCursor();
	} else
		base::Delete();
}

void HtCursor::Drop() {
	Filet& pageMap = Ht->PageMap;
	if (pageMap.PageRoot) {
		UInt64 len = pageMap.Length;
		for (UInt64 i=0; i<len; i+=4) {
			if (UInt32 pgno = pageMap.GetUInt32(i))
				DeepFreePage(Ht->Tx.OpenPage(pgno));
		}
		pageMap.Length = 0;
		pageMap.PageRoot = nullptr;
		Map->Dirty = true;
	}
}


}}} // Ext::DB::KV::

