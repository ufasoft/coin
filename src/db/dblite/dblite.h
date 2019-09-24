/*######   Copyright (c) 2014-2019 Ufasoft  http://ufasoft.com  mailto:support@ufasoft.com,  Sergey Pavlov  mailto:dev@ufasoft.com ####
#                                                                                                                                     #
# 		See LICENSE for licensing information                                                                                         #
#####################################################################################################################################*/

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

namespace Ext {
namespace DB {
namespace KV {

using Ext::DB::CursorPos;
using Ext::DB::ITransactionable;

const int DB_MAX_VIEWS = 2048 << UCFG_PLATFORM_64, DB_NUM_DELETE_FROM_CACHE = 16;

const uint32_t DB_DEFAULT_PAGECACHE_SIZE = 1024 << (UCFG_PLATFORM_64 * 2);

const size_t DEFAULT_PAGE_SIZE = 4096;

const size_t DEFAULT_VIEW_SIZE = (DEFAULT_PAGE_SIZE * 64) << UCFG_PLATFORM_64;

const size_t MIN_KEYS = 4;
const size_t MAX_KEY_SIZE = 255;

struct PageHeader;
struct EntryDesc;
struct IndexedEntryDesc;
struct LiteEntry;
class BTree;
class DbTransactionBase;
class DbTable;
class KVStorage;
class PagedMap;
class PageObj;
class CursorObj;

size_t CalculateLocalDataSize(uint64_t dataSize, size_t cbExtendedPrefix, uint32_t pageSize);

struct LiteEntry {
	uint8_t* P;

	Span Key(uint8_t keySize) {
		return keySize ? Span(P, keySize) : Span(P + 1, P[0]);
	}

	uint32_t PgNo() {
		return GetLeUInt32(P - 4);
	}

	void PutPgNo(uint32_t v) {
		PutLeUInt32(P - 4, v);
	}

	struct CLocalData {
		Span Span;
		uint64_t DataSize;
		const uint8_t* P;
	};

	pair<Span, uint64_t> LocalData(uint32_t pageSize, uint8_t keySize, uint8_t keyOffset) const {
		const uint8_t* p = P + (keySize ? keySize - keyOffset : 1 + P[0]);
		uint64_t dataSize = Read7BitEncoded(p);
		return make_pair(Span(p, CalculateLocalDataSize(dataSize, p - P + keyOffset, pageSize)), dataSize);
	}

	uint32_t FirstBigdataPage() {
		return (this + 1)->PgNo();
	}

	uint8_t* Upper() { return (this + 1)->P; }

	size_t Size() { return Upper() - P; }
};

class ViewBase : public Object {
  public:
	typedef InterlockedPolicy interlocked_policy;

	KVStorage& Storage;
	ViewBase *Next, *Prev; // for IntrusiveList<>
	uint32_t N;
	volatile bool Flushed, Loaded;
	bool Removed;

	ViewBase(KVStorage& storage)
		: Storage(storage)
		, Next(0) // necessary to init
		, Prev(0)
		, Flushed(false)
		, Loaded(false)
		, Removed(false)
	{
	}

	virtual void Load() {}
	virtual void Flush() = 0;
	virtual void* GetAddress() = 0;
	virtual PageObj* AllocPage(uint32_t pgno) = 0;
};

struct IndexedBuf {
	const uint8_t* P;
	uint16_t Size, Index;

	IndexedBuf(const uint8_t* p = 0, uint16_t size = 0, uint16_t index = 0)
		: P(p), Size(size), Index(index) {}
};

class PageDesc {
public:
	PageHeader& Header;
	LiteEntry *Entries;

	PageDesc(PageHeader& h, LiteEntry* e) : Header(h), Entries(e) {}
	LiteEntry& operator[](int i) const { return Entries[i]; }
};

class PageObj : public Object {
public:
	typedef InterlockedPolicy interlocked_policy;

