/*######   Copyright (c) 2013-2015 Ufasoft  http://ufasoft.com  mailto:support@ufasoft.com,  Sergey Pavlov  mailto:dev@ufasoft.com ####
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
public:
	EmbeddedMiner(Wallet& wallet)
		:	m_wallet(wallet)
		,	m_merkleToBlock(10)
		,	m_prevBlock(nullptr)
	{
		HashAlgo = m_wallet.m_eng->ChainParams.HashAlgo;

#	ifdef X_DEBUG //!!!D
		ThreadCount = 0;
#	endif
	}

	void RegisterForMining();
	void UnregisterForMining();
protected:
	ptr<BitcoinWorkData> GetWork(WebClient*& curWebClient) override;
	void SubmitResult(WebClient*& curWebClient, const BitcoinWorkData& wd) override;
private:
	Wallet& m_wallet;

	HashValue BestHash;

	vector<WalletBase*> m_childWallets;
	int m_merkleSize;
	uint32_t m_merkleNonce;
	DateTime m_dtPrevGetwork;
	uint32_t m_extraNonce;
	Block m_prevBlock;
	
	mutex m_mtxMerkleToBlock;
	LruMap<HashValue, Block> m_merkleToBlock;

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
		:	Tx(tx)
		,	Priority(0)
	{}
};

typedef list<COrphan> COrhpans;

void WalletBase::ReserveGenKey() {
	EXT_LOCK (m_eng->Mtx) {
		pair<Blob, HashValue160> pp = GetReservedPublicKey();
		m_genPubKey = pp.first;
		m_genHash160 = pp.second;
	}
}

Block WalletBase::CreateNewBlock() {
	CCoinEngThreadKeeper engKeeper(&get_Eng());

	if (m_eng->IsInitialBlockDownload())
		return Block(nullptr);

	Block block;
	Tx tx;
	tx.EnsureCreate();
	tx.m_pimpl->m_txIns.resize(1);
	tx.m_pimpl->m_bLoadedIns = true;
	tx.TxOuts().resize(1);
	int64_t nFees = 0;
	EXT_LOCK (m_eng->Mtx) {
		MemoryStream ms;
		ScriptWriter wr(ms);
		if (m_genPubKey.Size != 0)
			wr << m_genPubKey << OP_CHECKSIG;
		else
			wr << OP_DUP << OP_HASH160 << HashValue160(m_genHash160) << OP_EQUALVERIFY << OP_CHECKSIG;

		tx.TxOuts()[0].m_pkScript = Blob(ConstBuf(ms));
		block.Add(tx);

		COrhpans orphans;
		unordered_map<HashValue, vector<COrhpans::iterator>> mapDependers;
		multimap<double, Tx> mapPriority;

		EXT_LOCK (m_eng->TxPool.Mtx) {
			for (auto it=m_eng->TxPool.m_hashToTx.begin(), e=m_eng->TxPool.m_hashToTx.end(); it!=e; ++it) {
				const Tx& tx = it->second;
				if (!tx.IsCoinBase() && tx.IsFinal()) {
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

		Block bestBlock = m_eng->BestBlock();
		block.m_pimpl->PrevBlockHash = Hash(bestBlock);
		block.GetFirstTxRef().TxOuts()[0].Value = m_eng->GetSubsidy(bestBlock.Height+1, block.m_pimpl->PrevBlockHash)+nFees;
		block.m_pimpl->Height = bestBlock.Height+1;
		block.m_pimpl->Timestamp = m_eng->GetTimestampForNextBlock();
		block.m_pimpl->DifficultyTargetBits = m_eng->GetNextTarget(bestBlock, block).m_value;
		block.m_pimpl->Nonce = 0;

		CoinsView view;
		int nBlockSigOps = 100;
		for (uint32_t cbBlockSize=1000; !mapPriority.empty();) {
			auto it = mapPriority.begin();
			double priority = -it->first;
			Tx tx = it->second;
			mapPriority.erase(it);		

			uint32_t cbNewBlockSize = cbBlockSize + tx.GetSerializeSize();

			if (cbNewBlockSize < MAX_BLOCK_SIZE_GEN) {
				bool bAllowFree = cbNewBlockSize<400 || Tx::AllowFree(priority);
				int64_t minFee = tx.GetMinFee(cbBlockSize, cbNewBlockSize<400 || Tx::AllowFree(priority));

				CoinsView viewTmp;
				viewTmp.TxMap = view.TxMap;
				try {
					tx.ConnectInputs(viewTmp, m_eng->BestBlockHeight()+1, nBlockSigOps, nFees, false, true, minFee, block.DifficultyTarget);
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
		}
	}
	return block;
}

void EmbeddedMiner::UpdateParentChilds() {
	m_childWallets.clear();
	/*!!!
	EXT_FOR (WalletBase *w, m_genWallets) {
		if (m_parentWallets.empty()) {
			WalletBestHash wbh = { w, HashValue() }; //!!!
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
		Miner.reset();
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
		if (block.Ver >= 2)
			wr << int64_t(block.Height);
		wr << BigInteger(to_time_t(block.get_Timestamp())) << BigInteger(m_extraNonce) << OP_2 << blob;
		Tx& tx = block.GetFirstTxRef();
		tx.m_pimpl->m_txIns.at(0).put_Script(ms);
		tx.m_pimpl->m_nBytesOfHash = 0;
	} while (eng.HaveTxInDb(Hash(block.GetFirstTxRef())));
	block.m_pimpl->m_merkleRoot.reset();
	block.m_pimpl->m_hash.reset();
}

ptr<BitcoinWorkData> EmbeddedMiner::GetWork(WebClient*& curWebClient) {
	int msWait = 0;
LAB_START:
	Thread::Sleep(msWait);

	ptr<BitcoinWorkData> wd = new BitcoinWorkData;
	m_wallet.Speed = Speed;
	m_wallet.OnChange();

	DateTime now = DateTime::UtcNow();		
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
	BinaryWriter wr(stm);
	block.WriteHeader(wr);

	Blob blob = stm;
	size_t len = blob.Size;
	blob.Size = 128;
	FormatHashBlocks(blob.data(), len);

	wd->Hash1 = Blob(0, 64);
	FormatHashBlocks(wd->Hash1.data(), 32);

	wd->HashTarget = HashValue::FromDifficultyBits(block.get_DifficultyTarget().m_value);
	wd->Timestamp = now;
	wd->Data = blob;
	wd->Midstate = CalcSha256Midstate(wd->Data);
	wd->HashAlgo = m_wallet.m_eng->ChainParams.HashAlgo;

	return wd;
}

void EmbeddedMiner::SubmitResult(WebClient*& curWebClient, const BitcoinWorkData& wd) {
	TRC(0, "FOUND BLOCK " << wd.Data);

	if (wd.Data.Size != 128)
		Throw(E_INVALIDARG);
	Blob data = wd.Data;
	uint32_t *pd = (uint32_t*)data.data();
	for (int i=0; i<128/4; ++i)
		pd[i] = _byteswap_ulong(pd[i]);

	HashValue merkle(ConstBuf(data.constData()+36, 32));

	Block block(nullptr);
	WalletBase *wb = 0;
	EXT_LOCK (m_mtxMerkleToBlock) {
		auto it = m_merkleToBlock.find(merkle);
		if (it == m_merkleToBlock.end())
			Throw(E_FAIL);		//!!!?TODO
		block = it->second.first;
		m_merkleToBlock.erase(it);
	}
	block.m_pimpl->Timestamp = DateTime::from_time_t(letoh(pd[17]));
	block.m_pimpl->Nonce = letoh(pd[19]);
	block.m_pimpl->m_hash.reset();
	CCoinEngThreadKeeper engKeeper(m_wallet.m_eng);
	ptr<BlockMessage> m = new BlockMessage(block);
	EXT_LOCK (m_wallet.m_eng->Mtx) {
		EXT_LOCK (m_wallet.m_eng->MtxPeers) {
			for (auto it=begin(wb->m_eng->Links); it!=end(wb->m_eng->Links); ++it) {			// early broadcast found Block
				CoinLink& link = static_cast<CoinLink&>(**it);
				link.Send(m);
			}
		}
		m_wallet.m_peng->m_cdb.RemovePubHash160FromReserved(m_wallet.m_genHash160);
		m_wallet.ReserveGenKey();
		block.Process();
	}
}


} // Coin::

#endif // UCFG_COIN_GENERATE

