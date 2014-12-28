/*######     Copyright (c) 1997-2015 Ufasoft  http://ufasoft.com  mailto:support@ufasoft.com,  Sergey Pavlov  mailto:dev@ufasoft.com #########################################################################################################
#                                                                                                                                                                                                                                            #
# This program is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation;  either version 3, or (at your option) any later version.          #
# This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.   #
# You should have received a copy of the GNU General Public License along with this program; If not, see <http://www.gnu.org/licenses/>                                                                                                      #
############################################################################################################################################################################################################################################*/

#pragma once

#include "../eng.h"

namespace Coin {

extern const Version VER_NAMECOIN_DOMAINS;

enum NameOpcode {
	OP_NAME_INVALID 	= 0,
	OP_NAME_NEW			= 1,
	OP_NAME_FIRSTUPDATE = 2,
	OP_NAME_UPDATE 		= 3,
	OP_NAME_NOP 		= 4
};

struct DecodedTx {
	vector<Blob> Args;
	Blob Value;
	int NOut;
	NameOpcode Op;
};

COIN_EXPORT DecodedTx DecodeNameTx(const Tx& tx);
String ToStringName(const ConstBuf& cbuf);

class DomainData : public CPersistent {
public:
	String AddressData;
	uint32_t Height;

	DomainData()
		:	AddressData(nullptr)
	{}

	void Write(BinaryWriter& wr) const override {
		wr << Height << AddressData;
	}

	void Read(const BinaryReader& rd) override {
		rd >> Height >> AddressData;
	}

	explicit operator bool() { return AddressData != nullptr; }
};

interface INamecoinDb {
	virtual int GetNameHeight(const ConstBuf& cbufName, int heightExpired) =0;
	virtual void OptionalDeleteExpiredDomains(uint32_t height) =0;
	virtual DomainData Resolve(RCString domain) =0;
	virtual void PutDomainData(RCString domain, uint32_t height, const HashValue& hashTx, RCString addressData, bool bInsert) =0;
};

class COIN_CLASS NamecoinEng : public CoinEng {
	typedef CoinEng base;
public:
	NamecoinEng(CoinDb& cdb)
		:	base(cdb)
	{
	}

	INamecoinDb& NamecoinDb();
protected:
	void TryUpgradeDb() override {
		if (Db->CheckUserVersion() < VER_NAMECOIN_DOMAINS)
			Db->Recreate();
		base::TryUpgradeDb();
	}

	void OnCheck(const Tx& tx) override;
	void OnConnectInputs(const Tx& tx, const vector<Tx>& vTxPrev, bool bBlock, bool bMiner) override;
	void OnConnectBlock(const Block& block) override;
	void OnDisconnectInputs(const Tx& tx) override;
	ptr<IBlockChainDb> CreateBlockChainDb() override;
private:
	int64_t GetNetworkFee(int height);
};


} // Coin::

