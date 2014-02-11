/*######     Copyright (c) 1997-2013 Ufasoft  http://ufasoft.com  mailto:support@ufasoft.com,  Sergey Pavlov  mailto:dev@ufasoft.com #######################################
#                                                                                                                                                                          #
# This program is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation;  #
# either version 3, or (at your option) any later version. This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the      #
# implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details. You should have received a copy of the GNU #
# General Public License along with this program; If not, see <http://www.gnu.org/licenses/>                                                                               #
##########################################################################################################################################################################*/

#include <el/ext.h>


#if UCFG_WIN32_FULL
	#include <el/win/nt.h>
	using namespace NT;
#	pragma comment(lib, "ntdll")
#endif // UCFG_WIN32_FULL


#if defined(HAVE_SQLITE3) || defined(HAVE_SQLITE4)

#	include "ext-sqlite.h"

#	if UCFG_USE_SQLITE==3
#		include <sqlite3.h>
#		if UCFG_USE_SQLITE_MDB
#			pragma comment(lib, "sqlite3_mdb")
#		else
#			pragma comment(lib, "sqlite3")
#		endif

#		define sqlite_prepare sqlite3_prepare_v2
#	else
#		include <sqlite4.h>
#		pragma comment(lib, "sqlite4")

#		define sqlite_prepare sqlite4_prepare

#		define SQLITE_OK				SQLITE_(OK)
#		define SQLITE_ROW				SQLITE_(ROW)
#		define SQLITE_DONE				SQLITE_(DONE)
#		define SQLITE_OPEN_EXCLUSIVE	SQLITE_(OPEN_EXCLUSIVE)
#		define SQLITE_OPEN_READONLY		SQLITE_(OPEN_READONLY)
#		define SQLITE_OPEN_READWRITE	SQLITE_(OPEN_READWRITE)
#	endif

#define sqlite_close					sqlite_(close)
#define sqlite_column_int64				sqlite_(column_int64)
#define sqlite_column_text16			sqlite_(column_text16)
#define sqlite_complete					sqlite_(complete)
#define sqlite_complete16				sqlite_(complete16)
#define sqlite_extended_result_codes	sqlite_(extended_result_codes)
#define sqlite_file						sqlite_(file)
#define sqlite_last_insert_rowid		sqlite_(last_insert_rowid)
#define sqlite_mem_methods				sqlite_(mem_methods)
#define sqlite_progress_handler			sqlite_(progress_handler)
#define sqlite_step						sqlite_(step)
#define sqlite_stmt						sqlite_(stmt)
#define sqlite_vfs_unregister			sqlite_(vfs_unregister)
#define sqlite_vfs						sqlite_(vfs)

namespace Ext { namespace DB { namespace sqlite_(NS) {

static struct CSqliteExceptionFabric : CExceptionFabric {
	CSqliteExceptionFabric(int fac)
		:	CExceptionFabric(fac)
	{		
	}

