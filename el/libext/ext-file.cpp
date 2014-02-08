/*######     Copyright (c) 1997-2013 Ufasoft  http://ufasoft.com  mailto:support@ufasoft.com,  Sergey Pavlov  mailto:dev@ufasoft.com #######################################
#                                                                                                                                                                          #
# This program is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation;  #
# either version 3, or (at your option) any later version. This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the      #
# implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details. You should have received a copy of the GNU #
# General Public License along with this program; If not, see <http://www.gnu.org/licenses/>                                                                               #
##########################################################################################################################################################################*/

#include <el/ext.h>

#if UCFG_USE_POSIX
#	include <utime.h>
#	include <glob.h>
#endif

#if UCFG_WIN32
#	include <shlwapi.h>

#	include <el/libext/win32/ext-win.h>
#endif

#if UCFG_WIN32_FULL
#	pragma comment(lib, "shlwapi")

#	include <el/libext/win32/ext-full-win.h>
#endif

namespace Ext {
using namespace std;

static bool WithDirSeparator(RCString s) {
	if (s.IsEmpty())
		return false;
	wchar_t ch = s[s.Length-1];
	return ch==Path::DirectorySeparatorChar || ch==Path::AltDirectorySeparatorChar;
}

String AFXAPI AddDirSeparator(RCString s) {
	return WithDirSeparator(s) ? s : s+String(Path::DirectorySeparatorChar);
}

String AFXAPI RemoveDirSeparator(RCString s, bool bOnEndOnly) {
	return WithDirSeparator(s) && (!bOnEndOnly || s.Length!=1) ? s.Left(int(s.Length-1)) : s;
}

Path::CSplitPath Path::SplitPath(RCString path) {
	CSplitPath sp;
#ifdef _WIN32
	TCHAR drive[_MAX_DRIVE],
		dir[_MAX_DIR],
		fname[_MAX_FNAME],
		ext[_MAX_EXT];
#	if UCFG_WCE
	_tsplitpath_s(path, drive, sizeof drive,
		dir, sizeof dir,
		fname, sizeof fname,
		ext, sizeof ext);
#	else
	_tsplitpath(path, drive, dir, fname, ext);
#	endif
	sp.m_drive = drive;
	sp.m_dir = dir;
	sp.m_fname = fname;
	sp.m_ext = ext;
#else
	int fn = path.LastIndexOf(DirectorySeparatorChar);
	String sSnExt = path.Mid(fn+1);

	int ext = sSnExt.LastIndexOf('.');
	if (ext != -1)
		sp.m_ext = sSnExt.Mid(ext);
	else
		ext = sSnExt.Length;
	sp.m_fname = sSnExt.Left(ext);
	sp.m_dir = path.Left(fn+1);
#endif
	return sp;
}

bool Path::IsPathRooted(RCString path) {
	wchar_t ch = SplitPath(path).m_dir[0];
	return ch==Path::DirectorySeparatorChar || ch==Path::AltDirectorySeparatorChar;
}

String Path::GetTempPath() {
#if UCFG_USE_POSIX
	char buf[PATH_MAX+1] = "XXXXXX";
	int fd = CCheck(::mkstemp(buf));
	close(fd);
	return buf;
#else
	TCHAR buf[MAX_PATH];
	DWORD r = ::GetTempPath(_countof(buf), buf);
	Win32Check(r && r<_countof(buf)); //!!! need to improve
	return buf;
#endif
}

pair<String, UINT> Path::GetTempFileName(RCString path, RCString prefix, UINT uUnique) {
#if UCFG_USE_POSIX
	char buf[PATH_MAX+1];
	ZeroStruct(buf);
	int fd = CCheck(::mkstemp(strncpy(buf, Path::Combine(path, prefix+"XXXXXX"), _countof(buf)-1)));
	close(fd);
	return pair<String, UINT>(buf, 1);
#else
	TCHAR buf[MAX_PATH];
	UINT r = Win32Check(::GetTempFileName(path, prefix, uUnique, buf));
	return pair<String, UINT>(buf, r);
#endif
}

String AFXAPI Path::GetDirectoryName(RCString path) {
	if (path.IsEmpty())
		return nullptr;
	CSplitPath sp = SplitPath(path);  
	return RemoveDirSeparator(sp.m_drive+sp.m_dir, true);
}

String AFXAPI Path::GetFileName(RCString path) {
	if (path == nullptr)
		return nullptr;
	CSplitPath sp = SplitPath(path);  
	return sp.m_fname+sp.m_ext;
}

String AFXAPI Path::GetFileNameWithoutExtension(RCString path) {
	return SplitPath(path).m_fname;
}

String AFXAPI Path::GetExtension(RCString path) {
	if (!path)
		return nullptr;
	return SplitPath(path).m_ext;
}

String Path::Combine(RCString path1, RCString path2) {
	if (path2.IsEmpty())
		return path1;
	if (path1.IsEmpty() || IsPathRooted(path2))
		return path2;
	return AddDirSeparator(path1)+path2;
}

String Path::ChangeExtension(RCString path, RCString ext) {
	return Combine(GetDirectoryName(path), GetFileNameWithoutExtension(path) + ext);
}

String Path::GetPhysicalPath(RCString p) {
	String path = p;
#if UCFG_WIN32_FULL
	while (true) {
		Path::CSplitPath sp = SplitPath(path);
		vector<String> vec = System.QueryDosDevice(sp.m_drive);				// expand SUBST-ed drives
		if (vec.empty() || vec[0].Left(4) != "\\??\\")
			break;
		path = vec[0].Mid(4)+sp.m_dir+sp.m_fname+sp.m_ext;
	}
#endif
	return path;
}

#if UCFG_WIN32_FULL && UCFG_USE_REGEX
static StaticWRegex s_reDosName("^\\\\\\\\\\?\\\\([A-Za-z]:.*)");   //  \\?\x:
#endif

String Path::GetTruePath(RCString p) {
#if UCFG_USE_POSIX	
	char buf[PATH_MAX];
	for (const char *psz = p;; psz = buf) {
		int len = ::readlink(psz, buf, sizeof(buf)-1);
		if (len == -1) {
			if (errno == EINVAL)
				return psz;
			CCheck(-1);
		}
		buf[len] = 0;
	}
#elif UCFG_WIN32_FULL
	TCHAR buf[_MAX_PATH];
	DWORD len = ::GetLongPathName(p, buf, _countof(buf)-1);
	Win32Check(len != 0);
	buf[len] = 0;

	typedef DWORD (WINAPI *PFN_GetFinalPathNameByHandle)(HANDLE hFile, LPTSTR lpszFilePath, DWORD cchFilePath, DWORD dwFlags);

	DlProcWrap<PFN_GetFinalPathNameByHandle> pfn("KERNEL32.DLL", EXT_WINAPI_WA_NAME(GetFinalPathNameByHandle));
	if (!pfn)
		return buf;
	TCHAR buf2[_MAX_PATH];
	File file;
	file.Attach(::CreateFile(buf, 0, 0, 0, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, 0));
	len = pfn(Handle(file), buf2, _countof(buf2)-1, 0);
	Win32Check(len != 0);
	buf2[len] = 0;	
#if UCFG_USE_REGEX
	wcmatch m;
	if (regex_search(buf2, m, s_reDosName))
		return m[1];
#else
	String sbuf(buf2);				//!!! incoplete check, better to use Regex
	int idx = sbuf.Find(':');
	if (idx != -1)
		return sbuf.Mid(idx-1);
#endif
	return buf2;
#else
	return p;
#endif
}

DirectoryInfo Directory::CreateDirectory(RCString path) {
	vector<String> ar = path.Split(String(Path::DirectorySeparatorChar)+String(Path::AltDirectorySeparatorChar));
	String dir;
	bool b = true;
	for (size_t i=0; i<ar.size(); i++) {
		dir = AddDirSeparator(dir+ar[i]);
#if UCFG_USE_POSIX
		if (::mkdir(dir, 0777) < 00 && errno!=EEXIST && errno!=EISDIR)
			CCheck(-1);
	}
#else
		b = ::CreateDirectory(dir, 0);
	}
	Win32Check(b, ERROR_ALREADY_EXISTS);
#endif
	return DirectoryInfo(path);
}

