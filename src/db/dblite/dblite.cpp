/*######   Copyright (c) 2014-2019 Ufasoft  http://ufasoft.com  mailto:support@ufasoft.com,  Sergey Pavlov  mailto:dev@ufasoft.com ####
#                                                                                                                                     #
# 		See LICENSE for licensing information                                                                                         #
#####################################################################################################################################*/

#include <el/ext.h>

#include "dblite.h"
#include "b-tree.h"

namespace Ext { namespace DB { namespace KV {


void DbTable::CheckKeyArg(RCSpan k) {
	if (k.size() == 0 || k.size() > MAX_KEY_SIZE)
		Throw(errc::invalid_argument);
}

void DbTable::Open(DbTransaction& tx, bool bCreate) {
	CKVStorageKeeper keeper(&tx.Storage);

	DbCursor cM(tx, DbTable::Main());
	Span k((const uint8_t*)Name.c_str(), Name.length());
	if (!cM.SeekToKey(k)) {
		if (!bCreate)
			Throw(E_FAIL);
		TableData td;
		td.Type = (uint8_t)Type;
		td.HtType = (uint8_t)HtType;
		td.RootPgNo = 0;
		td.KeySize = KeySize;
		DbTable::Main().Put(tx, k, Span((const uint8_t*)&td, sizeof td));
	//	cM.SeekToKey(k);		//!!!?
	}
}

void DbTable::Drop(DbTransaction& tx) {
	if (tx.ReadOnly)
		Throw(errc::permission_denied);
	DbCursor(tx, _self).Drop();
	Span k((const uint8_t*)Name.c_str(), Name.length());
	DbTable::Main().Delete(tx, k);
}

void DbTable::Put(DbTransaction& tx, RCSpan k, RCSpan d, bool bInsert) {
	CKVStorageKeeper keeper(&tx.Storage);
	CheckKeyArg(k);

	DbCursor c(tx, _self);
	c.Put(k, d, bInsert);
}

bool DbTable::Delete(DbTransaction& tx, RCSpan k) {
	CKVStorageKeeper keeper(&tx.Storage);
	CheckKeyArg(k);

	DbCursor c(tx, _self);
	bool r = c.SeekToKey(k);
	if (r)
		c.Delete();
	return r;
}

void KVStorage::Vacuum() {
	path pathParent = FilePath.parent_path();
	if (pathParent.empty())
		pathParent = ".";
	path tmpPath = Path::GetTempFileName(pathParent, "tmp").first;
	lock_guard<mutex> lkWrite(MtxWrite);
	unique_lock<shared_mutex> lk(ShMtx, defer_lock);
	try {
		KVStorage dbNew;
		dbNew.ProtectPages = false;
		dbNew.AppName = AppName;
		dbNew.UserVersion = UserVersion;
		dbNew.FrontEndName = FrontEndName;
		dbNew.FrontEndVersion = FrontEndVersion;

		dbNew.Create(tmpPath);
		{
			DbTransaction txS(_self, true), txD(dbNew);
			txD.Bulk = true;

			int nProgress = m_stepProgress;

			for (DbCursor ct(txS, DbTable::Main()); ct.SeekToNext();) {
				string tableName((const char*)ct.Key.data(), ct.Key.size());
				const TableData& td = *(const TableData*)ct.get_Data().data();

				DbTable tS(tableName), tD(tableName);
				tD.Type = (TableType)td.Type;
				tD.KeySize = td.KeySize;
				tD.Open(txD, true);
				int n = 0;
				int64_t bytes = 0;
				for (DbCursor c(txS, tS); c.SeekToNext();) {
					tD.Put(txD, c.Key, c.Data);
					if (m_pfnProgress && !--nProgress) {
						if (m_pfnProgress(m_ctxProgress))
							Throw(HRESULT_FROM_WIN32(ERROR_CANCELLED));
						nProgress = m_stepProgress;
					}
					++n;
					bytes += c.get_Data().size();
				}
				TRC(2, "Table: " << tableName << ":\t" << n << " records\t" << bytes << " bytes");
			}
			txD.Commit();
		}
		lk.lock();
		Close(false);
	} catch (RCExc) {
		filesystem::remove(tmpPath);
		throw;
	}
	path tmpOriginal = FilePath;
	tmpOriginal.replace_extension((String(FilePath.extension().native()) + ".bak").c_str());
	filesystem::rename(FilePath, tmpOriginal);
	filesystem::rename(tmpPath, FilePath);
	filesystem::remove(tmpOriginal);
	Open(FilePath);
}


}}} // Ext::DB::KV::
