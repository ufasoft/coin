/*######     Copyright (c) 1997-2013 Ufasoft  http://ufasoft.com  mailto:support@ufasoft.com,  Sergey Pavlov  mailto:dev@ufasoft.com #######################################
#                                                                                                                                                                          #
# This program is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation;  #
# either version 3, or (at your option) any later version. This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the      #
# implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details. You should have received a copy of the GNU #
# General Public License along with this program; If not, see <http://www.gnu.org/licenses/>                                                                               #
##########################################################################################################################################################################*/

#include <el/ext.h>

#include "proof-of-stake.h"


namespace Coin {

const Version VER_PPCOIN_STAKE_MODIFIER(0, 50);

class PPCoinBlockObj : public PosBlockObj {
	typedef PPCoinBlockObj class_type;
	typedef PosBlockObj base;
public:

	PPCoinBlockObj()
		:	DtV30Switch(2013, 3, 20, 17, 20)
	{}

	bool IsV02Protocol(const DateTime& dt) const {
		return dt < DtV30Switch;
	}

	void CheckCoinStakeTimestamp() override {
		DateTime dtTx = GetTxObj(1).Timestamp;
		if (!IsV02Protocol(dtTx))
			base::CheckCoinStakeTimestamp();
		else if (dtTx > Timestamp || Timestamp > dtTx+TimeSpan::FromSeconds(MAX_FUTURE_SECONDS)) 
			Throw(E_COIN_TimestampViolation);
	}

	void WriteKernelStakeModifier(BinaryWriter& wr, const Block& blockPrev) const override {
		DateTime dtTx = GetTxObj(1).Timestamp;
		if (!IsV02Protocol(dtTx))
			base::WriteKernelStakeModifier(wr, blockPrev);
		else
			wr << DifficultyTargetBits;		
	}

private:
	DateTime DtV30Switch;

};


class COIN_CLASS PPCoinEng : public PosEng {
	typedef PosEng base;
public:
	PPCoinEng(CoinDb& cdb)
		:	base(cdb)
	{
	}

protected:
	void TryUpgradeDb() override {
		if (Db->CheckUserVersion() < VER_PPCOIN_STAKE_MODIFIER)
			Db->Recreate();
		base::TryUpgradeDb();
	}

	BlockObj *CreateBlockObj() override { return new PPCoinBlockObj; }


};

static class PPCoinChainParams : public ChainParams {
	typedef ChainParams base;
public:
	PPCoinChainParams(bool)
		:	base("PPCoin", false)
	{	
//!!!R		MaxPossibleTarget = Target(0x1D00FFFF);
		ChainParams::Add(_self);
	}

protected:
	PPCoinEng *CreateObject(CoinDb& cdb, IBlockChainDb *db) override {
		PPCoinEng *r = new PPCoinEng(cdb);
		r->SetChainParams(_self, db);
		return r;
	}

} s_ppcoinParams(true);




} // Coin::

