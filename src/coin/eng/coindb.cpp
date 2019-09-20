/*######   Copyright (c) 2013-2019 Ufasoft  http://ufasoft.com  mailto:support@ufasoft.com,  Sergey Pavlov  mailto:dev@ufasoft.com ####
#                                                                                                                                     #
# 		See LICENSE for licensing information                                                                                         #
#####################################################################################################################################*/

#include <el/ext.h>

#include <el/crypto/hash.h>
#include <el/crypto/ext-openssl.h>

#include "coin-protocol.h"
#include "script.h"
#include "eng.h"
#include "crypter.h"


namespace Coin {

#ifdef _WIN32

#	if UCFG_USE_SQLITE==3
static SqliteMalloc s_sqliteMalloc;
static SqliteVfs s_sqliteVfs;			// enable atomic writes
#	endif

#endif

CoinDb::CoinDb()
	: IrcManager(_self)
	, CmdAddNewKey("INSERT INTO privkeys (privkey, type, pubkey, timestamp, comment, reserved) VALUES(?, ?, ?, ?, ?, ?)"	, m_dbWallet)
	, m_cmdIsFromMe("SELECT coins.rowid"
		" FROM coins INNER JOIN mytxes ON txid=mytxes.id"
		" WHERE mytxes.netid=? AND mytxes.hash=? AND nout=?"
		, m_dbWallet)
	, CmdGetBalance("SELECT SUM(value) FROM coins INNER JOIN mytxes ON txid=mytxes.id WHERE mytxes.netid=? AND (blockord > -2 OR fromme)"	, m_dbWallet)
	, CmdRecipients("SELECT type, hash160, comment FROM pubkeys WHERE netid=?"																				, m_dbWallet)
	, CmdGetTxId("SELECT id FROM mytxes WHERE netid=? AND hash=?"																						, m_dbWallet)
	, CmdGetTx("SELECT timestamp, data, blockord, comment, fromme FROM mytxes WHERE netid=? AND hash=?"												, m_dbWallet)
	, CmdGetTxes("SELECT mytxes.timestamp, mytxes.data, mytxes.blockord, mytxes.comment, value, nout"
		" FROM usertxes INNER JOIN mytxes ON txid=mytxes.id"
		" WHERE mytxes.netid=? ORDER BY mytxes.timestamp DESC"
		, m_dbWallet)
	, CmdPendingTxes("SELECT timestamp, data, blockord, comment, fromme"
		" FROM mytxes"
		" WHERE (blockord BETWEEN -14 AND -1) AND netid=? ORDER BY timestamp"
		, m_dbWallet)					//!!! timestamp < " << to_time_t(m_eng->.Chain.DtBestReceived) - 5*60 << " AND
	, CmdSetBestBlockHash("UPDATE nets SET bestblockhash=? WHERE netid=?"																				, m_dbWallet)
	, CmdFindCoin("SELECT rowid FROM coins WHERE txid=? AND nout=?"																					, m_dbWallet)
	, CmdInsertCoin("INSERT INTO coins (txid, nout, value, keyid, pkscript) VALUES(?, ?, ?, ?, ?)"													, m_dbWallet)
	, CmdPeerByIp("SELECT rowid FROM peers WHERE ip=?"																								, m_dbPeers)
	, CmdInsertPeer("INSERT INTO peers (ip, lastlive, ban) VALUES(?, ?, ?)"																			, m_dbPeers)
	, CmdUpdatePeer("UPDATE peers SET lastlive=?, ban=? WHERE rowid=?"																				, m_dbPeers)
	, CmdEndpointByIp("SELECT rowid FROM endpoints WHERE netid=? AND ip=?"																			, m_dbPeers)
	, CmdInsertEndpoint("INSERT INTO endpoints (netid, ip, port, services) VALUES(?, ?, ?, ?)"														, m_dbPeers)
	, CmdUpdateEndpoint("UPDATE endpoints SET port=?, services=? WHERE rowid=?"																		, m_dbPeers)
{
	ProxyString = g_conf.ProxyString;
	ListeningPort = g_conf.Port ? g_conf.Port : UCFG_COIN_DEFAULT_PORT;
	SoftPortRestriction = true;
	IrcManager.m_pTr.reset(&m_tr);

	DbWalletFilePath = AfxGetCApp()->get_AppDataDir() / "wallet.db";
	DbPeersFilePath = AfxGetCApp()->get_AppDataDir() / "peers.db";
	MsgLoopThread = new Coin::MsgLoopThread(m_tr, _self);

}

CoinDb::~CoinDb()  {
	m_tr.StopChilds();
}

void CoinDb::CreateWalletNetDbEntry(int netid, RCString name) {
	SqliteCommand("INSERT INTO nets (netid, name) VALUES(?, ?)", m_dbWallet)
		.Bind(1, netid)
		.Bind(2, name)
		.ExecuteNonQuery();
}

void CoinDb::InitAes(BuggyAes& aes, RCString password, uint8_t encrtyptAlgo) {
	int mul = 1000;
	switch (encrtyptAlgo) {
	case 'A': mul = PASSWORD_ENCRYPT_ROUNDS_A; break;
	case 'B': mul = PASSWORD_ENCRYPT_ROUNDS_B; break;
	default:
		Throw(E_NOTIMPL);
	}

	std::pair<Blob, Blob> pp = Aes::GetKeyAndIVFromPassword(password, Salt.data(), mul);
	aes.Key = pp.first;
	aes.IV = pp.second;
}

KeyInfo CoinDb::AddNewKey(KeyInfo ki) {
	BuggyAes aes;
	if (!m_masterPassword.empty()) {
		if (m_cachedMasterKey.first.size() == 0) {
			InitAes(aes, m_masterPassword, DEFAULT_PASSWORD_ENCRYPT_METHOD);
			m_cachedMasterKey = make_pair(aes.Key, aes.IV);
		} else {
			aes.Key = m_cachedMasterKey.first;
			aes.IV = m_cachedMasterKey.second;
		}
	}

	CmdAddNewKey
		.Bind(1, !m_masterPassword.empty() ? EncryptedPrivKey(aes, ki) : ki.PlainPrivKey)
		.Bind(2, (int)ki.AddressType)
		.Bind(3, ki.PubKey.ToCompressed())
		.Bind(4, to_time_t(ki->Timestamp))
		.Bind(5, ki->Comment)
		.Bind(6, ki->Reserved)
		.ExecuteNonQuery();

	ki->KeyRowId = m_dbWallet.LastInsertRowId;
	return Hash160ToKey.insert(make_pair(ki.PubKey.Hash160, ki)).first->second;
}

KeyInfo CoinDb::FindReservedKey(AddressType type) {
	EXT_LOCK (MtxDb) {
		SqliteCommand cmd("SELECT id, pubkey FROM privkeys WHERE reserved ORDER by id", m_dbWallet);
		DbDataReader dr = cmd.ExecuteReader();
		if (!dr.Read())
			return nullptr;
		auto id = dr.GetInt32(0);
		Blob pubkey = dr.GetBytes(1);
		SqliteCommand cmdUpdate("UPDATE privkeys SET type=? WHERE id=?", m_dbWallet);
		cmdUpdate
			.Bind(1, (int)type)
			.Bind(2, id)
			.ExecuteNonQuery();
		KeyInfo keyInfo = Hash160ToKey[Hash160(CanonicalPubKey::FromCompressed(pubkey).Data)];
		keyInfo.AddressType = type;
		return keyInfo;
	}
}

bool CoinDb::get_NeedPassword() {
	return m_masterPassword == nullptr;
}

void CoinDb::TopUpPool() {
	if (NeedPassword)
		Throw(HRESULT_FROM_WIN32(ERROR_PASSWORD_RESTRICTION));
	Rng rng;
	EXT_LOCK (MtxDb) {
		int nRes = (int)SqliteCommand("SELECT COUNT(*) FROM privkeys WHERE reserved", m_dbWallet).ExecuteInt64Scalar();
		for (int i = 0; i < g_conf.KeyPool - nRes; ++i) {
			KeyInfo keyInfo;
			keyInfo->GenRandom(rng);
			keyInfo->Reserved = true;
			AddNewKey(keyInfo);
		}
		UpdateFilter();
	}
}

// Contract: MtxDb is locked
KeyInfo CoinDb::FindPrivkey(const Address& addr) {
	Span data = addr.Data();
	switch (addr.Type) {
	case AddressType::P2PKH:
		if (auto o = Lookup(Hash160ToKey, (HashValue160)addr))
			return o.value();
		break;
	case AddressType::P2SH:
		if (auto o = Lookup(P2SHToKey, (HashValue160)addr))
			return o.value();
		break;
	case AddressType::Bech32:
		switch (data.size()) {
		case 20:
			if (auto o = Lookup(Hash160ToKey, (HashValue160)addr))
				return o.value();
			break;
		case 32:
			if (auto o = Lookup(P2SHToKey, Hash160(((HashValue)addr).ToSpan())))		//!!!?
				return o.value();
			break;
		default:
			Throw(E_NOTIMPL);
		}
		break;
	default:
		Throw(E_NOTIMPL);
	}
	return nullptr;
}

bool CoinDb::SetPrivkeyComment(const Address& addr, RCString comment) {
	EXT_LOCK(MtxDb) {
		if (KeyInfo key = FindPrivkey(addr)) {
			key->Comment = comment;
			SqliteCommand(EXT_STR("UPDATE privkeys SET comment=?, reserved=? WHERE id=" << key->KeyRowId), m_dbWallet)
				.Bind(1, comment)
				.Bind(2, comment == nullptr)
				.ExecuteNonQuery();
			return true;
		}
		return false;
	}
}

KeyInfo CoinDb::GenerateNewAddress(AddressType type, RCString comment) {
	EXT_LOCK (MtxDb) {
		KeyInfo ki = FindReservedKey(type);
		if (!ki) {
			TopUpPool();
			ki = FindReservedKey(type);
		}		
		SetPrivkeyComment(ki->ToAddress(), comment);
		return ki;
	}
}

void CoinDb::RemoveKeyInfoFromReserved(const KeyInfo& keyInfo) {
	EXT_LOCK (MtxDb) {
		SqliteCommand("UPDATE privkeys SET reserved=0 WHERE id=?", m_dbWallet)
			.Bind(1, keyInfo->KeyRowId)
			.ExecuteNonQuery();
	}
}

void CoinDb::CreateDbWallet() {
	if (NeedPassword)
		Throw(HRESULT_FROM_WIN32(ERROR_PASSWORD_RESTRICTION));

	create_directories(DbWalletFilePath.parent_path());
	m_dbWallet.Create(DbWalletFilePath);

	try {
		TransactionScope dbtx(m_dbWallet);

		m_dbWallet.ExecuteNonQuery("PRAGMA page_size=8192");
		m_dbWallet.ExecuteNonQuery(
			"CREATE TABLE globals (name PRIMARY KEY, value);"
			"CREATE TABLE nets (netid INTEGER PRIMARY KEY AUTOINCREMENT"
				", name UNIQUE"
				", bestblockhash);"
			"CREATE TABLE privkeys ( id INTEGER PRIMARY KEY AUTOINCREMENT"
				", privkey UNIQUE"
				", pubkey"
				", type"
				", timestamp"
				", comment DEFAULT NULL"
				", reserved);"
			"CREATE TABLE pubkeys (netid INTEGER REFERENCES nets(netid)"
				", hash160"
				", type"
				", comment"
				", PRIMARY KEY (netid, type, hash160));"
			"CREATE TABLE mytxes (id INTEGER PRIMARY KEY AUTOINCREMENT"
			   ", netid INTEGER REFERENCES nets(netid)"
				", hash"
				", data"
				", timestamp"
				", blockord"
				", comment"
				", fromme"
				", UNIQUE (netid, hash));" //  blockord=-15 means the Tx is Canceled
			"CREATE TABLE usertxes (txid INTEGER REFERENCES mytxes(id) ON DELETE CASCADE"
				", nout"
				", value);"
//!!!R Obsolete				", addr160);"
			"CREATE TABLE coins (txid INTEGER REFERENCES mytxes(id) ON DELETE CASCADE"
				", nout INTEGER NOT NULL CHECK (nout>=0)"
				", value INTEGER NOT NULL CHECK (value>0)"
				", keyid INTEGER REFERENCES privkeys(id)"
				", pkscript"
				", PRIMARY KEY (txid, nout))"
			);

		CreateWalletNetDbEntry(1, "Bitcoin");

		SetUserVersion(m_dbWallet);

		GetSystemURandomReader().BaseStream.ReadBuffer(Salt.data(), sizeof Salt);
		m_dbWallet.ExecuteNonQuery(EXT_STR("INSERT INTO globals (name, value) VALUES(\'salt\', " << *(int64_t*)Salt.data() << ")"));

		GenerateNewAddress(AddressType::P2SH, "");	//!!! reserve more keys
	} catch (RCExc) {
		m_dbWallet.Close();
		filesystem::remove(DbWalletFilePath);
		throw;
	}
}

void CoinDb::CreateDbPeers() {
	m_dbPeers.Create(DbPeersFilePath);
	try {
		TransactionScope dbtx(m_dbPeers);

		m_dbPeers.ExecuteNonQuery("PRAGMA page_size=8192");
		m_dbPeers.ExecuteNonQuery(
			"CREATE TABLE nets (id INTEGER PRIMARY KEY, name UNIQUE);"
			"CREATE TABLE peers (ip PRIMARY KEY, lastlive, ban);"
			"CREATE TABLE endpoints (netid INTEGER REFERENCES nets(id) ON DELETE CASCADE, ip REFERENCES peers(ip) ON DELETE CASCADE, port, services, PRIMARY KEY (netid, ip));"
			"CREATE INDEX endpoint_ip ON endpoints(ip)"
			);
		SetUserVersion(m_dbPeers);
	} catch (RCExc) {
		m_dbPeers.Close();
		filesystem::remove(DbPeersFilePath);
		throw;
	}
}

void CoinDb::UpdateOrInsertPeer(const Peer& peer) {
	Blob blobIp = peer.get_EndPoint().Address.GetAddressBytes();
	DbDataReader dr = CmdPeerByIp.Bind(1, blobIp).ExecuteReader();
	if (dr.Read()) {
		CmdUpdatePeer.Bind(1, to_time_t(peer.get_LastPersistent()))
			.Bind(2, peer.Banned)
			.Bind(3, dr.GetInt32(0))
			.ExecuteNonQuery();
	} else {
		CmdInsertPeer.Bind(1, blobIp)
			.Bind(2, to_time_t(peer.get_LastPersistent()))
			.Bind(3, peer.Banned)
			.ExecuteNonQuery();
	}
}

void CoinDb::BanPeer(Peer& peer) {
	base::BanPeer(peer);
	EXT_LOCKED(MtxDbPeers, UpdateOrInsertPeer(peer));
}

void CoinDb::SavePeers(int idPeersNet, const vector<ptr<Peer>>& peers) {
	EXT_LOCK (MtxDbPeers) {
		TransactionScope dbtx(m_dbPeers);

		for (int i=0; i<peers.size(); ++i) {
			Peer& peer = *peers[i];
			if (exchange(peer.IsDirty, false)) {
				UpdateOrInsertPeer(peer);

				Blob blobIp = peer.get_EndPoint().Address.GetAddressBytes();
				DbDataReader dr = CmdEndpointByIp.Bind(1, idPeersNet).Bind(2, blobIp).ExecuteReader();
				if (dr.Read()) {
					CmdUpdateEndpoint.Bind(1, peer.get_EndPoint().Port)
						.Bind(2, (int64_t)peer.Services)
						.Bind(3, dr.GetInt32(0))
						.ExecuteNonQuery();
				} else {
					CmdInsertEndpoint.Bind(1, idPeersNet)
						.Bind(2, blobIp)
						.Bind(3, peer.get_EndPoint().Port)
						.Bind(4, (int64_t)peer.Services)
						.ExecuteNonQuery();
				}
			}
		}
	}
}

bool CoinDb::get_WalletDatabaseExists() {
	return exists(DbWalletFilePath);
}

bool CoinDb::get_PeersDatabaseExists() {
	return exists(DbPeersFilePath);
}

void CoinDb::UpdateFilter() {
	Filter = new CoinFilter(Hash160ToKey.size() * 2, 0.0000001, Ext::Random().Next(), CoinFilter::BLOOM_UPDATE_ALL);
	DtEarliestKey = Clock::now();
	for (auto& kv : Hash160ToKey) {
		Filter->Insert(kv.first);
		Filter->Insert(kv.second.PubKey.Data);
		DtEarliestKey = (min)(DtEarliestKey, kv.second->Timestamp);
	}
	for (auto& kv : P2SHToKey)
		Filter->Insert(kv.first);
	ASSERT(IsFilterValid(Filter));
}

void CoinDb::LoadKeys(RCString password) {
	// Eng() is not defined here
	BuggyAes aesA;
	BuggyAes aesB;
	if (!password.empty()) {
		InitAes(aesA, password, 'A');
		InitAes(aesB, password, 'B');
	}

	EXT_LOCK (MtxDb) {
		unordered_map<HashValue160, KeyInfo> keys;
		CP2SHToKey redeemScripts;
		SqliteCommand cmd("SELECT id, privkey, pubkey, type, timestamp, comment FROM privkeys", m_dbWallet);
		for (DbDataReader dr = cmd.ExecuteReader(); dr.Read();) {
			KeyInfo ki;
			ki->KeyRowId = dr.GetInt32(0);
			ki.PubKey = CanonicalPubKey::FromCompressed(dr.GetBytes(2));
			ki.AddressType = (AddressType)dr.GetInt64(3);
			ki->Timestamp = DateTime::from_time_t(dr.GetInt64(4));
			ki->Comment = dr.IsDBNull(4) ? nullptr : dr.GetString(5);

			Span buf = dr.GetBytes(1);
			if (buf.size() < 2)
				Throw(E_FAIL);
			switch (buf[0]) {
			case 0: ki->SetPrivData(PrivateKey(buf.subspan(1), ki.PubKey.IsCompressed()));
				break;
			case 'A':
			case 'B':
				if (password == "")
					Throw(ExtErr::LogonFailure);
				if (password == nullptr) {
					m_masterPassword = nullptr;
					m_cachedMasterKey = make_pair(Blob(), Blob());
					ki->SetKeyFromPubKey();
				} else {
// try { //!!!T
					try {
						BuggyAes *pAes = 0;
						switch (buf[0]) {
						case 'A': pAes = &aesA; break;
						case 'B': pAes = &aesB; break;
						}
						Blob ptext = pAes->Decrypt(buf.subspan(1));
						if (ptext.size() < 5)
							Throw(CoinErr::InconsistentDatabase);
						ki->SetPrivData(PrivateKey(Span(ptext.constData(), ptext.size() - 4), ki.PubKey.IsCompressed()));
						if (!Equal(Crc32().ComputeHash(ki->get_PrivKey()), Blob(ptext.constData() + ptext.size() - 4, 4)))
							Throw(ExtErr::LogonFailure);
					} catch (OpenSslException&) {
						Throw(ExtErr::LogonFailure);
					} catch (Exception& ex) {
						if (ex.code() == ExtErr::Crypto)
							Throw(ExtErr::LogonFailure);
						throw;
					}
// } catch (RCExc ex) {//!!!T
// 	TRC(1, ex.what());
// 	continue;
// }
// 	TRC(1, "Successfully loaded key");
				}
				break;
			default:
				Throw(ExtErr::UnsupportedEncryptionAlgorithm);
			}

			keys.insert(make_pair(ki.PubKey.Hash160, ki));

			if (ki.PubKey.IsCompressed())
				redeemScripts.insert(make_pair(ki.PubKey.ScriptHash, ki));
		}
#ifdef X_DEBUG//!!!T
		if (!password.IsEmpty()) {
			for (auto it = keys.begin(), e = keys.end(); it != e; ++it) {
				KeyInfo& ki = it->second;
				if (ki.PrivKey.Size > 33) {
					ki.PrivKey = ki.Key.Export(CngKeyBlobFormat::OSslEccPrivateBignum);
					SqliteCommand("UPDATE privkeys SET privkey=? WHERE rowid=?", m_dbWallet)
						.Bind(1, ki.EncryptedPrivKey(aes))
						.Bind(2, ki.KeyRowid)
						.ExecuteNonQuery();
				}
			}
		}
#endif
		Hash160ToKey = keys;
		P2SHToKey = redeemScripts;

#ifdef _DEBUG//!!!D
		int _count = 0;
		for (auto& kv : P2SHToKey) {
			auto typ = kv.second.AddressType;
			if (typ != AddressType::P2PKH) {
				++_count;
			}
		}
#endif
		UpdateFilter();

#ifdef X_DEBUG//!!!D
		EXT_FOR (const CoinDb::CHash160MyKey::value_type& kv, Hash160ToKey) {
			ASSERT(Filter->Contains(kv.first) && Filter->Contains(kv.second.PubKey.Data));
		}
#endif
	}
	if (!NeedPassword)
		TopUpPool();
}

bool CoinDb::IsFilterValid(CoinFilter* filter) {
	if (!filter)
		return false;
	EXT_LOCK(MtxDb) {
		for (auto& kv : Hash160ToKey)
			if (!filter->Contains(kv.first))
				return false;
		for (auto& kv : P2SHToKey)
			if (!filter->Contains(kv.first))
				return false;
	}
	return true;
}

KeyInfo CoinDb::FindMine(RCSpan scriptPubKey, ScriptContext context) {
	Address parsed = TxOut::CheckStandardType(scriptPubKey);
	HashValue160 hash160;
	switch (parsed.Type) {
	case AddressType::P2SH:
	case AddressType::P2PKH:
	case AddressType::WitnessV0KeyHash:
		hash160 = HashValue160(parsed);	// prepare
	}

	Blob script(nullptr);
	switch (parsed.Type) {
	case AddressType::PubKey:
		hash160 = Hash160(parsed.Data());
		EXT_LOCK(MtxDb) {
			CoinDb::CHash160ToKey::iterator it = Hash160ToKey.find(hash160);
			if (it != Hash160ToKey.end())
				return it->second;
		}
		break;
	case AddressType::WitnessV0KeyHash:
		if (context == ScriptContext::WitnessV0)
			return nullptr;
		EXT_LOCK(MtxDb) {
			if (auto o = Lookup(Hash160ToKey, hash160))
				return o.value();
		}
		break;
	case AddressType::WitnessV0ScriptHash:
		if (context == ScriptContext::WitnessV0)
			return nullptr;
		{
			MemoryStream ms;
			ScriptWriter(ms) << Opcode::OP_RETURN << parsed.Data();
			hash160 = Hash160(ms);
			EXT_LOCK(MtxDb) {
				CoinDb::CHash160ToKey::iterator it = P2SHToKey.find(hash160);
				if (it == P2SHToKey.end())
					return nullptr;
			}
			hashval hv = RIPEMD160().ComputeHash(parsed.Data());
			parsed->Data = AddressObj::CData(hv.data(), hv.size());
		}
		break;
	case AddressType::P2SH:
		if (context != ScriptContext::Top)
			return nullptr;
		EXT_LOCK(MtxDb) {
			CoinDb::CHash160ToKey::iterator it = P2SHToKey.find(hash160);
			if (it == P2SHToKey.end())
				break;
			hash160 = it->second.PubKey.Hash160;
		}
	case AddressType::P2PKH:
		EXT_LOCK(MtxDb) {
			if (auto o = Lookup(Hash160ToKey, hash160))
				return o.value();
		}
		break;
	case AddressType::MultiSig:
		if (context == ScriptContext::Top)
			return nullptr;
		if (context != ScriptContext::P2SH)
			for (auto& key : parsed->Datas)
				if (key.size() != 33)
					return 0;
		KeyInfo ki(nullptr);	//!!!TODO we return the last key, mut must return all set or bool value.
		hash160 = Hash160(parsed.Data());
		EXT_LOCK(MtxDb) {
			for (auto& key : parsed->Datas) {
				CoinDb::CHash160ToKey::iterator it = Hash160ToKey.find(Hash160(key));
				if (it == Hash160ToKey.end())
					return nullptr;
				ki = it->second;
			}
		}
		return ki;
	}
	return nullptr;
}

void CoinDb::Load() {
	if (!m_bLoaded) {
		m_bLoaded = true;
//!!!R		DetectGlobalIp::Start(&m_tr);

		if (!DbWalletFilePath.empty()) {
			TRC(1, "DbWallet File: " << DbWalletFilePath);

			bool bOpened = false;
			if (WalletDatabaseExists) {
				m_dbWallet.Open(DbWalletFilePath, FileAccess::ReadWrite, FileShare::None);
				try {
					SqliteCommand("SELECT * FROM nets", m_dbWallet).ExecuteReader();		// failed if there are no tables in DB
					bOpened = true;
				} catch (RCExc) {
				}
				if (bOpened) {
					if (CheckUserVersion(m_dbWallet) < VER_SEGWIT) {
						m_dbWallet.ExecuteNonQuery("ALTER TABLE pubkeys ADD COLUMN type");
						m_dbWallet.ExecuteNonQuery("ALTER TABLE privkeys ADD COLUMN type");
						//!!!?						SetUserVersion(m_dbWallet, VER_COINS_SPENT_COLUMN);

					}
//!!!?						m_dbWallet.ExecuteNonQuery("ALTER TABLE coins ADD COLUMN spent");
//!!!?						SetUserVersion(m_dbWallet, VER_COINS_SPENT_COLUMN);
				}
			}
			if (!bOpened)
				CreateDbWallet();
			m_dbWallet.ExecuteNonQuery("PRAGMA locking_mode=EXCLUSIVE");

			LoadKeys();
			int64_t salt = SqliteCommand("SELECT value FROM globals WHERE name=\'salt\'", m_dbWallet).ExecuteInt64Scalar();
			memcpy(&Salt[0], &salt, sizeof Salt);
		}

		if (!DbPeersFilePath.empty()) {
			if (PeersDatabaseExists) {
				m_dbPeers.Open(DbPeersFilePath, FileAccess::ReadWrite, FileShare::None);
				if (CheckUserVersion(m_dbPeers) < VER_INDEXED_PEERS) {
					m_dbPeers.ExecuteNonQuery("CREATE INDEX endpoint_ip ON endpoints(ip)");
					SetUserVersion(m_dbPeers, VER_INDEXED_PEERS);
				}

				m_dbPeers.ExecuteNonQuery(EXT_STR("DELETE FROM peers WHERE rowid NOT IN "
													"(SELECT rowid FROM peers ORDER BY lastlive DESC LIMIT " << P2P::MAX_SAVED_PEERS << ")"));
			} else
				CreateDbPeers();
			DateTime now = Clock::now();
			SqliteCommand(EXT_STR("UPDATE peers SET ban=0 WHERE lastlive < " << to_time_t(now-TimeSpan::FromDays(1))), m_dbPeers).ExecuteNonQuery();

			SqliteCommand cmdBanned("SELECT ip FROM peers WHERE ban", m_dbPeers);
			for (DbDataReader rd=cmdBanned.ExecuteReader(); rd.Read();)
				BannedIPs.insert(IPAddress(rd.GetBytes(0)));
		}
	}
}

void CoinDb::Start() {
	if (!m_bStarted) {
		m_bStarted = true;

		if (ListeningPort != -1 && ProxyString.empty())
			P2P::ListeningThread::StartListeners(_self, m_tr);
		MsgLoopThread->Start();
	}
}

void CoinDb::Close() {
	if (m_bLoaded) {
		m_bLoaded = false;

		m_cmdIsFromMe.Dispose();
		m_dbWallet.Close();
		m_dbPeers.Close();
	}
}

void CoinDb::CheckPasswordPolicy(RCString password) {
//	if (password.Length < 1)
//		Throw(HRESULT_FROM_WIN32(ERROR_PASSWORD_RESTRICTION));
}

void CoinDb::put_Password(RCString password) {
	CheckPasswordPolicy(password);
	if (m_bLoaded) {
		LoadKeys(password);
	}
	m_masterPassword = password;
	m_cachedMasterKey = make_pair(Blob(), Blob());
	Load();
	TopUpPool();
	Start();
}

void CoinDb::ChangePassword(RCString oldPassword, RCString newPassword) {
	CheckPasswordPolicy(newPassword);
	try {
		LoadKeys(oldPassword);
	} catch (Exception& ex) {
		if (ex.code() == ExtErr::LogonFailure)
			Throw(ExtErr::InvalidPassword);
		throw;
	}

	BuggyAes aes;
	if (!newPassword.empty())
		InitAes(aes, newPassword, DEFAULT_PASSWORD_ENCRYPT_METHOD);

	EXT_LOCK (MtxDb) {
		SqliteCommand cmd("UPDATE privkeys SET privkey=? WHERE id=?", m_dbWallet);
		TransactionScope dbtx(m_dbWallet);
		for (CHash160ToKey::iterator it = Hash160ToKey.begin(), e = Hash160ToKey.end(); it != e; ++it) {
			const KeyInfo& ki = it->second;
			cmd.Bind(1, !newPassword.empty() ? EncryptedPrivKey(aes, ki) : ki.PlainPrivKey)
				.Bind(2, ki->KeyRowId)
				.ExecuteNonQuery();
		}
		dbtx.Commit();
		m_masterPassword = newPassword;
		m_cachedMasterKey = make_pair(Blob(), Blob());

		m_dbWallet.DisposeCommandsWithoutUnregiter();
 		m_dbWallet.ExecuteNonQuery("VACUUM");		// to clean old unencrypted keys
	}
}

KeyInfo CoinDb::GetMyKeyInfo(const HashValue160& hash160) {
	return EXT_LOCKED(MtxDb, Hash160ToKey.at(hash160));
}

KeyInfo CoinDb::GetMyKeyInfoByScriptHash(const HashValue160& hash160) {
	return EXT_LOCKED(MtxDb, P2SHToKey.at(hash160));
}

Blob CoinDb::GetMyRedeemScript(const HashValue160& hash160) {
	return GetMyKeyInfoByScriptHash(hash160)->ToAddress()->ToScriptPubKey();
}

P2P::Link *CoinDb::CreateLink(thread_group& tr) {
	Link *r = new Coin::Link(_self, tr);
	r->Tcp.ProxyString = ProxyString;
	return r;
}


} // Coin::