vector<String> Directory::GetItems(RCString path, RCString searchPattern, bool bDirs) {
#if UCFG_USE_POSIX
#	if UCFG_STLSOFT
	typedef unixstl::glob_sequence gs_t;
	gs_t gs(path.c_str(), searchPattern.c_str(), gs_t::files | (bDirs ? gs_t::directories : 0));
	return vector<String>(gs.begin(), gs.end());
#	else
	vector<String> r;
	glob_t gt;
	CCheck(::glob(Path::Combine(path, searchPattern), GLOB_MARK, 0, &gt));
	for (int i=0; i<gt.gl_pathc; ++i) {
		const char *p = gt.gl_pathv[i];
		size_t len = strlen(p);
		if (p[len-1] != '/')
			r.push_back(p);
		else if (bDirs)
			r.push_back(String(p, len-1));
	}
	globfree(&gt);
	if (searchPattern == "*") {
		CCheck(::glob(Path::Combine(path, ".*"), GLOB_MARK, 0, &gt));
		for (int i=0; i<gt.gl_pathc; ++i) {
			const char *p = gt.gl_pathv[i];
			size_t len = strlen(p);
			if (p[len-1] != '/')
				r.push_back(p);
			else if (bDirs) {
				String s(p, len-1);
				if (s.Right(3) != "/.." && s.Right(2) != "/.")
					r.push_back(s);
			}
		}
		globfree(&gt);
	}
	return r;
#	endif
#else
	vector<String> r;
	CWinFileFind ff(Path::Combine(path, searchPattern));
	for (WIN32_FIND_DATA fd; ff.Next(fd);) {
		if (!(bool(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) ^ bDirs)) {
			String name = fd.cFileName;
			if (name != "." && name != "..")
				r.push_back(Path::Combine(path, name));
		}
	}
	return r;
#endif
}

static bool FileDirectoryExists(RCString path, bool bDir) {
#if UCFG_USE_POSIX
	struct stat st;
	int rc = ::stat(path, &st);
	if (rc < 0) {
		if (errno != ENOENT)
			CCheck(rc);
		return false;
	} else
		return !(st.st_mode & S_IFDIR) ^ bDir;
#else
	DWORD r = ::GetFileAttributes(path);
	return r!=INVALID_FILE_ATTRIBUTES &&  (!(r & FILE_ATTRIBUTE_DIRECTORY) ^ bDir);
#endif
}

bool Directory::Exists(RCString path) {
	return FileDirectoryExists(path, true);
}

