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

#ifdef _DEBUG//!!!D
//	m_db.UseMMapPager = false;
#endif

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


const ConstBuf MAX_HEIGHT_KEY("MaxHeight", 9);

int DbliteBlockChainDb::GetMaxHeight() {
	DbReadTxRef dbt(m_db);
	if (m_db.UserVersion < VER_BLOCKS_TABLE_IS_HASHTABLE) {
		DbCursor c(dbt, m_tableBlocks);
		if (c.Seek(CursorPos::Last))
			return BlockKey(c.Key);
	} else {
		DbCursor c(dbt, m_tableProperties);
		if (c.SeekToKey(MAX_HEIGHT_KEY))
			return letoh(*(int32_t*)c.get_Data().P);
	}
	return -1;
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

uint64_t DbliteBlockChainDb::GetBlockOffset(int height) {
	DbReadTxRef dbt(m_db);
	DbCursor c(dbt, m_tableBlocks);
	if (!c.Get(BlockKey(height)))
		Throw(E_COIN_InconsistentDatabase);
	return letoh(*(uint64_t UNALIGNED *)c.get_Data().P);
}

void DbliteBlockChainDb::OpenBootstrapFile(const path& dir) {
	File::OpenInfo oi;
	oi.Path = dir / "bootstrap.dat";
	oi.Mode = FileMode::OpenOrCreate;
	oi.Share = FileShare::Read;
	oi.Options = FileOptions::RandomAccess;
	m_fileBootstrap.Open(oi);
	int h = GetMaxHeight();
	if (h != -1)
		PositionOwningFileStream stm(m_fileBootstrap, GetBlockOffset(h));
		Block block;
		BinaryReader(stm) >> block;
		Eng().OffsetInBootstrap = stm.Position;
	}

	MappedSize = 0;
#if UCFG_CPU_X86_X64
	if (m_fileBootstrap.Length > MIN_MM_LENGTH) {
		m_mmBootstrap = MemoryMappedFile::CreateFromFile(m_fileBootstrap, nullptr, 0, MemoryMappedFileAccess::Read);
		m_viewBootstrap = m_mmBootstrap.CreateView(0, MappedSize = m_fileBootstrap.Length & ~0xFFFFull, MemoryMappedFileAccess::Read);
	}
#endif	
}

bool DbliteBlockChainDb::Create(const path& p) {
	CoinEng& eng = Eng();
	
	if (eng.Mode==EngMode::Lite || eng.Mode==EngMode::BlockParser)
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
	if (eng.Mode == EngMode::Bootstrap)
		OpenBootstrapFile(p.parent_path());

	if (TryToConvert(p))
		return false;

	return true;
}

bool DbliteBlockChainDb::Open(const path& p) {
	CoinEng& eng = Eng();

	if (eng.Mode==EngMode::Lite || eng.Mode==EngMode::BlockParser)
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
	if (eng.Mode == EngMode::Bootstrap)
		OpenBootstrapFile(p.parent_path());
	if (TryToConvert(p))
		return false;
	return true;
}

