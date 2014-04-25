#pragma once

// File implementation
// Features:
//		Size up to 2^32 * BlockSize
//		Sparse files

#include "dblite.h"

namespace Ext { namespace DB { namespace KV {

typedef vector<pair<Page, int>> PagedPath;

struct  PathVisitor {
	virtual bool OnPathLevel(int level, Page& page) { Throw(E_NOTIMPL); }
	virtual bool OnPathLevelFlat(int level, UInt32& pgno) { Throw(E_NOTIMPL); }
};

class Filet {
	typedef Filet class_type;
public:
	DbTransaction& m_tx;
	Page PageRoot;
	const int PageSizeBits;

	Filet(DbTransaction& tx)
		:	m_tx(tx)
		,	PageSizeBits(BitOps::Scan(tx.Storage.PageSize)-1)
	{}
	
	UInt64 get_Length() const { return m_length; }
	void put_Length(UInt64 v);
	DEFPROP(UInt64, Length);

	UInt64 GetPagesForLength(UInt64 len) const;
	UInt32 GetUInt32(UInt64 offset) const;
	void PutUInt32(UInt64 offset, UInt32 v);
	void PutPgNo(Page& page, int idx, UInt32 pgno) const;	// const because called from universal FindPath()
private:
	UInt64 m_length;

	UInt32 GetPgNo(UInt32 pgno, int idx) const { return m_tx.Storage.GetUInt32(pgno, idx*4); }
	
	UInt32 GetPgNo(Page& page, int idx) const;	
	int IndirectLevels() const;
	bool TouchPage(Page& page);
	void TouchPage(Page& pageData, PagedPath& path);
	UInt32 RemoveRange(int level, Page& page, UInt32 first, UInt32 last);
	Page FindPath(UInt64 offset, PathVisitor& visitor) const;
	byte *FindPathFlat(UInt64 offset, PathVisitor& visitor) const;
	Page GetPageToModify(UInt64 offset, bool bOptional);

	friend class HashTable;
};



}}} // Ext::DB::KV::


