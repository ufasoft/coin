#include <el/ext.h>

#include <el/bignum.h>
#include <el/crypto/hash.h>


//#include "coineng.h"
#include "coin-protocol.h"
#include "script.h"
#include "eng.h"
#include "coin-msg.h"
#include "namecoin.h"

namespace Coin {

const int NAMECOIN_TX_VERSION = 0x7100;

const int MAX_NAME_LENGTH = 255,
		 MAX_VALUE_LENGTH = 1023;

const int MIN_FIRSTUPDATE_DEPTH = 12;


String ToStringName(const ConstBuf& cbuf) {
	try {
		DBG_LOCAL_IGNORE_NAME(E_EXT_InvalidUTF8String, ignE_EXT_InvalidUTF8String);

		return Encoding::UTF8.GetChars(cbuf);
	} catch (RCExc) {
		return Encoding::GetEncoding(1252)->GetChars(cbuf);
	}
}

pair<bool, DecodedTx> DecodeNameScript(const ConstBuf& cbufScript) {
	pair<bool, DecodedTx> r;

	Script script(cbufScript);
	if (script.size() < 1)
		return r;
	int op = script[0].Opcode;
	if (op != OP_PUSHDATA1)
		return r;
	BigInteger n = ToBigInteger(script[0].Value);
	if (n < 1 || n > 16)
		return r;
	r.second.Op = (NameOpcode)(int)explicit_cast<int>(n);
	Script::iterator it = script.begin()+1;
	for (; it!=script.end(); ++it) {
		switch (op = it->Opcode) {
		case OP_DROP:
		case OP_2DROP:
		case OP_NOP:
			{
				++it;
				while (it != script.end() && (op = it->Opcode) != OP_DROP && op != OP_2DROP && op != OP_NOP)
					++it;
				r.first = r.second.Op == OP_NAME_NEW && r.second.Args.size() == 1 ||
						  r.second.Op == OP_NAME_FIRSTUPDATE && r.second.Args.size() == 3 ||
						  r.second.Op == OP_NAME_UPDATE && r.second.Args.size() == 2;
				goto LAB_RET;
			}
		default:
			if (op < 0 || op > OP_PUSHDATA4)
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
		pair<bool, DecodedTx> pp = DecodeNameScript(tx.TxOuts()[i].get_PkScript());
		if (pp.first) {
			if (exchange(bFound, true))
				Throw(E_COIN_NAME_InvalidTx);
			r = pp.second;
			r.NOut = i;
		}
	}
	if (!bFound)
		Throw(E_COIN_NAME_InvalidTx);
	return r;
}

void NamecoinEng::CreateAdditionalTables() {
#if UCFG_COIN_COINCHAIN_BACKEND != COIN_BACKEND_SQLITE
	String dbPath = GetDomainsDbPath();
	File::Delete(dbPath);
	m_db.Create(dbPath);
#endif
	if (ResolverMode)
		m_db.ExecuteNonQuery("CREATE TABLE domains (name PRIMARY KEY"
												", address"
												", height)");
	else
		m_db.ExecuteNonQuery("CREATE TABLE domains (txid INTEGER PRIMARY KEY"
												", name UNIQUE)");
}

bool NamecoinEng::OpenDb() {
	bool r = base::OpenDb();
	String dbPath = GetDomainsDbPath();
	if (File::Exists(dbPath))
		m_db.Open(dbPath, FileAccess::ReadWrite, FileShare::None);
	else
		CreateAdditionalTables();
	return r;
}

static class NamecoinChainParams : public ChainParams {
	typedef ChainParams base;
public:
	NamecoinChainParams(bool)
		:	base("Namecoin"			, false)
	{	
		ChainParams::Add(_self);
	}

protected:
	NamecoinEng *CreateObject(CoinDb& cdb, IBlockChainDb *db) override {
		NamecoinEng *r = new NamecoinEng(cdb, db);
		r->SetChainParams(_self, db);
		return r;
	}
} s_namecoinParams(true);

void NamecoinEng::OnCheck(const Tx& tx) {
	if (tx.m_pimpl->Ver != NAMECOIN_TX_VERSION)
		return;
	DecodedTx dt = DecodeNameTx(tx);
	if (dt.Args[0].Size > MAX_NAME_LENGTH)
		Throw(E_COIN_NAME_ToolLongName);
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
	Throw(E_COIN_NAME_InvalidTx);
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

static Int64 GetNameNetFee(const Tx& tx) {
	Int64 r = 0;
	EXT_FOR (const TxOut& txOut, tx.TxOuts()) {
		if (txOut.get_PkScript().Size == 1 && txOut.get_PkScript()[0] == OP_RETURN)
			r += txOut.Value;
	}
	return r;
}

Int64 NamecoinEng::GetNetworkFee(int height) {			// Speed up network fee decrease 4x starting at 24000    
	height += std::max(0, height - 24000) * 3;
	if ((height >> 13) >= 60)
		return 0;
	Int64 nStart = ChainParams.IsTestNet ? 10 * ChainParams.CoinValue / 100 : 50 * ChainParams.CoinValue;
	Int64 r = nStart >> (height >> 13);
	return r -= (r >> 14) * (height % 8192);
}

/*!!!R
bool NamecoinEng::ShouldBeSaved(const Tx& tx) {
	if (tx.Ver == NAMECOIN_TX_VERSION) {
		DecodedTx dt = DecodeNameTx(tx);
		switch (dt.Op) {
		case OP_NAME_FIRSTUPDATE:
		case OP_NAME_UPDATE:
			return true;
		}
	}
	return base::ShouldBeSaved(tx);
} */

void NamecoinEng::OnConnectInputs(const Tx& tx, const vector<Tx>& vTxPrev, bool bBlock, bool bMiner)  {
	try {
		DBG_LOCAL_IGNORE_NAME(E_COIN_NAME_ExpirationError, ignE_COIN_NAME_ExpirationError); //!!!?

		bool bFound = false;
		DecodedTx dtPrev;
		if (!LiteMode) {
			for (int i=0; i<tx.TxIns().size(); ++i) {
				const TxIn& txIn = tx.TxIns()[i];
				const TxOut& txOut = vTxPrev[i].TxOuts()[txIn.PrevOutPoint.Index];
				auto pp = DecodeNameScript(txOut.get_PkScript());
				if (pp.first) {
					bFound = true;
					dtPrev = pp.second;
					dtPrev.NOut = i;
				}
			}
		}
		if (tx.m_pimpl->Ver != NAMECOIN_TX_VERSION) {
			if (bFound)
				Throw(E_COIN_NAME_NameCoinTransactionWithInvalidVersion);
			return;
		}
		DecodedTx dt = DecodeNameTx(tx);
		HashValue hashTx = Hash(tx);
		switch (dt.Op) {
		case OP_NAME_NEW:
			if (bFound)
				Throw(E_COIN_NAME_NewPointsPrevious);
			break;
		case OP_NAME_FIRSTUPDATE:
			{
				if (GetNameNetFee(tx) < GetNetworkFee(tx.Height))
					Throw(E_COIN_TxFeeIsLow);
				if (!LiteMode) {
					if (!bFound || dtPrev.Op != OP_NAME_NEW)
						Throw(E_COIN_NAME_InvalidTx);
				}

				int hPrev = GetNameHeight(dt.Args[0]);
				if (hPrev>=0 && (tx.Height-hPrev)<GetExpirationDepth(tx.Height))
					Throw(E_COIN_NAME_ExpirationError);
				if (!LiteMode) {
					int depth = GetRelativeDepth(tx, vTxPrev[dtPrev.NOut], MIN_FIRSTUPDATE_DEPTH);
					if (depth >= 0 && depth < MIN_FIRSTUPDATE_DEPTH)
						Throw(E_COIN_NAME_ExpirationError);
				}

				if (bBlock) {
					String domain = ToStringName(dt.Args[0]);
					if (ResolverMode) {
						if (hPrev < 0) {
							m_cmdInsertDomain.Bind(1, domain)
											.Bind(2, ToStringName(dt.Value))
											.Bind(3, tx.Height)
											.ExecuteNonQuery();
						} else {
							m_cmdUpdateDomain.Bind(3, domain)
											.Bind(1, ToStringName(dt.Value))
											.Bind(2, tx.Height)
											.ExecuteNonQuery();
						}
					} else {
						if (hPrev < 0) {
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
			}
			break;

		case OP_NAME_UPDATE:
			{
				if (!LiteMode) {
					if (!bFound || (dtPrev.Op != OP_NAME_FIRSTUPDATE && dtPrev.Op != OP_NAME_UPDATE))
						Throw(E_COIN_NAME_InvalidTx);
					int depth = GetRelativeDepth(tx, vTxPrev[dtPrev.NOut], GetExpirationDepth(tx.Height));
					if (depth < 0)
						Throw(E_COIN_NAME_ExpirationError);
				}

				if (bBlock) {
					String domain = ToStringName(dt.Args[0]);
					if (ResolverMode) {
						m_cmdUpdateDomain.Bind(3, domain)
										.Bind(1, ToStringName(dt.Value))
										.Bind(2, tx.Height)
										.ExecuteNonQuery();
					} else {
						m_cmdUpdateDomain.Bind(1, ReducedHashValue(hashTx))
										 .Bind(2, domain)
										.ExecuteNonQuery();
					}
				}
			}
			break;
		}
	} catch (RCExc ex) {
		switch (HResultInCatch(ex)) {
		case E_COIN_NAME_ExpirationError:			//!!!?
			break;
		default:
			;
//!!!			throw;
		}
	}
}

void NamecoinEng::OnConnectBlock(const Block& block) {
	int heightExpired = block.Height-GetExpirationDepth(block.Height)-1000;
	if (heightExpired >= 0) {
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
}

void NamecoinEng::OnDisconnectInputs(const Tx& tx) {
	if (tx.m_pimpl->Ver != NAMECOIN_TX_VERSION)
		return;

	DecodedTx dt = DecodeNameTx(tx);
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
				if (txIn.PrevOutPoint.TxHash != HashValue()) {
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

}

pair<bool, String> NamecoinEng::FindName(RCString name) {
	if (!ResolverMode)
		Throw(E_NOTIMPL);
	pair<bool, String> r;
	EXT_LOCK (Mtx) {
		DbDataReader dr = m_cmdFindDomain.Bind(1, name).ExecuteReader();
		if (r.first = dr.Read())
			r.second = dr.GetString(0);
	}
	return r;
}



} // Coin::

