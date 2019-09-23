/*######   Copyright (c) 2014-2015 Ufasoft  http://ufasoft.com  mailto:support@ufasoft.com,  Sergey Pavlov  mailto:dev@ufasoft.com ####
#                                                                                                                                     #
# 		See LICENSE for licensing information                                                                                         #
#####################################################################################################################################*/

#include <el/ext.h>

#include "dblite.h"
#include "dblite-file-format.h"
#include "b-tree.h"


namespace Ext { namespace DB { namespace KV {

// Params
const uint32_t RESERVED_FILE_SPACE = 8192;
const int FLUSH_WAIT_MS = 100000;

static const char s_Magic[] = UDB_MAGIC;

extern bool g_UseLargePages;

EXT_THREAD_PTR(KVStorage) t_pKVStorage;

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

void Pager::Flush() {
	EXT_FOR(KVStorage::CViews::value_type& kv, Storage.Views) {
		if (kv)
			kv->Flush();				// Flush View syncs to the disk, FlushFileBuffers() is not enough
	}
}


void GlobalLruViewCache::AddRef() {
	if (++m_aRefCount == 1) {
		EXT_LOCK(m_mtxThread) {
			if (!Thread)
				(Thread = new FlushThread)->Start();
		}
	}
}

GlobalLruViewCache::~GlobalLruViewCache() {
}

void GlobalLruViewCache::Release() {
	if (!--m_aRefCount) {
		ASSERT(empty());
		EXT_LOCK(m_mtxThread) {
			Thread->interrupt();
			Thread->Join();
			Thread = nullptr;
		}
	}
}

void GlobalLruViewCache::Execute() {
	for (int nDeleted=0; !Thread->m_bStop;) {
		EXT_LOCK(Mtx) {
			if (size() > DB_MAX_VIEWS || nDeleted < DB_NUM_DELETE_FROM_CACHE) {
				for (CLruViewList::iterator ir=begin(); ir!=end();) {
					ViewBase& mmv = *ir;
					if (1 != mmv.m_aRef)
						++ir;
					else {
						m_viewProcessed = &mmv;
						m_viewProcessed->Removed = false;
						++nDeleted;
						goto LAB_FOUND;
					}
				}
			}
		}
		nDeleted = 0;
		Cv.notify_all();
		if (!m_viewProcessed)
			Thread->m_ev.lock(FLUSH_WAIT_MS);
		else {
LAB_FOUND:
			KVStorage& stg = m_viewProcessed->Storage;
			m_viewProcessed->Flush();
			bool bNotifyStorage = false;
			EXT_LOCK(Mtx) {
				if (m_viewProcessed->Flushed && (2 == m_viewProcessed->m_aRef)) {
					if (m_viewProcessed->Removed)
						bNotifyStorage = true;
					else {
						erase(GlobalLruViewCache::const_iterator(m_viewProcessed.get()));
						EXT_LOCKED(stg.MtxViews, stg.Views[m_viewProcessed->N].reset());
					}
				}
				m_viewProcessed = nullptr;
			}
			if (bNotifyStorage)
				Cv.notify_all();
		}
	}
}

InterlockedSingleton<GlobalLruViewCache> g_lruViewCache;


void FlushThread::Execute() {
	Name = "FlushViewsThread";
	g_lruViewCache->Execute();
}


typedef SIZE_T (WINAPI *PFN_GetLargePageMinimum)();
DlProcWrap<PFN_GetLargePageMinimum> s_pfnGetLargePageMinimum("KERNEL32.DLL", "GetLargePageMinimum");


KVStorage::KVStorage()
	: ViewSize(DEFAULT_VIEW_SIZE)
	, MainTableRoot(nullptr)
	, m_pfnProgress(0)
//!!!R	, ReaderRefCount(0)
//!!!R	, PageCache(1024)
	, PageCacheSize(DB_DEFAULT_PAGECACHE_SIZE)
	, NewPageCount(0)
	, Durability(true)
	, UseFlush(true)
	, CheckpointPeriod(TimeSpan::FromSeconds(30))	//!!!T was 60
	, ProtectPages(true)
	, m_state(OpenState::Closed)
	, m_salt((uint32_t)Random().Next())
	, ViewAddress(0)
	, UseMMapPager(true)
	, AllowLargePages(g_UseLargePages)
	, PhysicalSectorSize(4096)
	, OldestAliveGen(0)
	, m_viewMode(UCFG_PLATFORM_X64 ? ViewMode::Full : ViewMode::Window)
{
	Init();
	g_lruViewCache->AddRef();
}

void KVStorage::Init() {
	PageSize = 0;
	FileIncrement = ViewSize / 2;
	m_accessViewMode = m_viewMode;
	m_alignedDbHeader.Free();
}

KVStorage::~KVStorage() {
	OpenState state = m_state;
	switch (state) {
	case OpenState::Closing:
		m_futClose.wait();
		break;
	case OpenState::Opened:
		DoClose(true);					// explicit Close to flush last transaction
		break;
	}
	g_lruViewCache->Release();

	ASSERT(OpenedPages.empty());
	ASSERT(FreePages.empty());
}

void KVStorage::WriteHeader() {
	DbHeader& header = DbHeaderRef();
	memset(&header, 0, HeaderSize);
	memcpy(header.Magic, s_Magic, strlen(s_Magic));
	memcpy(header.AppName, AppName.c_str(), std::min(12, (int)strlen(AppName.c_str())));
	header.UserVersion = uint32_t((UserVersion.Major << 16)|UserVersion.Minor);

	memcpy(header.FrontEndName, FrontEndName.c_str(), std::min(12, (int)strlen(FrontEndName.c_str())));
	header.FrontEndVersion = uint32_t((FrontEndVersion.Major << 16)|FrontEndVersion.Minor);

	header.Version = htole(UDB_VERSION);
	header.PageSize = uint16_t(PageSize == 65536 ? 1 : PageSize);
	header.Salt = Salt;
	header.PageCount = PageCount;

	DbFile.Write(&header, HeaderSize);

	FileLength = RESERVED_FILE_SPACE;
}

void KVStorage::CommonOpen() {
//	MMFile = MemoryMappedFile::CreateFromFile(DbFile, nullptr, ViewSize);
	DbHeader& header = DbHeaderRef();
	if (uint32_t pgnoRoot = header.LastTxes[0].MainDbPage)
		MainTableRoot = OpenPage(pgnoRoot);
	m_state = OpenState::Opened;
	DtNextCheckpoint = Clock::now() + CheckpointPeriod;
	CurGen = new SnapshotGenObj(_self);
}

#if UCFG_DB_FREE_PAGES_BITSET
void KVStorage::UpdateFreePagesBitset() {
	FreePagesBitset.reset();
	EXT_FOR (uint32_t pgno, FreePages) {
		if (pgno >= FreePagesBitset.size())
			FreePagesBitset.resize(pgno+1);
		FreePagesBitset.set(pgno);
	}
}
#endif // UCFG_DB_FREE_PAGES_BITSET

void KVStorage::PrepareCreateOpen() {
	if (m_state == OpenState::Closing)
		m_futClose.wait();
	if (UseMMapPager)
		m_pager = CreateMMapPager(_self);
	else {
		m_accessViewMode = m_viewMode = ViewMode::Window;
		UseFlush = true;
		if (s_pfnGetLargePageMinimum)
			ViewSize = s_pfnGetLargePageMinimum();
		m_pager = CreateBufferPager(_self);
	}
}

void KVStorage::Open(File::OpenInfo& oi) {
	PrepareCreateOpen();
	oi.Share = FileShare::Read;
	oi.Options = FileOptions::RandomAccess;
//	oi.BufferingEnabled = false;	//!!!T?
	DbFile.Open(oi);
	if (size_t physSec = DbFile.PhysicalSectorSize())
		PhysicalSectorSize = physSec;
	HeaderSize = (min)(PhysicalSectorSize, RESERVED_FILE_SPACE);
	m_alignedDbHeader.Alloc(HeaderSize, 4096);
}

void KVStorage::Create(const path& filepath) {
	File::OpenInfo oi(FilePath = filepath);
	oi.Mode = FileMode::Open;
	if (!exists(filepath) || file_size(filepath) != 0)
		oi.Mode = FileMode::CreateNew;
	Open(oi);
	if (!PageSize)
		SetPageSize(PhysicalSectorSize >= 4096 ? PhysicalSectorSize : DEFAULT_PAGE_SIZE);

	PageCount = 2;
	OpenedPages.resize(PageCount);
	Views.resize(1);
#if UCFG_DB_FREE_PAGES_BITSET
	FreePagesBitset.resize(PageCount);
#endif

	WriteHeader();
	DbFile.Flush();
	CommonOpen();
}

void KVStorage::Open(const path& filepath) {
	File::OpenInfo oi(FilePath = filepath);
	if (ReadOnly) {
		oi.Access = FileAccess::Read;
		oi.Share = FileShare::ReadWrite;
	}
	oi.Mode = FileMode::Open;
	Open(oi);

	FileLength = DbFile.Length;
	DbHeader& header = DbHeaderRef();
	DbFile.Read(&header, HeaderSize);							//!!! SectorSize
	if (memcmp(header.Magic, s_Magic, strlen(s_Magic)))
		Throw(ExtErr::DB_Corrupt);
	uint32_t ver = letoh(header.Version);
	if (ver > UDB_VERSION)
		Throw(ExtErr::DB_Version);
	if (ver < COMPATIBLE_UDB_VERSION)		//!!!
		Throw(ExtErr::IncompatibleFileVersion);
	AppName = header.AppName;
	uint32_t uver = header.UserVersion;
	UserVersion = Version(uver>>16, uver & 0xFFFF);
	m_salt = header.Salt;

	FrontEndName = header.FrontEndName;
	uint32_t fever = header.FrontEndVersion;
	FrontEndVersion = Version(fever>>16, fever & 0xFFFF);

	SetPageSize(header.PageSize == 1 ? 65536 : int(header.PageSize));
	PageCount = header.PageCount;
	if (FileLength < PageSpaceSize())
		Throw(ExtErr::DB_Corrupt);

	FileIncrement = max((unsigned long long)FileIncrement, (1ULL << (BitOps::ScanReverse(FileLength) - 1)) / 2);

	OpenedPages.resize(PageCount);
#if UCFG_DB_FREE_PAGES_BITSET
	FreePagesBitset.resize(PageCount);
#endif
	AdjustViewCount();
	m_pager->AddFullMapping(ReadOnly ? FileLength : (FileLength + ViewSize - 1) & ~(uint64_t(ViewSize) - 1));
	CommonOpen();

	FreePages.insert(begin(header.FreePages), std::find(begin(header.FreePages), end(header.FreePages), 0));

#if UCFG_DBLITE_DEBUG_VERBYTE
	for (int i=0; i<_countof(header.FreePages); ++i) {
		ASSERT(header.FreePages[i] < PageCount);
	}
#endif
	for (uint32_t pgno = header.FreePagePoolList; pgno;) {
		if (pgno >= PageCount)
			Throw(ExtErr::DB_Corrupt);
		Page page = OpenPage(pgno);
		m_nextFreePages.insert(pgno);
		BeUInt32* p = (BeUInt32*)page.get_Address();
		pgno = p[0];
		for (BeUInt32 *q = p + 1, *e = &p[PageSize / 4]; q != e; ++q) {
			if (uint32_t pn = *q) {
				if (!FreePages.insert(pn).second)
					Throw(ExtErr::DB_Corrupt);
			} else if (pgno) {
#ifdef _DEBUG
				Throw(ExtErr::DB_Corrupt);		// compatibility with old DBLite versions
#endif
			}
		}
	}
	TRC(2, "FreePagePoolList contains: " << FreePages.size() << " pages");
	EXT_FOR (uint32_t pgno, FreePages) {
		if (pgno >= PageCount)
			Throw(ExtErr::DB_Corrupt);
	}

#if UCFG_DB_FREE_PAGES_BITSET
	UpdateFreePagesBitset();
#endif
#ifdef _DEBUG//!!!D
	EXT_FOR (uint32_t pgno, m_nextFreePages) {
		ASSERT(!FreePages.count(pgno));
		pgno = pgno;
		ASSERT(!FreePagesBitset[pgno]);
	}
#endif
}

void KVStorage::FreePage(uint32_t pgno) {
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
}

uint32_t KVStorage::NextFreePgno(COrderedPageSet& newFreePages) {		// .second==true  if pgno from ReleasedPages
	uint32_t r = 0;
	if (!ReleasedPages.empty()) {
		CReleasedPages::iterator it = ReleasedPages.begin();
		r = *it;
		ReleasedPages.erase(it);
	} else if (!PagesFreeAfterCheckpoint.empty()) {
		auto it = PagesFreeAfterCheckpoint.begin();
		newFreePages.insert(r = *it);
		PagesFreeAfterCheckpoint.erase(it);
	} else if (!m_nextFreePages.empty()) {
		COrderedPageSet::iterator it = m_nextFreePages.begin();
		newFreePages.insert(r = *it);
		m_nextFreePages.erase(it);
	} else if (!FreePages.empty()) {					// Draw from FreePages last because we use them to Allocate pages during DoCheckpoint()
		CFreePages::iterator it = FreePages.begin();
		newFreePages.insert(r = *it);
		FreePages.erase(it);
#if UCFG_DB_FREE_PAGES_BITSET
		FreePagesBitset.reset(r);
#endif
	}
	return r;
}

bool KVStorage::DoCheckpoint(const DateTime& now, bool bLock) {
	if (!m_bModified)
		return false;

#ifdef X_DEBUG//!!!D
	LogObjectCounters();
#endif

	unique_lock<mutex> lk(MtxWrite, defer_lock);
	if (bLock)
		lk.lock();
	ASSERT(!MtxWrite.try_lock());

	DbHeader& h = DbHeaderRef();
	memcpy(h.AppName, AppName.c_str(), std::min(12, (int)strlen(AppName.c_str())));
	h.UserVersion = uint32_t((UserVersion.Major << 16) | UserVersion.Minor);
	h.ChangeCounter = h.ChangeCounter + 1;
	h.LastTransactionId = htole(LastTransactionId);
	ZeroStruct(h.FreePages);
	TxRecord& rec = h.LastTxes[0];
	rec.Id = h.LastTransactionId;
	rec.MainDbPage = MainTableRoot.N;

	shared_lock<KVStorage> shlk(_self, defer_lock);
	if (bLock)
		shlk.lock();
	EXT_LOCK (MtxFreePages) {
		for (auto p = OldestAliveGen; p; p = p->NextGen.get()) {
			ReleasedPages.insert(p->PagesToFree.begin(), p->PagesToFree.end());
			ReleasedPages.insert(p->PagesFreeAfterCheckpoint.begin(), p->PagesFreeAfterCheckpoint.end());
		}

		CFreePages newFreePages, nextFreePages;
		h.FreePagePoolList = 0;
		Page pagePrev;
		BeUInt32* pNextTrunk = &h.FreePagePoolList;
		const uint32_t PGNOS_IN_PAGE = PageSize / 4;
		uint32_t nFreePages = FreePages.size(),
			nReleasedPages = ReleasedPages.size(),
			nFreePageList = 0;
		while (FreePages.size() + ReleasedPages.size() + m_nextFreePages.size() + PagesFreeAfterCheckpoint.size() > size(h.FreePages)) {
			Page page = Allocate(false);
			nextFreePages.insert(page.N);
			BeUInt32* p = (BeUInt32*)page.get_Address();
			p[0] = 0;
			*exchange(pNextTrunk, p) = page.N;
			pagePrev = page;
			uint32_t i;
			for (i = 1; i < PGNOS_IN_PAGE && (p[i] = NextFreePgno(newFreePages)); ++i)
				;
			nFreePageList += i - 1;
			memset(p + i, 0, PageSize - (i * 4));
		}
		for (uint32_t i = 0; h.FreePages[i] = NextFreePgno(newFreePages); ++i)
			;
		m_nextFreePages = nextFreePages;
		h.FreePageCount = newFreePages.size();
		FreePages.swap(newFreePages);
		TRC(1, "FreePages: " << nFreePages << ", ReleasedPages: " << nReleasedPages << ", New FreePagesList: " << nFreePageList);

#if UCFG_DB_FREE_PAGES_BITSET
		UpdateFreePagesBitset();
#endif
		AllocatedSinceCheckpointPages.reset();
	}

	if (UseFlush) {
		EXT_LOCK(MtxViews) {
			m_pager->Flush();
		}
		DbFile.Flush();
	}
	h.PageCount = PageCount;					// PageCount can be changed during allocating FreePool, so save it last
	DbFile.Write(&h, HeaderSize, 0);					//!!! SectorSize
	if (UseFlush)
		DbFile.Flush();
	DtNextCheckpoint = now + CheckpointPeriod;;
	m_bModified = false;
	return true;
}

void KVStorage::AfterOpenView(ViewBase *view, bool bNewView, bool bCacheLocked) {
	GlobalLruViewCache& cache = *g_lruViewCache;
	if (bNewView) {
		unique_lock<mutex> lk(cache.Mtx, defer_lock);
		if (!bCacheLocked)
			lk.lock();
		if (!view->Next)							// RC, other thread can already add it to cache
			cache.push_back(*view);
		if (cache.size() > DB_MAX_VIEWS)
			cache.WakeUpFlush();
		if (!bCacheLocked && cache.size() > DB_MAX_VIEWS*3/2)
			cache.Cv.wait(lk);
	} else if (m_viewMode == ViewMode::Window) {
		unique_lock<mutex> lk(cache.Mtx, try_to_lock);
		if (lk) {
			if (view->Next)
				cache.erase(CLruViewList::const_iterator(view));
			cache.push_back(*view);
		}
	}
}

void KVStorage::AdjustViewCount() {
	if (m_viewMode == ViewMode::Window)
		Views.resize((PageCount + ((1 << m_bitsViewPageRatio) - 1)) >> m_bitsViewPageRatio);
}

ptr<ViewBase> KVStorage::OpenView(uint32_t vno) {
	if (m_viewMode == ViewMode::Full)
		vno = 0;
	bool bNewView;
	ptr<ViewBase> r;
	EXT_LOCK(MtxViews) {
		ptr<ViewBase>& view = Views.at(vno);
		if (bNewView = !view)
			(view = m_pager->CreateView())->N = vno;
		r = view;
	}
	AfterOpenView(r.get(), bNewView, false);
	return r;
}

ViewBase* KVStorage::GetViewNoLock(uint32_t vno) {
	if (m_viewMode == ViewMode::Full)
		vno = 0;
	bool bNewView;
	ptr<ViewBase>& view = Views.at(vno);
	if (bNewView = !view) {
		(view = m_pager->CreateView())->N = vno;
		view->Load();
	}
	if (bNewView)
		AfterOpenView(view, bNewView, true);
	return view;
}

void *KVStorage::GetPageAddress(ViewBase *view, uint32_t pgno) {
	return m_viewMode == ViewMode::Full
		? (uint8_t*)ViewAddress + uint64_t(pgno) * PageSize
		: (uint8_t*)view->GetAddress() + uint64_t(pgno & ((1 << m_bitsViewPageRatio) - 1)) * PageSize;
}

Page KVStorage::OpenPage(uint32_t pgno, bool bAlloc) {
	ASSERT(pgno);

	OnOpenPage(pgno);

#ifdef X_DEBUG//!!!D
	ASSERT(!m_nextFreePages.count(pgno));
#endif

	bool bNewView = false;
	Page r;
	PageObj *po;

	EXT_LOCK (MtxViews) {
		if (pgno >= OpenedPages.size())
			Throw(ExtErr::DB_Corrupt);

		if (po = OpenedPages[pgno]) {
			po->Live = true;
			if (bAlloc)
				po->Flushed = false;
			return Page(po);
		}
		if (NewPageCount > PageCacheSize) {
			int count = 0;
			for (size_t i = 0, sz = OpenedPages.size(); i < sz; ++i) {
				if (PageObj* poj = OpenedPages[i]) {
					if (poj->m_aRef != 1)
						++count;
					else if (!exchange(poj->Live, false)) {
						CCounterIncDec<PageObj, InterlockedPolicy>::Release(poj);
						OpenedPages[i] = 0;
					}
				}
			}
			if (count > PageCacheSize/2)
				PageCacheSize *= 2;
			NewPageCount = 0;
		}

		++NewPageCount;
		uint32_t vno = pgno >> m_bitsViewPageRatio;
		if (m_viewMode == ViewMode::Full)
			vno = 0;
		ptr<ViewBase>& view = Views.at(vno);
		if (view) {
			if (!bAlloc) {
				r = po = new PageObj(_self);
				view->Load();
			}
			if (UseMMapPager)
				view->Flushed = false;
		} else {
			view = m_pager->CreateView();
			bNewView = true;
			view->N = vno;
			if (!bAlloc) {
				r = po = new PageObj(_self);
				view->Load();
			}
		}
		if (bAlloc)
			r = po = view->AllocPage(pgno);
		po->View = view;
		po->N = pgno;
		++((OpenedPages[pgno] = po)->m_aRef);
	}
	po->Live = true;
	AfterOpenView(po->View.get(), bNewView, false);

	return r;
}

uint32_t KVStorage::TryAllocateMappedFreePage() {
	ASSERT(m_bitsViewPageRatio >= 3);

	const uint32_t q = (1<<m_bitsViewPageRatio),
			step = min(q, (uint32_t)64);
	const uint64_t mask = (uint64_t(1) << step) - 1;

	EXT_LOCK (MtxViews) {
		for (uint32_t vno = 0; vno < Views.size(); ++vno) {
			uint32_t pgBeg = vno << m_bitsViewPageRatio;
#if UCFG_DB_FREE_PAGES_BITSET
			uint32_t pgEnd=pgBeg + q;
/*!!! bitset::find_ has O(n) complexity
			CFreePagesBitset::size_type pgnoFree = pgno ? FreePagesBitset.find_next_set(pgno-1) : FreePagesBitset.find_first_set();
			if (pgnoFree != CFreePagesBitset::npos && pgnoFree < pgEnd) {
				FreePagesBitset.reset(pgnoFree);
				FreePages.erase(pgnoFree);
				return pgnoFree;
			} else if (pgnoFree == CFreePagesBitset::npos && pgEnd > PageCount) {
				uint32_t r = FreePagesBitset.size();
				FreePagesBitset.push_back(false);
				for (uint32_t i=PageCount; i<r; ++i)
					FreePage(i);
				OpenedPages.resize(PageCount = r+1);
				return r;
			}
*/

			uint32_t pgno = pgBeg;
			for (; pgno < pgEnd; pgno += step) {
				if (pgno + step > PageCount || (mask & ToUInt64AtBytePos(FreePagesBitset, pgno & ~(step - 1)))) {
					for (uint32_t j = pgno; j < pgno + step; ++j) {
						if (j >= PageCount || FreePagesBitset[j]) {
							if (j >= PageCount) {
								FreePagesBitset.resize(j + 1);
								for (uint32_t i = PageCount; i < j; ++i)
									FreePage(i);
								OpenedPages.resize(PageCount = j + 1);
							} else {
								FreePagesBitset.reset(j);
								FreePages.erase(j);
							}
							return j;
						}
					}
					Throw(ExtErr::CodeNotReachable);
				}
			}
#else
			COrderedPgNos::iterator itFree = FreePages.lower_bound(pgBeg);
			if (itFree != FreePages.lower_bound(pgBeg + q)) {
				uint32_t r = *itFree;
				FreePages.erase(itFree);
				return r;
			}
#endif
		}
	}
	return 0;
}

void KVStorage::MarkAllocatedPage(uint32_t pgno) {
	if (AllocatedSinceCheckpointPages.size() < pgno + 1)
		AllocatedSinceCheckpointPages.resize(std::max(pgno + 1, (uint32_t)AllocatedSinceCheckpointPages.size() * 2)); //!!!TODO 2^32 limitation
	AllocatedSinceCheckpointPages.set(pgno);
}

Page KVStorage::Allocate(bool bLock) {
	m_bModified = true;
	uint32_t pgno = 0;
	{
		unique_lock<mutex> lk(MtxFreePages, defer_lock);
		if (bLock)
			lk.lock();

		if (!FreePages.empty()) {
#if UCFG_DB_FREE_PAGES_BITSET
			if (m_viewMode == ViewMode::Full || !(pgno = TryAllocateMappedFreePage())) {

#else
			if (true) {
#endif
				CFreePages::iterator it = FreePages.begin();
				pgno = *it;
				FreePages.erase(it);
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
		AdjustViewCount();
	}
	if (UseMMapPager && FileLength < PageSpaceSize()) {
		const uint64_t MAX_INCREMENT = 1024 * 1024 * 1024;
		FileIncrement = (min)(MAX_INCREMENT, FileIncrement * 2);
		uint64_t newFileSize = (PageSpaceSize() + FileIncrement - 1) / FileIncrement * FileIncrement;
		m_pager->AddFullMapping(newFileSize);
		DbFile.Flush();			// save Metadata

#ifdef X_DEBUG//!!!D
		LogObjectCounters();
#endif
	}
LAB_ALLOCATED:
	Page r = OpenPage(pgno, true);
	if (ProtectPages)
		MemoryMappedView::Protect(r.Address, PageSize, MemoryMappedFileAccess::ReadWrite);
	return r;
}

uint32_t KVStorage::GetUInt32(uint32_t pgno, int offset) {
	uint8_t* p = m_viewMode == ViewMode::Window
		? (uint8_t*)OpenPage(pgno).get_Address()
		: (uint8_t*)ViewAddress + uint64_t(pgno) * PageSize;
	return letoh(*(uint32_t*)(p + offset));
}

void KVStorage::SetPageSize(size_t v) {
	m_bitsViewPageRatio = BitOps::Scan(ViewSize / (PageSize = v)) - 1;
}

void KVStorage::DoClose(bool bLock) {
//!!!?	if (ReaderRefCount)
//!!!		Throw(E_FAIL);

	DoCheckpoint(Clock::now(), bLock);
	MainTableRoot = Page(nullptr);

	EXT_FOR (PageObj *po, OpenedPages) {
		if (po) {
			ASSERT(po->m_aRef == 1);
			CCounterIncDec<PageObj, InterlockedPolicy>::Release(po);
		}
	}
	OpenedPages.clear();

	if (m_viewMode == ViewMode::Full) {
		EXT_LOCKED(MtxViews, Views.clear());
	} else {
		GlobalLruViewCache& cache = *g_lruViewCache;
		unique_lock<mutex> lk(cache.Mtx);
		EXT_LOCK (MtxViews) {				// can be accessed from other storages
			for (size_t i = 0; i < Views.size(); ++i)
				if (ViewBase *view = Views[i])
					cache.erase(CLruViewList::const_iterator(view));
			Views.clear();
		}
		if (cache.m_viewProcessed && &cache.m_viewProcessed->Storage == this) {
			cache.m_viewProcessed->Removed = true;
			cache.Cv.wait(lk);
		}
	}

	ViewAddress = nullptr;
	m_pager->Close();
	if (!ReadOnly)
		DbFile.Length = PageSpaceSize();
	DbFile.Close();
	m_bModified = false;
	EXT_LOCK(MtxFreePages) {
		CurGen.reset();
		ReleasedPages.clear();
		FreePages.clear();
	}
	m_state = OpenState::Closed;
	m_nextFreePages.clear();
#if UCFG_DB_FREE_PAGES_BITSET
	FreePagesBitset.clear();
#endif

	Init();
}

void KVStorage::Close(bool bLock) {
	switch (m_state) {
	case OpenState::Closing:
		if (!AsyncClose)
			m_futClose.wait();
		break;
	case OpenState::Opened:
		if (AsyncClose) {
			m_state = OpenState::Closing;
			m_futClose = std::async(&KVStorage::DoClose, this, bLock);
		} else
			DoClose(bLock);
	}
}

PageObj::PageObj(KVStorage& storage)
	: m_address(0)
//!!!R	, Storage(storage)
	, N(0)
	, aEntries(nullptr)
	, Overflows(0)
	, Dirty(false)
	, Flushed(true)
{
}

PageObj::~PageObj() {
	if (m_address && View->Storage.ProtectPages && Dirty)
		MemoryMappedView::Protect(m_address, View->Storage.PageSize, MemoryMappedFileAccess::Read);

	free(aEntries);
}

void Page::ClearEntries() const {
	free(m_pimpl->aEntries);
	m_pimpl->aEntries = nullptr;
}

DbTransactionBase::DbTransactionBase(KVStorage& storage)
	: Storage(storage)
	, ReadOnly(true)
	, m_shlk(storage, defer_lock)
{
	InitReadOnly();
}

DbTransactionBase::DbTransactionBase(KVStorage& storage, bool bReadOnly)
	: Storage(storage)
	, ReadOnly(bReadOnly)
	, m_shlk(storage, defer_lock)
{
	if (ReadOnly)
		InitReadOnly();
}

DbTransactionBase::~DbTransactionBase() {
	if (SnapshotGen) {
		EXT_LOCK(Storage.MtxFreePages) {
			ptr<SnapshotGenObj> next = SnapshotGen->NextGen;		// To avoid recursion
			SnapshotGen.reset();
			for (ptr<SnapshotGenObj> cur = next; cur; cur = next) {
				next = cur->NextGen;
				cur.reset();
			}
		}
	}
}

void DbTransactionBase::InitReadOnly() {
	m_shlk.lock();
	EXT_LOCK(Storage.MtxRoot) {
		TransactionId = Storage.LastTransactionId;
		MainTableRoot = Storage.MainTableRoot;
		SnapshotGen = Storage.CurGen;
	}
}

Page DbTransactionBase::OpenPage(uint32_t pgno) {
	return Storage.OpenPage(pgno);
}

void DbTransactionBase::Commit() {
	m_shlk.unlock();
	m_bComplete = true;
}

void DbTransactionBase::Rollback() {
	m_shlk.unlock();
	m_bComplete = true;
}


DbTransaction::DbTransaction(KVStorage& storage, bool bReadOnly)
	: base(storage, bReadOnly)
	, m_lockWrite(storage.MtxWrite, defer_lock)
{
	if (!ReadOnly) {
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

void DbTransaction::FreePage(uint32_t pgno) {
	if (AllocatedPages.erase(pgno)) {
		EXT_LOCKED(Storage.MtxFreePages, Storage.FreePage(pgno));
	} else if (Storage.Durability || !Storage.AllocatedSinceCheckpointPages.contains(pgno))
		PagesFreeAfterCheckpoint.push_back(pgno);
	else
		PagesToFree.push_back(pgno);
}

Page DbTransaction::Allocate(PageAlloc pa, Page *pCopyFrom) {
	Page r = Storage.Allocate();
	r.ClearEntries(); //!!!?
	r->Dirty = true;
	AllocatedPages.insert(r.N);
	switch (pa) {
	case PageAlloc::Leaf:
	case PageAlloc::Branch:
		{
			PageHeader& h = r.Header();
			h.Num = 0;
			h.Flags = pa == PageAlloc::Branch ? PageHeader::FLAG_BRANCH : 0;
		}
		break;
	case PageAlloc::Copy:
		MemcpyAligned32(r.Address, pCopyFrom->Address, Storage.PageSize);
		break;
	case PageAlloc::Move:
		MemcpyAligned32(r.Address, pCopyFrom->Address, Storage.PageSize);
		FreePage(*pCopyFrom);
		break;
	case PageAlloc::Zero:
		memset(r.get_Address(), 0, Storage.PageSize);				//!!!TODO: optimize
		break;
	}
	ASSERT(!r->aEntries.load() && !r->Overflows);
	return r;
}

Page DbTransaction::OpenPage(uint32_t pgno) {
	Page r = Storage.OpenPage(pgno);
	if (!ReadOnly) {
		bool bDirty = r->Dirty = AllocatedPages.count(pgno);
		if (bDirty && Storage.ProtectPages)
			MemoryMappedView::Protect(r->GetAddress(), Storage.PageSize, MemoryMappedFileAccess::ReadWrite);
	}
	return r;
}

vector<uint32_t> DbTransaction::AllocatePages(int n) {
	vector<uint32_t> r;
	r.reserve(n);
	struct FreeReleaser {
		DbTransaction *Tx;
		vector<uint32_t> *Pages;

		~FreeReleaser() {
			if (Pages)
				for (int i=0; i<Pages->size(); ++i)
					Tx->FreePage((*Pages)[i]);
		}
	} pf = { this, &r };
	for (int i=0; i<n; ++i)
		r.push_back(Allocate(PageAlloc::Nothing).N);
	pf.Pages = 0;
	return r;
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
			EXT_FOR (uint32_t pgno, AllocatedPages) {
				Storage.AllocatedSinceCheckpointPages.reset(pgno);
				Storage.FreePage(pgno);
			}
		}
	}
	Complete();
}

SnapshotGenObj::~SnapshotGenObj() {
	ASSERT(!Storage.MtxFreePages.try_lock());		// always called under MtxFreePages lock
	ASSERT(Storage.m_state != KVStorage::OpenState::Closed || PagesToFree.empty());

	Storage.OldestAliveGen = NextGen.get();
	EXT_FOR(uint32_t pgno, PagesToFree) {
		Storage.FreePage(pgno);
	}
	Storage.PagesFreeAfterCheckpoint.insert(PagesFreeAfterCheckpoint.begin(), PagesFreeAfterCheckpoint.end());
}

void DbTransaction::Commit() {
	CKVStorageKeeper keeper(&Storage);

	if (!ReadOnly) {
		TRC(6, "");

		if (m_bError) {
			Rollback();
			Throw(ExtErr::DB_InternalError);
		}

		for (CTables::iterator it = Tables.begin(), e = Tables.end(); it != e; ++it) {
			PagedMap& m = *it->second;
			if (m.Dirty && m.Name != DbTable::Main().Name) {
				TableData td = m.GetTableData();
				Span k((const uint8_t*)m.Name.c_str(), m.Name.length());
				DbCursor(_self, DbTable::Main()).Put(k, Span((const uint8_t*)&td, sizeof td));
			}
		}


		EXT_LOCK (Storage.MtxRoot) {
			EXT_LOCK (Storage.MtxFreePages) {
				Storage.MainTableRoot = MainTableRoot;
				Storage.LastTransactionId = TransactionId;

				ptr<SnapshotGenObj> prevGen = Storage.CurGen;
				prevGen->PagesToFree = PagesToFree;
				prevGen->PagesFreeAfterCheckpoint = PagesFreeAfterCheckpoint;
				prevGen->NextGen = Storage.CurGen = new SnapshotGenObj(Storage);
				if (!Storage.OldestAliveGen)
					Storage.OldestAliveGen = Storage.CurGen.get();
			}
		}

		EXT_LOCK (Storage.MtxViews) {
			EXT_FOR (uint32_t pgno, AllocatedPages) {
				if (PageObj *po = Storage.OpenedPages[pgno]) {
//!!!?					if (Storage.ProtectPages && po.Dirty)
//						MemoryMappedView::Protect(po.Address, Storage.PageSize, MemoryMappedFileAccess::Read);
					po->Dirty = false;
				}
			}
				//	for (KVStorage::COpenedPages::iterator it=Storage.OpenedPages.begin(), e=Storage.OpenedPages.end(); it!=e; ++it)
				//	it->second->Dirty = false;
		}

		AllocatedPages.clear();			//!!!? is it necessary?
		PagesToFree.clear();
		PagesFreeAfterCheckpoint.clear();

		ASSERT(m_lockWrite.owns_lock());
		DateTime now = Clock::now();
		if (Storage.Durability || now >= Storage.DtNextCheckpoint) {
			shared_lock<KVStorage> shlk(Storage);
			Storage.DoCheckpoint(now, false);
		}
	}
	Complete();
}

static DbTable s_mainTable("__main");

DbTable& AFXAPI DbTable::Main() {
	return s_mainTable;
}


}}} // Ext::DB::KV::
