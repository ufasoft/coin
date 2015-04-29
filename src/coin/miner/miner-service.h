/*######   Copyright (c) 2013-2015 Ufasoft  http://ufasoft.com  mailto:support@ufasoft.com,  Sergey Pavlov  mailto:dev@ufasoft.com ####
#                                                                                                                                     #
# 		See LICENSE for licensing information                                                                                         #
#####################################################################################################################################*/

#pragma once 

#include "miner.h"

namespace Coin {

extern String MINER_SERVICE_NAME;

void InstallMinerService();
void UninstallMinerService();
void RunMinerAsService(BitcoinMiner& miner);

} // Coin::
