/*######   Copyright (c) 2013-2015 Ufasoft  http://ufasoft.com  mailto:support@ufasoft.com,  Sergey Pavlov  mailto:dev@ufasoft.com ####
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
	:	IrcManager(_self)
	,	CmdAddNewKey("INSERT INTO privkeys (privkey, pubkey, timestamp, comment, reserved) VALUES(?, ?, ?, ?, ?)"										, m_dbWallet)
	,	m_cmdIsFromMe("SELECT coins.rowid FROM coins INNER JOIN mytxes ON txid=mytxes.id WHERE mytxes.netid=? AND mytxes.hash=? AND nout=?"				, m_dbWallet)
	,	CmdGetBalance("SELECT SUM(value) FROM coins INNER JOIN mytxes ON txid=mytxes.id WHERE mytxes.netid=? AND (blockord > -2 OR fromme)"	, m_dbWallet)
	,	CmdRecipients("SELECT hash160, comment FROM pubkeys WHERE netid=?"																				, m_dbWallet)
	,	CmdGetTxId("SELECT id FROM mytxes WHERE netid=? AND hash=?"																						, m_dbWallet)
	,	CmdGetTx("SELECT timestamp, data, blockord, comment, fromme FROM mytxes WHERE netid=? AND hash=?"												, m_dbWallet)
	,	CmdGetTxes("SELECT mytxes.timestamp, mytxes.data, mytxes.blockord, mytxes.comment, value, addr160 FROM usertxes INNER JOIN mytxes ON txid=mytxes.id WHERE mytxes.netid=? ORDER BY mytxes.timestamp DESC"	, m_dbWallet)
	,	CmdPendingTxes("SELECT timestamp, data, blockord, comment, fromme FROM mytxes WHERE (blockord BETWEEN -14 AND -1) AND netid=? ORDER BY timestamp"	, m_dbWallet)					//!!! timestamp < " << to_time_t(m_eng->.Chain.DtBestReceived) - 5*60 << " AND
	,	CmdSetBestBlockHash("UPDATE nets SET bestblockhash=? WHERE netid=?"																				, m_dbWallet)
	,	CmdFindCoin("SELECT rowid FROM coins WHERE txid=? AND nout=?"																					, m_dbWallet)
	,	CmdInsertCoin("INSERT INTO coins (txid, nout, value, keyid, pkscript) VALUES(?, ?, ?, ?, ?)"													, m_dbWallet)
	,	CmdPeerByIp("SELECT rowid FROM peers WHERE ip=?"																								, m_dbPeers)
	,	CmdInsertPeer("INSERT INTO peers (ip, lastlive, ban) VALUES(?, ?, ?)"																			, m_dbPeers)
	,	CmdUpdatePeer("UPDATE peers SET lastlive=?, ban=? WHERE rowid=?"																				, m_dbPeers)
	,	CmdEndpointByIp("SELECT rowid FROM endpoints WHERE netid=? AND ip=?"																			, m_dbPeers)
	,	CmdInsertEndpoint("INSERT INTO endpoints (netid, ip, port, services) VALUES(?, ?, ?, ?)"														, m_dbPeers)
	,	CmdUpdateEndpoint("UPDATE endpoints SET port=?, services=? WHERE rowid=?"																		, m_dbPeers)
{
	ListeningPort = UCFG_COIN_DEFAULT_PORT;
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

void CoinDb::InitAes(BuggyAes& aes, RCString password, byte encrtyptAlgo) {
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

MyKeyInfo& CoinDb::AddNewKey(MyKeyInfo& ki) {
	BuggyAes aes;
	if (!m_masterPassword.empty()) {
		if (m_cachedMasterKey.first.Size == 0) {
			InitAes(aes, m_masterPassword, DEFAULT_PASSWORD_ENCRYPT_METHOD);
			m_cachedMasterKey = make_pair(aes.Key, aes.IV);
		} else {
			aes.Key = m_cachedMasterKey.first;
			aes.IV = m_cachedMasterKey.second;
		}
	}
		
	CmdAddNewKey.Bind(1, !m_masterPassword.empty() ? ki.EncryptedPrivKey(aes) : ki.PlainPrivKey())
		.Bind(2, ToCompressedKey(ki.PubKey))
		.Bind(3, to_time_t(DateTime::UtcNow()))
		.Bind(4, ki.Comment)
		.Bind(5, ki.Comment == nullptr)
		.ExecuteNonQuery();

	ki.KeyRowid = m_dbWallet.LastInsertRowId;
	return Hash160ToMyKey.insert(make_pair(ki.Hash160, ki)).first->second;
}

MyKeyInfo *CoinDb::FindReservedKey() {
	EXT_LOCK (MtxDb) {
		SqliteCommand cmd("SELECT pubkey FROM privkeys WHERE reserved ORDER by id", m_dbWallet);
		DbDataReader dr = cmd.ExecuteReader();
		if (!dr.Read())
			return 0;
		return &Hash160ToMyKey[Hash160(ToUncompressedKey(dr.GetBytes(0)))];
	}
}

bool CoinDb::get_NeedPassword() {
	return m_masterPassword == nullptr;
}

void CoinDb::TopUpPool(int lim) {
	if (NeedPassword)
		Throw(HRESULT_FROM_WIN32(ERROR_PASSWORD_RESTRICTION));
	EXT_LOCK (MtxDb) {
		int nRes = (int)SqliteCommand("SELECT COUNT(*) FROM privkeys WHERE reserved", m_dbWallet).ExecuteInt64Scalar();
		for (int i=0; i<lim-nRes; ++i) {
			AddNewKey(CCrypter::GenRandomKey());
		}
	}
}

bool CoinDb::SetPrivkeyComment(const HashValue160& hash160, RCString comment) {
	EXT_LOCK (MtxDb) {
		CHash160ToMyKey::iterator it = Hash160ToMyKey.find(hash160);
		if (it != Hash160ToMyKey.end()) {
			SqliteCommand(EXT_STR("UPDATE privkeys SET comment=?, reserved=? WHERE id=" << it->second.KeyRowid), m_dbWallet)
				.Bind(1, it->second.Comment = comment)
				.Bind(2, comment == nullptr)
				.ExecuteNonQuery();
			return true;
		}
	}
	return false;
}

const MyKeyInfo& CoinDb::GenerateNewAddress(RCString comment, int lim) {
	EXT_LOCK (MtxDb) {
		MyKeyInfo *ki = FindReservedKey();
		if (!ki) {
			TopUpPool(lim);
			ki = FindReservedKey();
		}
		SetPrivkeyComment(ki->Hash160, comment);
		return *ki;
	}
}

void CoinDb::RemovePubKeyFromReserved(const Blob& pubKey) {
	EXT_LOCK (MtxDb) {
		SqliteCommand("UPDATE privkeys SET reserved=0 WHERE pubkey=?", m_dbWallet)
			.Bind(1, ToCompressedKey(pubKey))
			.ExecuteNonQuery();
	}
}

void CoinDb::RemovePubHash160FromReserved(const HashValue160& hash160) {
	EXT_LOCK (MtxDb) {
		ASSERT(Hash160ToMyKey.count(hash160));
		RemovePubKeyFromReserved(Hash160ToMyKey[hash160].PubKey);
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
			"CREATE TABLE globals (name PRIMARY KEY,"
								   "value);"
			"CREATE TABLE nets (netid INTEGER PRIMARY KEY AUTOINCREMENT,"
							   "name UNIQUE,"
							   "bestblockhash);"
			"CREATE TABLE privkeys ( id INTEGER PRIMARY KEY AUTOINCREMENT,"
								   	"privkey UNIQUE,"
									"pubkey,"
									"timestamp,"
									"comment DEFAULT NULL,"
									"reserved);"
			"CREATE TABLE pubkeys (  netid INTEGER REFERENCES nets(netid),"
									"hash160,"
									"comment,"
									"PRIMARY KEY (netid, hash160));"
			"CREATE TABLE mytxes ( id INTEGER PRIMARY KEY AUTOINCREMENT"
								", netid INTEGER REFERENCES nets(netid)"
								", hash"
								", data"
								", timestamp"
								", blockord"
								", comment"
								", fromme"
								", UNIQUE (netid, hash));"  		//  blockord=-15 means the Tx is Canceled
			"CREATE TABLE usertxes (   txid INTEGER REFERENCES mytxes(id) ON DELETE CASCADE"
									", nout"
									", value"
									", addr160);"
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

		GenerateNewAddress("", 2);	//!!! reserve more keys
	} catch (RCExc) {
		m_dbWallet.Close();
		sys::remove(DbWalletFilePath);
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
		sys::remove(DbPeersFilePath);
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

/*!!!R void CoinDb::OnIpDetected(const IPAddress& ip) {
	EXT_LOCK (MtxDb) {
		TRC(1, ip);
		LocalIp4 = ip;
	}
	AddLocal(ip);
}*/

