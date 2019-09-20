/*######   Copyright (c) 2011-2015 Ufasoft  http://ufasoft.com  mailto:support@ufasoft.com,  Sergey Pavlov  mailto:dev@ufasoft.com ####
#                                                                                                                                     #
# 		See LICENSE for licensing information                                                                                         #
#####################################################################################################################################*/

#include <el/ext.h>

#include <el/bignum.h>
#include <el/crypto/hash.h>


#include "../coin-protocol.h"
#include "../script.h"
#include "../eng.h"
#include "namecoin.h"

#if UCFG_COIN_COINCHAIN_BACKEND == COIN_BACKEND_DBLITE
#	include "../backend-dblite.h"
#endif

namespace Coin {

const Version VER_NAMECOIN_DOMAINS(0, 81);

#if UCFG_COIN_COINCHAIN_BACKEND == COIN_BACKEND_DBLITE

class NamecoinDbliteDb : public DbliteBlockChainDb, public INamecoinDb {
	typedef DbliteBlockChainDb base;
public:
	DbTable m_tableDomains;

	NamecoinDbliteDb()
		: m_tableDomains("domains", 0, TableType::HashTable)
	{
	}
protected:
	void OnOpenTables(DbTransaction& dbt, bool bCreate) override {
		base::OnOpenTables(dbt, bCreate);
		if (bCreate || m_db.UserVersion >= VER_NAMECOIN_DOMAINS)
			m_tableDomains.Open(dbt, bCreate);
	}

	int GetNameHeight(RCSpan cbufName, int heightExpired) override {
		DbReadTxRef dbt(m_db);
		DbCursor c(dbt, m_tableDomains);
		if (c.SeekToKey(cbufName)) {
			int r = (int)letoh(*(uint32_t*)c.get_Data().P);
			if (r > heightExpired)
				return r;
			c.Delete();
		}
		return -1;
	}

	DomainData Resolve(RCString domain) override {
		DomainData r;
		const char *pDomain = domain;
		Span cbufName(pDomain, strlen(pDomain));
		DbReadTxRef dbt(m_db);
		DbCursor c(dbt, m_tableDomains);
		if (c.SeekToKey(cbufName))
			BinaryReader(CMemReadStream(c.get_Data())) >> r;
		return r;
	}

	void PutDomainData(RCString domain, uint32_t height, const HashValue& hashTx, RCString addressData, bool bInsert) override {
		const char *pDomain = domain;
		Span cbufName(pDomain, strlen(pDomain));
		if (!bInsert && GetNameHeight(cbufName, -1) < 0)
			Throw(CoinErr::InconsistentDatabase);
		DomainData dd;
		dd.Height = height;
		dd.AddressData = addressData;
		DbTxRef dbt(m_db);
		m_tableDomains.Put(dbt, cbufName, EXT_BIN(dd), bInsert);
	}

	void OptionalDeleteExpiredDomains(uint32_t height) override {
		if (height & 0xFFF)
			return;
		DbTxRef dbt(m_db);
		for (DbCursor c(dbt, m_tableDomains); c.SeekToNext();) {
			if (letoh(*(uint32_t*)c.get_Data().P) <= height)
				c.Delete();
		}
	}
};

typedef NamecoinDbliteDb NamecoinDbType;

#else

class NamecoinSqliteDb : public SqliteBlockChainDb, public INamecoinDb {
	typedef SqliteBlockChainDb base;
public:
	CBool ResolverMode;

	NamecoinSqliteDb()
		: ResolverMode(true)
		, m_cmdInsertDomain("INSERT INTO domains (name, txid) VALUES (?, ?)"				, m_db)
		, m_cmdUpdateDomain("UPDATE domains SET txid=? WHERE name=?"						, m_db)
		, m_cmdFindDomain("SELECT txid FROM domains WHERE name=?"							, m_db)
		, m_cmdDeleteDomain("DELETE FROM domains WHERE txid=?"							, m_db)
		, m_cmdNameHeight("SELECT blockid FROM domains JOIN txes ON domains.txid=txes.id WHERE name=?"	, m_db)
	{
		if (ResolverMode) {
			m_cmdFindDomain.CommandText = "SELECT address, height FROM domains WHERE name=?";
			m_cmdInsertDomain.CommandText = "INSERT INTO domains (name, address, height) VALUES (?, ?, ?)";
			m_cmdUpdateDomain.CommandText = "UPDATE domains SET address=?, height=? WHERE name=?";
			m_cmdNameHeight.CommandText = "SELECT height FROM domains WHERE name=?";
		}
	}

protected:
	SqliteCommand m_cmdInsertDomain, m_cmdUpdateDomain, m_cmdFindDomain, m_cmdNameHeight, m_cmdDeleteDomain;

