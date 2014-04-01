#pragma once

#include <el/stl/dynamic_bitset>
#include EXT_HEADER_SHARED_MUTEX

#include EXT_HEADER_FUTURE

#include <el/db/db-itf.h>

#if UCFG_LIB_DECLS
#	ifdef _DBLITE
#		define DBLITE_CLASS // AFX_CLASS_EXPORT
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

#include "dblite-file-format.h"

namespace Ext { namespace DB { namespace KV {

using Ext::DB::CursorPos;
using Ext::DB::ITransactionable;

const int DB_MAX_VIEWS = 2048 << UCFG_PLATFORM_64,
		DB_NUM_DELETE_FROM_CACHE = 16;

const UInt32 DB_DEFAULT_PAGECACHE_SIZE = 1024 << (UCFG_PLATFORM_64 * 2);


const size_t DEFAULT_PAGE_SIZE = 8192;

const size_t DEFAULT_VIEW_SIZE = (DEFAULT_PAGE_SIZE * 64) << UCFG_PLATFORM_64;

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
class PagedMap;

class MMView : public Object {
	typedef MMView class_type;
	
//!!!D	DBG_OBJECT_COUNTER(class_type);
public:
	typedef Interlocked interlocked_policy;

	KVStorage& Storage;
	MMView *Next, *Prev;		// for IntrusiveList<>

	MemoryMappedView View;
	UInt32 N;
	volatile bool Flushed;
	bool Removed;

	MMView(KVStorage& storage)
		:	Storage(storage)
		,	Flushed(false)
		,	Removed(false)
		,	Next(0)				// necessary to init
		,	Prev(0)
	{
	}

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
	typedef Pimpl<PageObj> base;
public:
	Page() {
	}

	Page(PageObj *obj) {
		m_pimpl = obj;
	}

	Page(const Page& v)
		:	base(v)
	{}

	Page(EXT_RV_REF(Page) rv)
		:	base(static_cast<EXT_RV_REF(base)>(rv))
	{}	

	Page& operator=(const Page& v) {
		base::operator=(v);
		return *this;
	}

