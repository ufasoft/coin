/*######     Copyright (c) 1997-2013 Ufasoft  http://ufasoft.com  mailto:support@ufasoft.com,  Sergey Pavlov  mailto:dev@ufasoft.com #######################################
#                                                                                                                                                                          #
# This program is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation;  #
# either version 3, or (at your option) any later version. This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the      #
# implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details. You should have received a copy of the GNU #
# General Public License along with this program; If not, see <http://www.gnu.org/licenses/>                                                                               #
##########################################################################################################################################################################*/

#pragma once

#include <el/stl/dynamic_bitset>
#include EXT_HEADER_SHARED_MUTEX

#include <el/db/db-itf.h>

#if UCFG_LIB_DECLS
#	ifdef _DBLITE
#		define DBLITE_CLASS AFX_CLASS_EXPORT
#	else
#		define DBLITE_CLASS
#		pragma comment(lib, "dblite")
#	endif
#else
#	define DBLITE_CLASS
#endif

#ifndef UCFG_DB_FREE_PAGES_BITSET
#	define UCFG_DB_FREE_PAGES_BITSET 1
#endif

#ifndef UCFG_DBLITE_DEBUG_VERBYTE
#	define UCFG_DBLITE_DEBUG_VERBYTE 0 //!!!D
#endif


namespace Ext { namespace DB { namespace KV {

using Ext::DB::CursorPos;
using Ext::DB::ITransactionable;

const int DB_MAX_VIEWS = 2048 << UCFG_PLATFORM_64,
		DB_NUM_DELETE_FROM_LRUVIEW = 8;

const UInt32 DB_DEFAULT_PAGECACHE_SIZE = 1024 << UCFG_PLATFORM_64;


const size_t DEFAULT_PAGE_SIZE = 8192;

const size_t DEFAULT_VIEW_SIZE = DEFAULT_PAGE_SIZE * 64;

const size_t MIN_KEYS = 4;
const size_t MAX_KEY_SIZE = 255;

struct PageHeader;
struct EntryDesc;
struct IndexedEntryDesc;
struct LiteEntry;
class BTree;
class DbTransaction;
class DbTable;
class KVStorage;

class MMView : public Object {
	typedef MMView class_type;
	
//!!!D	DBG_OBJECT_COUNTER(class_type);
public:
	KVStorage& Storage;
	MMView *Next, *Prev;		// for IntrusiveList<>

	MemoryMappedView View;
	UInt32 N;

	MMView(KVStorage& storage)
		:	Storage(storage)
	{}

	~MMView();
};

struct IndexedBuf {
	const byte *P;
	UInt16 Size, Index;

	IndexedBuf(const byte *p = 0, UInt16 size = 0, UInt16 index = 0)
		:	P(p)
		,	Size(size)
		,	Index(index)
	{}
};

class PageObj : public Object {
public:
	typedef Interlocked interlocked_policy;

	KVStorage& Storage;
	void *Address;
	ptr<MMView> View;
	LiteEntry * volatile Entries;
	IndexedBuf OverflowCells[2];
	UInt32 N;
	byte Overflows;
	CBool Dirty, ReadOnly, Live;

	PageObj(KVStorage& storage);
	~PageObj();
};

static const byte
	PAGE_FLAG_BRANCH = 1,
	PAGE_FLAG_LEAF = 2,
	PAGE_FLAG_OVERLOW = 4,
	PAGE_FLAG_FREE = 8;

class Page : public Pimpl<PageObj> {
	typedef Page class_type;
public:
	Page() {
	}

	Page(PageObj *obj) {
		m_pimpl = obj;
	}

	UInt32 get_N() const { return m_pimpl->N; }
	DEFPROP_GET(UInt32, N);

	bool get_Dirty() const { return m_pimpl->Dirty; }
	DEFPROP_GET(bool, Dirty);

	void *get_Address() const { return m_pimpl->Address; }
	DEFPROP_GET(void *, Address);

	PageHeader& Header() const { return *(PageHeader*)m_pimpl->Address; }

	byte get_Overflows() const { return m_pimpl->Overflows; }
	DEFPROP_GET(byte, Overflows);

	bool get_IsBranch() const;
	DEFPROP_GET(bool, IsBranch);

	LiteEntry *Entries(byte keySize) const;
	void ClearEntries() const;
	size_t SizeLeft(byte keySize) const;
	int FillPercent(byte keySize);
	bool IsUnderflowed(byte keySize);
};

}}}
EXT_DEF_HASH(Ext::DB::KV::Page)
namespace Ext { namespace DB { namespace KV {

int NumKeys(const Page& page);

struct DbHeader;

typedef unordered_set<UInt32> CPgNos;
typedef set<UInt32> COrderedPgNos;

class MappedFile : public Object {
public:
	Ext::MemoryMappedFile MemoryMappedFile;
};

class DBLITE_CLASS KVStorage {
public:
	static const size_t HeaderPageSize = 4096;

