/*######     Copyright (c) 1997-2013 Ufasoft  http://ufasoft.com  mailto:support@ufasoft.com,  Sergey Pavlov  mailto:dev@ufasoft.com #######################################
#                                                                                                                                                                          #
# This program is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation;  #
# either version 3, or (at your option) any later version. This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the      #
# implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details. You should have received a copy of the GNU #
# General Public License along with this program; If not, see <http://www.gnu.org/licenses/>                                                                               #
##########################################################################################################################################################################*/

#pragma once






#define CREATE_NEW          1 //!!!  windows
typedef struct tagEXCEPINFO EXCEPINFO;

#if UCFG_USE_POSIX
#	include <nl_types.h>
#endif

namespace Ext {

class Stream;
class DirectoryInfo;


typedef UInt32 (*AFX_THREADPROC)(void *);


#if !UCFG_WDM
class CResID : public CPersistent, public CPrintable {
public:
	AFX_API CResID(UINT nResId = 0);
	AFX_API CResID(const char *lpName);
	AFX_API CResID(const String::Char *lpName);
	AFX_API CResID& operator=(const char *resId);
	AFX_API CResID& operator=(const String::Char *resId);
	AFX_API CResID& operator=(RCString resId);
	AFX_API operator const char *() const;
	AFX_API operator const String::Char *() const;
	AFX_API operator UINT() const;

	bool operator==(const CResID& resId) const {
		return m_resId==resId.m_resId && m_name==resId.m_name;
	}

	String ToString() const;
//!!!private:
	DWORD_PTR m_resId;
	String m_name;

	void Read(const BinaryReader& rd) override;
	void Write(BinaryWriter& wr) const override;
};

}
namespace EXT_HASH_VALUE_NS {
	inline size_t hash_value(const Ext::CResID& resId) { return std::hash<Ext::UInt64>()((Ext::UInt64)resId.m_resId) + std::hash<Ext::String>()(resId.m_name); }
}
EXT_DEF_HASH(Ext::CResID) namespace Ext {


#endif

const int NOTIFY_PROBABILITY = 1000;

#if UCFG_USE_POSIX

class DlException : public Exception {
	typedef Exception base;
public:
	DlException()
		:	base(E_EXT_Dynamic_Library, dlerror())
	{
	}
};

inline void DlCheck(int rc) {
	if (rc)
		throw DlException();
}


#endif

class EXTAPI CDynamicLibrary {
	typedef CDynamicLibrary class_type;
public:
	mutable CInt<HMODULE> m_hModule;
	String Path;

	CDynamicLibrary() {
	}

	CDynamicLibrary(const String& path, bool bDelay = false) {
		Path = path;
		if (!bDelay)
			Load(path);
	}

	~CDynamicLibrary();

	operator HMODULE() const {
		if (!m_hModule)
			Load(Path);
		return m_hModule;
	}

	void Load(const String& path) const;
	void Free();
	FARPROC GetProcAddress(const CResID& resID);
private:
	CDynamicLibrary(const class_type&);
	CDynamicLibrary& operator=(const CDynamicLibrary&);
};

enum _NoInit {
	E_NoInit
};

#if UCFG_WIN32

class DlProcWrapBase {
public:
	void Init(HMODULE hModule, RCString funname);
protected:
	void *m_p;

	DlProcWrapBase()
		:	m_p(0)
	{}

	DlProcWrapBase(RCString dll, RCString funname);
};

template <typename F>
class DlProcWrap : public DlProcWrapBase {
	typedef DlProcWrapBase base;
public:
	DlProcWrap() {
	}

	DlProcWrap(HMODULE hModule, RCString funname) {
		Init(hModule, funname);
	}

	DlProcWrap(RCString dll, RCString funname)
		:	base(dll, funname)
	{
	}

	operator F() const { return (F)m_p; }
};

template <typename F> bool GetProcAddress(F& pfn, HMODULE hModule, RCString funname) {
	return pfn = (F)::GetProcAddress(hModule, funname);
}

template <typename F> bool GetProcAddress(F& pfn, RCString dll, RCString funname) {
	return GetProcAddress(pfn, ::GetModuleHandle(dll), funname);
}

class ResourceObj : public Object {
public:
	ResourceObj(HMODULE hModule, HRSRC hRsrc);
	~ResourceObj();
private:
	HMODULE m_hModule;
	HRSRC m_hRsrc;
	HGLOBAL m_hglbResource;
	void *m_p;

	friend class Resource;
};
#endif // UCFG_WIN32

class Resource {
	typedef Resource class_type;
public:
	Resource(const CResID& resID, const CResID& resType, HMODULE hModule = 0);

	const void *get_Data();
	DEFPROP_GET(const void *, Data);

	size_t get_Size();
	DEFPROP_GET(size_t, Size);

	operator ConstBuf() {
		return ConstBuf(Data, Size);
	}

private:
#if UCFG_WIN32
	ptr<ResourceObj> m_pimpl;
#else
	Blob m_blob;
#endif
};



#if UCFG_COM

const int COINIT_APARTMENTTHREADED  = 0x2;

class AFX_CLASS CUsingCOM {
	bool m_bInitialized;
	CDynamicLibrary m_dllOle;
public:
	CUsingCOM(DWORD dwCoInit = COINIT_APARTMENTTHREADED);
	CUsingCOM(_NoInit);
	~CUsingCOM();
	void Initialize(DWORD dwCoInit = COINIT_APARTMENTTHREADED);
	void Uninitialize();
};

#endif




#if !UCFG_USE_PTHREADS

struct CTimesInfo {
	FILETIME m_tmCreation,
		m_tmExit,
		m_tmKernel,
		m_tmUser;
};

#endif

class IOExc : public Exception {
	typedef Exception base;
public:
	String FileName;

	IOExc(HRESULT hr)
		:	base(hr)
	{}

	String get_Message() const override {
		String r = base::get_Message();
		if (!FileName.IsEmpty())
			r += " "+FileName;
		return r;
	}
};

class FileNotFoundException : public IOExc {
	typedef IOExc base;
public:
	FileNotFoundException()
		:	base(HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND))
	{}
};

class FileAlreadyExistsExc : public IOExc {
	typedef IOExc base;
public:
	FileAlreadyExistsExc()
		:	base(HRESULT_FROM_WIN32(ERROR_FILE_EXISTS))
	{}
};

class DirectoryNotFoundExc : public Exception {
	typedef Exception base;
public:
	DirectoryNotFoundExc()
		:	base(HRESULT_OF_WIN32(ERROR_PATH_NOT_FOUND))
	{}
};


