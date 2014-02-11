/*######     Copyright (c) 1997-2013 Ufasoft  http://ufasoft.com  mailto:support@ufasoft.com,  Sergey Pavlov  mailto:dev@ufasoft.com #######################################
#                                                                                                                                                                          #
# This program is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation;  #
# either version 3, or (at your option) any later version. This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the      #
# implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details. You should have received a copy of the GNU #
# General Public License along with this program; If not, see <http://www.gnu.org/licenses/>                                                                               #
##########################################################################################################################################################################*/

#include <el/ext.h>

#include "ext-sqlite.h"


#if UCFG_USE_SQLITE==3
#	include <sqlite3.h>
#	if UCFG_USE_SQLITE_MDB
//#		pragma comment(lib, "sqlite3_mdb")
#	else
#		pragma comment(lib, "sqlite3")
#	endif
#else
#	include <sqlite4.h>
#	pragma comment(lib, "sqlite4")
#endif

namespace Ext { namespace DB { namespace sqlite_(NS) {

#if UCFG_USE_SQLITE==3 && !UCFG_USE_SQLITE_MDB

class CSqliteMappedFile {
public:
	CSqliteMappedFile()
		:	m_lockLevel(0)
	{}

	int Open(const char *zName, int flags, int *pOutFlags);
	int Close();
	int Read(void *data, int size, Int64 offset);
	int Write(const void *data, int size, Int64 offset);
	int GetFileSize(Int64& fileSize);
	int Truncate(Int64 size);
	int Lock(int level);
	int Unlock(int level);
	int CheckReservedLock(int& bLocked);
	int Sync(int flags);
private:
	const sqlite3_io_methods *pMethod; /*** Must be first ***/
	sqlite3_vfs *pVfs;      /* The VFS used to open this file */
	String FilePath;
	File m_file;
	
	MemoryMappedFile m_mmap;
	MemoryMappedView m_mview;