	bool Create(RCString path) override {
		bool r = base::Create(path);
		if (ResolverMode)
			m_db.ExecuteNonQuery("CREATE TABLE domains (name PRIMARY KEY"
													", address"
													", height)");
		else
			m_db.ExecuteNonQuery("CREATE TABLE domains (txid INTEGER PRIMARY KEY"
													", name UNIQUE)");
		return r;
	}

	int GetNameHeight(RCSpan cbufName, int heightExpired) override {
		DbDataReader dr = m_cmdNameHeight.Bind(1, ToStringName(cbufName)).ExecuteReader();
		return dr.Read() ? dr.GetInt32(0) : -1;
	}

	DomainData Resolve(RCString domain) override {
		if (!ResolverMode)
			Throw(E_NOTIMPL);
		DomainData r;
		EXT_LOCK (MtxSqlite) {
			DbDataReader dr = m_cmdFindDomain.Bind(1, domain).ExecuteReader();
			if (dr.Read()) {
				r.AddressData = dr.GetString(0);
				r.Height = dr.GetInt32(1);
			}
		}
		return r;
	}

	void PutDomainData(RCString domain, uint32_t height, const HashValue& hashTx, RCString addressData, bool bInsert) override {
		if (ResolverMode) {
			if (bInsert) {
				m_cmdInsertDomain.Bind(1, domain)
								.Bind(2, addressData)
								.Bind(3, height)
								.ExecuteNonQuery();
			} else {
				m_cmdUpdateDomain.Bind(3, domain)
								.Bind(1, addressData)
								.Bind(2, height)
								.ExecuteNonQuery();
			}
		} else {
			if (bInsert) {
				m_cmdInsertDomain.Bind(1, domain)
								.Bind(2, ReducedHashValue(hashTx))
								.ExecuteNonQuery();
			} else {
				m_cmdUpdateDomain.Bind(1, ReducedHashValue(hashTx))
								.Bind(2, domain)
								.ExecuteNonQuery();
			}
		}
	}

