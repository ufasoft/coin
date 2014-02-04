#include <el/ext.h>

#include "param.h"

#include <sqlite3.h>

#include "eng.h"
#include "coin-msg.h"


namespace Coin {

class SqliteBlockChainDb : public IBlockChainDb {
public:
	recursive_mutex MtxSqlite;			// nested FindBlock() in NameCoin
	SqliteConnection m_db;

	class SavepointTransactionable : public ITransactionable {
	public:
		SqliteBlockChainDb& m_parent;

		SavepointTransactionable(SqliteBlockChainDb& parent)
			:	m_parent(parent)
		{}

		void BeginTransaction() override {
			m_parent.MtxSqlite.lock();
			m_parent.m_cmdSavepoint.ExecuteNonQuery();
		}

		void Commit() override {
			m_parent.m_cmdReleaseSavepoint.ExecuteNonQuery();
			m_parent.MtxSqlite.unlock();
		}

		void Rollback() override {
			SqliteCommand("ROLLBACK TO block", m_parent.m_db).ExecuteNonQuery();
			m_parent.MtxSqlite.unlock();
		}
	} m_savepoint;

	SqliteCommand
		m_cmdBeginExclusive,
		m_cmdCommit,
		m_cmdSavepoint,
		m_cmdReleaseSavepoint,
		m_cmdFindBlock,
		m_cmdFindBlockByOrd,
		m_cmdFindTx,
//!!!R		m_cmdTxesOfBlock,
		m_cmdTxBlockordIdxToHash,
		m_cmdTxHashToBlockordIdx,
		m_cmdTxFindCoins, m_cmdTxUpdateCoins,
		m_cmdTxFindIns,
		CmdPubkeyIdToData, CmdPubkeyInsert, CmdPubkeyUpdate,
		m_cmdInsertBlock,
		m_cmdInsertTx;

	SqliteBlockChainDb()
		:	m_savepoint(_self)
		,	m_cmdBeginExclusive("BEGIN EXCLUSIVE"																			, m_db)
		,	m_cmdCommit("COMMIT"																							, m_db)
		,	m_cmdSavepoint("SAVEPOINT block"																				, m_db)
		,	m_cmdReleaseSavepoint("RELEASE SAVEPOINT block"																	, m_db)
		,	m_cmdFindBlock("SELECT id, hash, data, txhashes FROM blocks WHERE hash=?"										, m_db)
		,	m_cmdFindBlockByOrd("SELECT id, hash, data, txhashes FROM blocks WHERE id=?"									, m_db)
		,	m_cmdFindTx("SELECT id, NULL, blockid, data FROM txes WHERE id=?"												, m_db)
		,	m_cmdInsertTx("INSERT INTO txes (id, blockid, ins, coins, data) VALUES(?, ?, ?, ?, ?)"							, m_db)
		,	m_cmdTxHashToBlockordIdx("SELECT blockid, txhashes FROM txes JOIN blocks ON blockid=blocks.id WHERE txes.id=?"	, m_db)
		,	m_cmdTxFindCoins("SELECT coins FROM txes WHERE id=?"															, m_db)
		,	m_cmdTxFindIns("SELECT ins FROM txes WHERE id=?"																, m_db)
		,	m_cmdTxUpdateCoins("UPDATE txes SET coins=? WHERE id=?"															, m_db)
		,	CmdPubkeyIdToData("SELECT data FROM pubkeys WHERE id=?"															, m_db)
		,	CmdPubkeyInsert("INSERT INTO pubkeys (id, data) VALUES(?, ?)"													, m_db)
		,	CmdPubkeyUpdate("UPDATE pubkeys SET data=? WHERE id=?"															, m_db)
		,	m_cmdInsertBlock("INSERT INTO blocks (hash, id, data, txhashes) VALUES(?, ?, ?, ?)"								, m_db)
		,	m_cmdTxBlockordIdxToHash("SELECT txhashes FROM blocks WHERE id=?"												, m_db)
	{
		IsSlow = true;
	}

	void *GetDbObject() override { return &m_db; }
	ITransactionable& GetSavepointObject() override { return m_savepoint; }

	void SetPragmas() {
#if !UCFG_WCE
		m_db.ExecuteNonQuery(EXT_STR("PRAGMA cache_size=-" << 256*1024));		// in -kibibytes
#endif

		m_db.ExecuteNonQuery("PRAGMA locking_mode=EXCLUSIVE");
	}
	
