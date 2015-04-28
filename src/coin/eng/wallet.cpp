/*######   Copyright (c) 2011-2015 Ufasoft  http://ufasoft.com  mailto:support@ufasoft.com,  Sergey Pavlov  mailto:dev@ufasoft.com ####
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
	m_eng->m_iiEngEvents.reset(this);
}

DbWriter& operator<<(DbWriter& wr, const WalletTx& wtx) {
	wr << static_cast<const Tx&>(wtx) << wtx.PrevTxes;
	return wr;
}

const DbReader& operator>>(const DbReader& rd, WalletTx& wtx) {
	rd >> static_cast<Tx&>(wtx) >> wtx.PrevTxes;
	return rd;
}

WalletTx::WalletTx(const Tx& tx)
	:	base(tx)
	,	To(Eng())
	,	Timestamp(DateTime::UtcNow())
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
		To = Address(Eng(), HashValue160(sr.GetBytes(5)));
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


bool IsPayToScriptHash(const Blob& script) {
	const byte *p = script.constData();
	return script.Size==23 && p[0]==OP_HASH160 && p[1]==20 && p[22]==OP_EQUAL;
}

bool VerifyScript(const Blob& scriptSig, const Blob& scriptPk, const Tx& txTo, uint32_t nIn, int32_t nHashType) {
	Vm vm;
	bool r = vm.Eval(scriptSig, txTo, nIn, nHashType);
	if (r) {
		Vm vmInner;
		if (t_bPayToScriptHash)
			vmInner = vm;
		if (r = (vm.Eval(scriptPk, txTo, nIn, nHashType)
				&& !vm.Stack.empty() && ToBool(vm.Stack.back())))
		{
			if (t_bPayToScriptHash && IsPayToScriptHash(scriptPk)) {
				Blob pubKeySerialized = vmInner.Stack.back();
				vmInner.Stack.pop_back();
				r = vmInner.Eval(pubKeySerialized, txTo, nIn, nHashType)
					&& vmInner.Eval(scriptPk, txTo, nIn, nHashType)
					&& !vmInner.Stack.empty() && ToBool(vm.Stack.back());
			}
		}
	}
	return r;
}

void VerifySignature(const Tx& txFrom, const Tx& txTo, uint32_t nIn, int32_t nHashType) {
    const TxIn& txIn = txTo.TxIns().at(nIn);
    if (txIn.PrevOutPoint.Index >= txFrom.TxOuts().size())
        Throw(E_FAIL);
    const TxOut& txOut = txFrom.TxOuts()[txIn.PrevOutPoint.Index]; 
    if (txIn.PrevOutPoint.TxHash != Hash(txFrom) || !VerifyScript(txIn.Script(), txOut.PkScript, txTo, nIn, nHashType)) {
#ifdef X_DEBUG//!!!D

		TRC(1, "txTo " << Hash(txTo));		
//		bool bC = txIn.PrevOutPoint.TxHash == Hash(txFrom);
//		bool bb = VerifyScript(txIn.Script(), txOut.PkScript, txTo, nIn, nHashType);
//		bb = bb;
#endif
		Throw(CoinErr::VerifySignatureFailed);
	}
}

void Wallet::Sign(ECDsa *randomDsa, const Blob& pkScript, Tx& txTo, uint32_t nIn, byte nHashType, const Blob& scriptPrereq) {
    TxIn& txIn = txTo.m_pimpl->m_txIns.at(nIn);
	HashValue hash = SignatureHash(scriptPrereq+pkScript, *txTo.m_pimpl, nIn, nHashType);

	MyKeyInfo ki;
	HashValue160 hash160;
	Blob pubkey;
	int sigType = TryParseDestination(pkScript, hash160, pubkey);
	switch (sigType) {
	case 33:
	case 65:
		ASSERT(Eng.m_cdb.GetMyKeyInfo(hash160).PubKey == pubkey);
	case 20:
		ki = m_eng->m_cdb.GetMyKeyInfo(hash160);
		break;
	default:
		Throw(E_FAIL);
	}
	Blob sig;
	if (randomDsa)
		sig = randomDsa->SignHash(hash)+Blob(&nHashType, 1);
	else {
		ECDsa dsa(ki.Key);
		sig = dsa.SignHash(hash)+Blob(&nHashType, 1);
	}
	MemoryStream msSig;
	ScriptWriter wrSig(msSig);
	wrSig << sig;
	if (20 == sigType)
		wrSig << ki.PubKey;
	txIn.put_Script(msSig);

	if (!randomDsa && 0==scriptPrereq.Size && !VerifyScript(txIn.Script(), pkScript, txTo, nIn, 0))
		Throw(E_FAIL);
}

const MyKeyInfo *Wallet::FindMine(const TxOut& txOut) {
	HashValue160 hash160;
	Blob pubkey;
	const MyKeyInfo *r = nullptr;
	switch (txOut.TryParseDestination(hash160, pubkey)) {
	case 33:
	case 65:
	case 20:
		EXT_LOCK (m_eng->m_cdb.MtxDb) {
			CoinDb::CHash160ToMyKey::iterator it = m_eng->m_cdb.Hash160ToMyKey.find(hash160);
			if (it!=m_eng->m_cdb.Hash160ToMyKey.end())
				r = &it->second;
		}
	}
	return r;
}

bool Wallet::IsMine(const Tx& tx) {	
	EXT_FOR (const TxOut& txOut, tx.TxOuts()) {
		if (FindMine(txOut))
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
			if (cmd.Bind(2, txIn.PrevOutPoint.TxHash)
					.Bind(3, txIn.PrevOutPoint.Index)
					.ExecuteReader().Read())
				return true;			
		}
	}
	return false;
}

int64_t Wallet::GetTxId(const HashValue& hashTx) {
	DbDataReader dr = m_eng->m_cdb.CmdGetTxId.Bind(1, m_dbNetId).Bind(2, hashTx).ExecuteReader();
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
		wtx.Timestamp = wtx.Height >= 0 ? m_eng->GetBlockByHeight(wtx.Height).Timestamp : DateTime::UtcNow();
		if (wtx.IsCoinBase() && wtx.Comment.empty())
			wtx.Comment = "Mined";
		MemoryStream ms;
		DbWriter wr(ms);
		wr.BlockchainDb = false;
		wr << wtx;
		SqliteCommand("INSERT INTO mytxes (netid, hash, data, timestamp, blockord, comment, fromme) VALUES(?, ?, ?, ?, ?, ?, ?)", m_eng->m_cdb.m_dbWallet)
			.Bind(1, m_dbNetId)
			.Bind(2, hashTx)
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

bool Wallet::InsertUserTx(int64_t txid, int nout, int64_t value, const HashValue160& hash160) {
	if (SqliteCommand("SELECT * FROM usertxes WHERE txid=? AND nout=?", m_eng->m_cdb.m_dbWallet).Bind(1, txid).Bind(2, nout).ExecuteReader().Read())
		return false;
	if (nout < 0) {
		SqliteCommand("INSERT INTO usertxes (txid, nout, value, addr160) VALUES(?, NULL, ?, ?)", m_eng->m_cdb.m_dbWallet)
			.Bind(1, txid).Bind(2, value).Bind(3, hash160)
			.ExecuteNonQuery();
	} else {
		SqliteCommand("INSERT INTO usertxes (txid, nout, value, addr160) VALUES(?, ?, ?, ?)", m_eng->m_cdb.m_dbWallet)
			.Bind(1, txid).Bind(2, nout).Bind(3, value).Bind(4, hash160)
			.ExecuteNonQuery();
	}
	return true;
}

void Wallet::ProcessMyTx(WalletTx& wtx, bool bPending) {
	EXT_LOCK (m_eng->m_cdb.MtxDb) {
		TransactionScope dbtx(m_eng->m_cdb.m_dbWallet);

		int64_t txid = Add(wtx, bPending);
		HashValue txHash = Hash(wtx);

		TRC(2, txHash);

		bool bIsFromMe = IsFromMe(wtx);
		if (bIsFromMe) {
//!!!?			SqliteCommand cmdCoin("UPDATE coins SET spent=1 WHERE txid=? AND nout=?", m_eng->m_cdb.m_dbWallet);	
			SqliteCommand cmdCoin("DELETE FROM coins WHERE txid=? AND nout=?", m_eng->m_cdb.m_dbWallet);			
			EXT_FOR (const TxIn& txIn, wtx.TxIns()) {
				cmdCoin.Bind(1, GetTxId(txIn.PrevOutPoint.TxHash))
					.Bind(2, txIn.PrevOutPoint.Index)
					.ExecuteNonQuery();
			}
			HashValue160 hash160;
			for (int i=0; i<wtx.TxOuts().size(); ++i) {
				const TxOut& txOut = wtx.TxOuts()[i];
				if (!FindMine(txOut)) {
					Blob pubkey;
					bool r = false;
					switch (txOut.TryParseDestination(hash160, pubkey)) {
					case 33:
					case 65:
					case 20:
						InsertUserTx(txid, i, txOut.Value, hash160);
						break;
//						goto LAB_FOUND;
					}					
				}
			}
//LAB_FOUND:
//			;
		}
		if (IsMine(wtx)) {
			for (int i=0; i<wtx.TxOuts().size(); ++i) {
				const TxOut& txOut = wtx.TxOuts()[i];
				if (const MyKeyInfo *ki = FindMine(txOut)) {
					if (!m_eng->m_cdb.CmdFindCoin.Bind(1, txid).Bind(2, i).ExecuteReader().Read()) {
						m_eng->m_cdb.CmdInsertCoin.Bind(1, txid).Bind(2, i).Bind(3, txOut.Value).Bind(4, ki->KeyRowid).Bind(5, txOut.get_PkScript())
							.ExecuteNonQuery();

						if (!bIsFromMe)
							InsertUserTx(txid, i, txOut.Value, ki->get_Hash160());
					}
				}
			}
		}
	}
}

bool Wallet::ProcessTx(const Tx& tx) {
	if (!IsMine(tx) && !IsFromMe(tx))
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
	EXT_LOCKED(m_eng->m_cdb.MtxDb, m_eng->m_cdb.CmdSetBestBlockHash.Bind(1, BestBlockHash=hash).Bind(2, m_dbNetId).ExecuteNonQuery());
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
		for (DbDataReader dr=m_eng->m_cdb.CmdPendingTxes.Bind(1, m_dbNetId).ExecuteReader(); dr.Read();) {
			WalletTx wtx;
			wtx.LoadFromDb(dr);
			ProcessMyTx(wtx, wtx.m_bFromMe);
			if (m_iiWalletEvents)
				m_iiWalletEvents->OnStateChanged();
		}
	}
}

void Wallet::OnBlockchainChanged() {
	for (int i=0; i<2; ++i) {
		if (Thread::CurrentThread->m_bStop)
			break;
		bool bChanged = false;
		EXT_LOCK (MtxCurrentHeight) {
			Block bestBlock = m_eng->BestBlock();
			if (CurrentHeight >= bestBlock.SafeHeight)
				break;
			if (CurrentHeight == -1 && BestBlockHash != HashValue()) {
				if (BestBlockHash == Hash(bestBlock))
					CurrentHeight = bestBlock.Height;
				break;
			}
			Block block = m_eng->GetBlockByHeight(CurrentHeight+1);
			if (m_eng->Mode==EngMode::Lite) {
				HashValue hash = Hash(block);
				EXT_LOCK (m_eng->m_cdb.MtxDb) {
					++CurrentHeight;
					BestBlockHash = hash;
					if (!(CurrentHeight & 0xFF) || (CurrentHeight==bestBlock.Height && !m_eng->IsInitialBlockDownload()) )
						SetBestBlockHash(hash);
					bChanged = true;
				}
			} else {
				block.LoadToMemory();	
				HashValue hash = Hash(block);
				EXT_LOCK (m_eng->m_cdb.MtxDb) {
					bool bUpdateWallet = false;
					EXT_FOR (const Tx& tx, block.get_Txes()) {
						bUpdateWallet |= ProcessTx(tx);
					}
					++CurrentHeight;
					BestBlockHash = hash;
					if (bUpdateWallet || !(CurrentHeight & 0xFF) || (CurrentHeight==bestBlock.Height && !m_eng->IsInitialBlockDownload()) )
						SetBestBlockHash(hash);
					bChanged = true;
				}
			}
		}
	}
	if (m_eng->Mode==EngMode::Lite && !m_eng->IsInitialBlockDownload())
		ProcessPendingTxes();
	if (m_iiWalletEvents)
		m_iiWalletEvents->OnStateChanged();
}

void Wallet::OnEraseTx(const HashValue& hashTx) {
	EXT_LOCK (m_eng->m_cdb.MtxDb) {
		SqliteCommand(EXT_STR("DELETE FROM mytxes WHERE netid=" << m_dbNetId << " AND hash=?"), m_eng->m_cdb.m_dbWallet)
			.Bind(1, hashTx)
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
		} threadPointerCleaner = { &Wallet};

		m_count = uint32_t(file_size(eng.GetDbFilePath()) / (128*1024));

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

	if (m_eng->Mode==EngMode::Lite) {
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
				.Bind(1, HashValue())
				.ExecuteNonQuery();
			CurrentHeight = -1;
			BestBlockHash = HashValue();
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
	for (DbDataReader dr=cmdTx.ExecuteReader(); dr.Read();) {
		WalletTx wtx;
		wtx.LoadFromDb(dr);

		DBG_LOCAL_IGNORE_CONDITION(CoinErr::TxNotFound);
		try {
			if (!wtx.IsCoinBase()) {
				EXT_FOR (const Tx& txPrev, wtx.PrevTxes) {
					if (!txPrev.IsCoinBase())
						m_eng->AddToPool(txPrev, vQueue);
				}
				m_eng->AddToPool(wtx, vQueue);
			}
		} catch (TxNotFoundException&) {
		}
	}
}

void Wallet::SetNextResendTime(const DateTime& dt) {
	m_dtNextResend = dt + seconds(Ext::Random().Next(30*60));
	TRC(2, "next resend at " << m_dtNextResend.ToLocalTime());
#ifdef X_DEBUG//!!!D
	m_dtNextResend = dt + TimeSpan::FromSeconds(3);
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
			ConstBuf mb = dr.GetBytes(1);
			BestBlockHash = mb.Size ? HashValue(mb) : HashValue();
		}
		EXT_LOCK(m_mtxMyTxHashes) {
			SqliteCommand cmd("SELECT hash FROM mytxes", m_eng->m_cdb.m_dbWallet);
			for (DbDataReader dr=cmd.ExecuteReader(); dr.Read();)
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
				if (CurrentHeight>=0 && CurrentHeight < m_eng->BestBlockHeight() || BestBlockHash==HashValue())
					StartRescan(BestBlockHash);
			}
		}		
	}

	m_eng->Start();
	SetNextResendTime(DateTime::UtcNow());
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
		for (CoinDb::CHash160ToMyKey::iterator it=m_eng->m_cdb.Hash160ToMyKey.begin(), e=m_eng->m_cdb.Hash160ToMyKey.end(); it!=e; ++it)
			if (it->second.Comment != nullptr)
				r.push_back(it->second.ToAddress());
	}
	return r;
}

vector<Address> Wallet::get_Recipients() {
	CCoinEngThreadKeeper engKeeper(m_eng);

	vector<Address> r;
	EXT_LOCK (m_eng->m_cdb.MtxDb) {
		for (DbDataReader dr=m_eng->m_cdb.CmdRecipients.Bind(1, m_dbNetId).ExecuteReader(); dr.Read();)
			r.push_back(Address(*m_eng, HashValue160(dr.GetBytes(0)), dr.GetString(1)));
	}
	return r;
}

void Wallet::AddRecipient(const Address& a) {
	EXT_LOCK (m_eng->m_cdb.MtxDb) {
		SqliteCommand cmdSel(EXT_STR("SELECT netid FROM pubkeys WHERE netid=" << m_dbNetId << " AND hash160=?"), m_eng->m_cdb.m_dbWallet);
		if (cmdSel.Bind(1, HashValue160(a)).ExecuteReader().Read())
			Throw(CoinErr::RecipientAlreadyPresents);

		SqliteCommand cmd(EXT_STR("INSERT INTO pubkeys (netid, hash160, comment) VALUES(" << m_dbNetId << ", ?, ?)"), m_eng->m_cdb.m_dbWallet);
		cmd.Bind(1, HashValue160(a))
			.Bind(2, a.Comment);
		try {
			cmd.ExecuteNonQuery();
		} catch (SqliteException&) {
			Throw(CoinErr::RecipientAlreadyPresents);
		}
	}
}

void Wallet::RemoveRecipient(RCString s) {
	EXT_LOCK (m_eng->m_cdb.MtxDb) {
		SqliteCommand(EXT_STR("DELETE FROM pubkeys WHERE netid=" << m_dbNetId << " AND hash160=?"), m_eng->m_cdb.m_dbWallet)
			.Bind(1, HashValue160(Address(*m_eng, s)))
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
			.Bind(2, hashTx).ExecuteVector();
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
	if (m_eng->m_cdb.SetPrivkeyComment(addr, comment))
		return;

	EXT_LOCK (Mtx) {
		vector<Address> ar = Recipients;
		for (int i=0; i<ar.size(); ++i) {		//!!!TODO  INSERT OR IGNORE; UPDATE
			if (ar[i] == addr) {		
				SqliteCommand cmd(EXT_STR("UPDATE pubkeys Set comment=? WHERE netid=" << m_dbNetId << " AND pubkey=?"), m_eng->m_cdb.m_dbWallet);
				cmd.Bind(1, comment)
					.Bind(2, ConstBuf(ar[i]));
				try {
					cmd.ExecuteNonQuery();
				} catch (RCExc) {
					SqliteCommand(EXT_STR("INSERT INTO pubkeys (netid, pubkey, comment) VALUES (" << m_dbNetId << ", ?, ?)"), m_eng->m_cdb.m_dbWallet)
						.Bind(1, ConstBuf(ar[i]))
						.Bind(2, comment)
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

pair<Blob, HashValue160> Wallet::GetReservedPublicKey() {
	EXT_LOCK (m_eng->m_cdb.MtxDb) {
		MyKeyInfo *ki = m_eng->m_cdb.FindReservedKey();
		Blob pubKey = ki ? ki->PubKey : m_eng->m_cdb.Hash160ToMyKey.begin()->second.PubKey;			
		return make_pair(pubKey, Hash160(pubKey));
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
			coin.OutPoint.TxHash = dr.GetBytes(0);
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

pair<WalletTx, decimal64> Wallet::CreateTransaction(ECDsa *randomDsa, const vector<pair<Blob, int64_t>>& vSend) {
	if (vSend.empty())
		Throw(errc::invalid_argument);
	int64_t nVal = 0;
	for (int i=0; i<vSend.size(); ++i)
		m_eng->CheckMoneyRange(nVal += vSend[i].second);

	EXT_LOCK (Mtx) {
		for (int64_t fee=0; ;) {
			WalletTx tx;
			tx.EnsureCreate();
			tx.m_bFromMe = true;

			int64_t nTotal = nVal + fee;
			for (int i=0; i<vSend.size(); ++i) {
				TxOut txOut(vSend[i].second, vSend[i].first);
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
				Blob scriptChange = Script(vSend[0].first).IsAddress() ? Script::BlobFromAddress(Address(*m_eng, Hash160(tx.ChangePubKey))) : Script::BlobFromPubKey(tx.ChangePubKey);
				tx.TxOuts().insert(tx.TxOuts().begin() + Ext::Random().Next(tx.TxOuts().size()), TxOut(nChange, scriptChange));
			}

			int nIn = 0;
			EXT_FOR (const Penny& penny, ppCoins.first) {
				Sign(randomDsa, penny.PkScript, tx, nIn++);
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
				tx.Timestamp = DateTime::UtcNow();
				return pair<WalletTx, decimal64>(tx, dfee);
			}
			fee = minFee;
		}
	}
	Throw(ExtErr::CodeNotReachable);
}

void Wallet::Relay(const WalletTx& wtx) {
	EXT_FOR (const Tx& tx, wtx.PrevTxes) {
		if (!tx.IsCoinBase())
			m_eng->Relay(tx);
	}
	if (!wtx.IsCoinBase())
		m_eng->Relay(wtx);
}

void Wallet::Commit(WalletTx& wtx) {
	if (wtx.ChangePubKey.Size > 0)
		m_eng->m_cdb.RemovePubKeyFromReserved(wtx.ChangePubKey);
	ProcessMyTx(wtx, true);
	Relay(wtx);
}

void Wallet::ResendWalletTxes() {
	EXT_LOCK (m_eng->MtxPeers) {
		if (m_eng->Links.empty())
			return;
	}

	DateTime now = DateTime::UtcNow();
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

void Wallet::OnPeriodic() {
	if (!RescanThread)
		ResendWalletTxes();
}

void Wallet::OnPeriodicForLink(CoinLink& link) {
}

void Wallet::SendTo(const decimal64& decAmount, RCString saddr, RCString comment) {
	TRC(0, decAmount << " " << m_eng->ChainParams.Symbol << " to " << saddr << " " << comment);

	if (decAmount > Balance)
		Throw(CoinErr::InsufficientAmount);
	int64_t amount = decimal_to_long_long(decAmount * m_eng->ChainParams.CoinValue);
	if (0 == amount)
		Throw(errc::invalid_argument);

	Address addr(*m_eng, saddr);
	vector<pair<Blob, int64_t>> vSend;
	vSend.push_back(make_pair(Script::BlobFromAddress(addr), int64_t(amount)));
	pair<WalletTx, decimal64> pp = CreateTransaction(nullptr, vSend);
	pp.first.Comment = comment;
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
	Address addr(*m_eng, GetReservedPublicKey().second);
	vector<pair<Blob, int64_t>> vSend;
	vSend.push_back(make_pair(Script::BlobFromAddress(addr), decimal_to_long_long(amount*m_eng->ChainParams.CoinValue)));
	ECDsa randomDsa(256);
	return CreateTransaction(&randomDsa, vSend).second;
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
	for (bool b=rd.ReadToDescendant("Chain"); b; b=rd.ReadToNextSibling("Chain"))
		Wallets.push_back(new Wallet(m_cdb, rd.GetAttribute("Name")));
}

WalletEng::~WalletEng() {
	for (int i=0; i<Wallets.size(); ++i) {
		Wallets[i]->m_eng->Stop();
		Wallets[i]->Close();
	}
	m_cdb.Close();
}

} // Coin::