#ifdef WIN32

//!!!O
struct CFileStatus {
	DateTime m_ctime;          // creation date/time of file
	DateTime m_mtime;          // last modification date/time of file
	DateTime m_atime;          // last access date/time of file
	Int64 m_size;            // logical size of file in bytes
	byte m_attribute;       // logical OR of File::Attribute enum values
	byte _m_padding;        // pad the structure to a WORD
	TCHAR m_szFullName[_MAX_PATH]; // absolute path name
};
#endif

ENUM_CLASS(FileMode) {
	CreateNew,
	Create,
	Open,
	OpenOrCreate,
	Truncate,
	Append
} END_ENUM_CLASS(FileMode);

ENUM_CLASS(FileAccess) {
	Read = 1,
	Write = 2,
	ReadWrite = 3,
} END_ENUM_CLASS(FileAccess);

ENUM_CLASS(FileShare) {
	None = 0,
	Read = 1,
	Write = 2,
	ReadWrite = 3,
	Delete = 4,
	Inheritable = 8
} END_ENUM_CLASS(FileShare);

class AFX_CLASS File : public SafeHandle {
public:
	typedef File class_type;

	enum Attribute {
		normal =    0x00,
		readOnly =  0x01,
		hidden =    0x02,
		system =    0x04,
		volume =    0x08,
		directory = 0x10,
		archive =   0x20
	};

	static bool AFXAPI Exists(RCString path);
	static void AFXAPI Delete(RCString path);
	static void AFXAPI Copy(RCString src, RCString dest, bool bOverwrite = false);
	static void AFXAPI Move(RCString src, RCString dest);

	EXT_DATA static bool s_bCreateFileWorksWithMMF;
	CBool m_bFileForMapping;

	File();

	File(HANDLE h, bool bOwn) {
		Attach(h, bOwn);
	}

	File(RCString path, FileMode mode, FileAccess access = FileAccess::ReadWrite, FileShare share = FileShare::Read) {
		Open(path, mode, access, share);
	}

	~File();

	static Blob AFXAPI ReadAllBytes(RCString path);
	static void AFXAPI WriteAllBytes(RCString path, const ConstBuf& mb);
	
	static String AFXAPI ReadAllText(RCString path, Encoding *enc = &Encoding::UTF8);
	static void AFXAPI WriteAllText(RCString path, RCString contents, Encoding *enc = &Encoding::UTF8);

	struct OpenInfo {
		String Path;
		FileMode Mode;
		FileAccess Access;
		FileShare Share;
		bool BufferingEnabled, RandomAccess;

		OpenInfo(RCString path = nullptr)
			:	Path(path)
			,	Mode(FileMode::Open)
			,	Access(FileAccess::ReadWrite)
			,	Share(FileShare::None)
			,	BufferingEnabled(true)
			,	RandomAccess(false)
		{}
	};

	void Open(const OpenInfo& oi);
	EXT_API virtual void Open(RCString path, FileMode mode, FileAccess access = FileAccess::ReadWrite, FileShare share = FileShare::None);
#ifdef WIN32
	void Create(RCString fileName, DWORD dwDesiredAccess = GENERIC_READ|GENERIC_WRITE, DWORD dwShareMode = FILE_SHARE_READ|FILE_SHARE_WRITE, DWORD dwCreationDisposition = CREATE_NEW, DWORD dwFlagsAndAttributes = FILE_ATTRIBUTE_NORMAL, HANDLE hTemplateFile = 0, LPSECURITY_ATTRIBUTES lpsa = 0);
	void CreateForMapping(LPCTSTR lpFileName, DWORD dwDesiredAccess = GENERIC_READ|GENERIC_WRITE, DWORD dwShareMode = FILE_SHARE_READ|FILE_SHARE_WRITE, DWORD dwCreationDisposition = CREATE_NEW, DWORD dwFlagsAndAttributes = FILE_ATTRIBUTE_NORMAL, HANDLE hTemplateFile = 0, LPSECURITY_ATTRIBUTES lpsa = 0);
	DWORD GetOverlappedResult(OVERLAPPED& ov, bool bWait = true);
	bool GetStatus(CFileStatus& rStatus) const;
	static bool AFXAPI GetStatus(RCString lpszFileName, CFileStatus& rStatus);
	bool Read(void *buf, UInt32 len, UInt32 *read, OVERLAPPED *pov);			// no default args to eleminate ambiguosness
	bool Write(const void *buf, UInt32 len, UInt32 *written, OVERLAPPED *pov);	// no default args to eleminate ambiguosness
	bool DeviceIoControl(int code, LPCVOID bufIn, size_t nIn, LPVOID bufOut, size_t nOut, LPDWORD pdw, LPOVERLAPPED pov = 0);
	DWORD DeviceIoControlAndWait(int code, LPCVOID bufIn = 0, size_t nIn = 0, LPVOID bufOut = 0, size_t nOut = 0);
	void CancelIo();
#endif
	EXT_API Int64 Seek(const Int64& off, SeekOrigin origin = SeekOrigin::Begin);
	Int64 SeekToEnd() { return Seek(0, SeekOrigin::End); }
	void Flush();
	void SetEndOfFile();

	UInt64 get_Length() const;
	void put_Length(UInt64 len);
	DEFPROP_CONST(UInt64, Length);

	static const Int64 CURRENT_OFFSET = -2;

	virtual UInt32 Read(void *lpBuf, UInt32 nCount);
	virtual void Write(const void *buf, size_t size, Int64 offset = CURRENT_OFFSET);
	void Lock(UInt64 pos, UInt64 len, bool bExclusive = true, bool bFailImmediately = false);
	void Unlock(UInt64 pos, UInt64 len);

	//!!!  static void Remove(LPCTSTR lpszFileName);
	//!!!  static void Rename(LPCTSTR oldName, LPCTSTR newName);
	//!!!D  static void Copy(LPCTSTR existingFile, LPCTSTR newFile, bool bFailIfExists = false);
	//!!!D  static void RemoveDirectory(RCString dir, bool bRecurse = false);
protected:
	String m_strFileName;
private:
#ifdef WIN32
	bool CheckPending(BOOL b);
#endif
};

class AFX_CLASS CStdioFile : public File {
public:
	bool ReadString(String& rString);
};

ENUM_CLASS(MemoryMappedFileAccess) {
	None 					= 0,
	CopyOnWrite 			= 1,
	Write 					= 2,
	Read 					= 4,
	ReadWrite 				= Read|Write,
	Execute 				= 8,
	ReadExecute 			= Read | Execute,
	ReadWriteExecute 		= Read | Write | Execute
} END_ENUM_CLASS(MemoryMappedFileAccess);

