/*######   Copyright (c) 2013 Ufasoft  http://ufasoft.com  mailto:support@ufasoft.com,  Sergey Pavlov  mailto:dev@ufasoft.com ####
#                                                                                                                                     #
# 		See LICENSE for licensing information                                                                                         #
#####################################################################################################################################*/

#include <el/ext.h>

#include "param.h"

#include <sqlite3.h>

#include "eng.h"
#include "coin-msg.h"

#include "backend-sqlite.h"

namespace Coin {


vector<BlockHeader> SqliteBlockChainDb::GetBlockHeaders(const LocatorHashes& locators, const HashValue& hashStop) {
	vector<Block> r;
	int idx = locators.FindIndexInMainChain();
	EXT_LOCK(MtxSqlite) {
		SqliteCommand cmd(EXT_STR("SELECT id, hash, data, txhashes FROM blocks WHERE id>" << idx << " ORDER BY id LIMIT " << MAX_HEADERS_RESULTS + locators.DistanceBack), m_db);
		for (DbDataReader dr = cmd.ExecuteReader(); dr.Read();) {
			BlockHeader header = LoadBlock(dr);
			r.push_back(header);
			if (Hash(header) == hashStop)
				break;
		}
	}
	return r;
}

Block SqliteBlockChainDb::LoadBlock(DbDataReader& sr) {
	CoinEng& eng = Eng();

	BlockHashValue hash = sr.GetBytes(1);
	Block r;
	r->Height = sr.GetInt32(0);

	CMemReadStream ms(sr.GetBytes(2));
	r->Read(DbReader(ms, &eng));
	r->m_hash = hash;

	r->m_txHashesOutNums = Coin::TxHashesOutNums(sr.GetBytes(3));

	if (r.Height > 0)
		r->PrevBlockHash = BlockHashValue(m_cmdFindBlockByOrd.Bind(1, r.Height-1).ExecuteVector().GetBytes(1));

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
	int64_t id = int64_t(CIdPk(hash160));

	CPubToTxHashes::iterator it = m_lruPubToTxHashes.find(id);
	if (it != m_lruPubToTxHashes.end()) {
		TxHashes& txHashes = it->second;
		txHashes.Vec.push_back(*(int64_t*)hashTx.data());
		txHashes.Modified = true;
	} else {
		DbDataReader dr = CmdFindByHash160.Bind(1, id).ExecuteReader();
		if (dr.Read()) {
			ConstBuf cbuf = dr.GetBytes(0);
			TxHashes txHashes(cbuf.Size/sizeof(int64_t)+1);
			memcpy((uint8_t*)memcpy(&txHashes.Vec[0], cbuf.P, cbuf.Size) + cbuf.Size, hashTx.data(), sizeof(int64_t));
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

vector<int64_t> SqliteBlockChainDb::GetTxesByPubKey(const HashValue160& pubkey) {
	vector<int64_t> r;
	EXT_LOCK (MtxSqlite) {
		SqliteCommand cmd("SELECT txhashes FROM pubkeys WHERE id=?", m_db);
		DbDataReader dr = cmd.Bind(1, pubkey).ExecuteReader();
		if (dr.Read()) {
			CMemReadStream stm(dr.GetBytes(0));
			BinaryReader rd(stm);
			for (int i = 0; !stm.Eof(); ++i)
				r.push_back(rd.ReadInt64());
		}
	}
	return r;
}

ptr<IBlockChainDb> CreateSqliteBlockChainDb() {
	return new SqliteBlockChainDb;
}



} // Coin::

