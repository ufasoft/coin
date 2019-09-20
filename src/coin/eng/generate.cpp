/*######   Copyright (c) 2013-2019 Ufasoft  http://ufasoft.com  mailto:support@ufasoft.com,  Sergey Pavlov  mailto:dev@ufasoft.com ####
#                                                                                                                                     #
# 		See LICENSE for licensing information                                                                                         #
#####################################################################################################################################*/

#include <el/ext.h>

#include "coin-protocol.h"
#include "eng.h"
#include "wallet.h"
#include "script.h"

#if UCFG_COIN_GENERATE

namespace Coin {

class EmbeddedMiner : public BitcoinMiner {
	typedef BitcoinMiner base;

	HashValue BestHash;
	Wallet& m_wallet;
	vector<WalletBase*> m_childWallets;
	DateTime m_dtPrevGetwork;
	Block m_prevBlock;

	mutex m_mtxMerkleToBlock;
	LruMap<HashValue, Block> m_merkleToBlock;

	uint32_t m_extraNonce;
	int m_merkleSize;
	uint32_t m_merkleNonce;

public:
	EmbeddedMiner(Wallet& wallet)
		: m_wallet(wallet)
		, m_merkleToBlock(10)
		, m_prevBlock(nullptr)
	{
		HashAlgo = m_wallet.m_eng->ChainParams.HashAlgo;

#	ifdef _DEBUG//!!!D
		ThreadCount = 2;
#	endif
	}

	void RegisterForMining();
	void UnregisterForMining();
protected:
	void SetSpeedCPD(float speed, float cpd) override;
	ptr<BitcoinWorkData> GetWork(WebClient*& curWebClient) override;
	void SubmitResult(WebClient*& curWebClient, const BitcoinWorkData& wd) override;
private:

	void UpdateParentChilds();
	void SetUniqueExtraNonce(Block& block, CoinEng& eng);
};


class COrphan {
public:
	Coin::Tx Tx;
	unordered_set<HashValue> DependsOn;
	double Priority;

	COrphan() {
	}

