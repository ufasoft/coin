/*######     Copyright (c) 1997-2013 Ufasoft  http://ufasoft.com  mailto:support@ufasoft.com,  Sergey Pavlov  mailto:dev@ufasoft.com #######################################
#                                                                                                                                                                          #
# This program is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation;  #
# either version 3, or (at your option) any later version. This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the      #
# implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details. You should have received a copy of the GNU #
# General Public License along with this program; If not, see <http://www.gnu.org/licenses/>                                                                               #
##########################################################################################################################################################################*/

#include <el/ext.h>

#include "param.h"

#if UCFG_COIN_COINCHAIN_BACKEND == COIN_BACKEND_DBLITE
#	include <db/dblite/dblite.h>
	using namespace Ext::DB::KV;
#endif


#if UCFG_COIN_COINCHAIN_BACKEND == COIN_BACKEND_DBLITE

using namespace Ext::DB;

#include "eng.h"
#include "coin-msg.h"


namespace Coin {

const size_t BLOCKID_SIZE = 3;
const size_t TXID_SIZE = 6;
const size_t PUBKEYID_SIZE = 5;
const size_t PUBKEYTOTXES_ID_SIZE = 8;

class MdbBlockChainDb;

class DbTxRef : NonCopiable {
public:
	DbTxRef(DbStorage& env);

	operator DbTransaction&() { return *m_p; }
private:
	DbTransaction *m_p;

	auto_ptr<DbTransaction> m_pMdbTx;
};

class SavepointTransaction;

class BlockKey {
public:
	BlockKey(UInt32 h)
		:	m_beH(htobe(h))
	{}

	BlockKey(const ConstBuf& cbuf)
		:	m_beH(0)
	{
		memcpy((byte*)&m_beH + 1, cbuf.P, BLOCKID_SIZE);
	}

	operator UInt32() const { return betoh(m_beH); }
	operator ConstBuf() const { return ConstBuf((const byte*)&m_beH + 1, BLOCKID_SIZE); }
private:
	UInt32 m_beH;
};


class MdbBlockChainDb : public IBlockChainDb {
public:
	mutex MtxDb;
	DbStorage m_db;
	DbTable m_tableBlocks, m_tableHashToBlock, m_tableTxes, m_tablePubkeys, m_tablePubkeyToTxes;

	auto_ptr<DbTransaction> m_dbt;

	MdbBlockChainDb()
		:	m_tableBlocks("blocks")
		,	m_tableHashToBlock("hash_blocks")
		,	m_tableTxes("txes")
		,	m_tablePubkeys("pubkeys")
		,	m_tablePubkeyToTxes("pubkey_txes")
	{
		DefaultFileExt = ".udb";

		m_tableBlocks.KeySize = BLOCKID_SIZE;
		m_tableTxes.KeySize = TXID_SIZE;
		m_tablePubkeys.KeySize = PUBKEYID_SIZE;
		m_tablePubkeyToTxes.KeySize = PUBKEYTOTXES_ID_SIZE;

		m_db.Durability = false;
		m_db.CheckpointPeriod = TimeSpan::FromMinutes(5);
//		m_db.ProtectPages = false;

		m_db.AppName = VER_PRODUCTNAME_STR;
		const int ar[] = { VER_PRODUCTVERSION };
		m_db.UserVersion = Version(ar[0], ar[1]);
	}

	void *GetDbObject() override {
		return &m_db;
	}

	ITransactionable& GetSavepointObject() override;

	bool Create(RCString path) override;
	bool Open(RCString path) override;

	void Close() override {
		m_dbt.reset();

		m_tableBlocks.Close();
		m_tableHashToBlock.Close();
		m_tableTxes.Close();
		m_tablePubkeys.Close();
		m_tablePubkeyToTxes.Close();
		m_db.Close();
	}

	Version CheckUserVersion() override {
    	Version ver(VER_PRODUCTVERSION),
				dver = m_db.UserVersion;
    	if (dver > ver && (dver.Major!=ver.Major || dver.Minor/10 != ver.Minor/10)) {					// Versions [mj.x0 .. mj.x9] are DB-compatible
    		m_db.Close();
    		VersionExc exc;
    		exc.Version = dver;
    		throw exc;
    	}
    	return dver;
	}