	UInt64 FileLength;
	UInt64 FileIncrement;
	const size_t ViewSize;
	size_t PageSize;
	UInt32 PageCount;

	String FilePath;
	File DbFile;

	list<ptr<MappedFile>> Mappings;

//!!!R	Blob m_blobDbHeader;
	AlignedMem m_alignedDbHeader;
	DbHeader& DbHeaderRef() { return *(DbHeader*)m_alignedDbHeader.get(); }

	//-----------------------------
	recursive_mutex MtxViews; // used in ~PageObj()

	typedef vector<PageObj*> COpenedPages;
	COpenedPages OpenedPages;

//!!!R	LruCache<Page> PageCache;
	UInt32 PageCacheSize, NewPageCount;

	typedef unordered_map<UInt32, ptr<MMView>> CViews;
	CViews Views;	

	//------------------
	mutex MtxRoot;

	UInt64 LastTransactionId;
	Page MainTableRoot;			// after OpenedPages
	//---------------------


	mutex MtxWrite;

	//-----------------------------
	mutex MtxFreePages;

#if UCFG_DB_FREE_PAGES_BITSET
	typedef dynamic_bitset<byte> CFreePagesBitset;
	CFreePagesBitset FreePagesBitset;
#endif

	typedef COrderedPgNos CFreePages;
	CFreePages FreePages;
	typedef CPgNos CReleasedPages;
	CReleasedPages ReleasedPages, ReaderLockedPages;
	dynamic_bitset<> AllocatedSinceCheckpointPages;

//!!!R	Int32 ReaderRefCount;
//!!!R	unordered_set<DbTransaction*> Readers;
	//-----------------------------

	String AppName;
	Version UserVersion;

	String FrontEndName;
	Version FrontEndVersion;

	TimeSpan CheckpointPeriod;
	bool Durability;
	bool ProtectPages;
	bool m_bOpened;
	
	KVStorage();
	~KVStorage();
	void Create(RCString filepath);
	void Open(RCString filepath);
	void Close(bool bLock = true);
	void Vacuum();
	bool Checkpoint() { return DoCheckpoint(); }

	Page OpenPage(UInt32 pgno);
	Page Allocate(bool bLock = true);

	void SetProgressHandler(int(*pfn)(void*), void* p = 0, int n = 1) {
		m_pfnProgress = pfn;
		m_ctxProgress = p;
		m_stepProgress = n;
	}

	void SetUserVersion(const Version& ver) {
		UserVersion = ver;
		m_bModified = true;
	}

	void lock_shared();
	void unlock_shared();
private:
	shared_mutex ShMtx;

//!!!R	void ReleaseReader(DbTransaction *dbTx = 0);

	COrderedPgNos m_nextFreePages;
	int(*m_pfnProgress)(void*);
	void *m_ctxProgress;
	int m_stepProgress;
		
	DateTime m_dtPrevCheckpoint;
	CBool m_bModified;

//!!!R	Page LastFreePoolPage;

	void WriteHeader();
	void MapMeta();
	void AddFullMapping(UInt64 fileLength);
	void Init();
	bool DoCheckpoint(bool bLock = true);
	UInt32 TryAllocateMappedFreePage();
	void UpdateFreePagesBitset();
	void FreePage(UInt32 pgno);
	UInt32 NextFreePgno(COrderedPgNos& newFreePages);
	void MarkAllocatedPage(UInt32 pgno);

	friend class DbTransaction;
};

typedef KVStorage DbStorage;

KVStorage& Stg();

struct PagePos {
	Ext::DB::KV::Page Page;
	int Pos;

	PagePos() {}

	PagePos(const Ext::DB::KV::Page& page, int pos)
		:	Page(page)
		,	Pos(pos)
	{}

	bool operator==(const PagePos& v) const { return Page==v.Page && Pos==v.Pos; }
};

class DBLITE_CLASS DbCursor : NonCopiable {
	typedef DbCursor class_type;
public:
	DbCursor *Next, *Prev;		// for IntrusiveList<>

	BTree *Tree;
	
	vector<PagePos> Path;
	CBool Initialized, Eof, IsDbDirty, IsSplitting;

	DbCursor(DbCursor& c);
	DbCursor(BTree& tree);
	DbCursor(DbTransaction& tx, DbTable& table);
	~DbCursor();

	const ConstBuf& get_Key();
	DEFPROP_GET(const ConstBuf&, Key);

	const ConstBuf& get_Data();
	DEFPROP_GET(const ConstBuf&, Data);

	PagePos& Top() {
		ASSERT(!Path.empty());
		return Path.back();
	}

	void PageTouch(int height);
	void Touch();
	bool SeekToFirst();
	bool SeekToLast();
	bool SeekToSibling(bool bToRight);
	bool SeekToNext();
	bool SeekToPrev();
	bool SeekToKey(const ConstBuf& k);
	bool Seek(CursorPos cPos, const ConstBuf& k = ConstBuf());
	
	bool Get(const ConstBuf& k) { return SeekToKey(k); }

