/*######     Copyright (c) 1997-2014 Ufasoft  http://ufasoft.com  mailto:support@ufasoft.com,  Sergey Pavlov  mailto:dev@ufasoft.com #########################################################################################################
#                                                                                                                                                                                                                                            #
# This program is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation;  either version 3, or (at your option) any later version.          #
# This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.   #
# You should have received a copy of the GNU General Public License along with this program; If not, see <http://www.gnu.org/licenses/>                                                                                                      #
############################################################################################################################################################################################################################################*/

#include <el/ext.h>

#include "../eng.h"

namespace Coin {

class MetisBlockObj : public BlockObj {
	typedef BlockObj base;
public:
	MetisBlockObj() {
		Ver = 112;
	}
protected:
};

class MetisTxObj : public TxObj {
	typedef TxObj base;
public:
	String Comment;

protected:
	MetisTxObj *Clone() const override { return new MetisTxObj(_self); }
	String GetComment() const override { return Comment; }

	void WriteSuffix(BinaryWriter& wr) const {
		if (Ver >= 2)
			WriteString(wr, Comment);
	}

	void ReadSuffix(const BinaryReader& rd) {
		if (Ver >= 2)
			Comment = ReadString(rd);
	}
};

class COIN_CLASS MetisCoinEng : public CoinEng {
	typedef CoinEng base;
public:
	MetisCoinEng(CoinDb& cdb)
		:	base(cdb)
	{
		MaxBlockVersion = 112;
	}
protected:
	bool MiningAllowed() override { return false; }
	BlockObj *CreateBlockObj() override { return new MetisBlockObj; }
	TxObj *CreateTxObj() override { return new MetisTxObj; }
};

static class MetisCoinChainParams : public ChainParams {
	typedef ChainParams base;
public:
	MetisCoinChainParams(bool)
		:	base("MetisCoin", false)
	{	
		MaxPossibleTarget = Target(0x1E00FFFF);
		ChainParams::Add(_self);
	}

	MetisCoinEng *CreateEng(CoinDb& cdb) override { return new MetisCoinEng(cdb); }
} s_metisCoinParams(true);


} // Coin::


