#pragma once

#include "filet.h"
#include "b-tree.h"

namespace Ext { namespace DB { namespace KV {

class HtCursor;

class HashTable : public PagedMap {
	typedef PagedMap base;
public:
	Filet PageMap;
	HashType HtType;
	const int MaxLevel;

	HashTable(DbTransaction& tx);
	TableType Type() override { return TableType::HashTable; }
	UInt32 Hash(const ConstBuf& key) const;
	int BitsOfHash() const;
	Page TouchBucket(UInt32 nPage);
	UInt32 GetPgno(UInt32 nPage);
	void Split(UInt32 nPage, int level);
private:
	void Init(const TableData& td) override {
		base::Init(td);
		HtType = (HashType)td.HtType;
		if (UInt32 pgno = letoh(td.RootPgNo))
			PageMap.PageRoot = Tx.OpenPage(pgno);
		PageMap.m_length = letoh(td.PageMapLength);
	}

	TableData GetTableData() override;
};

class BTreeSubCursor : public BTreeCursor {
	typedef BTreeCursor base;
public:
	BTree m_btree;

	BTreeSubCursor(HtCursor& cHT);

	~BTreeSubCursor() {
		SetMap(nullptr);
	}
};

class HtCursor : public CursorObj {
	typedef CursorObj base;
	typedef HtCursor class_type;
public:
	CPointer<HashTable> Ht;

	HtCursor() {}

	HtCursor(DbTransaction& tx, DbTable& table);

	void SetMap(PagedMap *pMap) override {
		base::SetMap(pMap);
		Ht = dynamic_cast<HashTable*>(pMap);
	}

	PagePos& Top() override { return SubCursor ? SubCursor->Top() : m_pagePos; }
	void Touch() override;
	bool SeekToFirst() override;
	bool SeekToLast() override;
	bool SeekToSibling(bool bToRight) override;
	bool SeekToNext() override;
	bool SeekToPrev() override;
	bool SeekToKey(const ConstBuf& k) override;	
	bool Get(const ConstBuf& k) { return SeekToKey(k); }
	void Put(ConstBuf k, const ConstBuf& d, bool bInsert = false) override;
	void Delete() override;


	void Drop() override;
#ifndef _DEBUG//!!!D
private:
#endif
	ptr<BTreeSubCursor> SubCursor;
	PagePos m_pagePos;
	UInt32 NPage;

	bool SeekToKeyHash(const ConstBuf& k, UInt32 hash);	// return <found, level>
	void UpdateFromSubCursor();

	HtCursor *Clone() override { return new HtCursor(*this); }

	friend class BTreeSubCursor;
};


}}} // Ext::DB::KV::