void Directory::Delete(RCString path, bool bRecursive) {
	if (bRecursive) {
		vector<String> ar =  Directory::GetFiles(path);
		for (int i=ar.size(); i--;)
			FileInfo(ar[i]).Delete();
		ar = Directory::GetDirectories(path);
		for (int i=ar.size(); i--;)
			Delete(ar[i], true);
	}
#if UCFG_USE_POSIX
	CCheck(::rmdir(path));
#else
	Win32Check(::RemoveDirectory(path));
#endif
}

#if UCFG_WIN32
bool File::s_bCreateFileWorksWithMMF = (System.Version.dwPlatformId == VER_PLATFORM_WIN32_WINDOWS) ||
	(Environment.OSVersion.Platform==PlatformID::Win32NT) ||
	(Environment.OSVersion.Version.Major >= 5); // CE
#endif

File::File() {
}

File::~File() {
}

void File::Open(const File::OpenInfo& oi) {
#if UCFG_USE_POSIX
	if (Directory::Exists(oi.Path))							// open() opens dirs, but we need not it
		Throw(E_ACCESSDENIED);

	int oflag = 0;
	switch (oi.Access) {
	case FileAccess::Read:	oflag = O_RDONLY;	break;
	case FileAccess::Write:	oflag = O_WRONLY;	break;
	case FileAccess::ReadWrite:	oflag = O_RDWR;	break;
	default:
		Throw(E_INVALIDARG);
	}
	switch (oi.Mode) {
	case FileMode::CreateNew:
		oflag |= O_CREAT | O_EXCL;
		break;
	case FileMode::Create:
		oflag |= O_CREAT | O_TRUNC;
		break;
	case FileMode::Open:
		break;
	case FileMode::OpenOrCreate:
		oflag |= O_CREAT;
		break;
	case FileMode::Append:
		oflag |= O_APPEND;
		break;
	case FileMode::Truncate:
		oflag |= O_TRUNC;
		break;
	default:
		Throw(E_INVALIDARG);
	}
	int fd = CCheck(::open(oi.Path, oflag, 0644));
	Attach((HANDLE)(uintptr_t)fd);
#else
	DWORD dwAccess = 0;
	switch (oi.Access) {
	case FileAccess::Read:		dwAccess = GENERIC_READ;	break;
	case FileAccess::Write:		dwAccess = GENERIC_WRITE;	break;
	case FileAccess::ReadWrite:	dwAccess = GENERIC_READ|GENERIC_WRITE;	break;
	default:
		Throw(E_INVALIDARG);
	}
	DWORD dwShareMode = 0;
	if ((int)oi.Share & (int)FileShare::Read)
		dwShareMode |= FILE_SHARE_READ;
	if ((int)oi.Share & (int)FileShare::Write)
		dwShareMode |= FILE_SHARE_WRITE;
	if ((int)oi.Share & (int)FileShare::Delete)
		dwShareMode |= FILE_SHARE_DELETE;
	DWORD dwCreateFlag;
	switch (oi.Mode) {
	case FileMode::CreateNew:
		dwCreateFlag = CREATE_NEW;
		break;
	case FileMode::Create:
		dwCreateFlag = CREATE_ALWAYS;
		break;
	case FileMode::Open:
		dwCreateFlag = OPEN_EXISTING;
		break;
	case FileMode::OpenOrCreate:
	case FileMode::Append:
		dwCreateFlag = OPEN_ALWAYS;
		break;
	case FileMode::Truncate:
		dwCreateFlag = TRUNCATE_EXISTING;
		break;
	default:
		Throw(E_INVALIDARG);
	}
	SECURITY_ATTRIBUTES sa = { sizeof sa };
	sa.bInheritHandle = (int)oi.Share & (int)FileShare::Inheritable;
	DWORD dwFlagsAndAttributes = FILE_ATTRIBUTE_NORMAL;
	dwFlagsAndAttributes |= oi.BufferingEnabled ? 0 : FILE_FLAG_NO_BUFFERING;
	dwFlagsAndAttributes |= oi.RandomAccess ? FILE_FLAG_RANDOM_ACCESS : 0;
	Attach(::CreateFile(ExcLastStringArgKeeper(oi.Path), dwAccess, dwShareMode, &sa, dwCreateFlag, dwFlagsAndAttributes, NULL));
	if (oi.Mode == FileMode::Append)
		SeekToEnd(); 
#endif	// UCFG_USE_POSIX
}

void File::Open(RCString path, FileMode mode, FileAccess access, FileShare share) {
	OpenInfo oi;
	oi.Path = path;
	oi.Mode = mode;
	oi.Access = access;
	oi.Share = share;
	Open(oi);
}

bool File::Exists(RCString path) {
	return FileDirectoryExists(path, false);
}

void AFXAPI File::Delete(RCString path) {
#if UCFG_USE_POSIX
	CCheck(::remove(ExcLastStringArgKeeper(path)));
#else
	Win32Check(::DeleteFile(ExcLastStringArgKeeper(path)), ERROR_FILE_NOT_FOUND);
#endif
}

void AFXAPI File::Copy(RCString src, RCString dest, bool bOverwrite) {
#if UCFG_USE_POSIX
	ifstream ifs((const char*)src, ios::binary);
	ofstream ofs((const char*)dest, ios::openmode(ios::binary | (bOverwrite ? ios::trunc : 0)));
	if (!ofs)
		Throw(E_FAIL);
	while (ifs) {
		char buf[4096];
		ifs.read(buf, sizeof buf);
		ofs.write(buf, ifs.gcount());		
	}
#else
	Win32Check(::CopyFile(src, dest, !bOverwrite));
#endif
}

