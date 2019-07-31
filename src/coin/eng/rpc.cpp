/*######   Copyright (c) 2013-2019 Ufasoft  http://ufasoft.com  mailto:support@ufasoft.com,  Sergey Pavlov  mailto:dev@ufasoft.com ####
#                                                                                                                                     #
# 		See LICENSE for licensing information                                                                                         #
#####################################################################################################################################*/

#include <el/ext.h>


#include "wallet.h"
#include "coin-rpc.h"

namespace Coin {

Rpc::CMap Rpc::s_map;

Rpc::Rpc(Coin::WalletEng& walletEng)
	: WalletEng(walletEng)
	, Wallet(0)
{
	Eng = &WalletEng.Wallets.at(0)->Eng;
}

void Rpc::Register(RCString name, Rpc::MF0 mf) {
	MemFun memfun;
	memfun.MF0 = mf;
	memfun.Sig = FunSig::Void;
	s_map.insert(make_pair(name.ToLower(), memfun));
}

void Rpc::Register(RCString name, Rpc::MF1 mf) {
	MemFun memfun;
	memfun.MF1 = mf;
	memfun.Sig = FunSig::Arg1;
	s_map.insert(make_pair(name.ToLower(), memfun));
}

#define COIN_RPC_REGISTER(fun) Rpc::Register(#fun, &Rpc::fun)

bool Rpc::RegisterRpcHandlers() {
	COIN_RPC_REGISTER(GetBlockchainInfo);
	COIN_RPC_REGISTER(GetBlockHash);
	return true;
}

static bool s_bRegisteredRpcHandlers = Rpc::RegisterRpcHandlers();


VarValue Rpc::GetBlockchainInfo() {
	VarValue r;
	r.Set("chain", Eng->ChainParams.Name);
	r.Set("blocks", Eng->BestBlockHeight());
	r.Set("bestblockhash", Hash(Eng->BestBlock()).ToString());
	r.Set("initialblockdownload", Eng->IsInitialBlockDownload());
	return r;
}

VarValue Rpc::GetBlockHash(const VarValue& varHeight) {
	auto height = varHeight.ToInt64();
	if (height < 0 || height > Eng->BestHeaderHeight())
		Throw(CoinErr::RPC_INVALID_PARAMETER);
	return Hash(Eng->GetBlockByHeight((uint32_t)height)).ToString();
}

VarValue Rpc::CallMethod(RCString name, const VarValue& params) {
	MemFun memFun = s_map.at(name);
	switch (memFun.Sig) {
	case FunSig::Void:
		return (this->*memFun.MF0)();
		break;
	case FunSig::Arg1:
		return (this->*memFun.MF1)(params[0]);
		break;
	case FunSig::Arg2:
		return (this->*memFun.MF2)(params[0], params[1]);
		break;
	case FunSig::Many:
		return (this->*memFun.MFN)(params);
		break;
	default:
		Throw(E_FAIL);
	}
}

void Rpc::GetBlockCount(JsonTextWriter& wr) {
	wr.Write(Eng->BestBlockHeight());
}

void Rpc::ListAccounts(JsonTextWriter& wr) {
	map<String, double> m;
	EXT_LOCK (Eng->m_cdb.MtxDb) {
		EXT_FOR (const CoinDb::CHash160ToKey::value_type& kv, Eng->m_cdb.Hash160ToKey) {
			KeyInfo ki = kv.second;
			SqliteCommand cmd("SELECT SUM(value) FROM coins INNER JOIN mytxes ON txid=mytxes.id WHERE mytxes.netid=? AND (blockord > -2 OR fromme) AND keyid=?", Eng->m_cdb.m_dbWallet);
			m[ki->Comment] += double(cmd.Bind(1, Wallet->m_dbNetId).Bind(2, ki->KeyRowId).ExecuteInt64Scalar()) / -(Eng->ChainParams.CoinValue);
		}
	}
	wr.Write(m);
}



} // Coin::

