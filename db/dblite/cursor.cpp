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
		m_key = pagePos.Page.Entries(Map->KeySize)[pagePos.Pos].Key(Map->KeySize);
	}
	return m_key;
}

const ConstBuf& CursorObj::get_Data() {
	if (!m_data.P) {
		EntryDesc e = GetEntryDesc(Top(), Map->KeySize);
		if (!e.Overflowed)
			m_data = e.LocalData;
		else {
			if (e.DataSize > numeric_limits<size_t>::max())
				Throw(E_FAIL);
			m_bigData.Size = (size_t)e.DataSize;
			size_t off = e.LocalData.Size;
			memcpy(m_bigData.data(), e.LocalData.P, off);
			KVStorage& stg = Map->Tx.Storage;
			for (UInt32 pgno = e.PgNo; pgno;) {
				Page page = Map->Tx.OpenPage(pgno);
				size_t size = std::min(stg.PageSize - 4, m_bigData.Size-off);
				memcpy(m_bigData.data()+off, (const byte*)page.get_Address()+4, size);
				off += size;
				pgno = *(BeUInt32*)(page.get_Address());
				ASSERT(pgno < 0x10000000); //!!!
			}
			m_data = m_bigData;
		}			
	}
	return m_data;
}

void CursorObj::FreeBigdataPages(UInt32 pgno) {
	DbTransaction& tx = Map->Tx;
	KVStorage& stg = tx.Storage;
	while (pgno)
		tx.FreePage(exchange(pgno, *(BeUInt32*)(tx.OpenPage(pgno).get_Address())));
}

void CursorObj::Delete() {
	FreeBigdataPages(DeleteEntry(Top(), Map->KeySize));
	Deleted = true;
}

void CursorObj::InsertImpHeadTail(pair<size_t, bool>& ppEntry, ConstBuf k, const ConstBuf& head, UInt64 fullSize, UInt32 pgnoTail, size_t pageSize) {
	PagePos& pp = Top();
	LiteEntry *entries = pp.Page.Entries(Map->KeySize);
	Map->Tx.TmpPageSpace.Size = pageSize;
	byte *p = Map->Tx.TmpPageSpace.data(), *q = p;
	if (!Map->KeySize)
		*p++ = (byte)k.Size;
	memcpy(p, k.P, k.Size);
	Write7BitEncoded(p += k.Size, fullSize);
	memcpy(exchange(p, p+ppEntry.first), head.P, ppEntry.first);
	if (ppEntry.second) {
		size_t cbOverflow = head.Size - ppEntry.first;
		if (0 == cbOverflow)
			PutLeUInt32(exchange(p, p+4), pgnoTail);
		else {
			int n = int((cbOverflow+pageSize-4-1) / (pageSize-4));
			vector<UInt32> pages = Map->Tx.AllocatePages(n);
			const byte *q = head.P + ppEntry.first;
			size_t off = ppEntry.first;
			for (size_t cbCopy, i=0; cbOverflow; q+=cbCopy, cbOverflow-=cbCopy, ++i) {
				cbCopy = std::min(cbOverflow, pageSize-4);
				Page page = Map->Tx.OpenPage(pages[i]);
				*(BeUInt32*)page.get_Address() = i==pages.size()-1 ? pgnoTail : pages[i+1];
				memcpy((byte*)page.get_Address()+4, q, cbCopy);
			}
			PutLeUInt32(exchange(p, p+4), pages[0]);
		}			
	}
	Map->Tx.m_bError = true;
	InsertCell(pp, ConstBuf(q, p-q), Map->KeySize);
	Deleted = false;
	if (pp.Page.Overflows)
		Balance();
	Map->Tx.m_bError = false;
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

DbCursor::DbCursor(DbTransaction& tx, DbTable& table) {
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
				Throw(E_INVALIDARG);
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

void DbCursor::AssignImpl(TableType type) {
	switch (type) {
	case TableType::BTree: m_pimpl = new BTreeCursor; break;
	case TableType::HashTable: m_pimpl = new HtCursor; break;
	default:
		Throw(E_INVALIDARG);
	}
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
		Throw(E_INVALIDARG);
	}
}

void DbCursor::Delete() {
	if (m_pimpl->Map->Tx.ReadOnly)
		Throw(E_ACCESSDENIED);
	if (!m_pimpl->Initialized || m_pimpl->Deleted)
		Throw(E_INVALIDARG);
	ASSERT(!m_pimpl->Top().Page.IsBranch);
	m_pimpl->Touch();
	m_pimpl->Delete();
}

void DbCursor::Put(ConstBuf k, const ConstBuf& d, bool bInsert) {
	if (m_pimpl->Map->Tx.ReadOnly)
		Throw(E_ACCESSDENIED);
	ASSERT(k.Size < 128 && (!m_pimpl->Map->KeySize || m_pimpl->Map->KeySize==k.Size));

	m_pimpl->Put(k, d, bInsert);
}

}}} // Ext::DB::KV::