	bool Create(RCString path) override {
		m_db.Create(path);

		m_db.ExecuteNonQuery("PRAGMA page_size=8192");

		m_db.ExecuteNonQuery(
			"CREATE TABLE blocks (id INTEGER PRIMARY KEY"
								", hash UNIQUE"
								", data"
								", txhashes);"
			"CREATE TABLE txes (id INTEGER PRIMARY KEY"
								", blockid INTEGER"
								", data"
								", ins"
								", coins);"
			);
		if (Eng().Mode == EngMode::BlockExplorer)
			m_db.ExecuteNonQuery("CREATE TABLE pubkeys (id INTEGER PRIMARY KEY"
														", data"
														", txhashes);");
		else
			m_db.ExecuteNonQuery("CREATE TABLE pubkeys (id INTEGER PRIMARY KEY"
														", data);");

		SetUserVersion(m_db);
		SetPragmas();
		return true;
	}
	
	bool Open(RCString path) override {
		m_db.Open(path, FileAccess::ReadWrite, FileShare::None);
		SetPragmas();
		return true;
	}

	void Close() override {
		m_db.Close();
	}

	Version CheckUserVersion() override {
		return Coin::CheckUserVersion(m_db);
	}

	void UpgradeTo(const Version& ver) override {
		CoinEng& eng = Eng();

		Version dbver = CheckUserVersion();
		if (dbver < ver) {
			TRC(0, "Upgrading Database " << eng.GetDbFilePath() << " from version " << dbver << " to " << ver);
		
			TransactionScope dbtx(m_db);
			eng.UpgradeDb(ver);
			SetUserVersion(m_db, ver);
		}
	}

	void TryUpgradeDb(const Version& verApp) override {
		CoinEng& eng = Eng();

		if (CheckUserVersion() != verApp) {
	//!!!R		UpgradeTo(NAMECOIN_VER_DOMAINS_TABLE);
	//!!!R		UpgradeTo(VER_DUPLICATED_TX);
			if (CheckUserVersion() < VER_KEYS32) {
				Recreate();
				return;
			}
	//		UpgradeTo(VER_KEYS32);
		}
	}

	bool HaveBlock(const HashValue& hash) override {
		EXT_LOCK (MtxSqlite) {
			return m_cmdFindBlock.Bind(1, ReducedBlockHash(hash)).ExecuteReader().Read();
		}
	}

	Block FindBlock(const HashValue& hash) override {
		EXT_LOCK (MtxSqlite) {
			DbDataReader dr = m_cmdFindBlock.Bind(1, ReducedBlockHash(hash)).ExecuteReader();
			return dr.Read() ? LoadBlock(dr) : Block(nullptr);
		}
	}

	Block FindBlock(int height) override {
		EXT_LOCK (MtxSqlite) {
			DbDataReader dr = m_cmdFindBlockByOrd.Bind(1, height).ExecuteVector();
			return LoadBlock(dr);
		}
	}

	int GetMaxHeight() override {
		EXT_LOCK (MtxSqlite) {
			if (SqliteCommand("SELECT * FROM blocks", m_db).ExecuteReader().Read())
				return (int)SqliteCommand("SELECT MAX(id) FROM blocks", m_db).ExecuteInt64Scalar();
			else
				return -1;
		}
	}

	TxHashesOutNums GetTxHashesOutNums(int height) override {
//!!!		ASSERT(!MtxSqlite.try_lock());
		return TxHashesOutNums(m_cmdTxBlockordIdxToHash.Bind(1, height).ExecuteVector().GetBytes(0));
	}

	void InsertBlock(int height, const HashValue& hash, const ConstBuf& data, const ConstBuf& txData) override {
		m_cmdInsertBlock
			.Bind(1, ReducedBlockHash(hash))
			.Bind(2, Int64(height))
			.Bind(3, data)
			.Bind(4, txData)
			.ExecuteNonQuery();
	}

	void DeleteBlock(int height, const vector<Int64>& txids) override {
		CoinEng& eng = Eng();

		ostringstream os;
		os << "DELETE FROM txes WHERE id IN (";
		for (int i=0; i<txids.size(); ++i)
			os << (i ? ", " : "") << txids[i];
		os << ")";
		//!!!R		SqliteCommand(EXT_STR("DELETE FROM txes WHERE blockord=" << height), m_db).ExecuteNonQuery();
		SqliteCommand(os.str(), m_db).ExecuteNonQuery();
		SqliteCommand(EXT_STR("DELETE FROM blocks WHERE id=" << height), m_db).ExecuteNonQuery();
	}