ENUM_CLASS(MemoryMappedFileRights) {
	CopyOnWrite 			= 1,
	Write 					= 2,
	Read 					= 4,
	ReadWrite 				= Read|Write,
	Execute 				= 8,
	ReadExecute 			= Read | Execute,
	ReadWriteExecute 		= Read | Write | Execute,
	Delete 					= 0x10000,
	ReadPermissions 		= 0x20000,
	ChangePermissions 		= 0x40000,
	TakeOwnership 			= 0x80000,
	FullControl 			= CopyOnWrite | ReadWriteExecute | Delete | ReadPermissions | ChangePermissions | TakeOwnership,
	AccessSystemSecurity 	= 0x1000000
} END_ENUM_CLASS(MemoryMappedFileRights);


class MemoryMappedFile ;

class MemoryMappedView {
public:
	UInt64 Offset;
	void *Address;
	size_t Size;
	MemoryMappedFileAccess Access;
	bool AddressFixed;

	MemoryMappedView()
		:	Offset(0)
		,	Address(0)
		,	Size(0)
		,	Access(MemoryMappedFileAccess::ReadWrite)
		,	AddressFixed(false)
	{
	}

	~MemoryMappedView() {
		Unmap();
	}

	MemoryMappedView(const MemoryMappedView& v)
		:	Offset(0)
		,	Address(0)
		,	Size(0)
		,	Access(MemoryMappedFileAccess::ReadWrite)
		,	AddressFixed(false)
	{
		if (v.Address)
			Throw(E_FAIL);
	}

	MemoryMappedView(EXT_RV_REF(MemoryMappedView) rv);

	MemoryMappedView& operator=(const MemoryMappedView& v) {
		Throw(E_FAIL);
	}

	MemoryMappedView& operator=(EXT_RV_REF(MemoryMappedView) rv);
	void Map(MemoryMappedFile& file, UInt64 offset, size_t size, void *desiredAddress = 0);
	void Unmap();
	void Flush();	

	static void AFXAPI Protect(void *p, size_t size, MemoryMappedFileAccess access);
};

class MemoryMappedFile {
	typedef MemoryMappedFile class_type;

public:
	SafeHandle m_hMapFile;
//!!!R	size_t Size;

	MemoryMappedFile()
//!!!R		,	Size(0)
	{}

	MemoryMappedFile(EXT_RV_REF(MemoryMappedFile) rv)
		:	m_hMapFile(static_cast<EXT_RV_REF(SafeHandle)>(rv.m_hMapFile))
	{}

	MemoryMappedFile& operator=(EXT_RV_REF(MemoryMappedFile) rv) {
		m_hMapFile.Close();
		m_hMapFile = static_cast<EXT_RV_REF(SafeHandle)>(rv.m_hMapFile);
		return *this;
	}

#if UCFG_USE_POSIX
	Ext::File *m_pFile;

	intptr_t GetHandle() { return (intptr_t)m_pFile->DangerousGetHandle(); }
#else
	HANDLE GetHandle() { return m_hMapFile.DangerousGetHandle(); }
#endif

	static MemoryMappedFile AFXAPI CreateFromFile(Ext::File& file, RCString mapName = nullptr, UInt64 capacity = 0, MemoryMappedFileAccess access = MemoryMappedFileAccess::ReadWrite);
	static MemoryMappedFile AFXAPI CreateFromFile(RCString path, FileMode mode = FileMode::Open, RCString mapName = nullptr, UInt64 capacity = 0, MemoryMappedFileAccess access = MemoryMappedFileAccess::ReadWrite);
	static MemoryMappedFile AFXAPI OpenExisting(RCString mapName, MemoryMappedFileRights rights = MemoryMappedFileRights::ReadWrite, HandleInheritability inheritability = HandleInheritability::None);
	MemoryMappedView CreateView(UInt64 offset, size_t size = 0, MemoryMappedFileAccess access = MemoryMappedFileAccess::ReadWrite);
};


class AFX_CLASS CSingleLock {
public:
	CSingleLock(CSyncObject *pObject, bool bInitialLock = false);
	~CSingleLock();
	void Lock(DWORD dwTimeOut = INFINITE);
	void Unlock();
	bool IsLocked();
protected:
	CSyncObject *m_pObject;
	CBool m_bAcquired;
};


class FileStream : public Stream {
	typedef FileStream class_type;
public:
//!!!	mutable File m_ownFile;
	CPointer<File> m_pFile;
	mutable FILE *m_fstm;
	CBool TextMode;

#if UCFG_WIN32_FULL
	CPointer<OVERLAPPED> m_ovl;
#endif

	FileStream()
		:	m_fstm(0)
	{
	}
	
	FileStream(Ext::File& file
#if UCFG_WIN32_FULL
, OVERLAPPED *ovl = nullptr
#endif
)
		:	m_pFile(&file)
		,	m_fstm(0)
#if UCFG_WIN32_FULL
		,	m_ovl(ovl)
#endif
	{
	}

	FileStream(RCString path, FileMode mode, FileAccess access = FileAccess::ReadWrite, FileShare share = FileShare::Read)
		:	m_fstm(0)
	{
		Open(path, mode, access, share);
	}

	~FileStream() {
		if (m_fstm)
			Close();
	}

	EXT_API void Open(RCString path, FileMode mode, FileAccess access = FileAccess::ReadWrite, FileShare share = FileShare::None);
	size_t Read(void *buf, size_t count) const override;
	void ReadBuffer(void *buf, size_t count) const override;
	void WriteBuffer(const void *buf, size_t count) override;
	void Close() const override;
	void Flush() override;

	HANDLE get_Handle() const;
	DEFPROP_GET(HANDLE, Handle);

	UInt64 get_Position() const override{
		if (m_fstm) {
			fpos_t r;
			CFileCheck(fgetpos(m_fstm, &r));
#if defined(_WIN32) || defined(__FreeBSD__)
			return r;
#else
			return r.__pos;
#endif			
		} else if (m_pFile)
			return m_pFile->Seek(0, SeekOrigin::Current);
		else
			Throw(E_FAIL);
	}

	void put_Position(UInt64 pos) const override {
		if (m_fstm) {
			fpos_t fpos;
#if defined(_WIN32) || defined(__FreeBSD__)
			fpos = pos;
#else
			CFileCheck(fgetpos(m_fstm, &fpos));
			fpos.__pos = pos;
#endif
			CFileCheck(fsetpos(m_fstm, &fpos));
		} else if (m_pFile)
			m_pFile->Seek(pos, SeekOrigin::Begin);
		else
			Throw(E_FAIL);
	}

