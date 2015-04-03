/*######   Copyright (c) 2014-2015 Ufasoft  http://ufasoft.com  mailto:support@ufasoft.com,  Sergey Pavlov  mailto:dev@ufasoft.com ####
#                                                                                                                                     #
# 		See LICENSE for licensing information                                                                                         #
#####################################################################################################################################*/

#pragma once

// File implementation
// Features:
//		Size up to 2^32 * BlockSize
//		Sparse files

#include "dblite.h"

namespace Ext { namespace DB { namespace KV {

#ifndef UCFG_DB_TLB
#	define UCFG_DB_TLB 0 //!!!T originally 0
#endif

typedef vector<pair<Page, int>> PagedPath;

struct  PathVisitor {
	virtual bool OnPathLevel(int level, Page& page) { Throw(E_NOTIMPL); }
	virtual bool OnPathLevelFlat(int level, uint32_t& pgno) { Throw(E_NOTIMPL); }
};

class Filet {
	typedef Filet class_type;
public:
	DbTransactionBase& m_tx;
	Page PageRoot;
	const int PageSizeBits;

	Filet(DbTransactionBase& tx)
		:	m_tx(tx)
		,	PageSizeBits(BitOps::Scan(tx.Storage.PageSize)-1)
#if UCFG_DB_TLB
		,	TlbPage(16)
		,	TlbAddress(16)
#endif
	{}

	void SetLength(uint64_t v);

	uint64_t get_Length() const { return m_length; }
	void put_Length(uint64_t v);
	DEFPROP(uint64_t, Length);

	void SetRoot(const Page& page) {
		PageRoot = page;
#if UCFG_DB_TLB
		ClearTlbs();
#endif
	}

	uint64_t GetPagesForLength(uint64_t len) const;
	uint32_t GetUInt32(uint64_t offset) const;
	void PutUInt32(uint64_t offset, uint32_t v);
	void PutPgNo(Page& page, int idx, uint32_t pgno) const;	// const because called from universal FindPath()
private:
	uint64_t m_length;
	int IndirectLevels;
	
	uint32_t GetPgNo(uint32_t pgno, int idx) const { return m_tx.Storage.GetUInt32(pgno, idx*4); }

#if UCFG_DB_TLB
	typedef LruMap<uint32_t, Page> CTlbPage;					// Caches
	mutable CTlbPage TlbPage;

	typedef LruMap<uint32_t, byte*> CTlbAddress;
	mutable CTlbAddress TlbAddress;

	void ClearTlbs() {
		TlbPage.clear();
		TlbAddress.clear();
	}
#endif // UCFG_DB_TLB

	uint32_t GetPgNo(Page& page, int idx) const;	
	bool TouchPage(Page& page);
	void TouchPage(Page& pageData, PagedPath& path);
	uint32_t RemoveRange(int level, Page& page, uint32_t first, uint32_t last);
	Page FindPath(uint64_t offset, PathVisitor& visitor) const;
	byte *FindPathFlat(uint64_t offset, PathVisitor& visitor) const;
	Page GetPageToModify(uint64_t offset, bool bOptional);

//	friend class HashTable;
};



}}} // Ext::DB::KV::