void AFXAPI File::Move(RCString src, RCString dest) {
#if UCFG_USE_POSIX
	CCheck(::rename(src, dest));
#else
	Win32Check(::MoveFile(src, dest));
#endif
}

Blob File::ReadAllBytes(RCString path) {
	FileStream stm(path, FileMode::Open, FileAccess::Read);
	UInt64 len = stm.Length;
	if (len > numeric_limits<size_t>::max())
		Throw(E_OUTOFMEMORY);
	Blob blob(0, (size_t)len);
	stm.ReadBuffer(blob.data(), blob.Size);
	return blob;
}

void File::WriteAllBytes(RCString path, const ConstBuf& mb) {
	FileStream(path, FileMode::Create, FileAccess::Write).WriteBuf(mb);
}

String File::ReadAllText(RCString path, Encoding *enc) {
	return enc->GetChars(ReadAllBytes(path));
}

void File::WriteAllText(RCString path, RCString contents, Encoding *enc) {
	WriteAllBytes(path, enc->GetBytes(contents));
}

#if !UCFG_USE_POSIX

void File::Create(RCString fileName, DWORD dwDesiredAccess, DWORD dwShareMode, DWORD dwCreationDisposition, DWORD dwFlagsAndAttributes, HANDLE hTemplateFile, LPSECURITY_ATTRIBUTES lpsa) {
#if UCFG_USE_POSIX
	int oflag = 0;
	if ((dwDesiredAccess & (GENERIC_READ|GENERIC_WRITE)) == (GENERIC_READ|GENERIC_WRITE))
		oflag = O_RDWR;
	else if (dwDesiredAccess & GENERIC_READ)
		oflag = O_RDONLY;
	else if (dwDesiredAccess & GENERIC_WRITE)
		oflag = O_WRONLY;
	Attach((HANDLE)CCheck(::open(fileName, oflag)));
#else
	Attach(::CreateFile(fileName, dwDesiredAccess, dwShareMode, lpsa, dwCreationDisposition, dwFlagsAndAttributes, hTemplateFile));
#endif
}

void File::CreateForMapping(LPCTSTR lpFileName, DWORD dwDesiredAccess, DWORD dwShareMode, DWORD dwCreationDisposition, DWORD dwFlagsAndAttributes, HANDLE hTemplateFile, LPSECURITY_ATTRIBUTES lpsa) {
#if UCFG_WCE
	if (!s_bCreateFileWorksWithMMF) {
		Attach(::CreateFileForMapping(lpFileName, dwDesiredAccess, dwShareMode, lpsa, dwCreationDisposition, dwFlagsAndAttributes, hTemplateFile));
		m_bFileForMapping = true;
		return;
	}
#endif
	Create(lpFileName, dwDesiredAccess, dwShareMode, dwCreationDisposition, dwFlagsAndAttributes, hTemplateFile, lpsa);
}

bool File::Read(void *buf, UInt32 len, UInt32 *read, OVERLAPPED *pov) {
	bool b = ::ReadFile(HandleAccess(_self), buf, len, (DWORD*)read, pov);
	if (!b && ::GetLastError()==ERROR_BROKEN_PIPE) {
		*read = 0;
		return true;
	}
	return CheckPending(b);
}

bool File::Write(const void *buf, UInt32 len, UInt32 *written, OVERLAPPED *pov) {
	return CheckPending(::WriteFile(HandleAccess(_self), buf, len, (DWORD*)written, pov));
}

bool File::DeviceIoControl(int code, LPCVOID bufIn, size_t nIn, LPVOID bufOut, size_t nOut, LPDWORD pdw, LPOVERLAPPED pov) {
	return CheckPending(::DeviceIoControl(HandleAccess(_self), code, (void*)bufIn, (DWORD)nIn, bufOut, (DWORD)nOut, pdw, pov));
}

void File::CancelIo() {
	static CDynamicLibrary dll;
	if (!dll)
		dll.Load("kernel32.dll");
	typedef BOOL(__stdcall *C_CancelIo)(HANDLE);
	Win32Check(((C_CancelIo)dll.GetProcAddress((LPCTSTR)String("CancelIo")))(HandleAccess(_self)));
}

bool File::CheckPending(BOOL b) {
	if (b)
		return true;
	Win32Check(GetLastError() == ERROR_IO_PENDING);
	return false;
}

void File::SetEndOfFile() {
	Win32Check(::SetEndOfFile(HandleAccess(_self)));
}


#if !UCFG_WCE
String AFXAPI AfxGetRoot(LPCTSTR lpszPath) {
	ASSERT(lpszPath != NULL);

	TCHAR buf[_MAX_PATH];
	lstrcpyn(buf, lpszPath, _MAX_PATH);
	buf[_MAX_PATH-1] = 0;
	Win32Check(::PathStripToRoot(buf));
	return buf;
}

