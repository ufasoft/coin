/*######   Copyright (c) 2011-2019 Ufasoft  http://ufasoft.com  mailto:support@ufasoft.com,  Sergey Pavlov  mailto:dev@ufasoft.com ####
#                                                                                                                                     #
# 		See LICENSE for licensing information                                                                                         #
#####################################################################################################################################*/

#include <el/ext.h>

#include "eng.h"


namespace Coin {

CoinConf g_conf;

CoinConf::CoinConf() {
	Ext::Inet::P2P::P2PConf::Instance() = this;

	EXT_CONF_OPTION(Server);
	EXT_CONF_OPTION(AcceptNonStdTxn, true);
	EXT_CONF_OPTION(Checkpoints, true);
	EXT_CONF_OPTION(RpcUser);
	EXT_CONF_OPTION(RpcPassword);
	EXT_CONF_OPTION(RpcPort);
	EXT_CONF_OPTION(RpcThreads, 4);
}

} // Coin::