	void UpgradeTo(const Version& ver) override {
		CoinEng& eng = Eng();

		Version dbver = CheckUserVersion();
		if (dbver < ver) {
			TRC(0, "Upgrading Database " << eng.GetDbFilePath() << " from version " << dbver << " to " << ver);

			DbTransaction dbtx(m_db);
			eng.UpgradeDb(ver);
			m_db.SetUserVersion(ver);
		}
	}

	bool HaveBlock(const HashValue& hash) override {
		DbTxRef dbt(m_db);
		return DbCursor(dbt, m_tableHashToBlock).Get(ReducedBlockHash(hash));
	}

	Block FindBlock(const HashValue& hash) override {
		Block r(nullptr);
		DbTxRef dbt(m_db);
		DbCursor c(dbt, m_tableHashToBlock);
		if (c.Get(ReducedBlockHash(hash))) {
			int height = (UInt32)BlockKey(c.Data);
			DbCursor c1(dbt, m_tableBlocks);
			if (!c1.Get(c.Data)) {
#ifdef X_DEBUG//!!!D
				DbCursor c2(dbt, m_tableBlocks);
				ConstBuf d = c.Data;
				c2.Get(d);
#endif
				Throw(E_COIN_InconsistentDatabase);
			}
			r = LoadBlock(dbt, height, c1.Data);
		}
		return r;
	}

	Block FindBlock(int height) override {
		DbTxRef dbt(m_db);

		DbCursor c(dbt, m_tableBlocks);
		if (!c.Get(BlockKey(height)))
			Throw(E_COIN_InconsistentDatabase);
		return LoadBlock(dbt, height, c.Data);
	}

	int GetMaxHeight() override {
		DbTxRef dbt(m_db);
		DbCursor c(dbt, m_tableBlocks);
		if (!c.Seek(CursorPos::Last))
			return -1;
		return BlockKey(c.Key);
	}

	TxHashesOutNums GetTxHashesOutNums(int height) override {
		DbTxRef dbt(m_db);
		DbCursor c(dbt, m_tableBlocks);
		if (!c.Get(BlockKey(height)))
			Throw(E_COIN_InconsistentDatabase);
		CMemReadStream stm(c.Data);
		Blob blob;
		BinaryReader(stm) >> blob >> blob >> blob;
		return TxHashesOutNums(blob);
	}

	void InsertBlock(int height, const HashValue& hash, const ConstBuf& data, const ConstBuf& txData) override {
		ASSERT(height < 0xFFFFFF);

		BlockKey blockKey(height);
		MemoryStream msB;
		BinaryWriter wrT(msB);
		wrT << ConstBuf(ReducedBlockHash(hash)) << data << txData;
		DbTxRef dbt(m_db);
		m_tableBlocks.Put(dbt, blockKey, msB, true);
		m_tableHashToBlock.Put(dbt, ReducedBlockHash(hash), blockKey, true);
	}

	struct TxData {
		UInt32 Height;
		Blob Data, TxIns, Coins;
		HashValue HashTx;

		void Read(BinaryReader& rd) {
			UInt32 h = 0;
			rd.BaseStream.ReadBuffer(&h, BLOCKID_SIZE);
			Height = letoh(h);
			rd >> Data >> TxIns >> Coins;
		}

		void Write(BinaryWriter& wr, bool bWriteHash = false) const {
			UInt32 h = htole(Height);
			wr.BaseStream.WriteBuffer(&h, BLOCKID_SIZE);
			wr << Data << TxIns << Coins;
			if (bWriteHash)
				wr << HashTx;
		}
	};

	struct TxDatas {
		vector<TxData> Items;
		int Index;

		TxDatas()
			:	Index(0)
		{}
	};

	ConstBuf TxKey(const HashValue& txHash) { return ConstBuf(txHash.data(), TXID_SIZE); }
	ConstBuf TxKey(const ConstBuf& txid8) { return ConstBuf(txid8.P, TXID_SIZE); }

	bool FindTxDatas(const ConstBuf& txid8, TxDatas& txDatas);
	TxDatas GetTxDatas(const ConstBuf& txid8);
	void PutTxDatas(const ConstBuf& txKey, const TxDatas& txDatas);

