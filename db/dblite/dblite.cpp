#include <el/ext.h>

#include "dblite.h"
#include "b-tree.h"

namespace Ext { namespace DB { namespace KV {


DbCursor::DbCursor(DbTransaction& tx, DbTable& table)
	:	Path(4)
{
	Path.resize(0);
	DbTransaction::CTables::iterator it = tx.Tables.find(table.Name);
	if (it != tx.Tables.end())
		Tree = &it->second;
	else {
		BTree btree(tx);
		btree.Name = table.Name;
		if (&table == &DbTable::Main)
			btree.Root = tx.MainTableRoot;
		else {
			DbCursor cMain(tx, DbTable::Main);
			ConstBuf k(table.Name.c_str(), strlen(table.Name.c_str()));
			if (!cMain.SeekToKey(k))
				Throw(E_FAIL);
			TableData& td = *(TableData*)cMain.get_Data().P;
			btree.SetKeySize(td.KeySize);
			if (UInt32 pgno = letoh(td.RootPgNo))
				btree.Root = tx.OpenPage(pgno);
			else if (tx.ReadOnly) {
//				TRC(0, "No root for " << table.Name);
			}
		}
		Tree = &tx.Tables.insert(make_pair(table.Name, btree)).first->second;
	}
	Tree->Cursors.push_back(_self);
}

void DbTable::CheckKeyArg(const ConstBuf& k) {
	if (k.Size == 0 || k.Size > MAX_KEY_SIZE)
		Throw(E_INVALIDARG);
}

void DbTable::Open(DbTransaction& tx, bool bCreate) {
	CKVStorageKeeper keeper(&tx.Storage);

	DbCursor cM(tx, DbTable::Main);
	ConstBuf k(Name.c_str(), strlen(Name.c_str()));
	if (!cM.SeekToKey(k)) {
		if (!bCreate)
			Throw(E_FAIL);
		TableData td;
		td.RootPgNo = 0;
		td.KeySize = KeySize;
		DbTable::Main.Put(tx, k, ConstBuf(&td, sizeof td));
	//	cM.SeekToKey(k);		//!!!?
	}
}

void DbTable::Drop(DbTransaction& tx) {
	if (tx.ReadOnly)
		Throw(E_ACCESSDENIED);
	DbCursor(tx, _self).Drop();
	ConstBuf k(Name.c_str(), strlen(Name.c_str()));
	DbTable::Main.Delete(tx, k);
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
	String tmpPath = Path::GetTempFileName(Path::GetDirectoryName(FilePath), "tmp").first;
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

			for (DbCursor ct(txS, DbTable::Main); ct.SeekToNext();) {
				String tableName = Encoding::UTF8.GetChars(ct.Key);
				const TableData& td = *(const TableData*)ct.get_Data().P;

				DbTable tS(tableName), tD(tableName);
				tD.KeySize = td.KeySize;
				tD.Open(txD, true);
				int n = 0;
				Int64 bytes = 0;
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
		File::Delete(tmpPath);
		throw;
	}
	String tmpOriginal = FilePath+".bak";
	File::Move(FilePath, tmpOriginal);
	File::Move(tmpPath, FilePath);
	File::Delete(tmpOriginal);
	Open(FilePath);
}


}}} // Ext::DB::KV::