// turn a file, relative path or other into an absolute path
bool AFXAPI AfxFullPath(LPTSTR lpszPathOut, LPCTSTR lpszFileIn)
	// lpszPathOut = buffer of _MAX_PATH
	// lpszFileIn = file, relative path or absolute path
	// (both in ANSI character set)
{
	ASSERT(AfxIsValidAddress(lpszPathOut, _MAX_PATH));

	// first, fully qualify the path name
	LPTSTR lpszFilePart;
	if (!GetFullPathName(lpszFileIn, _MAX_PATH, lpszPathOut, &lpszFilePart))
	{
#ifdef _DEBUG
		//!!!		if (lpszFileIn[0] != '\0')
		//!!!			TRACE1("Warning: could not parse the path '%s'.\n", lpszFileIn);
#endif
		lstrcpyn(lpszPathOut, lpszFileIn, _MAX_PATH); // take it literally
		return FALSE;
	}

	String strRoot = AfxGetRoot(lpszPathOut);
	if (!::PathIsUNC( strRoot ))
	{
		// get file system information for the volume
		DWORD dwFlags, dwDummy;
		if (!GetVolumeInformation(strRoot, NULL, 0, NULL, &dwDummy, &dwFlags,
			NULL, 0))
		{
			//!!!	TRACE1("Warning: could not get volume information '%s'.\n", (LPCTSTR)strRoot);
			return FALSE;   // preserving case may not be correct
		}

		// not all characters have complete uppercase/lowercase
		if (!(dwFlags & FS_CASE_IS_PRESERVED))
			CharUpper(lpszPathOut);

		// assume non-UNICODE file systems, use OEM character set
		if (!(dwFlags & FS_UNICODE_STORED_ON_DISK))
		{
			WIN32_FIND_DATA data;
			HANDLE h = FindFirstFile(lpszFileIn, &data);
			if (h != INVALID_HANDLE_VALUE)
			{
				FindClose(h);
				lstrcpy(lpszFilePart, data.cFileName);
			}
		}
	}
	return TRUE;
}


bool File::GetStatus(RCString lpszFileName, CFileStatus& rStatus) {
	// attempt to fully qualify path first
	if (!AfxFullPath(rStatus.m_szFullName, lpszFileName)) {
		rStatus.m_szFullName[0] = '\0';
		return false;
	}

	WIN32_FIND_DATA findFileData;
	HANDLE hFind = FindFirstFile(lpszFileName, &findFileData);
	if (hFind == INVALID_HANDLE_VALUE)
		return FALSE;
	Win32Check(FindClose(hFind));

	// strip attribute of NORMAL bit, our API doesn't have a "normal" bit.
	rStatus.m_attribute = (BYTE)
		(findFileData.dwFileAttributes & ~FILE_ATTRIBUTE_NORMAL);

	rStatus.m_size = findFileData.nFileSizeLow+((Int64)findFileData.nFileSizeHigh << 32);

	// convert times as appropriate
	rStatus.m_ctime = findFileData.ftCreationTime;
	rStatus.m_atime = findFileData.ftLastAccessTime;
	rStatus.m_mtime = findFileData.ftLastWriteTime;

	if (!rStatus.m_ctime.Ticks)
		rStatus.m_ctime = rStatus.m_mtime;

	if (!rStatus.m_atime.Ticks)
		rStatus.m_atime = rStatus.m_mtime;
	return true;
}
#endif

#endif // !UCFG_USE_POSIX

UInt32 File::Read(void *lpBuf, UInt32 nCount) {
#if UCFG_USE_POSIX
	return CCheck(::read((int)(LONG_PTR)(HANDLE)HandleAccess(_self), lpBuf, nCount));
#else
	DWORD dwRead;
	Win32Check(::ReadFile(HandleAccess(_self), lpBuf, nCount, &dwRead, 0), ERROR_BROKEN_PIPE);
	return dwRead;
#endif
}

void File::Write(const void *buf, size_t size, Int64 offset) {
#if UCFG_USE_POSIX
	if (offset >= 0)
		CCheck(::pwrite((int)(LONG_PTR)(HANDLE)HandleAccess(_self), buf, size, offset));
	else
		CCheck(::write((int)(LONG_PTR)(HANDLE)HandleAccess(_self), buf, size));
#else
	OVERLAPPED ov, *pov = 0;
#	if UCFG_WCE
	switch (offset) {
	case -1:
		SeekToEnd();
	case CURRENT_OFFSET:
		break;
	default:
		Seek(offset);
	}
#	else
	if (offset != CURRENT_OFFSET)
		ZeroStruct(*(pov = &ov));
#	endif
	DWORD nWritten;
	for (const byte *p = (const byte*)buf; size; size -= nWritten, p += nWritten) {
		if (pov) {
			ov.Offset = DWORD(offset);
			ov.OffsetHigh = DWORD(offset >> 32);
		}
		Win32Check(::WriteFile(HandleAccess(_self), p, std::min(size, (size_t)0xFFFFFFFF), &nWritten, pov));
		if (offset != -1)
			offset += nWritten;
	}
#endif
}

void File::Lock(UInt64 pos, UInt64 len, bool bExclusive, bool bFailImmediately) {
#if UCFG_USE_POSIX
	Int64 prev = File::Seek(pos, SeekOrigin::Begin);
	int rc = ::lockf((int)(LONG_PTR)(HANDLE)HandleAccess(_self), F_LOCK, len);
	File::Seek(prev, SeekOrigin::Begin);
	CCheck(rc);
#else
	OVERLAPPED ovl = { 0 };
	ovl.Offset = DWORD(pos);
	ovl.OffsetHigh = DWORD(pos >> 32);
	DWORD flags = bExclusive ? LOCKFILE_EXCLUSIVE_LOCK : 0;
	if (bFailImmediately)
		flags |= LOCKFILE_FAIL_IMMEDIATELY;
	Win32Check(::LockFileEx(HandleAccess(_self), flags, 0, DWORD(len), DWORD(len >> 32), &ovl));
#endif
}

