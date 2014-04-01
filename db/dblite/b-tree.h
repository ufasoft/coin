#pragma once

namespace Ext { namespace DB { namespace KV {

const int FILL_THRESHOLD_PERCENTS = 25;
const size_t MAX_KEYS_PER_PAGE = 1024;
const byte ENTRY_FLAG_BIGDATA = 1;

struct EntryDesc : Buf {
	ConstBuf Key() {
		return ConstBuf(P+1, P[0]);
	}

	UInt64 DataSize;
	ConstBuf LocalData;
	UInt32 PgNo;
	bool Overflowed;
};

EntryDesc GetEntryDesc(const PagePos& pp, byte keySize);
LiteEntry GetLiteEntry(const PagePos& pp, byte keySize);
pair<size_t, bool> GetDataEntrySize(size_t ksize, UInt64 dsize, size_t pageSize);	// <size, isBigData>
size_t GetEntrySize(const pair<size_t, bool>& ppEntry, size_t ksize, UInt64 dsize);
void InsertCell(const PagePos& pagePos, const ConstBuf& cell, byte keySize);
UInt32 DeleteEntry(const PagePos& pp, byte keySize);
size_t CalculateLocalDataSize(UInt64 dataSize, size_t keyFullSize, size_t pageSize);

struct LiteEntry {
	byte *P;

	ConstBuf Key(byte keySize) {
		return keySize ? ConstBuf(P, keySize) : ConstBuf(P+1, P[0]);
	}

	UInt32 PgNo() {
		return GetLeUInt32(P-4);
	}

	void PutPgNo(UInt32 v) {
		PutLeUInt32(P-4, v);
	}

	UInt64 DataSize(byte keySize) {
		const byte *p = P + (keySize ? keySize : 1+P[0]);
		return Read7BitEncoded(p);
	}

	ConstBuf LocalData(size_t pageSize, byte keySize) {
		const byte *p = P + (keySize ? keySize : 1+P[0]);
		UInt64 dataSize = Read7BitEncoded(p);
		return ConstBuf(p, CalculateLocalDataSize(dataSize, p-P, pageSize));
	}

	UInt32 FirstBigdataPage() {
		return (this+1)->PgNo();
	}

	byte *Upper() { return (this+1)->P; }

	size_t Size() { return Upper()-P; }
};

class BTree : public PagedMap {
	typedef PagedMap base;
public:
	Page Root;

	BTree(DbTransaction& tx)
		:	base(tx)
	{}

	~BTree() {
		ASSERT(Cursors.empty());
	}

	TableType Type() override { return TableType::BTree; }
	EntryDesc SetOverflowPages(bool bIsBranch, const ConstBuf& k, const ConstBuf& d, UInt32 pgno, byte flags);
	static void AddEntry(void *p, bool bIsBranch, const ConstBuf& key, const ConstBuf& data, UInt32 pgno, byte flags);
	void AddEntry(const PagePos& pagePos, const ConstBuf& key, const ConstBuf& data, UInt32 pgno, byte flags = 0);
private:
	void SetRoot(const Page& page);
	void BalanceNonRoot(PagePos& ppParent, Page&, byte *tmpPage);

	void Init(const TableData& td) override {
		base::Init(td);
		if (UInt32 pgno = letoh(td.RootPgNo))
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
	CPointer<BTree> Tree;

	BTreeCursor();

	PagePos& Top() override {
		ASSERT(!Path.empty());
		return Path.back();
	}

	void SetMap(PagedMap *pMap) override {
		base::SetMap(pMap);
		Tree = dynamic_cast<BTree*>(pMap);
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

	Page FindLeftSibling();
	Page FindRightSibling();

	void InsertImp(ConstBuf k, const ConstBuf& d);
	
	friend class BTree;
	friend class DbTable;
};



}}} // Ext::DB::KV::