	DECLSPEC_NORETURN void ThrowException(HRESULT hr, RCString msg) {
		throw SqliteException(hr, msg);
	}
} s_exceptionFabric(FACILITY_SQLITE);

SqliteException::SqliteException(HRESULT hr, RCString s)
	:	base(hr, "SQLite Error: "+s)
{
}

int SqliteCheck(sqlite_db *db, int code) {
	switch (code) {
	case SQLITE_OK:
	case SQLITE_ROW:
	case SQLITE_DONE:
		return code;
	}
	throw SqliteException(MAKE_HRESULT(SEVERITY_ERROR, FACILITY_SQLITE, code), db ? String((const Char16*)sqlite_(errmsg16)(db)) : String());
}

bool SqliteIsComplete(const char *sql) {
	switch (int rc = ::sqlite_complete(sql)) {
	case 0:
	case 1:
		return rc;
	case SQLITE_(NOMEM):
		Throw(E_OUTOFMEMORY);
	default:
		Throw(E_FAIL);
	}
}

bool SqliteIsComplete16(const void *sql) {
	switch (int rc = ::sqlite_complete16(sql)) {
	case 0:
	case 1:
		return rc;
	case SQLITE_(NOMEM):
		Throw(E_OUTOFMEMORY);
	default:
		Throw(E_FAIL);
	}
}

SqliteReader::SqliteReader(SqliteCommand& cmd)
	:	m_cmd(cmd)
{
}

SqliteReader::~SqliteReader() {
	if (std::uncaught_exception()) {
		try {
			m_cmd.ResetHandle();
		} catch (RCExc) {
		}
	} else
		m_cmd.ResetHandle();
}

bool SqliteReader::Read() {	
	return SqliteCheck(m_cmd.m_con, ::sqlite_(step)(m_cmd)) == SQLITE_(ROW);
}

Int32 SqliteReader::GetInt32(int i) {
	return sqlite_(column_int)(m_cmd, i);
}

Int64 SqliteReader::GetInt64(int i) {
	return sqlite_(column_int64)(m_cmd, i);
}

double SqliteReader::GetDouble(int i) {
	return sqlite_(column_double)(m_cmd, i);
}

String SqliteReader::GetString(int i) {
	return (const Char16*)sqlite_(column_text16)(m_cmd, i);
}

ConstBuf SqliteReader::GetBytes(int i) {
	sqlite_(value) *value = sqlite_(column_value)(m_cmd, i);
	return ConstBuf(sqlite_(value_blob)(value), sqlite_(value_bytes)(value));
}

DbType SqliteReader::GetFieldType(int i) {
	switch (int typ = ::sqlite_(column_type)(m_cmd, i)) {
	case SQLITE_(NULL): return DbType::Null;
	case SQLITE_(INTEGER): return DbType::Integer;
	case SQLITE_(FLOAT): return DbType::Float;
	case SQLITE_(BLOB): return DbType::Blob;
	case SQLITE_(TEXT): return DbType::Text;
	default:
		Throw(E_FAIL);
	}
}

int SqliteReader::FieldCount() {
	return ::sqlite_(column_count)(m_cmd);
}

String SqliteReader::GetName(int idx) {
	return (const Char16*)::sqlite_(column_name16)(m_cmd, idx);
}

SqliteCommand::SqliteCommand(SqliteConnection& con)
	:	m_con(con)
{
	m_con.Register(_self);
}

SqliteCommand::SqliteCommand(RCString cmdText, SqliteConnection& con)
	:	m_con(con)
{
	m_con.Register(_self);
	CommandText = cmdText;
}

SqliteCommand::~SqliteCommand() {
	m_con.Unregister(_self);
	Dispose();
}

void SqliteCommand::Dispose() {
	if (m_stmt) {
		int rc = ::sqlite_(finalize)(exchange(m_stmt, nullptr));
		if (rc != SQLITE_(BUSY) && !std::uncaught_exception())
			SqliteCheck(m_con, rc);
	}
}

sqlite_(stmt) *SqliteCommand::Handle() {
	if (!m_stmt) {
#if UCFG_USE_SQLITE==3
		const void *tail = 0;
		SqliteCheck(m_con, ::sqlite3_prepare16_v2(m_con, (const String::Char*)CommandText, -1, &m_stmt, &tail));
#else
		const char *tail = 0;
		Blob utf = Encoding::UTF8.GetBytes(CommandText);
		SqliteCheck(m_con, ::sqlite4_prepare(m_con, (const char*)utf.constData(), -1, &m_stmt, &tail));
#endif
	}
	return m_stmt;
}

sqlite_stmt *SqliteCommand::ResetHandle(bool bNewNeedReset) {
	sqlite_(stmt) *h = Handle();
	if (m_bNeedReset) {
		int rc = ::sqlite_(reset)(h);
		if (SQLITE_(CONSTRAINT) != (byte)rc)
			SqliteCheck(m_con, rc);
	}
	m_bNeedReset = bNewNeedReset;
	return h;
}

void SqliteCommand::ClearBindings() {
	SqliteCheck(m_con, ::sqlite_(clear_bindings)(Handle()));
}

SqliteCommand& SqliteCommand::Bind(int column, std::nullptr_t) {
	SqliteCheck(m_con, ::sqlite_(bind_null)(ResetHandle(), column));
	return _self;
}

SqliteCommand& SqliteCommand::Bind(int column, Int32 v) {
	SqliteCheck(m_con, ::sqlite_(bind_int)(ResetHandle(), column, v));
	return _self;
}

SqliteCommand& SqliteCommand::Bind(int column, Int64 v) {
	SqliteCheck(m_con, ::sqlite_(bind_int64)(ResetHandle(), column, v));
	return _self;
}

SqliteCommand& SqliteCommand::Bind(int column, const ConstBuf& mb, bool bTransient) {
	SqliteCheck(m_con, ::sqlite_(bind_blob)(ResetHandle(), column, mb.P, mb.Size, bTransient ? SQLITE_(TRANSIENT) : SQLITE_(STATIC)));
	return _self;
}

SqliteCommand& SqliteCommand::Bind(int column, RCString s) {
	const Char16 *p = (const Char16*)s;
	SqliteCheck(m_con, p ? ::sqlite_(bind_text16)(ResetHandle(), column, p, s.Length*2, SQLITE_(TRANSIENT)) : ::sqlite_(bind_null)(ResetHandle(), column));
	return _self;
}

SqliteCommand& SqliteCommand::Bind(RCString parname, std::nullptr_t v) {
	return Bind(::sqlite_(bind_parameter_index)(ResetHandle(), parname), v);
}

SqliteCommand& SqliteCommand::Bind(RCString parname, Int32 v) {
	return Bind(::sqlite_(bind_parameter_index)(ResetHandle(), parname), v); 
}

SqliteCommand& SqliteCommand::Bind(RCString parname, Int64 v) {
	return Bind(::sqlite_(bind_parameter_index)(ResetHandle(), parname), v); 
}

SqliteCommand& SqliteCommand::Bind(RCString parname, const ConstBuf& mb, bool bTransient) {
	return Bind(::sqlite_(bind_parameter_index)(ResetHandle(), parname), mb, bTransient); 
}

SqliteCommand& SqliteCommand::Bind(RCString parname, RCString s) {
	return Bind(::sqlite_(bind_parameter_index)(ResetHandle(), parname), s); 
}

void SqliteCommand::ExecuteNonQuery() {
	SqliteCheck(m_con, ::sqlite_(step)(ResetHandle(true)));
}

DbDataReader SqliteCommand::ExecuteReader() {
	ResetHandle(true);
	return DbDataReader(new SqliteReader(_self));
}

DbDataReader SqliteCommand::ExecuteVector() {
	DbDataReader r = SqliteCommand::ExecuteReader();
	if (!r.Read())
		Throw(E_EXT_DB_NoRecord);
	return r;
}

String SqliteCommand::ExecuteScalar() {
	if (SqliteCheck(m_con, ::sqlite_step(ResetHandle(true))) == SQLITE_ROW)
		return (const Char16*)sqlite_column_text16(m_stmt, 0);
	Throw(E_EXT_DB_NoRecord);
}

Int64 SqliteCommand::ExecuteInt64Scalar() {
	if (SqliteCheck(m_con, ::sqlite_step(ResetHandle(true))) == SQLITE_ROW)
		return ::sqlite_column_int64(m_stmt, 0);
	Throw(E_EXT_DB_NoRecord);
}

ptr<IDbCommand> SqliteConnection::CreateCommand() {
	return ptr<IDbCommand>(new SqliteCommand(_self));
}

void SqliteConnection::ExecuteNonQuery(RCString sql) {
	String s = sql.TrimEnd();
	if (s.Length > 1 && s[s.Length-1] != ';')
		s += ";";

#if UCFG_USE_SQLITE==3
	for (const void *tail=(const Char16*)s; SqliteIsComplete16(tail);) {
		SqliteCommand cmd(_self);
		SqliteCheck(_self, ::sqlite3_prepare16_v2(_self, tail, -1, &cmd.m_stmt, &tail));
#else
	Blob utf = Encoding::UTF8.GetBytes(s);
	for (const char *tail=(const char*)utf.constData(); SqliteIsComplete(tail);) {
		SqliteCommand cmd(_self);
		SqliteCheck(_self, ::sqlite_prepare(_self, tail, -1, &cmd.m_stmt, &tail));
#endif
		cmd.ExecuteNonQuery();
	}
}

Int64 SqliteConnection::get_LastInsertRowId() {
	return sqlite_last_insert_rowid(m_db);
}

pair<int, int> SqliteConnection::Checkpoint(int eMode) {
#if UCFG_USE_SQLITE==3
	int log, ckpt;
	SqliteCheck(_self, ::sqlite3_wal_checkpoint_v2(_self, 0, eMode, &log, &ckpt));
	return make_pair(log, ckpt);
#else
	ExecuteNonQuery("PRAGMA lsm_checkpoint");
	return make_pair(0, 0);							//!!!?
#endif
}

/*!!!
static void __cdecl DbTraceHandler(void *cd, const char *zSql) {
	String s = zSql;
	OutputDebugString(s);
}*/

void SqliteConnection::SetProgressHandler(int(*pfn)(void*), void*p, int n) {
	::sqlite_progress_handler(m_db, n, pfn, p);
}

void SqliteConnection::Create(RCString file) {
#if UCFG_USE_SQLITE==3
	SqliteCheck(m_db, ::sqlite3_open16((const String::Char*)file, &m_db));
	::sqlite_extended_result_codes(m_db, true);	//!!!? only Sqlite3?
#else
	Blob utf = Encoding::UTF8.GetBytes(file);
	SqliteCheck(m_db, ::sqlite4_open(0, (const char*)utf.constData(), &m_db));
#endif
	ExecuteNonQuery("PRAGMA encoding = \"UTF-8\"");
	ExecuteNonQuery("PRAGMA foreign_keys = ON");

/*!!!
#ifndef SQLITE_OMIT_TRACE
	sqlite3_trace(m_con.db, DbTraceHandler, this);
#endif
	*/
}

void SqliteConnection::Open(RCString file, FileAccess fileAccess, FileShare share) {
	int flags = 0;
#if UCFG_USE_SQLITE==3
	if (share == FileShare::None)
		flags = SQLITE_OPEN_EXCLUSIVE;
#else
	Blob utf = Encoding::UTF8.GetBytes(file);
#endif
	switch (fileAccess) {
	case FileAccess::ReadWrite: flags |= SQLITE_OPEN_READWRITE; break;
	case FileAccess::Read: flags |= SQLITE_OPEN_READONLY; break;
	default:
		Throw(E_INVALIDARG);
	}
#if UCFG_USE_SQLITE==3
	SqliteCheck(m_db, ::sqlite3_open_v2(file, &m_db, flags, 0));
	::sqlite3_extended_result_codes(m_db, true);
#else
	SqliteCheck(m_db, ::sqlite4_open(0, (const char*)utf.constData(), &m_db));
#endif
	ExecuteNonQuery("PRAGMA foreign_keys = ON");
}

void SqliteConnection::Close() {
	if (m_db) {
		DisposeCommands();
		sqlite_db *db = exchange(m_db, (sqlite_db*)0);
		SqliteCheck(db, ::sqlite_close(db));
	}
}

void SqliteConnection::BeginTransaction() {
	ExecuteNonQuery("BEGIN TRANSACTION");
}

void SqliteConnection::Commit() {
	ExecuteNonQuery("COMMIT");
}

void SqliteConnection::Rollback() {
	ExecuteNonQuery("ROLLBACK");
}


#if UCFG_USE_SQLITE==3

static void *SqliteMallocFun(int size) {
	return Malloc(size);
}

static void SqliteFree(void *p) {
	Free(p);
}

static void *SqliteRealloc(void *p, int size) {
	return Realloc(p, size);
}

static int SqliteSize(void *p) {
	if (!p)
		return 0;
#if UCFG_INDIRECT_MALLOC
	return (int)CAlloc::s_pfnMSize(p);
#else
	return (int)CAlloc::MSize(p);
#endif
}

static int SqliteRoundup(int size) {
	return (size+7) & ~7;
}

static int SqliteInit(void *) {
	return 0;
}

static void SqliteShutdown(void *) {
}

SqliteMalloc::SqliteMalloc() {
	sqlite_mem_methods memMeth = {
		&SqliteMallocFun,
		&SqliteFree,
		&SqliteRealloc,
		&SqliteSize,
		&SqliteRoundup,
		&SqliteInit,
		SqliteShutdown
	};
	SqliteCheck(0, ::sqlite3_config(SQLITE_CONFIG_MALLOC, &memMeth));
//!!! don't disable MemoryControl	SqliteCheck(0, ::sqlite3_config(SQLITE_CONFIG_MEMSTATUS, 0));
	::sqlite3_soft_heap_limit64(64*1024*1024);
}

typedef int (*PFN_Sqlite_xOpen)(sqlite_vfs*, const char *zName, sqlite_file*, int flags, int *pOutFlags);

#ifdef _WIN32

static int Sqlite_xDeviceCharacteristics(sqlite3_file*) {
	return SQLITE_IOCAP_UNDELETABLE_WHEN_OPEN | SQLITE_IOCAP_ATOMIC512 | SQLITE_IOCAP_ATOMIC1K | SQLITE_IOCAP_ATOMIC2K | SQLITE_IOCAP_ATOMIC4K | SQLITE_IOCAP_SAFE_APPEND | SQLITE_IOCAP_POWERSAFE_OVERWRITE;
}

static PFN_Sqlite_xOpen s_pfnSqlite_xOpen;
static sqlite3_io_methods s_myMethods;

#if UCFG_WIN32_FULL

struct CWinFile {
  void *pMethod;/* Must be first */
  sqlite3_vfs *pVfs;
  HANDLE h;               /* Handle for accessing the file */
};

static int NT_Read(sqlite3_file *file, void *data, int iAmt, sqlite3_int64 iOfst) {
	CWinFile& wf = *(CWinFile*)file;
	IO_STATUS_BLOCK iost;
	LARGE_INTEGER li;
	li.QuadPart = iOfst;
	NTSTATUS st = ::NtReadFile(wf.h, 0, 0, 0, &iost, data, iAmt, &li, 0);
	if (st == STATUS_END_OF_FILE)
		iost.Information = 0;
	else if (st != 0)
		return SQLITE_IOERR_READ;
	if (iost.Information < iAmt) {
	    memset(&((char*)data)[iost.Information], 0, iAmt-iost.Information);
		return SQLITE_IOERR_SHORT_READ;
	}
	return SQLITE_OK;
}

static int NT_Write(sqlite3_file *file, const void *data, int iAmt, sqlite3_int64 iOfst) {
	CWinFile& wf = *(CWinFile*)file;
	IO_STATUS_BLOCK iost;
	LARGE_INTEGER li;
	li.QuadPart = iOfst;
	NTSTATUS st = ::NtWriteFile(wf.h, 0, 0, 0, &iost, (void*)data, iAmt, &li, 0);
	if (st != 0)
		return SQLITE_IOERR_WRITE;
	if (iost.Information < iAmt)
		return SQLITE_FULL;					//!!!?
	return SQLITE_OK;
}

#endif // UCFG_WIN32_FULL


int Sqlite_xOpen(sqlite3_vfs*vfs, const char *zName, sqlite3_file *file, int flags, int *pOutFlags) {
	int rc = s_pfnSqlite_xOpen(vfs, zName, file, flags, pOutFlags);
	if (SQLITE_OK == rc) {
		sqlite3_io_methods methods = *file->pMethods;					//!!!O
		methods.xDeviceCharacteristics = &Sqlite_xDeviceCharacteristics;
#if UCFG_WIN32_FULL
		methods.xRead = &NT_Read;
		methods.xWrite = &NT_Write;
#endif
		s_myMethods = methods;
		file->pMethods = &s_myMethods;
	}
	return rc;
}

#endif


SqliteVfs::SqliteVfs(bool bDefault)
	:	m_pimpl(new sqlite3_vfs)
{
	sqlite3_vfs *defaultVfs = ::sqlite3_vfs_find(0);
	*m_pimpl = *defaultVfs;
#ifdef _WIN32
	s_pfnSqlite_xOpen = m_pimpl->xOpen;
	m_pimpl->xOpen = &Sqlite_xOpen;
#endif
	SqliteCheck(0, ::sqlite3_vfs_register(m_pimpl, bDefault));
}

SqliteVfs::~SqliteVfs() {
	auto_ptr<sqlite3_vfs> p(m_pimpl);
	SqliteCheck(0, ::sqlite_vfs_unregister(p.get()));
}
#endif // UCFG_USE_SQLITE==3



}}} // namespace Ext::DB::sqlite_(NS)::

#endif // HAVE_SQLITE3 || HAVE_SQLITE4