void File::Unlock(UInt64 pos, UInt64 len) {
#if UCFG_USE_POSIX
	Int64 prev = File::Seek(pos, SeekOrigin::Begin);
	int rc = ::lockf((int)(LONG_PTR)(HANDLE)HandleAccess(_self), F_ULOCK, len);
	File::Seek(prev, SeekOrigin::Begin);
	CCheck(rc);
#else
	OVERLAPPED ovl = { 0 };
	ovl.Offset = DWORD(pos);
	ovl.OffsetHigh = DWORD(pos >> 32);
	Win32Check(::UnlockFileEx(HandleAccess(_self), 0, DWORD(len), DWORD(len >> 32), &ovl));
#endif
}

Int64 File::Seek(const Int64& off, SeekOrigin origin) {
#if UCFG_USE_POSIX
	return CCheck(::lseek((int)(LONG_PTR)(HANDLE)HandleAccess(_self), (long)off, (int)origin));
#else
	ULARGE_INTEGER uli;
	uli.QuadPart = (ULONGLONG)off;
	uli.LowPart = ::SetFilePointer(HandleAccess(_self), (LONG)uli.LowPart, (LONG*)&uli.HighPart, (DWORD)origin);
	if (uli.LowPart == MAXDWORD)
		Win32Check(GetLastError() == NO_ERROR);
	return uli.QuadPart;
#endif
}

void File::Flush() {
#if UCFG_USE_POSIX
	CCheck(::fsync((int)(LONG_PTR)(HANDLE)HandleAccess(_self)));
#else
	Win32Check(::FlushFileBuffers(HandleAccess(_self)));
#endif
}  

UInt64 File::get_Length() const {
#if UCFG_USE_POSIX
	struct stat st;
	CCheck(::fstat((int)(LONG_PTR)(HANDLE)HandleAccess(_self), &st));
	return st.st_size;
#else
	ULARGE_INTEGER uli;
	uli.LowPart = ::GetFileSize(HandleAccess(_self), &uli.HighPart);
	if (uli.LowPart == INVALID_FILE_SIZE)
		Win32Check(GetLastError() == NO_ERROR);
	return uli.QuadPart;
#endif
}

void File::put_Length(UInt64 len) {
#if UCFG_USE_POSIX
	CCheck(::ftruncate((int)(LONG_PTR)(HANDLE)HandleAccess(_self), len));
#else
	Seek((Int64)len, SeekOrigin::Begin);
	SetEndOfFile();
#endif
}

void FileStream::Open(RCString path, FileMode mode, FileAccess access, FileShare share) {
	HANDLE h = File(path, mode, access, share).Detach();
#if UCFG_WIN32_FULL
	int flags = 0;
	if (mode == FileMode::Append)
		flags |= _O_APPEND;
	switch (access) {
	case FileAccess::Read:		flags |= _O_RDONLY; break;
	case FileAccess::Write:		flags |= _O_WRONLY; break;
	case FileAccess::ReadWrite:	flags |= _O_RDWR; break;
	}

	int fd = _open_osfhandle(intptr_t(h), flags);
#elif UCFG_WCE
	HANDLE fd = h;
#else
	int fd = (int)(LONG_PTR)h;				// on POSIX our HANDLE used only as int
#endif

	char smode[10] = "";
	if (access == FileAccess::Read || access == FileAccess::ReadWrite)
		strcat(smode, "r");
	switch (mode) {
	case FileMode::Append:
		strcpy(smode, "a");
		if ((int)access & (int)FileAccess::Read)
			strcat(smode, "+");
		break;
	case FileMode::CreateNew:
	case FileMode::Create:
		strcat(smode, "w");
		break;
	case FileMode::Open:
	case FileMode::OpenOrCreate:
		if (access == FileAccess::ReadWrite)
			strcat(smode, "+");
		else if (access == FileAccess::Write)
			strcat(smode, "w");
		break;
	case FileMode::Truncate:
		strcpy(smode, "w+");
		break;
	}
	if (!TextMode)
		strcat(smode, "b");

#if UCFG_WCE
	m_fstm = _wfdopen(fd, String(smode));
	if (!m_fstm)
		Throw(E_FAIL);
#else
	m_fstm = fdopen(fd, smode);
	if (!m_fstm)
		CCheck(-1);
#endif

	/*!!!R
#if UCFG_WCE
	m_bUseFstream = true;
	int m = ios::binary;
	if (mode == FileMode::Append)
		m |= ios::app;
	switch (access)
	{
	case FileAccess::Read:		m |= ios::in;	break;
	case FileAccess::Write:		m |= ios::out;	break;
	case FileAccess::ReadWrite:	m |= ios::in|ios::out;	break;
	default:
		Throw(E_INVALIDARG);
	}
	m_fs.open(path, m);
	if (!m_fs)
		Throw(HRESULT_OF_WIN32(ERROR_FILE_NOT_FOUND));
#else
	File.Open(path, mode, access, share);
#endif */
}

HANDLE FileStream::get_Handle() const {
	if (m_fstm) {
#if UCFG_WIN32_FULL
		return (HANDLE)_get_osfhandle(fileno(m_fstm));
#else
		return (HANDLE)(uintptr_t)fileno(m_fstm);
#endif
	} else if (m_pFile)
		return m_pFile->DangerousGetHandle();
	else
		Throw(E_FAIL);
}

