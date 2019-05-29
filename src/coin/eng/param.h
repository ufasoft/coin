/*######   Copyright (c) 2015-2019 Ufasoft  http://ufasoft.com  mailto:support@ufasoft.com,  Sergey Pavlov  mailto:dev@ufasoft.com ####
#                                                                                                                                     #
# 		See LICENSE for licensing information                                                                                         #
#####################################################################################################################################*/

#pragma once

#include "consensus.h"

namespace Coin {


const uint64_t SEND_FEE_THOUSANDTH = 4;		// 0.4%


const size_t MAX_BLOOM_FILTER_SIZE = 36000; // bytes
const int MAX_HASH_FUNCS = 50;


const int MAX_SEND_SIZE = 1000000;

const int
	MAX_BLOCKS_IN_TRANSIT_PER_PEER = 32,				// 16 in the reference client
	BLOCK_DOWNLOAD_WINDOW = 1024,						// at least 1024 is recommended for some non-sorted bootstrap.dat files
	MAX_HEADERS_RESULTS	= 2000;


//!!!static const int64_t MIN_TX_FEE = 50000;
//!!!static const int64_t MIN_RELAY_TX_FEE = 10000;
static const int KEYPOOL_SIZE = 100;

const int INITIAL_BLOCK_THRESHOLD = 120;

const int SECONDS_RESEND_PERIODICITY = 15 * 60;


} // Coin::

#define COIN_BACKEND_DBLITE 'D'
#define COIN_BACKEND_SQLITE 'S'

#ifndef UCFG_COIN_COINCHAIN_BACKEND
#	define UCFG_COIN_COINCHAIN_BACKEND COIN_BACKEND_DBLITE
#endif

#ifndef UCFG_COIN_CONVERT_TO_UDB
#	define UCFG_COIN_CONVERT_TO_UDB 0
#endif

#ifndef UCFG_COIN_USE_FUTURES
#	define UCFG_COIN_USE_FUTURES 1
#endif

#ifndef UCFG_COIN_TX_CONNECT_FUTURES
#	define UCFG_COIN_TX_CONNECT_FUTURES UCFG_COIN_USE_FUTURES
#endif

#ifndef UCFG_COIN_MERKLE_FUTURES
#	define UCFG_COIN_MERKLE_FUTURES UCFG_COIN_USE_FUTURES
#endif

#ifndef UCFG_COIN_PKSCRIPT_FUTURES
#	define UCFG_COIN_PKSCRIPT_FUTURES UCFG_COIN_USE_FUTURES
#endif

#ifndef UCFG_COIN_COMPACT_AUX
#	define UCFG_COIN_COMPACT_AUX 0					// incompatible space optimization
#endif

#ifndef UCFG_COIN_USE_IRC
#	define UCFG_COIN_USE_IRC 0
#endif

#define UCFG_COIN_TXES_IN_BLOCKTABLE 1

#ifndef UCFG_COIN_CONF_FILENAME
#	define UCFG_COIN_CONF_FILENAME "coin.conf"
#endif

