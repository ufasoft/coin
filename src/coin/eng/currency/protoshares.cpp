/*######     Copyright (c) 1997-2015 Ufasoft  http://ufasoft.com  mailto:support@ufasoft.com,  Sergey Pavlov  mailto:dev@ufasoft.com #########################################################################################################
#                                                                                                                                                                                                                                            #
# This program is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation;  either version 3, or (at your option) any later version.          #
# This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.   #
# You should have received a copy of the GNU General Public License along with this program; If not, see <http://www.gnu.org/licenses/>                                                                                                      #
############################################################################################################################################################################################################################################*/

#include <el/ext.h>

#include "../eng.h"

namespace Coin {

static HashValue s_hashMax("FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF");

class ProtoSharesBlockObj : public BlockObj {
	typedef BlockObj base;
public:
	uint32_t BirthdayA, BirthdayB;

	ProtoSharesBlockObj() {
		Ver = 1;
	};

	void WriteHeader(BinaryWriter& wr) const override {
		base::WriteHeader(wr);
		wr << BirthdayA << BirthdayB;
	}

	void ReadHeader(const BinaryReader& rd, bool bParent, const HashValue *pMerkleRoot) override {
		base::ReadHeader(rd, bParent, pMerkleRoot);
		rd >> BirthdayA >> BirthdayB;
	}

	using base::Write;
	using base::Read;

	void Write(DbWriter& wr) const override {
		base::Write(wr);
		wr << BirthdayA << BirthdayB;
	}

	void Read(const DbReader& rd) override {
		base::Read(rd);
		rd >> BirthdayA >> BirthdayB;
	}

	Coin::HashValue Hash() const override {
		if (!m_hash) {
			MemoryStream ms;
			base::WriteHeader(BinaryWriter(ms).Ref());
			m_hash = MomentumVerify(Coin::Hash(ms), BirthdayA, BirthdayB)
				? Coin::Hash(EXT_BIN(Ver << PrevBlockHash << MerkleRoot() << (uint32_t)to_time_t(Timestamp) << get_DifficultyTarget() << Nonce << BirthdayA << BirthdayB))
				: s_hashMax;
		}
		return m_hash.get();
	}

};

class ProtoSharesEng : public CoinEng {
	typedef CoinEng base;
public:
	ProtoSharesEng(CoinDb& cdb)
		:	base(cdb)
	{
	}

protected:
	bool MiningAllowed() override { return false; }
	BlockObj *CreateBlockObj() override { return new ProtoSharesBlockObj; }

	int64_t GetSubsidy(int height, const HashValue& prevBlockHash, double difficulty, bool bForCheck) override {
		return std::max(int64_t(19012850), int64_t(ChainParams.InitBlockValue * pow(0.95, height / 2016)) + (bForCheck ? 1 : -1));		// +1|-1 to avoid FP divergence from Integer algorithm
	}

	TimeSpan AdjustSpan(int height, const TimeSpan& span, const TimeSpan& targetSpan) override {
		return clamp(span, targetSpan / (height<11999 ? 4 : 32), targetSpan*4);
	}

	TimeSpan GetActualSpanForCalculatingTarget(const BlockObj& bo, int nInterval) override {
		return bo.Timestamp - GetBlockByHeight(std::max(0, int(bo.Height - nInterval))).Timestamp;
	}
};


static class ProtoSharesChainParams : public ChainParams {
	typedef ChainParams base;
public:
	ProtoSharesChainParams(bool)
		:	base("ProtoShares", false)
	{	
		ChainParams::Add(_self);		
	}

	ProtoSharesEng *CreateEng(CoinDb& cdb) override { return new ProtoSharesEng(cdb); }
} s_protoSharesParams(true);


} // Coin::