	bool FindTxById(const ConstBuf& txid, Tx *ptx) override {
		EXT_LOCK (MtxSqlite) {
			DbDataReader dr = m_cmdFindTx.Bind(1, *(Int64*)txid.P).ExecuteReader();
			if (!dr.Read())
				return false;
			if (ptx) {
				ptx->EnsureCreate();
				ptx->SetReducedHash(txid);
				LoadFromDb(*ptx, dr);
			}
		}
		return true;
	}

	bool FindTx(const HashValue& hash, Tx *ptx) override {
		EXT_LOCK (MtxSqlite) {
			DbDataReader dr = m_cmdFindTx.Bind(1, ReducedHashValue(hash)).ExecuteReader();
			if (!dr.Read())
				return false;
			if (ptx) {
				ptx->EnsureCreate();
				ptx->SetHash(hash);
				LoadFromDb(*ptx, dr);
			}
		}
		return true;
	}

	void ReadTxIns(const HashValue& hash, const TxObj& txObj) override {
		EXT_LOCK (MtxSqlite) {
			txObj.ReadTxIns(DbReader(CMemReadStream(m_cmdTxFindIns.Bind(1, ReducedHashValue(hash)).ExecuteVector().GetBytes(0)), &Eng()));
		}
	}

	pair<int, int> FindPrevTxCoords(DbWriter& wr, int height, const HashValue& hash) override {
//!!!		ASSERT(!MtxSqlite.try_lock());

		DbDataReader dr = m_cmdTxHashToBlockordIdx
								.Bind(1, ReducedHashValue(hash))
								.ExecuteVector();
		int heightOut = dr.GetInt32(0);
		wr.Write7BitEncoded(height-heightOut+1);
		pair<int, int> pp = TxHashesOutNums(dr.GetBytes(1)).StartingTxOutIdx(hash);
		if (pp.second < 0)
			Throw(E_COIN_InconsistentDatabase);
		return pp;
	}

	void InsertPubkeyToTxes(const Tx& tx);
	vector<Int64> GetTxesByPubKey(const HashValue160& pubkey) override;

	void InsertTx(const Tx& tx, const TxHashesOutNums& hashesOutNums, const HashValue& txHash, int height, const ConstBuf& txIns, const ConstBuf& spend, const ConstBuf& data) override {
		CoinEng& eng = Eng();

		m_cmdInsertTx
			.Bind(1, ReducedHashValue(txHash))
			.Bind(2, (Int64)height)
			.Bind(3, txIns)
			.Bind(4, spend);

		m_cmdInsertTx.Bind(5, data);
		try {
			DBG_LOCAL_IGNORE_NAME(MAKE_HRESULT(SEVERITY_ERROR, FACILITY_SQLITE, SQLITE_CONSTRAINT_PRIMARYKEY), ignSQLITE_CONSTRAINT_PRIMARYKEY);
					
			m_cmdInsertTx.ExecuteNonQuery();
		} catch (const SqliteException&) {
			TRC(1, "Duplicated Transaction: " << txHash);

			if (height >= eng.ChainParams.CheckDupTxHeight && ContainsInLinear(GetCoinsByTxHash(txHash), true))
				Throw(E_COIN_DupNonSpentTx);						

			SqliteCommand("UPDATE txes SET coins=? WHERE id=?", m_db)
				.Bind(1, spend)
				.Bind(2, ReducedHashValue(txHash))
				.ExecuteNonQuery();
		}
		if (eng.Mode == EngMode::BlockExplorer)
			InsertPubkeyToTxes(tx);
	}

	vector<bool> GetCoinsByTxHash(const HashValue& hash) override {
		DbDataReader dr = m_cmdTxFindCoins.Bind(1, ReducedHashValue(hash)).ExecuteVector();
		vector<bool> vec;
		if (!dr.IsDBNull(0)) {
			CMemReadStream stm(dr.GetBytes(0));
			for (int v; (v=stm.ReadByte())!=-1;) {
				for (int i=0; i<8; ++i)
					vec.push_back(v & (1 << i));
			}
		}
		return vec;
	}

