/*######   Copyright (c) 2014-2015 Ufasoft  http://ufasoft.com  mailto:support@ufasoft.com,  Sergey Pavlov  mailto:dev@ufasoft.com ####
#                                                                                                                                     #
# 		See LICENSE for licensing information                                                                                         #
#####################################################################################################################################*/

#include <el/ext.h>

#if UCFG_WIN32
#	include <memoryapi.h>

#	include <el/libext/win32/ext-win.h>
#	include <el/win/policy.h>
#endif

#include "dblite.h"

namespace Ext { namespace DB { namespace KV {

static bool InitUseLargePages() {
#if UCFG_WIN32
	try {
		DBG_LOCAL_IGNORE_WIN32(ERROR_NOT_ALL_ASSIGNED);

		AccessToken(Process::GetCurrentProcess()).AdjustPrivilege("SeLockMemoryPrivilege");
		return true;
	} catch (RCExc) {
	}
#endif // UCFG_WIN32
	return false;
}

static bool s_UseLargePages = InitUseLargePages();



class AllocatedPageObj : public PageObj {
	typedef PageObj base;
public:
	AllocatedPageObj(KVStorage& storage, void *a)
		:	base(storage)
	{
		if (a)
			m_address = a;
		else {
			m_ownMem.Alloc(storage.PageSize, max(storage.PageSize, storage.PhysicalSectorSize));
			m_address = m_ownMem.get();
		}
		Flushed = false;		
	}

	~AllocatedPageObj() {
		Flush();
	}

	void Flush() override;
private:
	AlignedMem m_ownMem;
};

class BufferView : public ViewBase {
	typedef ViewBase base;
public:
	atomic<uint8_t*> aAddress;

	BufferView(KVStorage& storage)
		:	base(storage)
		,	aAddress(0)
	{
	}

	void LoadOnDemand() {
		VirtualMem vmem(Storage.ViewSize, MemoryMappedFileAccess::ReadWrite, s_UseLargePages);
		Storage.DbFile.Read(vmem.get(), Storage.ViewSize, uint64_t(N)*Storage.ViewSize);
		if (Storage.ReadOnly) {
#if UCFG_WIN32
			if (!s_UseLargePages)																				// Win32: Large Pages don't support ReadOnly access
#endif
				MemoryMappedView::Protect(vmem.get(), Storage.ViewSize, MemoryMappedFileAccess::Read);
		}
		uint8_t *prev = 0;
		if (aAddress.compare_exchange_strong(prev, (uint8_t*)vmem.get())) {
			m_vmem.Attach(vmem.Detach(), vmem.size());
			Loaded = true;
			Flushed = true;	
		}
	}

	void Flush() override {
		if (!exchange(Flushed, true) && aAddress.load() && !Storage.ReadOnly)
			Storage.DbFile.Write(aAddress, Storage.ViewSize, uint64_t(N)*Storage.ViewSize);			//!!!TODO race condition
	}

	void *GetAddress() override {
		if (!Loaded) {
			LoadOnDemand();
		}
		return aAddress;
	}

	PageObj *AllocPage(uint32_t pgno) override {
		AllocatedPageObj *r = new AllocatedPageObj(Storage, Loaded ? (uint8_t*)m_vmem.get() + uint64_t(pgno & ((1<<Storage.m_bitsViewPageRatio) - 1))*Storage.PageSize : nullptr);
		r->View = this;
		return r;
	}
private:
	VirtualMem m_vmem;
};

void AllocatedPageObj::Flush() {
	if (!Flushed) {
		Flushed = true;
		if (m_ownMem.get()) {
			BufferView *view = static_cast<BufferView*>(View.get());
			KVStorage& stg = View->Storage;
			if (view->Loaded) {
				uint8_t *newAddress = (uint8_t*)view->aAddress + uint64_t(N & ((1<<stg.m_bitsViewPageRatio) - 1))*stg.PageSize;
				memcpy(newAddress, exchange(m_address, newAddress), stg.PageSize);
				View->Flushed = false;
			} else
				stg.DbFile.Write(m_address, stg.PageSize, uint64_t(N)*stg.PageSize);			//!!!TODO race condition
		}
	}
}

class BufferPager : public Pager {
	typedef Pager base;
public:
	BufferPager(KVStorage& storage)
		:	base(storage)
	{}
protected:
	void Flush() override {
		EXT_FOR (PageObj *po, Storage.OpenedPages) {
			if (po)
				po->Flush();
		}
		base::Flush();
	}

	ViewBase *CreateView() override { return new BufferView(Storage); }

};

Pager *CreateBufferPager(KVStorage& storage) {
	return new BufferPager(storage);
}

#if UCFG_WIN32 && defined(_AFXDLL)

class CDbliteDllServer : public CDllServer {
	HRESULT OnRegister() override {
		try {
			DBG_LOCAL_IGNORE_CONDITION(errc::permission_denied);
			Lsa::Policy(nullptr, MAXIMUM_ALLOWED).AddAccountRight(AccessToken(Process::GetCurrentProcess()).User, "SeLockMemoryPrivilege");		// to enable MEM_LARGE_PAGES
		} catch (RCExc) {
		}
		return S_OK;
	}
} s_dbliteDllServer;


#endif // UCFG_WIN32 && defined(_AFXDLL)



}}} // Ext::DB::KV::