bool CoinDb::get_WalletDatabaseExists() {
	return exists(DbWalletFilePath);
}

bool CoinDb::get_PeersDatabaseExists() {
	return exists(DbPeersFilePath);
}

void CoinDb::LoadKeys(RCString password) {
	BuggyAes aesA;
	BuggyAes aesB;
	if (!password.empty()) {
		InitAes(aesA, password, 'A');
		InitAes(aesB, password, 'B');
	}

	EXT_LOCK (MtxDb) {
		unordered_map<HashValue160, MyKeyInfo> keys;
		SqliteCommand cmd("SELECT id, privkey, pubkey, timestamp, comment FROM privkeys", m_dbWallet);
		for (DbDataReader dr=cmd.ExecuteReader(); dr.Read();) {
			MyKeyInfo ki;
			ki.KeyRowid = dr.GetInt32(0);
			ki.PubKey = ToUncompressedKey(dr.GetBytes(2));
			ki.Timestamp = DateTime::from_time_t(dr.GetInt64(3));
			ki.Comment = dr.IsDBNull(4) ? nullptr : dr.GetString(4);

			ConstBuf buf = dr.GetBytes(1);
			if (buf.Size < 2)
				Throw(E_FAIL);
			switch (buf.P[0]) {
			case 0:
				ki.SetPrivData(ConstBuf(buf.P+1, buf.Size-1), ki.PubKey.Size == 33);
				ki.Key = CngKey::Import(ki.get_PrivKey(), CngKeyBlobFormat::OSslEccPrivateBignum);
				break;
			case 'A':
			case 'B':
				if (password == "")
					Throw(HRESULT_FROM_WIN32(ERROR_LOGON_FAILURE));
				if (password == nullptr) {
					m_masterPassword = nullptr;
					m_cachedMasterKey = make_pair(Blob(), Blob());
					ki.Key = CngKey::Import(ki.PubKey, CngKeyBlobFormat::OSslEccPublicBlob);
				} else {
// try { //!!!T
					try {
						BuggyAes *pAes = 0;
						switch (buf.P[0]) {
						case 'A': pAes = &aesA; break;
						case 'B': pAes = &aesB; break;
						}						
						Blob ptext = pAes->Decrypt(ConstBuf(buf.P+1, buf.Size-1));
						if (ptext.Size < 5)
							Throw(CoinErr::InconsistentDatabase);
						ki.SetPrivData(Blob(ptext.constData(), ptext.Size-4), ki.PubKey.Size == 33);
						if (Crc32().ComputeHash(ki.get_PrivKey()) != Blob(ptext.constData()+ptext.Size-4, 4))
							Throw(HRESULT_FROM_WIN32(ERROR_LOGON_FAILURE));
					} catch (OpenSslException&) {
						Throw(HRESULT_FROM_WIN32(ERROR_LOGON_FAILURE));
					} catch (Exception& ex) {
						if (ex.code() == ExtErr::Crypto)
							Throw(HRESULT_FROM_WIN32(ERROR_LOGON_FAILURE));
						throw;
					}
// } catch (RCExc ex) {//!!!T
// 	TRC(1, ex.what());
// 	continue;
// }
// 	TRC(1, "Successfully loaded key");
					ki.Key = CngKey::Import(ki.get_PrivKey(), CngKeyBlobFormat::OSslEccPrivateBignum);
				}
				break;
			default:
				Throw(ExtErr::UnsupportedEncryptionAlgorithm);
			}
			//!!!R ki.Key = CngKey::Import(ki.PrivKey, ki.PrivKey.Size <= 33 ? CngKeyBlobFormat::OSslEccPrivateBignum : CngKeyBlobFormat::OSslEccPrivateBlob);

			keys.insert(make_pair(ki.Hash160, ki));
		}
#ifdef X_DEBUG//!!!T
		if (!password.IsEmpty()) {
			for (auto it=keys.begin(), e=keys.end(); it!=e; ++it) {
				MyKeyInfo& ki = it->second;
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
		Hash160ToMyKey = keys;
		Filter = new CoinFilter(Hash160ToMyKey.size()*2, 0.0000001, Ext::Random().Next(), CoinFilter::BLOOM_UPDATE_ALL);
		EXT_FOR (const CoinDb::CHash160ToMyKey::value_type& kv, Hash160ToMyKey) {
			Filter->Insert(kv.first);
			Filter->Insert(kv.second.PubKey);
		}
#ifdef X_DEBUG//!!!D
		EXT_FOR (const CoinDb::CHash160ToMyKey::value_type& kv, Hash160ToMyKey) {
			ASSERT(Filter->Contains(kv.first) && Filter->Contains(kv.second.PubKey));
		}
#endif
	}
	if (!NeedPassword)
		TopUpPool();
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
					CheckUserVersion(m_dbWallet);
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
			DateTime now = DateTime::UtcNow();
			SqliteCommand(EXT_STR("UPDATE peers SET ban=0 WHERE lastlive < " << to_time_t(now-TimeSpan::FromDays(1))), m_dbPeers).ExecuteNonQuery();

			SqliteCommand cmdBanned("SELECT ip FROM peers WHERE ban", m_dbPeers);
			for (DbDataReader rd=cmdBanned.ExecuteReader(); rd.Read();)
				BannedIPs.insert(IPAddress(rd.GetBytes(0)));
		}

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
}

void CoinDb::ChangePassword(RCString oldPassword, RCString newPassword) {
	CheckPasswordPolicy(newPassword);
	try {
		LoadKeys(oldPassword);	
	} catch (Exception& ex) {
		if (ex.code() == error_condition(HRESULT_FROM_WIN32(ERROR_LOGON_FAILURE), hresult_category()))
			Throw(HRESULT_FROM_WIN32(ERROR_WRONG_PASSWORD));
		throw;
	}

	BuggyAes aes;
	if (!newPassword.empty())
		InitAes(aes, newPassword, DEFAULT_PASSWORD_ENCRYPT_METHOD);

	EXT_LOCK (MtxDb) {
		SqliteCommand cmd("UPDATE privkeys SET privkey=? WHERE id=?", m_dbWallet);
		TransactionScope dbtx(m_dbWallet);
		for (CHash160ToMyKey::iterator it=Hash160ToMyKey.begin(), e=Hash160ToMyKey.end(); it!=e; ++it) {
			const MyKeyInfo& ki = it->second;
			cmd.Bind(1, !newPassword.empty() ? ki.EncryptedPrivKey(aes) : ki.PlainPrivKey())
				.Bind(2, ki.KeyRowid)
				.ExecuteNonQuery();
		}
		dbtx.Commit();
		m_masterPassword = newPassword;
		m_cachedMasterKey = make_pair(Blob(), Blob());
		
		m_dbWallet.DisposeCommandsWithoutUnregiter();
 		m_dbWallet.ExecuteNonQuery("VACUUM");		// to clean old unencrypted keys
	}
}

MyKeyInfo CoinDb::GetMyKeyInfo(const HashValue160& hash160) {
	MyKeyInfo r;
	if (!EXT_LOCKED(MtxDb, Lookup(Hash160ToMyKey, hash160, r)))
		Throw(E_FAIL);
	return r;
}

Link *CoinDb::CreateLink(thread_group& tr) {
	Link *r = new CoinLink(_self, tr);
	r->Tcp.ProxyString = ProxyString;
	return r;
}


} // Coin::

