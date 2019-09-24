/*######   Copyright (c) 2014-2019 Ufasoft  http://ufasoft.com  mailto:support@ufasoft.com,  Sergey Pavlov  mailto:dev@ufasoft.com ####
#                                                                                                                                     #
# 		See LICENSE for licensing information                                                                                         #
#####################################################################################################################################*/

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
	r.Type = (uint8_t)Type();
	r.KeySize = KeySize;
	return r;
}

CursorObj::~CursorObj() {
//!!!R	CKVStorageKeeper keeper(&Tree->Tx.Storage);
	if (Map)
		Map->Cursors.erase(IntrusiveList<CursorObj>::const_iterator(this));
//!!!R	Path.clear();																	// explicit to be in scope of t_pKVStorage
}

RCSpan CursorObj::get_Key() {
	if (!m_key.data()) {
		PagePos& pagePos = Top();
		LiteEntry& e = pagePos.Page.Entries(Map->KeySize)[pagePos.Pos];
		if (uint8_t keyOffset = pagePos.Page.Header().KeyOffset()) {
			m_bigKey.resize(Map->KeySize);
			uint32_t leNpage = htole(NPage);
			memcpy(keyOffset + (uint8_t*)memcpy(&m_bigKey[0], &leNpage, keyOffset), e.P, Map->KeySize - keyOffset);
			m_key = Span(m_bigKey);
		} else
			m_key = e.Key(Map->KeySize);
	}
	return m_key;
}

void CursorStream::Init() {
	if (!m_bInited) {
		EntryDesc e = m_curObj.Map->GetEntryDesc(m_curObj.Top());
		if (e.Overflowed) {
			m_length = e.DataSize;
			m_pgnoNext = e.PgNo;
		} else {
			m_length = e.LocalData.size();
			m_pgnoNext = DB_EOF_PGNO;
		}
		m_mb = e.LocalData;
		m_pos = 0;
		m_bInited = true;
	}
}

void CursorStream::put_Position(uint64_t pos) const {
	if (pos < m_pos)
		Throw(E_NOTIMPL);
	KVStorage& stg = m_curObj.Map->Tx.Storage;
	uint64_t rest;
	while ((rest=pos-m_pos) >= m_mb.size()) {
		m_pos += m_mb.size();
		if (m_pos == m_length)
			break;
		if (m_pgnoNext == DB_EOF_PGNO)
			Throw(ExtErr::DB_Corrupt);
		m_page = m_curObj.Map->Tx.OpenPage(m_pgnoNext);
		m_mb = Span((const uint8_t*)m_page.get_Address() + 4, (size_t)std::min(uint64_t(stg.PageSize - 4), m_length - m_pos));
		m_pgnoNext = *(BeUInt32*)(m_mb.data() - 4);
	}
    m_mb = m_mb.subspan((size_t)rest);
	base::put_Position(pos);



	/*!!!R

	m_bigData.Size = (size_t)e.DataSize;
	size_t off = e.LocalData.Size;
	memcpy(m_bigData.data(), e.LocalData.P, off);
	KVStorage& stg = Map->Tx.Storage;
	for (uint32_t pgno = e.PgNo; pgno!=DB_EOF_PGNO;) {
	Page page = Map->Tx.OpenPage(pgno);
	pgno = *(BeUInt32*)(page.get_Address());
	size_t size = std::min(stg.PageSize - 4, m_bigData.Size-off);
	memcpy(m_bigData.data()+off, (const uint8_t*)page.get_Address()+4, size);
	off += size;
	//				ASSERT(pgno < 0x10000000); //!!!
	}
	*/

}

bool CursorStream::Eof() const {
	return m_pos == m_length;
}

size_t CursorStream::Read(void *buf, size_t count) const {
	size_t r = 0;
	while (m_pos != m_length && count) {
		size_t size = (min)(count, m_mb.size());
		memcpy(buf, m_mb.data(), size);
		buf = (uint8_t*)buf + size;
		count -= size;
		r += size;
		put_Position(m_pos + size);
	}
	return r;
}