	//!!!R	KVStorage& Storage;
	void* m_address;
	ptr<ViewBase> View;
	atomic<LiteEntry*> aEntries;
	IndexedBuf OverflowCells[2];
	uint32_t N;
	uint8_t Overflows;
	CBool ReadOnly, Live;
	volatile bool Dirty, Flushed;

	PageObj(KVStorage& storage);
	~PageObj();
	inline void* GetAddress();
	__forceinline PageHeader& Header() { return *(PageHeader*)GetAddress(); }
	PageDesc Entries(uint8_t keySize);

	virtual void Flush() {}
};

class Page : public Pimpl<PageObj> {
	typedef Page class_type;
	typedef Pimpl<PageObj> base;

public:
	Page() {}

	Page(PageObj* obj) : base(obj) {}

	Page(const Page& v) : base(v) {}

	Page(EXT_RV_REF(Page) rv) : base(static_cast<EXT_RV_REF(base)>(rv)) {}

	Page& operator=(const Page& v) {
		base::operator=(v);
		return *this;
	}

	Page& operator=(EXT_RV_REF(Page) rv) {
		base::operator=(static_cast<EXT_RV_REF(base)>(rv));
		return *this;
	}

	uint32_t get_N() const { return m_pimpl->N; }
	DEFPROP_GET(uint32_t, N);

	bool get_Dirty() const { return m_pimpl->Dirty; }
	DEFPROP_GET(bool, Dirty);

	void* get_Address() const { return m_pimpl->GetAddress(); }
	DEFPROP_GET(void*, Address);

	__forceinline PageHeader& Header() const { return m_pimpl->Header(); }
	PageDesc Entries(uint8_t keySize) const { return m_pimpl->Entries(keySize); }

	uint8_t get_Overflows() const { return m_pimpl->Overflows; }
	DEFPROP_GET(uint8_t, Overflows);

	__forceinline bool get_IsBranch() const noexcept {
		return Header().Flags & PageHeader::FLAG_BRANCH;
	}
	DEFPROP_GET(bool, IsBranch);
	
	void ClearEntries() const;
	size_t SizeLeft(uint8_t keySize) const;
	int FillPercent(uint8_t keySize);
	bool IsUnderflowed(uint8_t keySize);
};

}}} // Ext::DB::KV

EXT_DEF_HASH(Ext::DB::KV::Page)
namespace Ext {
namespace DB {
namespace KV {

int NumKeys(const Page& page);

struct DbHeader;

typedef unordered_set<uint32_t> CUnorderedPageSet;
typedef set<uint32_t> COrderedPageSet;

class Pager : public InterlockedObject {
public:
	KVStorage& Storage;

	Pager(KVStorage& storage) : Storage(storage) {}

	virtual ~Pager() {}

	virtual void AddFullMapping(uint64_t fileLength) {}
	virtual void Flush();
	virtual void Close() {}
	virtual ViewBase* CreateView() = 0;
};

Pager* CreateMMapPager(KVStorage& storage);
Pager* CreateBufferPager(KVStorage& storage);

typedef COrderedPageSet CFreePages;
typedef CUnorderedPageSet CReleasedPages;

class SnapshotGenObj : public Object {
public:
	typedef InterlockedPolicy interlocked_policy;

	KVStorage& Storage;
	ptr<SnapshotGenObj> NextGen;
	vector<uint32_t>
		PagesToFree,
		PagesFreeAfterCheckpoint;

	SnapshotGenObj(KVStorage& storage)
		: Storage(storage)
	{}
	
	~SnapshotGenObj() override;
};

enum class ViewMode { Window, Full };

class DBLITE_CLASS KVStorage {
	typedef KVStorage class_type;

public:
	static const size_t HeaderPageSize = 4096;
	static const size_t MAX_KEY_SIZE = 254;

	path FilePath;
	File DbFile;

	//!!!R	Blob m_blobDbHeader;
	AlignedMem m_alignedDbHeader;
	DbHeader& DbHeaderRef() { return *(DbHeader*)m_alignedDbHeader.get(); }

