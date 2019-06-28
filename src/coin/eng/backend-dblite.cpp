/*######   Copyright (c) 2013-2019 Ufasoft  http://ufasoft.com  mailto:support@ufasoft.com,  Sergey Pavlov  mailto:dev@ufasoft.com ####
#                                                                                                                                     #
# 		See LICENSE for licensing information                                                                                         #
#####################################################################################################################################*/

#include <el/ext.h>

#include "param.h"

#if UCFG_WIN32
#	include <memoryapi.h>

#	include <el/libext/win32/ext-win.h>
#	include <el/win/policy.h>
#endif


#if UCFG_COIN_COINCHAIN_BACKEND == COIN_BACKEND_DBLITE

#	include "backend-dblite.h"

namespace Ext {

template <> EXT_THREAD_PTR(Ext::DB::KV::DbReadTransaction) CThreadTxRef<Ext::DB::KV::DbReadTransaction>::t_pTx;
}

namespace Coin {

const Version VER_BLOCKS_TABLE_IS_HASHTABLE(0, 81), VER_PUBKEY_RECOVER(0, 90), VER_HEADERS(0, 120);

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

DbliteBlockChainDb::DbliteBlockChainDb(CoinEng& eng)
	: Eng(eng)
	//	, m_tableBlocks("blocks", BLOCKID_SIZE, TableType::HashTable, HashType::Identity)
	, m_tableBlocks("blocks", BLOCKID_SIZE, TableType::BTree) // ordered					//!!!T
	, m_tableHashToBlock("hash_blocks", 0, TableType::HashTable)
	, m_tableTxes("txes", TXID_SIZE, TableType::HashTable, HashType::Identity)
	, m_tablePubkeys("pubkeys", PUBKEYID_SIZE, TableType::HashTable, HashType::Identity)
	, m_tablePubkeyToTxes("pubkey_txes", PUBKEYTOTXES_ID_SIZE, TableType::HashTable, HashType::Identity)
	, m_tableProperties("properties", 0, TableType::HashTable) {
	DefaultFileExt = ".udb";

#	ifdef _DEBUG //!!!D
//	m_db.UseMMapPager = false;
#	endif

	m_db.AsyncClose = true;
	m_db.Durability = false; // Blockchain DB always can be restored from the Net, so we need not guarantee Commits

	m_db.ProtectPages = false; //!!!?
	//	m_db.CheckpointPeriod = TimeSpan::FromMinutes(5);
}

class SavepointTransaction : public DbTransaction {
	typedef DbTransaction base;

public:
	DbliteBlockChainDb& m_bcdb;

	SavepointTransaction(DbliteBlockChainDb& bcdb)
		: base(bcdb.m_db)
		, m_bcdb(bcdb)
	{
		m_bcdb.MtxDb.lock();
		ASSERT(!DbReadTxRef::t_pTx);
		DbReadTxRef::t_pTx = this;
	}

	~SavepointTransaction() {
		DbReadTxRef::t_pTx = nullptr;
		m_bcdb.MtxDb.unlock();
	}

	void BeginTransaction() override {}

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

#define COIN_DEF_DB_KEY(name) const Span KEY_##name((const uint8_t*)#name, strlen(#name));

COIN_DEF_DB_KEY(MaxHeight);
COIN_DEF_DB_KEY(MaxHeaderHeight);
COIN_DEF_DB_KEY(BootstrapOffset);
COIN_DEF_DB_KEY(LastPrunedHeight);


int DbliteBlockChainDb::GetMaxHeight() {
	DbReadTxRef dbt(m_db);
	if (m_db.UserVersion < VER_BLOCKS_TABLE_IS_HASHTABLE) {
		DbCursor c(dbt, m_tableBlocks);
		if (c.Seek(CursorPos::Last))
			return BlockKey(c.Key);
	} else {
		DbCursor c(dbt, m_tableProperties);
		if (c.SeekToKey(KEY_MaxHeight))
			return (int)GetLeUInt32(c.get_Data().data());
	}
	return -1;
}

int DbliteBlockChainDb::GetMaxHeaderHeight() {
	DbReadTxRef dbt(m_db);
	DbCursor c(dbt, m_tableProperties);
	if (c.SeekToKey(KEY_MaxHeaderHeight))
		return (int)GetLeUInt32(c.get_Data().data());
	return -1;
}

uint64_t DbliteBlockChainDb::GetBoostrapOffset() {
	DbReadTxRef dbt(m_db);
	DbCursor c(dbt, m_tableProperties);
	return c.SeekToKey(KEY_BootstrapOffset) ? GetLeUInt64(c.get_Data().data()) : 0;
}

void DbliteBlockChainDb::SetBoostrapOffset(uint64_t v) {
	DbTxRef dbt(m_db);
	v = htole(v);
	m_tableProperties.Put(dbt, KEY_BootstrapOffset, Span((const uint8_t*)&v, sizeof v));
}

int32_t DbliteBlockChainDb::GetLastPrunedHeight() {
	DbReadTxRef dbt(m_db);
	DbCursor c(dbt, m_tableProperties);
	return c.SeekToKey(KEY_LastPrunedHeight) ? (int)GetLeUInt32(c.get_Data().data()) : 0;
}

void DbliteBlockChainDb::SetLastPrunedHeight(int32_t height) {
	DbTxRef dbt(m_db);
	height = htole(height);
	m_tableProperties.Put(dbt, KEY_LastPrunedHeight, Span((const uint8_t*)& height, sizeof height));
}


#	if UCFG_COIN_CONVERT_TO_UDB
class ConvertDbThread : public Thread {
	typedef Thread base;

public:
	DbliteBlockChainDb& Db;
	CoinEng& Eng;
	path Path;
	ptr<CoinEng> EngSqlite;

