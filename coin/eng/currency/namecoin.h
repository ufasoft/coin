/*######     Copyright (c) 1997-2014 Ufasoft  http://ufasoft.com  mailto:support@ufasoft.com,  Sergey Pavlov  mailto:dev@ufasoft.com #######################################
#                                                                                                                                                                          #
# This program is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation;  #
# either version 3, or (at your option) any later version. This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the      #
# implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details. You should have received a copy of the GNU #
# General Public License along with this program; If not, see <http://www.gnu.org/licenses/>                                                                               #
##########################################################################################################################################################################*/

#pragma once

#include "../eng.h"

namespace Coin {

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

class COIN_CLASS NamecoinEng : public CoinEng {
	typedef CoinEng base;
public:
	CBool ResolverMode;

#if UCFG_COIN_COINCHAIN_BACKEND == COIN_BACKEND_SQLITE
	SqliteConnection& m_db;
#else
	SqliteConnection m_db;

	bool OpenDb() override;

	void Close() override {
		m_db.Close();
		base::Close();
	}

	void BeginTransaction() override { m_db.BeginTransaction(); }
	void Commit() override { m_db.Commit(); }
	void Rollback() override { m_db.Rollback(); }	
#endif // UCFG_COIN_COINCHAIN_BACKEND == COIN_BACKEND_SQLITE


	NamecoinEng(CoinDb& cdb, IBlockChainDb *db)
		:	base(cdb)
#if UCFG_COIN_COINCHAIN_BACKEND == COIN_BACKEND_SQLITE
		,	m_db(*(SqliteConnection*)db->GetDbObject())
#endif
		,	m_cmdInsertDomain("INSERT INTO domains (name, txid) VALUES (?, ?)"				, m_db)
		,	m_cmdUpdateDomain("UPDATE domains SET txid=? WHERE name=?"						, m_db)
		,	m_cmdFindDomain("SELECT txid FROM domains WHERE name=?"							, m_db)
		,	m_cmdDeleteDomain("DELETE FROM domains WHERE txid=?"							, m_db)
		,	m_cmdNameHeight("SELECT blockid FROM domains JOIN txes ON domains.txid=txes.id WHERE name=?"	, m_db)
	{
#if UCFG_COIN_COINCHAIN_BACKEND == COIN_BACKEND_DBLITE
		ResolverMode = true;
#endif
	}

	void Load() override {
		if (ResolverMode) {
			m_cmdFindDomain.CommandText = "SELECT address FROM domains WHERE name=?";
			m_cmdInsertDomain.CommandText = "INSERT INTO domains (name, address, height) VALUES (?, ?, ?)";
			m_cmdUpdateDomain.CommandText = "UPDATE domains SET address=?, height=? WHERE name=?";
			m_cmdNameHeight.CommandText = "SELECT height FROM domains WHERE name=?";
		}
		base::Load();
	}

	pair<bool, String> FindName(RCString name);
protected:
	String GetDomainsDbPath() {
		String s = GetDbFilePath();
		return Path::Combine(Path::GetDirectoryName(s), Path::GetFileNameWithoutExtension(s)+"-domains.db");
	}

	void CreateAdditionalTables() override;
	void OnCheck(const Tx& tx) override;
	void OnConnectInputs(const Tx& tx, const vector<Tx>& vTxPrev, bool bBlock, bool bMiner) override;
	void OnConnectBlock(const Block& block) override;
	void OnDisconnectInputs(const Tx& tx) override;
//!!!	bool ShouldBeSaved(const Tx& tx) override;
private:
	SqliteCommand m_cmdInsertDomain, m_cmdUpdateDomain, m_cmdFindDomain, m_cmdNameHeight, m_cmdDeleteDomain;

	int GetNameHeight(const ConstBuf& cbufName) {
		DbDataReader dr = m_cmdNameHeight.Bind(1, ToStringName(cbufName)).ExecuteReader();
		return dr.Read() ? dr.GetInt32(0) : -1;
	}

	Int64 GetNetworkFee(int height);
};


} // Coin::