	Int64 Seek(Int64 offset, SeekOrigin origin) const override { 
		if (m_fstm) {
			CCheck(fseek(m_fstm, (long)offset, (int)origin));	//!!!
			return Position;
		} else if (m_pFile)
			return m_pFile->Seek(offset, origin);
		else
			Throw(E_FAIL);
	}

	UInt64 get_Length() const override;
	bool Eof() const override;
};

class Guid : public GUID {
public:
	Guid() { ZeroStruct(*this); }
	Guid(const GUID& guid) { *this = guid; }
	explicit Guid(RCString s);

	static Guid AFXAPI NewGuid();

	Guid& operator=(const GUID& guid) {
		*(GUID*)this = guid;
		return *this;
	}

	bool operator==(const GUID& guid) const { return !memcmp(this, &guid, sizeof(GUID)); }

	String ToString(RCString format = nullptr) const;
};

inline BinaryWriter& operator<<(BinaryWriter& wr, const Guid& guid) {
	return wr.WriteStruct(guid);
}

inline const BinaryReader& operator>>(const BinaryReader& rd, Guid& guid) {
	return rd.ReadStruct(guid);
}


inline bool operator<(const GUID& guid1, const GUID& guid2) {
	return memcmp(&guid1, &guid2, sizeof(GUID)) < 0;
}


} // Ext::

namespace EXT_HASH_VALUE_NS {
inline size_t hash_value(const Ext::Guid& guid) {
	return Ext::hash_value((const byte*)&guid, sizeof(GUID));
}
}

EXT_DEF_HASH(Ext::Guid)

//!!!#if !UCFG_STDSTL
//!!!using namespace _STL_NAMESPACE;
//!!!#endif

#	include "ext-thread.h"

namespace Ext {

class Version : public CPrintable {
public:
	int Major, Minor, Build, Revision;

	explicit Version(int major = 0, int minor = 0, int build = -1, int revision = -1)
		:	Major(major)
		,	Minor(minor)
		,	Build(build)
		,	Revision(revision)
	{}

	explicit Version(RCString s);

#ifdef WIN32
	static Version AFXAPI FromFileInfo(int ms, int ls, int fieldCount = 4);
#endif

	bool operator==(const Version& ver) const {
		return Major==ver.Major && Minor==ver.Minor && Build==ver.Build && Revision==ver.Revision;
	}

	bool operator!=(const Version& ver) const { return !operator==(ver); }

	bool operator<(const Version& ver) const {
		return Major<ver.Major ||
			Major==ver.Major && (Minor<ver.Minor ||
			                     Minor==ver.Minor && (Build<ver.Build ||
								                      Build==ver.Build  && Revision<ver.Revision));
	}

	bool operator>(const Version& ver) const {
		return ver < *this;
	}

	String ToString(int fieldCount) const;
	String ToString() const override;
};


} // Ext::

#ifdef WIN32

//#	include "win32/ext-win.h"
//#	include "extwin32.h"
#else
#	define AFX_MANAGE_STATE(p)
#endif

namespace Ext {

AFX_API int AFXAPI Rand();

class EXTAPI Random {
public:
	Int32 m_seed;

	explicit Random(int seed = Rand())
		:	m_seed(seed)
	{}

	virtual void NextBytes(const Buf& mb);
	int Next();
	int Next(int maxValue);
	double NextDouble();
private:
	UInt16 NextWord();
};

struct IAnnoy {
	virtual void OnAnnoy() =0;
};

class EXTAPI CAnnoyer {
public:
	CAnnoyer(IAnnoy *iAnnoy = 0)
		:	m_iAnnoy(iAnnoy)
		,	m_period(TimeSpan::FromSeconds(1))
	{
	}

	void Request();
protected:
	virtual void OnAnnoy();
private:
	CPointer<IAnnoy> m_iAnnoy;

	DateTime m_prev;
	TimeSpan m_period;
};

#if !UCFG_WCE
template <class T> class CSubscriber {
public:
	typedef std::unordered_set<T*> CSet;
	CSet m_set;
	
	void operator+=(T *v) {
		m_set.insert(v);
	}

	void operator-=(T *v) {
		m_set.erase(v);
	}
};
#endif


class Directory {
public:
	EXT_API static DirectoryInfo AFXAPI CreateDirectory(RCString path);
	static void AFXAPI Delete(RCString path, bool bRecursive = false);
	static bool AFXAPI Exists(RCString path);

	static std::vector<String> AFXAPI GetFiles(RCString path, RCString searchPattern = "*") {
		return GetItems(path, searchPattern, false);
	}

	static std::vector<String> AFXAPI GetDirectories(RCString path, RCString searchPattern = "*") {
		return GetItems(path, searchPattern, true);
	}

#if !UCFG_WCE
	EXT_API static String AFXAPI GetCurrentDirectory();
	EXT_API static void AFXAPI SetCurrentDirectory(RCString path);
#endif
private:
	EXT_API static std::vector<String> AFXAPI GetItems(RCString path, RCString searchPattern, bool bDirs);
};

#if !UCFG_WCE

class CDirectoryKeeper {
	String m_prevDir;
public:
	CDirectoryKeeper(RCString newDir) {
		m_prevDir = Directory::GetCurrentDirectory();
		Directory::SetCurrentDirectory(newDir);
	}

	~CDirectoryKeeper() {
		Directory::SetCurrentDirectory(m_prevDir);
	}
};

#endif

class /*!!!R EXTCLASS*/ Path {
public:
	struct CSplitPath {
		String m_drive,
			m_dir,
			m_fname,
			m_ext;
	};

#ifdef _WIN32				//	for WDM too
	static const char DirectorySeparatorChar = '\\';
	static const char AltDirectorySeparatorChar = '/';
	static const char VolumeSeparatorChar = ':';
	static const char PathSeparator = ';';
#else
	static const char DirectorySeparatorChar = '/';
	static const char AltDirectorySeparatorChar = '/';
	static const char VolummeSeparatorChar = '/';
	static const char PathSeparator = ':';
#endif

	EXT_API static String AFXAPI GetTempPath();
	EXT_API static std::pair<String, UINT> AFXAPI GetTempFileName(RCString path, RCString prefix, UINT uUnique = 0);
	EXT_API static String AFXAPI GetTempFileName() { return GetTempFileName(GetTempPath(), "tmp").first; }
	static String AFXAPI GetFullPath(RCString s);
	static bool AFXAPI IsPathRooted(RCString path);

