/*######     Copyright (c) 1997-2015 Ufasoft  http://ufasoft.com  mailto:support@ufasoft.com,  Sergey Pavlov  mailto:dev@ufasoft.com #########################################################################################################
#                                                                                                                                                                                                                                            #
# This program is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation;  either version 3, or (at your option) any later version.          #
# This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.   #
# You should have received a copy of the GNU General Public License along with this program; If not, see <http://www.gnu.org/licenses/>                                                                                                      #
############################################################################################################################################################################################################################################*/

#include <el/ext.h>

#include "param.h"


#if UCFG_COIN_COINCHAIN_BACKEND == COIN_BACKEND_DBLITE

#include "backend-dblite.h"

#include "coin-msg.h"

namespace Ext {

template<> EXT_THREAD_PTR(Ext::DB::KV::DbReadTransaction)  CThreadTxRef<Ext::DB::KV::DbReadTransaction>::t_pTx;
}

namespace Coin {

const Version VER_BLOCKS_TABLE_IS_HASHTABLE(0, 81),
	VER_PUBKEY_RECOVER(0, 90);



//
//template class CThreadTxRef<DbReadTransaction>;


/*!!!
EXT_THREAD_PTR(DbReadTransaction) t_pDbTransaction;

DbReadTxRef::DbReadTxRef(DbStorage& env) {
	if (!(m_p = t_pDbTransaction)) {
		m_p = new(m_placeDbTx) DbReadTransaction(env);
		m_bOwn = true;
	}
}

DbReadTxRef::~DbReadTxRef() {
	if (m_bOwn)
		reinterpret_cast<DbReadTransaction*>(m_placeDbTx)->~DbReadTransaction();
}
*/

DbTxRef::DbTxRef(DbStorage& env) {
	m_p = &dynamic_cast<DbTransaction&>(*DbReadTxRef::t_pTx);
/*!!!
	if (DbReadTransaction *p = t_pDbTransaction)
		m_p = dynamic_cast<DbTransaction*>(p);		//!!!TODO O static_cast<>
	else
		m_pMdbTx.reset(m_p = new DbTransaction(env, true));
		*/
}

DbliteBlockChainDb::DbliteBlockChainDb()
//	:	m_tableBlocks		("blocks",		BLOCKID_SIZE,			TableType::HashTable, HashType::Identity)
	:	m_tableBlocks		("blocks",		BLOCKID_SIZE,			TableType::BTree)			// ordered					//!!!T
	,	m_tableHashToBlock	("hash_blocks",	0,						TableType::HashTable)
	,	m_tableTxes			("txes",		TXID_SIZE,				TableType::HashTable, HashType::Identity)
	,	m_tablePubkeys		("pubkeys",		PUBKEYID_SIZE,			TableType::HashTable, HashType::Identity)
	,	m_tablePubkeyToTxes	("pubkey_txes",	PUBKEYTOTXES_ID_SIZE,	TableType::HashTable, HashType::Identity)
	,	m_tableProperties	("properties",	0,						TableType::HashTable)
{
	DefaultFileExt = ".udb";

	m_db.AsyncClose = true;
	m_db.Durability = false;
	m_db.UseFlush = false;			//!!!?
	m_db.ProtectPages = false;	//!!!?
	m_db.CheckpointPeriod = TimeSpan::FromMinutes(5);
#ifdef X_DEBUG//!!!T
		
//		m_db.Durability = true;
//		m_db.UseFlush = false;
#endif

}

class SavepointTransaction : public DbTransaction {
	typedef DbTransaction base;
public:
	DbliteBlockChainDb& m_bcdb;

	SavepointTransaction(DbliteBlockChainDb& bcdb)
		:	base(bcdb.m_db)
		,	m_bcdb(bcdb)
	{
		m_bcdb.MtxDb.lock();
		ASSERT(!DbReadTxRef::t_pTx);
		DbReadTxRef::t_pTx = this;
	}

