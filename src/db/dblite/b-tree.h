/*######   Copyright (c) 2014-2019 Ufasoft  http://ufasoft.com  mailto:support@ufasoft.com,  Sergey Pavlov  mailto:dev@ufasoft.com ####
#                                                                                                                                     #
# 		See LICENSE for licensing information                                                                                         #
#####################################################################################################################################*/

#pragma once

namespace Ext { namespace DB { namespace KV {

const int FILL_THRESHOLD_PERCENTS = 25;
const size_t MAX_KEYS_PER_PAGE = 1024;
const uint8_t ENTRY_FLAG_BIGDATA = 1;

struct EntryDesc {
    uint8_t *P;
    size_t Size;

	Span Key() {
		return Span(P + 1, P[0]);
	}

	uint64_t DataSize;
	Span LocalData;
	uint32_t PgNo;
	bool Overflowed;
};

LiteEntry GetLiteEntry(const PagePos& pp, uint8_t keySize);
size_t GetEntrySize(const pair<size_t, bool>& ppEntry, size_t ksize, uint64_t dsize);
void InsertCell(const PagePos& pagePos, RCSpan cell, uint8_t keySize);
uint32_t DeleteEntry(const PagePos& pp, uint8_t keySize);
size_t CalculateLocalDataSize(uint64_t dataSize, size_t cbExtendedPrefix, size_t pageSize);

struct LiteEntry {
	uint8_t* P;

	Span Key(uint8_t keySize) {
		return keySize ? Span(P, keySize) : Span(P + 1, P[0]);
	}

	uint32_t PgNo() {
		return GetLeUInt32(P-4);
	}

	void PutPgNo(uint32_t v) {
		PutLeUInt32(P-4, v);
	}

	uint64_t DataSize(uint8_t keySize, uint8_t keyOffset) const {
		const uint8_t* p = P + (keySize ? keySize - keyOffset : 1 + P[0]);
		return Read7BitEncoded(p);
	}

	Span LocalData(size_t pageSize, uint8_t keySize, uint8_t keyOffset) const {
		const uint8_t* p = P + (keySize ? keySize - keyOffset : 1 + P[0]);
		uint64_t dataSize = Read7BitEncoded(p);
		return Span(p, CalculateLocalDataSize(dataSize, p - P + keyOffset, pageSize));
	}

	uint32_t FirstBigdataPage() {
		return (this + 1)->PgNo();
	}

	uint8_t* Upper() { return (this + 1)->P; }

	size_t Size() { return Upper()-P; }
};

class BTree : public PagedMap {
	typedef PagedMap base;
public:
	Page Root;

	BTree(DbTransactionBase& tx)
		:	base(tx)
	{}

	~BTree() {
		ASSERT(Cursors.empty());
	}

	TableType Type() override { return TableType::BTree; }
	static void AddEntry(void* p, bool bIsBranch, RCSpan key, RCSpan data, uint32_t pgno, uint8_t flags);
	void AddEntry(const PagePos& pagePos, RCSpan key, RCSpan data, uint32_t pgno, uint8_t flags = 0);
private:
	void SetRoot(const Page& page);
	void BalanceNonRoot(PagePos& ppParent, Page&, uint8_t* tmpPage);

	void Init(const TableData& td) override {
		base::Init(td);
		if (uint32_t pgno = letoh(td.RootPgNo))
			Root = Tx.OpenPage(pgno);
	}

	TableData GetTableData() override {
		TableData r = base::GetTableData();
		r.RootPgNo = htole(Root.N);
		return r;
	}

	friend class BTreeCursor;
};

class DBLITE_CLASS BTreeCursor : public CursorObj {
	typedef CursorObj base;
	typedef BTreeCursor class_type;
public:
	vector<PagePos> Path;
	observer_ptr<BTree> Tree;

	BTreeCursor();

	PagePos& Top() override {
		ASSERT(!Path.empty());
		return Path.back();
	}

	void SetMap(PagedMap *pMap) override {
		base::SetMap(pMap);
		Tree.reset(dynamic_cast<BTree*>(pMap));
	}

	void PageTouch(int height);
	void Touch() override;
	bool SeekToFirst() override;
	bool SeekToLast() override;
	bool SeekToSibling(bool bToRight) override;
	bool SeekToKey(RCSpan k) override;

	void Put(Span k, RCSpan d, bool bInsert) override;
	void PushFront(Span k, RCSpan d) override;
	void Delete() override;

private:
	BTreeCursor(DbCursor& c, bool bRight, Page& pageSibling);				// make siblig cursor
	BTreeCursor *Clone() override { return new BTreeCursor(*this); }

	void UpdateKey(RCSpan key);
	bool PageSearchRoot(RCSpan k, bool bModify);
	bool PageSearch(RCSpan k, bool bModify = false);
	void Drop() override;
	void Balance() override;
	int CompareEntry(LiteEntry *entries, int idx, RCSpan kk);

	Page FindLeftSibling();
	Page FindRightSibling();

	void InsertImp(Span k, RCSpan d);

	friend class BTree;
	friend class DbTable;
};


}}} // Ext::DB::KV::
