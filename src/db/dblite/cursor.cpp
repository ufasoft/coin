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

void PagedMap::Init(const TableData& td) {
	SetKeySize(td.KeySize);
}

TableData PagedMap::GetTableData() {
	TableData r;
	r.Type = (byte)Type();
	r.KeySize = KeySize;
	return r;
}

CursorObj::~CursorObj() {
//!!!R	CKVStorageKeeper keeper(&Tree->Tx.Storage);
	if (Map)
		Map->Cursors.erase(IntrusiveList<CursorObj>::const_iterator(this));
//!!!R	Path.clear();																	// explicit to be in scope of t_pKVStorage 
}

const ConstBuf& CursorObj::get_Key() {
	if (!m_key.P) {
		PagePos& pagePos = Top();
		LiteEntry& e = pagePos.Page.Entries(Map->KeySize)[pagePos.Pos];
		if (byte keyOffset = pagePos.Page.Header().KeyOffset()) {
			m_bigKey.Size = Map->KeySize;
			uint32_t leNpage = htole(NPage);
			memcpy(keyOffset + (byte*)memcpy(m_bigKey.data(), &leNpage, keyOffset), e.P, Map->KeySize - keyOffset);
			m_key = m_bigKey;
		} else
			m_key = e.Key(Map->KeySize);
	}
	return m_key;
}

const ConstBuf& CursorObj::get_Data() {
	if (!m_data.P) {
		EntryDesc e = Map->GetEntryDesc(Top());
		if (!e.Overflowed)
			m_data = e.LocalData;
		else {
			if (e.DataSize > numeric_limits<size_t>::max())
				Throw(E_FAIL);
			m_bigData.Size = (size_t)e.DataSize;
			size_t off = e.LocalData.Size;
			memcpy(m_bigData.data(), e.LocalData.P, off);
			KVStorage& stg = Map->Tx.Storage;
			for (uint32_t pgno = e.PgNo; pgno!=DB_EOF_PGNO;) {
				Page page = Map->Tx.OpenPage(pgno);
				pgno = *(BeUInt32*)(page.get_Address());
				size_t size = std::min(stg.PageSize - 4, m_bigData.Size-off);
				memcpy(m_bigData.data()+off, (const byte*)page.get_Address()+4, size);
				off += size;
//				ASSERT(pgno < 0x10000000); //!!!
			}
			m_data = m_bigData;
		}			
	}
	return m_data;
}

void CursorObj::FreeBigdataPages(uint32_t pgno) {
	DbTransaction& tx = dynamic_cast<DbTransaction&>(Map->Tx);
	KVStorage& stg = tx.Storage;
	while (pgno != DB_EOF_PGNO)
		tx.FreePage(exchange(pgno, *(BeUInt32*)(tx.OpenPage(pgno).get_Address())));
}

void CursorObj::Delete() {
	FreeBigdataPages(DeleteEntry(Top(), Map->KeySize));
	Deleted = true;
}

void CursorObj::InsertImpHeadTail(pair<size_t, bool>& ppEntry, ConstBuf k, const ConstBuf& head, uint64_t fullSize, uint32_t pgnoTail) {
	DbTransaction& tx = dynamic_cast<DbTransaction&>(Map->Tx);
	size_t pageSize = tx.Storage.PageSize;
	PagePos& pp = Top();
	LiteEntry *entries = pp.Page.Entries(Map->KeySize);
	tx.TmpPageSpace.Size = pageSize;
	byte *p = tx.TmpPageSpace.data(), *q = p;
	if (!Map->KeySize)
		*p++ = (byte)k.Size;
	byte keyOffset = pp.Page.Header().KeyOffset();
	memcpy(p, k.P+keyOffset, k.Size-keyOffset);
	Write7BitEncoded(p += k.Size-keyOffset, fullSize);
	memcpy(exchange(p, p+ppEntry.first), head.P, ppEntry.first);
	if (ppEntry.second) {
		size_t cbOverflow = head.Size - ppEntry.first;
		if (0 == cbOverflow)
			PutLeUInt32(exchange(p, p+4), pgnoTail);
		else {
			int n = int((cbOverflow+pageSize-4-1) / (pageSize-4));
			vector<uint32_t> pages = tx.AllocatePages(n);
			const byte *q = head.P + ppEntry.first;
			size_t off = ppEntry.first;
			for (size_t cbCopy, i=0; cbOverflow; q+=cbCopy, cbOverflow-=cbCopy, ++i) {
				cbCopy = std::min(cbOverflow, pageSize-4);
				Page page = tx.OpenPage(pages[i]);
				*(BeUInt32*)page.get_Address() = i==pages.size()-1 ? pgnoTail : pages[i+1];
				memcpy((byte*)page.get_Address()+4, q, cbCopy);
			}
			PutLeUInt32(exchange(p, p+4), pages[0]);
		}			
	}
	tx.m_bError = true;
	InsertCell(pp, ConstBuf(q, p-q), Map->KeySize);
	Deleted = false;
	if (pp.Page.Overflows)
		Balance();
	tx.m_bError = false;
}

bool CursorObj::SeekToNext() {
	if (!Initialized)
		return SeekToFirst();
	if (Eof)
		return false;

	PagePos& pp = Top();
	if (!exchange(Deleted, false) || pp.Pos >= NumKeys(pp.Page)) {
		if (pp.Pos+1 < NumKeys(pp.Page))
			pp.Pos++;
		else if (!SeekToSibling(true)) {
			Eof = true;
			return Initialized = false;		
		}
	}
		
//	ASSERT(!Top().Page.IsBranch);
	return ClearKeyData();
}

