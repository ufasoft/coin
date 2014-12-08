#pragma once

#include <db/dblite/dblite.h>
using namespace Ext::DB;
using namespace Ext::DB::KV;


#include "eng.h"

#ifndef UCFG_COIN_TABLE_TYPE
#	define UCFG_COIN_TABLE_TYPE TableType::HashTable
#endif

namespace Coin {

const size_t BLOCKID_SIZE = 3;
const size_t TXID_SIZE = 6;
const size_t PUBKEYID_SIZE = 5;
const size_t PUBKEYTOTXES_ID_SIZE = 8;

class DbliteBlockChainDb;

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

	bool Create(RCString path) override;
	bool Open(RCString path) override;
	void Close(bool bAsync) override;
	Version CheckUserVersion() override;
	void UpgradeTo(const Version& ver) override;

	bool HaveBlock(const HashValue& hash) override {
		DbTxRef dbt(m_db);
		return DbCursor(dbt, m_tableHashToBlock).Get(ReducedBlockHash(hash));
	}

	Block FindBlock(const HashValue& hash) override;
	Block FindBlock(int height) override;
	int GetMaxHeight() override;
	TxHashesOutNums GetTxHashesOutNums(int height) override;
	void InsertBlock(int height, const HashValue& hash, const ConstBuf& data, const ConstBuf& txData) override;

	struct TxData {
		UInt32 Height;
		Blob Data, TxIns, Coins;
		HashValue HashTx;

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
	void DeleteBlock(int height, const vector<Int64>& txids) override;
	bool FindTxById(const ConstBuf& txid8, Tx *ptx) override;

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

	pair<int, int> FindPrevTxCoords(DbWriter& wr, int height, const HashValue& hash) override;
	void InsertPubkeyToTxes(DbTransaction& dbTx, const Tx& tx);
	vector<Int64> GetTxesByPubKey(const HashValue160& pubkey) override;

	void InsertTx(const Tx& tx, const TxHashesOutNums& hashesOutNums, const HashValue& txHash, int height, const ConstBuf& txIns, const ConstBuf& spend, const ConstBuf& data) override;
	vector<bool> GetCoinsByTxHash(const HashValue& hash) override;
	void SaveCoinsByTxHash(const HashValue& hash, const vector<bool>& vec) override;

	void BeginEngTransaction() override {
		Throw(E_NOTIMPL);
	}

	vector<Block> GetBlocks(const LocatorHashes& locators, const HashValue& hashStop) override {
		Throw(E_NOTIMPL);
	}

	Blob FindPubkey(Int64 id) override;
	void InsertPubkey(Int64 id, const ConstBuf& pk) override;
	void UpdatePubkey(Int64 id, const ConstBuf& pk) override;

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
		m_db.SetProgressHandler(pfn, p, n);
	}

	void Vacuum() override {
		m_db.Vacuum();	
	}
protected:
	virtual void OnOpenTables(DbTransaction& dbt, bool bCreate) {}
private:
	Block LoadBlock(DbTransaction& dbt, int height, const ConstBuf& cbuf);
	bool TryToConvert(RCString path);
};


} // Coin::