	static CSplitPath AFXAPI SplitPath(RCString path);
	static String AFXAPI GetDirectoryName(RCString path);
	static String AFXAPI GetFileName(RCString path);
	static String AFXAPI GetFileNameWithoutExtension(RCString path);
	static String AFXAPI GetExtension(RCString path);
	static bool AFXAPI HasExtension(RCString path) { return !GetExtension(path).IsEmpty(); }

	static String AFXAPI Combine(RCString path1, RCString path2);
	static String AFXAPI ChangeExtension(RCString path, RCString ext);
	static String AFXAPI GetPhysicalPath(RCString p);
	static String AFXAPI GetTruePath(RCString p);
};

AFX_API String AFXAPI AddDirSeparator(RCString s);
AFX_API String AFXAPI RemoveDirSeparator(RCString s, bool bOnEndOnly = false);


class FileSystemInfo {
	typedef FileSystemInfo class_type;
public:
	String FullPath;
	bool m_bDir;

	FileSystemInfo(RCString name, bool bDir)
		:	FullPath(name)
		,	m_bDir(bDir)
	{}

	DWORD get_Attributes() const;
	DEFPROP_GET(DWORD, Attributes);

	bool get_Exists() const {
		return m_bDir ? Directory::Exists(FullPath) : File::Exists(FullPath);
	}
	DEFPROP_GET_CONST(bool, Exists);

	void Delete(RCString path) {
		return m_bDir ? Directory::Delete(FullPath) : File::Delete(FullPath);
	}

	operator bool() const { return Exists; }

	DateTime get_CreationTime() const;
	void put_CreationTime(const DateTime& dt);
	DEFPROP_CONST(DateTime, CreationTime);

	DateTime get_LastAccessTime() const;
	void put_LastAccessTime(const DateTime& dt);
	DEFPROP_CONST(DateTime, LastAccessTime);

	DateTime get_LastWriteTime() const;
	void put_LastWriteTime(const DateTime& dt);
	DEFPROP_CONST(DateTime, LastWriteTime);
protected:
#ifdef WIN32
	EXT_API WIN32_FIND_DATA GetData() const;
#endif
};

class EXTAPI FileInfo : public FileSystemInfo {
	typedef FileInfo class_type;
public:
	FileInfo(RCString name)
		:	FileSystemInfo(name, false)
	{}

	Int64 get_Length() const;
	DEFPROP_GET(Int64, Length);

	FileInfo CopyTo(RCString destFileName, bool bOverwrite = false) {
		File::Copy(FullPath, destFileName, bOverwrite);
		return destFileName;
	}
	
	void MoveTo(RCString destFileName) {
		File::Move(FullPath, destFileName);
	}
	
	void Delete() {
		File::Delete(FullPath);
	}
};

class DirectoryInfo : public FileSystemInfo {
public:
	DirectoryInfo(RCString name)
		:	FileSystemInfo(name, true)
	{}

	void Delete(RCString path, bool bRecursive) {
		Directory::Delete(path, bRecursive);
	}
};

#if UCFG_WIN32_FULL

class SerialPort {
public:
	EXT_API static std::vector<String> AFXAPI GetPortNames();
};

#endif // UCFG_WIN32_FULL

ENUM_CLASS(PlatformID) {
	Win32S,
	Win32Windows,
	Win32NT,
	WinCE,
	Unix,
	XBox
} END_ENUM_CLASS(PlatformID);

class OperatingSystem : public CPrintable {
	typedef OperatingSystem class_type;
public:
	PlatformID Platform;
	Ext::Version Version;
	String ServicePack;

	OperatingSystem();

	String get_PlatformName() const;
	DEFPROP_GET(String, PlatformName);

	String get_VersionName() const;
	DEFPROP_GET(String, VersionName);

	String get_VersionString() const;
	DEFPROP_GET(String, VersionString);

	String ToString() const override {
		return VersionString;
	}
};

#define CSIDL_DESKTOP                   0x0000        // <desktop>
#define CSIDL_INTERNET                  0x0001        // Internet Explorer (icon on desktop)
#define CSIDL_PROGRAMS                  0x0002        // Start Menu\Programs
#define CSIDL_CONTROLS                  0x0003        // My Computer\Control Panel
#define CSIDL_PRINTERS                  0x0004        // My Computer\Printers
#define CSIDL_PERSONAL                  0x0005        // My Documents
#define CSIDL_FAVORITES                 0x0006        // <user name>\Favorites
#define CSIDL_STARTUP                   0x0007        // Start Menu\Programs\Startup
#define CSIDL_RECENT                    0x0008        // <user name>\Recent
#define CSIDL_SENDTO                    0x0009        // <user name>\SendTo
#define CSIDL_BITBUCKET                 0x000a        // <desktop>\Recycle Bin
#define CSIDL_STARTMENU                 0x000b        // <user name>\Start Menu
#define CSIDL_MYDOCUMENTS               CSIDL_PERSONAL //  Personal was just a silly name for My Documents
#define CSIDL_MYMUSIC                   0x000d        // "My Music" folder
#define CSIDL_MYVIDEO                   0x000e        // "My Videos" folder
#define CSIDL_DESKTOPDIRECTORY          0x0010        // <user name>\Desktop
#define CSIDL_DRIVES                    0x0011        // My Computer
#define CSIDL_NETWORK                   0x0012        // Network Neighborhood (My Network Places)
#define CSIDL_NETHOOD                   0x0013        // <user name>\nethood
#define CSIDL_FONTS                     0x0014        // windows\fonts
#define CSIDL_TEMPLATES                 0x0015
#define CSIDL_COMMON_STARTMENU          0x0016        // All Users\Start Menu
#define CSIDL_COMMON_PROGRAMS           0X0017        // All Users\Start Menu\Programs
#define CSIDL_COMMON_STARTUP            0x0018        // All Users\Startup
#define CSIDL_COMMON_DESKTOPDIRECTORY   0x0019        // All Users\Desktop
#define CSIDL_APPDATA                   0x001a        // <user name>\Application Data
#define CSIDL_PRINTHOOD                 0x001b        // <user name>\PrintHood

#define CSIDL_PROGRAM_FILES             0x0026        // C:\Program Files
#define CSIDL_MYPICTURES                0x0027        // C:\Program Files\My Pictures

ENUM_CLASS(SpecialFolder) {
#ifdef CSIDL_DESKTOP
#ifdef CSIDL_APPDATA
	ApplicationData			= CSIDL_APPDATA,
#endif
#ifdef CSIDL_COMMON_APPDATA
	CommonApplicationData	= CSIDL_COMMON_APPDATA,
#endif
#ifdef CSIDL_PROGRAM_FILES_COMMON
	CommonProgramFiles		= CSIDL_PROGRAM_FILES_COMMON,
#endif
#ifdef CSIDL_COOKIES
	Cookies					= CSIDL_COOKIES,
#endif
	Desktop					= CSIDL_DESKTOP,
	DesktopDirectory		= CSIDL_DESKTOPDIRECTORY,
	Favorites				= CSIDL_FAVORITES,
#ifdef CSIDL_HISTORY
	History					= CSIDL_HISTORY,
#endif
#ifdef CSIDL_INTERNET_CACHE
	InternetCache			= CSIDL_INTERNET_CACHE,
#endif
#ifdef CSIDL_LOCAL_APPDATA
	LocalApplicationData	= CSIDL_LOCAL_APPDATA,
#endif
	MyComputer				= CSIDL_DRIVES,
#ifdef CSIDL_MYDOCUMENTS
	MyDocuments				= CSIDL_MYDOCUMENTS,
#endif
	MyMusic					= CSIDL_MYMUSIC,
	MyPictures				= CSIDL_MYPICTURES,
	Personal				= CSIDL_PERSONAL,
	ProgramFiles			= CSIDL_PROGRAM_FILES,
	Programs				= CSIDL_PROGRAMS,
	Recent					= CSIDL_RECENT,
	SendTo					= CSIDL_SENDTO,
	StartMenu				= CSIDL_STARTMENU,
	Startup					= CSIDL_STARTUP,
#ifdef CSIDL_SYSTEM
	System					= CSIDL_SYSTEM,
#endif
	Templates				= CSIDL_TEMPLATES,

#endif // CSIDL_DESKTOP
} END_ENUM_CLASS(SpecialFolder);


std::vector<String> ParseCommandLine(RCString s);

class AFX_CLASS Environment {
	typedef Environment class_type;
public:
	EXT_DATA static int ExitCode;
	EXT_DATA static const OperatingSystem OSVersion;

#if UCFG_WIN32
	static Int32 AFXAPI TickCount();
#endif

#if UCFG_WIN32_FULL
	class CStringsKeeper {
	public:
		LPTSTR m_p;

