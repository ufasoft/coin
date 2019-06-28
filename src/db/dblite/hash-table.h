/*######   Copyright (c) 2014-2019 Ufasoft  http://ufasoft.com  mailto:support@ufasoft.com,  Sergey Pavlov  mailto:dev@ufasoft.com ####
#                                                                                                                                     #
# 		See LICENSE for licensing information                                                                                         #
#####################################################################################################################################*/

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

	HashTable(DbTransactionBase& tx);
	TableType Type() override { return TableType::HashTable; }
	uint32_t Hash(RCSpan key) const;
	int BitsOfHash() const;
	Page TouchBucket(uint32_t nPage);
	uint32_t GetPgno(uint32_t nPage) const;
	void Split(uint32_t nPage, int level);
private:
	void Init(const TableData& td) override {
		base::Init(td);
		HtType = (HashType)td.HtType;
		if (uint32_t pgno = letoh(td.RootPgNo))
			PageMap.SetRoot(Tx.OpenPage(pgno));
		PageMap.SetLength(letoh(td.PageMapLength));
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
	observer_ptr<HashTable> Ht;
#ifndef _DEBUG//!!!D
private:
#endif
	ptr<BTreeSubCursor> SubCursor;
	PagePos m_pagePos;
public:
	HtCursor() {}

	HtCursor(DbTransaction& tx, DbTable& table);

	void SetMap(PagedMap *pMap) override {
		base::SetMap(pMap);
		Ht.reset(dynamic_cast<HashTable*>(pMap));
	}

	PagePos& Top() override { return SubCursor ? SubCursor->Top() : m_pagePos; }

	void Touch() override;
	bool SeekToFirst() override;
	bool SeekToLast() override;
	bool SeekToSibling(bool bToRight) override;
	bool SeekToNext() override;
	bool SeekToPrev() override;
	bool SeekToKey(RCSpan k) override;
	bool Get(RCSpan k) { return SeekToKey(k); }
	void Put(Span k, RCSpan d, bool bInsert = false) override;
	void Update(RCSpan d) override;
	void Delete() override;

	void Drop() override;
	bool SeekToKeyHash(RCSpan k, uint32_t hash);				// returns bFound
	void UpdateFromSubCursor();

	HtCursor *Clone() override { return new HtCursor(*this); }

	friend class BTreeSubCursor;

	bool UpdateImpl(RCSpan k, RCSpan d, bool bInsert);
};


}}} // Ext::DB::KV::
