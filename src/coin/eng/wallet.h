/*######   Copyright (c) 2012-2019 Ufasoft  http://ufasoft.com  mailto:support@ufasoft.com,  Sergey Pavlov  mailto:dev@ufasoft.com ####
#                                                                                                                                     #
# 		See LICENSE for licensing information                                                                                         #
#####################################################################################################################################*/

#pragma once

#include "coin-model.h"
#include "eng.h"
#include "script.h"
#include "coin-rpc.h"

namespace Coin {

class Script;
class Wallet;

class WalletTx : public Tx {
	typedef WalletTx class_type;
	typedef Tx base;
public:
	typedef InterlockedPolicy interlocked_policy;

	DateTime Timestamp;
	Address To;
	CanonicalPubKey ChangePubKey;
	int64_t Amount;				// negative for Debits in COM transactions
	vector<Tx> PrevTxes;
	String Comment;
	CBool m_bFromMe;

	WalletTx()
		:	To(Eng())
//		:	Confirmations(0)
	{}

	explicit WalletTx(const Tx& tx);

	void LoadFromDb(DbDataReader& sr, bool bLoadExt = false);
	bool IsConfirmed(Wallet& wallet) const;
	String GetComment() const;

	String UniqueKey() const {		//!!!T
		return Convert::ToString(Timestamp.Ticks)+To.ToString();
	}

	decimal64 GetDecimalAmount() const { return make_decimal64((long long)Amount, -Eng().ChainParams.Log10CoinValue()); }
};

DbWriter& operator<<(DbWriter& wr, const WalletTx& wtx);
const DbReader& operator>>(const DbReader& rd, WalletTx& wtx);


class Penny {
	typedef Penny class_type;
public:
	Coin::OutPoint OutPoint;
	uint64_t Value;
	Blob PkScript;
	CBool IsSpent;

	Penny()
		:	Value(0)
	{}

	bool operator==(const Penny& v) const {
		return OutPoint == v.OutPoint && Value == v.Value;
	}

	int64_t get_Debit() const;
	DEFPROP_GET(int64_t, Debit);

	bool get_IsFromMe() const { return Debit > 0; }
	DEFPROP_GET(bool, IsFromMe);

	size_t GetHashCode() const {
		return hash<Coin::OutPoint>()(OutPoint);
	}
};

EXT_DEF_HASH_NS(Coin, Penny);


/*!!!
} template<> struct hash<Coin::Penny> {
	size_t operator()(const Coin::Penny& p) const {
		return hash<Coin::OutPoint>()(p.OutPoint);
	}
}; namespace Coin {
*/

class Wallet;
extern EXT_THREAD_PTR(Wallet) t_pWallet;

class RescanThread : public Thread {
	typedef Thread base;
public:
	Coin::Wallet& Wallet;
	int64_t m_i, m_count;
	HashValue m_hashFrom;

	RescanThread(Coin::Wallet& wallet, const HashValue& hashFrom);
protected:
	void Execute() override;
};

class CompactThread : public Thread {
	typedef Thread base;
public:
	Coin::Wallet& Wallet;
	atomic<int> m_ai;
	uint32_t m_count;
	HashValue m_hashFrom;

	CompactThread(Coin::Wallet& wallet);
protected:
	void Execute() override;
};

class COIN_CLASS Wallet : public WalletBase {
	typedef Wallet class_type;
	typedef WalletBase base;
public:
	recursive_mutex Mtx;
	HashValue BestBlockHash;

	typedef unordered_map<OutPoint, TxOut> COutPointToTxOut;
	COutPointToTxOut OutPointToTxOut;
	ptr<Coin::RescanThread> RescanThread;
	ptr<Coin::CompactThread> CompactThread;
	vector<ptr<Alert>> Alerts;
	ptr<CoinEng> m_peng;

	mutex MtxCurrentHeight;
	int CurrentHeight;

	int32_t m_dbNetId;
	float Progress;
	CBool m_bLoaded;

	Wallet(CoinEng& eng);
	Wallet(CoinDb& cdb, RCString name);

	static Wallet& Current() { return *t_pWallet; }

	void Start();
	void Stop();

	unordered_set<Address> get_MyAddresses();
	DEFPROP_GET(unordered_set<Address>, MyAddresses);