	void SaveCoinsByTxHash(const HashValue& hash, const vector<bool>& vec) override {
		if (find(vec.begin(), vec.end(), true) != vec.end())
			m_cmdTxUpdateCoins.Bind(1, CoinEng::SpendVectorToBlob(vec));
		else
			m_cmdTxUpdateCoins.Bind(1, nullptr);
		m_cmdTxUpdateCoins.Bind(2, ReducedHashValue(hash))
			.ExecuteNonQuery();
	}

	Blob FindPubkey(Int64 id) override {
		EXT_LOCK (MtxSqlite) {
			DbDataReader dr = CmdPubkeyIdToData.Bind(1, id).ExecuteReader();
			return dr.Read() ? Blob(dr.GetBytes(0)) : Blob(nullptr);
		}
	}

	void InsertPubkey(Int64 id, const ConstBuf& pk) override {
//!!!		ASSERT(!MtxSqlite.try_lock());
		CmdPubkeyInsert.Bind(1, id).Bind(2, pk).ExecuteNonQuery();
	}

	void UpdatePubkey(Int64 id, const ConstBuf& pk) override {
//!!!		ASSERT(!MtxSqlite.try_lock());
		CmdPubkeyUpdate.Bind(1, pk).Bind(2, id).ExecuteNonQuery();
	}

	void BeginEngTransaction() override {
	}

	void BeginTransaction() override {
		EXT_LOCKED(MtxSqlite, m_cmdBeginExclusive.ExecuteNonQuery());
	//	EXT_LOCKED(MtxSqlite, m_db.BeginTransaction());
	 }
	
	void Commit() override { EXT_LOCKED(MtxSqlite, m_cmdCommit.ExecuteNonQuery()); }

	void Rollback() override { EXT_LOCKED(MtxSqlite, m_db.Rollback()); }
	
	void SetProgressHandler(int(*pfn)(void*), void *p = 0, int n = 1) override {
		EXT_LOCK (MtxSqlite) {
			m_db.SetProgressHandler(pfn, p, n);
		}
	}

	void Vacuum() override {
		EXT_LOCK (MtxSqlite) {
			Eng().InWalJournalMode = false;
			m_db.ExecuteNonQuery("PRAGMA journal_mode=delete");
			m_db.ExecuteNonQuery("PRAGMA synchronous=1");
			m_db.ExecuteNonQuery("VACUUM");
		}
	}

	void BeforeEnsureTransactionStarted() override {
		CoinEng& eng = Eng();
		EXT_LOCK (MtxSqlite) {
			if (eng.BestBlock() && !eng.JournalModeDelete) {
				TimeSpan span = DateTime::UtcNow()-eng.BestBlock().Timestamp;
				if (!eng.InWalJournalMode && span > TimeSpan::FromDays(7)) {
					eng.InWalJournalMode = true;
					m_db.ExecuteNonQuery("PRAGMA journal_mode=wal");
					m_db.ExecuteNonQuery("PRAGMA synchronous=off");
					m_db.ExecuteNonQuery("PRAGMA temp_store=2");
				} else if (eng.InWalJournalMode && span < TimeSpan::FromDays(1)) {
					eng.InWalJournalMode = false;
					m_db.ExecuteNonQuery("PRAGMA journal_mode=delete");
					m_db.ExecuteNonQuery("PRAGMA synchronous=1");
				}

				if (eng.InWalJournalMode && !(++m_nCheckpont & 3))
					m_db.Checkpoint(SQLITE_CHECKPOINT_RESTART);
			}
		}
	}

	vector<Block> GetBlocks(const LocatorHashes& locators, const HashValue& hashStop) override {
		vector<Block> r;
		int idx = locators.FindIndexInMainChain();
		EXT_LOCK (MtxSqlite) {
			SqliteCommand cmd(EXT_STR("SELECT id, hash, data, txhashes FROM blocks WHERE id>" << idx << " ORDER BY id LIMIT " << 2000 + locators.DistanceBack), m_db);
			for (DbDataReader dr = cmd.ExecuteReader(); dr.Read();) {
				Block block = LoadBlock(dr);
				r.push_back(block);
				if (Hash(block) == hashStop)
					break;
			}
		}
		return r;
	}
private:	
	Block LoadBlock(DbDataReader& sr);
	void LoadFromDb(Tx& tx, DbDataReader& sr);
};


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

#if UCFG_COIN_COINCHAIN_BACKEND == COIN_BACKEND_SQLITE

ptr<IBlockChainDb> CreateBlockChainDb() {
	return CreateSqliteBlockChainDb();
}

#endif


} // Coin::

