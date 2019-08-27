/*######   Copyright (c) 2012-2019 Ufasoft  http://ufasoft.com  mailto:support@ufasoft.com,  Sergey Pavlov  mailto:dev@ufasoft.com ####
#                                                                                                                                     #
# 		See LICENSE for licensing information                                                                                         #
#####################################################################################################################################*/

#pragma once

#include EXT_HEADER_DECIMAL
using namespace std::decimal;

#include <el/inet/http.h>
using namespace Ext::Inet;

#include "util.h"
#include "block-template.h"

#ifndef LOG		//!!!?
#	define LOG(s) TRC(2, s)
#endif

namespace Coin {

class INewBlockNotify {
public:
	virtual void OnNewBlock(const HashValue& hashBlock) =0;

};

struct WalletTxInfo {
	HashValue HashTx, HashBlock;
	String Address;
	decimal64 Amount;
	DateTime Timestamp;
	int Confirmations;
	CBool Generated;

	struct TxDetails {
		String Account;
		String Address;
		decimal64 Amount;
		CBool IsSend;
	};
	vector<TxDetails> Details;
};

struct SinceBlockInfo {
	vector<WalletTxInfo> Txes;
	HashValue LastBlock;
};

struct BlockInfo {
	HashValue Hash, PrevBlockHash, MerkleRoot;
	vector<HashValue> HashTxes;
	DateTime Timestamp;
	double Difficulty;
	int Version;
	int Height;
	int Confirmations;
};

class IWalletClient : public InterlockedObject {
public:
	String Name;
	path PathDaemon;
	path DataDir;
	String Args;
	observer_ptr<INewBlockNotify> OnNewBlockNotify;

	Uri RpcUrl;
	String Login, Password;

	IPEndPoint EpApi;

	mutex MtxHeaders;
	WebHeaderCollection Headers;

	bool Listen;
	bool IsTestNet;
	bool EnableNotifications;
	bool WalletNotifications;

	IWalletClient()
		: Listen(true)
		, IsTestNet(false)
		, EnableNotifications(true)
		, WalletNotifications(false)
	{}

	virtual void Start() =0;
	virtual void Stop() =0;
	virtual void StopOrKill() { Stop(); }
	virtual int GetBestHeight()										{ Throw(E_NOTIMPL); }
	virtual HashValue GetBlockHash(uint32_t height)					{ Throw(E_NOTIMPL); }
	virtual BlockInfo GetBlock(const HashValue& hashBlock)			{ Throw(E_NOTIMPL); }
	virtual double GetDifficulty() 									{ Throw(E_NOTIMPL); }
	virtual WalletTxInfo GetTransaction(const HashValue& hashTx)			{ Throw(E_NOTIMPL); }
	virtual SinceBlockInfo ListSinceBlock(const HashValue& hashBlock = HashValue::Null())							{ Throw(E_NOTIMPL); }
//!!!R	virtual ptr<PoolWorkData> GetWork() 													{ Throw(E_NOTIMPL); }
	virtual String GetAccountAddress(RCString account = "")										{ Throw(E_NOTIMPL); } // Deprecated
	virtual String GetNewAddress(RCString account = nullptr) { Throw(E_NOTIMPL); }
	virtual HashValue SendToAddress(RCString address, decimal64 amount, RCString comment = "") 	{ Throw(E_NOTIMPL); }
	virtual ptr<MinerBlock> GetBlockTemplate(const vector<String>& capabilities = vector<String>()) { Throw(E_NOTIMPL); }
	virtual void GetWork(RCSpan data) 						{ Throw(E_NOTIMPL); }
	virtual void SubmitBlock(RCSpan data, RCString workid) 						{ Throw(E_NOTIMPL); }
};

ptr<IWalletClient> CreateRpcWalletClient(RCString name, const path& pathDaemon, WebClient *pwc = 0);
ptr<IWalletClient> CreateInprocWalletClient(RCString name);

enum class RpcCoinErr {
    MISC_ERROR                  = -1  // std::exception thrown in command handling
    , FORBIDDEN_BY_SAFE_MODE      = -2  // Server is in safe mode, and command is not allowed in safe mode
	, TYPE_ERROR                  = -3  // Unexpected type was passed as parameter
	, INVALID_ADDRESS_OR_KEY      = -5  // Invalid address or key
	, OUT_OF_MEMORY               = -7  // Ran out of memory during operation
	, INVALID_PARAMETER           = -8  // Invalid, missing or duplicate parameter
	, DATABASE_ERROR              = -20 // Database error
	, DESERIALIZATION_ERROR       = -22 // Error parsing or validating structure in raw format

    // P2P client errors
	, CLIENT_NOT_CONNECTED        = -9  // Bitcoin is not connected
	, CLIENT_IN_INITIAL_DOWNLOAD  = -10 // Still downloading initial blocks

    // Wallet errors
	, WALLET_ERROR                = -4  // Unspecified problem with wallet (key not found etc.)
	, WALLET_INSUFFICIENT_FUNDS   = -6  // Not enough funds in wallet or account
	, WALLET_INVALID_ACCOUNT_NAME = -11 // Invalid account name
	, WALLET_KEYPOOL_RAN_OUT      = -12 // Keypool ran out, call keypoolrefill first
	, WALLET_UNLOCK_NEEDED        = -13 // Enter the wallet passphrase with walletpassphrase first
	, WALLET_PASSPHRASE_INCORRECT = -14 // The wallet passphrase entered was incorrect
	, WALLET_WRONG_ENC_STATE      = -15 // Command given in wrong wallet encryption state (encrypting an encrypted wallet etc.)
	, WALLET_ENCRYPTION_FAILED    = -16 // Failed to encrypt the wallet
	, WALLET_ALREADY_UNLOCKED     = -17 // Wallet is already unlocked
};


} // Coin::

namespace Ext {
	inline HRESULT HResult(Coin::RpcCoinErr err) { return MAKE_HRESULT(SEVERITY_ERROR, FACILITY_JSON_RPC, uint16_t(err)); }
} // Ext::

