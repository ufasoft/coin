#pragma once

#include <el/comp/json-writer.h>

#include "wallet.h"

namespace Coin {

class Rpc {
public:
	Wallet& Wallet;

	Rpc(Coin::Wallet& wallet)
		:	Wallet(wallet)
	{}

	void GetBlockCount(JsonTextWriter& wr);
	void ListAccounts(JsonTextWriter& wr);
};


} // Coin::




