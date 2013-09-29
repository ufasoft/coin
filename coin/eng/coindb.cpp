/*######     Copyright (c) 1997-2013 Ufasoft  http://ufasoft.com  mailto:support@ufasoft.com,  Sergey Pavlov  mailto:dev@ufasoft.com #######################################
#                                                                                                                                                                          #
# This program is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation;  #
# either version 3, or (at your option) any later version. This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the      #
# implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details. You should have received a copy of the GNU #
# General Public License along with this program; If not, see <http://www.gnu.org/licenses/>                                                                               #
##########################################################################################################################################################################*/

#include <el/ext.h>

#include <el/crypto/hash.h>
#include <el/crypto/ext-openssl.h>


//#include "coineng.h"
#include "coin-protocol.h"
#include "script.h"
#include "eng.h"
#include "coin-msg.h"
#include "crypter.h"


namespace Coin {

#ifdef _WIN32

#	if UCFG_USE_SQLITE==3
static SqliteMalloc s_sqliteMalloc;
static SqliteVfs s_sqliteVfs;			// enable atomic writes
#	endif

//static MMappedSqliteVfs s_sqliteMMappedVfs;	//!!!
#endif

CoinDb::CoinDb()
	:	IrcManager(_self)
	,	CmdAddNewKey("INSERT INTO privkeys (privkey, pubkey, timestamp, comment, reserved) VALUES(?, ?, ?, ?, ?)"										, m_dbWallet)
	,	m_cmdIsFromMe("SELECT coins.rowid FROM coins INNER JOIN mytxes ON txid=mytxes.id WHERE mytxes.netid=? AND mytxes.hash=? AND nout=?"				, m_dbWallet)
	,	CmdGetBalance("SELECT SUM(value) FROM coins INNER JOIN mytxes ON txid=mytxes.id WHERE mytxes.netid=? AND (blockord > -2 OR fromme)"	, m_dbWallet)
	,	CmdRecipients("SELECT hash160, comment FROM pubkeys WHERE netid=?"																				, m_dbWallet)
	,	CmdGetTxId("SELECT id FROM mytxes WHERE netid=? AND hash=?"																						, m_dbWallet)
	,	CmdPendingTxes("SELECT timestamp, data, blockord, comment, fromme FROM mytxes WHERE blockord<0 AND blockord>-15 AND netid=? ORDER BY timestamp"	, m_dbWallet)					//!!! timestamp < " << m_eng->.Chain.DtBestReceived.UnixEpoch-5*60 << " AND
	,	CmdSetBestBlockHash("UPDATE nets SET bestblockhash=? WHERE netid=?"																				, m_dbWallet)
	,	CmdFindCoin("SELECT rowid FROM coins WHERE txid=? AND nout=?"																					, m_dbWallet)
	,	CmdInsertCoin("INSERT INTO coins (txid, nout, value, keyid, pkscript) VALUES(?, ?, ?, ?, ?)"													, m_dbWallet)
	,	m_cmdIsBanned("SELECT ban FROM peers WHERE ip=?"																								, m_dbPeers)
	,	CmdPeerByIp("SELECT rowid FROM peers WHERE ip=?"																								, m_dbPeers)
	,	CmdInsertPeer("INSERT INTO peers (ip, lastlive, ban) VALUES(?, ?, ?)"																			, m_dbPeers)
	,	CmdUpdatePeer("UPDATE peers SET lastlive=?, ban=? WHERE rowid=?"																				, m_dbPeers)
	,	CmdEndpointByIp("SELECT rowid FROM endpoints WHERE netid=? AND ip=?"																			, m_dbPeers)
	,	CmdInsertEndpoint("INSERT INTO endpoints (netid, ip, port, services) VALUES(?, ?, ?, ?)"														, m_dbPeers)
	,	CmdUpdateEndpoint("UPDATE endpoints SET port=?, services=? WHERE rowid=?"																		, m_dbPeers)
{
	ListeningPort = 8333;
	IrcManager.m_pTr = &m_tr;

	DbWalletFilePath = Path::Combine(AfxGetCApp()->get_AppDataDir(), "wallet.db");
	DbPeersFilePath = Path::Combine(AfxGetCApp()->get_AppDataDir(), "peers.db");
	MsgLoopThread = new Coin::MsgLoopThread(m_tr, _self);
}

CoinDb::~CoinDb()  {
#if UCFG_COIN_GENERATE
	if (Miner.get())
		Miner.get()->Stop();
#endif
	m_tr.StopChilds();
}

void CoinDb::CreateWalletNetDbEntry(int netid, RCString name) {
	SqliteCommand("INSERT INTO nets (netid, name) VALUES(?, ?)", m_dbWallet)
		.Bind(1, netid)
		.Bind(2, name)
		.ExecuteNonQuery();
}

MyKeyInfo& CoinDb::AddNewKey(MyKeyInfo& ki) {
	Aes aes;
	if (!m_masterPassword.IsEmpty())
		InitAes(aes, m_masterPassword);
		
	CmdAddNewKey.Bind(1, !m_masterPassword.IsEmpty() ? ki.EncryptedPrivKey(aes) : ki.PlainPrivKey())
		.Bind(2, ToCompressedKey(ki.PubKey))
		.Bind(3, DateTime::UtcNow().UnixEpoch)
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

void CoinDb::TopUpPool(int lim) {
	if (NeedPassword)
		Throw(HRESULT_FROM_WIN32(ERROR_PASSWORD_RESTRICTION));
	EXT_LOCK (MtxDb) {
		int nRes = (int)SqliteCommand("SELECT COUNT(*) FROM privkeys WHERE reserved", m_dbWallet).ExecuteInt64Scalar();
		for (int i=0; i<lim-nRes; ++i) {
			ECDsa dsa(256);
			MyKeyInfo ki;
			ki.Comment = nullptr;
			ki.Key = dsa.Key;
			Blob privData = ki.Key.Export(CngKeyBlobFormat::OSslEccPrivateBignum);
			ki.SetPrivData(privData, true);
			ki.PubKey = ki.Key.Export(CngKeyBlobFormat::OSslEccPublicCompressedBlob);
			AddNewKey(ki);
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
	
	Directory::CreateDirectory(Path::GetDirectoryName(DbWalletFilePath));
	m_dbWallet.Create(DbWalletFilePath);

	try {
		TransactionScope dbtx(m_dbWallet);

		m_dbWallet.ExecuteNonQuery("PRAGMA page_size=8192");
		m_dbWallet.ExecuteNonQuery(
			"CREATE TABLE globals (name PRIMARY KEY, value);"
			"CREATE TABLE nets (netid INTEGER PRIMARY KEY AUTOINCREMENT, name UNIQUE, bestblockhash);"
			"CREATE TABLE privkeys (id INTEGER PRIMARY KEY AUTOINCREMENT, privkey UNIQUE, pubkey, timestamp, comment DEFAULT NULL, reserved);"
			"CREATE TABLE pubkeys (netid INTEGER REFERENCES nets(netid), hash160, comment, PRIMARY KEY (netid, hash160));"
			"CREATE TABLE mytxes (id INTEGER PRIMARY KEY AUTOINCREMENT, netid INTEGER REFERENCES nets(netid), hash, data, timestamp, blockord, comment, fromme, UNIQUE (netid, hash));"  		//  blockord=-15 means the Tx is Canceled
			"CREATE TABLE usertxes (txid INTEGER REFERENCES mytxes(id) ON DELETE CASCADE, nout, value, addr160);"
			"CREATE TABLE coins (txid INTEGER REFERENCES mytxes(id) ON DELETE CASCADE, nout INTEGER NOT NULL CHECK (nout>=0), value INTEGER NOT NULL CHECK (value>0), keyid INTEGER REFERENCES privkeys(id), pkscript, PRIMARY KEY (txid, nout))"
			);
		
		CreateWalletNetDbEntry(1, "Bitcoin");

		SetUserVersion(m_dbWallet);

		RandomRef().NextBytes(Buf(Salt.data(), sizeof Salt));
		m_dbWallet.ExecuteNonQuery(EXT_STR("INSERT INTO globals (name, value) VALUES(\'salt\', " << *(Int64*)Salt.data() << ")"));

		GenerateNewAddress("", 1);
	} catch (RCExc) {
		m_dbWallet.Close();
		File::Delete(DbWalletFilePath);
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
		File::Delete(DbPeersFilePath);
		throw;
	}
}

bool CoinDb::IsBanned(const IPAddress& ip) {
	EXT_LOCK (MtxDbPeers) {		
		DbDataReader dr = m_cmdIsBanned.Bind(1, ip.GetAddressBytes()).ExecuteReader();
		return dr.Read() && dr.GetInt32(0);
	}
}

void CoinDb::UpdateOrInsertPeer(const Peer& peer) {
	Blob blobIp = peer.get_EndPoint().Address.GetAddressBytes();
	DbDataReader dr = CmdPeerByIp.Bind(1, blobIp).ExecuteReader();
	if (dr.Read()) {
		CmdUpdatePeer.Bind(1, peer.get_LastPersistent().UnixEpoch)
			.Bind(2, peer.Banned)
			.Bind(3, dr.GetInt32(0))
			.ExecuteNonQuery();
	} else {
		CmdInsertPeer.Bind(1, blobIp)
			.Bind(2, peer.get_LastPersistent().UnixEpoch)
			.Bind(3, peer.Banned)
			.ExecuteNonQuery();
	}
}

void CoinDb::BanPeer(Peer& peer) {
	peer.Banned = true;
	EXT_LOCK (MtxDbPeers) {
		UpdateOrInsertPeer(peer);
	}
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
						.Bind(2, (Int64)peer.Services)
						.Bind(3, dr.GetInt32(0))
						.ExecuteNonQuery();
				} else {
					CmdInsertEndpoint.Bind(1, idPeersNet)
						.Bind(2, blobIp)
						.Bind(3, peer.get_EndPoint().Port)
						.Bind(4, (Int64)peer.Services)
						.ExecuteNonQuery();
				}
			}
		}
	}
}