		CStringsKeeper();
		~CStringsKeeper();
	};

	static UInt32 AFXAPI GetLastInputInfo();
#endif

	static String AFXAPI CommandLine();
#if !UCFG_WCE
	EXT_API static std::vector<String> AFXAPI GetCommandLineArgs();
	EXT_API static String AFXAPI GetEnvironmentVariable(RCString s);
	EXT_API static void AFXAPI SetEnvironmentVariable(RCString name, RCString val);
	AFX_API static String AFXAPI ExpandEnvironmentVariables(RCString name);
	EXT_API static std::map<String, String> AFXAPI GetEnvironmentVariables();
#endif

	static String AFXAPI SystemDirectory();
	EXT_API static String AFXAPI GetFolderPath(SpecialFolder folder);
	static String AFXAPI GetMachineType();
	static String AFXAPI GetMachineVersion();
	
	int get_ProcessorCount();
	DEFPROP_GET(int, ProcessorCount);

	int get_ProcessorCoreCount();
	DEFPROP_GET(int, ProcessorCoreCount);
};

extern EXT_DATA Ext::Environment Environment;

bool AFXAPI IsConsole();

struct CaseInsensitiveStringLess : std::less<String> {
	bool operator()(RCString a, RCString b) const {
		return a.CompareNoCase(b) < 0;
	}
};	

struct CaseInsensitiveHash {
	size_t operator()(RCString s) const {
		return std::hash<String>()(s.ToUpper());
	}
};

struct CaseInsensitiveEqual {
	size_t operator()(RCString a, RCString b) const {
		return a.CompareNoCase(b) == 0;
	}
};

class NameValueCollection : public std::map<String, CStringVector, CaseInsensitiveStringLess>, public CPrintable {
	typedef std::map<String, CStringVector, CaseInsensitiveStringLess> base;
public:
	CStringVector GetValues(RCString key) {
		CStringVector r;
		base::iterator i = find(key);
		return i!=end() ? i->second : CStringVector();
	}

	String Get(RCString key) {
		return find(key)!=end() ? String::Join(",", GetValues(key)) : nullptr;
	}

	void Set(RCString name, RCString v) {
		CStringVector ar;
		ar.push_back(v);
		(*this)[name] = ar;
	}

	String ToString() const;
};

class WebHeaderCollection : public NameValueCollection {
};

class HttpUtility {
public:
	static String AFXAPI UrlEncode(RCString s, Encoding& enc = Encoding::UTF8);
	static String AFXAPI UrlDecode(RCString s, Encoding& enc = Encoding::UTF8);

	static NameValueCollection AFXAPI ParseQueryString(RCString query);
};


typedef std::unordered_map<UINT, const char*> CMapStringRes;
CMapStringRes& MapStringRes();

typedef vararray<byte, 64> hashval;
//!!!R typedef Blob HashValue;

class HashAlgorithm {
public:
	CBool IsHaifa;

	virtual ~HashAlgorithm() {}
	virtual hashval ComputeHash(Stream& stm);
	virtual hashval ComputeHash(const ConstBuf& mb);

	virtual void InitHash(void *dst) {}
	virtual void HashBlock(void *dst, const byte *src, UInt64 counter) noexcept {}
protected:
	CBool Is64Bit;
};

class Crc32 : public HashAlgorithm {
	typedef HashAlgorithm base;
public:
	using base::ComputeHash;
	hashval ComputeHash(Stream& stm) override;
};

extern EXT_DATA std::mutex g_mfcCS;

#if UCFG_USE_POSIX

class MessageCatalog {
public:
	MessageCatalog()
		:	m_catd(nl_catd(-1))
	{
	}

	~MessageCatalog() {
		if (_self)
			CCheck(::catclose(m_catd));
	}

	EXPLICIT_OPERATOR_BOOL() const {
		return m_catd != nl_catd(-1) ? EXT_CONVERTIBLE_TO_TRUE : 0;
	}

	void Open(const char *name, int oflag = 0) {
		CCheck((m_catd = ::catopen(name, oflag))==(nl_catd)-1 ? -1 : 0);
	}