void DbliteBlockChainDb::Commit() {
	if (Eng().Mode == EngMode::Bootstrap)
		m_fileBootstrap.Flush();
	//		m_dbt->Commit();
	//		m_dbt.reset();
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

	m_viewBootstrap.Unmap();
	m_mmBootstrap.Close();
	m_fileBootstrap.Close();
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

	Block r;
	r.m_pimpl->Height = height;
	if (eng.Mode == EngMode::Bootstrap) {
		uint64_t off = letoh(*(uint64_t UNALIGNED *)cbuf.P);
		PositionOwningFileStream stm(m_fileBootstrap, off);
		r.Read(BinaryReader(stm));
		r.m_pimpl->OffsetInBootstrap = off;
	} else {
		CMemReadStream msBlock(cbuf);
		BinaryReader rd(msBlock);

		Blob blob;
		rd >> blob;
		BlockHashValue hash(blob);

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
	}	
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

vector<uint32_t> DbliteBlockChainDb::InsertBlock(const Block& block, const ConstBuf& data, const ConstBuf& txData) {
	vector<uint32_t> txOffsets;

	int height = block.Height;
	ASSERT(height < 0xFFFFFF);
	HashValue hash = Hash(block);
	BlockKey blockKey(height);
	MemoryStream msB;
	BinaryWriter wrT(msB);

	CoinEng& eng = Eng();
	if (eng.Mode == EngMode::Bootstrap) {
		MemoryStream ms;
		BinaryWriter wr(ms);
		wr << eng.ChainParams.ProtocolMagic << uint32_t(0);
		block.m_pimpl->WriteHeader(wr);
		const CTxes& txes = block.m_pimpl->get_Txes();
		txOffsets.resize(txes.size());
		CoinSerialized::WriteVarInt(wr, txes.size());
		for (size_t i=0; i<txes.size(); ++i) {
			txOffsets[i] = uint32_t(ms.Position - 8);
			wr << txes[i];
		}
		block.m_pimpl->WriteSuffix(wr);
		Blob blob = ms;
		*(uint32_t*)(blob.data() + 4) = htole(uint32_t(blob.Size - 8));

		if (!block.m_pimpl->OffsetInBootstrap) {
			m_fileBootstrap.Write(blob.constData(), blob.Size, OffsetInBootstrap);
//!!!?T			m_fileBootstrap.Flush();
			block.m_pimpl->OffsetInBootstrap = OffsetInBootstrap + 8;
			OffsetInBootstrap += blob.Size;
		}
		wrT << block.m_pimpl->OffsetInBootstrap;
	} else
		wrT << ConstBuf(ReducedBlockHash(hash)) << data << txData;
	DbTxRef dbt(m_db);
	m_tableBlocks.Put(dbt, blockKey, msB, true);
	m_tableHashToBlock.Put(dbt, ReducedBlockHash(hash), blockKey, true);
	if (m_db.UserVersion >= VER_BLOCKS_TABLE_IS_HASHTABLE) {
		int32_t h = htole(int32_t(height));
		m_tableProperties.Put(dbt, MAX_HEIGHT_KEY, ConstBuf(&h, sizeof h));
	}
	dbt.CommitIfLocal();
	return txOffsets;
}

void DbliteBlockChainDb::TxData::Read(BinaryReader& rd) {
	uint32_t h = 0;
	rd.BaseStream.ReadBuffer(&h, BLOCKID_SIZE);
	Height = letoh(h);
	switch (Eng().Mode) {
	case EngMode::Bootstrap:
		Data.Size = 3;
		rd.BaseStream.ReadBuffer(Data.data(), 3);
		break;
	default:
		rd >> Data >> TxIns;
	}
	rd >> Coins;
}

void DbliteBlockChainDb::TxData::Write(BinaryWriter& wr, bool bWriteHash) const {
	uint32_t h = htole(Height);
	wr.BaseStream.WriteBuffer(&h, BLOCKID_SIZE);
	switch (Eng().Mode) {
	case EngMode::Bootstrap:
		wr.BaseStream.WriteBuffer(Data.constData(), 3);
		break;
	default:
		wr << Data << TxIns;
	}
	wr << Coins;
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
		for (size_t i=0; i<txDatas.Items.size(); ++i) {
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

static const size_t AVG_TX_SIZE = 400;

void DbliteBlockChainDb::ReadTx(uint64_t off, Tx& tx) {
	if (off+MAX_BLOCK_SIZE < MappedSize)
		BinaryReader(CMemReadStream(ConstBuf((byte*)m_viewBootstrap.Address + off, MAX_BLOCK_SIZE))) >> tx;
	else
		BinaryReader(BufferedStream(PositionOwningFileStream(m_fileBootstrap, off), AVG_TX_SIZE)) >> tx;
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
		switch (eng.Mode) {
		case EngMode::Bootstrap:
			ReadTx(GetBlockOffset(txData.Height) + letoh(*(uint32_t*)txData.Data.constData()), *ptx);
			break;
		default:
			CMemReadStream stm(txData.Data);
			DbReader rd(stm, &eng);
			rd >> *ptx;
		}
		ptx->SetReducedHash(txid8);
	}
	return true;
}

bool DbliteBlockChainDb::FindTx(const HashValue& hash, Tx *ptx) {
	bool r = FindTxById(ConstBuf(hash.data(), 8), ptx);
	if (r && ptx)
		ptx->SetHash(hash);
	return r;
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
	BinaryWriter wr(msT);
	uint32_t h = htole(height);
	msT.WriteBuffer(&h, BLOCKID_SIZE);
	switch (eng.Mode) {
	case EngMode::Bootstrap:
		ASSERT(data.Size == 3);
		msT.WriteBuffer(data.P, 3);
		break;
	default:
		wr << data << txIns;
	}
	wr << spend;

	DbTxRef dbt(m_db);
	try {
		DBG_LOCAL_IGNORE(E_EXT_DB_DupKey);

		m_tableTxes.Put(dbt, TxKey(txHash), msT, true);
	} catch (DbException& ex) {
		TxDatas txDatas;

		if (FindTxDatas(ConstBuf(txHash.data(), 8), txDatas)) {
			if (txDatas.Items.size() == 1) {
				if (eng.Mode == EngMode::Bootstrap) {
					Tx txOld;
					ReadTx(GetBlockOffset(txDatas.Items[0].Height) + letoh(*(uint32_t*)txDatas.Items[0].Data.constData()), txOld);
					if (Hash(txOld) != txHash) {
						txDatas.Items[0].HashTx = Hash(txOld);
						goto LAB_FOUND;
					}
				} else if (ConstBuf(txDatas.Items[0].TxIns) != txIns || ConstBuf(txDatas.Items[0].Data) != data)
					goto LAB_OTHER_TX;
			}
			TxData& txData = txDatas.Items[txDatas.Index];
			txData.Coins = spend;

			String msg = ex.what(); //!!!D
			TRC(1, "Duplicated Transaction: " << txHash);

			if (height >= eng.ChainParams.CheckDupTxHeight && ContainsInLinear(GetCoinsByTxHash(txHash), true))
				Throw(E_COIN_DupNonSpentTx);
			PutTxDatas(TxKey(txHash), txDatas);
			goto LAB_END;
		}
LAB_OTHER_TX:
		ASSERT(!txDatas.Items.empty() && eng.Mode != EngMode::Bootstrap);
		if (txDatas.Items.size() == 1) {
			TxData& t = txDatas.Items[0];
			TxHashesOutNums hons = t.Height==height ? hashesOutNums : GetTxHashesOutNums(t.Height);
			for (size_t i=0; i<hons.size(); ++i) {
				if (!memcmp(hons[i].HashTx.data(), txHash.data(), TXID_SIZE)) {
					t.HashTx = hons[i].HashTx;
					goto LAB_FOUND;
				}
			}
			Throw(E_COIN_InconsistentDatabase);
		}
LAB_FOUND:
		{
			TxData txData;
			txData.Height = height;
			txData.Data = data;
			txData.TxIns = txIns;
			txData.Coins = spend;
			txData.HashTx = txHash;
			txDatas.Items.push_back(txData);
			PutTxDatas(TxKey(txHash), txDatas);
		}
LAB_END:
		;
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
