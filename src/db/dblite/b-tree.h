/*######   Copyright (c) 2014-2015 Ufasoft  http://ufasoft.com  mailto:support@ufasoft.com,  Sergey Pavlov  mailto:dev@ufasoft.com ####
#                                                                                                                                     #
# 		See LICENSE for licensing information                                                                                         #
#####################################################################################################################################*/

#pragma once

namespace Ext { namespace DB { namespace KV {

const int FILL_THRESHOLD_PERCENTS = 25;
const size_t MAX_KEYS_PER_PAGE = 1024;
const byte ENTRY_FLAG_BIGDATA = 1;

struct EntryDesc : Buf {
	ConstBuf Key() {
		return ConstBuf(P+1, P[0]);
	}

	uint64_t DataSize;
	ConstBuf LocalData;
	uint32_t PgNo;
	bool Overflowed;
};

LiteEntry GetLiteEntry(const PagePos& pp, byte keySize);
size_t GetEntrySize(const pair<size_t, bool>& ppEntry, size_t ksize, uint64_t dsize);
void InsertCell(const PagePos& pagePos, const ConstBuf& cell, byte keySize);
uint32_t DeleteEntry(const PagePos& pp, byte keySize);
size_t CalculateLocalDataSize(uint64_t dataSize, size_t cbExtendedPrefix, size_t pageSize);

struct LiteEntry {
	byte *P;

	ConstBuf Key(byte keySize) {
		return keySize ? ConstBuf(P, keySize) : ConstBuf(P+1, P[0]);
	}

	uint32_t PgNo() {
		return GetLeUInt32(P-4);
	}

	void PutPgNo(uint32_t v) {
		PutLeUInt32(P-4, v);
	}

	uint64_t DataSize(byte keySize, byte keyOffset) const {
		const byte *p = P + (keySize ? keySize-keyOffset : 1+P[0]);
		return Read7BitEncoded(p);
	}

	ConstBuf LocalData(size_t pageSize, byte keySize, byte keyOffset) const {
		const byte *p = P + (keySize ? keySize-keyOffset : 1+P[0]);
		uint64_t dataSize = Read7BitEncoded(p);
		return ConstBuf(p, CalculateLocalDataSize(dataSize, p - P + keyOffset, pageSize));
	}

	uint32_t FirstBigdataPage() {
		return (this+1)->PgNo();
	}

	byte *Upper() { return (this+1)->P; }

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
	static void AddEntry(void *p, bool bIsBranch, const ConstBuf& key, const ConstBuf& data, uint32_t pgno, byte flags);
	void AddEntry(const PagePos& pagePos, const ConstBuf& key, const ConstBuf& data, uint32_t pgno, byte flags = 0);
private:
	void SetRoot(const Page& page);
	void BalanceNonRoot(PagePos& ppParent, Page&, byte *tmpPage);

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
	bool SeekToKey(const ConstBuf& k) override;	

	void Put(ConstBuf k, const ConstBuf& d, bool bInsert) override;
	void PushFront(ConstBuf k, const ConstBuf& d) override;
	void Delete() override;

private:
	BTreeCursor(DbCursor& c, bool bRight, Page& pageSibling);				// make siblig cursor
	BTreeCursor *Clone() override { return new BTreeCursor(*this); }
	
	void UpdateKey(const ConstBuf& key);
	bool PageSearchRoot(const ConstBuf& k, bool bModify);
	bool PageSearch(const ConstBuf& k, bool bModify = false);
	void Drop() override;
	void Balance() override;
	int CompareEntry(LiteEntry *entries, int idx, const ConstBuf& kk);

	Page FindLeftSibling();
	Page FindRightSibling();

	void InsertImp(ConstBuf k, const ConstBuf& d);
	
	friend class BTree;
	friend class DbTable;
};


}}} // Ext::DB::KV::