	void OptionalDeleteExpiredDomains(uint32_t height) override {
		if (ResolverMode) {
			if (!(heightExpired & 0xFF)) {
				SqliteCommand(EXT_STR("DELETE FROM domains WHERE height <= " << heightExpired), m_db)
					.ExecuteNonQuery();
			}
		} else {
			Block blockExpired = GetBlockByHeight(heightExpired);
			EXT_FOR (const TxHashOutNum& hom, blockExpired.get_TxHashesOutNums()) {
				m_cmdDeleteDomain
					.Bind(1, ReducedHashValue(hom.HashTx))
					.ExecuteNonQuery();
			}
		}
	}
};

typedef NamecoinSqliteDb NamecoinDbType;

#endif // COIN_BACKEND_DBLITE

const int NAMECOIN_TX_VERSION = 0x7100;

const int MAX_NAME_LENGTH = 255,
		 MAX_VALUE_LENGTH = 1023;

const int MIN_FIRSTUPDATE_DEPTH = 12;


String ToStringName(RCSpan cbuf) {
	try {
		DBG_LOCAL_IGNORE_CONDITION(ExtErr::InvalidUTF8String);

		return Encoding::UTF8.GetChars(cbuf);
	} catch (RCExc) {
		return Encoding::GetEncoding(1252)->GetChars(cbuf);
	}
}

pair<bool, DecodedTx> DecodeNameScript(RCSpan cbufScript) {
	pair<bool, DecodedTx> r;

	Script script(cbufScript);
	if (script.size() < 1)
		return r;
	int op = script[0].Opcode;
	if (op != Opcode::OP_PUSHDATA1)
		return r;
	BigInteger n = ToBigInteger(script[0].Value);
	if (n < 1 || n > 16)
		return r;
	r.second.Op = (NameOpcode)(int)explicit_cast<int>(n);
	Script::iterator it = script.begin() + 1;
	for (; it!=script.end(); ++it) {
		switch (op = it->Opcode) {
		case Opcode::OP_DROP:
		case Opcode::OP_2DROP:
		case Opcode::OP_NOP:
			{
				++it;
				while (it != script.end() && (op = it->Opcode) != Opcode::OP_DROP && op != Opcode::OP_2DROP && op != Opcode::OP_NOP)
					++it;
				r.first = r.second.Op == OP_NAME_NEW && r.second.Args.size() == 1 ||
						  r.second.Op == OP_NAME_FIRSTUPDATE && r.second.Args.size() == 3 ||
						  r.second.Op == OP_NAME_UPDATE && r.second.Args.size() == 2;
				goto LAB_RET;
			}
		default:
			if (op < 0 || op > Opcode::OP_PUSHDATA4)
				return r;
		}
		r.second.Args.push_back(it->Value);
	}
LAB_RET:
	switch (r.second.Op) {
	case OP_NAME_FIRSTUPDATE:
		r.second.Value = r.second.Args[2];
		break;
	case OP_NAME_UPDATE:
		r.second.Value = r.second.Args[1];
		break;
	}
	return r;
}

DecodedTx DecodeNameTx(const Tx& tx) {
	DecodedTx r;
	bool bFound = false;
	for (int i=0; i<tx.TxOuts().size(); ++i) {
		try {
			DBG_LOCAL_IGNORE_CONDITION(CoinErr::InvalidScript);

			pair<bool, DecodedTx> pp = DecodeNameScript(tx.TxOuts()[i].get_ScriptPubKey());
			if (pp.first) {
				if (exchange(bFound, true))
					Throw(CoinErr::NAME_InvalidTx);
				r = pp.second;
				r.NOut = i;
			}
		} catch (Exception& ex) {
			if (ex.code() != CoinErr::InvalidScript)
				throw;
		}
	}
	if (!bFound)
		Throw(CoinErr::NAME_InvalidTx);
	return r;
}

void NamecoinEng::OnCheck(const Tx& tx) {
	if (tx->Ver != NAMECOIN_TX_VERSION)
		return;
	DecodedTx dt = DecodeNameTx(tx);
	if (dt.Args[0].Size > MAX_NAME_LENGTH)
		Throw(CoinErr::NAME_ToolLongName);
	switch (dt.Op) {
	case OP_NAME_NEW:
		if (dt.Args[0].Size == 20)
			return;
		break;
	case OP_NAME_FIRSTUPDATE:
		if (dt.Args[1].Size > 20)
			break;
	case OP_NAME_UPDATE:
		if (dt.Value.Size <= MAX_VALUE_LENGTH)
			return;
		break;
	}
	Throw(CoinErr::NAME_InvalidTx);
}

static int GetExpirationDepth(int height) {
	return height < 24000 ? 12000
		 : height < 48000 ? height - 12000
		 : 36000;
}

static int GetRelativeDepth(const Tx& tx, const Tx& txPrev, int maxDepth) {
	int depth = tx.Height - txPrev.Height;
	return depth < maxDepth ? depth : -1;
}

static int64_t GetNameNetFee(const Tx& tx) {
	int64_t r = 0;
	EXT_FOR (const TxOut& txOut, tx.TxOuts()) {
		if (txOut.get_ScriptPubKey().size() == 1 && txOut.get_ScriptPubKey()[0] == OP_RETURN)
			r += txOut.Value;
	}
	return r;
}

int64_t NamecoinEng::GetNetworkFee(int height) {			// Speed up network fee decrease 4x starting at 24000
	height += std::max(0, height - 24000) * 3;
	if ((height >> 13) >= 60)
		return 0;
	int64_t nStart = ChainParams.IsTestNet ? 10 * ChainParams.CoinValue / 100 : 50 * ChainParams.CoinValue;
	int64_t r = nStart >> (height >> 13);
	return r -= (r >> 14) * (height % 8192);
}

void NamecoinEng::OnConnectInputs(const Tx& tx, const vector<Tx>& vTxPrev, bool bBlock, bool bMiner)  {
	try {
		DBG_LOCAL_IGNORE_CONDITION(CoinErr::NAME_ExpirationError); //!!!?

		bool bFound = false;
		DecodedTx dtPrev;
		if (Mode != EngMode::Lite && Mode != EngMode::BlockParser) {
			for (int i = 0; i < tx.TxIns().size(); ++i) {
				const TxIn& txIn = tx.TxIns()[i];
				const TxOut& txOut = vTxPrev[i].TxOuts()[txIn.PrevOutPoint.Index];
				auto pp = DecodeNameScript(txOut.get_ScriptPubKey());
				if (pp.first) {
					bFound = true;
					dtPrev = pp.second;
					dtPrev.NOut = i;
				}
			}
		}
		if (tx->Ver != NAMECOIN_TX_VERSION) {
			if (bFound)
				Throw(CoinErr::NAME_NameCoinTransactionWithInvalidVersion);
			return;
		}
		DecodedTx dt = DecodeNameTx(tx);
		HashValue hashTx = Hash(tx);
		switch (dt.Op) {
		case OP_NAME_NEW:
			if (bFound)
				Throw(CoinErr::NAME_NewPointsPrevious);
			break;
		case OP_NAME_FIRSTUPDATE:
			if (dt.Args[0].size() == 0 || dt.Args[0].size() > KVStorage::MAX_KEY_SIZE)				//!!! DBLite supports key length 1..254
				return;
			{
				if (GetNameNetFee(tx) < GetNetworkFee(tx.Height))
					Throw(CoinErr::TxFeeIsLow);
				if (Mode != EngMode::Lite && Mode != EngMode::BlockParser) {
					if (!bFound || dtPrev.Op != OP_NAME_NEW)
						Throw(CoinErr::NAME_InvalidTx);
				}

				int heightExpired = tx.Height - GetExpirationDepth(tx.Height);
				int hPrev = NamecoinDb().GetNameHeight(dt.Args[0], heightExpired);
				if (hPrev>=0 && hPrev > heightExpired)
					Throw(CoinErr::NAME_ExpirationError);
				if (Mode != EngMode::Lite && Mode != EngMode::BlockParser) {
					int depth = GetRelativeDepth(tx, vTxPrev[dtPrev.NOut], MIN_FIRSTUPDATE_DEPTH);
					if (depth >= 0 && depth < MIN_FIRSTUPDATE_DEPTH)
						Throw(CoinErr::NAME_ExpirationError);
				}

				if (bBlock)
					NamecoinDb().PutDomainData(ToStringName(dt.Args[0]), tx.Height, hashTx, ToStringName(dt.Value), hPrev < 0);
			}
			break;

		case OP_NAME_UPDATE:
			if (dt.Args[0].size() == 0 || dt.Args[0].size() > KVStorage::MAX_KEY_SIZE)				//!!! DBLite supports key length 1..254
				return;
			{
				if (Mode!=EngMode::Lite && Mode!=EngMode::BlockParser) {
					if (!bFound || (dtPrev.Op != OP_NAME_FIRSTUPDATE && dtPrev.Op != OP_NAME_UPDATE))
						Throw(CoinErr::NAME_InvalidTx);
					int depth = GetRelativeDepth(tx, vTxPrev[dtPrev.NOut], GetExpirationDepth(tx.Height));
					if (depth < 0)
						Throw(CoinErr::NAME_ExpirationError);
				}

				if (bBlock)
					NamecoinDb().PutDomainData(ToStringName(dt.Args[0]), tx.Height, hashTx, ToStringName(dt.Value), false);
			}
			break;
		}
	} catch (Exception& ex) {
		if (ex.code() == CoinErr::NAME_ExpirationError) {
					//!!!?
		} else {
			//!!!			throw;
		}
	}
}

void NamecoinEng::OnConnectBlock(const Block& block) {
	int heightExpired = block.Height - GetExpirationDepth(block.Height) - 1000;
	if (heightExpired >= 0)
		NamecoinDb().OptionalDeleteExpiredDomains(heightExpired);
}

void NamecoinEng::OnDisconnectInputs(const Tx& tx) {
	if (tx->Ver != NAMECOIN_TX_VERSION)
		return;

	DecodedTx dt = DecodeNameTx(tx);
	/*!!!?
	switch (dt.Op) {
	case OP_NAME_FIRSTUPDATE:
		SqliteCommand("DELETE FROM domains WHERE name=?", m_db)
			.Bind(1, ToStringName(dt.Args[0]))
			.ExecuteNonQuery();
		break;
	case OP_NAME_UPDATE:
		if (!ResolverMode) {
			for (int i=0; i<tx.TxIns().size(); ++i) {
				const TxIn& txIn = tx.TxIns()[i];
				if (txIn.PrevOutPoint.TxHash) {
					Tx txPrev = Tx::FromDb(txIn.PrevOutPoint.TxHash);
					const TxOut& txOut = txPrev.TxOuts()[txIn.PrevOutPoint.Index];
					auto pp = DecodeNameScript(txOut.get_PkScript());
					if (pp.first) {
						SqliteCommand("UPDATE domains SET txid=? WHERE name=?", m_db)
							.Bind(1, ReducedHashValue(Hash(txPrev)))
							.Bind(2, ToStringName(dt.Args[0]))
							.ExecuteNonQuery();
						break;
					}
				}
			}
		}
		break;
	}
	*/
}


ptr<IBlockChainDb> NamecoinEng::CreateBlockChainDb() {
	return new NamecoinDbType;
}

INamecoinDb& NamecoinEng::NamecoinDb() {
	return dynamic_cast<NamecoinDbType&>(*Db);
}

static CurrencyFactory<NamecoinEng> s_namecoin("Namecoin");

} // Coin::
