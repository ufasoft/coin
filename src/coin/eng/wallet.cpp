/*######   Copyright (c) 2011-2019 Ufasoft  http://ufasoft.com  mailto:support@ufasoft.com,  Sergey Pavlov  mailto:dev@ufasoft.com ####
#                                                                                                                                     #
# 		See LICENSE for licensing information                                                                                         #
#####################################################################################################################################*/

#include <el/ext.h>


#include "wallet.h"
#include "script.h"

#ifdef C_DEBUG//!!!D
#	include <el/xml.h>
	namespace Coin {
#		include "../blockexplorer/xml-write.cpp"
	}
#endif

namespace Coin {

EXT_THREAD_PTR(Wallet) t_pWallet;

DbWriter& operator<<(DbWriter& wr, const WalletTx& wtx) {
	wr << static_cast<const Tx&>(wtx);
	Write(wr, wtx.PrevTxes);
	return wr;
}

const DbReader& operator>>(const DbReader& rd, WalletTx& wtx) {
	rd >> static_cast<Tx&>(wtx);
	Read(rd, wtx.PrevTxes);
	return rd;
}

Wallet::Wallet(CoinEng& eng)
	:	base(eng)
{
	Init();
}

Wallet::Wallet(CoinDb& cdb, RCString name)
	:	m_peng(CoinEng::CreateObject(cdb, name))
{
	m_eng.reset(m_peng.get());
	Init();
}

void Wallet::Init() {
	Progress = 1;
	CurrentHeight = -1;
	m_eng->Events.Subscribers.push_back(this);
}

WalletTx::WalletTx(const Tx& tx)
	:	base(tx)
	,	To(Eng())
	,	Timestamp(Clock::now())
{}

void WalletTx::LoadFromDb(DbDataReader& sr, bool bLoadExt) {
	Timestamp = DateTime::from_time_t(sr.GetInt32(0));
	CMemReadStream stm(sr.GetBytes(1));
	Coin::DbReader rd(stm, &Eng());
	rd.BlockchainDb = false;
	rd >> _self;
	Height = sr.GetInt32(2);
	Comment = sr.GetString(3);
	m_bFromMe = sr.GetInt32(4);

	if (bLoadExt) {
		Amount = sr.GetInt64(4);
		int nOut = sr.GetInt32(5);
		const TxOut& txOut = TxOuts().at(nOut);
		To = TxOut::CheckStandardType(txOut.ScriptPubKey);
	}
}

bool WalletTx::IsConfirmed(Wallet& wallet) const {
	if (!IsFinal())
		return false;
	if (DepthInMainChain >= 1)
		return true;
	if (!m_bFromMe && !wallet.IsFromMe(_self))
		return false;
	EXT_FOR (const Tx& tx, PrevTxes) {
		if (!tx.IsFinal())
			return false;
		if (tx.DepthInMainChain < 1) {
			if (!wallet.IsFromMe(tx))
				return false;
		}
	}
	return true;
}

String WalletTx::GetComment() const {
	String s = m_pimpl->GetComment();
	String r = s.empty() ? String() : s;
	return Comment.empty() ? r : r + ". " + Comment;
}

int64_t Penny::get_Debit() const {
	Throw(E_NOTIMPL);
	/*!!!
	Wallet& w = Wallet::Current();
	Wallet::COutPoint2Penny::iterator it == w.OutPoint2Penny.find(OutPoint);
	return it != w.OutPointToTxOut.end() && w.IsMine(it->second) ? it->second.Value : 0;
	*/
}

KeyInfo WalletSigner::GetMyKeyInfo(const HashValue160& hash160) {
	return m_wallet.m_eng->m_cdb.GetMyKeyInfo(hash160);
}

KeyInfo WalletSigner::GetMyKeyInfoByScriptHash(const HashValue160& hash160) {
	return m_wallet.m_eng->m_cdb.GetMyKeyInfoByScriptHash(hash160);
}

Blob WalletSigner::GetMyRedeemScript(const HashValue160& hash160) {
	return m_wallet.m_eng->m_cdb.GetMyRedeemScript(hash160);
}


bool Wallet::IsToMe(const Tx& tx) {
	CoinDb& cdb = m_eng->m_cdb;
	EXT_FOR (const TxOut& txOut, tx.TxOuts()) {
		if (cdb.FindMine(txOut.get_ScriptPubKey()))
			return true;
	}
	return false;
}

bool Wallet::IsFromMe(const Tx& tx) {
	EXT_LOCK(m_mtxMyTxHashes) {
		EXT_FOR (const TxIn& txIn, tx.TxIns()) {
			if (!m_myTxHashes.count(txIn.PrevOutPoint.TxHash))
				return false;
		}
	}
	EXT_LOCK (m_eng->m_cdb.MtxDb) {
		SqliteCommand& cmd = m_eng->m_cdb.m_cmdIsFromMe.Bind(1, m_dbNetId);
		EXT_FOR (const TxIn& txIn, tx.TxIns()) {
			if (cmd.Bind(2, txIn.PrevOutPoint.TxHash.ToSpan())
					.Bind(3, txIn.PrevOutPoint.Index)
					.ExecuteReader().Read())
				return true;
		}
	}
	return false;
}

int64_t Wallet::GetTxId(const HashValue& hashTx) {
	DbDataReader dr = m_eng->m_cdb.CmdGetTxId.Bind(1, m_dbNetId).Bind(2, hashTx.ToSpan()).ExecuteReader();
	return dr.Read() ? dr.GetInt64(0) : -1;
}

int64_t Wallet::Add(WalletTx& wtx, bool bPending) {
	HashValue hashTx = Hash(wtx);
	EXT_LOCKED(m_mtxMyTxHashes, m_myTxHashes.insert(hashTx));
	int64_t r = GetTxId(hashTx);
	if (r != -1) {
		SqliteCommand cmd(EXT_STR("UPDATE mytxes SET blockord=? WHERE id=" << r), m_eng->m_cdb.m_dbWallet);
		cmd.Bind(1, wtx.Height >= 0 ? wtx.Height : (bPending ? -1 : -2));
		cmd.ExecuteNonQuery();
	} else {
		wtx.Timestamp = wtx.Height >= 0 ? m_eng->GetBlockByHeight(wtx.Height).Timestamp : Clock::now();
		if (wtx.IsCoinBase() && wtx.Comment.empty())
			wtx.Comment = "Mined";
		MemoryStream ms;
		DbWriter wr(ms);
		wr.BlockchainDb = false;
		wr << wtx;
		SqliteCommand("INSERT INTO mytxes (netid, hash, data, timestamp, blockord, comment, fromme) VALUES(?, ?, ?, ?, ?, ?, ?)", m_eng->m_cdb.m_dbWallet)
			.Bind(1, m_dbNetId)
			.Bind(2, hashTx.ToSpan())
			.Bind(3, ms)
			.Bind(4, to_time_t(wtx.Timestamp))
			.Bind(5, wtx.Height >= 0 ? wtx.Height : (bPending ? -1 : -2))
			.Bind(6, wtx.Comment)
			.Bind(7, bool(wtx.m_bFromMe))
			.ExecuteNonQuery();
		r = m_eng->m_cdb.m_dbWallet.LastInsertRowId;
	}
	return r;
}

bool Wallet::InsertUserTx(int64_t txid, int nout, int64_t value) {
	if (SqliteCommand("SELECT * FROM usertxes WHERE txid=? AND nout=?", m_eng->m_cdb.m_dbWallet).Bind(1, txid).Bind(2, nout).ExecuteReader().Read())
		return false;
	SqliteCommand cmd("INSERT INTO usertxes (txid, nout, value) VALUES(?, ?, ?)", m_eng->m_cdb.m_dbWallet);
	if (nout < 0)
		cmd.Bind(2, nullptr);
	else
		cmd.Bind(2, nout);
	cmd.Bind(1, txid)
		.Bind(3, value)
		.ExecuteNonQuery();
	return true;
}

void Wallet::ProcessMyTx(WalletTx& wtx, bool bPending) {
	CoinDb& cdb = m_eng->m_cdb;
	EXT_LOCK (cdb.MtxDb) {
		TransactionScope dbtx(cdb.m_dbWallet);

		int64_t txid = Add(wtx, bPending);
		HashValue txHash = Hash(wtx);

		bool bIsFromMe = IsFromMe(wtx),
			bIsToMe = IsToMe(wtx);
		TRC(2, txHash << boolalpha << ", IsFromMe: " << bIsFromMe << ", IsToMe: " << bIsToMe);
		if (bIsFromMe) {
//!!!?			SqliteCommand cmdCoin("UPDATE coins SET spent=1 WHERE txid=? AND nout=?", m_eng->m_cdb.m_dbWallet);
			SqliteCommand cmdCoin("DELETE FROM coins WHERE txid=? AND nout=?", m_eng->m_cdb.m_dbWallet);
			EXT_FOR (const TxIn& txIn, wtx.TxIns()) {
				cmdCoin
					.Bind(1, GetTxId(txIn.PrevOutPoint.TxHash))
					.Bind(2, txIn.PrevOutPoint.Index)
					.ExecuteNonQuery();
			}
			HashValue160 hash160;
			for (int i = 0; i < wtx.TxOuts().size(); ++i) {
				const TxOut& txOut = wtx.TxOuts()[i];
				Span scriptPubKey = txOut.get_ScriptPubKey();
				if (!cdb.FindMine(scriptPubKey)) {
					Address dest = TxOut::CheckStandardType(scriptPubKey);
					switch (dest.Type) {
					case AddressType::NonStandard:
					case AddressType::NullData:
						break;
					default:
						InsertUserTx(txid, i, txOut.Value);
					}
				}
			}
//LAB_FOUND:
//			;
		}
		if (bIsToMe) {
			for (int i = 0; i < wtx.TxOuts().size(); ++i) {
				const TxOut& txOut = wtx.TxOuts()[i];
				if (KeyInfo ki = cdb.FindMine(txOut.get_ScriptPubKey())) {
					if (!m_eng->m_cdb.CmdFindCoin.Bind(1, txid).Bind(2, i).ExecuteReader().Read()) {
						m_eng->m_cdb.CmdInsertCoin
							.Bind(1, txid)
							.Bind(2, i)
							.Bind(3, txOut.Value)
							.Bind(4, ki.KeyRowId)
							.Bind(5, txOut.get_ScriptPubKey())
							.ExecuteNonQuery();

						if (!bIsFromMe)
							InsertUserTx(txid, i, txOut.Value);
					}
				}
			}
		}
	}
}

bool Wallet::ProcessTx(const Tx& tx) {
	if (!IsToMe(tx) && !IsFromMe(tx))
		return false;
	WalletTx wtx(tx);
	ProcessMyTx(wtx, false);
	return true;
}

void Wallet::OnProcessTx(const Tx& tx) {
	if (!RescanThread)
		ProcessTx(tx);
}

void Wallet::SetBestBlockHash(const HashValue& hash) {
	BestBlockHash = hash;
	EXT_LOCKED(m_eng->m_cdb.MtxDb, m_eng->m_cdb.CmdSetBestBlockHash.Bind(1, (BestBlockHash = hash).ToSpan()).Bind(2, m_dbNetId).ExecuteNonQuery());
	if (m_iiWalletEvents)
		m_iiWalletEvents->OnStateChanged();
}

void Wallet::OnAlert(Alert *alert) {
	EXT_LOCK (Mtx) {
		Alerts.push_back(alert);
	}

	if (m_iiWalletEvents)
		m_iiWalletEvents->OnStateChanged();
}

void Wallet::OnProcessBlock(const Block& block) {
	if (m_iiWalletEvents)
		m_iiWalletEvents->OnStateChanged();
}

void Wallet::ProcessPendingTxes() {
	EXT_LOCK (m_eng->m_cdb.MtxDb) {
		for (DbDataReader dr = m_eng->m_cdb.CmdPendingTxes.Bind(1, m_dbNetId).ExecuteReader(); dr.Read();) {
			WalletTx wtx;
			wtx.LoadFromDb(dr);
			ProcessMyTx(wtx, wtx.m_bFromMe);
			if (m_iiWalletEvents)
				m_iiWalletEvents->OnStateChanged();
		}
	}
}

void Wallet::OnBlockchainChanged() {
	for (int i = 0; i < 2; ++i) {
		if (Thread::CurrentThread->m_bStop)
			return;
		bool bChanged = false;
		Block bestBlock = m_eng->BestBlock();
		EXT_LOCK(MtxCurrentHeight) {
			if (CurrentHeight >= bestBlock.SafeHeight)
				break;
			if (CurrentHeight == -1 && BestBlockHash) {
				if (BestBlockHash == Hash(bestBlock))
					CurrentHeight = bestBlock.Height;
				break;
			}
		}
		Block block = m_eng->GetBlockByHeight(CurrentHeight + 1);
		if (m_eng->Mode != EngMode::Lite)
			block.LoadToMemory();
		HashValue hash = Hash(block);
		EXT_LOCK(MtxCurrentHeight) {
			EXT_LOCK(m_eng->m_cdb.MtxDb) {
				bool bUpdateWallet = m_eng->Mode == EngMode::Lite;
				if (bUpdateWallet) {
					EXT_FOR(const Tx & tx, block.get_Txes()) {
						bUpdateWallet |= ProcessTx(tx);
					}
				}
				++CurrentHeight;
				BestBlockHash = hash;
				if (bUpdateWallet || !(CurrentHeight & 0xFF) || (CurrentHeight == bestBlock.Height && !m_eng->IsInitialBlockDownload()))
					SetBestBlockHash(hash);
				bChanged = true;
			}
		}
	}
	if (m_eng->Mode == EngMode::Lite && !m_eng->IsInitialBlockDownload())
		ProcessPendingTxes();
	if (m_iiWalletEvents)
		m_iiWalletEvents->OnStateChanged();

#if UCFG_COIN_GENERATE
	int bestHeight = m_eng->BestBlockHeight();
	EXT_LOCK(MtxMiner) {
		if (Miner) {
			EXT_LOCK(Miner->m_csCurrentData) {
#ifndef X_DEBUG//!!!D
				Miner->MaxHeight = bestHeight;
				Miner->TaskQueue.clear();
				Miner->WorksToSubmit.clear();
#endif
			}
		}
	}
#endif // UCFG_COIN_GENERATE
}

void Wallet::OnEraseTx(const HashValue& hashTx) {
	EXT_LOCK (m_eng->m_cdb.MtxDb) {
		SqliteCommand(EXT_STR("DELETE FROM mytxes WHERE netid=" << m_dbNetId << " AND hash=?"), m_eng->m_cdb.m_dbWallet)
			.Bind(1, hashTx.ToSpan())
			.ExecuteNonQuery();
	}
}

CompactThread::CompactThread(Coin::Wallet& wallet)
	: base(&wallet.m_eng->m_tr)
	, Wallet(wallet)
	, m_ai(0)
	, m_count(numeric_limits<uint32_t>::max())
{
}

static int CompactProgressHandler(void *p) {
	CompactThread *t = (CompactThread*)p;
	if (++(t->m_ai) >= t->m_count)
		t->m_count *= 2;
	if (t->Wallet.m_iiWalletEvents)
		t->Wallet.m_iiWalletEvents->OnStateChanged();
	return t->m_bStop;
}

void CompactThread::Execute() {
	Name = "CompactThread";

	CoinEng& eng = Wallet.Eng;
	CCoinEngThreadKeeper engKeeper(&eng);

	EXT_LOCK (Wallet.m_eng->Mtx) {
		struct ThreadPointerCleaner {
			Coin::Wallet *m_pWallet;

			~ThreadPointerCleaner() {
				m_pWallet->m_eng->Db->SetProgressHandler(0);
				m_pWallet->CompactThread = nullptr;
				if (m_pWallet->m_iiWalletEvents)
					m_pWallet->m_iiWalletEvents->OnStateChanged();
			}
		} threadPointerCleaner = { &Wallet };

		m_count = uint32_t(file_size(eng.GetDbFilePath()) / (128 * 1024));

		eng.Db->SetProgressHandler(&CompactProgressHandler, this, 4096);
		eng.CommitTransactionIfStarted();
		try {
			eng.Db->Vacuum();
		} catch (RCExc) {
			if (!m_bStop)
				throw;
		}
	}
}

RescanThread::RescanThread(Coin::Wallet& wallet, const HashValue& hashFrom)
	:	base(&wallet.m_eng->m_tr)
	,	Wallet(wallet)
	,	m_i(0)
	,	m_hashFrom(hashFrom)
{
}

void RescanThread::Execute() {
	Name = "RescanThread";

	struct ThreadPointerCleaner {
		Coin::Wallet *m_pWallet;
		SqliteCommand *m_pCmdRescan;
		DbDataReader *m_pDataReader;

		~ThreadPointerCleaner() {
			m_pWallet->RescanThread = nullptr;
		}
	} threadPointerCleaner = { &Wallet }; //!!!R, &cmdRescan, &dr };
	CCoinEngThreadKeeper engKeeper(Wallet.m_eng);

	int ord = 0;
	m_count = Wallet.m_eng->Db->GetMaxHeight()+1;
	if (Block block = Wallet.m_eng->Db->FindBlock(m_hashFrom))
		m_count -= (ord = block.Height+1);
	Wallet.OnSetProgress(0);

CONTINUE_SCAN_LAB:
	while (Wallet.CurrentHeight < Wallet.m_eng->BestBlockHeight()) {
		if (m_bStop)
			goto LAB_STOP;
		Wallet.OnBlockchainChanged();
//!!!		Wallet.OnSetProgress(float(m_i) / m_count);
	}
	if (Block bestBlock = Wallet.m_eng->BestBlock()) {
		HashValue hashBest = Hash(bestBlock);
		if (hashBest == Wallet.BestBlockHash) {
			Wallet.ProcessPendingTxes();
		} else {
			ord = int(ord+m_i);
			m_count = Wallet.m_eng->BestBlockHeight() - ord + 1;
			goto CONTINUE_SCAN_LAB;
		}
	}
LAB_STOP:
	;
}

void Wallet::StartRescan(const HashValue& hashFrom) {
	(RescanThread = new Coin::RescanThread(_self, hashFrom))->Start();
}

void Wallet::StartCompactDatabase() {
	if (!CompactThread)
		(CompactThread = new Coin::CompactThread(_self))->Start();
}

void Wallet::Rescan() {
	if (RescanThread)
		return;

	if (m_eng->Mode == EngMode::Lite) {
		m_eng->PurgeDatabase();
	} else if (m_eng->IsInitialBlockDownload())
		Throw(CoinErr::RescanDisabledDuringInitialDownload);

	EXT_LOCK (Mtx) {
		EXT_LOCK (m_eng->m_cdb.MtxDb) {
			TransactionScope dbtx(m_eng->m_cdb.m_dbWallet);

			SqliteCommand(EXT_STR("DELETE FROM coins WHERE txid IN (SELECT id FROM mytxes WHERE netid=" << m_dbNetId << ")"), m_eng->m_cdb.m_dbWallet).ExecuteNonQuery();
			SqliteCommand(EXT_STR("DELETE FROM usertxes WHERE txid IN (SELECT id FROM mytxes WHERE netid=" << m_dbNetId << ")"), m_eng->m_cdb.m_dbWallet).ExecuteNonQuery();
			SqliteCommand(EXT_STR("UPDATE mytxes SET blockord=-1 WHERE blockord>-15 AND netid=" << m_dbNetId), m_eng->m_cdb.m_dbWallet).ExecuteNonQuery();

			SqliteCommand(EXT_STR("UPDATE nets SET bestblockhash=? WHERE netid=" << m_dbNetId), m_eng->m_cdb.m_dbWallet)
				.Bind(1, HashValue::Null().ToSpan())
				.ExecuteNonQuery();
			CurrentHeight = -1;
			BestBlockHash = HashValue::Null();
			StartRescan();
		}
	}
}

void Wallet::CancelPendingTxes() {
	EXT_LOCK (m_eng->m_cdb.MtxDb) {
		TransactionScope dbtx(m_eng->m_cdb.m_dbWallet);
		SqliteCommand(EXT_STR("UPDATE mytxes SET blockord=-15 WHERE blockord<0 AND blockord>-15 AND netid=" << m_dbNetId), m_eng->m_cdb.m_dbWallet).ExecuteNonQuery();
		SqliteCommand("DELETE FROM coins WHERE txid IN (SELECT id FROM mytxes WHERE blockord=-15)", m_eng->m_cdb.m_dbWallet).ExecuteNonQuery();
		SqliteCommand("DELETE FROM usertxes WHERE txid IN (SELECT id FROM mytxes WHERE blockord=-15)", m_eng->m_cdb.m_dbWallet).ExecuteNonQuery();
	}
	Rescan();

	/*!!!
	EXT_LOCK (m_eng->Mtx) {
		EXT_LOCK (m_eng->m_cdb.MtxDb) {
			vector<int64_t> txesToDelete;
			SqliteCommand cmdTx(EXT_STR("SELECT timestamp, data, blockord, comment, fromme, id FROM mytxes WHERE blockord<0 AND blockord>-15 AND netid=" << m_dbNetId << " ORDER BY timestamp"), m_eng->m_cdb.m_dbWallet);
			for (DbDataReader dr=cmdTx.ExecuteReader(); dr.Read();) {
				try {
					WalletTx wtx;
					wtx.LoadFromDb(dr);
					if (wtx.Fee < m_eng->AllowFreeTxes ? 0 : wtx.GetMinFee(1, false)) {
						int64_t txid = dr.GetInt64(5);
						TRC(1, "Canceling Tx by fee, mytxes.id: " << txid << "\t" << Hash(wtx));
						txesToDelete.push_back(txid);
					}
				} catch (RCExc) {				// Fee cannot be calculated during Initialization
				}
			}
			if (!txesToDelete.empty()) {
				TransactionScope dbtx(m_eng->m_cdb.m_dbWallet);

				EXT_FOR (int64_t txid, txesToDelete) {
					SqliteCommand(EXT_STR("UPDATE mytxes SET blockord=-15 WHERE id=" << txid), m_eng->m_cdb.m_dbWallet).ExecuteNonQuery();
					SqliteCommand(EXT_STR("DELETE FROM coins WHERE txid=" << txid), m_eng->m_cdb.m_dbWallet).ExecuteNonQuery();
					SqliteCommand(EXT_STR("DELETE FROM usertxes WHERE txid=" << txid), m_eng->m_cdb.m_dbWallet).ExecuteNonQuery();
				}
			}
		}
	}*/
}

void Wallet::ReacceptWalletTxes() {
	vector<HashValue> vQueue;
	SqliteCommand cmdTx(EXT_STR("SELECT timestamp, data, blockord, comment, fromme FROM mytxes WHERE blockord<0 AND blockord>-15 AND netid=" << m_dbNetId << " ORDER BY timestamp"), m_eng->m_cdb.m_dbWallet);	//!!! timestamp < " << to_time_t(m_eng->Chain.DtBestReceived) - 5*60 << " AND
	for (DbDataReader dr = cmdTx.ExecuteReader(); dr.Read();) {
		WalletTx wtx;
		wtx.LoadFromDb(dr);

		DBG_LOCAL_IGNORE_CONDITION(CoinErr::TxNotFound);
		try {
			if (!wtx.IsCoinBase()) {
				EXT_FOR (const Tx& txPrev, wtx.PrevTxes) {
					if (!txPrev.IsCoinBase())
						m_eng->TxPool.AddToPool(txPrev, vQueue);
				}
				m_eng->TxPool.AddToPool(wtx, vQueue);
			}
		} catch (TxNotFoundException&) {
		}
	}
}

void Wallet::SetNextResendTime(const DateTime& dt) {
	m_dtNextResend = dt + seconds(Ext::Random().Next(SECONDS_RESEND_PERIODICITY));
	TRC(2, "next resend at " << m_dtNextResend.ToLocalTime());
#ifdef X_DEBUG//!!!D
	m_dtNextResend = dt + seconds(3);
#endif
}

void Wallet::Start() {
	if (m_bLoaded)
		return;
	CCoinEngThreadKeeper engKeeper(m_eng);
	m_eng->Load();

	bool bIsInitialBlockDownload = m_eng->IsInitialBlockDownload();
	EXT_LOCK (m_eng->m_cdb.MtxDb) {
		{
			SqliteCommand cmd("SELECT netid, bestblockhash FROM nets WHERE name=?", m_eng->m_cdb.m_dbWallet);
			DbDataReader dr = cmd.Bind(1, m_eng->ChainParams.Name).ExecuteVector();
			m_dbNetId = dr.GetInt32(0);
			Span mb = dr.GetBytes(1);
			BestBlockHash = mb.size() ? HashValue(mb) : HashValue::Null();
		}
		EXT_LOCK(m_mtxMyTxHashes) {
			SqliteCommand cmd("SELECT hash FROM mytxes", m_eng->m_cdb.m_dbWallet);
			for (DbDataReader dr = cmd.ExecuteReader(); dr.Read();)
				m_myTxHashes.insert(HashValue(dr.GetBytes(0)));
		}
	}
	if (Block block = m_eng->Db->FindBlock(BestBlockHash))
		CurrentHeight = block.Height;

#ifdef X_DEBUG//!!!D
	auto fee = CalcFee(decimal64(0.0003615));
	String sFee = EXT_STR(fee);
#endif

	EXT_LOCK (m_eng->m_cdb.MtxDb) {
		if (m_eng->Mode!=EngMode::Lite) {
			if (!bIsInitialBlockDownload)
				ReacceptWalletTxes();												//!!!TODO

			if (!RescanThread) {
				if (CurrentHeight >= 0 && CurrentHeight < m_eng->BestBlockHeight() || BestBlockHash == HashValue::Null())
					StartRescan(BestBlockHash);
			}
		}
	}

	m_eng->Start();
	SetNextResendTime(Clock::now());
	m_bLoaded = true;
}

void Wallet::Stop() {
	SetBestBlockHash(BestBlockHash);
	m_bLoaded = false;
#if UCFG_COIN_GENERATE
	UnregisterForMining(this);
#endif
	m_eng->Stop();
}

void Wallet::Close() {
	Stop();
}

vector<Address> Wallet::get_MyAddresses() {
	vector<Address> r;
	EXT_LOCK (m_eng->m_cdb.MtxDb) {
		for (CoinDb::CHash160ToKey::iterator it = m_eng->m_cdb.Hash160ToKey.begin(), e = m_eng->m_cdb.Hash160ToKey.end(); it != e; ++it)
			if (it->second.Comment != nullptr)
				r.push_back(it->second.ToAddress());
	}
	return r;
}

vector<Address> Wallet::get_Recipients() {
	CCoinEngThreadKeeper engKeeper(m_eng);

	vector<Address> r;
	EXT_LOCK (m_eng->m_cdb.MtxDb) {
		for (DbDataReader dr = m_eng->m_cdb.CmdRecipients.Bind(1, m_dbNetId).ExecuteReader(); dr.Read();)
			r.push_back(Address(*m_eng, AddressType(dr.GetInt32(0)), HashValue160(dr.GetBytes(1)), 0, dr.GetString(2))); //!!!?
	}
	return r;
}

void Wallet::AddRecipient(const Address& a) {
	if (a.Type != AddressType::P2PKH && a.Type != AddressType::P2SH)	//!!!TODO
		Throw(E_NOTIMPL);
	HashValue160 hash160(a);
	EXT_LOCK (m_eng->m_cdb.MtxDb) {
		SqliteCommand cmdSel(EXT_STR("SELECT netid FROM pubkeys WHERE netid=" << m_dbNetId << " AND type=? AND hash160=?"), m_eng->m_cdb.m_dbWallet);
		cmdSel.Bind(1, (int)a.Type)
			.Bind(2, hash160);
		if (cmdSel.ExecuteReader().Read())
			Throw(CoinErr::RecipientAlreadyPresents);

		SqliteCommand cmd(EXT_STR("INSERT INTO pubkeys (netid, type, hash160, comment) VALUES(" << m_dbNetId << ", ?, ?, ?)"), m_eng->m_cdb.m_dbWallet);
		cmd.Bind(1, (int)a.Type)
			.Bind(2, hash160)
			.Bind(3, a.Comment);
		cmd.ExecuteNonQuery();
	}
}

void Wallet::RemoveRecipient(RCString s) {
	Address a(*m_eng, s);
	if (a.Type != AddressType::P2PKH && a.Type != AddressType::P2SH)	//!!!TODO
		Throw(E_NOTIMPL);
	HashValue160 hash160(a);
	EXT_LOCK (m_eng->m_cdb.MtxDb) {
		SqliteCommand(EXT_STR("DELETE FROM pubkeys WHERE netid=" << m_dbNetId << " AND hash160=?"), m_eng->m_cdb.m_dbWallet)
			.Bind(1, hash160)
			.ExecuteNonQuery();
	}
}

pair<int64_t, int> Wallet::GetBalanceValueExp() {
	return pair<int64_t, int>(EXT_LOCKED(m_eng->m_cdb.MtxDb, m_eng->CheckMoneyRange(m_eng->m_cdb.CmdGetBalance.Bind(1, m_dbNetId).ExecuteInt64Scalar())), m_eng->ChainParams.Log10CoinValue());
}

decimal64 Wallet::get_Balance() {
	pair<int64_t, int> pp = GetBalanceValueExp();
	return make_decimal64((long long)pp.first, -pp.second);
}

WalletTx Wallet::GetTx(const HashValue& hashTx) {
	WalletTx wtx;
	EXT_LOCK (m_eng->m_cdb.MtxDb) {
		DbDataReader dr = m_eng->m_cdb.CmdGetTx.Bind(1, m_dbNetId)
			.Bind(2, hashTx.ToSpan()).ExecuteVector();
		wtx.LoadFromDb(dr);
	}
	return wtx;
}

decimal64 Wallet::GetDecimalFee(const WalletTx& wtx) {
	if (wtx.IsCoinBase())
		return 0;
	int64_t sum = 0;
	EXT_FOR (const TxIn& txIn, wtx.TxIns()) {
		Coin::Tx tx;
		if (!Tx::TryFromDb(txIn.PrevOutPoint.TxHash, &tx))
			tx = GetTx(txIn.PrevOutPoint.TxHash);
		sum += tx.TxOuts().at(txIn.PrevOutPoint.Index).Value;
	}
	int64_t fee = sum - wtx.ValueOut;
	return make_decimal64((long long)fee, -m_eng->ChainParams.Log10CoinValue());
}

void Wallet::SetAddressComment(const Address& addr, RCString comment) {
	if (addr.Type != AddressType::P2PKH && addr.Type != AddressType::P2SH)	//!!!TODO
		Throw(E_NOTIMPL);
	HashValue160 hash160(addr);

	if (m_eng->m_cdb.SetPrivkeyComment(hash160, comment))
		return;

	EXT_LOCK (Mtx) {
		vector<Address> ar = Recipients;
		for (int i = 0; i < ar.size(); ++i) { //!!!TODO  INSERT OR IGNORE; UPDATE
			if (ar[i] == addr) {
				if (ar[i].Type != AddressType::P2PKH && ar[i].Type != AddressType::P2SH)	//!!!TODO
					Throw(E_NOTIMPL);
				HashValue160 hash160(ar[i]);

				SqliteCommand cmd(EXT_STR("UPDATE pubkeys Set comment=? WHERE netid=" << m_dbNetId << " AND pubkey=?"), m_eng->m_cdb.m_dbWallet);
				cmd.Bind(1, comment)
					.Bind(2, Span(hash160));
				try {
					cmd.ExecuteNonQuery();
				} catch (RCExc) {
					SqliteCommand(EXT_STR("INSERT INTO pubkeys (netid, type, pubkey, comment) VALUES (" << m_dbNetId << ", ?, ?)"), m_eng->m_cdb.m_dbWallet)
						.Bind(1, (int)ar[i].Type)
						.Bind(2, Span(hash160))
						.Bind(3, comment)
						.ExecuteNonQuery();
				}
				return;
			}
		}
	}

	Throw(E_FAIL);
}

void Wallet::put_MiningEnabled(bool b) {
#if UCFG_COIN_GENERATE
	if (m_bMiningEnabled = b)
		RegisterForMining(this);
	else
		UnregisterForMining(this);
#endif
}

pair<CanonicalPubKey, HashValue160> Wallet::GetReservedPublicKey() {
	EXT_LOCK (m_eng->m_cdb.MtxDb) {
		KeyInfo ki = m_eng->m_cdb.FindReservedKey();
		const CanonicalPubKey& pubKey = ki ? ki.PubKey : m_eng->m_cdb.Hash160ToKey.begin()->second.PubKey;
		return make_pair(pubKey, pubKey.Hash160);
	}
}

bool Wallet::SelectCoins(uint64_t amount, uint64_t fee, int nConfMine, int nConfTheirs, pair<unordered_set<Penny>, uint64_t>& pp) {
	pp.first.clear();
	pp.second = 0;

	vector<Penny> coins;
	EXT_LOCK (m_eng->m_cdb.MtxDb) {
		SqliteCommand cmd(EXT_STR("SELECT hash, nout, value, pkscript FROM coins JOIN mytxes ON txid=mytxes.id WHERE netid=" << m_dbNetId << " ORDER BY value"), m_eng->m_cdb.m_dbWallet);
		DbDataReader dr = cmd.ExecuteReader();
		while (dr.Read()) {
			Penny coin;
			coin.OutPoint.TxHash = HashValue(dr.GetBytes(0));
			coin.OutPoint.Index = dr.GetInt32(1);
			coin.Value = dr.GetInt64(2);
			coin.PkScript = dr.GetBytes(3);
			coins.push_back(coin);
		}
	}

	Penny coinBestLarger;
	int64_t totalLower = 0;
	for (int i=coins.size(); i--;) {
		Penny& coin = coins[i];
		WalletTx wtx = GetTx(coin.OutPoint.TxHash);
		int depth = wtx.DepthInMainChain;
		if (wtx.IsConfirmed(_self) &&
			(!wtx.IsCoinBase() || depth > m_eng->ChainParams.CoinbaseMaturity+20) &&
			depth >= (wtx.m_bFromMe || IsFromMe(wtx) ? nConfMine : nConfTheirs))
		{
			if (coin.Value >= amount) {
				coinBestLarger = coin;
				coins.erase(coins.begin()+i);
			} else
				totalLower += coin.Value;
		} else
			coins.erase(coins.begin()+i);
	}
	if (totalLower < amount) {
		if (coinBestLarger.Value < amount)
			return false;
		pp.first.insert(coinBestLarger);
		pp.second = coinBestLarger.Value;
		return true;
	}
	for (int i=coins.size(); i--;) {
		Penny& coin = coins[i];
		pp.first.insert(coin);
		pp.second += coin.Value;
		if (pp.second >= amount)
			break;
	}
	ASSERT(pp.second >= amount);
	return true;
}

pair<unordered_set<Penny>, uint64_t> Wallet::SelectCoins(uint64_t amount, uint64_t fee) {
	pair<unordered_set<Penny>, uint64_t> r;
	if (SelectCoins(amount, fee, 1, 6, r) ||
		SelectCoins(amount, fee, 1, 1, r) ||
		SelectCoins(amount, fee, 0, 1, r))
		return r;
	Throw(CoinErr::InsufficientAmount);
}

pair<WalletTx, decimal64> Wallet::CreateTransaction(const KeyInfo& randomKey, const vector<pair<Address, int64_t>>& vSend) {
	if (vSend.empty())
		Throw(errc::invalid_argument);
	int64_t nVal = 0;
	for (int i = 0; i < vSend.size(); ++i)
		m_eng->CheckMoneyRange(nVal += vSend[i].second);

	EXT_LOCK (Mtx) {
		for (int64_t fee = 0;;) {
			WalletTx tx;
			tx.EnsureCreate(*m_eng);
			tx.m_bFromMe = true;

			int64_t nTotal = nVal + fee;
			for (int i = 0; i < vSend.size(); ++i) {
				TxOut txOut(vSend[i].second, vSend[i].first.ToScriptPubKey());
				txOut.CheckForDust();
				tx.TxOuts().push_back(txOut);
			}

			pair<unordered_set<Penny>, uint64_t> ppCoins = SelectCoins(nTotal, 0);
			double priority = 0;
			EXT_FOR (const Penny& penny, ppCoins.first) {
				priority += double(penny.Value) * GetTx(penny.OutPoint.TxHash).DepthInMainChain;

				TxIn txIn;
				txIn.PrevOutPoint = penny.OutPoint;
				tx.m_pimpl->m_txIns.push_back(txIn);
			}
			tx.m_pimpl->m_bLoadedIns = true;
			int64_t nChange = ppCoins.second - nVal - fee;

			if (fee < m_eng->ChainParams.MinTxFee && nChange > 0 && nChange < m_eng->ChainParams.CoinValue/100) {
                int64_t nMoveToFee = std::min(nChange, m_eng->ChainParams.MinTxFee - fee);
                nChange -= nMoveToFee;
                fee += nMoveToFee;
			}
			if (nChange > 0 && nChange < m_eng->ChainParams.MinTxOutAmount)
				fee += exchange(nChange, 0);

			if (nChange > 0) {
				tx.ChangePubKey = GetReservedPublicKey().first;
				const Address& a = vSend[0].first;
				bool isAddress = a.Type == AddressType::P2PKH || a.Type == AddressType::P2SH;
				Blob scriptChange = isAddress ? Address(*m_eng, AddressType::P2PKH, tx.ChangePubKey.Hash160).ToScriptPubKey() : Script::BlobFromPubKey(tx.ChangePubKey);	//!!!?
				tx.TxOuts().insert(tx.TxOuts().begin() + Ext::Random().Next(tx.TxOuts().size()), TxOut(nChange, scriptChange));
			}

			WalletSigner signer(_self, tx);
			int nIn = 0;
			EXT_FOR (const Penny& penny, ppCoins.first) {
				signer.Sign(randomKey, penny.PkScript, nIn++);
			}

			m_eng->CheckForDust(tx);

			uint32_t size = tx.GetSerializeSize();
			if (size >= MAX_STANDARD_TX_SIZE)
				Throw(CoinErr::TxTooLong);
			priority /= size;

//!!!?			int64_t minFee = tx.GetMinFee(1, Tx::AllowFree(priority));
			int64_t minFee = tx.GetMinFee(1, false);
			if (fee >= minFee) {
				decimal64 dfee = make_decimal64((long long)fee, -m_eng->ChainParams.Log10CoinValue());
				TRC(3, make_decimal64((long long)nVal, -m_eng->ChainParams.Log10CoinValue()) << " " << Eng.ChainParams.Symbol << ",  Fee: " << dfee << " " << Eng.ChainParams.Symbol);
				tx.Timestamp = Clock::now();
				return pair<WalletTx, decimal64>(tx, dfee);
			}
			fee = minFee;
		}
	}
	Throw(ExtErr::CodeNotReachable);
}

void Wallet::CheckCreatedTransaction(const Tx& tx) {
	CoinsView view(*m_eng);
	int nBlockSigOps = 0;
	int64_t nFees = 0;
	//!!!TODO:  accepts only transaction with inputs already in the DB. view should check inputs in the mem pool too
	t_features.PayToScriptHash = true;
	tx.ConnectInputs(view, m_eng->BestBlockHeight(), nBlockSigOps, nFees, false, false, tx.GetMinFee(1, m_eng->AllowFreeTxes, MinFeeMode::Relay), m_eng->ChainParams.MaxTarget);	//!!!? MaxTarget
}

void Wallet::Relay(const WalletTx& wtx) {
#ifdef X_DEBUG//!!!D
//	CheckCreatedTransaction(wtx);
#endif

	EXT_FOR (const Tx& tx, wtx.PrevTxes) {
		if (!tx.IsCoinBase()) {
			TxInfo txInfo(tx, tx.GetSerializeSize());
			m_eng->Relay(txInfo);
		}
	}
	if (!wtx.IsCoinBase()) {
		TxInfo txInfo(wtx, wtx.GetSerializeSize());
		m_eng->Relay(txInfo);
	}
}

void Wallet::Commit(WalletTx& wtx) {
	if (wtx.ChangePubKey.Data.size() > 0)
		m_eng->m_cdb.RemovePubKeyFromReserved(wtx.ChangePubKey);
	ProcessMyTx(wtx, true);
	Relay(wtx);
}

void Wallet::ResendWalletTxes(const DateTime& now) {
	if (EXT_LOCKED(m_eng->MtxPeers, m_eng->Links.empty()))
		return;

	if (now >= m_dtNextResend) {
		SetNextResendTime(now);
		if (m_dtLastResend < m_eng->Caches.DtBestReceived) {
			m_dtLastResend = now;

			EXT_LOCK (m_eng->Mtx) {
				EXT_LOCK (m_eng->m_cdb.MtxDb) {
					for (DbDataReader dr=m_eng->m_cdb.CmdPendingTxes.Bind(1, m_dbNetId).ExecuteReader(); dr.Read();) {
						WalletTx wtx;
						wtx.LoadFromDb(dr);
						if (!wtx.IsCoinBase()) {

#ifdef X_DEBUG//!!!D
							{
								XmlTextWriter x("c:\\work\\coin\\tx.log");
								x.Formatting = XmlFormatting::Indented;
								x << wtx;
							}
							int64_t minFee = wtx.GetMinFee();
							minFee = wtx.GetMinFee();
							minFee = wtx.GetMinFee();
							wtx.Check();
#endif

	#ifdef X_DEBUG//!!!D

							TRC(1, "Tx: " << Hash(wtx));
							EXT_FOR (const TxIn& txIn, wtx.TxIns) {
								TRC(1, "TxIn: " << txIn.PrevOutPoint.TxHash);
							}
							EXT_FOR (const TxOut& txOut, wtx.TxOuts) {
								TRC(1, "TxOut: " << txOut.Value);
							}
							vector<HashValue> vQueue;
	//						pair<bool, bool> pp = m_eng->AddToPool(wtx, vQueue);
	#endif
							Relay(wtx);
						}
					}
				}
			}
		}
	}
}

void Wallet::OnPeriodic(const DateTime& now) {
	if (!RescanThread)
		ResendWalletTxes(now);
}

void Wallet::OnPeriodicForLink(Link& link) {
}

void Wallet::SendTo(const decimal64& decAmount, RCString saddr, RCString comment) {
	TRC(0, decAmount << " " << m_eng->ChainParams.Symbol << " to " << saddr << " " << comment);

	if (decAmount > Balance)
		Throw(CoinErr::InsufficientAmount);
	int64_t amount = decimal_to_long_long(decAmount * m_eng->ChainParams.CoinValue);
	if (0 == amount)
		Throw(errc::invalid_argument);

	vector<pair<Address, int64_t>> vSend(1, make_pair(Address(*m_eng, saddr), amount));
	pair<WalletTx, decimal64> pp = CreateTransaction(nullptr, vSend);
	pp.first.Comment = comment;
	t_features.PayToScriptHash = true;

	Commit(pp.first);
	if (m_iiWalletEvents)
		m_iiWalletEvents->OnStateChanged();
}

decimal64 Wallet::CalcFee(const decimal64& amount) {
	DBG_LOCAL_IGNORE_CONDITION(CoinErr::InsufficientAmount);

	if (0 == amount)
		return make_decimal64((long long)m_eng->ChainParams.MinTxFee, - m_eng->ChainParams.Log10CoinValue());
	if (amount > Balance)
		Throw(CoinErr::InsufficientAmount);
	Address addr(*m_eng, AddressType::P2PKH, GetReservedPublicKey().second); //!!!?
	vector<pair<Address, int64_t>> vSend(1, make_pair(addr, decimal_to_long_long(amount * m_eng->ChainParams.CoinValue)));
	KeyInfo randomKey;
	randomKey.m_pimpl->GenRandom();
	return CreateTransaction(randomKey, vSend).second;
}

WalletEng::WalletEng() {
	m_cdb.Load();
	path pathXml = System.GetExeDir() / "coin-chains.xml";
	TRC(1, pathXml);

	if (!exists(pathXml))
		Throw(CoinErr::XmlFileNotFound);
#if UCFG_WIN32
	XmlDocument doc = new XmlDocument;
	doc.Load(pathXml.native());
	XmlNodeReader rd(doc);
#else
	ifstream ifs(pathXml.c_str());
	XmlTextReader rd(ifs);
#endif
	for (bool b = rd.ReadToDescendant("Chain"); b; b = rd.ReadToNextSibling("Chain"))
		Wallets.push_back(new Wallet(m_cdb, rd.GetAttribute("Name")));

	m_cdb.Start();
	if (g_conf.Server && !g_conf.RpcPassword.empty()) {
		Rpc = new Coin::Rpc(_self);
		Rpc->Start();
	}
}

WalletEng::~WalletEng() {
	for (int i=0; i<Wallets.size(); ++i) {
		Wallets[i]->m_eng->Stop();
		Wallets[i]->Close();
	}
	m_cdb.Close();
}

} // Coin::
