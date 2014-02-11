/*######     Copyright (c) 1997-2013 Ufasoft  http://ufasoft.com  mailto:support@ufasoft.com,  Sergey Pavlov  mailto:dev@ufasoft.com #######################################
#                                                                                                                                                                          #
# This program is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation;  #
# either version 3, or (at your option) any later version. This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the      #
# implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details. You should have received a copy of the GNU #
# General Public License along with this program; If not, see <http://www.gnu.org/licenses/>                                                                               #
##########################################################################################################################################################################*/

#pragma once

#include "db-itf.h"

#ifndef UCFG_USE_SQLITE
#	define UCFG_USE_SQLITE 3
#endif

#ifndef UCFG_USE_SQLITE_MDB
#	define UCFG_USE_SQLITE_MDB 0
#endif

#if UCFG_USE_SQLITE == 3
#	define sqlite_(name) sqlite3_##name
#	define SQLITE_(name) SQLITE_##name

	struct sqlite3;
	typedef sqlite3 sqlite_db;
#elif UCFG_USE_SQLITE == 4
#	define sqlite_(name) sqlite4_##name
#	define SQLITE_(name) SQLITE4_##name

	struct sqlite4;
	typedef sqlite4 sqlite_db;
#endif

struct sqlite_(stmt);
struct sqlite_(vfs);

#define SQLITE_CHECKPOINT_RESTART 2		//!!! dup

namespace Ext { namespace DB { namespace sqlite_(NS) {



int SqliteCheck(sqlite_db *db, int code);

class SqliteException : public DbException {
	typedef DbException base;
public:
	SqliteException(HRESULT hr, RCString s);
};

class SqliteConnection;
class SqliteCommand;

class SqliteReader : public IDataReader {
public:
	SqliteReader(SqliteCommand& cmd);
	~SqliteReader();
	Int32 GetInt32(int i) override;
	Int64 GetInt64(int i) override;
	double GetDouble(int i) override;
	String GetString(int i) override;
	ConstBuf GetBytes(int i) override;
	DbType GetFieldType(int i) override;
	int FieldCount() override;
	String GetName(int idx) override;
	bool Read() override;
private:
	SqliteCommand& m_cmd;
};

class DbDataReader : public Pimpl<SqliteReader> {
	typedef Pimpl<SqliteReader> base;
public:
	DbDataReader() {
	}

	Int32 GetInt32(int i) { return m_pimpl->GetInt32(i); }
	Int64 GetInt64(int i) { return m_pimpl->GetInt64(i); }
	double GetDouble(int i) { return m_pimpl->GetDouble(i); }
	String GetString(int i) { return m_pimpl->GetString(i); }
	ConstBuf GetBytes(int i) { return m_pimpl->GetBytes(i); }
	DbType GetFieldType(int i) { return m_pimpl->GetFieldType(i); }
	int FieldCount() { return m_pimpl->FieldCount(); }
	String GetName(int idx) { return m_pimpl->GetName(idx); }
	bool Read() { return m_pimpl->Read(); }
	int GetOrdinal(RCString name) { return m_pimpl->GetOrdinal(name); }
	bool IsDBNull(int i) { return m_pimpl->IsDBNull(i); }
protected:
	DbDataReader(SqliteReader *sr) {
		m_pimpl = sr;
	}

	friend class SqliteCommand;
};

class SqliteCommand : public IDbCommand {
public:
	SqliteConnection& m_con;

	SqliteCommand(SqliteConnection& con);
	SqliteCommand(RCString cmdText, SqliteConnection& con);
	~SqliteCommand();
	operator sqlite_(stmt)*() { return m_stmt; }
	void ClearBindings();

	SqliteCommand& Bind(int column, std::nullptr_t) override;
	SqliteCommand& Bind(int column, Int32 v) override;
	SqliteCommand& Bind(int column, Int64 v) override;
	SqliteCommand& Bind(int column, const ConstBuf& mb, bool bTransient = true) override;
	SqliteCommand& Bind(int column, RCString s) override;

	SqliteCommand& Bind(RCString parname, std::nullptr_t) override;
	SqliteCommand& Bind(RCString parname, Int32 v) override;
	SqliteCommand& Bind(RCString parname, Int64 v) override;
	SqliteCommand& Bind(RCString parname, const ConstBuf& mb, bool bTransient = true) override;
	SqliteCommand& Bind(RCString parname, RCString s) override;

#if	UCFG_SEPARATE_LONG_TYPE
	SqliteCommand& Bind(int column, long v) { return Bind(column, Int64(v)); }
	SqliteCommand& Bind(RCString parname, long v) { return Bind(parname, Int64(v)); }
#endif

	void Dispose() override;
	void ExecuteNonQuery() override;
	DbDataReader ExecuteReader();
	DbDataReader ExecuteVector();
	String ExecuteScalar() override;
	Int64 ExecuteInt64Scalar();
private:
	CPointer<sqlite_(stmt)> m_stmt;
	CBool m_bNeedReset;
	
	sqlite_(stmt) *Handle();
	sqlite_(stmt) *ResetHandle(bool bNewNeedReset = false);

	friend class SqliteConnection;
	friend class SqliteReader;
};

class SqliteConnection : public IDbConn, public ITransactionable {
public:
	SqliteConnection() {
	}

	SqliteConnection(RCString file) {
		Open(file);
	}

	~SqliteConnection() {
		Close();
	}

	operator sqlite_db*() { return m_db; }

	void SetProgressHandler(int(*pfn)(void*), void* p = 0, int n = 1);
	void Create(RCString file) override;
	void Open(RCString file, FileAccess fileAccess = FileAccess::ReadWrite, FileShare share = FileShare::ReadWrite) override;
	void Close() override;

	ptr<IDbCommand> CreateCommand() override;
	void ExecuteNonQuery(RCString sql) override;
	Int64 get_LastInsertRowId() override;
	pair<int, int> Checkpoint(int eMode);

	void BeginTransaction() override;
	void Commit() override;
	void Rollback() override;
private:
	CPointer<sqlite_db> m_db;
};

class SqliteMalloc {
public:
	SqliteMalloc();
};

class SqliteVfs {
public:
	SqliteVfs(bool bDefault = true);
	~SqliteVfs();
protected:
	sqlite_(vfs) *m_pimpl;
};

class MMappedSqliteVfs : public SqliteVfs {
	typedef SqliteVfs base;
public:
	MMappedSqliteVfs();

};


}}} // namespace Ext::DB::sqlite_(NS)::