	//-----------------------------
	recursive_mutex MtxViews; // used in ~PageObj()
	void* volatile ViewAddress;
	
	typedef vector<PageObj*> COpenedPages;
	COpenedPages OpenedPages;	

	typedef vector<ptr<ViewBase>> CViews;
	CViews Views;

	uint32_t PageCacheSize, NewPageCount;
	//------------------

	
	mutex MtxRoot;
	uint64_t LastTransactionId;
	Page MainTableRoot; // after OpenedPages
	//---------------------

	mutex MtxWrite;

	mutex MtxFreePages;
	CFreePages FreePages, PagesFreeAfterCheckpoint;
#if UCFG_DB_FREE_PAGES_BITSET
	typedef dynamic_bitset<uint8_t> CFreePagesBitset;
	CFreePagesBitset FreePagesBitset;
#endif
	ptr<SnapshotGenObj> CurGen;
	SnapshotGenObj* OldestAliveGen;
	//-----------------------------


	CReleasedPages ReleasedPages;
	dynamic_bitset<> AllocatedSinceCheckpointPages;

	//!!!R	int32_t ReaderRefCount;
	//!!!R	unordered_set<DbTransaction*> Readers;
	//-----------------------------

	String AppName;
	Version UserVersion;

	String FrontEndName;
	Version FrontEndVersion;

	TimeSpan CheckpointPeriod;
	DateTime DtNextCheckpoint;

	uint64_t FileLength;
	uint64_t FileIncrement;
	size_t ViewSize; // should be const

	ViewMode m_viewMode;
	ViewMode m_accessViewMode;

	enum class OpenState { Closed, Closing, Opened };
protected:
	COrderedPageSet m_nextFreePages;
	uint32_t m_salt;
	uint32_t PhysicalSectorSize;
	size_t HeaderSize;

	int (*m_pfnProgress)(void*);
	int m_stepProgress;
	void* m_ctxProgress;

private:
	shared_mutex ShMtx;	//!!!?

	volatile OpenState m_state;
	future<void> m_futClose;

	//!!!R	void ReleaseReader(DbTransaction *dbTx = 0);

	ptr<Pager> m_pager;

	CBool m_bModified;
	int m_bitsViewPageRatio;

	//!!!R	Page LastFreePoolPage;

public:
	uint32_t PageSize;
	uint32_t PageCount;
	bool Durability;
	bool UseFlush; // setting it to FALSE is very dangerous
	bool ProtectPages;
	bool UseMMapPager;
	bool AllowLargePages;
	CBool AsyncClose;
	CBool ReadOnly;

	KVStorage();
	~KVStorage();
	void Create(const path& filepath);
	void Open(const path& filepath);
	void Close(bool bLock = true);
	void Vacuum();
	bool Checkpoint() { return DoCheckpoint(Clock::now()); }
	uint64_t PageSpaceSize() const { return uint64_t(PageCount) * PageSize; }

	ptr<ViewBase> OpenView(uint32_t vno);
	ViewBase* GetViewNoLock(uint32_t vno);
	virtual void OnOpenPage(uint32_t pgno) {}
	void* GetPageAddress(ViewBase* view, uint32_t pgno);
	Page OpenPage(uint32_t pgno, bool bAlloc = false);
	Page Allocate(bool bLock = true);

