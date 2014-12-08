#pragma once

#include "coin-model.h"
#include "eng.h"
#include "script.h"

namespace Coin {

class Script;
class Wallet;

class WalletTx : public Tx {
	typedef WalletTx class_type;
	typedef Tx base;
public:
	typedef Interlocked interlocked_policy;

	DateTime Timestamp;
	Address To;
	Blob ChangePubKey;
	Int64 Amount;				// negative for Debits in COM transactions
//!!!R	int Confirmations;
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

	decimal64 GetDecimalAmount() const { return make_decimal64(Amount, -Eng().ChainParams.Log10CoinValue()); }
};

DbWriter& operator<<(DbWriter& wr, const WalletTx& wtx);
const DbReader& operator>>(const DbReader& rd, WalletTx& wtx);


class Penny {
	typedef Penny class_type;
public:
	Coin::OutPoint OutPoint;
	UInt64 Value;
	Blob PkScript;
	CBool IsSpent;

	Penny()
		:	Value(0)
	{}

	bool operator==(const Penny& v) const {
		return OutPoint == v.OutPoint && Value == v.Value;
	}

	Int64 get_Debit() const;
	DEFPROP_GET(Int64, Debit);

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
	Int64 m_i, m_count;	
	HashValue m_hashFrom;

	RescanThread(Coin::Wallet& wallet, const HashValue& hashFrom);
protected:
	void Execute() override;
};

class CompactThread : public Thread {
	typedef Thread base;
public:
	Coin::Wallet& Wallet;
	volatile Int32 m_i;
	UInt32 m_count;	
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

	Int32 m_dbNetId;
	float Progress;
	CBool m_bLoaded;

	Wallet(CoinEng& eng);
	Wallet(CoinDb& cdb, RCString name);

	static Wallet& Current() { return *t_pWallet; }

	void Start();
	void Stop();
	
	vector<Address> get_MyAddresses();
	DEFPROP_GET(vector<Address>, MyAddresses);

	vector<Address> get_Recipients();
	DEFPROP_GET(vector<Address>, Recipients);

	void AddRecipient(const Address& a) override;
	void RemoveRecipient(RCString s);
	pair<Int64, int> GetBalanceValueExp();

	decimal64 get_Balance();
	DEFPROP_GET(decimal64, Balance);

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
	void Sign(ECDsa *randomDsa, const Blob& pkScript, Tx& txTo, UInt32 nIn, byte nHashType = SIGHASH_ALL, const Blob& scriptPrereq = Blob());
	pair<WalletTx, decimal64> CreateTransaction(ECDsa *randomDsa, const vector<pair<Blob, Int64>>& vSend);															// returns Fee
	void Relay(const WalletTx& wtx);
	Int64 Add(WalletTx& wtx, bool bPending);		
	void Commit(WalletTx& wtx);
	const MyKeyInfo *FindMine(const TxOut& txOut);
	bool IsMine(const Tx& tx);
	bool IsFromMe(const Tx& tx) override;
	void OnPeriodic() override;
	void StartCompactDatabase();
	void ExportWalletToBdb(RCString filepath);
	void CancelPendingTxes();

	void OnChange() override {
		if (m_iiWalletEvents)
			m_iiWalletEvents->OnStateChanged();
	}
protected:
	pair<Blob, HashValue160> GetReservedPublicKey() override;
	
	void OnProcessBlock(const Block& block) override;	
	void OnProcessTx(const Tx& tx) override;
	void OnEraseTx(const HashValue& hashTx) override;
	void OnBlockchainChanged() override;
	void SetNextResendTime(const DateTime& dt);
	void ResendWalletTxes();
	void OnPeriodicForLink(CoinLink& link) override;
	
private:
	String DbFilePath;
	DateTime m_dtNextResend;
	DateTime m_dtLastResend;
	
	void Init();
	void StartRescan(const HashValue& hashFrom = HashValue());
	void TryRescanStep();

	Int64 GetTxId(const HashValue& hashTx);
	bool InsertUserTx(Int64 txid, int nout, Int64 value, const HashValue160& hash160);
	void ProcessMyTx(WalletTx& wtx, bool bPending);

//!!!R	void SaveBlockords();
	WalletTx GetTx(const HashValue& hashTx);
	bool SelectCoins(UInt64 amount, UInt64 fee, int nConfMine, int nConfTheirs, pair<unordered_set<Penny>, UInt64>& pp);
	pair<unordered_set<Penny>, UInt64> SelectCoins(UInt64 amount, UInt64 fee);			// <returns Pennies, RequiredFee>
	bool ProcessTx(const Tx& tx);
	void SetBestBlockHash(const HashValue& hash);
	void ReacceptWalletTxes();

	void OnSetProgress(float v) override {
		Progress = v;
	}


	void OnAlert(Alert *alert) override;

	friend class RescanThread;
};

bool VerifyScript(const Blob& scriptSig, const Blob& scriptPk, const Tx& txTo, UInt32 nIn, Int32 nHashType);

class COIN_CLASS WalletEng : public Object {
public:
	CoinDb m_cdb;
	vector<ptr<Wallet>> Wallets;
	
	WalletEng();
	~WalletEng();
};


} // Coin::

