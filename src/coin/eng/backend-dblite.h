/*######   Copyright (c) 2013-2019 Ufasoft  http://ufasoft.com  mailto:support@ufasoft.com,  Sergey Pavlov  mailto:dev@ufasoft.com ####
#                                                                                                                                     #
# 		See LICENSE for licensing information                                                                                         #
#####################################################################################################################################*/

#pragma once

#include <db/dblite/dblite.h>
using namespace Ext::DB;
using namespace Ext::DB::KV;


#include "eng.h"

namespace Coin {

const size_t BLOCKID_SIZE = 3;
const size_t TXID_SIZE = 6;
const size_t PUBKEYID_SIZE = 5;
const size_t PUBKEYTOTXES_ID_SIZE = 8;

class DbliteBlockChainDb;
}


namespace Coin {

typedef CThreadTxRef<DbReadTransaction> DbReadTxRef;



/**!!!T
class DbReadTxRef : noncopyable {
public:
	DbReadTxRef(DbStorage& env);
	~DbReadTxRef();

	operator DbReadTransaction&() { return *m_p; }
private:
	DbReadTransaction *m_p;

	uint8_t m_placeDbTx[sizeof(DbReadTransaction)];
	CBool m_bOwn;
};
*/

class DbTxRef : noncopyable {
public:
	DbTxRef(DbStorage& env);

	operator DbTransaction&() { return *m_p; }

	void CommitIfLocal() {
		if (m_pMdbTx.get())
			m_pMdbTx->Commit();
	}
private:
	DbTransaction *m_p;

	unique_ptr<DbTransaction> m_pMdbTx;
};

class SavepointTransaction;

class BlockKey {
public:
	BlockKey(uint32_t h)
		:	m_beH(htobe(h))
	{}

	BlockKey(RCSpan cbuf)
		:	m_beH(0)
	{
		memcpy((uint8_t*)&m_beH + 1, cbuf.data(), BLOCKID_SIZE);
	}

	operator uint32_t() const { return betoh(m_beH); }
	operator Span() const { return Span((const uint8_t*)&m_beH + 1, BLOCKID_SIZE); }
private:
	uint32_t m_beH;
};


class DbliteBlockChainDb : public IBlockChainDb {
	typedef IBlockChainDb base;
public:
	mutex MtxDb;
	DbStorage m_db;
	DbTable m_tableBlocks, m_tableHashToBlock, m_tableTxes, m_tablePubkeys, m_tablePubkeyToTxes, m_tableProperties;


//!!!R	unique_ptr<DbTransaction> m_dbt;

	DbliteBlockChainDb();

	void *GetDbObject() override {
		return &m_db;
	}

	ITransactionable& GetSavepointObject() override;

	bool Create(const path& p) override;
	bool Open(const path& p) override;
	void Close(bool bAsync) override;
	void Checkpoint() override;
	Version CheckUserVersion() override;
	void UpgradeTo(const Version& ver) override;
	void TryUpgradeDb(const Version& verApp) override;
	bool HaveHeader(const HashValue& hash) override;
	bool HaveBlock(const HashValue& hash) override;
	BlockHeader FindHeader(int height) override;
	BlockHeader FindHeader(const HashValue& hash) override;
	Block FindBlock(const HashValue& hash) override;
	Block FindBlock(int height) override;
	Block FindBlockPrefixSuffix(int height) override;
	int GetMaxHeight() override;
	int GetMaxHeaderHeight() override;
	TxHashesOutNums GetTxHashesOutNums(int height) override;
	pair<OutPoint, TxOut> GetOutPointTxOut(int height, int idxOut) override;
	void SpendInputs(const Tx& tx);

	struct TxData {
		uint32_t Height;
		Blob Data, TxIns, Coins;
		HashValue HashTx;
		uint32_t TxOffset;
		uint16_t N;

		void Read(BinaryReader& rd);
		void Write(BinaryWriter& wr, bool bWriteHash = false) const;
	};