	void Put(ConstBuf k, const ConstBuf& d, bool bInsert = false);
	void PushFront(ConstBuf k, const ConstBuf& d);
	void Delete();
private:
	DbCursor(DbCursor& c, bool bRight, Page& pageSibling);				// make siblig cursor

	Blob m_bigData;
	ConstBuf m_key, m_data;

	void ClearKeyData() {
		m_bigData = Blob();
		m_key = m_data = ConstBuf();
	}

	void UpdateKey(const ConstBuf& key);
	bool PageSearchRoot(const ConstBuf& k, bool bModify);
	bool PageSearch(const ConstBuf& k, bool bModify = false);
	void FreeBigdataPages(UInt32 pgno);
	void DeepFreePage(const Page& page);
	void Drop();
	void Balance();

	bool ReturnFromSeekKey(int pos);
	Page FindLeftSibling();
	Page FindRightSibling();

	void InsertImpHeadTail(pair<size_t, bool>& ppEntry, ConstBuf k, const ConstBuf& head, UInt64 fullSize, UInt32 pgnoTail, size_t pageSize);
	void InsertImp(ConstBuf k, const ConstBuf& d);
	
	friend class BTree;
	friend class DbTable;
};

#pragma pack(push, 1)
struct PageHeader {
	UInt16 Num;
	byte Flags;
	byte Data[];

};
#pragma pack(pop)

class BTree {
public:
	String Name;
	DbTransaction& Tx;
	Page Root;
	CBool Dirty;
	byte KeySize;

	BTree(const BTree& v);

	BTree(DbTransaction& tx)
		:	Tx(tx)
		,	KeySize(0)
		,	m_pfnCompare(&Compare)
	{}

	~BTree() {
		ASSERT(Cursors.empty());
	}

	void SetKeySize(byte keySize) {
		m_pfnCompare = (KeySize = keySize) ? &::memcmp : &Compare;
	}

	EntryDesc SetOverflowPages(bool bIsBranch, const ConstBuf& k, const ConstBuf& d, UInt32 pgno, byte flags);
	static void AddEntry(void *p, bool bIsBranch, const ConstBuf& key, const ConstBuf& data, UInt32 pgno, byte flags);
	void AddEntry(const PagePos& pagePos, const ConstBuf& key, const ConstBuf& data, UInt32 pgno, byte flags = 0);

private:
	IntrusiveList<DbCursor> Cursors;

	typedef int (__cdecl *PFN_Compare)(const void *p1, const void *p2, size_t size);
	PFN_Compare m_pfnCompare;

	static int __cdecl Compare(const void *p1, const void *p2, size_t cb2);
	pair<int, bool> EntrySearch(LiteEntry *entries, int nKey, bool bIsBranch, const ConstBuf& key);
	void SetRoot(const Page& page);
	void BalanceNonRoot(PagePos& ppParent, Page&, byte *tmpPage);
	friend class DbCursor;
};

class DBLITE_CLASS DbTransaction : NonCopiable, public ITransactionable {
public:
	KVStorage& Storage;

	Int64 TransactionId;
	Page MainTableRoot;

	typedef map<String, BTree> CTables;			// map<> has faster dtor than unordered_map<>
	CTables Tables;

	bool ReadOnly;
	CBool Bulk;

	DbTransaction(KVStorage& storage, bool bReadOnly = false);
	virtual ~DbTransaction();

	DbTransaction& Current();

	BTree& Table(RCString name);
	Page Allocate(bool bBranch = false);
	vector<UInt32> AllocatePages(int n);
	Page OpenPage(UInt32 pgno);

	void BeginTransaction() override {
		Throw(E_FAIL);
	}

	void Commit() override;
	void Rollback() override;
private:
	unique_lock<mutex> m_lockWrite;
	shared_lock<KVStorage> m_shlk;
	Blob TmpPageSpace;
	CPgNos AllocatedPages, ReleasedPages;
	CBool m_bComplete, m_bError;

	void FreePage(UInt32 pgno);
	void FreePage(const Page& page);
	void Complete();

	friend class BTree;
	friend class DbCursor;
};

class DBLITE_CLASS DbTable {
public:
	static DbTable Main;

	String Name;
	byte KeySize;						// 0=variable

	DbTable(RCString name = nullptr)
		:	Name(name)
		,	KeySize(0)
	{}

	void Open(DbTransaction& tx, bool bCreate = false);
	void Close() {}
	void Drop(DbTransaction& tx);
	void Put(DbTransaction& tx, const ConstBuf& k, const ConstBuf& d, bool bInsert = false);
	bool Delete(DbTransaction& tx, const ConstBuf& k);
private:
	void CheckKeyArg(const ConstBuf& k);
};

class CKVStorageKeeper {
	KVStorage *m_prev;
public:
	CKVStorageKeeper(KVStorage *cur);
	~CKVStorageKeeper();
};

#ifdef _DEBUG//!!!D
void CheckPage(Page& page);
#endif

}}} // Ext::DB::KV::