	int m_lockLevel;
	Int64 FileSize;
};

static int MMapped_Close(sqlite3_file *file) {
	int rc = ((CSqliteMappedFile*)file)->Close();
	((CSqliteMappedFile*)file)->~CSqliteMappedFile();
	return rc;
}

static int MMapped_Read(sqlite3_file *file, void *data, int iAmt, sqlite3_int64 iOfst) {
	return ((CSqliteMappedFile*)file)->Read(data, iAmt, iOfst);
}

static int MMapped_Write(sqlite3_file *file, const void *data, int iAmt, sqlite3_int64 iOfst) {
	return ((CSqliteMappedFile*)file)->Write(data, iAmt, iOfst);
}

static int MMapped_Truncate(sqlite3_file *file, sqlite3_int64 size) {
	return ((CSqliteMappedFile*)file)->Truncate(size);
}

static int MMapped_Sync(sqlite3_file *file, int flags) {
	return ((CSqliteMappedFile*)file)->Sync(flags);
}

static int MMapped_FileSize(sqlite3_file *file, sqlite3_int64 *pSize) {
	return ((CSqliteMappedFile*)file)->GetFileSize(*pSize);
}

static int MMapped_Lock(sqlite3_file *file, int level) {
	return ((CSqliteMappedFile*)file)->Lock(level);
}

static int MMapped_Unlock(sqlite3_file *file, int level) {
	return ((CSqliteMappedFile*)file)->Unlock(level);
}

static int MMapped_CheckReservedLock(sqlite3_file *file, int *pResOut) {
	return ((CSqliteMappedFile*)file)->CheckReservedLock(*pResOut);
}

static int MMapped_FileControl(sqlite3_file *file, int op, void *pArg) {
	return SQLITE_NOTFOUND;
}

static int MMapped_SectorSize(sqlite3_file *file) {
	return 4096;
}

static int MMapped_DeviceCharacteristics(sqlite3_file *file) {
	return SQLITE_IOCAP_UNDELETABLE_WHEN_OPEN | SQLITE_IOCAP_ATOMIC512 | SQLITE_IOCAP_ATOMIC1K | SQLITE_IOCAP_ATOMIC2K | SQLITE_IOCAP_ATOMIC4K | SQLITE_IOCAP_SAFE_APPEND;		//!!!? SQLITE_IOCAP_SAFE_APPEND 
}

static int MMapped_ShmMap(sqlite3_file *file, int iPg, int pgsz, int, void volatile**) {
	Throw(E_NOTIMPL);
}

static int MMapped_ShmLock(sqlite3_file *file, int offset, int n, int flags) {
	Throw(E_NOTIMPL);
}

static void MMapped_ShmBarrier(sqlite3_file *file) {
	Throw(E_NOTIMPL);
}

static int MMapped_ShmUnmap(sqlite3_file *file, int deleteFlag) {
	Throw(E_NOTIMPL);
}

static sqlite3_io_methods s_mmappedMethods = {
	2,
	&MMapped_Close,
	&MMapped_Read,
	&MMapped_Write,
	&MMapped_Truncate,
	&MMapped_Sync,
	&MMapped_FileSize,
	&MMapped_Lock,
	&MMapped_Unlock,
	&MMapped_CheckReservedLock,
	&MMapped_FileControl,
	&MMapped_SectorSize,
	&MMapped_DeviceCharacteristics,
	&MMapped_ShmMap,
	&MMapped_ShmLock,
	&MMapped_ShmBarrier,
	&MMapped_ShmUnmap
};

static int MMapped_xOpen(sqlite3_vfs *vfs, const char *zName, sqlite3_file *file, int flags, int *pOutFlags) {
	CSqliteMappedFile *mf = new(file) CSqliteMappedFile;
	int rc = mf->Open(zName, flags, pOutFlags);
	if (SQLITE_OK == rc) {
		file->pMethods = &s_mmappedMethods;
	}
	return rc;
}


int CSqliteMappedFile::Open(const char *zName, int flags, int *pOutFlags) {
	String path;
	if (zName)
		path = zName;
	else
		path = Path::GetTempFileName();
	FilePath = path;
	FileMode mode = FileMode::Open;
	FileAccess access = FileAccess::ReadWrite;
	if (flags & SQLITE_OPEN_CREATE)
		mode = FileMode::Create;
//	if (flags & SQLITE_OPEN_READONLY)
//		access = FileAccess::Read;
	m_file.Open(path, mode, access);
	FileSize = m_file.Length;
	if (FileSize > 0) {
		m_mmap = MemoryMappedFile::CreateFromFile(m_file);
		m_mview = m_mmap.CreateView(0);
	}
	return SQLITE_OK;
}

int CSqliteMappedFile::Close() {
	m_mview.Unmap();
	m_mmap.m_hMapFile.Close();
	m_file.Close();
	return SQLITE_OK;
}

int CSqliteMappedFile::Read(void *data, int size, Int64 offset) {
	if (offset > FileSize)
		return SQLITE_FULL;
	int n = std::min(size, int(FileSize-offset));
	if (n)
		g_fastMemcpy(data, (byte*)m_mview.Address+offset, n);
	if (n < size) {
		memset((byte*)data+n, 0, size-n);
		return SQLITE_IOERR_SHORT_READ;
	} else
		return SQLITE_OK;
}

const int FILE_INCREMENT = 64*1024;

int CSqliteMappedFile::Write(const void *data, int size, Int64 offset) {
	if (FileSize < offset+size) {
		m_mview.Unmap();
		m_mmap.m_hMapFile.Close();
		m_file.Length = FileSize = (offset+size+FILE_INCREMENT-1) & ~(FILE_INCREMENT-1);
		m_mmap = MemoryMappedFile::CreateFromFile(m_file);
		m_mview = m_mmap.CreateView(0);
	}
	g_fastMemcpy((byte*)m_mview.Address+offset, data, size);
	return SQLITE_OK;
}

int CSqliteMappedFile::GetFileSize(Int64& fileSize) {
	fileSize = FileSize = m_file.Length;
	return SQLITE_OK;
}

int CSqliteMappedFile::Truncate(Int64 size) {
	m_mview.Unmap();
	m_mmap.m_hMapFile.Close();
	m_file.Seek(size);
	m_file.SetEndOfFile();
	FileSize = size;
	if (FileSize > 0) {
		m_mmap = MemoryMappedFile::CreateFromFile(m_file);
		m_mview = m_mmap.CreateView(0);
	}
	return SQLITE_OK;
}

int CSqliteMappedFile::Lock(int level) {
	++m_lockLevel;
	return SQLITE_OK;
}

int CSqliteMappedFile::Unlock(int level) {
	if (m_lockLevel > 0) {
		--m_lockLevel;
		return SQLITE_OK;
	}
	return SQLITE_ERROR;
}

#define NO_LOCK         0
#define SHARED_LOCK     1
#define RESERVED_LOCK   2
#define PENDING_LOCK    3
#define EXCLUSIVE_LOCK  4

#define PENDING_BYTE     (0x40000000)
#define RESERVED_BYTE     (PENDING_BYTE+1)


int CSqliteMappedFile::CheckReservedLock(int& bLocked) {
	if (m_lockLevel >= RESERVED_LOCK)
		bLocked = 1;
	else {
		try {
			m_file.Lock(RESERVED_BYTE, 1);
			m_file.Unlock(RESERVED_BYTE, 1);
			bLocked = 0;
		} catch (RCExc) {
			bLocked = 1;
		}
	}
	return SQLITE_OK;
}

int CSqliteMappedFile::Sync(int flags) {
	m_mview.Flush();
	return SQLITE_OK;
}

MMappedSqliteVfs::MMappedSqliteVfs() {
	m_pimpl->szOsFile = sizeof(CSqliteMappedFile);
	m_pimpl->xOpen = &MMapped_xOpen;

}

#endif // UCFG_USE_SQLITE==3
}}} // namespace Ext::DB::sqlite_(NS)::


