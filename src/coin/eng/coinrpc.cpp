#include <el/ext.h>


#include "wallet.h"
#include "coinrpc.h"

namespace Coin {

void Rpc::GetBlockCount(JsonTextWriter& wr) {
	wr.Write(Wallet.m_eng->BestBlockHeight());
}

void Rpc::ListAccounts(JsonTextWriter& wr) {
	map<String, double> m;
	EXT_LOCK (Wallet.m_eng->m_cdb.MtxDb) {
		EXT_FOR (const CoinDb::CHash160ToMyKey::value_type& kv, Wallet.m_eng->m_cdb.Hash160ToMyKey) {
			const MyKeyInfo& ki = kv.second;
			SqliteCommand cmd("SELECT SUM(value) FROM coins INNER JOIN mytxes ON txid=mytxes.id WHERE mytxes.netid=? AND (blockord > -2 OR fromme) AND keyid=?", Wallet.m_eng->m_cdb.m_dbWallet);
			m[ki.Comment] += double(cmd.Bind(1, Wallet.m_dbNetId).Bind(2, ki.KeyRowid).ExecuteInt64Scalar()) / -Wallet.m_eng->ChainParams.CoinValue;
		}
	}
	wr.Write(m);
}



} // Coin::

