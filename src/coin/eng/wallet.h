/*######     Copyright (c) 1997-2015 Ufasoft  http://ufasoft.com  mailto:support@ufasoft.com,  Sergey Pavlov  mailto:dev@ufasoft.com #########################################################################################################
#                                                                                                                                                                                                                                            #
# This program is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation;  either version 3, or (at your option) any later version.          #
# This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.   #
# You should have received a copy of the GNU General Public License along with this program; If not, see <http://www.gnu.org/licenses/>                                                                                                      #
############################################################################################################################################################################################################################################*/

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
	int64_t Amount;				// negative for Debits in COM transactions
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
	volatile int32_t m_i;
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
	
	vector<Address> get_MyAddresses();
	DEFPROP_GET(vector<Address>, MyAddresses);

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
	void Sign(ECDsa *randomDsa, const Blob& pkScript, Tx& txTo, uint32_t nIn, byte nHashType = SIGHASH_ALL, const Blob& scriptPrereq = Blob());
	pair<WalletTx, decimal64> CreateTransaction(ECDsa *randomDsa, const vector<pair<Blob, int64_t>>& vSend);															// returns Fee
	void Relay(const WalletTx& wtx);
	int64_t Add(WalletTx& wtx, bool bPending);		
	void Commit(WalletTx& wtx);
	const MyKeyInfo *FindMine(const TxOut& txOut);
	bool IsMine(const Tx& tx);
	bool IsFromMe(const Tx& tx) override;
	void OnPeriodic() override;
	void StartCompactDatabase();
	void ExportWalletToBdb(const path& filepath);
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
	void ProcessPendingTxes();
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

	int64_t GetTxId(const HashValue& hashTx);
	bool InsertUserTx(int64_t txid, int nout, int64_t value, const HashValue160& hash160);
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

	friend class RescanThread;
};

bool VerifyScript(const Blob& scriptSig, const Blob& scriptPk, const Tx& txTo, uint32_t nIn, int32_t nHashType);

class COIN_CLASS WalletEng : public Object {
public:
	CoinDb m_cdb;
	vector<ptr<Wallet>> Wallets;
	
	WalletEng();
	~WalletEng();
};


} // Coin::