	void SetProgressHandler(int (*pfn)(void*), void* p = 0, int n = 1) {
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

	uint32_t get_Salt() { return m_salt; }

	void put_Salt(uint32_t v) {
		if (m_state == OpenState::Opened)
			Throw(E_FAIL);
		m_salt = v;
	}
	DEFPROP(uint32_t, Salt);
private:
	uint32_t GetUInt32(uint32_t pgno, int offset);
	void SetPageSize(size_t v);
	void DoClose(bool bLock);
	void WriteHeader();
	void CommonOpen();
	void Init();
	bool DoCheckpoint(const DateTime& now, bool bLock = true);
	uint32_t TryAllocateMappedFreePage();
	void UpdateFreePagesBitset();
	void FreePage(uint32_t pgno);
	uint32_t NextFreePgno(COrderedPageSet& newFreePages);
	void MarkAllocatedPage(uint32_t pgno);
	void PrepareCreateOpen();
	void Open(File::OpenInfo& oi);
	void AfterOpenView(ViewBase* view, bool bNewView, bool bCacheLocked);
	void AdjustViewCount();

	friend class DbTransaction;
	friend class HashTable;
	friend class Filet;
	friend class AllocatedPageObj;
	friend class BufferView;
	friend class SnapshotGenObj;
};

typedef KVStorage DbStorage;

KVStorage& Stg();

struct PagePos {
	Ext::DB::KV::Page Page;
	int Pos;
	//!!!R	uint8_t KeyOffset;

	PagePos()
	//!!!R		: KeyOffset(0)	//!!!?
	{}

	PagePos(const Ext::DB::KV::Page& page, int pos)
		: Page(page), Pos(pos)
	//!!!R		,	KeyOffset(0)
	{}

	bool operator==(const PagePos& v) const { return Page == v.Page && Pos == v.Pos; }
};

const uint32_t DB_EOF_PGNO = 0;

class CursorStream : public MemStreamWithPosition {
	typedef MemStreamWithPosition base;

	CursorObj& m_curObj;
	uint64_t m_length;
	mutable Span m_mb;
	mutable Page m_page;
	mutable uint32_t m_pgnoNext;
	CBool m_bInited;
public:
	CursorStream(CursorObj& curObj) : m_curObj(curObj) {}

	uint64_t get_Length() const override { return m_length; }

	void Init();

	void Clear() {
		m_page = nullptr;
		m_bInited = false;
	}
	void put_Position(uint64_t pos) const override;
	bool Eof() const override;
	size_t Read(void* buf, size_t count) const override;
private:
	friend class CursorObj;
};

struct EntrySize {
	size_t Size;
	bool IsBigData;

	EntrySize(size_t size = 0, bool isBigData = false)
		: Size(size)
		, IsBigData(isBigData)
	{}
};

class CursorObj : public Object {
public:
	typedef NonInterlockedPolicy interlocked_policy;

	PagedMap* Map;
	CursorObj *Next, *Prev; // for IntrusiveList<>
protected:
	vector<uint8_t> m_bigKey, m_bigData;
	Span m_key, m_data;
	CursorStream m_dataStream;
	uint32_t NPage; // used in get_Key()
	CBool Initialized, Deleted, Eof, IsDbDirty, Positioned;
public:
	CursorObj()
		: Map(0)
		, m_dataStream(*this)
		, NPage(0xFFFFFFFF) {
	}

	~CursorObj() override;
	virtual PagePos& Top() = 0;
	virtual void SetMap(PagedMap* pMap) {
		Map = pMap;
	}
	virtual void Touch() = 0;
	virtual bool SeekToFirst() = 0;
	virtual bool SeekToLast() = 0;
	virtual bool SeekToSibling(bool bToRight) = 0;
	virtual bool SeekToKey(RCSpan k) = 0;
	virtual bool SeekToNext();
	virtual bool SeekToPrev();
	virtual void Delete();
	virtual void Put(Span k, RCSpan d, bool bInsert) = 0;
	virtual void Update(RCSpan d) {
		Put(get_Key(), d, false);
	}
	virtual void PushFront(Span k, RCSpan d) {
		Throw(E_NOTIMPL);
	}
	virtual void Drop() = 0;
	virtual void Balance() {
	}

protected:
	bool ClearKeyData() {
		m_dataStream.Clear();
		m_bigKey.clear();
		m_bigData.clear();
		m_key = m_data = Span();
		return true; // just for method chaining
	}