	void DeleteBlock(int height, const vector<Int64>& txids) override {
		DbTxRef dbt(m_db);
		DbCursor c(dbt, m_tableBlocks);
		if (c.Get(BlockKey(height))) {
			Blob blob;
			BinaryReader(CMemReadStream(c.Data)) >> blob;
			c.Delete();
			m_tableHashToBlock.Delete(dbt, blob);
		}
		EXT_FOR (Int64 idTx, txids) {
			idTx = htole(idTx);
			ConstBuf txid8(&idTx, 8);
			TxDatas txDatas = GetTxDatas(txid8);
			for (int i=txDatas.Items.size(); i--;) {
				if (txDatas.Items[i].Height == height)
					txDatas.Items.erase(txDatas.Items.begin()+i);
			}
			if (txDatas.Items.empty())
				m_tableTxes.Delete(dbt, TxKey(txid8));
			else
				PutTxDatas(TxKey(txid8), txDatas);
		}
	}

	bool FindTxById(const ConstBuf& txid8, Tx *ptx) override {
		TxDatas txDatas;
		if (!FindTxDatas(txid8, txDatas))
			return false;
		if (ptx) {
			ptx->EnsureCreate();

			CoinEng& eng = Eng();
			const TxData& txData = txDatas.Items[txDatas.Index];
			ptx->Height = txData.Height;
			CMemReadStream stm(txData.Data);
			DbReader rd(stm, &eng);
			rd >> *ptx;
			ptx->SetReducedHash(txid8);
		}
		return true;
	}

	bool FindTx(const HashValue& hash, Tx *ptx) override {
		bool r = FindTxById(ConstBuf(hash.data(), 8), ptx);
		if (r && ptx)
			ptx->SetHash(hash);
		return r;
	}

	void ReadTxIns(const HashValue& hash, const TxObj& txObj) override {
		TxDatas txDatas = GetTxDatas(ConstBuf(hash.data(), 8));
		txObj.ReadTxIns(DbReader(CMemReadStream(txDatas.Items[txDatas.Index].TxIns), &Eng()));
	}

	pair<int, int> FindPrevTxCoords(DbWriter& wr, int height, const HashValue& hash) override {
		DbTxRef dbt(m_db);

		TxDatas txDatas = GetTxDatas(ConstBuf(hash.data(), 8));
		int heightOut = txDatas.Items[txDatas.Index].Height;
		wr.Write7BitEncoded(height-heightOut+1);

		DbCursor c1(dbt, m_tableBlocks);
		if (!c1.Get(BlockKey(heightOut)))
			Throw(E_COIN_InconsistentDatabase);
		CMemReadStream ms(c1.Data);
		Blob blob;
		BinaryReader(ms) >> blob >> blob >> blob;
		pair<int, int> pp = TxHashesOutNums(blob).StartingTxOutIdx(hash);
		if (pp.second < 0)
			Throw(E_COIN_InconsistentDatabase);
		return pp;
	}

	void InsertPubkeyToTxes(DbTransaction& dbTx, const Tx& tx);
	vector<Int64> GetTxesByPubKey(const HashValue160& pubkey) override;

	void InsertTx(const Tx& tx, const TxHashesOutNums& hashesOutNums, const HashValue& txHash, int height, const ConstBuf& txIns, const ConstBuf& spend, const ConstBuf& data) override {
		ASSERT(height < 0xFFFFFF);

		CoinEng& eng = Eng();

		MemoryStream msT;
		UInt32 h = htole(height);
		msT.WriteBuffer(&h, BLOCKID_SIZE);
		BinaryWriter(msT).Ref() << data << txIns << spend;

		DbTxRef dbt(m_db);
		try {
			DBG_LOCAL_IGNORE_NAME(E_EXT_DB_DupKey, E_EXT_DB_DupKey);

			m_tableTxes.Put(dbt, TxKey(txHash), msT, true);
		} catch (DbException& ex) {
			TxDatas txDatas;
			if (FindTxDatas(ConstBuf(txHash.data(), 8), txDatas) && (txDatas.Items.size()!=1 ||
				ConstBuf(txDatas.Items[0].TxIns) == txIns) && ConstBuf(txDatas.Items[0].Data) == data)
			{
				TxData& txData = txDatas.Items[txDatas.Index];
				txData.Coins = spend;

				String msg = ex.Message; //!!!D
				TRC(1, "Duplicated Transaction: " << txHash);

				if (height >= eng.ChainParams.CheckDupTxHeight && ContainsInLinear(GetCoinsByTxHash(txHash), true))
					Throw(E_COIN_DupNonSpentTx);
				PutTxDatas(TxKey(txHash), txDatas);
			} else {
				ASSERT(!txDatas.Items.empty());
				if (txDatas.Items.size() == 1) {
					TxData& t = txDatas.Items[0];
					TxHashesOutNums hons = t.Height==height ? hashesOutNums : GetTxHashesOutNums(t.Height);
					for (int i=0; i<hons.size(); ++i) {
						if (!memcmp(hons[i].HashTx.data(), txHash.data(), TXID_SIZE)) {
							t.HashTx = hons[i].HashTx;
							goto LAB_FOUND;
						}
					}
					Throw(E_COIN_InconsistentDatabase);
				}
LAB_FOUND:					
				TxData txData;
				txData.Height = height;
				txData.Data = data;
				txData.TxIns = txIns;
				txData.Coins = spend;
				txData.HashTx = txHash;
				txDatas.Items.push_back(txData);
				PutTxDatas(TxKey(txHash), txDatas);
			}
		}
		if (eng.Mode == EngMode::BlockExplorer)
			InsertPubkeyToTxes(dbt, tx);
	}

