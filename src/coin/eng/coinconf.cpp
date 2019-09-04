/*######   Copyright (c) 2011-2019 Ufasoft  http://ufasoft.com  mailto:support@ufasoft.com,  Sergey Pavlov  mailto:dev@ufasoft.com ####
#                                                                                                                                     #
# 		See LICENSE for licensing information                                                                                         #
#####################################################################################################################################*/

#include <el/ext.h>

#include "eng.h"

#include "param.h"


namespace Coin {

CoinConf g_conf;

CoinConf::CoinConf() {
	Ext::Inet::P2P::P2PConf::Instance() = this;

	EXT_CONF_OPTION(AcceptNonStdTxn, true);
	EXT_CONF_OPTION(Checkpoints, true);
	EXT_CONF_OPTION(MinTxFee, DEFAULT_TRANSACTION_MINFEE);
	EXT_CONF_OPTION(MinRelayTxFee, DEFAULT_MIN_RELAY_TX_FEE);
	EXT_CONF_OPTION(AddressType, "bech32", "legacy, p2sh-segwit or bech32");
	EXT_CONF_OPTION(ChangeType, "", "legacy, p2sh-segwit or bech32");
	EXT_CONF_OPTION(RpcUser);
	EXT_CONF_OPTION(RpcPassword);
	EXT_CONF_OPTION(RpcPort);
	EXT_CONF_OPTION(RpcThreads, 4);
	EXT_CONF_OPTION(Server);
	EXT_CONF_OPTION(KeyPool, DEFAULT_KEYPOOL_SIZE);
	EXT_CONF_OPTION(Testnet);
}

AddressType CoinConf::ToAddressType(RCString s) {
	if (s == "legacy")
		return Coin::AddressType::P2PKH;
	if (s == "bech32")
		return Coin::AddressType::Bech32;		//!!! ambiguity: P2WKH or P2WSH
	if (s == "p2sh-segwit")
		return Coin::AddressType::P2WPKH_IN_P2SH;
	Throw(ExtErr::InvalidOption);
}

} // Coin::