	void DeepFreePage(const Page& page);
	bool ReturnFromSeekKey(int pos);
	void FreeBigdataPages(uint32_t pgno);
	virtual CursorObj* Clone() = 0;
	void InsertImpHeadTail(EntrySize es, Span k, RCSpan head, uint64_t fullSize, uint32_t pgnoTail);
	void DoDelete();
	void DoPut(Span k, RCSpan d, bool bInsert);
	void DoUpdate(const Span& d);
	bool DoSeekToKey(RCSpan k);

	RCSpan get_Key();
	RCSpan get_Data();

	friend class DbCursor;
};

class DbCursor : public Pimpl<CursorObj> {
	typedef DbCursor class_type;

	static const int MAX_CURSOR_SIZEOF = 35; // must be >= sizeof(BTreeCursor or HtCursor)
	uint64_t m_cursorImp[MAX_CURSOR_SIZEOF];
public:
	DbCursor(DbTransactionBase& tx, DbTable& table);
	DbCursor(DbCursor& c);
	DbCursor(DbCursor& c, bool bRight, Page& pageSibling);
	~DbCursor();

	RCSpan get_Key() { return m_pimpl->get_Key(); }
	DEFPROP_GET(const Span&, Key);

	RCSpan get_Data() { return m_pimpl->get_Data(); }
	DEFPROP_GET(const Span&, Data);

	Stream& get_DataStream() {
		m_pimpl->m_dataStream.Init();
		return m_pimpl->m_dataStream;
	}
	DEFPROP_GET(Stream&, DataStream);

	bool SeekToFirst() { return m_pimpl->SeekToFirst(); }
	bool SeekToLast() { return m_pimpl->SeekToLast(); }
	bool Seek(CursorPos cPos, RCSpan k = Span());
	bool SeekToKey(RCSpan k) { return m_pimpl->DoSeekToKey(k); }
	bool SeekToNext() { return m_pimpl->SeekToNext(); }
	bool SeekToPrev() { return m_pimpl->SeekToPrev(); }
	bool Get(RCSpan k) { return SeekToKey(k); }
	void Delete() { m_pimpl->DoDelete(); }
	void Put(Span k, RCSpan d, bool bInsert = false) { return m_pimpl->DoPut(k, d, bInsert); }
	void Update(const Span& d) { return m_pimpl->DoUpdate(d); } // requires to call Seek() before. Optimization: key k ignored if possible
	void PushFront(Span k, RCSpan d) { m_pimpl->PushFront(k, d); }
	void Drop() { m_pimpl->Drop(); }

private:
	void AssignImpl(TableType type);
};

struct TableData;

class PagedMap : public InterlockedObject {
protected:
	typedef int(__cdecl* PFN_Compare)(const void* p1, const void* p2, size_t size);
	PFN_Compare m_pfnCompare;
public:
	IntrusiveList<CursorObj> Cursors;
	string Name;
	DbTransactionBase& Tx;
	uint8_t KeySize;
	CBool Dirty;

	PagedMap(DbTransactionBase& tx) : Tx(tx), KeySize(0), m_pfnCompare(&Compare) {}

	void SetKeySize(uint8_t keySize) { m_pfnCompare = (KeySize = keySize) ? &::memcmp : &Compare; }

	virtual TableType Type() = 0;
	virtual void Init(const TableData& td);
	virtual TableData GetTableData();
	EntrySize GetDataEntrySize(RCSpan k, uint64_t dsize) const;
	pair<int, bool> EntrySearch(const PageDesc& pd, RCSpan k);
	EntryDesc GetEntryDesc(const PagePos& pp);
protected:
	static int __cdecl Compare(const void* p1, const void* p2, size_t cb2);
};

ENUM_CLASS(PageAlloc){
	Zero, Nothing, Leaf, Branch, Copy,
	Move // Copy and Free source
} END_ENUM_CLASS(PageAlloc);

class DBLITE_CLASS DbTransactionBase : noncopyable, public ITransactionable {
public:
	KVStorage& Storage;

	int64_t TransactionId;
	Page MainTableRoot;