	String GetMessage(int set_id, int msg_id, const char *s = 0) {
		return ::catgets(m_catd, set_id, msg_id, s);
	}
private:
	nl_catd m_catd;
};


#endif // UCFG_USE_POSIX


class AFX_CLASS CNoTrackString : public CNoTrackObject {
public:
	String m_string;
};

class AFX_CLASS CMessageRange {
public:
	CMessageRange();
	virtual ~CMessageRange();
	virtual String CheckMessage(DWORD code) { return ""; }
};

class EXTAPI CMessageProcessor {
	struct CModuleInfo {
		UInt32 m_lowerCode, m_upperCode;
		String m_moduleName;
#if UCFG_USE_POSIX
		MessageCatalog m_mcat;
		CBool m_bCheckedOpen;
#endif
		void Init(UInt32 lowerCode, UInt32 upperCode, RCString moduleName);
		String GetMessage(HRESULT hr);
	};

	std::vector<CModuleInfo> m_vec;
	CModuleInfo m_default;

	void CheckStandard();
public:
	std::vector<CMessageRange*> m_ranges;

	CMessageProcessor();
	static void AFXAPI RegisterModule(DWORD lowerCode, DWORD upperCode, RCString moduleName);
#if UCFG_COM
	static HRESULT AFXAPI Process(HRESULT hr, EXCEPINFO *pexcepinfo);
#endif
	String ProcessInst(HRESULT hr);
	static String AFXAPI Process(HRESULT hr);

	CThreadLocal<CNoTrackString> m_param;

	static void AFXAPI SetParam(RCString s);
};

extern CMessageProcessor g_messageProcessor;

AFX_API String AFXAPI AfxProcessError(HRESULT hr);
#if UCFG_COM
AFX_API HRESULT AFXAPI AfxProcessError(HRESULT hr, EXCEPINFO *pexcepinfo);
#endif

ENUM_CLASS(VarType) {
	Null,
	Int,
	Float,
	Bool,
	String,
	Array,
	Map
} END_ENUM_CLASS(VarType);

class VarValue;

class VarValueObj : public Object {
public:
	virtual bool HasKey(RCString key) const =0;
	virtual VarType type() const =0;
	virtual size_t size() const =0;
	virtual VarValue operator[](int idx) const =0;
	virtual VarValue operator[](RCString key) const =0;
	virtual String ToString() const =0;
	virtual Int64 ToInt64() const =0;
	virtual bool ToBool() const =0;
	virtual double ToDouble() const =0;
	virtual void Set(int idx, const VarValue& v) =0;
	virtual void Set(RCString key, const VarValue& v) =0;
	virtual std::vector<String> Keys() const =0;
};

class VarValue {
public:	
	ptr<VarValueObj> m_pimpl;

	VarValue() {}

	VarValue(std::nullptr_t) {}

	VarValue(Int64 v);
	VarValue(int v);
	VarValue(double v);
	explicit VarValue(bool v);
	VarValue(RCString v);
	VarValue(const char *p);
	VarValue(const std::vector<VarValue>& ar);

	bool operator==(const VarValue& v) const;
	bool operator!=(const VarValue& v) const { return !operator==(v); }

	EXPLICIT_OPERATOR_BOOL() const {
		return m_pimpl ? EXT_CONVERTIBLE_TO_TRUE : 0;
	}

	bool HasKey(RCString key) const { return Impl().HasKey(key); }
	VarType type() const {
		return m_pimpl ? Impl().type() : VarType::Null;
	}
	size_t size() const { return Impl().size(); }
	VarValue operator[](int idx)  const { return Impl().operator[](idx); }
	VarValue operator[](RCString key) const { return Impl().operator[](key); }
	String ToString() const { return Impl().ToString(); }
	Int64 ToInt64() const { return Impl().ToInt64(); }
	bool ToBool() const { return Impl().ToBool(); }
	double ToDouble() const { return Impl().ToDouble(); }
	void Set(int idx, const VarValue& v);
	void Set(RCString key, const VarValue& v);
	void SetType(VarType typ);
	std::vector<String> Keys() const { return Impl().Keys(); }
private:
	template <typename T>
	VarValue(const T*);							// to prevent VarValue(bool) ctor for pointers

	VarValueObj& Impl() const {
		if (!m_pimpl)
			Throw(E_INVALIDARG);
		return *m_pimpl;
	}
};

} // Ext::
namespace EXT_HASH_VALUE_NS {
	size_t hash_value(const Ext::VarValue& v);
}
EXT_DEF_HASH(Ext::VarValue)
namespace Ext {


class MarkupParser : public Object {
public:
	virtual ~MarkupParser() {
	}

	virtual VarValue Parse(std::istream& is, Encoding *enc = &Encoding::UTF8) =0;
	virtual void Print(std::ostream& os, const VarValue& v) =0;

	static ptr<MarkupParser> AFXAPI CreateJsonParser();
};

VarValue AFXAPI ParseJson(RCString s);

template <class T>
struct PersistentTraits {
	static void Read(const BinaryReader& rd, T& v) {
		rd >> v;
	}

	static void Write(BinaryWriter& wr, const T& v) {
		wr << v;
	}
};

#ifndef UCFG_USE_PERSISTENT_CACHE
#	define UCFG_USE_PERSISTENT_CACHE 1
#endif

class PersistentCache {
public:
	template <class T>
	static bool TryGet(RCString key, T& val) {
#if UCFG_USE_PERSISTENT_CACHE
		Blob blob;
		if (!Lookup(key, blob))
			return false;
		CMemReadStream ms(blob);
		PersistentTraits<T>::Read(BinaryReader(ms), val);
		return true;
#else
		return false;
#endif
	}

	template <class T>
	static void Set(RCString key, const T& val) {
#if UCFG_USE_PERSISTENT_CACHE
		MemoryStream ms;
		BinaryWriter wr(ms);
		PersistentTraits<T>::Write(wr, val);
		Set(key, ConstBuf(ms.get_Blob()));
#endif
	}
private:
	static bool AFXAPI Lookup(RCString key, Blob& blob);
	static void AFXAPI Set(RCString key, const ConstBuf& mb);
};

class Temperature : public CPrintable {
public:
	EXT_DATA static const Temperature Zero;

	Temperature()
		:	m_kelvin(0)
	{}

	static Temperature FromKelvin(float v) { return Temperature(v); };
	static Temperature FromCelsius(float v) { return Temperature(float(v+273.15)); };
	static Temperature FromFahrenheit(float v) { return Temperature(float((v-32)*5/9+273.15)); };

