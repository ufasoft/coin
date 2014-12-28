/*######     Copyright (c) 1997-2015 Ufasoft  http://ufasoft.com  mailto:support@ufasoft.com,  Sergey Pavlov  mailto:dev@ufasoft.com #########################################################################################################
#                                                                                                                                                                                                                                            #
# This program is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation;  either version 3, or (at your option) any later version.          #
# This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.   #
# You should have received a copy of the GNU General Public License along with this program; If not, see <http://www.gnu.org/licenses/>                                                                                                      #
############################################################################################################################################################################################################################################*/

#include <el/ext.h>

#include "dblite.h"
#include "dblite-file-format.h"
#include "b-tree.h"

namespace Ext { namespace DB { namespace KV {

// Params 
const int RESERVED_FILE_SPACE = 8192;
const int FLUSH_WAIT_MS = 100000;

static const char s_Magic[] = UDB_MAGIC;


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

typedef IntrusiveList<MMView> CLruViewList;

class GlobalLruViewCache : public CLruViewList {
public:
	mutex Mtx;
	condition_variable Cv;
	
	ptr<MMView> m_viewProcessed;

	GlobalLruViewCache()
		:	RefCount(0)
	{}

	void AddRef() {
		if (Interlocked::Increment(RefCount) == 1) {
			EXT_LOCK (m_mtxThread) {
				if (!Thread)
					(Thread = new FlushThread)->Start();
			}
		}
	}

	void Release() {
		if (!Interlocked::Decrement(RefCount)) {
			ASSERT(empty());
			EXT_LOCK (m_mtxThread) {
				Thread->interrupt();
				Thread->Join();
				Thread = nullptr;
			}
		}
	}