	vector<bool> GetCoinsByTxHash(const HashValue& hash) override {
		TxDatas txDatas = GetTxDatas(ConstBuf(hash.data(), 8));
		const TxData& txData = txDatas.Items[txDatas.Index];
		vector<bool> vec;
		for (int j=0; j<txData.Coins.Size; ++j) {
			byte v = txData.Coins.constData()[j];
			for (int i=0; i<8; ++i)
				vec.push_back(v & (1 << i));
		}
		return vec;
	}

	void SaveCoinsByTxHash(const HashValue& hash, const vector<bool>& vec) override {
		TxDatas txDatas = GetTxDatas(ConstBuf(hash.data(), 8));
		TxData& txData = txDatas.Items[txDatas.Index];
		txData.Coins = find(vec.begin(), vec.end(), true) != vec.end() ? CoinEng::SpendVectorToBlob(vec) : Blob();
		PutTxDatas(TxKey(hash), txDatas);
	}

	void BeginEngTransaction() override {
		Throw(E_NOTIMPL);
	}

	vector<Block> GetBlocks(const LocatorHashes& locators, const HashValue& hashStop) override {
		Throw(E_NOTIMPL);
	}

	Blob FindPubkey(Int64 id) override {
		DbTxRef dbt(m_db);
		id = htole(id);
		DbCursor c(dbt, m_tablePubkeys);
		return c.Get(ConstBuf(&id, PUBKEYID_SIZE)) ? Blob(c.Data) : Blob(nullptr);				// Blob(c.Data) should be explicit here because else implicit Blob(ConstBuf(Blob(nullptr))) generated
	}

	void InsertPubkey(Int64 id, const ConstBuf& pk) override {
		DbTxRef dbt(m_db);
		id = htole(id);
		m_tablePubkeys.Put(dbt, ConstBuf(&id, PUBKEYID_SIZE), pk);
	}

	void UpdatePubkey(Int64 id, const ConstBuf& pk) override {
		DbTxRef dbt(m_db);
		id = htole(id);
		m_tablePubkeys.Put(dbt, ConstBuf(&id, PUBKEYID_SIZE), pk);		
	}

	void BeginTransaction() override {
//		ASSERT(!m_dbt.get());
//		m_dbt.reset(new MdbTransaction(m_db));
	}

	void Commit() override {
//		m_dbt->Commit();
//		m_dbt.reset();
	}

	void Rollback() override  {
	//	m_dbt->Rollback();
//		m_dbt.reset();
	}

	void SetProgressHandler(int(*pfn)(void*), void *p, int n) override {
#if UCFG_COIN_COINCHAIN_BACKEND == COIN_BACKEND_DBLITE
		m_db.SetProgressHandler(pfn, p, n);
#endif
	}

#if UCFG_COIN_COINCHAIN_BACKEND == COIN_BACKEND_DBLITE
	void Vacuum() override {
		m_db.Vacuum();	
	}
#endif
private:
	Block LoadBlock(DbTransaction& dbt, int height, const ConstBuf& cbuf);
	bool TryToConvert(RCString path);
};

EXT_THREAD_PTR(DbTransaction, t_pDbTransaction);

DbTxRef::DbTxRef(DbStorage& env) {
	if (!(m_p = t_pDbTransaction))
		m_pMdbTx.reset(m_p = new DbTransaction(env, true));
}

class SavepointTransaction : public DbTransaction {
	typedef DbTransaction base;
public:
	MdbBlockChainDb& m_bcdb;

