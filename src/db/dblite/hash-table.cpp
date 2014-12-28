/*######     Copyright (c) 1997-2015 Ufasoft  http://ufasoft.com  mailto:support@ufasoft.com,  Sergey Pavlov  mailto:dev@ufasoft.com #########################################################################################################
#                                                                                                                                                                                                                                            #
# This program is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation;  either version 3, or (at your option) any later version.          #
# This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.   #
# You should have received a copy of the GNU General Public License along with this program; If not, see <http://www.gnu.org/licenses/>                                                                                                      #
############################################################################################################################################################################################################################################*/

#include <el/ext.h>

#include "dblite.h"
#include "b-tree.h"
#include "hash-table.h"

namespace Ext { namespace DB { namespace KV {

HashTable::HashTable(DbTransactionBase& tx)
	:	base(tx)
	,	PageMap(tx)	
	,	HtType(HashType::MurmurHash3)
#ifdef X_DEBUG//!!!D
	, MaxLevel(16)
#else
	,	MaxLevel(32)
#endif
{
	ASSERT(MaxLevel <= 32);
}

uint32_t HashTable::Hash(const ConstBuf& key) const {
   	switch (HtType) {
   	case HashType::MurmurHash3:
   		return MurmurHash3_32(key, Tx.Storage.m_salt);
	case HashType::Identity:
	{
		uint32_t r = 0;
		for (size_t n=min(key.Size, size_t(4)), i=0; i<n; ++i)
			r |= key.P[i] << 8*i;
		return r;
	}
	case HashType::RevIdentity:
		{
			uint32_t r = 0;
			for (size_t n=min(key.Size, size_t(4)), i=n; i--;)
				r |= key.P[n-i-1] << 8*i;
			return r;
		}
   	default:
   		Throw(E_NOTIMPL);
   	}
}

int HashTable::BitsOfHash() const {
	int r = 0;
	if (uint64_t nPages = PageMap.Length/4)
		r = BitOps::ScanReverse(uint32_t(nPages - 1));
	return r;
}

Page HashTable::TouchBucket(uint32_t nPage) {
	Dirty = true;
	uint64_t offset = uint64_t(nPage) * 4;
	Page page = Tx.OpenPage(PageMap.GetUInt32(offset));
	if (!page.Dirty)
		PageMap.PutUInt32(offset, (page = dynamic_cast<DbTransaction&>(Tx).Allocate(PageAlloc::Move, &page)).N);
	return page;
}

TableData HashTable::GetTableData() {
	TableData r = base::GetTableData();
	r.HtType = (byte)HtType;
	r.RootPgNo = PageMap.PageRoot ? htole(PageMap.PageRoot.N) : 0;
	r.PageMapLength = htole(PageMap.Length);
	return r;
}

uint32_t HashTable::GetPgno(uint32_t nPage) const {
	uint64_t offset = uint64_t(nPage) * 4;	
	return offset<PageMap.Length ? PageMap.GetUInt32(offset) : 0;
}

BTreeSubCursor::BTreeSubCursor(HtCursor& cHT)
	:	m_btree(cHT.Ht->Tx)
{
	SetMap(&m_btree);
	m_btree.SetKeySize(cHT.Ht->KeySize);
	m_btree.Root = cHT.m_pagePos.Page;
	NPage = cHT.NPage;
}

HtCursor::HtCursor(DbTransaction& tx, DbTable& table)
{
}


bool HtCursor::SeekToFirst() {
	for (uint64_t nPages=Ht->PageMap.Length/4, nPage=0; nPage<nPages; ++nPage) {
		if (uint32_t pgno = Ht->GetPgno(NPage = (uint32_t)nPage)) {
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
	for (uint64_t nPage=Ht->PageMap.Length/4; nPage--;) {
		if (uint32_t pgno = Ht->GetPgno(NPage = (uint32_t)nPage)) {
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
	m_pagePos.Pos = 0;
	if (bToRight) {
		for (uint64_t nPages=Ht->PageMap.Length/4; ++NPage < nPages;)
			if (uint32_t pgno = Ht->GetPgno(NPage))
				if ((m_pagePos.Page = Map->Tx.OpenPage(pgno)).Header().Num)
					return true;
	} else {
		while (NPage-- > 0)
			if (uint32_t pgno = Ht->GetPgno(NPage))
				if ((m_pagePos.Page = Map->Tx.OpenPage(pgno)).Header().Num)
					return true;
	}
	m_pagePos.Page = nullptr;
	return false;
}

bool HtCursor::SeekToNext() {
	for (;; SubCursor = new BTreeSubCursor(_self)) {
		if (SubCursor) {
			if (SubCursor->SeekToNext())
				return ClearKeyData();
			SubCursor = nullptr;
			m_pagePos.Pos = NumKeys(m_pagePos.Page);
		}	
		bool r = base::SeekToNext();
		if (!r || !m_pagePos.Page.IsBranch)
			return r;
	}
}

bool HtCursor::SeekToPrev() {
	for (;; SubCursor = new BTreeSubCursor(_self)) {
		if (SubCursor) {
			if (SubCursor->SeekToPrev())
				return ClearKeyData();
			SubCursor = nullptr;
			m_pagePos.Pos = 0;
		}
		bool r = base::SeekToPrev();
		if (!r || !m_pagePos.Page.IsBranch)
			return r;
	}
}

bool HtCursor::SeekToKeyHash(const ConstBuf& k, uint32_t hash) {
	SubCursor = nullptr;
	Initialized = ClearKeyData();
	for (int level=Ht->BitsOfHash(); level>=0; --level) {
		if (uint32_t pgno = Ht->GetPgno(NPage = hash & uint32_t((1LL << level)-1))) {				// converting to 1LL because int(1) << 32 == 1
			PageHeader& h = (m_pagePos.Page = Ht->Tx.OpenPage(pgno)).Header();
			if (h.Flags & PageHeader::FLAG_BRANCH) {
				return (SubCursor = new BTreeSubCursor(_self))->SeekToKey(k);
			} else {
				pair<int, bool> pp = Map->EntrySearch(m_pagePos.Page.Entries(Ht->KeySize), h, k);
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
	Ht->PageMap.PutUInt32(uint64_t(NPage)*4, (m_pagePos.Page = SubCursor->m_btree.Root).N);
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

void HashTable::Split(uint32_t nPage, int level) {
	uint32_t nPageNew = (1 << level) | nPage;
	ASSERT(nPageNew != nPage);
	Page page = TouchBucket(nPage);
	Page pageNew = dynamic_cast<DbTransaction&>(Tx).Allocate(PageAlloc::Leaf);
	PageMap.PutUInt32(uint64_t(nPageNew)*4, pageNew.N);
	uint32_t mask = uint32_t(1LL << (level+1))-1;
	byte bitMask = 0;
	PageHeader& h = page.Header();
	byte keyOffset = h.KeyOffset();
	if (HtType==HashType::Identity && KeySize) {
		if (h.Flags & PageHeader::FLAGS_KEY_OFFSET) {
			bitMask = 1 << (level & 7);
		} else if (level == 7)
			bitMask = 0x80;										// Start to Cut Key Head
	}
	LiteEntry *entries = page.Entries(KeySize);
	PageHeader& dh = pageNew.Header();
	int off = (bitMask == 0x80) && keyOffset<KeySize ? 1 : 0;	//!!!  was: (bitMask == 0x80) && keyOffset<KeySize-1 ? 1 : 0
	h.SetKeyOffset(byte(keyOffset + off));
	dh.SetKeyOffset(h.KeyOffset());
	byte *ps = h.Data,
		 *pd = dh.Data;
	for (int i=0; i<h.Num; ++i) {
		LiteEntry& e = entries[i];
		size_t size = e.Size() - off;
		if ((*e.P & bitMask)
			|| !bitMask && ((Hash(e.Key(KeySize)) & mask) != nPage))
		{
			memcpy(pd, e.P + off, size);
			pd += size;
			dh.Num++;
		} else {
			memmove(ps, e.P + off, size);
			ps += size;
		}
	}
	h.Num -= dh.Num;
	page.ClearEntries();
}

void HtCursor::Put(ConstBuf k, const ConstBuf& d, bool bInsert) {
	if (Ht->PageMap.Length == 0) {
		Ht->PageMap.PutUInt32(0, dynamic_cast<DbTransaction&>(Ht->Tx).Allocate(PageAlloc::Leaf).N);
		Map->Dirty = true;
	}
	for (uint32_t hash = Ht->Hash(k);;) {
LAB_AGAIN:
		bool bExists = SeekToKeyHash(k, hash);
		if (SubCursor)
			break;
		if (bExists && bInsert)
			throw DbException(E_EXT_DB_DupKey, nullptr);
		Touch();
		if (bExists)
			Delete();
		pair<size_t, bool> ppEntry = Map->GetDataEntrySize(k, d.Size);
		size_t ksize = Map->KeySize ? Map->KeySize-m_pagePos.Page.Header().KeyOffset() : 1+k.Size;
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
		InsertImpHeadTail(ppEntry, k, d, d.Size, DB_EOF_PGNO);
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
		uint64_t len = pageMap.Length;
		for (uint64_t i=0; i<len; i+=4) {
			if (uint32_t pgno = pageMap.GetUInt32(i))
				DeepFreePage(Ht->Tx.OpenPage(pgno));
		}
		pageMap.Length = 0;
		pageMap.SetRoot(nullptr);
		Map->Dirty = true;
	}
}


}}} // Ext::DB::KV::

