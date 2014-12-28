/*######     Copyright (c) 1997-2015 Ufasoft  http://ufasoft.com  mailto:support@ufasoft.com,  Sergey Pavlov  mailto:dev@ufasoft.com #########################################################################################################
#                                                                                                                                                                                                                                            #
# This program is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation;  either version 3, or (at your option) any later version.          #
# This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.   #
# You should have received a copy of the GNU General Public License along with this program; If not, see <http://www.gnu.org/licenses/>                                                                                                      #
############################################################################################################################################################################################################################################*/

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
	uint32_t Hash(const ConstBuf& key) const;
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

	bool SeekToKeyHash(const ConstBuf& k, uint32_t hash);				// returns bFound
	void UpdateFromSubCursor();

	HtCursor *Clone() override { return new HtCursor(*this); }

	friend class BTreeSubCursor;
};


}}} // Ext::DB::KV::
