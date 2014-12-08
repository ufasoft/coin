#include <el/ext.h>

#include "param.h"

#include <sqlite3.h>

#include "eng.h"
#include "coin-msg.h"

#include "backend-sqlite.h"

namespace Coin {



Block SqliteBlockChainDb::LoadBlock(DbDataReader& sr) {
	CoinEng& eng = Eng();

	BlockHashValue hash = sr.GetBytes(1);
	Block r;
	r.m_pimpl->Height = sr.GetInt32(0);

	CMemReadStream ms(sr.GetBytes(2));
	r.m_pimpl->Read(DbReader(ms, &eng));
	r.m_pimpl->m_hash = hash;

	r.m_pimpl->m_txHashesOutNums = Coin::TxHashesOutNums(sr.GetBytes(3));

	if (r.Height > 0)
		r.m_pimpl->PrevBlockHash = BlockHashValue(m_cmdFindBlockByOrd.Bind(1, r.Height-1).ExecuteVector().GetBytes(1));

//!!!	ASSERT(Hash(r) == hash);
	
	return r;
}

void SqliteBlockChainDb::LoadFromDb(Tx& tx, DbDataReader& sr) {
	CoinEng& eng = Eng();
	tx.Height = sr.GetInt32(2);
	CMemReadStream stm(sr.GetBytes(3));
	DbReader rd(stm, &eng);
	rd >> tx;
}


/*!!!
void InsertOrUpdateTx(const HashValue160& hash160, const HashValue& hashTx) {
	Int64 id = Int64(CIdPk(hash160));

	CPubToTxHashes::iterator it = m_lruPubToTxHashes.find(id);
	if (it != m_lruPubToTxHashes.end()) {
		TxHashes& txHashes = it->second;
		txHashes.Vec.push_back(*(Int64*)hashTx.data());
		txHashes.Modified = true;
	} else {
		DbDataReader dr = CmdFindByHash160.Bind(1, id).ExecuteReader();
		if (dr.Read()) {
			ConstBuf cbuf = dr.GetBytes(0);
			TxHashes txHashes(cbuf.Size/sizeof(Int64)+1);
			memcpy((byte*)memcpy(&txHashes.Vec[0], cbuf.P, cbuf.Size) + cbuf.Size, hashTx.data(), sizeof(Int64));
			m_lruPubToTxHashes.insert(make_pair(id, txHashes));
		} else {
			CmdInsertPkTxes
				.Bind(1, ConstBuf(hashTx.data(), 8))
				.Bind(2, id)
				.Bind(3, hash160).ExecuteNonQuery();
		}
	}
}
*/

void SqliteBlockChainDb::InsertPubkeyToTxes(const Tx& tx) {
	Throw(E_NOTIMPL);
}

vector<Int64> SqliteBlockChainDb::GetTxesByPubKey(const HashValue160& pubkey) {
	vector<Int64> r;
	EXT_LOCK (MtxSqlite) {
		SqliteCommand cmd("SELECT txhashes FROM pubkeys WHERE id=?", m_db);
		DbDataReader dr = cmd.Bind(1, pubkey).ExecuteReader();
		if (dr.Read()) {
			CMemReadStream stm(dr.GetBytes(0));
			BinaryReader rd(stm);
			for (int i=0; !stm.Eof(); ++i)
				r.push_back(rd.ReadInt64());
		}
	}
	return r;
}

ptr<IBlockChainDb> CreateSqliteBlockChainDb() {
	return new SqliteBlockChainDb;
}



} // Coin::