UInt64 FileStream::get_Length() const {
	if (m_fstm) {
		File file(Handle, false);
		return file.Length;
	} else if (m_pFile)
		return m_pFile->Length;
	else
		Throw(E_FAIL);
}

bool FileStream::Eof() const {
	if (m_fstm) {
		if (feof(m_fstm))
			return true;
		int ch = fgetc(m_fstm);
		if (ch == EOF)
			CFileCheck(ferror(m_fstm));		//!!!Check
		else
			ungetc(ch, m_fstm);
		return ch == EOF;
	} else if (m_pFile)
		return Position == m_pFile->Length;
	else
		Throw(E_FAIL);
}

size_t FileStream::Read(void *buf, size_t count) const {
	if (m_fstm)
		return fread(buf, 1, count, m_fstm);
		
#if UCFG_WIN32_FULL
	if (m_ovl) {
		UInt32 n;
		Win32Check(::ResetEvent(m_ovl->hEvent));
		if (!m_pFile->Read(buf, count, &n, m_ovl)) {
			int r = ::WaitForSingleObjectEx(m_ovl->hEvent, INFINITE, TRUE);
			if (r == WAIT_IO_COMPLETION)
				Throw(E_EXT_ThreadInterrupted);
			if (r != 0)
				Throw(E_FAIL);
			try {
				DBG_LOCAL_IGNORE_WIN32(ERROR_BROKEN_PIPE);
				n = m_pFile->GetOverlappedResult(*m_ovl);
			} catch (const Exception& ex) {
				if (ex.HResult != HRESULT_FROM_WIN32(ERROR_BROKEN_PIPE))
					throw;
				n = 0;
			}
		}
		return n;
	}
#endif
	return m_pFile->Read(buf, (UINT)count);
}

void FileStream::ReadBuffer(void *buf, size_t count) const {
	if (m_fstm) {
		size_t r = fread(buf, 1, count, m_fstm);
		if (r != count) {
			if (feof(m_fstm))
				Throw(E_EXT_EndOfStream);
			else {
				CFileCheck(ferror(m_fstm));
				Throw(E_FAIL);
			}
		}
		return;
	}

	while (count) {
		UInt32 n;
#if UCFG_WIN32_FULL
		if (m_ovl) {
			Win32Check(::ResetEvent(m_ovl->hEvent));
			bool b = m_pFile->Read(buf, count, &n, m_ovl);
			if (!b) {
				int r = ::WaitForSingleObjectEx(m_ovl->hEvent, INFINITE, TRUE);
				if (r == WAIT_IO_COMPLETION)
					Throw(E_EXT_ThreadInterrupted);
				if (r != 0)
					Throw(E_FAIL);
				n = m_pFile->GetOverlappedResult(*m_ovl);
			}
		} else
#endif
			n = m_pFile->Read(buf, (UINT)count);
		if (n) {    
			count -= n;
			(BYTE*&)buf += n;
		} else
			Throw(E_EXT_EndOfStream);
	}
}

void FileStream::WriteBuffer(const void *buf, size_t count) {
	if (m_fstm) {
		size_t r = fwrite(buf, 1, count, m_fstm);
		if (r != count) {
			CFileCheck(ferror(m_fstm));
			Throw(E_FAIL);
		}
		return;
	}
#if UCFG_WIN32_FULL
	UInt32 n;
	if (m_ovl) {
		Win32Check(::ResetEvent(m_ovl->hEvent));
		bool b = m_pFile->Write(buf, count, &n, m_ovl);
		if (!b) {
				int r = ::WaitForSingleObjectEx(m_ovl->hEvent, INFINITE, TRUE);
			if (r == WAIT_IO_COMPLETION)
				Throw(E_EXT_ThreadInterrupted);
			if (r != 0)
				Throw(E_FAIL);
			n = m_pFile->GetOverlappedResult(*m_ovl);
			if (n != count)
				Throw(E_FAIL);
		}
	} else
#endif
		m_pFile->Write(buf, (UINT)count);
}

void FileStream::Close() const {
	if (m_fstm) {
		CCheck(fclose(exchange(m_fstm, nullptr)));
	} else if (m_pFile)
		m_pFile->Close();
}

void FileStream::Flush() {
	if (m_fstm)
		CCheck(fflush(m_fstm));
	else if (m_pFile)
		m_pFile->Flush();
	else
		Throw(E_FAIL);
}


#ifdef WIN32
WIN32_FIND_DATA FileSystemInfo::GetData() const {
	WIN32_FIND_DATA findFileData;
	HANDLE hFind = FindFirstFile(FullPath, &findFileData);
	Win32Check(hFind != INVALID_HANDLE_VALUE);
	Win32Check(FindClose(hFind));
	return findFileData;
}

DWORD FileSystemInfo::get_Attributes() const {
	DWORD r = ::GetFileAttributes(ExcLastStringArgKeeper(FullPath));
	Win32Check(r != INVALID_FILE_ATTRIBUTES);
	return r;
}
#endif

DateTime FileSystemInfo::get_CreationTime() const {
#if UCFG_USE_POSIX
	struct stat st;
	CCheck(::stat(FullPath, &st));
	return DateTime::from_time_t(st.st_ctime);
#else
	return GetData().ftCreationTime;
#endif
}

void FileSystemInfo::put_CreationTime(const DateTime& dt) {
#if UCFG_USE_POSIX
	Throw(E_NOTIMPL);
#else
	File file;
	file.Create(FullPath, FILE_WRITE_ATTRIBUTES, FILE_SHARE_READ|FILE_SHARE_WRITE, OPEN_EXISTING);
	FILETIME ft = dt;
	Win32Check(::SetFileTime(Handle(file), &ft, 0, 0));	
#endif
}

