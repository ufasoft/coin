/*######   Copyright (c) 2019      Ufasoft  http://ufasoft.com  mailto:support@ufasoft.com,  Sergey Pavlov  mailto:dev@ufasoft.com ####
#                                                                                                                                     #
# 		See LICENSE for licensing information                                                                                         #
#####################################################################################################################################*/

#include <el/ext.h>

#include "eng.h"

namespace Coin {

static unordered_map<String, int> s_mapByUserAgent;
static unordered_map<HashValue, int> s_mapByGenesis;

struct SByGenesis {
	const char* P;
	int Sym;
};

static const SByGenesis s_genesises[] = {
	  { "fdbe99b90c90bae7505796461471d89ae8388ab953997aa06a355bbda8d915cb", 'FTC'	}	// Feathercoin
	, { "cc22a327027dda63764cb25db7ab2349ad67ecb5a6bae0e1b629c9defab3bb14", 'FUNC'	}	// Cypherfunks
	, { "12e05e8d4a8258aed79e113ee429c5194ee1c8999f50df176ecea97380d474ab", 'RPC'	}	// RonPaulCoin
};


static struct InitMap {
	InitMap() {
		s_mapByUserAgent["ByteCoin"]		= 'BCN';
		s_mapByUserAgent["FlashcoinCore"]	= 'FLAS';	//!!!
		s_mapByUserAgent["jinCoinCore"]		= 'JIN';
		s_mapByUserAgent["LitecoinCore"]	= 'LTC';
		s_mapByUserAgent["SmartCoinCore"]	= 'SMC';	// SmartCoin
		s_mapByUserAgent["YouHubCoinCore"]	= 'YHC';	//!!!?
		s_mapByUserAgent["EasticoinCore"]	= 'EAC';
		s_mapByUserAgent["MindexcoinCore"]	= 'MIC';
		s_mapByUserAgent["MonsoonCoinCore"] = 'MSC';
		s_mapByUserAgent["WorldcoinFoundation"] = 'WDC';
		

		for (size_t i = 0; i< size(s_genesises); ++i)
			s_mapByGenesis[HashValue(s_genesises[i].P)] = s_genesises[i].Sym;
	}
} s_initMap;

static regex s_reUserAgent("/([^/]+):([^/]+)/");

int DetectBlockchain(RCString userAgent) {
	string s = userAgent;
	smatch m;
	if (regex_match(s, m, s_reUserAgent))
		if (auto o = Lookup(s_mapByUserAgent, String(m[1])))
			return o.value();
	return 0;
}

int DetectBlockchain(const HashValue& hashGenesis) {
	if (auto o = Lookup(s_mapByGenesis, hashGenesis))
		return o.value();
	return 0;
}

} // Coin::