	ConvertDbThread(DbliteBlockChainDb& db, CoinEng& eng, const path& p)
		: base(&eng.m_tr)
		, Eng(eng)
		, Db(db)
		, Path(p) {}

protected:
	void Execute() override {
		Name = "ConvertDbThread";

		String pathSqlite = Path::ChangeExtension(Path, ".db");
		CCoinEngThreadKeeper engKeeper(&Eng);
		int n = EngSqlite->Db->GetMaxHeight() + 1;
		if (n > 0) {
			Eng.UpgradingDatabaseHeight = n;
			for (int i = Eng.Db->GetMaxHeight() + 1; i < n && !m_bStop; ++i) {
				Eng.Events.OnSetProgress((float)i);
				Eng.Events.OnChange();

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
				EXT_LOCK(Eng.Mtx) { block.Process(0); }
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
#	endif // UCFG_COIN_CONVERT_TO_UDB

bool DbliteBlockChainDb::TryToConvert(const path& p) {
#	if UCFG_COIN_CONVERT_TO_UDB
	CoinEng& eng = Eng();
	if (eng.ChainParams.Name == "Bitcoin" || eng.ChainParams.Name == "Bitcoin-testnet2" || eng.ChainParams.Name == "LiteCoin") { // convert only huge blockchains
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
#	endif // UCFG_COIN_CONVERT_TO_UDB
	return false;
}

uint64_t DbliteBlockChainDb::GetBlockOffset(int height) {
	DbReadTxRef dbt(m_db);
	DbCursor c(dbt, m_tableBlocks);
	if (!c.Get(BlockKey(height)))
		Throw(CoinErr::InconsistentDatabase);
	return GetLeUInt64(c.get_Data().data());
}

void DbliteBlockChainDb::OpenBootstrapFile(const path& dir) {
	File::OpenInfo oi;
	oi.Path = Eng.GetBootstrapPath();
	oi.Mode = FileMode::OpenOrCreate;
	oi.Share = FileShare::Read;
	oi.Options = FileOptions::RandomAccess;
	m_fileBootstrap.Open(oi);
	int h = GetMaxHeight();
	if (h != -1) {
		PositionOwningFileStream stm(m_fileBootstrap, GetBlockOffset(h));
		Block block;
        block.Read(ProtocolReader(stm, true));
		Eng.OffsetInBootstrap = stm.Position;
	}

	MappedSize = 0;
#if UCFG_PLATFORM_X64
	uint64_t fileLen = m_fileBootstrap.Length;
	if (fileLen >= MIN_MM_LENGTH) {
		MappedSize = fileLen & ~0xFFFFuLL;
		m_mmBootstrap = MemoryMappedFile::CreateFromFile(m_fileBootstrap, nullptr, 0);
		m_viewBootstrap = m_mmBootstrap.CreateView(0, MappedSize, m_mmBootstrap.Access);
	}
#endif // UCFG_PLATFORM_X64
}

void DbliteBlockChainDb::BeforeDbOpenCreate() {
	switch (Eng.Mode) {
	case EngMode::Normal:
	case EngMode::BlockExplorer:
		m_db.UseFlush = false; //!!!? this optimization lowers DB reliability, but improves performance for huge DBs
		break;
	default: m_db.UseFlush = true; break;
	}
}

bool DbliteBlockChainDb::Create(const path& p) {
	BeforeDbOpenCreate();
	m_db.AppName = "Coin";
	m_db.UserVersion = Version(VER_PRODUCTVERSION);
	m_db.Create(p);
	{
		DbTransaction dbt(m_db);
		m_tableBlocks.Open(dbt, true);
		m_tableHashToBlock.Open(dbt, true);
		m_tableTxes.Open(dbt, true);
		m_tablePubkeys.Open(dbt, true);
		m_tableProperties.Open(dbt, true);
		if (Eng.Mode == EngMode::BlockExplorer) {
			m_tablePubkeyToTxes.Open(dbt, true);
		}
		OnOpenTables(dbt, true);
		dbt.Commit();
	}
	m_db.Checkpoint();
	if (Eng.Mode == EngMode::Bootstrap)
		OpenBootstrapFile(p.parent_path());

	if (TryToConvert(p))
		return false;

	return true;
}

bool DbliteBlockChainDb::Open(const path& p) {
	BeforeDbOpenCreate();
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
		if (Eng.Mode == EngMode::BlockExplorer) {
			m_tablePubkeyToTxes.Open(dbt);
		}
		OnOpenTables(dbt, false);
	}
	if (Eng.Mode == EngMode::Bootstrap)
		OpenBootstrapFile(p.parent_path());
	if (TryToConvert(p))
		return false;
	return true;
}

void DbliteBlockChainDb::Commit() {
#ifdef _DEBUG//!!!T
	return;
#endif
	if (Eng.Mode == EngMode::Bootstrap)
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

void DbliteBlockChainDb::Checkpoint() {
	m_db.Checkpoint();
}

Version DbliteBlockChainDb::CheckUserVersion() {
	if (m_db.AppName != "Coin") {
		m_db.Close();
		throw Exception(E_FAIL, "This database has AppName \"" + m_db.AppName + "\", but must be \"Coin\"");
	}

	Version ver(VER_PRODUCTVERSION), dver = m_db.UserVersion;
	if (dver > ver && (dver.Major != ver.Major || dver.Minor / 10 != ver.Minor / 10)) { // Versions [mj.x0 .. mj.x9] are DB-compatible
		m_db.Close();
		throw VersionException(dver);
	}
	return dver;
}

void DbliteBlockChainDb::UpgradeTo(const Version& ver) {
	Version dbver = CheckUserVersion();
	if (dbver < ver) {
		TRC(0, "Upgrading Database " << Eng.GetDbFilePath() << " from version " << dbver << " to " << ver);

		DbTransaction dbtx(m_db);

		if (dbver < VER_BLOCKS_TABLE_IS_HASHTABLE) {
			m_tableProperties.Open(dbtx, true);
			int32_t h = htole(int32_t(GetMaxHeight()));
			m_tableProperties.Put(dbtx, KEY_MaxHeight, Span((const uint8_t*)&h, sizeof h));
		}

		if (dbver < VER_HEADERS) {
			int32_t h = htole(int32_t(GetMaxHeight()));
			m_tableProperties.Put(dbtx, KEY_MaxHeaderHeight, Span((const uint8_t*)&h, sizeof h));
		}

		Eng.UpgradeDb(ver);
		m_db.SetUserVersion(ver);
		dbtx.Commit();
		m_db.Checkpoint();
	}
}

void DbliteBlockChainDb::TryUpgradeDb(const Version& verApp) {
	if (Eng.Mode != EngMode::Bootstrap && CheckUserVersion() < VER_HEADERS)
		Recreate();
	else
		UpgradeTo(VER_HEADERS);
}

HashValue DbliteBlockChainDb::ReadPrevBlockHash(DbReadTransaction& dbt, int height, bool bFromBlock) {
	if (height == 0)
		return HashValue::Null();
	DbCursor c(dbt, m_tableBlocks);
	if (!c.Get(BlockKey(height - 1)))
		Throw(CoinErr::InconsistentDatabase);
	Stream& msPrev = c.DataStream;
	BinaryReader rdDb(msPrev);
	if (bFromBlock && Eng.Mode == EngMode::Bootstrap) {
		PositionOwningFileStream stmFile(m_fileBootstrap, rdDb.ReadUInt64());
		BlockHeader header(Eng.CreateBlockObj());
		header.m_pimpl->ReadHeader(ProtocolReader(stmFile), false, 0);
		return Hash(header);
	} else {
		Blob blob;
		rdDb >> blob;
		return BlockHashValue(blob);
	}
}

Block DbliteBlockChainDb::LoadBlock(DbReadTransaction& dbt, int height, Stream& stmBlocks, bool bFullRead) {
	Block r;
	r.m_pimpl->Height = height;
	BinaryReader rdDb(stmBlocks);
	if (Eng.Mode == EngMode::Bootstrap) {
		uint64_t off = rdDb.ReadUInt64();
		PositionOwningFileStream stmFile(m_fileBootstrap, off);
		BufferedStream stm(stmFile);
		ProtocolReader rd(stm, true);
		if (bFullRead)
			r.Read(rd);
		else
			r.m_pimpl->ReadHeader(rd, false, 0);
		r.m_pimpl->OffsetInBootstrap = off;
		r.m_pimpl->ReadDbSuffix(rdDb);
	} else {
		Blob blob;
		rdDb >> blob;
		BlockHashValue hash(blob);

		rdDb >> blob;
		CMemReadStream ms(blob);
		r.m_pimpl->Read(DbReader(ms, &Eng));
		r.m_pimpl->m_hash = hash;

		rdDb >> blob;
		r.m_pimpl->m_txHashesOutNums = Coin::TxHashesOutNums(blob);
		r.m_pimpl->PrevBlockHash = ReadPrevBlockHash(dbt, r.Height);

		//!!!	ASSERT(Hash(r) == hash);
	}
	return r;
}

bool DbliteBlockChainDb::HaveHeader(const HashValue& hash) {
	DbReadTxRef dbt(m_db);
	return DbCursor(dbt, m_tableHashToBlock).Get(ReducedBlockHash(hash));
}

BlockHeader DbliteBlockChainDb::LoadHeader(DbReadTransaction& dbt, int height, Stream& stmBlocks, int hMaxBlock) {
	if (hMaxBlock == -2)
		hMaxBlock = GetMaxHeight();
	if (height <= hMaxBlock)
		return LoadBlock(dbt, height, stmBlocks, false);
	BlockHeader r(Eng.CreateBlockObj());
	r.m_pimpl->Height = height;
	DbReader rd(stmBlocks);
	rd.ForHeader = true;
	Blob blob;
	rd >> blob;
	BlockHashValue hash(blob);
	r.m_pimpl->Read(rd);
	r.m_pimpl->PrevBlockHash = ReadPrevBlockHash(dbt, height, (height - 1) <= hMaxBlock);
	ASSERT(Hash(r) == hash);
	r.m_pimpl->m_hash = hash;
	return r;
}

BlockHeader DbliteBlockChainDb::FindHeader(int height) {
	DbReadTxRef dbt(m_db);
	DbCursor c(dbt, m_tableBlocks);
	if (!c.Get(BlockKey(height)))
		Throw(CoinErr::BlockNotFound);
	return LoadHeader(dbt, height, c.DataStream);
}

BlockHeader DbliteBlockChainDb::FindHeader(const HashValue& hash) {
	BlockHeader r(nullptr);
	DbReadTxRef dbt(m_db);
	DbCursor c(dbt, m_tableHashToBlock);
	if (!c.Get(ReducedBlockHash(hash)))
		return r;
	int height = (uint32_t)BlockKey(c.Data);
	DbCursor c1(dbt, m_tableBlocks);
	if (!c1.Get(c.Data)) {
		Throw(CoinErr::InconsistentDatabase);
	}
	return LoadHeader(dbt, height, c1.DataStream);
}

bool DbliteBlockChainDb::HaveBlock(const HashValue& hash) {
	DbReadTxRef dbt(m_db);
	DbCursor c(dbt, m_tableHashToBlock);
	if (!c.Get(ReducedBlockHash(hash)))
		return false;
	int height = (uint32_t)BlockKey(c.Data);
	return height <= GetMaxHeight();
}

Block DbliteBlockChainDb::FindBlock(const HashValue& hash) {
	Block r(nullptr);
	DbReadTxRef dbt(m_db);
	DbCursor c(dbt, m_tableHashToBlock);
	if (c.Get(ReducedBlockHash(hash))) {
		int height = (uint32_t)BlockKey(c.Data);
		if (height < GetMaxHeight()) {
			DbCursor c1(dbt, m_tableBlocks);
			if (!c1.Get(c.Data)) {
				Throw(CoinErr::InconsistentDatabase);
			}
			r = LoadBlock(dbt, height, c1.DataStream);
		}
	}
	return r;
}

Block DbliteBlockChainDb::FindBlock(int height) {
	DbReadTxRef dbt(m_db);
	DbCursor c(dbt, m_tableBlocks);
	if (!c.Get(BlockKey(height)))
		Throw(CoinErr::BlockNotFound);
	return LoadBlock(dbt, height, c.DataStream);
}

vector<BlockHeader> DbliteBlockChainDb::GetBlockHeaders(const LocatorHashes& locators, const HashValue& hashStop) {
	DbReadTxRef dbt(m_db);
	int hMaxBlock = GetMaxHeight();
	DbCursor c(dbt, m_tableBlocks);

	vector<BlockHeader> r;
	for (int height = locators.FindHeightInMainChain() + 1; c.Get(BlockKey(height)); ++height) {
		BlockHeader header = LoadHeader(dbt, height, c.DataStream, hMaxBlock);
		r.push_back(header);
		if (Hash(header) == hashStop || r.size() >= MAX_HEADERS_RESULTS)
			break;
	}
	return r;
}

Block DbliteBlockChainDb::FindBlockPrefixSuffix(int height) {
	DbReadTxRef dbt(m_db);

	DbCursor c(dbt, m_tableBlocks);
	if (!c.Get(BlockKey(height)))
		Throw(CoinErr::BlockNotFound);
	return LoadBlock(dbt, height, c.DataStream, false);
}

TxHashesOutNums DbliteBlockChainDb::GetTxHashesOutNums(int height) {
	DbReadTxRef dbt(m_db);
	DbCursor c(dbt, m_tableBlocks);
	if (!c.Get(BlockKey(height)))
		Throw(CoinErr::InconsistentDatabase);
	Blob blob;
	BinaryReader(c.DataStream) >> blob >> blob >> blob;
	return TxHashesOutNums(blob);
}

pair<OutPoint, TxOut> DbliteBlockChainDb::GetOutPointTxOut(int height, int idxOut) {
	DbReadTxRef dbt(m_db);
	DbCursor c(dbt, m_tableBlocks);
	if (!c.Get(BlockKey(height)))
		Throw(CoinErr::InconsistentDatabase);
	Stream& stm = c.DataStream;
	//	CMemReadStream stm(c.Data);
	Blob blob;
	BinaryReader rd(stm);
	rd >> blob >> blob >> blob;

	OutPoint outPoint;
	CMemReadStream stmHashes(blob);
	BinaryReader rdHashes(stmHashes);
	int nTx;
	for (nTx = 0; !rdHashes.BaseStream.Eof(); ++nTx) {
		HashValue hashTx;
		rdHashes >> hashTx;
		uint32_t nOuts = (uint32_t)rdHashes.Read7BitEncoded();
		if (idxOut < nOuts) {
			outPoint = OutPoint(hashTx, idxOut);
			break;
		}
		idxOut -= nOuts;
	}

	for (int i = 0; i < nTx; ++i)
		rd >> blob >> blob;
	rd >> blob;
	Tx tx;
	tx.EnsureCreate(Eng);
	DbReader(CMemReadStream(blob), &Eng) >> tx;
	return make_pair(outPoint, tx.TxOuts()[outPoint.Index]);
}

void DbliteBlockChainDb::InsertHeader(const BlockHeader& header) {
	int32_t height = header.Height;
	ASSERT(height < 0xFFFFFF);
	HashValue hash = Hash(header);
	BlockKey blockKey(height);
	MemoryStream msB;
	DbWriter wrT(msB);
	wrT.ForHeader = true;
	DbTxRef dbt(m_db);
	wrT << Span(ReducedBlockHash(hash));
	header.m_pimpl->Write(wrT);
	m_tableBlocks.Put(dbt, blockKey, Span(msB), true);
	m_tableHashToBlock.Put(dbt, ReducedBlockHash(hash), blockKey, true);
	int32_t h = htole(height);
	m_tableProperties.Put(dbt, KEY_MaxHeaderHeight, Span((const uint8_t*)&h, sizeof h));

	dbt.CommitIfLocal();
}

void DbliteBlockChainDb::TxData::Read(BinaryReader& rd, CoinEng& eng) {
	uint32_t h = 0;
	rd.BaseStream.ReadBuffer(&h, BLOCKID_SIZE);
	Height = letoh(h);
	switch (eng.Mode) {
#	if UCFG_COIN_TXES_IN_BLOCKTABLE
	case EngMode::Normal:
	case EngMode::BlockExplorer:
		rd >> N;
#	endif
	case EngMode::Bootstrap:
		Data.resize(3);
		rd.BaseStream.ReadBuffer(Data.data(), 3);
		break;
	default:
		rd >> Data >> TxIns;
	}
	if (rd.BaseStream.Eof())
		Coins = Blob();
	else {
		rd >> Coins;
		if (Coins.empty())
			LastSpendHeight = uint32_t(Height + rd.Read7BitEncoded());
	}
}

void DbliteBlockChainDb::TxData::Write(BinaryWriter& wr, CoinEng& eng, bool bWriteHash) const {
	uint32_t h = htole(Height);
	wr.BaseStream.WriteBuffer(&h, BLOCKID_SIZE);
	switch (eng.Mode) {
#	if UCFG_COIN_TXES_IN_BLOCKTABLE
	case EngMode::Normal:
	case EngMode::BlockExplorer:
#	endif
		wr << N;
	case EngMode::Bootstrap:
		wr.BaseStream.WriteBuffer(Data.constData(), 3);
		break;
	default: wr << Data << TxIns;
	}
	wr << Coins;
	if (Coins.empty())
		wr.Write7BitEncoded(LastSpendHeight - Height);
	if (bWriteHash)
		wr << HashTx;
}

DbliteBlockChainDb::TxDatas DbliteBlockChainDb::FindTxDatas(DbCursor& c, RCSpan txid8) {
	if (!c.Get(TxKey(txid8)))
		return TxDatas();
	CMemReadStream msT(c.Data);
	BinaryReader rdT(msT);
	TxDatas txDatas(1);
	txDatas.Items[0].Read(rdT, Eng);
	if (msT.Eof()) {
		txDatas.Index = 0;
		return txDatas;
	} else {
		rdT >> txDatas.Items[0].HashTx;
		while (!msT.Eof()) {
			txDatas.Items.resize(txDatas.Items.size() + 1);
			txDatas.Items.back().Read(rdT, Eng);
			rdT >> txDatas.Items.back().HashTx;
		}
		for (size_t i = 0; i < txDatas.Items.size(); ++i) {
			TxData& d = txDatas.Items[i];
			if (!memcmp(d.HashTx.data(), txid8.data(), 8)) {
				txDatas.Index = i;
				return txDatas;
			}
		}
		return TxDatas();
	}
}

DbliteBlockChainDb::TxDatas DbliteBlockChainDb::GetTxDatas(RCSpan txid8) {
	DbReadTxRef dbt(m_db);
	DbCursor c(dbt, m_tableTxes);

	TxDatas txDatas = FindTxDatas(c, txid8);
	if (!txDatas)
		Throw(CoinErr::InconsistentDatabase);
	return txDatas;
}

DbliteBlockChainDb::TxDatas DbliteBlockChainDb::GetTxDatas(const HashValue& hashTx) {
	return GetTxDatas(Span(hashTx.data(), 8));
}

void DbliteBlockChainDb::PutTxDatas(DbCursor& c, RCSpan txKey, const TxDatas& txDatas, bool bUpdate) {
	ASSERT(!txDatas.Items.empty() || bUpdate);

	// Don't remove fully spent TxDatas because Reorganize may need them
	if (!txDatas.Items.empty()) {
		MemoryStream ms;
		BinaryWriter wr(ms);
		if (txDatas.Items.size() == 1)
			txDatas.Items[0].Write(wr, Eng, false);
		else {
			EXT_FOR (const TxData& txData, txDatas.Items) {
				txData.Write(wr, Eng, true);
			}
		}
		if (bUpdate)
			c.Update(Span(ms));
		else
			c.Put(txKey, Span(ms));
	}
}

pair<int, int> DbliteBlockChainDb::FindPrevTxCoords(DbWriter& wr, int height, const HashValue& hash) {
	DbReadTxRef dbt(m_db);

	TxDatas txDatas = GetTxDatas(hash);
	int heightOut = txDatas.Items[txDatas.Index].Height;
	wr.Write7BitEncoded(height - heightOut + 1);

	DbCursor c1(dbt, m_tableBlocks);
	if (!c1.Get(BlockKey(heightOut))) {
		Throw(CoinErr::InconsistentDatabase);
	}
	CMemReadStream ms(c1.Data);
	Blob blob;
	BinaryReader(ms) >> blob >> blob >> blob;
	pair<int, int> pp = TxHashesOutNums(blob).StartingTxOutIdx(hash);
	if (pp.second < 0)
		Throw(CoinErr::InconsistentDatabase);
	return pp;
}

static optional<HashValue160> TryExtractHash160(const TxOut& txOut) {
	Address a = TxOut::CheckStandardType(txOut.get_ScriptPubKey());
	switch (a.Type) {
	case AddressType::PubKey:
		return Hash160(a.Data());
	case AddressType::P2PKH:
	case AddressType::P2SH:
		return HashValue160(a.Data());
	default:
		return nullopt;
	}
}

void DbliteBlockChainDb::InsertPubkeyToTxes(DbTransaction& dbTx, const Tx& tx) {
	unordered_set<HashValue160> pubkeys;

	HashValue hashTx = Hash(tx);
	EXT_FOR(const TxOut& txOut, tx.TxOuts()) {
		optional<HashValue160> o = TryExtractHash160(txOut);
		if (o)
			pubkeys.insert(o.value());
	}
	EXT_FOR(const TxIn& txIn, tx.TxIns()) {
		if (txIn.PrevOutPoint.Index >= 0) {
			Tx txPrev = Tx::FromDb(txIn.PrevOutPoint.TxHash);
			optional<HashValue160> o = TryExtractHash160(txPrev.TxOuts()[txIn.PrevOutPoint.Index]);
			if (o)
				pubkeys.insert(o.value());
		}
	}

	EXT_FOR(const HashValue160& pubkey, pubkeys) {
		Span dbkey(pubkey.data(), PUBKEYTOTXES_ID_SIZE);
		DbCursor c(dbTx, m_tablePubkeyToTxes);
		c.PushFront(dbkey, Span(hashTx.data(), 8));

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

void DbliteBlockChainDb::DeleteBlock(int height, const vector<int64_t>* txids) {
	DbTxRef dbt(m_db);
	DbCursor c(dbt, m_tableBlocks);
	if (c.Get(BlockKey(height))) {
		HashValue hashBlock;
		switch (Eng.Mode) {
		case EngMode::Bootstrap: {
			uint64_t off = GetLeUInt64(c.get_Data().data());
			Block block;
			block.Read(ProtocolReader(PositionOwningFileStream(m_fileBootstrap, off)));
			hashBlock = Hash(block);
		} break;
		default:
			Blob blobHash;
			BinaryReader(c.DataStream) >> blobHash;
			hashBlock = BlockHashValue(blobHash);
		}
		c.Delete();
		m_tableHashToBlock.Delete(dbt, ReducedBlockHash(hashBlock));
	}
	int32_t h = htole(height - 1);
	m_tableProperties.Put(dbt, KEY_MaxHeight, Span((const uint8_t*)&h, sizeof h));
	m_tableProperties.Put(dbt, KEY_MaxHeaderHeight, Span((const uint8_t*)&h, sizeof h));
	if (txids) {
		EXT_FOR(int64_t idTx, *txids) {
			idTx = htole(idTx);
			Span txid8((const uint8_t*)&idTx, 8);
			TxDatas txDatas = GetTxDatas(txid8);
			for (int i = txDatas.Items.size(); i--;) {
				if (txDatas.Items[i].Height == height)
					txDatas.Items.erase(txDatas.Items.begin() + i);
			}
			if (txDatas.Items.empty())
				m_tableTxes.Delete(dbt, TxKey(txid8));
			else {
				DbCursor cTxes(dbt, m_tableTxes);
				PutTxDatas(cTxes, TxKey(txid8), txDatas);
			}
		}
	}
	dbt.CommitIfLocal();
}

static const size_t AVG_TX_SIZE = 400;

void DbliteBlockChainDb::ReadTx(uint64_t off, Tx& tx) {
    tx.Read(off + MAX_BLOCK_SIZE < MappedSize
        ? ProtocolReader(CMemReadStream(Span((uint8_t*)m_viewBootstrap.Address + off, MAX_BLOCK_WEIGHT)), true)
        : ProtocolReader(BufferedStream(PositionOwningFileStream(m_fileBootstrap, off), AVG_TX_SIZE), true));
}

bool DbliteBlockChainDb::FindTxByHash(const HashValue& hashTx, Tx* ptx) {
	DbReadTxRef dbt(m_db);
	DbCursor c(dbt, m_tableTxes);

	TxDatas txDatas = FindTxDatas(c, Span(hashTx.data(), 8));
	if (!txDatas)
		return false;
	if (ptx) {
		ptx->EnsureCreate(Eng);

		const TxData& txData = txDatas.Items[txDatas.Index];
		ptx->Height = txData.Height;
		switch (Eng.Mode) {
		case EngMode::Bootstrap: {
			uint32_t off = 0;
			memcpy(&off, txData.Data.constData(), 3);
			ReadTx(GetBlockOffset(txData.Height) + letoh(off), *ptx);
		} break;
		default:
#	if UCFG_COIN_TXES_IN_BLOCKTABLE
			DbCursor cb(dbt, m_tableBlocks);
			if (!cb.Get(BlockKey(txData.Height)))
				Throw(CoinErr::InconsistentDatabase);
			Stream& stm = cb.DataStream;
			BinaryReader rd(stm);
			Blob blob;
			rd >> blob >> blob >> blob;
			CMemReadStream stmHashes(blob);
			BinaryReader rdHashes(stmHashes);
			HashValue h;
			uint32_t nOut = 0;
			for (int nTx = 0; nTx <= txData.N; ++nTx) {
				rdHashes >> h;
				nOut = (uint32_t)rdHashes.Read7BitEncoded();
			}
			stm.Position = ptx->m_pimpl->OffsetInBlock = GetLeUInt32(txData.Data.constData());
			rd >> blob; //!!!TODO Optimize
			CMemReadStream stmTx(blob);
			DbReader drTx(stmTx, &Eng);
			drTx.NOut = nOut;
			drTx >> *ptx;
			ptx->SetHash(h);
#	else
			CMemReadStream stmTx(txData.Data);
			DbReader(stmTx, &eng) >> *ptx;
#	endif
		}
	}
	return true;
}

bool DbliteBlockChainDb::FindTx(const HashValue& hash, Tx* ptx) {
	bool r = FindTxByHash(hash, ptx);
	if (r && ptx) {
		ptx->SetHash(hash);
	}
	return r;
}

void DbliteBlockChainDb::ReadTxes(const BlockObj& bo) {
#if UCFG_COIN_TXES_IN_BLOCKTABLE
	DbReadTxRef dbt(m_db);
	DbCursor c(dbt, m_tableBlocks);
	if (!c.Get(BlockKey(bo.Height)))
		Throw(CoinErr::InconsistentDatabase);
	CMemReadStream stm(c.Data);
	Blob blob;
	BinaryReader rd(stm);
	rd >> blob >> blob >> blob;
	for (int i = 0; i < bo.m_txHashesOutNums.size(); ++i) {
		const TxHashOutNum& ho = bo.m_txHashesOutNums[i];
		Tx& tx = bo.m_txes[i];
		tx.EnsureCreate(Eng);
		tx.Height = bo.Height;
		tx.m_pimpl->OffsetInBlock = uint32_t(stm.Position);
		rd >> blob;
		CMemReadStream stmTx(blob);
		DbReader drTx(stmTx, &Eng);
		drTx.NOut = ho.NOuts;
		drTx >> tx;
		tx.SetHash(ho.HashTx);
	}
#endif
}

void DbliteBlockChainDb::ReadTxIns(const HashValue& hash, const TxObj& txObj) {
#	if UCFG_COIN_TXES_IN_BLOCKTABLE
	DbReadTxRef dbt(m_db);
	DbCursor c(dbt, m_tableBlocks);
	if (!c.Get(BlockKey(txObj.Height)))
		Throw(CoinErr::InconsistentDatabase);
	Stream& stm = c.DataStream;
	stm.Position = txObj.OffsetInBlock;
	Blob blob;
	BinaryReader(stm) >> blob; //!!!TODO Optimize
	CMemReadStream stmTx(blob);
	DbReader drTx(stmTx, &Eng);
	drTx.NOut = txObj.TxOuts.size();
	Tx txDummy;
	drTx >> txDummy;
	txObj.ReadTxIns(drTx);
#	else
	TxDatas txDatas = GetTxDatas(hash);
	txObj.ReadTxIns(DbReader(CMemReadStream(txDatas.Items[txDatas.Index].TxIns), &Eng()));
#	endif
}

vector<int64_t> DbliteBlockChainDb::GetTxesByPubKey(const HashValue160& pubkey) {
	vector<int64_t> r;
	DbReadTxRef dbt(m_db);
	DbCursor c(dbt, m_tablePubkeyToTxes);
	if (c.Get(Span(pubkey.data(), PUBKEYTOTXES_ID_SIZE))) {
		Span data = c.Data;
		r.resize(data.size() / 8);
		memcpy(&r[0], data.data(), data.size());
	}
	return r;
}

vector<bool> DbliteBlockChainDb::GetCoinsByTxHash(const HashValue& hash) {
	TxDatas txDatas = GetTxDatas(hash);
	const TxData& txData = txDatas.Items[txDatas.Index];
	vector<bool> vec;
	for (int j = 0; j < txData.Coins.size(); ++j) {
		uint8_t v = txData.Coins.constData()[j];
		for (int i = 0; i < 8; ++i)
			vec.push_back(v & (1 << i));
	}
	return vec;
}

void DbliteBlockChainDb::SaveCoinsByTxHash(const HashValue& hash, const vector<bool>& vec) {
	TxDatas txDatas = GetTxDatas(hash);
	TxData& txData = txDatas.Items[txDatas.Index];
	txData.Coins = find(vec.begin(), vec.end(), true) != vec.end() ? CoinEng::SpendVectorToBlob(vec) : Blob();

	DbTxRef dbt(m_db);
	DbCursor cTxes(dbt, m_tableTxes);
	PutTxDatas(cTxes, TxKey(hash), txDatas);
	dbt.CommitIfLocal();
}

void DbliteBlockChainDb::UpdateCoins(const OutPoint& op, bool bSpend, int32_t heightCur) {
	Span txid8 = Span(op.TxHash.data(), 8);
	DbTxRef dbt(m_db);
	DbCursor cTxes(dbt, m_tableTxes);

	TxDatas txDatas = FindTxDatas(cTxes, txid8);
	if (!txDatas)
		Throw(CoinErr::InconsistentDatabase);
	TxData& txData = txDatas.Items[txDatas.Index];
	int pos = op.Index >> 3;
	uint8_t mask = 1 << (op.Index & 7);
	uint8_t* p;
	if (bSpend) {
		p = txData.Coins.data();
		if (pos >= txData.Coins.size() || !(p[pos] & mask))
			Throw(CoinErr::InputsAlreadySpent);
		p[pos] &= ~mask;
	} else {
		if (pos >= txData.Coins.size())
			txData.Coins.resize(pos + 1);
		(p = txData.Coins.data())[pos] |= mask;
	}
	size_t i;
	for (i = txData.Coins.size(); i-- && !p[i];)
		;
	if (i + 1 != txData.Coins.size())
		txData.Coins.resize(i + 1);
	if (txData.Coins.size() == 0) {
		if (Eng.Mode == EngMode::Bootstrap || UCFG_COIN_TXES_IN_BLOCKTABLE) {
			SpentTx stx = { op.TxHash, txData.Height, GetLeUInt32(txData.Data.constData()), txData.N };
			auto& caches = Eng.Caches;
			EXT_LOCK(caches.Mtx) {
				caches.m_cacheSpentTxes.push_front(stx);
				if (caches.m_cacheSpentTxes.size() > MAX_LAST_SPENT_TXES)
					caches.m_cacheSpentTxes.pop_back();
			}
		}
		txData.LastSpendHeight = heightCur;
//-		txDatas.Items.erase(txDatas.Items.begin() + txDatas.Index);  // Don't remove fully spent TxDatas because Reorganize may need them
	}

	PutTxDatas(cTxes, TxKey(txid8), txDatas, true);
	dbt.CommitIfLocal();
}

void DbliteBlockChainDb::PruneTxo(const OutPoint& op, int32_t heightCur) {
	DbTxRef dbt(m_db);
	DbCursor cTxes(dbt, m_tableTxes);
	if (TxDatas txDatas = FindTxDatas(cTxes, Span(op.TxHash.data(), 8))) {	// May be has been removed in other txin of this heightCur
		if (!txDatas.Items[txDatas.Index].IsCoinSpent(op.Index))
			Throw(CoinErr::InconsistentDatabase);
		for (auto& d : txDatas.Items)
			if (!d.Coins.empty() || d.LastSpendHeight > heightCur)
				return;
#ifdef _DEBUG	//!!!D
		if (txDatas.Items.size() >= 2) {
			heightCur = heightCur;
		}
#endif
		cTxes.Delete();		// Only when all UTXO are spent
	}
}

void DbliteBlockChainDb::InsertTx(const Tx& tx, uint16_t nTx, const TxHashesOutNums& hashesOutNums, const HashValue& txHash, int height, RCSpan txIns, RCSpan spend, RCSpan data) {
	ASSERT(height < 0xFFFFFF);

	MemoryStream msT;
	BinaryWriter wr(msT);
	uint32_t h = htole(height);
	msT.WriteBuffer(&h, BLOCKID_SIZE);
	switch (Eng.Mode) {
#	if UCFG_COIN_TXES_IN_BLOCKTABLE
	case EngMode::Normal:
	case EngMode::BlockExplorer:
#	endif
		wr << nTx;
	case EngMode::Bootstrap:
		ASSERT(data.size() == 3);
		msT.WriteBuffer(data.data(), 3);
		break;
	default:
		wr << data << txIns;
	}
	wr << spend;

	DbTxRef dbt(m_db);
	try {
		DBG_LOCAL_IGNORE_CONDITION(ExtErr::DB_DupKey);

		m_tableTxes.Put(dbt, TxKey(txHash), msT, true);
	} catch (DbException& ex) {
		if (ex.code() != ExtErr::DB_DupKey)
			throw;

		DbCursor cTxes(dbt, m_tableTxes);
		TxDatas txDatas = FindTxDatas(cTxes, Span(txHash.data(), 8));
		if (txDatas) {
			if (txDatas.Items.size() == 1) {
				TxData& d = txDatas.Items[0];
				if (Eng.Mode == EngMode::Bootstrap) {
					Tx txOld;
					ReadTx(GetBlockOffset(d.Height) + GetLeUInt32(d.Data.constData()), txOld);
					if (Hash(txOld) != txHash) {
						d.HashTx = Hash(txOld);
						goto LAB_FOUND;
					}
				} else if (!Equal(Span(d.TxIns), txIns) || !Equal(Span(d.Data), data))
					goto LAB_OTHER_TX;
			}
			TxData& txData = txDatas.Items[txDatas.Index];
			txData.Coins = spend;

			String msg = ex.what(); //!!!D
			TRC(1, "Duplicated Transaction: " << txHash);

			if (height >= Eng.ChainParams.CheckDupTxHeight && ContainsInLinear(GetCoinsByTxHash(txHash), true))
				Throw(CoinErr::DupNonSpentTx);
			PutTxDatas(cTxes, TxKey(txHash), txDatas);
			goto LAB_END;
		}
	LAB_OTHER_TX:
		ASSERT(!txDatas.Items.empty() && Eng.Mode != EngMode::Bootstrap);
		if (txDatas.Items.size() == 1) {
			TxData& t = txDatas.Items[0];
			TxHashesOutNums hons = t.Height == height ? hashesOutNums : GetTxHashesOutNums(t.Height);
			for (size_t i = 0; i < hons.size(); ++i) {
				if (!memcmp(hons[i].HashTx.data(), txHash.data(), TXID_SIZE)) {
					t.HashTx = hons[i].HashTx;
					goto LAB_FOUND;
				}
			}
			Throw(CoinErr::InconsistentDatabase);
		}
	LAB_FOUND : {
		txDatas.Items.resize(txDatas.Items.size() + 1);
		TxData& txData = txDatas.Items.back();
		txData.Height = height;
		txData.N = nTx;
		txData.Data = data;
		txData.TxIns = txIns;
		txData.Coins = spend;
		txData.HashTx = txHash;

#ifdef X_DEBUG
		if (txDatas.Items.size() > 1) {
			auto& dd = txDatas.Items[0];
			cout << dd.Height;
		}
#endif
		PutTxDatas(cTxes, TxKey(txHash), txDatas);
	}
	LAB_END:;
	}
	if (Eng.Mode == EngMode::BlockExplorer)
		InsertPubkeyToTxes(dbt, tx);
	dbt.CommitIfLocal();
}

void DbliteBlockChainDb::InsertSpentTxOffsets(const unordered_map<HashValue, SpentTx>& spentTxOffsets) {
	ASSERT(Eng.Mode == EngMode::Bootstrap);

	DbTxRef dbt(m_db);
	for (unordered_map<HashValue, SpentTx>::const_iterator it = spentTxOffsets.begin(), e = spentTxOffsets.end(); it != e; ++it) {
		const SpentTx& spentTx = it->second;
		uint32_t off = htole(spentTx.Offset);
		InsertTx(Tx(), spentTx.N, TxHashesOutNums(), it->first, spentTx.Height, Span(), Span(), Span((const uint8_t*)&off, 3));
	}
	dbt.CommitIfLocal();
}

void DbliteBlockChainDb::SpendInputs(const Tx& tx) {
	if (!tx.IsCoinBase()) {
		EXT_FOR(const TxIn& txIn, tx.TxIns()) {
			UpdateCoins(txIn.PrevOutPoint, true, tx.Height);
		}
	}
}

void DbliteBlockChainDb::InsertBlock(const Block& block, CConnectJob& job) {
	const CTxes& txes = block.Txes;
	vector<uint32_t> txOffsets(txes.size());

	int height = block.Height;
	ASSERT(height < 0xFFFFFF);
	HashValue hash = Hash(block);
	BlockKey blockKey(height);
	MemoryStream msB;
	BinaryWriter wrT(msB);

	Coin::TxHashesOutNums txHashOutNums;

	switch (Eng.Mode) {
	case EngMode::Bootstrap: {
		MemoryStream ms(MAX_BLOCK_SIZE + 8);
		ProtocolWriter wr(ms);
		block.m_pimpl->WriteHeader(wr);
		CoinSerialized::WriteVarInt(wr, txes.size());
		for (size_t i = 0; i < txes.size(); ++i) {
			txOffsets[i] = uint32_t(ms.Position);
            txes[i].Write(wr);
		}
		block.m_pimpl->WriteSuffix(wr);
		Span cb = ms;
		if (!block.m_pimpl->OffsetInBootstrap) {
			vector<uint32_t> ar((cb.size() + 3) / 4 + 2);
			ar[0] = htole(Eng.ChainParams.ProtocolMagic);
			ar[1] = htole(uint32_t(cb.size()));
			memcpy(&ar[2], cb.data(), cb.size());
			m_fileBootstrap.Write(&ar[0], cb.size() + 8, Eng.OffsetInBootstrap);
			//!!!?T			m_fileBootstrap.Flush();
			block.m_pimpl->OffsetInBootstrap = Eng.OffsetInBootstrap + 8;
			Eng.NextOffsetInBootstrap = Eng.OffsetInBootstrap += cb.size() + 8;
		}
		wrT << block.m_pimpl->OffsetInBootstrap;
		block.m_pimpl->WriteDbSuffix(wrT);
	} break;
#if UCFG_COIN_USE_NORMAL_MODE
	case EngMode::Normal:
	case EngMode::BlockExplorer: {
		txHashOutNums.resize(txes.size());
		for (size_t i = 0; i < txes.size(); ++i) {
			const Tx& tx = txes[i];
			HashValue txHash = Hash(tx);
			txHashOutNums[i].HashTx = txHash;
			txHashOutNums[i].NOuts = (uint32_t)tx.TxOuts().size();
		}

		MemoryStream ms;
		DbWriter wr(ms);
		wr.ConnectJob.reset(&job);
		block.m_pimpl->Write(wr);
		//!!!?			wr << uint8_t(0);

		wrT << Span(ReducedBlockHash(hash)) << Span(ms) << EXT_BIN(txHashOutNums);

		for (size_t i = 0; i < txes.size(); ++i) {
			if (!eng.Runned)
				Throw(ExtErr::ThreadInterrupted);
			const Tx& tx = txes[i];
			HashValue txHash = Hash(tx);

			MemoryStream msTx;
			DbWriter dw(msTx);
			dw.ConnectJob.reset(&job);
			dw.PCurBlock.reset(&block);
			dw.PTxHashesOutNums.reset(&txHashOutNums);

			dw << tx;
			tx.WriteTxIns(dw);
			/*!!!R
				MemoryStream msTxIns;
				DbWriter wrIns(msTxIns);
				wrIns.PCurBlock.reset(&block);
				wrIns.ConnectJob.reset(&job);
				wrIns.PTxHashesOutNums.reset(&txHashOutNums);
				*/

#	if UCFG_COIN_TXES_IN_BLOCKTABLE
			txOffsets[i] = uint32_t(msB.Position);
			wrT << Span(msTx);
#	else
			InsertTx(tx, txHashOutNums, txHash, Height, msTxIns, CoinEng::SpendVectorToBlob(vector<bool>(tx.TxOuts().size(), true)), msTx);
			SpendInputs(tx);
#	endif
		}
	} break;
#endif // UCFG_COIN_USE_NORMAL_MODE
	}
	DbTxRef dbt(m_db);
	if (Eng.NextOffsetInBootstrap) // changed only by BootstrapDbThread class
		SetBoostrapOffset(Eng.NextOffsetInBootstrap);

	m_tableBlocks.Put(dbt, blockKey, msB);
	m_tableHashToBlock.Put(dbt, ReducedBlockHash(hash), blockKey);

	int32_t h = htole(int32_t(height));
	m_tableProperties.Put(dbt, KEY_MaxHeight, Span((const uint8_t*)&h, sizeof h));

	h = htole(int32_t((max)(height, GetMaxHeaderHeight())));
	m_tableProperties.Put(dbt, KEY_MaxHeaderHeight, Span((const uint8_t*)&h, sizeof h));

	switch (Eng.Mode) {
	case EngMode::Bootstrap:
#	if UCFG_COIN_TXES_IN_BLOCKTABLE
	case EngMode::Normal:
	case EngMode::BlockExplorer:
#	endif
		for (int nTx = 0; nTx < txes.size(); ++nTx) {
			const Tx& tx = txes[nTx];
			Coin::HashValue txHash = Coin::Hash(tx);
			uint32_t offset = htole(txOffsets[nTx]);
			InsertTx(tx, (uint16_t)nTx, txHashOutNums, txHash, height, Span(), CoinEng::SpendVectorToBlob(vector<bool>(tx.TxOuts().size(), true)), Span((const uint8_t*)&offset, 3));
			SpendInputs(tx);
		}
		break;
	}

	dbt.CommitIfLocal();
}

Blob DbliteBlockChainDb::FindPubkey(int64_t id) {
	id = htole(id);
	DbReadTxRef dbt(m_db);
	DbCursor c(dbt, m_tablePubkeys);
	if (!c.Get(Span((const uint8_t*)&id, PUBKEYID_SIZE)))
		return Blob(nullptr); // Blob(c.Data) should be explicit here because else implicit Blob(ConstBuf(Blob(nullptr))) generated
	Span cbuf = c.Data;
	if (cbuf.size() != 16)
		return cbuf;
	Blob r(0, 20);
	uint8_t* p = (uint8_t*)memcpy(r.data(), cbuf.data(), 12);
	*(uint32_t*)(p + 12) = *(uint32_t*)&id;
	*(uint32_t*)(p + 16) = *(uint32_t*)(cbuf.data() + 12);
	return r;
}

void DbliteBlockChainDb::InsertPubkey(int64_t id, RCSpan pk) {
	id = htole(id);
	DbTxRef dbt(m_db);
	if (pk.size() != 20)
		m_tablePubkeys.Put(dbt, Span((const uint8_t*)&id, PUBKEYID_SIZE), pk);
	else {
		ASSERT(!memcmp(&id, pk.data() + 12, 4));
		uint8_t buf[16];
		memcpy(buf, pk.data(), 12);
		*(uint32_t*)(buf + 12) = *(uint32_t*)(pk.data() + 16);
		m_tablePubkeys.Put(dbt, Span((const uint8_t*)&id, PUBKEYID_SIZE), Span(buf, 16));
	}
	dbt.CommitIfLocal();
}

void DbliteBlockChainDb::UpdatePubkey(int64_t id, RCSpan pk) {
	id = htole(id);
	DbTxRef dbt(m_db);
	m_tablePubkeys.Put(dbt, Span((const uint8_t*)&id, PUBKEYID_SIZE), pk);
	dbt.CommitIfLocal();
}

} // namespace Coin

#endif // UCFG_COIN_COINCHAIN_BACKEND == COIN_BACKEND_DBLITE
