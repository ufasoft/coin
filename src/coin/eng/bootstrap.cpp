/*######   Copyright (c) 2011-2015 Ufasoft  http://ufasoft.com  mailto:support@ufasoft.com,  Sergey Pavlov  mailto:dev@ufasoft.com ####
#                                                                                                                                     #
# 		See LICENSE for licensing information                                                                                         #
#####################################################################################################################################*/

#include <el/ext.h>

#include "coin-protocol.h"
#include "eng.h"


namespace Coin {

void BootstrapDbThread::Execute() {
	Name = "BootstrapDbThread";

	CCoinEngThreadKeeper engKeeper(&Eng);
	FileStream stmFile(PathBootstrap, FileMode::Open, FileAccess::Read, FileShare::Read);
	stmFile.Position = Eng.OffsetInBootstrap;
	BufferedStream stm(stmFile);
	BinaryReader rd(stm);
	try {
		while (!m_bStop && !stm.Eof()) {
			if (rd.ReadUInt32() != Eng.ChainParams.ProtocolMagic)
				Throw(E_COIN_InvalidBootstrapFile);
			size_t size = rd.ReadUInt32();
			uint64_t pos = stm.Position;
			Block block;
			block.Read(rd);
			block.m_pimpl->OffsetInBootstrap = pos;
			block.Process(nullptr);
			if (stm.Position != pos+size)
				Throw(E_COIN_InvalidBootstrapFile);
			Eng.OffsetInBootstrap = stm.Position;
			TRC(9, "Pos: " << stm.Position);
		}
	} catch (RCExc ex) {
		TRC(1, ex.what())
	}
	Eng.UpgradingDatabaseHeight = 0;
}


} // Coin::

