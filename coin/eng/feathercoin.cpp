/*######     Copyright (c) 1997-2013 Ufasoft  http://ufasoft.com  mailto:support@ufasoft.com,  Sergey Pavlov  mailto:dev@ufasoft.com #######################################
#                                                                                                                                                                          #
# This program is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation;  #
# either version 3, or (at your option) any later version. This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the      #
# implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details. You should have received a copy of the GNU #
# General Public License along with this program; If not, see <http://www.gnu.org/licenses/>                                                                               #
##########################################################################################################################################################################*/

#include <el/ext.h>

#include "eng.h"
#include "script.h"
#include "coin-model.h"


namespace Coin {

class COIN_CLASS FeatherCoinEng : public CoinEng {
	typedef CoinEng base;
public:
	FeatherCoinEng(CoinDb& cdb)
		:	base(cdb)
	{
	}
protected:
	Target GetNextTargetRequired(const Block& blockLast, const Block& block) {
		int h = blockLast.Height+1;
		int nInterval = h<33000 ? 504*4 : 504;
		if (h % nInterval && h != 33000)
			return blockLast.DifficultyTarget;
		
		Block prev = blockLast;
		for (int goback = nInterval - int(h==nInterval); goback--;)
			prev = prev.GetPrevBlock();
		int targetSpan = int((ChainParams.BlockSpan * nInterval).get_TotalSeconds()),
			spanActual = int((blockLast.get_Timestamp() - prev.get_Timestamp()).get_TotalSeconds());
		spanActual = h < 33000 ? std::min(std::max(spanActual, targetSpan/4), targetSpan*4)
			:	std::min(std::max(spanActual, targetSpan*70/99), targetSpan*99/70);

		return std::min(Target(BigInteger(blockLast.get_DifficultyTarget())*spanActual/targetSpan), ChainParams.MaxTarget);		//!!!
	}
};


static class FeatherCoinChainParams : public ChainParams {
	typedef ChainParams base;
public:
	FeatherCoinChainParams(bool)
		:	base("FeatherCoin", false)
	{	
		ChainParams::Add(_self);
	}

protected:
	FeatherCoinEng *CreateObject(CoinDb& cdb, IBlockChainDb *db) override {
		FeatherCoinEng *r = new FeatherCoinEng(cdb);
		r->SetChainParams(_self, db);
		return r;
	}

} s_feathercoinParams(true);


} // Coin::