bool CoinDb::get_NeedPassword() {
	return m_masterPassword == nullptr;
}

void CoinDb::OnIpDetected(const IPAddress& ip) {
	EXT_LOCK (MtxDb) {
		TRC(1, ip);
		LocalIp4 = ip;
	}
	AddLocal(ip);
}

bool CoinDb::get_WalletDatabaseExists() {
	return File::Exists(DbWalletFilePath);
}

bool CoinDb::get_PeersDatabaseExists() {
	return File::Exists(DbPeersFilePath);
}

void CoinDb::LoadKeys(RCString password) {
	Aes aes;
	if (!password.IsEmpty()) {
		InitAes(aes, password);
	}

	EXT_LOCK (MtxDb) {
		unordered_map<HashValue160, MyKeyInfo> keys;
		SqliteCommand cmd("SELECT id, privkey, pubkey, timestamp, comment FROM privkeys", m_dbWallet);
		for (DbDataReader dr=cmd.ExecuteReader(); dr.Read();) {
			MyKeyInfo ki;
			ki.KeyRowid = dr.GetInt32(0);
			ki.PubKey = ToUncompressedKey(dr.GetBytes(2));
			ki.Timestamp = DateTime::FromUnix(dr.GetInt64(3));
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
				if (password == "")
					Throw(HRESULT_FROM_WIN32(ERROR_LOGON_FAILURE));
				if (password == nullptr) {
					m_masterPassword = nullptr;
					ki.Key = CngKey::Import(ki.PubKey, CngKeyBlobFormat::OSslEccPublicBlob);
				} else {
					try {
						Blob ptext = aes.Decrypt(ConstBuf(buf.P+1, buf.Size-1));
						if (ptext.Size < 5)
							Throw(E_FAIL);
						ki.SetPrivData(Blob(ptext.constData(), ptext.Size-4), ki.PubKey.Size == 33);
						if (Crc32().ComputeHash(ki.get_PrivKey()) != Blob(ptext.constData()+ptext.Size-4, 4))
							Throw(HRESULT_FROM_WIN32(ERROR_LOGON_FAILURE));
					} catch (OpenSslException&) {
						Throw(HRESULT_FROM_WIN32(ERROR_LOGON_FAILURE));
					}
					ki.Key = CngKey::Import(ki.get_PrivKey(), CngKeyBlobFormat::OSslEccPrivateBignum);
				}
				break;
			default:
				Throw(E_COIN_InconsistentDatabase);
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
		Filter = new CoinFilter(Hash160ToMyKey.size()*2, 0.01, Ext::Random().Next(), CoinFilter::BLOOM_UPDATE_ALL);
		EXT_FOR (const CoinDb::CHash160ToMyKey::value_type& kv, Hash160ToMyKey) {
			Filter->Insert(kv.first);
			Filter->Insert(kv.second.PubKey);
		}
	}
}

void CoinDb::Load() {
	if (!m_bLoaded) {
		m_bLoaded = true;
		DetectGlobalIp::Start(&m_tr);

		if (!!DbWalletFilePath) {
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
			Int64 salt = SqliteCommand("SELECT value FROM globals WHERE name=\'salt\'", m_dbWallet).ExecuteInt64Scalar();
			memcpy(&Salt[0], &salt, sizeof Salt);
		}			

		if (!!DbPeersFilePath) {
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
			SqliteCommand(EXT_STR("UPDATE peers SET ban=0 WHERE lastlive < " << (now-TimeSpan::FromDays(1)).UnixEpoch), m_dbPeers).ExecuteNonQuery();
		}

		if (ListeningPort != -1)
			(new P2P::ListeningThread(_self, m_tr))->Start();
		MsgLoopThread->Start();
	}
}

void CoinDb::Close() {
	if (m_bLoaded) {
#if UCFG_COIN_GENERATE
		if (Miner.get())
			Miner->Stop();
#endif
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

void CoinDb::InitAes(Aes& aes, RCString password) {
	std::pair<Blob, Blob> pp = Aes::GetKeyAndIVFromPassword(password, Salt.data(), 1000);
	aes.Key = pp.first;
	aes.IV = pp.second;
}

void CoinDb::put_Password(RCString password) {
	CheckPasswordPolicy(password);
	if (m_bLoaded) {
		LoadKeys(password);
	}
	m_masterPassword = password;
	Load();
	TopUpPool();
}

void CoinDb::ChangePassword(RCString oldPassword, RCString newPassword) {
	CheckPasswordPolicy(newPassword);
	try {
		LoadKeys(oldPassword);	
	} catch (RCExc ex) {
		if (ex.HResult == HRESULT_FROM_WIN32(ERROR_LOGON_FAILURE))
			Throw(HRESULT_FROM_WIN32(ERROR_WRONG_PASSWORD));
		throw;
	}

	Aes aes;
	if (!newPassword.IsEmpty())
		InitAes(aes, newPassword);

	EXT_LOCK (MtxDb) {
		SqliteCommand cmd("UPDATE privkeys SET privkey=? WHERE id=?", m_dbWallet);
		TransactionScope dbtx(m_dbWallet);
		for (CHash160ToMyKey::iterator it=Hash160ToMyKey.begin(), e=Hash160ToMyKey.end(); it!=e; ++it) {
			const MyKeyInfo& ki = it->second;
			cmd.Bind(1, !newPassword.IsEmpty() ? ki.EncryptedPrivKey(aes) : ki.PlainPrivKey())
				.Bind(2, ki.KeyRowid)
				.ExecuteNonQuery();
		}
		m_masterPassword = newPassword;
	}
}

MyKeyInfo CoinDb::GetMyKeyInfo(const HashValue160& hash160) {
	MyKeyInfo r;
	if (!EXT_LOCKED(MtxDb, Lookup(Hash160ToMyKey, hash160, r)))
		Throw(E_FAIL);
	return r;
}

Link *CoinDb::CreateLink(CThreadRef& tr) {
	return new CoinLink(_self, tr);
}


} // Coin::