	void WakeUpFlush() {
		Thread->m_ev.Set();
	}
private:
	mutex m_mtxThread;
	ptr<FlushThread> Thread;
	volatile int32_t RefCount;
};

static InterlockedSingleton<GlobalLruViewCache> g_lruViewCache;


void FlushThread::Execute() {
	Name = "FlushViewsThread";
	GlobalLruViewCache& cache = *g_lruViewCache;

	for (int nDeleted=0; !m_bStop;) {
		EXT_LOCK (cache.Mtx) {
			if (cache.size() > DB_MAX_VIEWS || nDeleted < DB_NUM_DELETE_FROM_CACHE) {
				for (CLruViewList::iterator ir=cache.begin(); ir!=cache.end();) {
					MMView& mmv = *ir;
					if (1 != mmv.m_dwRef)
						++ir;
					else {				
						cache.m_viewProcessed = &mmv;
						cache.m_viewProcessed->Removed = false;
						++nDeleted;
						goto LAB_FOUND;
					}
				}
			}
		}
		nDeleted = 0;
		cache.Cv.notify_all();
		if (!cache.m_viewProcessed)
			m_ev.lock(FLUSH_WAIT_MS);
		else {
LAB_FOUND:
			cache.m_viewProcessed->Flushed = true;
			if (cache.m_viewProcessed->Storage.UseFlush)
				cache.m_viewProcessed->View.Flush();
			bool bNotifyStorage = false;
			EXT_LOCK (cache.Mtx) {
				if (cache.m_viewProcessed->Flushed && (2 == cache.m_viewProcessed->m_dwRef)) {
					if (cache.m_viewProcessed->Removed)
						bNotifyStorage = true;					
					else {
						cache.erase(GlobalLruViewCache::const_iterator(cache.m_viewProcessed.get()));
						EXT_LOCKED(cache.m_viewProcessed->Storage.MtxViews, cache.m_viewProcessed->Storage.Views.erase(cache.m_viewProcessed->N));
					}
				}
				cache.m_viewProcessed = nullptr;
			}
			if (bNotifyStorage)
				cache.Cv.notify_all();
		}
	}
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
	,	UseFlush(true)
	,	CheckpointPeriod(TimeSpan::FromMinutes(1))
	,	ProtectPages(true)
	,	m_state(OpenState::Closed)
	,	m_alignedDbHeader(RESERVED_FILE_SPACE, 4096)		//!!! SectorSize
	,	m_salt((uint32_t)Random().Next())
	,	ViewAddress(0)
#if UCFG_PLATFORM_X64
	,	m_viewMode(ViewMode::Full)
#else
	,	m_viewMode(ViewMode::Window)
#endif
{
	Init();
	g_lruViewCache->AddRef();
}

void KVStorage::Init() {
	PageSize = 0;
	FileIncrement = ViewSize/2;
	m_accessViewMode = m_viewMode;
}

KVStorage::~KVStorage() {
	switch (m_state) {
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
	memset(&header, 0, RESERVED_FILE_SPACE);
	memcpy(header.Magic, s_Magic, strlen(s_Magic));
	memcpy(header.AppName, AppName.c_str(), std::min(12, (int)strlen(AppName.c_str())));
	header.UserVersion = uint32_t((UserVersion.Major << 16)|UserVersion.Minor);

	memcpy(header.FrontEndName, FrontEndName.c_str(), std::min(12, (int)strlen(FrontEndName.c_str())));
	header.FrontEndVersion = uint32_t((FrontEndVersion.Major << 16)|FrontEndVersion.Minor);

	header.Version = htole(UDB_VERSION);
	header.PageSize = uint16_t(PageSize==65536 ? 1 : PageSize);
	header.Salt = Salt;
	header.PageCount = PageCount;
	
	DbFile.Write(&header, sizeof header);
	
	FileLength = RESERVED_FILE_SPACE;
}

void KVStorage::MapMeta() {
//	MMFile = MemoryMappedFile::CreateFromFile(DbFile, nullptr, ViewSize);
	DbHeader& header = DbHeaderRef();
	if (uint32_t pgnoRoot = header.LastTxes[0].MainDbPage)	
		MainTableRoot = OpenPage(pgnoRoot);
}

void KVStorage::AddFullMapping(uint64_t fileLength) {
	EXT_LOCK (MtxViews) {
		ptr<MappedFile> m = new MappedFile;
		MemoryMappedFileAccess acc = ReadOnly ? MemoryMappedFileAccess::Read : MemoryMappedFileAccess::ReadWrite;
		m->MemoryMappedFile = MemoryMappedFile::CreateFromFile(DbFile, nullptr, FileLength = fileLength, acc);
		Mappings.push_back(m);
		if (m_viewMode == ViewMode::Full) {
			ptr<MMView> v = new MMView(_self);
			v->View = Mappings.back()->MemoryMappedFile.CreateView(0, 0, acc);
			ViewAddress = v->View.Address;
			m_fullViews.push_back(v);
			Views[0] = v;
		}
	}
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

void KVStorage::Create(const path& filepath) {
	if (m_state == OpenState::Closing)
		m_futClose.wait();

	File::OpenInfo oi(FilePath = filepath);
	oi.Share = FileShare::Read;
	oi.Mode = FileMode::Open;
	oi.Options = FileOptions::RandomAccess;
//	oi.BufferingEnabled = false;
	if (!exists(filepath) || file_size(filepath) != 0)
		oi.Mode = FileMode::CreateNew;
	DbFile.Open(oi);
	size_t sectorSize = DbFile.PhysicalSectorSize();
	if (!PageSize)
		SetPageSize(sectorSize>=4096 ? sectorSize : DEFAULT_PAGE_SIZE);
	
	PageCount = 2;
	OpenedPages.resize(PageCount);
#if UCFG_DB_FREE_PAGES_BITSET
	FreePagesBitset.resize(PageCount);
#endif

	WriteHeader();
	DbFile.Flush();
	MapMeta();
	m_state = OpenState::Opened;
	m_dtPrevCheckpoint = DateTime::UtcNow();
}

void KVStorage::Open(const path& filepath) {
	if (m_state == OpenState::Closing)
		m_futClose.wait();

	File::OpenInfo oi(FilePath = filepath);
	oi.Share = FileShare::Read;
	if (ReadOnly) {
		oi.Access = FileAccess::Read;
		oi.Share = FileShare::ReadWrite;
	}
	oi.Mode = FileMode::Open;
	oi.Options = FileOptions::RandomAccess;
//	oi.BufferingEnabled = false;
	DbFile.Open(oi);
	FileLength = DbFile.Length;
	DbHeader& header = DbHeaderRef();
	DbFile.Read(&header, 4096);							//!!! SectorSize
	if (memcmp(header.Magic, s_Magic, strlen(s_Magic)))
		Throw(E_EXT_DB_Corrupt);
	uint32_t ver = letoh(header.Version);
	if (ver > UDB_VERSION)
		Throw(E_EXT_DB_Version);
	if (ver < COMPATIBLE_UDB_VERSION)		//!!!
		Throw(E_EXT_IncompatibleFileVersion);
	AppName = header.AppName;
	uint32_t uver = header.UserVersion;
	UserVersion = Version(uver>>16, uver & 0xFFFF);
	m_salt = header.Salt;

	FrontEndName = header.FrontEndName;
	uint32_t fever = header.FrontEndVersion;
	FrontEndVersion = Version(fever>>16, fever & 0xFFFF);

	SetPageSize(header.PageSize==1 ? 65536 : int(header.PageSize));
	PageCount = header.PageCount;
	if (FileLength < uint64_t(PageCount)*PageSize)
		Throw(E_EXT_DB_Corrupt);
	FileIncrement = max((unsigned long long)FileIncrement, (1ULL << (BitOps::ScanReverse(FileLength)-1))/2);

	OpenedPages.resize(PageCount);
#if UCFG_DB_FREE_PAGES_BITSET
	FreePagesBitset.resize(PageCount);
#endif

	AddFullMapping(ReadOnly ? FileLength : (FileLength + ViewSize - 1) & ~(uint64_t(ViewSize) - 1));
	MapMeta();
	
	FreePages.insert(begin(header.FreePages), std::find(begin(header.FreePages), end(header.FreePages), 0));

#if UCFG_DBLITE_DEBUG_VERBYTE
	for (int i=0; i<_countof(header.FreePages); ++i) {
		ASSERT(header.FreePages[i] < PageCount);
	}
#endif
	for (uint32_t pgno=header.FreePagePoolList; pgno; ) {
		if (pgno >= PageCount)
			Throw(E_EXT_DB_Corrupt);
		Page page = OpenPage(pgno);
		m_nextFreePages.insert(pgno);
		BeUInt32 *p = (BeUInt32*)page.get_Address();
		pgno = p[0];
		for (BeUInt32 *q=p+1, *e=&p[PageSize/4]; q!=e; ++q) {
			if (uint32_t pn = *q) {
				if (!FreePages.insert(pn).second)
					Throw(E_EXT_DB_Corrupt);
			} else if (pgno) {
#ifdef _DEBUG
				Throw(E_EXT_DB_Corrupt);		// compatibility with old DBLite versions
#endif
			}
		}
	}
	EXT_FOR (uint32_t pgno, FreePages) {
		if (pgno >= PageCount)
			Throw(E_EXT_DB_Corrupt);
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
	m_state = OpenState::Opened;
	m_dtPrevCheckpoint = DateTime::UtcNow();
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
	unique_lock<shared_mutex> lk(ShMtx, try_to_lock);
	if (lk) {
		EXT_LOCK (MtxFreePages) {
			EXT_FOR (uint32_t pgno, ReaderLockedPages) {
				FreePage(pgno);
			}
			ReaderLockedPages.clear();
		}
	}
}

uint32_t KVStorage::NextFreePgno(COrderedPgNos& newFreePages) {		// .second==true  if pgno from ReleasedPages
	uint32_t r = 0;
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
	} else if (!m_nextFreePages.empty()) {
		COrderedPgNos::iterator it = m_nextFreePages.begin();
		newFreePages.insert(r = *it);
		m_nextFreePages.erase(it);
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
	memcpy(h.AppName, AppName.c_str(), std::min(12, (int)strlen(AppName.c_str())));
	h.UserVersion = uint32_t((UserVersion.Major << 16)|UserVersion.Minor);
	h.ChangeCounter = h.ChangeCounter + 1;
	h.LastTransactionId = htole(LastTransactionId);
	ZeroStruct(h.FreePages);
	TxRecord& rec = h.LastTxes[0];
	rec.Id = h.LastTransactionId;
	rec.MainDbPage = MainTableRoot.N;

	{
		shared_lock<KVStorage> shlk(_self, defer_lock);
		if (bLock)
			shlk.lock();
		EXT_LOCK (MtxFreePages) {
			COrderedPgNos newFreePages, nextFreePages;
			h.FreePagePoolList = 0;
			Page pagePrev;
			BeUInt32 *pNextTrunk = &h.FreePagePoolList;
			const uint32_t PGNOS_IN_PAGE = PageSize / 4;
			while (FreePages.size() + ReleasedPages.size() + m_nextFreePages.size() > _countof(h.FreePages)) {
				Page page = Allocate(false);
				nextFreePages.insert(page.N);
				BeUInt32 *p = (BeUInt32*)page.get_Address();
				p[0] = 0;
				*exchange(pNextTrunk, p) = page.N;
				pagePrev = page;
				uint32_t i;
				for (i=1; i<PGNOS_IN_PAGE && (p[i]=NextFreePgno(newFreePages)); ++i)
					;
				memset(p+i, 0, PageSize-(i*4));
			}
			for (uint32_t i=0; h.FreePages[i]=NextFreePgno(newFreePages); ++i)
				;
			m_nextFreePages = nextFreePages;
			h.FreePageCount = newFreePages.size();
			FreePages.swap(newFreePages);

#if UCFG_DB_FREE_PAGES_BITSET
			UpdateFreePagesBitset();
#endif

			AllocatedSinceCheckpointPages.reset();
		}

		if (UseFlush) {
			EXT_LOCK (MtxViews) {
				EXT_FOR (CViews::value_type& kv, Views) {
					kv.second->View.Flush();				// Flush View syncs to the disk, FlushFileBuffers() is not enough
				}
			}
			DbFile.Flush();
		}
		h.PageCount = PageCount;					// PageCount can be changed during allocating FreePool, so save it last
		DbFile.Write(&h, 4096, 0);					//!!! SectorSize
		if (UseFlush)
			DbFile.Flush();
	}
	m_dtPrevCheckpoint = DateTime::UtcNow();
	m_bModified = false;
	return true;
}

Page KVStorage::OpenPage(uint32_t pgno) {
	ASSERT(pgno);

#ifdef X_DEBUG//!!!D
	ASSERT(!m_nextFreePages.count(pgno));
#endif

	bool bNewView = false;
	Page r;
	PageObj *po;

	EXT_LOCK (MtxViews) {
		if (pgno >= OpenedPages.size())
			Throw(E_EXT_DB_Corrupt);

		if (po = OpenedPages[pgno]) {
			po->Live = true;
			return Page(po);
		}
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
		uint32_t vno = pgno >> m_bitsViewPageRatio;
		if (m_viewMode == ViewMode::Full)
			vno = 0;
		CViews::iterator it = Views.find(vno);
		if (it != Views.end()) {
			MMView *mmview = (po->View = it->second).get();
			mmview->Flushed = false;
		} else {
			ptr<MMView> mmview = new MMView(_self);
			bNewView = true;
			mmview->N = vno;
			size_t viewSize = ViewSize;
			if (ReadOnly && uint64_t(vno+1)*ViewSize > FileLength)
				viewSize = FileLength % ViewSize;
			mmview->View = Mappings.back()->MemoryMappedFile.CreateView(uint64_t(vno)*ViewSize, viewSize, ReadOnly ? MemoryMappedFileAccess::Read : MemoryMappedFileAccess::ReadWrite);
			if (ProtectPages && !ReadOnly)
				MemoryMappedView::Protect(mmview->View.Address, viewSize, MemoryMappedFileAccess::Read);
			po->View = mmview;
			Views.insert(make_pair(vno, mmview));
		}
		po->N = pgno;
		if (m_viewMode == ViewMode::Full)
			po->Address = (byte*)ViewAddress + uint64_t(pgno)*PageSize;
		else
			po->Address = (byte*)po->View->View.Address + uint64_t(pgno & ((1<<m_bitsViewPageRatio) - 1))*PageSize;
		Interlocked::Increment((OpenedPages[pgno] = po)->m_dwRef);
	}
	po->Live = true;
	GlobalLruViewCache& cache = *g_lruViewCache;
	if (bNewView) {
		unique_lock<mutex> lk(cache.Mtx);
		if (!po->View->Next)							// RC, other thread can already add it to cache
			cache.push_back(*po->View.get());
		if (cache.size() > DB_MAX_VIEWS)
			cache.WakeUpFlush();
		if (cache.size() > DB_MAX_VIEWS*3/2)
			cache.Cv.wait(lk);
	} else if (m_viewMode == ViewMode::Window) {	
		unique_lock<mutex> lk(cache.Mtx, try_to_lock);
		if (lk) {
			if (po->View->Next)
				cache.erase(CLruViewList::const_iterator(po->View));
			cache.push_back(*po->View);
		}
	}

	return r;
}

uint32_t KVStorage::TryAllocateMappedFreePage() {
	ASSERT(m_bitsViewPageRatio >= 3);

	const uint32_t q = (1<<m_bitsViewPageRatio),
			step = min(q, (uint32_t)64);
	const uint64_t mask = (uint64_t(1) << step) - 1; 

	EXT_LOCK (MtxViews) {
		for (CViews::iterator it=Views.begin(), e=Views.end(); it!=e; ++it) {
			uint32_t pgBeg = it->first << m_bitsViewPageRatio;
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
			for (; pgno<pgEnd; pgno+=step) {
				if (pgno+step > PageCount || (mask & ToUInt64AtBytePos(FreePagesBitset, pgno & ~(step-1)))) {
					for (uint32_t j=pgno; j<pgno+step; ++j) {
						if (j>=PageCount || FreePagesBitset[j]) {
							if (j >= PageCount) {
								FreePagesBitset.resize(j+1);
								for (uint32_t i=PageCount; i<j; ++i)
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
	if (AllocatedSinceCheckpointPages.size() < pgno+1)
		AllocatedSinceCheckpointPages.resize(std::max(pgno+1, (uint32_t)AllocatedSinceCheckpointPages.size()*2)); 	//!!!TODO 2^32 limitation
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
			if (m_viewMode==ViewMode::Full || !(pgno = TryAllocateMappedFreePage())) {

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
	if (FileLength < uint64_t(PageCount)*PageSize) {
		//FileIncrement = min(FileIncrement*2, uint64_t(1024*1024*1024));
		FileIncrement *= 2;
		AddFullMapping((uint64_t(PageCount) * PageSize + FileIncrement - 1) & ~(FileIncrement - 1));
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

uint32_t KVStorage::GetUInt32(uint32_t pgno, int offset) {
	if (m_viewMode == ViewMode::Window) {
		return letoh(*(uint32_t*)((byte*)OpenPage(pgno).get_Address() + offset));
	} else {
		return letoh(*(uint32_t*)((byte*)ViewAddress + uint64_t(pgno)*PageSize + offset));
	}
}

void KVStorage::SetPageSize(size_t v) {
	m_bitsViewPageRatio = BitOps::Scan(ViewSize / (PageSize = v)) - 1;
}

void KVStorage::DoClose(bool bLock) {
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

	if (m_viewMode == ViewMode::Full) {
		EXT_LOCKED(MtxViews, Views.clear());
	} else {
		GlobalLruViewCache& cache = *g_lruViewCache;
		unique_lock<mutex> lk(cache.Mtx);
		EXT_LOCK (MtxViews) {				// can be accessed from other storages
			EXT_FOR (const CViews::value_type& kv, Views) {
				cache.erase(CLruViewList::const_iterator(kv.second.get()));
			}
			Views.clear();
		}
		if (cache.m_viewProcessed && &cache.m_viewProcessed->Storage == this) {
			cache.m_viewProcessed->Removed = true;
			cache.Cv.wait(lk);
		}
	}

	ViewAddress = nullptr;
	m_fullViews.clear();
	Mappings.clear();
	if (!ReadOnly)
		DbFile.Length = uint64_t(PageCount) * PageSize;
	DbFile.Close();
	m_bModified = false;
	m_state = OpenState::Closed;
	m_nextFreePages.clear();
	ReleasedPages.clear();
	FreePages.clear();
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

MMView::~MMView() {
	if (!Flushed && Storage.UseFlush)
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

	free(Entries);
}

void Page::ClearEntries() const {
	free(exchange(m_pimpl->Entries, nullptr));
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
}

void DbTransactionBase::InitReadOnly() {
	m_shlk.lock();
	EXT_LOCK(Storage.MtxRoot) {
		TransactionId = Storage.LastTransactionId;
		MainTableRoot = Storage.MainTableRoot;
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
	:	base(storage, bReadOnly)
	,	m_lockWrite(storage.MtxWrite, defer_lock)
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

Page DbTransaction::Allocate(PageAlloc pa, Page *pCopyFrom) {
	Page r = Storage.Allocate();
	r.ClearEntries(); //!!!?
	r.m_pimpl->Dirty = true;
	AllocatedPages.insert(r.N);
	switch (pa) {
	case PageAlloc::Leaf:
	case PageAlloc::Branch:
		{
			PageHeader& h = r.Header();
			h.Num = 0;
			h.Flags = pa==PageAlloc::Branch ? PageHeader::FLAG_BRANCH : 0;
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
	ASSERT(!r.m_pimpl->Entries && !r.m_pimpl->Overflows);
	return r;
}

Page DbTransaction::OpenPage(uint32_t pgno) {
	Page r = Storage.OpenPage(pgno);
	if (!ReadOnly) {
		r.m_pimpl->Dirty = AllocatedPages.count(pgno);
		if (r.m_pimpl->Dirty && Storage.ProtectPages)
			MemoryMappedView::Protect(r.m_pimpl->Address, Storage.PageSize, MemoryMappedFileAccess::ReadWrite);
	}
	return r;	
}

void DbTransaction::FreePage(uint32_t pgno) {
	if (AllocatedPages.erase(pgno)) {
		EXT_LOCKED(Storage.MtxFreePages, Storage.FreePage(pgno));
	} else
		ReleasedPages.insert(pgno);
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
			EXT_FOR(uint32_t pgno, AllocatedPages) {
				Storage.AllocatedSinceCheckpointPages.reset(pgno);
			}
			EXT_FOR (uint32_t pgno, AllocatedPages) {
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
			PagedMap& m = *it->second;
			if (m.Dirty && m.Name != DbTable::Main().Name) {
				TableData td = m.GetTableData();
				ConstBuf k(m.Name.c_str(), strlen(m.Name.c_str()));
				DbCursor(_self, DbTable::Main()).Put(k, ConstBuf(&td, sizeof td));
			}
		}
			
		EXT_LOCK (Storage.MtxRoot) {
			Storage.MainTableRoot = MainTableRoot;
			Storage.LastTransactionId = TransactionId;
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
		AllocatedPages.clear();
		
		{
			unique_lock<shared_mutex> lk(Storage.ShMtx, try_to_lock);
			EXT_LOCK (Storage.MtxFreePages) {
				EXT_FOR (uint32_t pgno, ReleasedPages) {
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

static DbTable s_mainTable("__main");

DbTable& AFXAPI DbTable::Main() {
	return s_mainTable;
}

}}} // Ext::DB::KV::