	vector<Address> get_Recipients();
	DEFPROP_GET(vector<Address>, Recipients);

	void AddRecipient(const Address& a) override;
	void RemoveRecipient(RCString s);
	pair<int64_t, int> GetBalanceValueExp();

	decimal64 get_Balance();
	DEFPROP_GET(decimal64, Balance);

	decimal64 GetDecimalFee(const WalletTx& wtx);

	CBool m_bMiningEnabled;

	bool get_MiningEnabled() { return m_bMiningEnabled; }

	void put_MiningEnabled(bool b);
	DEFPROP(bool, MiningEnabled);

	void SetAddressComment(const Address& addr, RCString comment);
	decimal64 CalcFee(const decimal64& amount);
	void SendTo(const decimal64& decAmount, RCString saddr, RCString comment);
	void Rescan();
	void Close();
//!!!	CngKey GetKey(const HashValue160& keyHash);
	pair<WalletTx, decimal64> CreateTransaction(const KeyInfo& randomKey, const vector<pair<Address, int64_t>>& vSend);															// returns Fee
	void Relay(const WalletTx& wtx);
	int64_t Add(WalletTx& wtx, bool bPending);
	void Commit(WalletTx& wtx);

	bool IsToMe(const Tx& tx);
	bool IsFromMe(const Tx& tx) override;
	void OnPeriodic(const DateTime& now) override;
	void StartCompactDatabase();
	void ExportWalletToBdb(const path& filepath);
	void CancelPendingTxes();

	void OnChange() override {
		if (m_iiWalletEvents)
			m_iiWalletEvents->OnStateChanged();
	}
protected:
	pair<CanonicalPubKey, HashValue160> GetReservedPublicKey() override;

	void OnProcessBlock(const Block& block) override;
	void OnProcessTx(const Tx& tx) override;
	void OnEraseTx(const HashValue& hashTx) override;
	void ProcessPendingTxes();
	void OnBlockchainChanged() override;
	void SetNextResendTime(const DateTime& dt);
	void ResendWalletTxes(const DateTime& now);
	void OnPeriodicForLink(Link& link) override;

private:
	String DbFilePath;
	DateTime m_dtNextResend;
	DateTime m_dtLastResend;

	mutex m_mtxMyTxHashes;
	unordered_set<HashValue> m_myTxHashes;

	void Init();
	void StartRescan(const HashValue& hashFrom = HashValue::Null());
	void TryRescanStep();

	int64_t GetTxId(const HashValue& hashTx);
	bool InsertUserTx(int64_t txid, int nout, int64_t value);
	void ProcessMyTx(WalletTx& wtx, bool bPending);

//!!!R	void SaveBlockords();
	WalletTx GetTx(const HashValue& hashTx);
	bool SelectCoins(uint64_t amount, uint64_t fee, int nConfMine, int nConfTheirs, pair<unordered_set<Penny>, uint64_t>& pp);
	pair<unordered_set<Penny>, uint64_t> SelectCoins(uint64_t amount, uint64_t fee);			// <returns Pennies, RequiredFee>
	bool ProcessTx(const Tx& tx);
	void SetBestBlockHash(const HashValue& hash);
	void ReacceptWalletTxes();

	void OnSetProgress(float v) override {
		Progress = v;
	}

	void OnAlert(Alert *alert) override;
	void CheckCreatedTransaction(const Tx& tx);

	friend class RescanThread;
};

class WalletSigner : public Signer {
	typedef Signer base;
public:
	Wallet& m_wallet;

	WalletSigner(Wallet& wallet, Tx& tx)
		: base(tx)
		, m_wallet(wallet)
	{}
protected:
	KeyInfo GetMyKeyInfo(const HashValue160& hash160) override;
	KeyInfo GetMyKeyInfoByScriptHash(const HashValue160& hash160) override;
	Blob GetMyRedeemScript(const HashValue160& hash160) override;
};


class COIN_CLASS WalletEng : public Object {
public:
	CoinDb m_cdb;
	vector<ptr<Wallet>> Wallets;
	ptr<Coin::Rpc> Rpc;

	WalletEng();
	~WalletEng();
};


} // Coin::

