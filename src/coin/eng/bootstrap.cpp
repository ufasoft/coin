/*######   Copyright (c) 2011-2015 Ufasoft  http://ufasoft.com  mailto:support@ufasoft.com,  Sergey Pavlov  mailto:dev@ufasoft.com ####
#                                                                                                                                     #
# 		See LICENSE for licensing information                                                                                         #
#####################################################################################################################################*/

#include <el/ext.h>

#include "coin-protocol.h"
#include "eng.h"


namespace Coin {

void BootstrapDbThread::BeforeStart() {
	if (!Exporting)
		Eng.UpgradingDatabaseHeight = CoinEng::HEIGHT_BOOTSTRAPING;
}

TRC_GLOBAL_CUT_PREFIX("Ext::");
TRC_GLOBAL_CUT_PREFIX("Coin::");

void BootstrapDbThread::Execute() {
	Name = "BootstrapDbThread";
	CCoinEngThreadKeeper engKeeper(&Eng);
	if (Exporting) {
		Eng.ExportToBootstrapDat(PathBootstrap);
		return;
	}

	DBG_LOCAL_IGNORE_CONDITION(ExtErr::EndOfStream);
	DBG_LOCAL_IGNORE_CONDITION(CoinErr::InvalidBootstrapFile);

	CEngStateDescription stateDesc(Eng, EXT_STR((Indexing ? "Indexing " : "Bootstrapping from ") << PathBootstrap));

	FileStream stm(PathBootstrap, FileMode::Open, FileAccess::Read, FileShare::ReadWrite, Stream::DEFAULT_BUF_SIZE, FileOptions::SequentialScan);
	stm.Position = Eng.OffsetInBootstrap;
	ProtocolReader rd(stm);
	rd.WitnessAware = true;
	try {
		while (!m_bStop && !stm.Eof()) {
			if (rd.ReadUInt32() != Eng.ChainParams.DiskMagic)
				Throw(CoinErr::InvalidBootstrapFile);
			size_t size = rd.ReadUInt32();
			uint64_t pos = stm.Position;
			Block block;
			block.Read(rd);
			Eng.NextOffsetInBootstrap = stm.Position;
			block->OffsetInBootstrap = pos;
			block.Process(nullptr);
			if (stm.Position != pos + size)
				Throw(CoinErr::InvalidBootstrapFile);
			Eng.OffsetInBootstrap = Eng.NextOffsetInBootstrap;
			TRC(9, "Pos: " << stm.Position);
		}
	} catch (RCExc ex) {
		TRC(1, ex.what())
	}
	Eng.UpgradingDatabaseHeight = 0;
}

void CoinEng::ExportToBootstrapDat(const path& pathBoostrap) {
	uint32_t n = Db->GetMaxHeight()+1;

#ifndef X_DEBUG//!!!T
	if (Mode == EngMode::Bootstrap) {
		CEngStateDescription stateDesc(_self, EXT_STR("Copying " << GetBootstrapPath() << " -> " << pathBoostrap));

		return (void)copy_file(GetBootstrapPath(), pathBoostrap, copy_options::overwrite_existing);
	}
#endif

	CEngStateDescription stateDesc(_self, EXT_STR("Exporting " << n << " blocks to " << pathBoostrap));

	FileStream fs(pathBoostrap, FileMode::Create, FileAccess::Write);
	BinaryWriter wr(fs);

	for (uint32_t i = 0; i < n && Runned; ++i) {
		wr << ChainParams.DiskMagic;
		MemoryStream ms;

		Block block = GetBlockByHeight(i);
//!!!?		block.LoadToMemory();
//!!!?		EXT_FOR (const Tx& tx, block.Txes) {
//!!!?			//			tx->m_nBytesOfHash = 0;
//!!!?		}
//!!!?		block->m_hash.reset();
//!!!?block->m_txHashesOutNums.clear();

		block.Write(ProtocolWriter(ms).Ref());
		wr << uint32_t(ms.Position);
		fs.Write(ms);
	}
}



} // Coin::

