/*######     Copyright (c) 1997-2015 Ufasoft  http://ufasoft.com  mailto:support@ufasoft.com,  Sergey Pavlov  mailto:dev@ufasoft.com #########################################################################################################
#                                                                                                                                                                                                                                            #
# This program is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation;  either version 3, or (at your option) any later version.          #
# This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.   #
# You should have received a copy of the GNU General Public License along with this program; If not, see <http://www.gnu.org/licenses/>                                                                                                      #
############################################################################################################################################################################################################################################*/

#include <el/ext.h>

#include "dblite.h"
#include "b-tree.h"

namespace Ext { namespace DB { namespace KV {


void DbTable::CheckKeyArg(const ConstBuf& k) {
	if (k.Size == 0 || k.Size > MAX_KEY_SIZE)
		Throw(errc::invalid_argument);
}

void DbTable::Open(DbTransaction& tx, bool bCreate) {
	CKVStorageKeeper keeper(&tx.Storage);

	DbCursor cM(tx, DbTable::Main());
	ConstBuf k(Name.c_str(), strlen(Name.c_str()));
	if (!cM.SeekToKey(k)) {
		if (!bCreate)
			Throw(E_FAIL);
		TableData td;
		td.Type = (byte)Type;
		td.HtType = (byte)HtType;
		td.RootPgNo = 0;
		td.KeySize = KeySize;
		DbTable::Main().Put(tx, k, ConstBuf(&td, sizeof td));
	//	cM.SeekToKey(k);		//!!!?
	}
}

void DbTable::Drop(DbTransaction& tx) {
	if (tx.ReadOnly)
		Throw(errc::permission_denied);
	DbCursor(tx, _self).Drop();
	ConstBuf k(Name.c_str(), strlen(Name.c_str()));
	DbTable::Main().Delete(tx, k);
}

void DbTable::Put(DbTransaction& tx, const ConstBuf& k, const ConstBuf& d, bool bInsert) {
	CKVStorageKeeper keeper(&tx.Storage);
	CheckKeyArg(k);

	DbCursor c(tx, _self);
	c.Put(k, d, bInsert);
}

bool DbTable::Delete(DbTransaction& tx, const ConstBuf& k) {
	CKVStorageKeeper keeper(&tx.Storage);
	CheckKeyArg(k);

	DbCursor c(tx, _self);
	bool r = c.SeekToKey(k);
	if (r)
		c.Delete();
	return r;
}

void KVStorage::Vacuum() {
	path tmpPath = Path::GetTempFileName(FilePath.parent_path(), "tmp").first;
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
				String tableName = Encoding::UTF8.GetChars(ct.Key);
				const TableData& td = *(const TableData*)ct.get_Data().P;

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
					bytes += c.get_Data().Size;
				}
				TRC(2, "Table: " << tableName << ":\t" << n << " records\t" << bytes << " bytes");
			}
			txD.Commit();
		}
		lk.lock();
		Close(false);
	} catch (RCExc) {
		sys::remove(tmpPath);
		throw;
	}
	path tmpOriginal = FilePath / ".bak";
	sys::rename(FilePath, tmpOriginal);
	sys::rename(tmpPath, FilePath);
	sys::remove(tmpOriginal);
	Open(FilePath);
}


}}} // Ext::DB::KV::