RCSpan CursorObj::get_Data() {
	if (!m_data.data()) {
		EntryDesc e = Map->GetEntryDesc(Top());
		if (!e.Overflowed)
			m_data = e.LocalData;
		else {
			if (e.DataSize > numeric_limits<size_t>::max())
				Throw(E_FAIL);
			m_dataStream.m_bInited = false;
			m_dataStream.Init();
			m_bigData.resize((size_t)m_dataStream.Length);
			m_dataStream.ReadBuffer(&m_bigData[0], m_bigData.size());
			m_data = Span(&m_bigData[0], m_bigData.size());
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

void CursorObj::InsertImpHeadTail(EntrySize es, Span k, RCSpan head, uint64_t fullSize, uint32_t pgnoTail) {
	DbTransaction& tx = dynamic_cast<DbTransaction&>(Map->Tx);
	uint32_t pageSize = tx.Storage.PageSize;
	PagePos& pp = Top();
	PageDesc pd = pp.Page.Entries(Map->KeySize);
	tx.TmpPageSpace.resize(pageSize);
	uint8_t *p = tx.TmpPageSpace.data(), *q = p;
	if (!Map->KeySize)
		*p++ = (uint8_t)k.size();
	uint8_t keyOffset = pd.Header.KeyOffset();
	memcpy(p, k.data() + keyOffset, k.size() - keyOffset);
	Write7BitEncoded(p += k.size() - keyOffset, fullSize);
	memcpy(exchange(p, p + es.Size), head.data(), es.Size);
	if (es.IsBigData) {
		size_t cbOverflow = head.size() - es.Size;
		if (0 == cbOverflow)
			PutLeUInt32(exchange(p, p + 4), pgnoTail);
		else {
			int n = int((cbOverflow + pageSize - 4 - 1) / (pageSize - 4));
			vector<uint32_t> pages = tx.AllocatePages(n);
			const uint8_t* q = head.data() + es.Size;
			size_t off = es.Size;
			for (size_t cbCopy, i = 0; cbOverflow; q += cbCopy, cbOverflow -= cbCopy, ++i) {
				cbCopy = std::min(cbOverflow, size_t(pageSize - 4));
				Page page = tx.OpenPage(pages[i]);
				*(BeUInt32*)page.get_Address() = i == pages.size() - 1 ? pgnoTail : pages[i + 1];
				memcpy((uint8_t*)page.get_Address() + 4, q, cbCopy);
			}
			PutLeUInt32(exchange(p, p + 4), pages[0]);
		}
	}
	tx.m_bError = true;
	InsertCell(pp, Span(q, p - q), Map->KeySize);
	Deleted = false;
	if (pp.Page.Overflows)
		Balance();
	tx.m_bError = false;
}

void CursorObj::DoDelete() {
	if (Map->Tx.ReadOnly)
		Throw(errc::permission_denied);
	if (!Initialized || Deleted)
		Throw(errc::invalid_argument);
	ASSERT(!Top().Page.IsBranch);
	Touch();
	Delete();
}

void CursorObj::DoPut(Span k, RCSpan d, bool bInsert) {
	if (Map->Tx.ReadOnly)
		Throw(errc::permission_denied);
	ASSERT(k.size() <= KVStorage::MAX_KEY_SIZE && (!Map->KeySize || Map->KeySize == k.size()));

	Put(k, d, bInsert);
}

void CursorObj::DoUpdate(const Span& d) {
	if (Map->Tx.ReadOnly)
		Throw(errc::permission_denied);
	if (!Initialized || Deleted || !Positioned)
		Throw(errc::invalid_argument);
	Update(d);
}

bool CursorObj::DoSeekToKey(RCSpan k) {
	bool r = SeekToKey(k);
	Positioned = r;
	return r;
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
			const char *pTableName = table.Name.c_str();
			Span k((const uint8_t*)pTableName, table.Name.length());
			if (!cMain.SeekToKey(k))
				Throw(E_FAIL);
			TableData& td = *(TableData*)cMain.get_Data().data();
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
	m_pimpl = c->Clone();
	m_pimpl->Map->Cursors.push_back(*m_pimpl.get());
}

DbCursor::DbCursor(DbCursor& c, bool bRight, Page& pageSibling) {
	m_pimpl = c->Clone();
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
	m_pimpl->m_aRef = 1000;
}

bool DbCursor::Seek(CursorPos cPos, RCSpan k) {
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


}}} // Ext::DB::KV::