	COrphan(const Coin::Tx& tx)
		: Tx(tx)
		, Priority(0)
	{}
};

typedef list<COrphan> COrhpans;

void WalletBase::ReserveGenKey() {
	EXT_LOCK (m_eng->Mtx) {
		m_genKeyInfo = m_eng->m_cdb.GenerateNewAddress(g_conf.GetAddressType(), nullptr);
	}
}

Tx WalletBase::CreateCoinbaseTx() {
	Tx tx;
	tx.EnsureCreate(get_Eng());
	tx->m_txIns.resize(1);
	tx->m_bLoadedIns = true;
	tx.TxOuts().resize(1);
	tx.TxOuts()[0].m_scriptPubKey = m_genKeyInfo->ToAddress()->ToScriptPubKey();
	return tx;
}

Block WalletBase::CreateNewBlock() {
	CoinEng& eng = get_Eng();
	CCoinEngThreadKeeper engKeeper(&eng);

	if (m_eng->IsInitialBlockDownload())
		return Block(nullptr);

	Block block;
	int64_t nFees = 0;
	EXT_LOCK (m_eng->Mtx) {
		block.Add(CreateCoinbaseTx());

#if UCFG_COIN_GENERATE_TXES_FROM_POOL
		COrhpans orphans;
		unordered_map<HashValue, vector<COrhpans::iterator>> mapDependers;
		multimap<double, Tx> mapPriority;

		EXT_LOCK (m_eng->TxPool.Mtx) {
			for (auto it = m_eng->TxPool.m_hashToTxInfo.begin(), e = m_eng->TxPool.m_hashToTxInfo.end(); it!= e; ++it) {
				const Tx& tx = it->second.Tx;
				if (!tx->IsCoinBase() && tx->IsFinal()) {
					double priority = 0;
					COrhpans::iterator itOrphan = orphans.end();
					EXT_FOR (const TxIn& txIn, tx.TxIns()) {
						Tx txPrev;
						if (Tx::TryFromDb(txIn.PrevOutPoint.TxHash, &txPrev))
							priority += double(txPrev.TxOuts().at(txIn.PrevOutPoint.Index).Value) * txPrev.DepthInMainChain;
						else {
							if (itOrphan == orphans.end())
								itOrphan = orphans.insert(orphans.end(), COrphan(tx));
							mapDependers[txIn.PrevOutPoint.TxHash].push_back(itOrphan);
							itOrphan->DependsOn.insert(txIn.PrevOutPoint.TxHash);
						}
					}

					priority /= tx.GetSerializeSize();
					if (itOrphan != orphans.end())
						itOrphan->Priority = priority;
					else
						mapPriority.insert(make_pair(-priority, tx));
				}
			}
		}
#endif // UCFG_COIN_GENERATE_TXES_FROM_POOL

		BlockHeader bestBlock = m_eng->BestBlock();
		block->PrevBlockHash = Hash(bestBlock);
		block.GetFirstTxRef().TxOuts()[0].Value = m_eng->GetSubsidy(bestBlock.Height + 1, block->PrevBlockHash) + nFees;
		block->Height = bestBlock.Height + 1;
		block->Timestamp = m_eng->GetTimestampForNextBlock();
		block->DifficultyTargetBits = m_eng->GetNextTarget(bestBlock, block).m_value;
		block->Nonce = 0;

#if UCFG_COIN_GENERATE_TXES_FROM_POOL
		CoinsView view(eng);
		int nBlockSigOps = 100;
		for (uint32_t cbBlockSize = 1000; !mapPriority.empty();) {
			auto it = mapPriority.begin();
			double priority = -it->first;
			Tx tx = it->second;
			mapPriority.erase(it);

			uint32_t cbNewBlockSize = cbBlockSize + tx.GetSerializeSize();

			if (cbNewBlockSize < eng.ChainParams.MaxBlockWeight) {
				bool bAllowFree = cbNewBlockSize < 400 || Tx::AllowFree(priority);
				int64_t minFee = tx.GetMinFee(cbBlockSize, cbNewBlockSize<400 || Tx::AllowFree(priority));

				CoinsView viewTmp(eng);
				viewTmp.TxMap = view.TxMap;
				try {
					tx.ConnectInputs(viewTmp, m_eng->BestBlockHeight() + 1, nBlockSigOps, nFees, false, true, minFee, block->DifficultyTarget);
				} catch (RCExc) {
					continue;
				}
				swap(view, viewTmp);
				block.Add(tx);
				cbBlockSize = cbNewBlockSize;

				HashValue hash = Hash(tx);
				auto itDep = mapDependers.find(hash);
				if (itDep != mapDependers.end()) {
					EXT_FOR (const COrhpans::iterator& itOrphan, itDep->second) {
						if (itOrphan->DependsOn.erase(hash) && itOrphan->DependsOn.empty())
							mapPriority.insert(make_pair(-itOrphan->Priority, itOrphan->Tx));
					}
				}
			}
			eng.OrderTxes(block->m_txes);
		}
#endif // UCFG_COIN_GENERATE_TXES_FROM_POOL
	}
	return block;
}

void EmbeddedMiner::UpdateParentChilds() {
	m_childWallets.clear();
	/*!!!
	EXT_FOR (WalletBase *w, m_genWallets) {
		if (m_parentWallets.empty()) {
			WalletBestHash wbh = { w, HashValue::Null() }; //!!!
			m_parentWallets.push_back(wbh);
		}
		else if (w->m_eng->ChainParams.AuxPowEnabled) {
			int chainId = w->m_eng->ChainParams.ChainId;
			EXT_FOR (WalletBase *w1, m_childWallets) {
				if (w1->m_eng->ChainParams.ChainId == chainId) {
					WalletBestHash wbh = { w, HashValue() }; //!!!
					m_parentWallets.push_back(wbh);
					goto LAB_DUP_CHAIN_ID;
				}
			}
			m_childWallets.push_back(w);
LAB_DUP_CHAIN_ID:
			;
		}
	}
	*/

	if (m_childWallets.empty())
		m_merkleSize = 0;
	else {
		for (m_merkleSize=1; m_childWallets.size()>m_merkleSize;)
			m_merkleSize *= 2;
		for (m_merkleNonce=0; m_merkleNonce<=uint32_t(-1); ++m_merkleNonce) {						// find non-clashing nonce
			set<int> idxes;
			EXT_FOR (WalletBase *w, m_childWallets) {
				if (idxes.insert(CalcMerkleIndex(m_merkleSize, w->m_eng->ChainParams.ChainId, m_merkleNonce)).second)
					goto LAB_NONCE_CLASH;
			}
			break;
LAB_NONCE_CLASH:
			;
		}
	}
}

void EmbeddedMiner::RegisterForMining() {
	if (!Started) {

		EXT_LOCK (m_wallet.m_eng->Mtx) {
			m_wallet.ReserveGenKey();
		}
		UpdateParentChilds();
		base::Start(&m_wallet.m_peng->m_cdb.m_tr);
	}
}

void EmbeddedMiner::UnregisterForMining() {
	UpdateParentChilds();
	base::Stop();
}

void WalletBase::RegisterForMining(WalletBase* wallet) {
	if (!Miner.get()) {
		Miner.reset(new EmbeddedMiner(*(Wallet*)wallet));
	}
	dynamic_cast<EmbeddedMiner*>(Miner.get())->RegisterForMining();
}

void WalletBase::UnregisterForMining(WalletBase* wallet) {
	if (Miner.get()) {
		dynamic_cast<EmbeddedMiner*>(Miner.get())->UnregisterForMining();
		EXT_LOCKED(MtxMiner, Miner.reset());
	}
}

void EmbeddedMiner::SetUniqueExtraNonce(Block& block, CoinEng& eng) {
	ASSERT(block.Height != -1);

	CCoinEngThreadKeeper engKeeper(&eng);
	do {
		++m_extraNonce;
		Blob blob(s_mergedMiningHeader, sizeof(s_mergedMiningHeader));
		Blob aux;				//!!!TODO
		blob = blob + aux;
		MemoryStream ms;
		ScriptWriter wr(ms);
		if (block->Ver >= 2)
			wr << int64_t(block.Height);
		wr << BigInteger(to_time_t(block.get_Timestamp())) << BigInteger(m_extraNonce) << Opcode::OP_2 << blob;
		Tx& tx = block.GetFirstTxRef();
		tx->m_txIns.at(0).put_Script(ms);
		tx->m_nBytesOfHash = 0;
	} while (eng.HaveTxInDb(Hash(block.GetFirstTxRef())));
	block->m_merkleRoot.reset();
	block->m_bHashCalculated = false;
}

void EmbeddedMiner::SetSpeedCPD(float speed, float cpd) {
	bool bChanged = Speed != speed;
	base::SetSpeedCPD(speed, cpd);
	m_wallet.Speed = Speed;
	if (bChanged)
		m_wallet.OnChange();
}

ptr<BitcoinWorkData> EmbeddedMiner::GetWork(WebClient*& curWebClient) {
	int msWait = 0;
LAB_START:
	Thread::Sleep(msWait);

	ptr<BitcoinWorkData> wd = new BitcoinWorkData;

	DateTime now = Clock::now();
	Block block(nullptr);

	if (now > m_dtPrevGetwork + seconds(60) ||
		m_wallet.m_eng->IsInitialBlockDownload() || BestHash != EXT_LOCKED(m_wallet.m_eng->Mtx, Hash(m_wallet.m_eng->BestBlock())))
	{
		block = m_wallet.CreateNewBlock();
		if (!block) {
			msWait = 1000;
			goto LAB_START;
		}
		BestHash = Hash(m_wallet.m_peng->BestBlock());
		m_dtPrevGetwork = now;
	} else {
		block = m_prevBlock.Clone();
	}
	CCoinEngThreadKeeper engKeeper(m_wallet.m_eng);
	SetUniqueExtraNonce(block, *m_wallet.m_eng);

	m_prevBlock = block;

	HashValue hashMerkleRoot = block.MerkleRoot;
	EXT_LOCK (m_mtxMerkleToBlock) {
		m_merkleToBlock.insert(make_pair(hashMerkleRoot, block));
	}

	MemoryStream stm;
	ProtocolWriter wr(stm);
	block.WriteHeader(wr);

	Blob blob = stm.AsSpan();
	size_t len = blob.size();

	wd->HashTarget = HashValue::FromDifficultyBits(block->get_DifficultyTarget().m_value);
	wd->Timestamp = now;
	wd->HashAlgo = m_wallet.m_eng->ChainParams.HashAlgo;
	wd->Height = MaxHeight = block.Height;

#ifdef X_DEBUG//!!!D
	wd->Height = MaxHeight = 590050;
	blob = Blob::FromHexString("70000000b528872c4636b188198c2befa8688c7ea48ec70c38996b3c31c75c0a000000004d72851cb2e78cb32e0231fd5d313e82c35df2cce9186902ba27cc5478797313ef8c5355f1931b1c35796600");
	HashValue merkle(ConstBuf(blob.constData()+36, 32));
	m_merkleToBlock[hashMerkleRoot] = block;
#endif

	switch (wd->HashAlgo) {
	case Coin::HashAlgo::Sha256:
		blob.resize(128);
		FormatHashBlocks(blob.data(), len);

		wd->Hash1 = Blob(0, 64);
		FormatHashBlocks(wd->Hash1.data(), 32);

		wd->Data = blob;
		wd->Midstate = CalcSha256Midstate(wd->Data);
		break;
	default:
		{
			uint32_t *pd = (uint32_t*)blob.data();
		for (int i = 0, n = blob.size() / 4; i < n; ++i)
				pd[i] = betoh(pd[i]);
			wd->Data = blob;
		}
	}


	return wd;
}

void EmbeddedMiner::SubmitResult(WebClient*& curWebClient, const BitcoinWorkData& wd) {
	if (wd.Data.size() != 128 && wd.Data.size() != 80)
		Throw(errc::invalid_argument);
	Blob data = wd.Data;
	uint32_t* pd = (uint32_t*)data.data();
	for (int i = 0, n = wd.Data.size() / 4; i < n; ++i)
		pd[i] = betoh(pd[i]);

	TRC(0, "FOUND BLOCK " << wd.Data);

	HashValue merkle(ConstBuf(data.constData()+36, 32));

	Block block(nullptr);
	WalletBase* wb = &m_wallet;
	EXT_LOCK (m_mtxMerkleToBlock) {
		auto it = m_merkleToBlock.find(merkle);
		if (it == m_merkleToBlock.end())
			Throw(E_FAIL);		//!!!?TODO
		block = it->second.first;
		m_merkleToBlock.erase(it);
	}
	block->Timestamp = DateTime::from_time_t(letoh(pd[17]));
	block->Nonce = letoh(pd[19]);
	block->m_bHashCalculated = false;
	TRC(0, "Hash of Found Block: " << Hash(block));
	CCoinEngThreadKeeper engKeeper(m_wallet.m_eng);
	ptr<BlockMessage> m = new BlockMessage(block);
	EXT_LOCK (m_wallet.m_eng->Mtx) {
		m_wallet.m_eng->Broadcast(m.get());
		m_wallet.m_peng->m_cdb.RemoveKeyInfoFromReserved(m_wallet.m_genKeyInfo);
		m_wallet.ReserveGenKey();
		block.Process();
	}
}


} // Coin::

#endif // UCFG_COIN_GENERATE
