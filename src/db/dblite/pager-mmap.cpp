/*######   Copyright (c) 2014-2015 Ufasoft  http://ufasoft.com  mailto:support@ufasoft.com,  Sergey Pavlov  mailto:dev@ufasoft.com ####
#                                                                                                                                     #
# 		See LICENSE for licensing information                                                                                         #
#####################################################################################################################################*/

#include <el/ext.h>

#include "dblite.h"

namespace Ext { namespace DB { namespace KV {

class MMapPager;

class MMView : public ViewBase {
	typedef ViewBase base;
	typedef MMView class_type;	
public:
	 MMapPager& Pager;

	MemoryMappedView View;

	MMView(MMapPager& pager);

	~MMView() {
		if (!Flushed && Storage.UseFlush)
			View.Flush();		// Flush View syncs to the disk, FlushFileBuffers() is not enough
	}

	void Load() override;

	void Flush() override {
		Flushed = true;
		if (Storage.UseFlush)
			View.Flush();
	}

	void *GetAddress() override { return View.Address; }

	PageObj *AllocPage(uint32_t pgno) override {
		Load();
		return new PageObj(Storage);
	}
};

class MappedFile : public InterlockedObject {
public:
	Ext::MemoryMappedFile MemoryMappedFile;
};


class MMapPager : public Pager {
	typedef Pager base;
public:
	list<ptr<MappedFile>> Mappings;
	list<ptr<ViewBase>> m_fullViews;

	MMapPager(KVStorage& storage)
		:	base(storage)
	{}
protected:
	void AddFullMapping(uint64_t fileLength) override {
		TRC(2, "File resizing to " << fileLength / (1024 * 1024) << " MiB")
    	EXT_LOCK (Storage.MtxViews) {
    		ptr<MappedFile> m = new MappedFile;
    		MemoryMappedFileAccess acc = Storage.ReadOnly ? MemoryMappedFileAccess::Read : MemoryMappedFileAccess::ReadWrite;
    		m->MemoryMappedFile = MemoryMappedFile::CreateFromFile(Storage.DbFile, nullptr, Storage.FileLength = fileLength, acc);
    		Mappings.push_back(m);
    		if (Storage.m_viewMode == ViewMode::Full) {
    			ptr<MMView> v = new MMView(_self);
    			v->View = Mappings.back()->MemoryMappedFile.CreateView(0, 0, acc);
				v->Loaded = true;
				v->N = 0;
    			Storage.ViewAddress = v->View.Address;
    			m_fullViews.push_back(v);
				Storage.Views.resize(1);
    			Storage.Views[0] = v;
    		}
    	}
	}

	void Close() override {
		m_fullViews.clear();
		Mappings.clear();
	}

	ViewBase *CreateView() override { return new MMView(_self); }

};

MMView::MMView(MMapPager& pager)
	: base(pager.Storage)
	, Pager(pager)
{
}

void MMView::Load() {
	if (!Loaded) {
		size_t viewSize = Storage.ViewSize;
		if (Storage.ReadOnly && uint64_t(N+1)*Storage.ViewSize > Storage.FileLength)
			viewSize = Storage.FileLength % Storage.ViewSize;

		View = Pager.Mappings.back()->MemoryMappedFile.CreateView(uint64_t(N) * Storage.ViewSize, viewSize); //!!!R , Storage.ReadOnly ? MemoryMappedFileAccess::Read : MemoryMappedFileAccess::ReadWrite);
		if (Storage.ProtectPages && !Storage.ReadOnly)
			MemoryMappedView::Protect(View.Address, viewSize, MemoryMappedFileAccess::Read);
		Loaded = true;
	}
}


Pager *CreateMMapPager(KVStorage& storage) {
	return new MMapPager(storage);
}



}}} // Ext::DB::KV::


