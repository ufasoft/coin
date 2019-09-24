/*######   Copyright (c) 2013-2019 Ufasoft  http://ufasoft.com  mailto:support@ufasoft.com,  Sergey Pavlov  mailto:dev@ufasoft.com ####
#                                                                                                                                     #
# 		See LICENSE for licensing information                                                                                         #
#####################################################################################################################################*/

#include <el/ext.h>

#include "param.h"
#include "coin-protocol.h"

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

const Version
	VER_BLOCKS_TABLE_IS_HASHTABLE(0, 81),
	VER_PUBKEY_RECOVER(0, 90),
	VER_HEADERS(0, 120),
	DB_VER_COMPACT_UTXO(1, 1),
	DB_VER_LATEST = DB_VER_COMPACT_UTXO;

#define COIN_DEF_DB_KEY(name) const Span KEY_##name((const uint8_t*)#name, strlen(#name));

COIN_DEF_DB_KEY(MaxHeight);
COIN_DEF_DB_KEY(MaxHeaderHeight);
COIN_DEF_DB_KEY(BootstrapOffset);
COIN_DEF_DB_KEY(LastPrunedHeight);
COIN_DEF_DB_KEY(Filter);



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
	, m_tableBlocks			("blocks"		, BLOCKID_SIZE			, TableType::HashTable, HashType::Identity)			// Don't make it BTree to avoid be sparse forever after replacing Headers by Offsets
	, m_tableHashToBlock	("hash_blocks"	, 0						, TableType::HashTable)
	, m_tableTxes			("txes"			, TXID_SIZE				, TableType::HashTable	, HashType::Identity)
	, m_tablePubkeys		("pubkeys"		, PUBKEYID_SIZE			, TableType::HashTable	, HashType::Identity)
	, m_tablePubkeyToTxes	("pubkey_txes"	, PUBKEYTOTXES_ID_SIZE	, TableType::HashTable	, HashType::Identity)
	, m_tableProperties		("properties"	, 0						, TableType::HashTable)
{
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

int DbliteBlockChainDb::GetMaxHeight() {
	DbReadTxRef dbt(m_db);
	if (m_db.UserVersion < VER_BLOCKS_TABLE_IS_HASHTABLE) {
		DbCursor c(dbt, m_tableBlocks);		//!!!O Remove in 2020
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
	m_db.UserVersion = Version(DB_VER_LATEST);
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
	if (Eng.Mode == EngMode::Bootstrap) {
		OpenBootstrapFile(p.parent_path());

		DbReadTxRef dbt(m_db);
		BlockOffsets.resize(GetMaxHeight() + 1);
		for (DbCursor c(dbt, m_tableBlocks); c.SeekToNext();) {
			int height = BlockKey(c.get_Key());
			if (height < BlockOffsets.size())
				BlockOffsets.at(height) = GetLeUInt64(c.get_Data().data());
		}
	}
	if (TryToConvert(p))
		return false;
	return true;
}

void DbliteBlockChainDb::Commit() {
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

	Version ver(DB_VER_LATEST), dbVer = m_db.UserVersion;
	if (ver.Major > dbVer.Major) {
		m_db.Close();
		throw UnsupportedOldVersionException(dbVer);
	} else if (ver.Major < dbVer.Major || ver.Major == dbVer.Major && ver.Minor < dbVer.Minor) {
		m_db.Close();
		throw UnsupportedNewVersionException(dbVer);
	}
	return dbVer;
}

void DbliteBlockChainDb::UpgradeTo(const Version& ver) {
	Version dbVer = CheckUserVersion();
	if (dbVer < ver) {
		TRC(0, "Upgrading Database " << Eng.GetDbFilePath() << " from version " << dbVer << " to " << ver);

		DbTransaction dbtx(m_db);

		if (dbVer < VER_BLOCKS_TABLE_IS_HASHTABLE) {
			m_tableProperties.Open(dbtx, true);
			int32_t h = htole(int32_t(GetMaxHeight()));
			m_tableProperties.Put(dbtx, KEY_MaxHeight, Span((const uint8_t*)&h, sizeof h));
		}

		if (dbVer < VER_HEADERS) {
			int32_t h = htole(int32_t(GetMaxHeight()));
			m_tableProperties.Put(dbtx, KEY_MaxHeaderHeight, Span((const uint8_t*)&h, sizeof h));
		}

		Eng.UpgradeDb(ver);
		m_db.SetUserVersion(ver);
		dbtx.Commit();
		m_db.Checkpoint();
	}
}

void DbliteBlockChainDb::TryUpgradeDb(const Version& verTarget) {
	if (Eng.Mode != EngMode::Bootstrap) {
		Version dbVer = CheckUserVersion();
		if (dbVer < verTarget) {
			Recreate(dbVer);
			return;
		}
	}
	UpgradeTo(verTarget);
}

HashValue DbliteBlockChainDb::ReadPrevBlockHash(DbReadTransaction& dbt, int height, bool bFromBlock) {
	if (height == 0)
		return HashValue::Null();
	DbCursor c(dbt, m_tableBlocks);
	if (!c.Get(BlockKey(height - 1)))
		Throw(CoinErr::InconsistentDatabase);
	Stream& msPrev = c.DataStream;
	BinaryReader rdDb(msPrev);
	if (bFromBlock) {
		switch (Eng.Mode) {
		case EngMode::Bootstrap: {
			PositionOwningFileStream stmFile(m_fileBootstrap, rdDb.ReadUInt64());
			BlockHeader header(Eng.CreateBlockObj());
			header->ReadHeader(ProtocolReader(stmFile), false, 0);
			return Hash(header);
		}
		case EngMode::Lite: {
			BlockHeader header(Eng.CreateBlockObj());
			header->ReadHeader(ProtocolReader(msPrev), false, 0);
			return Hash(header);
		}
		}
	}
	Blob blob;
	rdDb >> blob;
	return BlockHashValue(blob);
}

Blob DbliteBlockChainDb::ReadBlob(uint64_t offset, uint32_t size) {
	Blob r(size, nullptr);
	m_fileBootstrap.Read(r.data(), size, offset);
	return r;
}

void DbliteBlockChainDb::CopyTo(uint64_t offset, uint32_t size, Stream& stm) {
	PositionOwningFileStream stmFile(m_fileBootstrap, offset, size);
	stmFile.CopyTo(stm);
}

Block DbliteBlockChainDb::LoadBlock(DbReadTransaction& dbt, int height, Stream& stmBlocks, bool bFullRead) {
	Block r;
	r->Height = height;
	BinaryReader rdDb(stmBlocks);
	switch (Eng.Mode) {
	case EngMode::Bootstrap: {
		if (!bFullRead && stmBlocks.Length > 8) {
			ProtocolReader rd(stmBlocks);
			r->ReadHeader(rd, false, 0);
		} else {
			uint64_t off = rdDb.ReadUInt64();
			PositionOwningFileStream stmFile(m_fileBootstrap, off);
			BufferedStream stm(stmFile);
			ProtocolReader rd(stm, true);
			if (bFullRead)
				r.Read(rd);
			else
				r->ReadHeader(rd, false, 0);
			r->OffsetInBootstrap = off;
			r->ReadDbSuffix(rdDb);
		}
	} break;
	case EngMode::Lite: {
		ProtocolReader rd(stmBlocks, true);
		rd.MayBeHeader = true;
		r.Read(rd);
	} break;
	default: {
		Blob blob;
		rdDb >> blob;
		BlockHashValue hash(blob);
		rdDb >> blob;
		CMemReadStream ms(blob);
		DbReader rd(ms, &Eng);
		r->Read(rd);
		r->m_hash = hash;

		if (bFullRead) {
			rdDb >> blob;
			r->m_txHashesOutNums = Coin::TxHashesOutNums(blob);
		}
		r->PrevBlockHash = ReadPrevBlockHash(dbt, r.Height);

		//!!!	ASSERT(Hash(r) == hash);
	}
	}
	r->IsInTrunk = true;
	return r;
}

bool DbliteBlockChainDb::HaveHeader(const HashValue& hash) {
	DbReadTxRef dbt(m_db);
	return DbCursor(dbt, m_tableHashToBlock).Get(ReducedBlockHash(hash));
}

BlockHeader DbliteBlockChainDb::LoadHeader(DbReadTransaction& dbt, int height, Stream& stmBlocks, int hMaxBlock) {
	if (hMaxBlock == -2)
		hMaxBlock = GetMaxHeight();
	auto len = (int)stmBlocks.Length;
	if (height <= hMaxBlock || !Between(len, 9, 79))
		return LoadBlock(dbt, height, stmBlocks, false);

	//!!!Obsolete code, can be removed after Bootstrap files will download full blocks (in 2020)
	BlockHeader r(Eng.CreateBlockObj());
	r->Height = height;
	DbReader rd(stmBlocks);
	rd.ForHeader = true;
	Blob blob;
	rd >> blob;
	BlockHashValue hash(blob);
	r->Read(rd);
	r->PrevBlockHash = ReadPrevBlockHash(dbt, height, (height - 1) <= hMaxBlock);
	ASSERT(Hash(r) == hash);
	r->m_hash = hash;
	r->IsInTrunk = true;
	return r;
}

int DbliteBlockChainDb::FindHeight(const HashValue& hash) {
	DbReadTxRef dbt(m_db);
	DbCursor c(dbt, m_tableHashToBlock);
	if (!c.Get(ReducedBlockHash(hash)))
		return -1;
	return (uint32_t)BlockKey(c.Data);
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

BlockHeader DbliteBlockChainDb::FindHeader(const BlockRef& bref) {
	BlockHeader r(nullptr);
	DbReadTxRef dbt(m_db);
	int height;
	if (bref.IsInTrunk())
		height = bref.HeightInTrunk;
	else {
		DbCursor c(dbt, m_tableHashToBlock);
		if (!c.Get(ReducedBlockHash(bref.Hash)))
			return r;
		height = (uint32_t)BlockKey(c.Data);
	}
	DbCursor c1(dbt, m_tableBlocks);
	if (!c1.Get(BlockKey(height))) {
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

optional<pair<uint64_t, uint32_t>> DbliteBlockChainDb::FindBlockOffset(const HashValue& hash) {
	if (Eng.Mode != EngMode::Bootstrap)
		Throw(E_FAIL);
	DbReadTxRef dbt(m_db);
	DbCursor c(dbt, m_tableHashToBlock);
	if (!c.Get(ReducedBlockHash(hash)))
		return nullopt;
	int height = (uint32_t)BlockKey(c.Data);
	if (height > GetMaxHeight())
		return nullopt;
	DbCursor c1(dbt, m_tableBlocks);
	if (!c1.Get(c.Data)) {
		Throw(CoinErr::InconsistentDatabase);
	}
	BinaryReader rdDb(c1.DataStream);
	uint64_t offset = rdDb.ReadUInt64();
	PositionOwningFileStream stmFile(m_fileBootstrap, offset - 4);
	uint32_t size = BinaryReader(stmFile).ReadUInt32();
	return make_pair(offset, size);
}

Block DbliteBlockChainDb::FindBlock(const HashValue& hash) {
	Block r(nullptr);
	DbReadTxRef dbt(m_db);
	DbCursor c(dbt, m_tableHashToBlock);
	if (c.Get(ReducedBlockHash(hash))) {
		int height = (uint32_t)BlockKey(c.Data);
		if (height <= GetMaxHeight()) {
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
	r.reserve(ProtocolParam::MAX_HEADERS_RESULTS);
	for (int height = locators.FindHeightInMainChain() + 1; c.Get(BlockKey(height)); ++height) {
		r.push_back(LoadHeader(dbt, height, c.DataStream, hMaxBlock));
		if (r.size() >= ProtocolParam::MAX_HEADERS_RESULTS || Hash(r.back()) == hashStop)
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

int DbliteBlockChainDb::FindHeightByOffset(uint64_t offset) {
	vector<uint64_t>::const_iterator b = BlockOffsets.begin(), e = BlockOffsets.end();
	if (b == e)
		return -1;
	auto it = lower_bound(b, e, offset);
	return int(it - b) - (it != e && *it == offset ? 0 : 1);
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

void DbliteBlockChainDb::TxData::Read(BinaryReader& rd, CoinEng& eng) {
	uint32_t h = 0;
	rd.BaseStream.ReadBuffer(&h, BLOCKID_SIZE);
	Height = letoh(h);
	uint8_t utxo = 0;
	switch (eng.Mode) {
#	if UCFG_COIN_TXES_IN_BLOCKTABLE
	case EngMode::Normal:
	case EngMode::BlockExplorer:
		rd >> N;
		Data.resize(3);
		rd.BaseStream.ReadBuffer(Data.data(), 3);
		break;
#	endif
	case EngMode::Bootstrap:
		rd.BaseStream.ReadBuffer(&TxOffset, 3);
		TxOffset = letoh(TxOffset) & 0xFFFFFF;
		if (TxOffset)
			utxo = uint8_t(TxOffset >> 22);
		else
			TxOffset = rd.ReadUInt32();
		break;
	default:
		rd >> Data >> TxIns;
	}
	if (rd.BaseStream.Eof()) {
		if (utxo) {
			Utxo = Span(&utxo, 1);
			TxOffset &= 0x3FFFFF;
		} else {
			Utxo.resize(0);
			LastSpendHeight = 0; //!!! unknown
		}
	} else {
		rd >> Utxo;
		if (Utxo.empty()) {
			LastSpendHeight = rd.BaseStream.Eof()
				? Height // Inconsistent DB, support for compatibility with old format
				: uint32_t(Height + rd.Read7BitEncoded());
		}
	}
}

void DbliteBlockChainDb::TxData::Write(BinaryWriter& wr, CoinEng& eng, bool bWriteHash) const {
	uint32_t h = htole(Height);
	wr.BaseStream.WriteBuffer(&h, BLOCKID_SIZE);
	switch (eng.Mode) {
#	if UCFG_COIN_TXES_IN_BLOCKTABLE
	case EngMode::Normal:
	case EngMode::BlockExplorer:
		wr << N;
		wr.BaseStream.WriteBuffer(Data.constData(), 3);
		break;
#	endif
	case EngMode::Bootstrap:
		if (!bWriteHash && Utxo.size() == 1 && Utxo[0] < 4 && TxOffset <= 0x3FFFFF) {	// Compact UTXO encoding
			uint32_t txOffset = htole(TxOffset | (uint32_t(Utxo[0]) << 22));
			wr.BaseStream.WriteBuffer(&txOffset, 3);
			return;
		} else {
			uint32_t txOffset = htole(TxOffset);
			if (TxOffset < 0x1000000)
				wr.BaseStream.WriteBuffer(&txOffset, 3);
			else {
				txOffset = 0;
				wr.BaseStream.WriteBuffer(&txOffset, 3);
				wr << TxOffset;
			}
		}
		break;
	default:
		wr << Data << TxIns;
	}
	wr << Utxo;
	if (Utxo.empty())
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

static const size_t AVG_TX_SIZE = 300;		// Average is ~259 bytes, but we read some reserve.

void DbliteBlockChainDb::ReadTx(uint64_t off, Tx& tx) {
    tx.Read(off + MAX_BLOCK_SIZE_FOR_ALL_CHAINS < MappedSize
        ? ProtocolReader(CMemReadStream(Span((uint8_t*)m_viewBootstrap.Address + off, MAX_BLOCK_SIZE_FOR_ALL_CHAINS)), true)
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
		ptx->m_pimpl->Height = txData.Height;
		switch (Eng.Mode) {
		case EngMode::Bootstrap:
			ReadTx(GetBlockOffset(txData.Height) + txData.TxOffset, *ptx);
			break;
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
		tx->Timestamp = bo.Timestamp;
		tx->Height = bo.Height;
		tx->OffsetInBlock = uint32_t(stm.Position);
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
	for (int j = 0; j < txData.Utxo.size(); ++j) {
		uint8_t v = txData.Utxo[j];
		for (int i = 0; i < 8; ++i)
			vec.push_back(v & (1 << i));
	}
	return vec;
}

void DbliteBlockChainDb::SaveCoinsByTxHash(const HashValue& hash, const vector<bool>& vec) {
	TxDatas txDatas = GetTxDatas(hash);
	TxData& txData = txDatas.Items[txDatas.Index];
	txData.Utxo = find(vec.begin(), vec.end(), true) != vec.end() ? CoinEng::SpendVectorToBlob(vec) : Blob();

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
	if (bSpend) {
		uint8_t* p = txData.Utxo.data();
		if (pos >= txData.Utxo.size() || !(p[pos] & mask))
			Throw(CoinErr::InputsAlreadySpent);
		p[pos] &= ~mask;
		size_t i;
		for (i = txData.Utxo.size(); i-- && !p[i];)
			;
		if (i + 1 != txData.Utxo.size())
			txData.Utxo.resize(i + 1);
		if (txData.Utxo.size() == 0) {
			if (Eng.Mode == EngMode::Bootstrap || UCFG_COIN_TXES_IN_BLOCKTABLE) {
				uint32_t txOffset = Eng.Mode == EngMode::Bootstrap ? txData.TxOffset : GetLeUInt32(txData.Data.constData());
				SpentTx stx = { op.TxHash, txData.Height, txOffset, txData.N };
				Eng.Caches.Add(stx);
			}
			txData.LastSpendHeight = heightCur;
			//-		txDatas.Items.erase(txDatas.Items.begin() + txDatas.Index);  // Don't remove fully spent TxDatas because Reorganize may need them
		}
	} else {
		if (pos >= txData.Utxo.size())
			txData.Utxo.resize(pos + 1);
		txData.Utxo.data()[pos] |= mask;
	}

	PutTxDatas(cTxes, TxKey(txid8), txDatas, true);
	dbt.CommitIfLocal();
}

void DbliteBlockChainDb::PruneTxo(const OutPoint& op, int32_t heightCur) {
	DbTxRef dbt(m_db);
	DbCursor cTxes(dbt, m_tableTxes);
	if (TxDatas txDatas = FindTxDatas(cTxes, Span(op.TxHash.data(), 8))) {	// May be has been removed in other txin of this heightCur
		if (!txDatas.Items[txDatas.Index].IsCoinSpent(op.Index)) {		// May be TxKey collision
			TxData& d = txDatas.Items[txDatas.Index];	//!!!D
			return;
		}
		for (auto& d : txDatas.Items) {
			if (!d.Utxo.empty()) {
#ifdef _DEBUG
				if (!d.Utxo.back()) {
					Throw(CoinErr::InconsistentDatabase);
				}
#endif
				return;
			}
			if (d.LastSpendHeight > heightCur)
				return;
		}

		cTxes.Delete();		// Only when there are no UTXO
	}
}

void DbliteBlockChainDb::InsertTx(const Tx& tx, uint16_t nTx, const TxHashesOutNums& hashesOutNums, const HashValue& txHash, int height, RCSpan txIns, RCSpan spend, RCSpan data, uint32_t txOffset) {
	ASSERT(height < 0xFFFFFF);

	TxData d;
	d.Height = height;
	d.N = nTx;
	d.TxOffset = txOffset;
	d.Utxo = spend;
	d.Data = data;
	d.TxIns = txIns;
	d.HashTx = txHash;

	MemoryStream msT;
	BinaryWriter wr(msT);
	d.Write(wr, Eng);

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
				TxData& t = txDatas.Items[0];
				if (Eng.Mode == EngMode::Bootstrap) {
					Tx txOld;
					ReadTx(GetBlockOffset(t.Height) + t.TxOffset, txOld);
					if (Hash(txOld) != txHash) {
						t.HashTx = Hash(txOld);
						goto LAB_FOUND;
					}
				} else if (!Equal(Span(t.TxIns), txIns))
					goto LAB_OTHER_TX;
				else if (Eng.Mode == EngMode::Bootstrap) {
					if (t.TxOffset != txOffset)
						goto LAB_OTHER_TX;
				} else if (!Equal(Span(t.Data), data))
					goto LAB_OTHER_TX;
			}
			TxData& txData = txDatas.Items[txDatas.Index];
			txData.Utxo = spend;

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
	LAB_FOUND :
		txDatas.Items.push_back(d);

#ifdef X_DEBUG
		if (txDatas.Items.size() > 1) {
			auto& dd = txDatas.Items[0];
			cout << dd.Height;
		}
#endif
		PutTxDatas(cTxes, TxKey(txHash), txDatas);
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
		InsertTx(Tx(), spentTx.N, TxHashesOutNums(), it->first, spentTx.Height, Span(), Span(), Span(), spentTx.Offset);
	}
	dbt.CommitIfLocal();
}

void DbliteBlockChainDb::SpendInputs(const Tx& tx) {
	if (!tx->IsCoinBase()) {
		EXT_FOR(const TxIn& txIn, tx.TxIns()) {
			UpdateCoins(txIn.PrevOutPoint, true, tx.Height);
		}
	}
}

void DbliteBlockChainDb::InsertHeader(const BlockHeader& header, bool bUpdateMaxHeight) {
	int32_t height = header.Height;
	ASSERT(height < 0xFFFFFF);
	HashValue hash = Hash(header);
	BlockKey blockKey(height);
	MemoryStream msB;
	ProtocolWriter wrT(msB);
	header->WriteHeader(wrT);
	DbTxRef dbt(m_db);
	m_tableBlocks.Put(dbt, blockKey, Span(msB), true);
	m_tableHashToBlock.Put(dbt, ReducedBlockHash(hash), blockKey, true);
	int32_t h = htole(height);
	m_tableProperties.Put(dbt, KEY_MaxHeaderHeight, Span((const uint8_t*)& h, sizeof h));
	if (bUpdateMaxHeight)
		m_tableProperties.Put(dbt, KEY_MaxHeight, Span((const uint8_t*)& h, sizeof h));

	dbt.CommitIfLocal();
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
	case EngMode::Lite: {
#ifdef _DEBUG //!!!D
		DbTxRef _dbt(m_db);
		DbCursor _c(_dbt, m_tableProperties);
		if (_c.SeekToKey(KEY_MaxHeight)) {
			auto prevHe = (int)GetLeUInt32(_c.get_Data().data());
			if (height != prevHe + 1) {
				Throw(E_FAIL);
			}
		}
#endif
		ProtocolWriter wr(msB);
		if (block.Txes.empty())
			block.WriteHeader(wr);
		else
			block.Write(wr);
	} break;
	case EngMode::Bootstrap: {
		MemoryStream ms(MAX_BLOCK_SIZE + 8);
		ProtocolWriter wr(ms);
		block->WriteHeader(wr);
		CoinSerialized::WriteCompactSize(wr, txes.size());
		for (size_t i = 0; i < txes.size(); ++i) {
			txOffsets[i] = uint32_t(ms.Position);
            txes[i].Write(wr);
		}
		block->WriteSuffix(wr);
		Span cb = ms;
		if (!block->OffsetInBootstrap) {
			vector<uint32_t> ar((cb.size() + 3) / 4 + 2);
			ar[0] = htole(Eng.ChainParams.DiskMagic);
			ar[1] = htole(uint32_t(cb.size()));
			memcpy(&ar[2], cb.data(), cb.size());
			m_fileBootstrap.Write(&ar[0], cb.size() + 8, Eng.OffsetInBootstrap);
			//!!!?T			m_fileBootstrap.Flush();
			block->OffsetInBootstrap = Eng.OffsetInBootstrap + 8;
			Eng.NextOffsetInBootstrap = Eng.OffsetInBootstrap += cb.size() + 8;
		}
		wrT << block->OffsetInBootstrap;
		block->WriteDbSuffix(wrT);
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
		block->Write(wr);
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
#	if UCFG_COIN_TXES_IN_BLOCKTABLE
			txOffsets[i] = uint32_t(msB.Position);
			wrT << Span(msTx);
#	else
			InsertTx(tx, txHashOutNums, txHash, Height, msTxIns, CoinEng::SpendVectorToBlob(vector<bool>(tx.TxOuts().size(), true)), msTx, 0);
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
		if (BlockOffsets.size() < height + 1)
			BlockOffsets.resize(height + 1);
		BlockOffsets[height] = block->OffsetInBootstrap;

#	if UCFG_COIN_TXES_IN_BLOCKTABLE
	case EngMode::Normal:
	case EngMode::BlockExplorer:
#	endif
		for (int nTx = 0; nTx < txes.size(); ++nTx) {			// Don't require topological order for BCH compatibility,
			const Tx& tx = txes[nTx];							// CoinEng::ConnectBlockTxes() already checked topological order for Bitcoin chain
			uint32_t txOffset = txOffsets[nTx];
			uint32_t leOffset = htole(txOffset);
			InsertTx(tx, (uint16_t)nTx, txHashOutNums, Coin::Hash(tx), height, Span(), CoinEng::SpendVectorToBlob(vector<bool>(tx.TxOuts().size(), true)), Span((const uint8_t*)& leOffset, 3), txOffset);
		}
		for (auto& tx : txes)
			SpendInputs(tx);
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

ptr<CoinFilter> DbliteBlockChainDb::GetFilter() {
	DbReadTxRef dbt(m_db);
	DbCursor c(dbt, m_tableProperties);
	if (!c.Get(KEY_Filter))
		return nullptr;
	ptr<CoinFilter> r = new CoinFilter;
	r->Read(BinaryReader(c.DataStream));
	return r;
}

void DbliteBlockChainDb::SetFilter(CoinFilter* filter) {
	DbTxRef dbt(m_db);
	if (!filter)
		m_tableProperties.Delete(dbt, KEY_Filter);
	else {
		MemoryStream ms;
		filter->Write(BinaryWriter(ms));
		m_tableProperties.Put(dbt, KEY_Filter, ms);
	}
}


} // namespace Coin


#endif // UCFG_COIN_COINCHAIN_BACKEND == COIN_BACKEND_DBLITE
