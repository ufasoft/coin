/*######     Copyright (c) 1997-2013 Ufasoft  http://ufasoft.com  mailto:support@ufasoft.com,  Sergey Pavlov  mailto:dev@ufasoft.com #######################################
#                                                                                                                                                                          #
# This program is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation;  #
# either version 3, or (at your option) any later version. This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the      #
# implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details. You should have received a copy of the GNU #
# General Public License along with this program; If not, see <http://www.gnu.org/licenses/>                                                                               #
##########################################################################################################################################################################*/

#pragma once

#include EXT_HEADER_DECIMAL
using namespace std::decimal;

#include "util.h"
#include "block-template.h"

namespace Coin {

class INewBlockNotify {
public:
	virtual void OnNewBlock(const HashValue& hashBlock) =0;

};

struct TxInfo {
	String Address;
	HashValue HashTx, HashBlock;
	decimal64 Amount;
	DateTime Timestamp;
	int Confirmations;
	CBool Generated;

	struct TxDetails {
		String Account;
		String Address;
		CBool IsSend;
		decimal64 Amount;
	};
	vector<TxDetails> Details;
};

struct SinceBlockInfo {
	vector<TxInfo> Txes;
	HashValue LastBlock;
};

struct BlockInfo {
	HashValue Hash, PrevBlockHash, MerkleRoot;
	DateTime Timestamp;
	int Version;
	int Height;
	int Confirmations;
	double Difficulty;
	vector<HashValue> HashTxes;
};

class IWalletClient : public Object {
public:
	String Name;
	String DataDir;
	CPointer<INewBlockNotify> OnNewBlockNotify;
	IPEndPoint EpRpc,
			EpApi;
	bool IsTestNet;
	bool Listen;
	bool WalletNotifications;

	IWalletClient()
		:	Listen(true)
		,	IsTestNet(false)
		,	WalletNotifications(false)
	{}

	virtual void Start() =0;
	virtual void Stop() =0;
	virtual void StopOrKill() { Stop(); }
	virtual int GetBestHeight()										{ Throw(E_NOTIMPL); }
	virtual HashValue GetBlockHash(UInt32 height)					{ Throw(E_NOTIMPL); }
	virtual BlockInfo GetBlock(const HashValue& hashBlock)			{ Throw(E_NOTIMPL); }
	virtual double GetDifficulty() 									{ Throw(E_NOTIMPL); }
	virtual TxInfo GetTransaction(const HashValue& hashTx)			{ Throw(E_NOTIMPL); }
	virtual SinceBlockInfo ListSinceBlock(const HashValue& hashBlock = HashValue())							{ Throw(E_NOTIMPL); }
//!!!R	virtual ptr<PoolWorkData> GetWork() 													{ Throw(E_NOTIMPL); }
	virtual String GetAccountAddress(RCString account = "")										{ Throw(E_NOTIMPL); }
	virtual HashValue SendToAddress(RCString address, decimal64 amount, RCString comment = "") 	{ Throw(E_NOTIMPL); }
	virtual ptr<MinerBlock> GetBlockTemplate(const vector<String>& capabilities = vector<String>()) { Throw(E_NOTIMPL); }
	virtual void SubmitBlock(const ConstBuf& data) 						{ Throw(E_NOTIMPL); }
	virtual String GetNewAddress(RCString account = nullptr)			{ Throw(E_NOTIMPL); }
};

ptr<IWalletClient> CreateRpcWalletClient(RCString name, RCString pathDaemon);
ptr<IWalletClient> CreateInprocWalletClient(RCString name);

enum class RpcCoinErr {
    MISC_ERROR                  = -1,  // std::exception thrown in command handling
    FORBIDDEN_BY_SAFE_MODE      = -2,  // Server is in safe mode, and command is not allowed in safe mode
    TYPE_ERROR                  = -3,  // Unexpected type was passed as parameter
    INVALID_ADDRESS_OR_KEY      = -5,  // Invalid address or key
    OUT_OF_MEMORY               = -7,  // Ran out of memory during operation
    INVALID_PARAMETER           = -8,  // Invalid, missing or duplicate parameter
    DATABASE_ERROR              = -20, // Database error
    DESERIALIZATION_ERROR       = -22, // Error parsing or validating structure in raw format

    // P2P client errors
    CLIENT_NOT_CONNECTED        = -9,  // Bitcoin is not connected
    CLIENT_IN_INITIAL_DOWNLOAD  = -10, // Still downloading initial blocks

    // Wallet errors
    WALLET_ERROR                = -4,  // Unspecified problem with wallet (key not found etc.)
    WALLET_INSUFFICIENT_FUNDS   = -6,  // Not enough funds in wallet or account
    WALLET_INVALID_ACCOUNT_NAME = -11, // Invalid account name
    WALLET_KEYPOOL_RAN_OUT      = -12, // Keypool ran out, call keypoolrefill first
    WALLET_UNLOCK_NEEDED        = -13, // Enter the wallet passphrase with walletpassphrase first
    WALLET_PASSPHRASE_INCORRECT = -14, // The wallet passphrase entered was incorrect
    WALLET_WRONG_ENC_STATE      = -15, // Command given in wrong wallet encryption state (encrypting an encrypted wallet etc.)
    WALLET_ENCRYPTION_FAILED    = -16, // Failed to encrypt the wallet
    WALLET_ALREADY_UNLOCKED     = -17, // Wallet is already unlocked
};


} // Coin::

namespace Ext {
	inline HRESULT HResult(Coin::RpcCoinErr err) { return MAKE_HRESULT(SEVERITY_ERROR, FACILITY_JSON_RPC, UInt16(err)); }
} // Ext::

