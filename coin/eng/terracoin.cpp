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

const Target TERRACOIN_FIVE_THOUSAND_LIMIT(0x1B0C7898);

class COIN_CLASS TerraCoinEng : public CoinEng {
	typedef CoinEng base;
public:
	TerraCoinEng(CoinDb& cdb)
		:	base(cdb)
	{
		Caches.HashToBlockCache.SetMaxSize(2165);
	}
protected:
	int GetIntervalForModDivision(int height) override {
		return height > 181200 ? base::GetIntervalForModDivision(height) : 30;
	}

	int GetIntervalForCalculatingTarget(int height) override {
		int r = base::GetIntervalForCalculatingTarget(height);
		return height > 181200 ? r
			: height > 101908 ? 90
			: height > 99988 ? 720
			: 30; 
	}

	TimeSpan GetActualSpanForCalculatingTarget(const BlockObj& bo, int nInterval) override {
		int r = (int)base::GetActualSpanForCalculatingTarget(bo, nInterval + int(bo.Height>99988)).TotalSeconds;
		return TimeSpan::FromSeconds(
			bo.Height > 101908 ? r / 3 * 3
			: bo.Height > 99988 ? r / 24 * 24
			: r);
	}

	Target GetNextTargetRequired(const Block& blockLast, const Block& block) override {
		if (blockLast.Height > 181200)
			return base::GetNextTargetRequired(blockLast, block);

		if (blockLast.Height <= 101631)
			return base::GetNextTargetRequired(blockLast, block);

		Target r;
		if (block.get_Timestamp()-blockLast.Timestamp > ChainParams.BlockSpan*10 && blockLast.Height <= 175000)
			r = Target(BigInteger(blockLast.get_DifficultyTarget()) * (blockLast.Height>101631 && blockLast.Height<103791 ? 10 : 2));
		else {
			const int PER_BLOCK_SPAN = (int)ChainParams.BlockSpan.TotalSeconds;	
			vector<int> durations(2160);
			Block b = blockLast;
			for (vector<int>::reverse_iterator it=durations.rbegin(), e=durations.rend(); it!=e; ++it) {
				Block prev = b.GetPrevBlock();
				int dur = (int)(exchange(b, prev).get_Timestamp()-prev.get_Timestamp()).TotalSeconds;
				if (blockLast.Height > 110322) {
					dur = std::min(PER_BLOCK_SPAN*3/2, dur);
					if (dur >= 0)
						dur = std::max(PER_BLOCK_SPAN/2, dur);
				}
				*it = dur<0 && blockLast.Height>104290 ? PER_BLOCK_SPAN : dur;
			}
			float acc = (float)PER_BLOCK_SPAN;
			for (int i=0; i<durations.size(); ++i)
				acc = blockLast.Height>110322 ? 0.06f*durations[i] + (1-0.06f)*acc				// float/double conversions are not well defined. Another way to calculate diverges with reference TerraCoin implementation
									: 0.09f*durations[i] + (1-0.09f)*acc;
			int actualSpan = std::min(std::max((int)acc, PER_BLOCK_SPAN/2), PER_BLOCK_SPAN * (blockLast.Height>110322 ? 2 : 4));
			r = Target(BigInteger(blockLast.get_DifficultyTarget()) * actualSpan / PER_BLOCK_SPAN);
		}
		return std::min(blockLast.Height>104290 ? std::min(r, TERRACOIN_FIVE_THOUSAND_LIMIT) : r, ChainParams.MaxTarget);
	}
};


static class TerraCoinChainParams : public ChainParams {
	typedef ChainParams base;
public:
	TerraCoinChainParams(bool)
		:	base("TerraCoin", false)
	{	
		ChainParams::Add(_self);
	}

protected:
	TerraCoinEng *CreateObject(CoinDb& cdb, IBlockChainDb *db) override {
		TerraCoinEng *r = new TerraCoinEng(cdb);
		r->SetChainParams(_self, db);
		return r;
	}

} s_terracoinParams(true);


} // Coin::