	struct TxDatas {
		vector<TxData> Items;
		int Index;

		TxDatas()
			:	Index(0)
		{}
	};

	Span TxKey(const HashValue& txHash) { return Span(txHash.data(), TXID_SIZE); }
	Span TxKey(RCSpan txid8) { return Span(txid8.data(), TXID_SIZE); }

	bool FindTxDatas(DbCursor& c, RCSpan txid8, TxDatas& txDatas);
	TxDatas GetTxDatas(RCSpan txid8);
	void PutTxDatas(DbCursor& c, RCSpan txKey, const TxDatas& txDatas, bool bUpdate = false);
	void DeleteBlock(int height, const vector<int64_t> *txids) override;
	void ReadTx(uint64_t off, Tx& tx);
	bool FindTxById(RCSpan txid8, Tx *ptx) override;
	bool FindTx(const HashValue& hash, Tx *ptx) override;
	void ReadTxes(const BlockObj& bo) override;
	void ReadTxIns(const HashValue& hash, const TxObj& txObj) override;
	pair<int, int> FindPrevTxCoords(DbWriter& wr, int height, const HashValue& hash) override;
	void InsertPubkeyToTxes(DbTransaction& dbTx, const Tx& tx);
	vector<int64_t> GetTxesByPubKey(const HashValue160& pubkey) override;

	void InsertTx(const Tx& tx, uint16_t nTx, const TxHashesOutNums& hashesOutNums, const HashValue& txHash, int height, RCSpan txIns, RCSpan spend, RCSpan data) override;
	void InsertSpentTxOffsets(const unordered_map<HashValue, SpentTx>& spentTxOffsets) override;

	void InsertBlock(const Block& block, CConnectJob& job) override;
	void InsertHeader(const BlockHeader& header) override;

	vector<bool> GetCoinsByTxHash(const HashValue& hash) override;
	void SaveCoinsByTxHash(const HashValue& hash, const vector<bool>& vec) override;
	void UpdateCoins(const OutPoint& op, bool bSpend) override;

	void BeginEngTransaction() override {
		Throw(E_NOTIMPL);
	}


	Blob FindPubkey(int64_t id) override;
	void InsertPubkey(int64_t id, RCSpan pk) override;
	void UpdatePubkey(int64_t id, RCSpan pk) override;

	void BeginTransaction() override {
//		ASSERT(!m_dbt.get());
//		m_dbt.reset(new MdbTransaction(m_db));
	}

	void Commit() override;

	void Rollback() override  {
	//	m_dbt->Rollback();
//		m_dbt.reset();
	}

	void SetProgressHandler(int(*pfn)(void*), void *p, int n) override {
		m_db.SetProgressHandler(pfn, p, n);
	}

	void Vacuum() override {
		m_db.Vacuum();
	}

	uint64_t GetBoostrapOffset() override;
	void SetBoostrapOffset(uint64_t v) override;
	vector<BlockHeader> GetBlockHeaders(const LocatorHashes& locators, const HashValue& hashStop) override;
protected:
	virtual void OnOpenTables(DbTransaction& dbt, bool bCreate) {}
private:
	static const uint64_t MIN_MM_LENGTH = 1024*1024*1024;

	File m_fileBootstrap;
	MemoryMappedFile m_mmBootstrap;
	MemoryMappedView m_viewBootstrap;
	uint64_t MappedSize;

	HashValue ReadPrevBlockHash(DbReadTransaction& dbt, int height, bool bFromBlock = false);
	BlockHeader LoadHeader(DbReadTransaction& dbt, int height, Stream& stmBlocks, int hMaxBlock = -2);
	Block LoadBlock(DbReadTransaction& dbt, int height, Stream& stmBlocks, bool bFullRead = true);
	bool TryToConvert(const path& p);
	uint64_t GetBlockOffset(int height);
	void OpenBootstrapFile(const path& dir);
	void BeforeDbOpenCreate();

	friend class BootstrapDbThread;
};


} // Coin::

