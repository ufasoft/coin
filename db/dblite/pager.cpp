#include <el/ext.h>

#include "dblite.h"
#include "dblite-file-format.h"
#include "b-tree.h"

namespace Ext { namespace DB { namespace KV {


const int RESERVED_FILE_SPACE = 8192;

static const char s_Magic[] = UDB_MAGIC;


EXT_THREAD_PTR(KVStorage, t_pKVStorage);

CKVStorageKeeper::CKVStorageKeeper(KVStorage *cur) {
	m_prev = t_pKVStorage;
	t_pKVStorage = cur;
}

CKVStorageKeeper::~CKVStorageKeeper() {
	t_pKVStorage = m_prev;
}


KVStorage& Stg() {
	return *t_pKVStorage;
}


KVStorage::KVStorage()
	:	ViewSize(DEFAULT_VIEW_SIZE)
	,	MainTableRoot(nullptr)
	,	m_pfnProgress(0)
//!!!R	,	ReaderRefCount(0)
//!!!R	,	PageCache(1024)
	,	PageCacheSize(DB_DEFAULT_PAGECACHE_SIZE)
	,	NewPageCount(0)
	,	Durability(true)
	,	CheckpointPeriod(TimeSpan::FromMinutes(1))
	,	ProtectPages(true)
	,	m_bOpened(false)
	,	m_alignedDbHeader(RESERVED_FILE_SPACE, 4096)		//!!! SectorSize
{
	Init();
}

void KVStorage::Init() {
	PageSize = DEFAULT_PAGE_SIZE;
	FileIncrement = ViewSize/2;
}

KVStorage::~KVStorage() {
	Close();					// explicit Close to flush last transaction
}

void KVStorage::WriteHeader() {
	DbHeader& header = DbHeaderRef();
	memset(&header, 0, RESERVED_FILE_SPACE);
	memcpy(header.Magic, s_Magic, strlen(s_Magic));
	memcpy(header.AppName, AppName.c_str(), std::min(12, (int)strlen(AppName.c_str())));
	header.UserVersion = UInt32((UserVersion.Major << 16)|UserVersion.Minor);

	memcpy(header.FrontEndName, FrontEndName.c_str(), std::min(12, (int)strlen(FrontEndName.c_str())));
	header.FrontEndVersion = UInt32((FrontEndVersion.Major << 16)|FrontEndVersion.Minor);

	header.Version = htole(UDB_VERSION);
	header.PageSize = UInt16(PageSize==65536 ? 1 : PageSize);

	header.PageCount = PageCount;
	
	DbFile.Write(&header, sizeof header);
	
	FileLength = RESERVED_FILE_SPACE;
}

void KVStorage::MapMeta() {
//	MMFile = MemoryMappedFile::CreateFromFile(DbFile, nullptr, ViewSize);
	DbHeader& header = DbHeaderRef();
	if (UInt32 pgnoRoot = header.LastTxes[0].MainDbPage)	
		MainTableRoot = OpenPage(pgnoRoot);
}

void KVStorage::AddFullMapping(UInt64 fileLength) {
	EXT_LOCK (MtxViews) {
		ptr<MappedFile> m = new MappedFile;
		m->MemoryMappedFile = MemoryMappedFile::CreateFromFile(DbFile, nullptr, FileLength = fileLength);
		Mappings.push_back(m);
	}
}

#if UCFG_DB_FREE_PAGES_BITSET
void KVStorage::UpdateFreePagesBitset() {
	FreePagesBitset.reset();
	EXT_FOR (UInt32 pgno, FreePages) {
		if (pgno >= FreePagesBitset.size())
			FreePagesBitset.resize(pgno+1);
		FreePagesBitset.set(pgno);
	}
}
#endif // UCFG_DB_FREE_PAGES_BITSET

void KVStorage::Create(RCString filepath) {;
	File::OpenInfo oi(FilePath = filepath);
	oi.Mode = FileMode::Open;
	oi.RandomAccess = true;
//	oi.BufferingEnabled = false;
	if (!File::Exists(filepath) || FileInfo(filepath).Length != 0)
		oi.Mode = FileMode::CreateNew;
	DbFile.Open(oi);
		
	PageCount = 2;
	OpenedPages.resize(PageCount);
#if UCFG_DB_FREE_PAGES_BITSET
	FreePagesBitset.resize(PageCount);
#endif

	WriteHeader();
	DbFile.Flush();
	MapMeta();
	m_bOpened = true;
	m_dtPrevCheckpoint = DateTime::UtcNow();
}

void KVStorage::Open(RCString filepath) {
	File::OpenInfo oi(FilePath = filepath);
	oi.Mode = FileMode::Open;
	oi.RandomAccess = true;
//	oi.BufferingEnabled = false;
	DbFile.Open(oi);
	FileLength = DbFile.Length;
	DbHeader& header = DbHeaderRef();
	DbFile.Read(&header, 4096);							//!!! SectorSize
	if (memcmp(header.Magic, s_Magic, strlen(s_Magic)))
		Throw(E_EXT_DB_Corrupt);
	UInt32 ver = letoh(header.Version);
	if (ver > UDB_VERSION)
		Throw(E_EXT_DB_Version);
	if (ver != UDB_VERSION)		//!!!
		Throw(E_EXT_DB_Corrupt);
	AppName = header.AppName;
	UInt32 uver = header.UserVersion;
	UserVersion = Version(uver>>16, uver & 0xFFFF);

	FrontEndName = header.FrontEndName;
	UInt32 fever = header.FrontEndVersion;
	FrontEndVersion = Version(fever>>16, fever & 0xFFFF);

	PageSize = header.PageSize==1 ? 65536 : int(header.PageSize);
	PageCount = header.PageCount;
	if (FileLength < UInt64(PageCount)*PageSize)
		Throw(E_EXT_DB_Corrupt);

	OpenedPages.resize(PageCount);
#if UCFG_DB_FREE_PAGES_BITSET
	FreePagesBitset.resize(PageCount);
#endif

	UInt64 newFileLength = (FileLength + ViewSize - 1) & ~(UInt64(ViewSize) - 1);
	AddFullMapping(newFileLength);
	MapMeta();
	
	FreePages.insert(begin(header.FreePages), std::find(begin(header.FreePages), end(header.FreePages), 0));

#if UCFG_DBLITE_DEBUG_VERBYTE
	for (int i=0; i<_countof(header.FreePages); ++i) {
		ASSERT(header.FreePages[i] < PageCount);
	}
#endif
	for (UInt32 pgno=header.FreePagePoolList; pgno; ) {
		if (pgno >= PageCount)
			Throw(E_EXT_DB_Corrupt);
		Page page = OpenPage(pgno);
		m_nextFreePages.insert(pgno);
		BeUInt32 *p = (BeUInt32*)page.get_Address();
		pgno = p[0];
		for (BeUInt32 *q=p+1, *e=&p[PageSize/4]; q!=e; ++q) {
			if (UInt32 pn = *q) {
				if (!FreePages.insert(pn).second)
					Throw(E_EXT_DB_Corrupt);
			} else if (pgno) {
#ifdef _DEBUG
				Throw(E_EXT_DB_Corrupt);		// compatibility with old DBLite versions
#endif
			}
		}
	}
	EXT_FOR (UInt32 pgno, FreePages) {
		if (pgno >= PageCount)
			Throw(E_EXT_DB_Corrupt);
	}

#if UCFG_DB_FREE_PAGES_BITSET
	UpdateFreePagesBitset();
#endif
#ifdef _DEBUG//!!!D
	EXT_FOR (UInt32 pgno, m_nextFreePages) {
		ASSERT(!FreePages.count(pgno));
		pgno = pgno;
		ASSERT(!FreePagesBitset[pgno]);
	}
#endif
	m_bOpened = true;
	m_dtPrevCheckpoint = DateTime::UtcNow();
}

void KVStorage::FreePage(UInt32 pgno) {
	FreePages.insert(pgno);
#if UCFG_DB_FREE_PAGES_BITSET
	FreePagesBitset.set(pgno);
#endif
}

void KVStorage::lock_shared() {
	ShMtx.lock_shared();
}

void KVStorage::unlock_shared() {
	ShMtx.unlock_shared();
	unique_lock<shared_mutex> lk(ShMtx, try_to_lock);
	if (lk) {
		EXT_LOCK (MtxFreePages) {
			EXT_FOR (UInt32 pgno, ReaderLockedPages) {
				FreePage(pgno);
			}
			ReaderLockedPages.clear();
		}
	}
}

UInt32 KVStorage::NextFreePgno(COrderedPgNos& newFreePages) {		// .second==true  if pgno from ReleasedPages
	UInt32 r = 0;
	if (!ReleasedPages.empty()) {
		CReleasedPages::iterator it = ReleasedPages.begin();
		ReaderLockedPages.insert(r = *it);
		ReleasedPages.erase(it);
	} else if (!FreePages.empty()) {
		CFreePages::iterator it = FreePages.begin();
		newFreePages.insert(r = *it);
		FreePages.erase(it);
#if UCFG_DB_FREE_PAGES_BITSET
		FreePagesBitset.reset(r);
#endif
	}
	return r;
}

bool KVStorage::DoCheckpoint(bool bLock) {
	if (!m_bModified)
		return false;
	TRC(1, "");

#ifdef X_DEBUG//!!!D
	LogObjectCounters();
#endif

	unique_lock<mutex> lk(MtxWrite, defer_lock);
	if (bLock)
		lk.lock();
	ASSERT(!MtxWrite.try_lock());

	DbHeader& h = DbHeaderRef();
	h.ChangeCounter = h.ChangeCounter + 1;
	h.LastTransactionId = htole(LastTransactionId);
	ZeroStruct(h.FreePages);
	TxRecord& rec = h.LastTxes[0];
	rec.Id = h.LastTransactionId;
	rec.MainDbPage = MainTableRoot.N;

	COrderedPgNos newFreePages;
	m_nextFreePages.swap(newFreePages);
	{
		shared_lock<KVStorage> shlk(_self, defer_lock);
		if (bLock)
			shlk.lock();
		EXT_LOCK (MtxFreePages) {
			h.FreePagePoolList = 0;
			Page pagePrev;
			BeUInt32 *pNextTrunk = &h.FreePagePoolList;
			const UInt32 PGNOS_IN_PAGE = PageSize / 4;
			while (FreePages.size() + ReleasedPages.size() > _countof(h.FreePages)) {
				Page page = Allocate(false);
				m_nextFreePages.insert(page.N);
				BeUInt32 *p = (BeUInt32*)page.get_Address();
				p[0] = 0;
				*exchange(pNextTrunk, p) = page.N;
				pagePrev = page;
				UInt32 i;
				for (i=1; i<PGNOS_IN_PAGE && (p[i]=NextFreePgno(newFreePages)); ++i)
					;
				memset(p+i, 0, PageSize-(i*4));
			}
			for (UInt32 i=0; h.FreePages[i]=NextFreePgno(newFreePages); ++i)
				;
			h.FreePageCount = newFreePages.size();
			FreePages.swap(newFreePages);

#if UCFG_DB_FREE_PAGES_BITSET
			UpdateFreePagesBitset();
#endif

			AllocatedSinceCheckpointPages.reset();
		}

		EXT_LOCK (MtxViews) {
			EXT_FOR (CViews::value_type& kv, Views) {
				kv.second->View.Flush();				// Flush View syncs to the disk, FlushFileBuffers() is not enough
			}
		}		
		DbFile.Flush();
		h.PageCount = PageCount;					// PageCount can be changed during allocating FreePool, so save it last
		DbFile.Write(&h, 4096, 0);					//!!! SectorSize
		DbFile.Flush();
	}
	m_dtPrevCheckpoint = DateTime::UtcNow();
	m_bModified = false;
	return true;
}

typedef IntrusiveList<MMView> CLruViewList;

class GlobalLruViewCache : public CLruViewList {
public:
	mutex Mtx;
};

static InterlockedSingleton<GlobalLruViewCache> g_lruViewCache;

Page KVStorage::OpenPage(UInt32 pgno) {
	ASSERT(pgno);

#ifdef _DEBUG//!!!D
	ASSERT(!m_nextFreePages.count(pgno));
#endif

	EXT_LOCK (MtxViews) {
		if (pgno >= OpenedPages.size())
			Throw(E_EXT_DB_Corrupt);

		Page r;
		PageObj *po = OpenedPages[pgno];
		if (po)
			r = po;
		else {
			if (NewPageCount > PageCacheSize) {
				int count = 0;
				for (size_t i=0, sz=OpenedPages.size(); i<sz; ++i) {
					if (PageObj* poj = OpenedPages[i]) {
						if (poj->m_dwRef != 1)
							++count;
						else if (!exchange(poj->Live, false)) {
							CCounterIncDec<PageObj, Interlocked>::Release(poj);
							OpenedPages[i] = 0;
						}
					}
				}
				if (count > PageCacheSize/2)
					PageCacheSize *= 2;
				NewPageCount = 0;
			}

			r = po = new PageObj(_self);
			++NewPageCount;
			UInt32 vno = pgno / (ViewSize/PageSize);
			GlobalLruViewCache& lruView = *g_lruViewCache;
			CViews::iterator it = Views.find(vno);
			if (it != Views.end()) {
				MMView *mmview = (po->View = it->second).get();
				unique_lock<mutex> lk(lruView.Mtx, try_to_lock);
				if (lk) {
					lruView.erase(CLruViewList::const_iterator(mmview));
					lruView.push_back(*mmview);
				}
			} else {
				ptr<MMView> mmview = new MMView(_self);
				mmview->N = vno;
				mmview->View = Mappings.back()->MemoryMappedFile.CreateView(UInt64(vno)*ViewSize, ViewSize, MemoryMappedFileAccess::ReadWrite);
				if (ProtectPages)
					MemoryMappedView::Protect(mmview->View.Address, ViewSize, MemoryMappedFileAccess::Read);
				po->View = mmview;
				Views.insert(make_pair(vno, mmview));

				EXT_LOCK (lruView.Mtx) {
					lruView.push_back(*mmview.get());

					if (lruView.size() > DB_MAX_VIEWS) {
						int nDeleted = 0;
						for (CLruViewList::iterator ir=lruView.begin(); ir!=lruView.end() && nDeleted < DB_NUM_DELETE_FROM_LRUVIEW;) {
							MMView& mmv = *ir;
							if (1 != mmv.m_dwRef)
								++ir;
							else {
								unique_lock<recursive_mutex> lk(mmv.Storage.MtxViews, defer_lock);
								if (&mmv.Storage != this) {
									if (!lk.try_lock() || mmv.m_dwRef!=1) {
										++ir;
										continue;
									}
								}
								lruView.erase(ir++);
								mmv.Storage.Views.erase(mmv.N);
								++nDeleted;
							}
						}
					}
				}
			}
			po->N = pgno;
			po->Address = (byte*)po->View->View.Address + UInt64(pgno & (ViewSize/PageSize - 1))*PageSize;
			Interlocked::Increment((OpenedPages[pgno] = po)->m_dwRef);
		}
		po->Live = true;
		ASSERT(r);//!!!D
		return r;
	}
}

UInt32 KVStorage::TryAllocateMappedFreePage() {
	EXT_LOCK (MtxViews) {
		int q = ViewSize/PageSize;
		ASSERT (q % 8 == 0);
		for (CViews::iterator it=Views.begin(), e=Views.end(); it!=e; ++it) {
			UInt32 pgBeg = it->first*q;
#if UCFG_DB_FREE_PAGES_BITSET
			UInt32 pgEnd=pgBeg+q;
/*!!! bitset::find_ has O(n) complexity
			CFreePagesBitset::size_type pgnoFree = pgno ? FreePagesBitset.find_next_set(pgno-1) : FreePagesBitset.find_first_set();
			if (pgnoFree != CFreePagesBitset::npos && pgnoFree < pgEnd) {
				FreePagesBitset.reset(pgnoFree);
				FreePages.erase(pgnoFree);
				return pgnoFree;
			} else if (pgnoFree == CFreePagesBitset::npos && pgEnd > PageCount) {
				UInt32 r = FreePagesBitset.size();
				FreePagesBitset.push_back(false);
				for (UInt32 i=PageCount; i<r; ++i)
					FreePage(i);
				OpenedPages.resize(PageCount = r+1);
				return r;
			}
*/

			UInt32 step=min(64, q), pgno=pgBeg;
			UInt64 mask = (UInt64(1) << step) - 1; 
			for (; pgno<pgEnd; pgno+=step) {
				if (pgno+step > PageCount || (mask & ToUInt64AtBytePos(FreePagesBitset, pgno & ~(step-1)))) {
					for (UInt32 j=pgno; j<pgno+step; ++j) {
						if (j>=PageCount || FreePagesBitset[j]) {
							if (j >= PageCount) {
								FreePagesBitset.resize(j+1);
								for (UInt32 i=PageCount; i<j; ++i)
									FreePage(i);
								OpenedPages.resize(PageCount = j+1);
							} else {
								FreePagesBitset.reset(j);
								FreePages.erase(j);
							}
							return j;
						}
					}
					Throw(E_EXT_CodeNotReachable);
				}
			}
#else
			COrderedPgNos::iterator itFree = FreePages.lower_bound(pgBeg);
			if (itFree != FreePages.lower_bound(pgBeg+q)) {
				UInt32 r = *itFree;
				FreePages.erase(itFree);
				return r;
			}
#endif
		}
	}
	return 0;
}

void KVStorage::MarkAllocatedPage(UInt32 pgno) {
	if (AllocatedSinceCheckpointPages.size() < pgno+1)
		AllocatedSinceCheckpointPages.resize(std::max(pgno+1, (UInt32)AllocatedSinceCheckpointPages.size()*2)); 	//!!!TODO 2^32 limitation
	AllocatedSinceCheckpointPages.set(pgno);
}

Page KVStorage::Allocate(bool bLock) {
	m_bModified = true;
	UInt32 pgno = 0;
	{
		unique_lock<mutex> lk(MtxFreePages, defer_lock);
		if (bLock)
			lk.lock();

		if (!FreePages.empty()) {
#if UCFG_DB_FREE_PAGES_BITSET
			if (!(pgno = TryAllocateMappedFreePage())) {

#else
			if (true) {
#endif
				pgno = *FreePages.begin();
				FreePages.erase(FreePages.begin());
#if UCFG_DB_FREE_PAGES_BITSET
				FreePagesBitset.reset(pgno);
#endif
			}
		}
		if (pgno) {
			MarkAllocatedPage(pgno);
			goto LAB_ALLOCATED;
		}
		MarkAllocatedPage(pgno = PageCount++);
#if UCFG_DB_FREE_PAGES_BITSET
		FreePagesBitset.resize(PageCount);
#endif
	}
	EXT_LOCK (MtxViews) {				//!!!?
		OpenedPages.push_back(nullptr);
	}
	if (FileLength < UInt64(PageCount)*PageSize) {
		FileIncrement *= 2;
		AddFullMapping((UInt64(PageCount) * PageSize + FileIncrement - 1) & ~(FileIncrement - 1));
		DbFile.Flush();			// save Metadata

#ifdef X_DEBUG//!!!D
		LogObjectCounters();
#endif
	}
LAB_ALLOCATED:
	Page r = OpenPage(pgno);
	if (ProtectPages)
		MemoryMappedView::Protect(r.Address, PageSize, MemoryMappedFileAccess::ReadWrite);
	return r;
}

void KVStorage::Close(bool bLock) {
	if (!m_bOpened)
		return;

//!!!?	if (ReaderRefCount)
//!!!		Throw(E_FAIL);

	DoCheckpoint(bLock);
	MainTableRoot = Page(nullptr);

	EXT_FOR (PageObj *po, OpenedPages) {
		if (po) {
			ASSERT(po->m_dwRef == 1);
			CCounterIncDec<PageObj, Interlocked>::Release(po);
		}
	}
	OpenedPages.clear();

	EXT_LOCK (MtxViews) {				// can be accessed from other storages
		GlobalLruViewCache& lruView = *g_lruViewCache;
		EXT_LOCK (lruView.Mtx) {
			EXT_FOR (const CViews::value_type& kv, Views) {
				lruView.erase(CLruViewList::const_iterator(kv.second.get()));
			}
		}
		Views.clear();
	}

	Mappings.clear();
	DbFile.Length = UInt64(PageCount) * PageSize;
	DbFile.Close();
	m_bOpened = m_bModified = false;
	m_nextFreePages.clear();
	ReleasedPages.clear();

	FreePages.clear();
#if UCFG_DB_FREE_PAGES_BITSET
	FreePagesBitset.clear();
#endif

	Init();
}

MMView::~MMView() {
	View.Flush();		// Flush View syncs to the disk, FlushFileBuffers() is not enough
}

PageObj::PageObj(KVStorage& storage)
	:	Storage(storage)
	,	N(0)
	,	Entries(nullptr)
	,	Overflows(0)
{
}

PageObj::~PageObj() {
	if (Storage.ProtectPages && Dirty)
		MemoryMappedView::Protect(Address, Storage.PageSize, MemoryMappedFileAccess::Read);

	Ext::Free(Entries);
}

bool Page::get_IsBranch() const {
	return Header().Flags & PAGE_FLAG_BRANCH;
}

void Page::ClearEntries() const {
	Ext::Free(exchange(m_pimpl->Entries, nullptr));
}

DbCursor::DbCursor(DbCursor& c)
	:	Tree(c.Tree)
	,	Path(c.Path)
	,	Initialized(c.Initialized)
	,	Eof(c.Eof)
	,	IsDbDirty(c.IsDbDirty)
{
	Tree->Cursors.push_back(_self);
}

DbCursor::DbCursor(BTree& tree)
	:	Tree(&tree)
{
	Path.reserve(4);
	Tree->Cursors.push_back(_self);
}

DbCursor::DbCursor(DbCursor& c, bool bRight, Page& pageSibling)
	:	Tree(c.Tree)
	,	Path(c.Path)
	,	Initialized(c.Initialized)
	,	Eof(c.Eof)
	,	IsDbDirty(c.IsDbDirty)
{
	Tree->Cursors.push_back(_self);
	Path[Path.size()-2].Pos += bRight ? 1 : -1;
	Top() = PagePos(pageSibling, bRight ? 0 : NumKeys(pageSibling));
}

DbCursor::~DbCursor() {
//!!!R	CKVStorageKeeper keeper(&Tree->Tx.Storage);

	Tree->Cursors.erase(IntrusiveList<DbCursor>::const_iterator(this));
//!!!R	Path.clear();																	// explicit to be in scope of t_pKVStorage 
}

void DbCursor::Touch() {
	if (Tree->Name != DbTable::Main.Name) {
		DbCursor cM(Tree->Tx, DbTable::Main);
		if (!cM.PageSearch(ConstBuf(Tree->Name.c_str(), strlen(Tree->Name.c_str())), true))
			Throw(E_FAIL);
		Tree->Dirty = true;
	}
	for (int i=0; i<Path.size(); ++i)
		PageTouch(i);
}

bool DbCursor::PageSearch(const ConstBuf& k, bool bModify) {
	if (Tree->Tx.m_bError)
		Throw(E_FAIL);

	if (!Tree->Root)
		return false;

	if (Tree->Name != DbTable::Main.Name && bModify && !IsDbDirty) {
		DbCursor cMain(Tree->Tx, DbTable::Main);
		const char *name = Tree->Name;
		if (!cMain.PageSearch(ConstBuf(name, strlen(name)), bModify))
			Throw(E_FAIL);
		IsDbDirty = bModify;
	}

	Path.resize(1);
	Path[0].Page = Tree->Root;
	Path[0].Pos = 0;
	if (bModify)
		PageTouch(Path.size()-1);
	return PageSearchRoot(k, bModify);
}

bool DbCursor::SeekToFirst() {
	if (!Initialized || Path.size() > 1) {
		if (!PageSearch(ConstBuf()))
			return false;
	}
	ASSERT(!Top().Page.IsBranch);
	Initialized = true;
	Top().Pos = 0;
	ClearKeyData();
	return true;
}

bool DbCursor::SeekToLast() {
	if (!Eof) {
		if (!Initialized || Path.size() > 1) {
			if (!PageSearch(ConstBuf(0, INT_MAX)))
				return false;
		}
		ASSERT(!Top().Page.IsBranch);
		Top().Pos = NumKeys(Top().Page)-1;
		Initialized = Eof = true;
	}
	ClearKeyData();
	return true;
}

bool DbCursor::SeekToNext() {
	if (!Initialized)
		return SeekToFirst();
	if (Eof)
		return false;

	PagePos& pp = Top();
	if (pp.Pos+1 < NumKeys(pp.Page))
		pp.Pos++;
	else if (!SeekToSibling(true)) {
		Eof = true;
		return Initialized = false;		
	}
		
//	ASSERT(!Top().Page.IsBranch);
	ClearKeyData();
	return true;
}

bool DbCursor::SeekToPrev() {
	if (!Initialized)
		return SeekToLast();

	PagePos& pp = Top();
	if (pp.Pos > 0)
		pp.Pos--;
	else if (!SeekToSibling(false))
		return Initialized = false;
	Top().Pos = NumKeys(Top().Page)-1;
	Eof = false;

	ASSERT(!Top().Page.IsBranch);
	ClearKeyData();
	return true;
}

bool DbCursor::Seek(CursorPos cPos, const ConstBuf& k) {
	switch (cPos) {
	case CursorPos::First:
		return SeekToFirst();
	case CursorPos::Last:
		return SeekToLast();
	case CursorPos::Next:
		return SeekToNext();
	case CursorPos::Prev:
		if (!Initialized || Eof)
			return SeekToLast();
		else
			return SeekToPrev();
	case CursorPos::FindKey:
		return SeekToKey(k);
	default:
		Throw(E_INVALIDARG);
	}
}

DbTransaction::DbTransaction(KVStorage& storage, bool bReadOnly)
	:	Storage(storage)
	,	ReadOnly(bReadOnly)
	,	m_lockWrite(storage.MtxWrite, defer_lock)
	,	m_shlk(storage, defer_lock)
{
	if (ReadOnly) {
		m_shlk.lock();		
		EXT_LOCK (Storage.MtxRoot) {
			TransactionId = Storage.LastTransactionId;
			MainTableRoot = Storage.MainTableRoot;
		}
	} else {
		m_lockWrite.lock();
		TransactionId = Storage.LastTransactionId;
		MainTableRoot = Storage.MainTableRoot;
	}
}

DbTransaction::~DbTransaction() {
	CKVStorageKeeper keeper(&Storage);

	if (!m_bComplete)
		Rollback();

	//Tables.clear();			// explicit to be in scope of t_pKVStorage 
}

Page DbTransaction::Allocate(bool bBranch) {
	Page r = Storage.Allocate();
	r.ClearEntries(); //!!!?
	r.m_pimpl->Dirty = true;
	AllocatedPages.insert(r.N);
	r.m_pimpl->Dirty = true;
	PageHeader& h = r.Header();

	h.Num = 0;
	h.Flags = (bBranch ? PAGE_FLAG_BRANCH : 0);
	ASSERT(!r.m_pimpl->Entries);
	ASSERT(!r.m_pimpl->Overflows);
	return r;
}

Page DbTransaction::OpenPage(UInt32 pgno) {
	Page r = Storage.OpenPage(pgno);
//!!!R	ASSERT(!r.m_pimpl->IsDeleted);
	r.m_pimpl->Dirty = AllocatedPages.count(pgno);
	if (r.m_pimpl->Dirty && Storage.ProtectPages)
		MemoryMappedView::Protect(r.m_pimpl->Address, Storage.PageSize, MemoryMappedFileAccess::ReadWrite);
	return r;	
}

void DbTransaction::FreePage(UInt32 pgno) {
	if (AllocatedPages.erase(pgno)) {
		EXT_LOCKED(Storage.MtxFreePages, Storage.FreePage(pgno));
	} else
		ReleasedPages.insert(pgno);
}

vector<UInt32> DbTransaction::AllocatePages(int n) {
	vector<UInt32> r;
	r.reserve(n);
	struct FreeReleaser {
		DbTransaction *Tx;
		vector<UInt32> *Pages;

		~FreeReleaser() {
			if (Pages)
				for (int i=0; i<Pages->size(); ++i)
					Tx->FreePage((*Pages)[i]);
		}
	} pf = { this, &r };
	for (int i=0; i<n; ++i)
		r.push_back(Allocate().N);
	pf.Pages = 0;
	return r;
}

void DbTransaction::FreePage(const Page& page) {
	FreePage(page.N);
}

void DbTransaction::Complete() {
	if (ReadOnly)
		m_shlk.unlock();
	else
		m_lockWrite.unlock();
	m_bComplete = true;
}

void DbTransaction::Rollback() {
	if (!ReadOnly) {
		EXT_LOCK (Storage.MtxFreePages) {
			EXT_FOR(UInt32 pgno, AllocatedPages) {
				Storage.AllocatedSinceCheckpointPages.reset(pgno);
			}
			EXT_FOR (UInt32 pgno, AllocatedPages) {
				Storage.FreePage(pgno);
			}
		}
	}
	Complete();
}

void DbTransaction::Commit() {
	CKVStorageKeeper keeper(&Storage);

	if (!ReadOnly) {
		TRC(6, "");

		if (m_bError) {
			Rollback();
			Throw(E_EXT_DB_InternalError);
		}

		for (CTables::iterator it=Tables.begin(), e=Tables.end(); it!=e; ++it) {
			BTree& btree = it->second;
			if (btree.Dirty && btree.Name != DbTable::Main.Name) {
				TableData td;
				td.RootPgNo = htole(btree.Root.N);
				td.KeySize = btree.KeySize;
				ConstBuf k(btree.Name.c_str(), strlen(btree.Name.c_str()));
				DbCursor(_self, DbTable::Main).Put(k, ConstBuf(&td, sizeof td));
			}
		}
			
		EXT_LOCK (Storage.MtxRoot) {
			Storage.MainTableRoot = MainTableRoot;
			Storage.LastTransactionId = TransactionId;
		}

		EXT_LOCK (Storage.MtxViews) {
			EXT_FOR (UInt32 pgno, AllocatedPages) {
				if (PageObj *po = Storage.OpenedPages[pgno]) {
//!!!?					if (Storage.ProtectPages && po.Dirty)
//						MemoryMappedView::Protect(po.Address, Storage.PageSize, MemoryMappedFileAccess::Read);
					po->Dirty = false;
				}
			}
				//	for (KVStorage::COpenedPages::iterator it=Storage.OpenedPages.begin(), e=Storage.OpenedPages.end(); it!=e; ++it)
				//	it->second->Dirty = false;
		}
		AllocatedPages.clear();
		
		{
			unique_lock<shared_mutex> lk(Storage.ShMtx, try_to_lock);
			EXT_LOCK (Storage.MtxFreePages) {
				EXT_FOR (UInt32 pgno, ReleasedPages) {
					if (Storage.Durability || pgno >= Storage.AllocatedSinceCheckpointPages.size() || !Storage.AllocatedSinceCheckpointPages[pgno])
						Storage.ReleasedPages.insert(pgno);
					else if (lk) {
//!!!R						Storage.AllocatedSinceCheckpointPages.reset(pgno);
						Storage.FreePage(pgno);
					} else
						Storage.ReaderLockedPages.insert(pgno);
				}
			}
		}
		ASSERT(m_lockWrite.owns_lock());
		if (Storage.Durability || DateTime::UtcNow() >= Storage.m_dtPrevCheckpoint + Storage.CheckpointPeriod) {
			shared_lock<KVStorage> shlk(Storage);
			Storage.DoCheckpoint(false);
		}
	}
	Complete();
}

DbTable DbTable::Main("__main");

}}} // Ext::DB::KV::