bool CursorObj::SeekToPrev() {
	if (!Initialized)
		return SeekToLast();

	PagePos& pp = Top();
	Deleted = false;
	if (pp.Pos > 0)
		pp.Pos--;
	else {
		if (!SeekToSibling(false))
			return Initialized = false;
		Top().Pos = NumKeys(Top().Page)-1;
	}
	Eof = false;

	ASSERT(!Top().Page.IsBranch);
	return ClearKeyData();
}

bool CursorObj::ReturnFromSeekKey(int pos) {
	Top().Pos = pos;
	Eof = false;	
	return Initialized = ClearKeyData();
}

/*!!!?
DbCursor::DbCursor(BTree& tree)
	:	Tree(&tree)
{
	Path.reserve(4);
	Tree->Cursors.push_back(_self);
}
*/

DbCursor::DbCursor(DbTransactionBase& tx, DbTable& table) {
	DbTransaction::CTables::iterator it = tx.Tables.find(table.Name);
	if (it != tx.Tables.end()) {
		AssignImpl(it->second->Type());
		m_pimpl->SetMap(it->second);
	} else {
		ptr<PagedMap> m;
		TableType type = table.Type;
		if (&table == &DbTable::Main()) {
			m = new BTree(tx);
			m->Name = table.Name;
			dynamic_cast<BTree*>(m.get())->Root = tx.MainTableRoot;
		} else {
			DbCursor cMain(tx, DbTable::Main());
			ConstBuf k(table.Name.c_str(), strlen(table.Name.c_str()));
			if (!cMain.SeekToKey(k))
				Throw(E_FAIL);
			TableData& td = *(TableData*)cMain.get_Data().P;
			type = (TableType)td.Type;
			switch (type) {
			case TableType::BTree: m = new BTree(tx); break;
			case TableType::HashTable: m = new HashTable(tx); break;
			default:
				Throw(errc::invalid_argument);
			}
			m->Name = table.Name;
			m->Init(td);
		}
		AssignImpl(type);
		m_pimpl->SetMap(m);
		tx.Tables.insert(make_pair(table.Name, m));
	}
	m_pimpl->Map->Cursors.push_back(*m_pimpl.get());
}


DbCursor::DbCursor(DbCursor& c) {
	m_pimpl = c.m_pimpl->Clone();
	m_pimpl->Map->Cursors.push_back(*m_pimpl.get());
}

DbCursor::DbCursor(DbCursor& c, bool bRight, Page& pageSibling) {
	m_pimpl = c.m_pimpl->Clone();
	m_pimpl->Map->Cursors.push_back(*m_pimpl.get());
	if (BTreeCursor *tc = dynamic_cast<BTreeCursor*>(m_pimpl.get())) {
		tc->Path[tc->Path.size()-2].Pos += bRight ? 1 : -1;
		m_pimpl->Top() = PagePos(pageSibling, bRight ? 0 : NumKeys(pageSibling));
	}
}

DbCursor::~DbCursor() {
	if (m_pimpl)
		m_pimpl->~CursorObj();
}

void DbCursor::AssignImpl(TableType type) {
	ASSERT(sizeof(m_cursorImp) >= max(sizeof(BTreeCursor), sizeof(HtCursor)));
	switch (type) {
	case TableType::BTree: m_pimpl = new(m_cursorImp) BTreeCursor; break;
	case TableType::HashTable: m_pimpl = new(m_cursorImp) HtCursor; break;
	default:
		Throw(errc::invalid_argument);
	}
	m_pimpl->m_dwRef = 1000;

/*
	switch (type) {
	case TableType::BTree: m_pimpl = new BTreeCursor; break;
	case TableType::HashTable: m_pimpl = new HtCursor; break;
	default:
		Throw(E_INVALIDARG);
	}
	*/
}

bool DbCursor::Seek(CursorPos cPos, const ConstBuf& k) {
	switch (cPos) {
	case CursorPos::First:
		return SeekToFirst();
	case CursorPos::Last:
		return SeekToLast();
	case CursorPos::Next:
		return SeekToNext();
	case CursorPos::Prev:
		if (!m_pimpl->Initialized || m_pimpl->Eof)
			return SeekToLast();
		else
			return SeekToPrev();
	case CursorPos::FindKey:
		return SeekToKey(k);
	default:
		Throw(errc::invalid_argument);
	}
}

void DbCursor::Delete() {
	if (m_pimpl->Map->Tx.ReadOnly)
		Throw(errc::permission_denied);
	if (!m_pimpl->Initialized || m_pimpl->Deleted)
		Throw(errc::invalid_argument);
	ASSERT(!m_pimpl->Top().Page.IsBranch);
	m_pimpl->Touch();
	m_pimpl->Delete();
}

void DbCursor::Put(ConstBuf k, const ConstBuf& d, bool bInsert) {
	if (m_pimpl->Map->Tx.ReadOnly)
		Throw(errc::permission_denied);
	ASSERT(k.Size <= KVStorage::MAX_KEY_SIZE && (!m_pimpl->Map->KeySize || m_pimpl->Map->KeySize==k.Size));

	m_pimpl->Put(k, d, bInsert);
}

}}} // Ext::DB::KV::