	typedef map<string, ptr<PagedMap>> CTables; // map<> has faster dtor than unordered_map<>
	CTables Tables;
protected:
	ptr<SnapshotGenObj> SnapshotGen;
	shared_lock<KVStorage> m_shlk;
	CBool m_bComplete, m_bError;
public:
	const bool ReadOnly;

	DbTransactionBase(KVStorage& storage); // ReadOnly ctor
	virtual ~DbTransactionBase();
	virtual Page OpenPage(uint32_t pgno);
protected:
	DbTransactionBase(KVStorage& storage, bool bReadOnly);
	void InitReadOnly();
	void Commit() override;
	void Rollback() override;

	void BeginTransaction() override { Throw(E_FAIL); }

	friend class BTreeCursor;
};

typedef DbTransactionBase DbReadTransaction;

class DBLITE_CLASS DbTransaction : public DbTransactionBase {
	typedef DbTransactionBase base;

	unique_lock<mutex> m_lockWrite;
	Blob TmpPageSpace;
	CUnorderedPageSet
		AllocatedPages;			// Pages allocated in the current transaction
//		ReleasedPages;			// Pages allocated in previous transactions but released in this. Must be unmodified until next Checkpoint.

	vector<uint32_t>
		PagesToFree,
		PagesFreeAfterCheckpoint;
public:
	CBool Bulk;

	DbTransaction(KVStorage& storage, bool bReadOnly = false);
	~DbTransaction();
	DbTransaction& Current();
	BTree& Table(RCString name);
	Page Allocate(PageAlloc pa, Page* pCopyFrom = 0);
	vector<uint32_t> AllocatePages(int n);
	Page OpenPage(uint32_t pgno) override;
	void Commit() override;
	void Rollback() override;
private:
	void FreePage(uint32_t pgno);
	void FreePage(const Page& page) { FreePage(page.N); }
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

	string Name;
	TableType Type;
	HashType HtType;
	uint8_t KeySize; // 0=variable

	DbTable(const string& name = nullptr, uint8_t keySize = 0, TableType type = TableType::BTree, HashType htType = HashType::MurmurHash3)
		: Name(name)
		, Type(type)
		, HtType(htType)
		, KeySize(keySize)
	{
	}

	void Open(DbTransaction& tx, bool bCreate = false);
	void Close() {}
	void Drop(DbTransaction& tx);
	void Put(DbTransaction& tx, RCSpan k, RCSpan d, bool bInsert = false);
	bool Delete(DbTransaction& tx, RCSpan k);
private:
	void CheckKeyArg(RCSpan k);
};

class CKVStorageKeeper {
	KVStorage* m_prev;
public:
	CKVStorageKeeper(KVStorage* cur);
	~CKVStorageKeeper();
};

inline void* PageObj::GetAddress() {
	return m_address ? m_address : m_address = View->Storage.GetPageAddress(View.get(), N);
}

#ifdef _DEBUG //!!!D
void CheckPage(Page& page);
#endif

class FlushThread : public Thread {
	typedef Thread base;

public:
	AutoResetEvent m_ev;

protected:
	void Execute() override;

	void Stop() override {
		base::Stop();
		m_ev.Set();
	}
};

typedef IntrusiveList<ViewBase> CLruViewList;

class GlobalLruViewCache : public CLruViewList {
public:
	mutex Mtx;
	condition_variable Cv;

	ptr<ViewBase> m_viewProcessed;
private:
	mutex m_mtxThread;
	ptr<FlushThread> Thread;
	//!!!R volatile int32_t RefCount;
	atomic<int32_t> m_aRefCount;
public:

	GlobalLruViewCache() : m_aRefCount(0) {}

	~GlobalLruViewCache();
	void AddRef();
	void Release();

	void WakeUpFlush() { Thread->m_ev.Set(); }
private:
	void Execute();

	friend class FlushThread;
};

extern InterlockedSingleton<GlobalLruViewCache> g_lruViewCache;

}}} // Ext::DB::KV::