	Page& operator=(EXT_RV_REF(Page) rv) {
		base::operator=(static_cast<EXT_RV_REF(base)>(rv));
		return *this;
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

	bool get_IsBranch() const { return Header().Flags & PAGE_FLAG_BRANCH; }
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
	typedef KVStorage class_type;
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
	bool UseFlush;					// setting it to FALSE is very dangerous
	bool ProtectPages;
	CBool AsyncClose;
	CBool ReadOnly;

	enum class OpenState {
		Closed,
		Closing,
		Opened
	};

	KVStorage();
	~KVStorage();
	void Create(RCString filepath);
	void Open(RCString filepath);
	void Close(bool bLock = true);
	void Vacuum();
	bool Checkpoint() { return DoCheckpoint(); }

	virtual Page OpenPage(UInt32 pgno);
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

	UInt32 get_Salt() { return m_salt; }
	
	UInt32 put_Salt(UInt32 v) {
		if (m_state == OpenState::Opened)
			Throw(E_FAIL);
		m_salt = v;
	}

	DEFPROP(UInt32, Salt);
protected:
	COrderedPgNos m_nextFreePages;
	UInt32 m_salt;

	int(*m_pfnProgress)(void*);
	int m_stepProgress;
	void *m_ctxProgress;

private:
	shared_mutex ShMtx;

	volatile OpenState m_state;
	future<void> m_futClose;

//!!!R	void ReleaseReader(DbTransaction *dbTx = 0);
	
	DateTime m_dtPrevCheckpoint;
	CBool m_bModified;
	int m_bitsViewPageRatio;

//!!!R	Page LastFreePoolPage;

	void SetPageSize(size_t v);
	void DoClose(bool bLock);
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
	friend class HashTable;
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

class CursorObj : public Object {
public:
	PagedMap *Map;
	CursorObj *Next, *Prev;		// for IntrusiveList<>

	CursorObj()
		:	Map(0)
	{}

	~CursorObj() override;
	virtual PagePos& Top() =0;
	virtual void SetMap(PagedMap *pMap) { Map = pMap; }
	virtual void Touch() =0;
	virtual bool SeekToFirst() =0;
	virtual bool SeekToLast() =0;
	virtual bool SeekToSibling(bool bToRight) =0;
	virtual bool SeekToKey(const ConstBuf& k) =0;
	virtual bool SeekToNext();
	virtual bool SeekToPrev();
	virtual void Delete();
	virtual void Put(ConstBuf k, const ConstBuf& d, bool bInsert) =0;
	virtual void PushFront(ConstBuf k, const ConstBuf& d) { Throw(E_NOTIMPL); }
	virtual void Drop() =0;
	virtual void Balance() {}
protected:
	Blob m_bigData;
	ConstBuf m_key, m_data;
	CBool Initialized, Deleted, Eof, IsDbDirty;

	bool ClearKeyData() {
		m_bigData = Blob();
		m_key = m_data = ConstBuf();
		return true;					// just for method chaining
	}

	void DeepFreePage(const Page& page);
	bool ReturnFromSeekKey(int pos);
	void FreeBigdataPages(UInt32 pgno);
	virtual CursorObj *Clone() =0;
	void InsertImpHeadTail(pair<size_t, bool>& ppEntry, ConstBuf k, const ConstBuf& head, UInt64 fullSize, UInt32 pgnoTail, size_t pageSize);

	const ConstBuf& get_Key();
	const ConstBuf& get_Data();

	friend class DbCursor;
};

class DbCursor : public Pimpl<CursorObj> {
	typedef DbCursor class_type;
public:
	DbCursor(DbTransaction& tx, DbTable& table);
	DbCursor(DbCursor& c);
	DbCursor(DbCursor& c, bool bRight, Page& pageSibling);

	const ConstBuf& get_Key() { return m_pimpl->get_Key(); }
	DEFPROP_GET(const ConstBuf&, Key);

	const ConstBuf& get_Data() { return m_pimpl->get_Data(); }
	DEFPROP_GET(const ConstBuf&, Data);

	bool SeekToFirst() { return m_pimpl->SeekToFirst(); }
	bool SeekToLast() { return m_pimpl->SeekToLast(); }
	bool Seek(CursorPos cPos, const ConstBuf& k = ConstBuf());
	bool SeekToKey(const ConstBuf& k) { return m_pimpl->SeekToKey(k); }
	bool SeekToNext() { return m_pimpl->SeekToNext(); }
	bool SeekToPrev() { return m_pimpl->SeekToPrev(); }
	bool Get(const ConstBuf& k) { return SeekToKey(k); }
	void Delete();
	void Put(ConstBuf k, const ConstBuf& d, bool bInsert = false);
	void PushFront(ConstBuf k, const ConstBuf& d) { m_pimpl->PushFront(k, d); }
	void Drop() { m_pimpl->Drop(); }
private:
	void AssignImpl(TableType type);
};

struct TableData;

class PagedMap : public Object {
public:
	IntrusiveList<CursorObj> Cursors;

	String Name;
	DbTransaction& Tx;
	byte KeySize;
	CBool Dirty;

	PagedMap(DbTransaction& tx)
		:	Tx(tx)
		,	KeySize(0)
		,	m_pfnCompare(&Compare)
	{}

	void SetKeySize(byte keySize) {
		m_pfnCompare = (KeySize = keySize) ? &::memcmp : &Compare;
	}

	virtual TableType Type() =0;
	virtual void Init(const TableData& td);
	virtual TableData GetTableData();
	pair<int, bool> EntrySearch(LiteEntry *entries, int nKey, const ConstBuf& key);
protected:
	typedef int (__cdecl *PFN_Compare)(const void *p1, const void *p2, size_t size);
	PFN_Compare m_pfnCompare;

	static int __cdecl Compare(const void *p1, const void *p2, size_t cb2);
};

ENUM_CLASS(PageAlloc) {
	Zero,
	Nothing,
	Leaf,
	Branch,
	Copy,
	Move			// Copy and Free source
} END_ENUM_CLASS(PageAlloc);


class DBLITE_CLASS DbTransaction : noncopyable, public ITransactionable {
public:
	KVStorage& Storage;

	Int64 TransactionId;
	Page MainTableRoot;

	typedef map<String, ptr<PagedMap>> CTables;			// map<> has faster dtor than unordered_map<>
	CTables Tables;

	bool ReadOnly;
	CBool Bulk;

	DbTransaction(KVStorage& storage, bool bReadOnly = false);
	virtual ~DbTransaction();

	DbTransaction& Current();

	BTree& Table(RCString name);
	Page Allocate(PageAlloc pa, Page *pCopyFrom = 0);
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
	friend class HashTable;
	friend class BTreeCursor;
	friend class CursorObj;
	friend class Filet;
};


class DBLITE_CLASS DbTable {
public:
	static DbTable& AFXAPI Main();

	String Name;
	TableType Type;
	byte KeySize;						// 0=variable

	DbTable(RCString name = nullptr, byte keySize = 0, TableType type = TableType::BTree)
		:	Name(name)
		,	KeySize(keySize)
		,	Type(type)
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