DateTime FileSystemInfo::get_LastAccessTime() const {
#if UCFG_USE_POSIX
	struct stat st;
	CCheck(::stat(FullPath, &st));
	return DateTime::from_time_t(st.st_atime);
#else
	return GetData().ftLastAccessTime;
#endif
}

void FileSystemInfo::put_LastAccessTime(const DateTime& dt) {
#if UCFG_USE_POSIX
	utimbuf ut;
	timeval tv, tvDt;
	get_LastWriteTime().ToTimeval(tv);
	ut.modtime = tv.tv_sec;
	dt.ToTimeval(tvDt);
	ut.actime = tvDt.tv_sec;
	CCheck(::utime(FullPath, &ut));
#else
	File file;
	file.Create(FullPath, FILE_WRITE_ATTRIBUTES, FILE_SHARE_READ|FILE_SHARE_WRITE, OPEN_EXISTING);
	FILETIME ft = dt;
	Win32Check(::SetFileTime(Handle(file), 0, &ft, 0));	
#endif
}

DateTime FileSystemInfo::get_LastWriteTime() const {
#if UCFG_USE_POSIX
	struct stat st;
	CCheck(::stat(FullPath, &st));
	return DateTime::from_time_t(st.st_mtime);
#else
	return GetData().ftLastWriteTime;
#endif
}

void FileSystemInfo::put_LastWriteTime(const DateTime& dt) {
#if UCFG_USE_POSIX
	utimbuf ut;
	timeval tv, tvDt;
	get_LastAccessTime().ToTimeval(tv);
	ut.actime = tv.tv_sec;
	dt.ToTimeval(tvDt);
	ut.modtime = tvDt.tv_sec;
	CCheck(::utime(FullPath, &ut));
#else
	File file;
	file.Create(FullPath, FILE_WRITE_ATTRIBUTES, FILE_SHARE_READ|FILE_SHARE_WRITE, OPEN_EXISTING);
	FILETIME ft = dt;
	Win32Check(::SetFileTime(Handle(file), 0, 0, &ft));	
#endif
}

Int64 FileInfo::get_Length() const {
#if UCFG_USE_POSIX
	struct stat st;
	CCheck(::stat(FullPath, &st));
	return st.st_size;
#elif !UCFG_WCE
	CFileStatus st;
	Win32Check(File::GetStatus(FullPath, st)); //!!!
	return st.m_size;
#else
	Throw(E_NOTIMPL);
#endif
}

#if !UCFG_WCE
String Directory::GetCurrentDirectory() {
#if UCFG_USE_POSIX
	char path[PATH_MAX];
	char *r = ::getcwd(path, sizeof path);
	CCheck(r ? 0 : -1);
	return r;
#else
	_TCHAR *p = (_TCHAR*)alloca(_MAX_PATH*sizeof(_TCHAR));
	DWORD dw = ::GetCurrentDirectory(_MAX_PATH, p);
	if (dw > _MAX_PATH) {
		p = (_TCHAR*)alloca(dw*sizeof(_TCHAR));
		dw = ::GetCurrentDirectory(dw, p);
	}
	Win32Check(dw);
	return p;
#endif
}

void Directory::SetCurrentDirectory(RCString path) {
#if UCFG_USE_POSIX
	CCheck(::chdir(path));
#else
	Win32Check(::SetCurrentDirectory(path));
#endif
}

String Path::GetFullPath(RCString s) {
#if UCFG_USE_POSIX
	return s;
#else
	_TCHAR *path = (TCHAR*)alloca(_MAX_PATH*sizeof(TCHAR));
	_TCHAR *p;
	DWORD dw = ::GetFullPathName(s, _MAX_PATH, path, &p);
	Win32Check(dw);
	if (dw >= _MAX_PATH) {
		int len = dw+1;
		path = (TCHAR*)alloca(len*sizeof(TCHAR));
		Win32Check(::GetFullPathName(s, len, path, &p));
	}
	return path;
#endif
}
#endif



#if UCFG_WIN32_FULL
CFileFind::CFileFind()
	:	m_handle(-1)
	,	m_b(false)
{
}

CFileFind::CFileFind(RCString filespec) {
	First(filespec);
}

CFileFind::~CFileFind() {
	if (m_handle != -1)
		CCheck(_findclose(m_handle));
}

void CFileFind::First(RCString filespec) {
	m_handle = _findfirst(filespec, &m_fd);  //!!! TCHAR
	m_b = m_handle != -1;
}

bool CFileFind::Next(SFindData& fd) {
	fd = m_fd;
	bool r = m_b;
	if (m_handle != -1)
		m_b = !_findnext(m_handle, &m_fd);  //!!! TCHAR
	return r;
}

vector<String> AFXAPI SerialPort::GetPortNames() {
	vector<String> r;
	try {
		DBG_LOCAL_IGNORE_NAME(E_ACCESSDENIED, E_ACCESSDENIED);

		RegistryKey key(HKEY_LOCAL_MACHINE, "HARDWARE\\DEVICEMAP\\SERIALCOMM", false);
		CRegistryValues rvals(key);
		for (int i=0, count=rvals.Count; i<count; ++i)
			r.push_back(rvals[i]);
	} catch(RCExc) {
	}
	return r;
}

#endif // UCFG_WIN32_FULL



} // Ext::