	SavepointTransaction(MdbBlockChainDb& bcdb)
		:	base(bcdb.m_db)
		,	m_bcdb(bcdb)
	{
		m_bcdb.MtxDb.lock();
		ASSERT(!t_pDbTransaction);
		t_pDbTransaction = this;
	}

	~SavepointTransaction() {
		t_pDbTransaction = nullptr;
		m_bcdb.MtxDb.unlock();
	}

	void BeginTransaction() override {
	}

	void Commit() override {
		base::Commit();

/*!!!		CoinEng& eng = Eng();
		if (!(++m_bcdb.m_nCheckpont & 2047))
			m_bcdb.m_db.Checkpoint(); */
		delete this;
	}

	void Rollback() override {
		base::Rollback();
		delete this;
	}
};

ITransactionable& MdbBlockChainDb::GetSavepointObject() {
	return *(new SavepointTransaction(_self));
}

#if UCFG_COIN_CONVERT_TO_UDB
class ConvertDbThread : public Thread {
	typedef Thread base;
public:
	MdbBlockChainDb& Db;
	CoinEng& Eng;
	String Path;
	ptr<CoinEng> EngSqlite;

	ConvertDbThread(MdbBlockChainDb& db, CoinEng& eng, RCString path)
		:	base(&eng.m_tr)
		,	Eng(eng)
		,	Db(db)
		,	Path(path)
	{}
protected:
	void Execute() override {
		Name = "ConvertDbThread";

		String pathSqlite = Path::ChangeExtension(Path, ".db");
		/*!!!
		String pathSqlite = Path+".sqlite";
		if (File::Exists(pathSqlite)) {
			File::Delete(Path);
		} else {
			File::Move(Path, pathSqlite);

			if (File::Exists(Path+"-wal"))
				File::Move(Path+"-wal", Path+".sqlite"+"-wal");
			if (File::Exists(Path+"-journal"))
				File::Move(Path+"-journal", Path+".sqlite"+"-journal");			
		}
		
		Db.Create(Path); */
		CCoinEngThreadKeeper engKeeper(&Eng);
		int n = EngSqlite->Db->GetMaxHeight()+1;
		if (n > 0) {
			Eng.UpgradingDatabaseHeight = n;
			for (int i=Eng.Db->GetMaxHeight()+1; i<n && !m_bStop; ++i) {
				if (Eng.m_iiEngEvents) {
					Eng.m_iiEngEvents->OnSetProgress((float)i);
					Eng.m_iiEngEvents->OnChange();
				}

				MemoryStream ms;
				try {
					CCoinEngThreadKeeper engKeeperSqlite(EngSqlite.get());
					EngSqlite->GetBlockByHeight(i).Write(BinaryWriter(ms).Ref());
				} catch (RCExc) {
					break;
				}
				CMemReadStream stm(ms);
				Block block;
				block.Read(BinaryReader(stm));
				EXT_LOCK (Eng.Mtx) {
					block.Process(0);
				}
			}
		}
		EngSqlite->Close();
		if (!m_bStop) {
//!!!			File::Delete(pathSqlite);
			Eng.ContinueLoad();
		}
		Eng.UpgradingDatabaseHeight = 0;
	}
};
#endif // UCFG_COIN_CONVERT_TO_UDB

bool MdbBlockChainDb::TryToConvert(RCString path) {
#if UCFG_COIN_CONVERT_TO_UDB
	CoinEng& eng = Eng();
	if (eng.ChainParams.Name == "Bitcoin" || eng.ChainParams.Name == "Bitcoin-testnet2" || eng.ChainParams.Name == "LiteCoin") {	// convert only huge blockchains
		String pathSqlite = Path::ChangeExtension(path, ".db");
		if (File::Exists(pathSqlite)) {
			char header[16];
			FileStream(pathSqlite, FileMode::Open).ReadBuffer(header, 15);
			header[15] = 0;
			if (!strcmp(header, "SQLite format 3")) {
				ptr<CoinEng> engSqlite = CoinEng::CreateObject(eng.m_cdb, eng.ChainParams.Name, CreateSqliteBlockChainDb());
				engSqlite->Db->Open(pathSqlite);
				if (engSqlite->Db->GetMaxHeight() > GetMaxHeight()) {
					ptr<ConvertDbThread> t = new ConvertDbThread(_self, eng, path);
					t->EngSqlite = engSqlite;
					eng.UpgradingDatabaseHeight = 1;
					t->Start();
					return true;
				}
			}
		}
	}
#endif // UCFG_COIN_CONVERT_TO_UDB
	return false;
}

bool MdbBlockChainDb::Create(RCString path) {
	CoinEng& eng = Eng();

	m_db.Create(path);

	{
		DbTransaction dbt(m_db);
		m_tableBlocks.Open(dbt, true);
		m_tableHashToBlock.Open(dbt, true);
		m_tableTxes.Open(dbt, true);
		m_tablePubkeys.Open(dbt, true);
		if (eng.Mode == EngMode::BlockExplorer) {
			m_tablePubkeyToTxes.Open(dbt, true);
		}
	}
	m_db.Checkpoint();
	if (TryToConvert(path))
		return false;

	return true;
}

bool MdbBlockChainDb::Open(RCString path) {
	CoinEng& eng = Eng();
		/*
	char header[16];
	FileStream(path, FileMode::Open).ReadBuffer(header, 15);
	header[15] = 0;
	if (!strcmp(header, "SQLite format 3") || File::Exists(path+".sqlite")) {
#if UCFG_COIN_CONVERT_TO_UDB
		if (eng.ChainParams.Name == "Bitcoin" || eng.ChainParams.Name == "Bitcoin-testnet2" || eng.ChainParams.Name == "LiteCoin") {	// convert only huge blockchains
			eng.UpgradingDatabaseHeight = 1;
			(new ConvertDbThread(_self, eng, path))->Start();
			return false;

		} else
#endif // UCFG_COIN_CONVERT_TO_UDB
		{
			File::Delete(path);
			Create(path);
			return true;
		}		
	}*/

	m_db.Open(path);

	{
		DbTransaction dbt(m_db, true);
		m_tableBlocks.Open(dbt);
		m_tableHashToBlock.Open(dbt);
		m_tableTxes.Open(dbt);
		m_tablePubkeys.Open(dbt);
		if (eng.Mode == EngMode::BlockExplorer) {
			m_tablePubkeyToTxes.Open(dbt);
		}
	}
#ifdef X_DEBUG//!!!D
	{
		DbTxRef dbt(m_db);

		UInt64 prev = 0;
		int n = 0;
		for (DbCursor c(dbt, m_tablePubkeys); c.SeekToNext();) {
			++n;
			ASSERT(c.Key.Size == 8);
			UInt64 nprev = betoh(*(UInt64*)c.Key.P)+1;
			Int64 id = letoh(*(UInt64*)c.Key.P);
			ASSERT(nprev > prev);
			prev = nprev;
		}
		n = n;
	}
#endif


	if (TryToConvert(path))
		return false;
	return true;
}


Block MdbBlockChainDb::LoadBlock(DbTransaction& dbt, int height, const ConstBuf& cbuf) {
	CoinEng& eng = Eng();

	CMemReadStream msBlock(cbuf);
	BinaryReader rd(msBlock);

	Blob blob;
	rd >> blob;
	BlockHashValue hash(blob);
	Block r;
	r.m_pimpl->Height = height;

	rd >> blob;
	CMemReadStream ms(blob);
	r.m_pimpl->Read(DbReader(ms, &eng));
	r.m_pimpl->m_hash = hash;

	rd >> blob;
	r.m_pimpl->m_txHashesOutNums = Coin::TxHashesOutNums(blob);

	if (r.Height > 0) {
		DbCursor c(dbt, m_tableBlocks);
		if (!c.Get(BlockKey(height-1)))
			Throw(E_COIN_InconsistentDatabase);
		CMemReadStream msPrev(c.Data);
		BinaryReader(msPrev) >> blob;
		r.m_pimpl->PrevBlockHash = BlockHashValue(blob);
	}

//!!!	ASSERT(Hash(r) == hash);
	
	return r;
}

void MdbBlockChainDb::InsertPubkeyToTxes(DbTransaction& dbTx, const Tx& tx) {
	unordered_set<HashValue160> pubkeys;

	HashValue hashTx = Hash(tx);
	HashValue160 hash160;
	Blob pk;
	EXT_FOR (const TxOut& txOut, tx.TxOuts()) {
		switch (txOut.TryParseDestination(hash160, pk)) {
		case 20:
		case 33:
		case 65:
			pubkeys.insert(hash160);
			break;
		}
	}
	EXT_FOR (const TxIn& txIn, tx.TxIns()) {
		if (txIn.PrevOutPoint.Index >= 0) {
			Tx txPrev = Tx::FromDb(txIn.PrevOutPoint.TxHash);
			const TxOut& txOut = txPrev.TxOuts()[txIn.PrevOutPoint.Index];
			switch (txOut.TryParseDestination(hash160, pk)) {
			case 20:
			case 33:
			case 65:
				pubkeys.insert(hash160);
				break;
			}
		}
	}

	EXT_FOR (const HashValue160& pubkey, pubkeys) {
		ConstBuf dbkey(pubkey.data(), PUBKEYTOTXES_ID_SIZE);
		DbCursor c(dbTx, m_tablePubkeyToTxes);
		c.PushFront(dbkey, ConstBuf(hashTx.data(), 8));

		/*!!!
		if (!c.Get(dbkey))
			c.Put(dbkey, ConstBuf(hashTx.data(), 8));
		else {
			ConstBuf cbuf = c.Data;
			Blob blob = c.Data + ConstBuf(hashTx.data(), 8);
			c.Put(dbkey, blob);
		}	*/		
	}
}

bool MdbBlockChainDb::FindTxDatas(const ConstBuf& txid8, TxDatas& txDatas) {
	ASSERT(txDatas.Items.empty());

	DbTxRef dbt(m_db);
	DbCursor c(dbt, m_tableTxes);
	if (!c.Get(TxKey(txid8)))
		return false;
	CMemReadStream msT(c.Data);
	BinaryReader rdT(msT);
	TxData txData;
	txData.Read(rdT);
	txDatas.Items.push_back(txData);
	if (msT.Eof()) {
		txDatas.Index = 0;
		return true;
	} else {
		rdT >> txDatas.Items.back().HashTx;
		while (!msT.Eof()) {
			TxData txData;
			txData.Read(rdT);
			rdT >> txData.HashTx;
			txDatas.Items.push_back(txData);
		}
		for (int i=0; i<txDatas.Items.size(); ++i) {
			if (!memcmp(txDatas.Items[i].HashTx.data(), txid8.P, 8)) {
				txDatas.Index = i;
				return true;
			}
		}
		return false;
	}	
}

MdbBlockChainDb::TxDatas MdbBlockChainDb::GetTxDatas(const ConstBuf& txid8) {
	TxDatas txDatas;
	if (!FindTxDatas(txid8, txDatas))
		Throw(E_COIN_InconsistentDatabase);
	return txDatas;
}

void MdbBlockChainDb::PutTxDatas(const ConstBuf& txKey, const TxDatas& txDatas) {
	ASSERT(!txDatas.Items.empty());

	MemoryStream ms;
	BinaryWriter wr(ms);
	if (txDatas.Items.size() == 1)
		txDatas.Items[0].Write(wr, false);
	else {
		EXT_FOR (const TxData& txData, txDatas.Items) {
			txData.Write(wr, true);
		}
	}
	m_tableTxes.Put(DbTxRef(m_db), txKey, ms);
}

vector<Int64> MdbBlockChainDb::GetTxesByPubKey(const HashValue160& pubkey) {
	vector<Int64> r;
	DbTxRef dbt(m_db);
	DbCursor c(dbt, m_tablePubkeyToTxes);
	if (c.Get(ConstBuf(pubkey.data(), PUBKEYTOTXES_ID_SIZE))) {
		ConstBuf data = c.Data;
		r.resize(data.Size / 8);
		memcpy(&r[0], data.P, data.Size);
	}
	return r;
}


ptr<IBlockChainDb> CreateBlockChainDb() {
	return new MdbBlockChainDb;
}


} // Coin::

#endif // UCFG_COIN_COINCHAIN_BACKEND == COIN_BACKEND_DBLITE
