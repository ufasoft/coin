#pragma once

#include <el/comp/json-writer.h>
#include <el/inet/http-server.h>
#include <el/inet/json-rpc.h>
using namespace Ext::Inet;

#include "wallet.h"

namespace Coin {

class WalletEng;


class Rpc : public HttpServer, JsonRpc {
public:
	typedef VarValue(Rpc::*MF0)();
	typedef VarValue(Rpc::*MF1)(const VarValue&);
	typedef VarValue(Rpc::*MF2)(const VarValue&, const VarValue&);
	typedef VarValue(Rpc::*MFN)(const VarValue& params);

	enum class FunSig {
		Void,
		Arg1,
		Arg2,
		Many
	};

	struct MemFun {
		union {
			MF0 MF0;
			MF1 MF1;
			MF2 MF2;
			MFN MFN;
		};
		FunSig Sig;

	};

	typedef unordered_map<String, MemFun> CMap;
	static unordered_map<String, MemFun> s_map;

	Coin::WalletEng& WalletEng;
	CoinEng *Eng;
	class Wallet *Wallet;

	Rpc(Coin::WalletEng& walletEng);
	static void Register(RCString name, MF0 mf);
	static void Register(RCString name, MF1 mf);
	static bool RegisterRpcHandlers();

	void GetBlockCount(JsonTextWriter& wr);
	void ListAccounts(JsonTextWriter& wr);
protected:
	VarValue CallMethod(RCString name, const VarValue& params) override;

	VarValue GetBlockchainInfo();
	VarValue GetBlockHash(const VarValue& varHeight);

};


} // Coin::




