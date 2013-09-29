/*######     Copyright (c) 1997-2013 Ufasoft  http://ufasoft.com  mailto:support@ufasoft.com,  Sergey Pavlov  mailto:dev@ufasoft.com #######################################
#                                                                                                                                                                          #
# This program is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation;  #
# either version 3, or (at your option) any later version. This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the      #
# implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details. You should have received a copy of the GNU #
# General Public License along with this program; If not, see <http://www.gnu.org/licenses/>                                                                               #
##########################################################################################################################################################################*/

#include <el/ext.h>

#if UCFG_USE_POSIX
#	include <sys/mman.h>
#endif

#if UCFG_WIN32
#	include <windows.h>
#endif

namespace Ext {
using namespace std;

static int MemoryMappedFileAccessToInt(MemoryMappedFileAccess access) {
#if UCFG_USE_POSIX
	switch (access) {
	case MemoryMappedFileAccess::None: return PROT_NONE;
	case MemoryMappedFileAccess::ReadWrite:			return PROT_READ | PROT_WRITE;
	case MemoryMappedFileAccess::Read:				return PROT_READ;
	case MemoryMappedFileAccess::Write:				return PROT_WRITE;
	case MemoryMappedFileAccess::CopyOnWrite:		return PROT_READ | PROT_WRITE;
	case MemoryMappedFileAccess::ReadExecute:		return PROT_READ | PROT_EXEC;
	case MemoryMappedFileAccess::ReadWriteExecute:	return PROT_READ | PROT_WRITE | PROT_EXEC;
	default:
		Throw(E_INVALIDARG);	
	}
#else
	switch (access) {
	case MemoryMappedFileAccess::None: return 0;
	case MemoryMappedFileAccess::ReadWrite:			return PAGE_READWRITE;
	case MemoryMappedFileAccess::Read:				return PAGE_READONLY;
	case MemoryMappedFileAccess::Write:				return PAGE_READWRITE;
	case MemoryMappedFileAccess::CopyOnWrite:		return PAGE_EXECUTE_READ;
	case MemoryMappedFileAccess::ReadExecute:		return PAGE_EXECUTE_READ;
	case MemoryMappedFileAccess::ReadWriteExecute:	return PAGE_EXECUTE_READWRITE;
	default:
		Throw(E_INVALIDARG);	
	}
#endif
}


MemoryMappedView::MemoryMappedView(EXT_RV_REF(MemoryMappedView) rv)
	:	Offset(exchange(rv.Offset, 0))
	,	Address(exchange(rv.Address, nullptr))
	,	Size(exchange(rv.Size, 0))
	,	Access(rv.Access)
	,	AddressFixed(rv.AddressFixed)
{
}

MemoryMappedView& MemoryMappedView::operator=(EXT_RV_REF(MemoryMappedView) rv) {
	Unmap();
	Address = exchange(rv.Address, nullptr);
	Offset = exchange(rv.Offset, 0);
	Size = exchange(rv.Size, 0);
	Access = rv.Access;
	AddressFixed = rv.AddressFixed;
	return *this;
}

void MemoryMappedView::Map(MemoryMappedFile& file, UInt64 offset, size_t size, void *desiredAddress) {
	if (Address)
		Throw(E_EXT_AlreadyOpened);
	Offset = offset;
	Size = size;
#if UCFG_USE_POSIX
	int prot = MemoryMappedFileAccessToInt(Access);
	int flags = Access==MemoryMappedFileAccess::CopyOnWrite ? 0 : MAP_SHARED;
	if (AddressFixed)
		flags |= MAP_FIXED;
	void *a = ::mmap(desiredAddress, Size, prot, flags, file.GetHandle(), offset);
	CCheck(a != MAP_FAILED);
	Address = a;
#else
	DWORD dwAccess = 0;
	switch (Access) {
	case MemoryMappedFileAccess::ReadWrite:			dwAccess = FILE_MAP_READ | FILE_MAP_WRITE;	break;
	case MemoryMappedFileAccess::Read:				dwAccess = FILE_MAP_READ;					break;
	case MemoryMappedFileAccess::Write:				dwAccess = FILE_MAP_WRITE;					break;
	case MemoryMappedFileAccess::CopyOnWrite:		dwAccess = FILE_MAP_COPY;					break;
	case MemoryMappedFileAccess::ReadExecute:		dwAccess = FILE_MAP_READ;					break;
	case MemoryMappedFileAccess::ReadWriteExecute:	dwAccess = FILE_MAP_ALL_ACCESS;				break;
	}
#	if UCFG_WCE
	Win32Check((Address = ::MapViewOfFile(file.GetHandle(), dwAccess, DWORD(Offset>>32), DWORD(Offset), Size)) != 0);
#	else
	Win32Check((Address = ::MapViewOfFileEx(file.GetHandle(), dwAccess, DWORD(Offset>>32), DWORD(Offset), Size, desiredAddress)) != 0);
#	endif
#endif
}

void MemoryMappedView::Unmap() {
	if (void *a = exchange(Address, nullptr)) {
#if UCFG_USE_POSIX
		CCheck(::munmap(a, Size));
#else
		Win32Check(::UnmapViewOfFile(a));
#endif
	}
}

void MemoryMappedView::Flush() {
#if UCFG_USE_POSIX
	CCheck(::msync(Address, Size, MS_SYNC));
#elif UCFG_WIN32
	Win32Check(::FlushViewOfFile(Address, Size));
#else
	Throw(E_NOTIMPL);
#endif
}

void AFXAPI MemoryMappedView::Protect(void *p, size_t size, MemoryMappedFileAccess access) {
	int prot = MemoryMappedFileAccessToInt(access);
#if UCFG_USE_POSIX
	CCheck(::mprotect(p, size, prot));
#else
	DWORD prev;
	Win32Check(::VirtualProtect(p, size, prot, &prev));
#endif
}

MemoryMappedView MemoryMappedFile::CreateView(UInt64 offset, size_t size, MemoryMappedFileAccess access) {
	MemoryMappedView r;
	r.Access = access;
	r.Map(_self, offset, size);
	return std::move(r);
}

MemoryMappedFile MemoryMappedFile::CreateFromFile(File& file, RCString mapName, UInt64 capacity, MemoryMappedFileAccess access) {
	int prot = MemoryMappedFileAccessToInt(access);
	MemoryMappedFile r;
#if UCFG_WIN32	
	r.m_hMapFile.Attach(::CreateFileMapping(file.DangerousGetHandle(), 0, prot, UInt32(capacity>>32), UInt32(capacity), mapName));	
#else
	Throw(E_NOTIMPL);
#endif
	return std::move(r);
}

MemoryMappedFile MemoryMappedFile::CreateFromFile(RCString path, FileMode mode, RCString mapName, UInt64 capacity, MemoryMappedFileAccess access) {
	File file;
	file.Open(path, mode);
	return CreateFromFile(file, mapName, capacity, access);
};


MemoryMappedFile MemoryMappedFile::OpenExisting(RCString mapName, MemoryMappedFileRights rights, HandleInheritability inheritability) {
	MemoryMappedFile r;
#if UCFG_WIN32
	DWORD dwAccess = 0;
	if (int(rights) & int(MemoryMappedFileRights::Read))
		dwAccess |= FILE_MAP_READ;
	if (int(rights) & int(MemoryMappedFileRights::Write))
		dwAccess |= FILE_MAP_WRITE;
	if (int(rights) & int(MemoryMappedFileRights::Execute))
		dwAccess |= FILE_MAP_EXECUTE;
	r.m_hMapFile.Attach(::OpenFileMapping(dwAccess, inheritability==HandleInheritability::Inheritable, mapName));
#else
	Throw(E_NOTIMPL);
#endif
	return std::move(r);
}



} // Ext::

