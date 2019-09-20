/*######   Copyright (c) 2013-2015 Ufasoft  http://ufasoft.com  mailto:support@ufasoft.com,  Sergey Pavlov  mailto:dev@ufasoft.com ####
#                                                                                                                                     #
# 		See LICENSE for licensing information                                                                                         #
#####################################################################################################################################*/

#pragma once

#include <sqlite3.h>


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
		: m_savepoint(_self)
		, m_cmdBeginExclusive("BEGIN EXCLUSIVE"																			, m_db)
		, m_cmdCommit("COMMIT"																							, m_db)
		, m_cmdSavepoint("SAVEPOINT block"																				, m_db)
		, m_cmdReleaseSavepoint("RELEASE SAVEPOINT block"																	, m_db)
		, m_cmdFindBlock("SELECT id, hash, data, txhashes FROM blocks WHERE hash=?"										, m_db)
		, m_cmdFindBlockByOrd("SELECT id, hash, data, txhashes FROM blocks WHERE id=?"									, m_db)
		, m_cmdFindTx("SELECT id, NULL, blockid, data FROM txes WHERE id=?"												, m_db)
		, m_cmdInsertTx("INSERT INTO txes (id, blockid, ins, coins, data) VALUES(?, ?, ?, ?, ?)"							, m_db)
		, m_cmdTxHashToBlockordIdx("SELECT blockid, txhashes FROM txes JOIN blocks ON blockid=blocks.id WHERE txes.id=?"	, m_db)
		, m_cmdTxFindCoins("SELECT coins FROM txes WHERE id=?"															, m_db)
		, m_cmdTxFindIns("SELECT ins FROM txes WHERE id=?"																, m_db)
		, m_cmdTxUpdateCoins("UPDATE txes SET coins=? WHERE id=?"															, m_db)
		, CmdPubkeyIdToData("SELECT data FROM pubkeys WHERE id=?"															, m_db)
		, CmdPubkeyInsert("INSERT INTO pubkeys (id, data) VALUES(?, ?)"													, m_db)
		, CmdPubkeyUpdate("UPDATE pubkeys SET data=? WHERE id=?"															, m_db)
		, m_cmdInsertBlock("INSERT INTO blocks (hash, id, data, txhashes) VALUES(?, ?, ?, ?)"								, m_db)
		, m_cmdTxBlockordIdxToHash("SELECT txhashes FROM blocks WHERE id=?"												, m_db)
	{
	}

	void *GetDbObject() override { return &m_db; }
	ITransactionable& GetSavepointObject() override { return m_savepoint; }

	void SetPragmas() {
#if !UCFG_WCE
		m_db.ExecuteNonQuery(EXT_STR("PRAGMA cache_size=-" << 256*1024));		// in -kibibytes
#endif

		m_db.ExecuteNonQuery("PRAGMA locking_mode=EXCLUSIVE");
	}

	bool Create(const path& p) override {
		m_db.Create(p);

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

	bool Open(const path& p) override {
		m_db.Open(p, FileAccess::ReadWrite, FileShare::None);
		SetPragmas();
		return true;
	}

	void Close(bool bAsync) override {
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

	void InsertBlock(const Block& block) override {
		m_cmdInsertBlock
			.Bind(1, ReducedBlockHash(Hash(block)))
			.Bind(2, int64_t(block.Height))
			.Bind(3, data)
			.Bind(4, txData)
			.ExecuteNonQuery();
	}

	void DeleteBlock(int height, const vector<int64_t>& txids) override {
		CoinEng& eng = Eng();

		ostringstream os;
		os << "DELETE FROM txes WHERE id IN (";
		for (size_t i=0; i<txids.size(); ++i)
			os << (i ? ", " : "") << txids[i];
		os << ")";
		SqliteCommand(os.str(), m_db).ExecuteNonQuery();
		SqliteCommand(EXT_STR("DELETE FROM blocks WHERE id=" << height), m_db).ExecuteNonQuery();
	}

	bool FindTxByHash(const HashValue& hashTx, Tx *ptx) override {
		EXT_LOCK (MtxSqlite) {
			DbDataReader dr = m_cmdFindTx.Bind(1, *(int64_t*)hashTx.data()).ExecuteReader();
			if (!dr.Read())
				return false;
			if (ptx) {
				ptx->EnsureCreate();
				ptx->SetReducedHash(Span(hashTx.data(), 8));
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
			Throw(CoinErr::InconsistentDatabase);
		return pp;
	}

	void InsertPubkeyToTxes(const Tx& tx);
	vector<int64_t> GetTxesByPubKey(const HashValue160& pubkey) override;

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

	void InsertTx(const Tx& tx, const TxHashesOutNums& hashesOutNums, const HashValue& txHash, int height, RCSpan txIns, RCSpan spend, RCSpan data, uint32_t txOffset) override {
		CoinEng& eng = Eng();

		m_cmdInsertTx
			.Bind(1, ReducedHashValue(txHash))
			.Bind(2, (int64_t)height)
			.Bind(3, txIns)
			.Bind(4, spend);

		m_cmdInsertTx.Bind(5, data);
		try {
			DBG_LOCAL_IGNORE(MAKE_HRESULT(SEVERITY_ERROR, FACILITY_SQLITE, SQLITE_CONSTRAINT_PRIMARYKEY));

			m_cmdInsertTx.ExecuteNonQuery();
		} catch (const SqliteException&) {
			TRC(1, "Duplicated Transaction: " << txHash);

			if (height >= eng.ChainParams.CheckDupTxHeight && ContainsInLinear(GetCoinsByTxHash(txHash), true))
				Throw(CoinErr::DupNonSpentTx);

			SqliteCommand("UPDATE txes SET coins=? WHERE id=?", m_db)
				.Bind(1, spend)
				.Bind(2, ReducedHashValue(txHash))
				.ExecuteNonQuery();
		}
		if (eng.Mode == EngMode::BlockExplorer)
			InsertPubkeyToTxes(tx);
	}

	void SaveCoinsByTxHash(const HashValue& hash, const vector<bool>& vec) override {
		if (find(vec.begin(), vec.end(), true) != vec.end())
			m_cmdTxUpdateCoins.Bind(1, CoinEng::SpendVectorToBlob(vec));
		else
			m_cmdTxUpdateCoins.Bind(1, nullptr);
		m_cmdTxUpdateCoins.Bind(2, ReducedHashValue(hash))
			.ExecuteNonQuery();
	}

	Blob FindPubkey(int64_t id) override {
		EXT_LOCK (MtxSqlite) {
			DbDataReader dr = CmdPubkeyIdToData.Bind(1, id).ExecuteReader();
			return dr.Read() ? Blob(dr.GetBytes(0)) : Blob(nullptr);
		}
	}

	void InsertPubkey(int64_t id, RCSpan pk) override {
//!!!		ASSERT(!MtxSqlite.try_lock());
		CmdPubkeyInsert.Bind(1, id).Bind(2, pk).ExecuteNonQuery();
	}

	void UpdatePubkey(int64_t id, RCSpan pk) override {
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
				TimeSpan span = Clock::now() - eng.BestBlock().Timestamp;
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

	vector<BlockHeader> GetBlockHeaders(const LocatorHashes& locators, const HashValue& hashStop) override;
private:
	Block LoadBlock(DbDataReader& sr);
	void LoadFromDb(Tx& tx, DbDataReader& sr);
};


} // Coin::