	~SavepointTransaction() {
		DbReadTxRef::t_pTx = nullptr;
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

ITransactionable& DbliteBlockChainDb::GetSavepointObject() {
	return *(new SavepointTransaction(_self));
}

#if UCFG_COIN_CONVERT_TO_UDB
class ConvertDbThread : public Thread {
	typedef Thread base;
public:
	DbliteBlockChainDb& Db;
	CoinEng& Eng;
	path Path;
	ptr<CoinEng> EngSqlite;

	ConvertDbThread(DbliteBlockChainDb& db, CoinEng& eng, const path& p)
		:	base(&eng.m_tr)
		,	Eng(eng)
		,	Db(db)
		,	Path(p)
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


bool DbliteBlockChainDb::TryToConvert(const path& p) {
#if UCFG_COIN_CONVERT_TO_UDB
	CoinEng& eng = Eng();
	if (eng.ChainParams.Name == "Bitcoin" || eng.ChainParams.Name == "Bitcoin-testnet2" || eng.ChainParams.Name == "LiteCoin") {	// convert only huge blockchains
		path pathSqlite = p;
		pathSqlite.replace_extension(".db");
		if (exists(pathSqlite)) {
			char header[16];
			FileStream(pathSqlite, FileMode::Open).ReadBuffer(header, 15);
			header[15] = 0;
			if (!strcmp(header, "SQLite format 3")) {
				ptr<CoinEng> engSqlite = CoinEng::CreateObject(eng.m_cdb, eng.ChainParams.Name, CreateSqliteBlockChainDb());
				engSqlite->Db->Open(pathSqlite);
				if (engSqlite->Db->GetMaxHeight() > GetMaxHeight()) {
					ptr<ConvertDbThread> t = new ConvertDbThread(_self, eng, p);
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

bool DbliteBlockChainDb::Create(const path& p) {
	CoinEng& eng = Eng();
	
	if (eng.LiteMode)
		m_db.UseFlush = true;
	m_db.AppName = VER_INTERNALNAME_STR;
	m_db.UserVersion = Version(VER_PRODUCTVERSION);
	m_db.Create(p);
	{
		DbTransaction dbt(m_db);
		m_tableBlocks.Open(dbt, true);
		m_tableHashToBlock.Open(dbt, true);
		m_tableTxes.Open(dbt, true);
		m_tablePubkeys.Open(dbt, true);
		m_tableProperties.Open(dbt, true);
		if (eng.Mode == EngMode::BlockExplorer) {
			m_tablePubkeyToTxes.Open(dbt, true);
		}
		OnOpenTables(dbt, true);
		dbt.Commit();
	}
	m_db.Checkpoint();
	if (TryToConvert(p))
		return false;

	return true;
}

bool DbliteBlockChainDb::Open(const path& p) {
	CoinEng& eng = Eng();

	if (eng.LiteMode)
		m_db.UseFlush = true;
	m_db.Open(p);
	{
		DbTransaction dbt(m_db, true);
		m_tableBlocks.Open(dbt);
		m_tableHashToBlock.Open(dbt);
		m_tableTxes.Open(dbt);
		m_tablePubkeys.Open(dbt);
		if (m_db.UserVersion >= VER_BLOCKS_TABLE_IS_HASHTABLE) {
			m_tableProperties.Open(dbt);
		}
		if (eng.Mode == EngMode::BlockExplorer) {
			m_tablePubkeyToTxes.Open(dbt);
		}
		OnOpenTables(dbt, false);
	}
	if (TryToConvert(p))
		return false;
	return true;
}

void DbliteBlockChainDb::Close(bool bAsync) {
//!!!!R		m_dbt.reset();

	m_tableProperties.Close();
	m_tableBlocks.Close();
	m_tableHashToBlock.Close();
	m_tableTxes.Close();
	m_tablePubkeys.Close();
	m_tablePubkeyToTxes.Close();
	m_db.AsyncClose = bAsync;
	m_db.Close();
}

Version DbliteBlockChainDb::CheckUserVersion() {
	if (m_db.AppName != VER_INTERNALNAME_STR) {
		m_db.Close();
		throw Exception(E_FAIL, "This database has AppName \"" + m_db.AppName + "\", but should be \"" + VER_INTERNALNAME_STR + "\"");
	}	

    Version ver(VER_PRODUCTVERSION),
			dver = m_db.UserVersion;
    if (dver > ver && (dver.Major!=ver.Major || dver.Minor/10 != ver.Minor/10)) {					// Versions [mj.x0 .. mj.x9] are DB-compatible
    	m_db.Close();
    	throw VersionException(dver);
    }
    return dver;
}

const ConstBuf MAX_HEIGHT_KEY("MaxHeight", 9);

int DbliteBlockChainDb::GetMaxHeight() {
	DbReadTxRef dbt(m_db);
	if (m_db.UserVersion < VER_BLOCKS_TABLE_IS_HASHTABLE) {
		DbCursor c(dbt, m_tableBlocks);
		if (!c.Seek(CursorPos::Last))
			return -1;
		return BlockKey(c.Key);
	} else {
		DbCursor c(dbt, m_tableProperties);
		if (!c.SeekToKey(MAX_HEIGHT_KEY))
			return -1;
		return letoh(*(int32_t*)c.get_Data().P);
	}
}

void DbliteBlockChainDb::UpgradeTo(const Version& ver) {
	CoinEng& eng = Eng();

	Version dbver = CheckUserVersion();
	if (dbver < ver) {
		TRC(0, "Upgrading Database " << eng.GetDbFilePath() << " from version " << dbver << " to " << ver);

		DbTransaction dbtx(m_db);

		if (dbver < VER_BLOCKS_TABLE_IS_HASHTABLE) {
			m_tableProperties.Open(dbtx, true);
			int32_t h = htole(int32_t(GetMaxHeight()));
			m_tableProperties.Put(dbtx, MAX_HEIGHT_KEY, ConstBuf(&h, sizeof h));
		}

		eng.UpgradeDb(ver);
		m_db.SetUserVersion(ver);
		dbtx.Commit();
	}
}

void DbliteBlockChainDb::TryUpgradeDb(const Version& verApp) {
	UpgradeTo(VER_PUBKEY_RECOVER);
}

Block DbliteBlockChainDb::LoadBlock(DbReadTransaction& dbt, int height, const ConstBuf& cbuf) {
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

Block DbliteBlockChainDb::FindBlock(const HashValue& hash) {
	Block r(nullptr);
	DbReadTxRef dbt(m_db);
	DbCursor c(dbt, m_tableHashToBlock);
	if (c.Get(ReducedBlockHash(hash))) {
		int height = (uint32_t)BlockKey(c.Data);
		DbCursor c1(dbt, m_tableBlocks);
		if (!c1.Get(c.Data)) {
			Throw(E_COIN_InconsistentDatabase);
		}
		r = LoadBlock(dbt, height, c1.Data);
	}
	return r;
}

Block DbliteBlockChainDb::FindBlock(int height) {
	DbReadTxRef dbt(m_db);

	DbCursor c(dbt, m_tableBlocks);
	if (!c.Get(BlockKey(height)))
		Throw(E_COIN_BlockNotFound);
	return LoadBlock(dbt, height, c.Data);
}

TxHashesOutNums DbliteBlockChainDb::GetTxHashesOutNums(int height) {
	DbReadTxRef dbt(m_db);
	DbCursor c(dbt, m_tableBlocks);
	if (!c.Get(BlockKey(height)))
		Throw(E_COIN_InconsistentDatabase);
	CMemReadStream stm(c.Data);
	Blob blob;
	BinaryReader(stm) >> blob >> blob >> blob;
	return TxHashesOutNums(blob);
}

void DbliteBlockChainDb::InsertBlock(int height, const HashValue& hash, const ConstBuf& data, const ConstBuf& txData) {
	ASSERT(height < 0xFFFFFF);

	BlockKey blockKey(height);
	MemoryStream msB;
	BinaryWriter wrT(msB);
	wrT << ConstBuf(ReducedBlockHash(hash)) << data << txData;
	DbTxRef dbt(m_db);
	m_tableBlocks.Put(dbt, blockKey, msB, true);
	m_tableHashToBlock.Put(dbt, ReducedBlockHash(hash), blockKey, true);
	if (m_db.UserVersion >= VER_BLOCKS_TABLE_IS_HASHTABLE) {
		int32_t h = htole(int32_t(height));
		m_tableProperties.Put(dbt, MAX_HEIGHT_KEY, ConstBuf(&h, sizeof h));
	}
	dbt.CommitIfLocal();
}

void DbliteBlockChainDb::TxData::Read(BinaryReader& rd) {
	uint32_t h = 0;
	rd.BaseStream.ReadBuffer(&h, BLOCKID_SIZE);
	Height = letoh(h);
	rd >> Data >> TxIns >> Coins;
}

void DbliteBlockChainDb::TxData::Write(BinaryWriter& wr, bool bWriteHash) const {
	uint32_t h = htole(Height);
	wr.BaseStream.WriteBuffer(&h, BLOCKID_SIZE);
	wr << Data << TxIns << Coins;
	if (bWriteHash)
		wr << HashTx;
}

pair<int, int> DbliteBlockChainDb::FindPrevTxCoords(DbWriter& wr, int height, const HashValue& hash) {
	DbReadTxRef dbt(m_db);

	TxDatas txDatas = GetTxDatas(ConstBuf(hash.data(), 8));
	int heightOut = txDatas.Items[txDatas.Index].Height;
	wr.Write7BitEncoded(height-heightOut+1);

	DbCursor c1(dbt, m_tableBlocks);
	if (!c1.Get(BlockKey(heightOut))) {
		Throw(E_COIN_InconsistentDatabase);
	}
	CMemReadStream ms(c1.Data);
	Blob blob;
	BinaryReader(ms) >> blob >> blob >> blob;
	pair<int, int> pp = TxHashesOutNums(blob).StartingTxOutIdx(hash);
	if (pp.second < 0)
		Throw(E_COIN_InconsistentDatabase);
	return pp;
}

void DbliteBlockChainDb::InsertPubkeyToTxes(DbTransaction& dbTx, const Tx& tx) {
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

bool DbliteBlockChainDb::FindTxDatas(const ConstBuf& txid8, TxDatas& txDatas) {
	ASSERT(txDatas.Items.empty());

	DbReadTxRef dbt(m_db);
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

DbliteBlockChainDb::TxDatas DbliteBlockChainDb::GetTxDatas(const ConstBuf& txid8) {
	TxDatas txDatas;
	if (!FindTxDatas(txid8, txDatas))
		Throw(E_COIN_InconsistentDatabase);
	return txDatas;
}

void DbliteBlockChainDb::PutTxDatas(const ConstBuf& txKey, const TxDatas& txDatas) {
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
	DbTxRef dbt(m_db);
	m_tableTxes.Put(dbt, txKey, ms);
	dbt.CommitIfLocal();
}

void DbliteBlockChainDb::DeleteBlock(int height, const vector<int64_t>& txids) {
	DbTxRef dbt(m_db);
	DbCursor c(dbt, m_tableBlocks);
	if (c.Get(BlockKey(height))) {
		Blob blob;
		BinaryReader(CMemReadStream(c.Data)) >> blob;
		c.Delete();
		m_tableHashToBlock.Delete(dbt, blob);
	}
	if (m_db.UserVersion >= VER_BLOCKS_TABLE_IS_HASHTABLE) {
		int32_t h = htole(height-1);
		m_tableProperties.Put(dbt, MAX_HEIGHT_KEY, ConstBuf(&h, sizeof h));
	}
	EXT_FOR (int64_t idTx, txids) {
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
	dbt.CommitIfLocal();
}

bool DbliteBlockChainDb::FindTxById(const ConstBuf& txid8, Tx *ptx) {
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

vector<int64_t> DbliteBlockChainDb::GetTxesByPubKey(const HashValue160& pubkey) {
	vector<int64_t> r;
	DbReadTxRef dbt(m_db);
	DbCursor c(dbt, m_tablePubkeyToTxes);
	if (c.Get(ConstBuf(pubkey.data(), PUBKEYTOTXES_ID_SIZE))) {
		ConstBuf data = c.Data;
		r.resize(data.Size / 8);
		memcpy(&r[0], data.P, data.Size);
	}
	return r;
}

vector<bool> DbliteBlockChainDb::GetCoinsByTxHash(const HashValue& hash) {
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

void DbliteBlockChainDb::InsertTx(const Tx& tx, const TxHashesOutNums& hashesOutNums, const HashValue& txHash, int height, const ConstBuf& txIns, const ConstBuf& spend, const ConstBuf& data) {
	ASSERT(height < 0xFFFFFF);

	CoinEng& eng = Eng();

	MemoryStream msT;
	uint32_t h = htole(height);
	msT.WriteBuffer(&h, BLOCKID_SIZE);
	BinaryWriter(msT).Ref() << data << txIns << spend;

	DbTxRef dbt(m_db);
	try {
		DBG_LOCAL_IGNORE(E_EXT_DB_DupKey);

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
	dbt.CommitIfLocal();
}

void DbliteBlockChainDb::SaveCoinsByTxHash(const HashValue& hash, const vector<bool>& vec) {
	TxDatas txDatas = GetTxDatas(ConstBuf(hash.data(), 8));
	TxData& txData = txDatas.Items[txDatas.Index];
	txData.Coins = find(vec.begin(), vec.end(), true) != vec.end() ? CoinEng::SpendVectorToBlob(vec) : Blob();
	PutTxDatas(TxKey(hash), txDatas);
}


Blob DbliteBlockChainDb::FindPubkey(int64_t id) {
	id = htole(id);
	DbReadTxRef dbt(m_db);
	DbCursor c(dbt, m_tablePubkeys);
	if (!c.Get(ConstBuf(&id, PUBKEYID_SIZE)))
		return Blob(nullptr);				// Blob(c.Data) should be explicit here because else implicit Blob(ConstBuf(Blob(nullptr))) generated
	ConstBuf cbuf = c.Data;
	if (cbuf.Size != 16)
		return cbuf;
	Blob r(0, 20);
	byte *p = (byte*)memcpy(r.data(), cbuf.P, 12);
	*(uint32_t*)(p+12) = *(uint32_t*)&id;
	*(uint32_t*)(p+16) = *(uint32_t*)(cbuf.P+12);
	return r; 
}

void DbliteBlockChainDb::InsertPubkey(int64_t id, const ConstBuf& pk) {
	id = htole(id);
	DbTxRef dbt(m_db);
	if (pk.Size != 20)
		m_tablePubkeys.Put(dbt, ConstBuf(&id, PUBKEYID_SIZE), pk);
	else {	
		ASSERT(!memcmp(&id, pk.P+12, 4));
		byte buf[16];
		memcpy(buf, pk.P, 12);
		*(uint32_t*)(buf+12) = *(uint32_t*)(pk.P+16);
		m_tablePubkeys.Put(dbt, ConstBuf(&id, PUBKEYID_SIZE), ConstBuf(buf, 16));
	}
	dbt.CommitIfLocal();
}

void DbliteBlockChainDb::UpdatePubkey(int64_t id, const ConstBuf& pk) {
	id = htole(id);
	DbTxRef dbt(m_db);
	m_tablePubkeys.Put(dbt, ConstBuf(&id, PUBKEYID_SIZE), pk);		
	dbt.CommitIfLocal();
}




} // Coin::

#endif // UCFG_COIN_COINCHAIN_BACKEND == COIN_BACKEND_DBLITE
