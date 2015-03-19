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

	byte m_placeDbTx[sizeof(DbReadTransaction)];
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

	BlockKey(const ConstBuf& cbuf)
		:	m_beH(0)
	{
		memcpy((byte*)&m_beH + 1, cbuf.P, BLOCKID_SIZE);
	}

	operator uint32_t() const { return betoh(m_beH); }
	operator ConstBuf() const { return ConstBuf((const byte*)&m_beH + 1, BLOCKID_SIZE); }
private:
	uint32_t m_beH;
};


class DbliteBlockChainDb : public IBlockChainDb {
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
	Version CheckUserVersion() override;
	void UpgradeTo(const Version& ver) override;
	void TryUpgradeDb(const Version& verApp) override;

	bool HaveBlock(const HashValue& hash) override {
		DbReadTxRef dbt(m_db);
		return DbCursor(dbt, m_tableHashToBlock).Get(ReducedBlockHash(hash));
	}

	Block FindBlock(const HashValue& hash) override;
	Block FindBlock(int height) override;
	int GetMaxHeight() override;
	TxHashesOutNums GetTxHashesOutNums(int height) override;
	vector<uint32_t> InsertBlock(const Block& block, const ConstBuf& data, const ConstBuf& txData) override;

	struct TxData {
		uint32_t Height;
		Blob Data, TxIns, Coins;
		HashValue HashTx;
		uint32_t TxOffset;

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

	ConstBuf TxKey(const HashValue& txHash) { return ConstBuf(txHash.data(), TXID_SIZE); }
	ConstBuf TxKey(const ConstBuf& txid8) { return ConstBuf(txid8.P, TXID_SIZE); }

	bool FindTxDatas(const ConstBuf& txid8, TxDatas& txDatas);
	TxDatas GetTxDatas(const ConstBuf& txid8);
	void PutTxDatas(const ConstBuf& txKey, const TxDatas& txDatas);
	void DeleteBlock(int height, const vector<int64_t>& txids) override;
	void ReadTx(uint64_t off, Tx& tx);
	bool FindTxById(const ConstBuf& txid8, Tx *ptx) override;
	bool FindTx(const HashValue& hash, Tx *ptx) override;

	void ReadTxIns(const HashValue& hash, const TxObj& txObj) override {
		TxDatas txDatas = GetTxDatas(ConstBuf(hash.data(), 8));
		txObj.ReadTxIns(DbReader(CMemReadStream(txDatas.Items[txDatas.Index].TxIns), &Eng()));
	}

	pair<int, int> FindPrevTxCoords(DbWriter& wr, int height, const HashValue& hash) override;
	void InsertPubkeyToTxes(DbTransaction& dbTx, const Tx& tx);
	vector<int64_t> GetTxesByPubKey(const HashValue160& pubkey) override;

	void InsertTx(const Tx& tx, const TxHashesOutNums& hashesOutNums, const HashValue& txHash, int height, const ConstBuf& txIns, const ConstBuf& spend, const ConstBuf& data) override;
	vector<bool> GetCoinsByTxHash(const HashValue& hash) override;
	void SaveCoinsByTxHash(const HashValue& hash, const vector<bool>& vec) override;

	void BeginEngTransaction() override {
		Throw(E_NOTIMPL);
	}

	vector<Block> GetBlocks(const LocatorHashes& locators, const HashValue& hashStop) override {
		Throw(E_NOTIMPL);
	}

	Blob FindPubkey(int64_t id) override;
	void InsertPubkey(int64_t id, const ConstBuf& pk) override;
	void UpdatePubkey(int64_t id, const ConstBuf& pk) override;

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
protected:
	virtual void OnOpenTables(DbTransaction& dbt, bool bCreate) {}
private:
	static const uint64_t MIN_MM_LENGTH = 1024*1024*1024;

	File m_fileBootstrap;
	MemoryMappedFile m_mmBootstrap;
	MemoryMappedView m_viewBootstrap;
	uint64_t MappedSize;

	Block LoadBlock(DbReadTransaction& dbt, int height, const ConstBuf& cbuf);
	bool TryToConvert(const path& p);
	uint64_t GetBlockOffset(int height);
	void OpenBootstrapFile(const path& dir);

	friend class BootstrapDbThread;
};


} // Coin::