	operator float() const { return m_kelvin; }
	float ToCelsius() const { return float(m_kelvin-273.15); }
	float ToFahrenheit() const { return float((m_kelvin-273.15)*1.8+32); }

	bool operator==(const Temperature& v) const { return m_kelvin == v.m_kelvin; }
	bool operator<(const Temperature& v) const { return m_kelvin < v.m_kelvin; }

	String ToString() const;
private:
	float m_kelvin;				// float to be atomic

	explicit Temperature(float kelvin)
		:	m_kelvin(kelvin)
	{}
};

std::ostream& AFXAPI GetNullStream();
std::wostream& AFXAPI GetWNullStream();


extern EXT_API std::mutex g_mtxObjectCounter;
typedef std::unordered_map<const std::type_info*, int> CTiToCounter;
extern EXT_API CTiToCounter g_tiToCounter;

void AFXAPI LogObjectCounters(bool fFull = true);

template <class T>
class DbgObjectCounter {
public:
	DbgObjectCounter() {
		EXT_LOCK (g_mtxObjectCounter) {
			++g_tiToCounter[&typeid(T)];
		}
	}

	DbgObjectCounter(const DbgObjectCounter&) {
		EXT_LOCK (g_mtxObjectCounter) {
			++g_tiToCounter[&typeid(T)];
		}
	}

	~DbgObjectCounter() {
		EXT_LOCK (g_mtxObjectCounter) {
			--g_tiToCounter[&typeid(T)];
		}
	}
};

#ifdef _DEBUG
#	define DBG_OBJECT_COUNTER(typ) Ext::DbgObjectCounter<typ> _dbgObjectCounter;
#else  
#	define DBG_OBJECT_COUNTER(typ) 
#endif


struct ProcessStartInfo {
	std::map<String, String> EnvironmentVariables;
	String FileName,
		Arguments,
		WorkingDirectory;
	CBool CreateNewWindow,
		CreateNoWindow,
		RedirectStandardInput,
		RedirectStandardOutput,
		RedirectStandardError;
	DWORD Flags;

	ProcessStartInfo(RCString fileName = String(), RCString arguments = String());
};

class ProcessObj : public SafeHandle { //!!!
	typedef SafeHandle base;
public:
	typedef Interlocked interlocked_policy;

	ProcessStartInfo StartInfo;

#if UCFG_EXTENDED && !UCFG_WCE
protected:
	File m_fileIn, m_fileOut, m_fileErr;
public:
	FileStream StandardInput,
		StandardOutput,
		StandardError;
#endif
public:
	ProcessObj();
	ProcessObj(HANDLE handle, bool bOwn = false);

	DWORD get_ID() const;
	DWORD get_ExitCode() const;
	bool get_HasExited();
	bool Start();
	void Kill();
	void WaitForExit(DWORD ms = INFINITE);

#if UCFG_WIN32
	ProcessObj(pid_t pid, DWORD dwAccess = MAXIMUM_ALLOWED, bool bInherit = false);

	EXT_API std::unique_ptr<CWinThread> Create(RCString commandLine, DWORD dwFlags = 0, const char *dir = 0, bool bInherit = false, STARTUPINFO *psi = 0);

	void Terminate(DWORD dwExitCode)  {	Win32Check(::TerminateProcess(HandleAccess(*this), dwExitCode)); }

	AFX_API CTimesInfo get_Times() const;
	DEFPROP_GET(CTimesInfo, Times);

	HWND get_MainWindowHandle() const;
	DEFPROP_GET(HWND, MainWindowHandle);

#if !UCFG_WCE
	AFX_API DWORD get_Version() const;
	DEFPROP_GET(DWORD, Version);

	bool get_IsWow64();
	DEFPROP_GET(bool, IsWow64);

	DWORD get_PriorityClass() { return Win32Check(::GetPriorityClass(HandleAccess(*this))); }
	void put_PriorityClass(DWORD pc) { Win32Check(::SetPriorityClass(HandleAccess(*this), pc)); }

	String get_MainModuleFileName();
	DEFPROP_GET(String, MainModuleFileName);
#endif

	void FlushInstructionCache(LPCVOID base = 0, SIZE_T size = 0) { Win32Check(::FlushInstructionCache(HandleAccess(*this), base, size)); }

	DWORD VirtualProtect(void *addr, size_t size, DWORD flNewProtect);
	MEMORY_BASIC_INFORMATION VirtualQuery(const void *addr);
	SIZE_T ReadMemory(LPCVOID base, LPVOID buf, SIZE_T size);
	SIZE_T WriteMemory(LPVOID base, LPCVOID buf, SIZE_T size); 	
#endif // UCFG_WIN32
protected:
	bool Valid() const override {
		return DangerousGetHandleEx() != 0;
	}

	void CommonInit();

	int m_stat_loc;
	mutable CInt<DWORD> m_pid;
};


class Process : public Pimpl<ProcessObj> {
	typedef Process class_type;
public:
	static Process AFXAPI GetProcessById(pid_t pid);
	static Process AFXAPI GetCurrentProcess();

	ProcessStartInfo& StartInfo() { return m_pimpl->StartInfo; }

	static Process AFXAPI Start(const ProcessStartInfo& psi);

#if UCFG_EXTENDED && !UCFG_WCE
	Stream& StandardInput() { return m_pimpl->StandardInput; }
#endif

	DWORD get_ID() const { return m_pimpl->get_ID(); }
	DEFPROP_GET(DWORD, ID);

	DWORD get_ExitCode() const { return m_pimpl->get_ExitCode(); }
	DEFPROP_GET(DWORD, ExitCode);

	void Kill() { m_pimpl->Kill(); }
	void WaitForExit(DWORD ms = INFINITE) { m_pimpl->WaitForExit(ms); }

	bool get_HasExited() { return m_pimpl->get_HasExited(); }
	DEFPROP_GET(bool, HasExited);

	String get_ProcessName();
	DEFPROP_GET(String, ProcessName);

	static std::vector<Process> AFXAPI GetProcesses();
	static std::vector<Process> AFXAPI GetProcessesByName(RCString name);

#if UCFG_WIN32_FULL
	DWORD get_PriorityClass() { return m_pimpl->get_PriorityClass(); }
	void put_PriorityClass(DWORD pc) { m_pimpl->put_PriorityClass(pc); }
	DEFPROP(DWORD, PriorityClass);

	void GenerateConsoleCtrlEvent(DWORD dwCtrlEvent = 1);		// CTRL_BREAK_EVENT==1
#endif

};


} // Ext::

#if UCFG_USE_REGEX
#	include "ext-regex.h"
#endif

