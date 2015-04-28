/*######   Copyright (c) 2013-2015 Ufasoft  http://ufasoft.com  mailto:support@ufasoft.com,  Sergey Pavlov  mailto:dev@ufasoft.com ####
#                                                                                                                                     #
# 		See LICENSE for licensing information                                                                                         #
#####################################################################################################################################*/

#include <el/ext.h>
#include <el/xml.h>
#include <el/crypto/hash.h>


#ifdef HAVE_BERKELEY_DB

#pragma warning(disable: 4458)
#include <el/db/bdb.h>

#include "wallet.h"


namespace Coin {

#define CLIENT_VERSION_MAJOR       0
#define CLIENT_VERSION_MINOR       6
#define CLIENT_VERSION_REVISION   99
#define CLIENT_VERSION_BUILD       0

static const int CLIENT_VERSION =
                           1000000 * CLIENT_VERSION_MAJOR
                         +   10000 * CLIENT_VERSION_MINOR 
                         +     100 * CLIENT_VERSION_REVISION
                         +       1 * CLIENT_VERSION_BUILD;

struct KeyPool {
	DateTime Timestamp;
	Blob PubKey;
};

class BdbWriter : protected BinaryWriter {
	typedef BinaryWriter base;
public:
	BdbWriter(Stream& stm)
		:	base(stm)
	{}


	void WriteCompactSize(size_t v) {
		if (v < 253)
			_self << byte(v);
		else if (v < numeric_limits<uint16_t>::max())
			_self << byte(253) << uint16_t(v);
		else if (v < numeric_limits<uint32_t>::max())
			_self << byte(254) << uint32_t(v);
		else
			_self << byte(255) << uint64_t(v);
	}

	void Write(int32_t v) {
		_self << v;
	}

	void Write(int64_t v) {
		_self << v;
	}

	void Write(const string& s) {
		WriteCompactSize(s.size());
		base::Write(s.data(), s.size());
	}

	void Write(const String& s) {
		Write(explicit_cast<string>(s));
	}

	void Write(const vector<byte>& v) {
		WriteCompactSize(v.size());
		if (!v.empty())
			base::Write(&v[0], v.size());
	}

	void Write(const Blob& blob) {
		WriteCompactSize(blob.Size);
		base::Write(blob.constData(), blob.Size);
	}

	template <typename T, typename U>
	void Write(const pair<T, U>& pp) {
		Write(pp.first);
		Write(pp.second);
	}

	void Write(const KeyPool& pool) {
		Write(CLIENT_VERSION);
		Write(to_time_t(pool.Timestamp));
		Write(pool.PubKey);
	}
};


class BdbWallet {
public:
	Db m_db;

	BdbWallet(DbEnv& env, RCString name)
		:	m_db(&env, 0)
	{
		m_db.open(0, name, "main", DB_BTREE, DB_CREATE, 0);

		Write(string("minversion"), int(60000));
	}

	template <typename K, typename V>
	void Write(const K& key, const V& val) {
		MemoryStream stmKey;
		BdbWriter(stmKey).Write(key);
		Blob blobKey = stmKey;

		MemoryStream stmVal;
		BdbWriter(stmVal).Write(val);
		Blob blobVal = stmVal;

		Dbt datKey(blobKey.data(), blobKey.Size),
			datVal(blobVal.data(), blobVal.Size);
        m_db.put(0, &datKey, &datVal, 0);
	}
};

void Wallet::ExportWalletToBdb(const path& filepath) {
	CCoinEngThreadKeeper engKeeper(&Eng);
	
	if (Eng.m_cdb.NeedPassword)
		Throw(HRESULT_FROM_WIN32(ERROR_PWD_TOO_SHORT));
	if (exists(filepath))
		Throw(HRESULT_FROM_WIN32(ERROR_FILE_EXISTS));

	path dir = filepath.parent_path(),
		name = filepath.filename();
    DbEnv dbenv(0);
    dbenv.set_flags(DB_AUTO_COMMIT, 1);
    dbenv.set_flags(DB_TXN_WRITE_NOSYNC, 1);
	dbenv.log_set_config(DB_LOG_AUTO_REMOVE, 1);

	int ret = dbenv.open(dir.string().c_str(), 
					DB_CREATE     |
                     DB_INIT_LOCK  |
                     DB_INIT_LOG   |
                     DB_INIT_MPOOL |
                     DB_INIT_TXN   |
                     DB_THREAD     |
					DB_RECOVER  ,

					S_IRUSR | S_IWUSR);

	EXT_LOCK (Eng.m_cdb.MtxDb) {
		BdbWallet w(dbenv, name.native());
		{
			SqliteCommand cmd("SELECT pubkey, reserved FROM privkeys ORDER BY id", Eng.m_cdb.m_dbWallet);
			int64_t nPool = 0;
			for (DbDataReader dr=cmd.ExecuteReader(); dr.Read();) {
				const MyKeyInfo& ki = Eng.m_cdb.Hash160ToMyKey[Hash160(ToUncompressedKey(dr.GetBytes(0)))];
				CngKey key = ki.Key;
				Blob blobPrivateKey = key.Export(ki.IsCompressed() ? CngKeyBlobFormat::OSslEccPrivateCompressedBlob : CngKeyBlobFormat::OSslEccPrivateBlob);
				w.Write(make_pair(string("key"), ki.PubKey), blobPrivateKey);

				if (dr.GetInt32(1)) {
					KeyPool pool = { ki.Timestamp, ki.PubKey };
					w.Write(make_pair(string("pool"), ++nPool), pool);
				}

				if (!ki.Comment.empty()) {
					Address addr = ki.ToAddress();
					w.Write(make_pair(string("name"), addr.ToString()), ki.Comment);
				}
			}
		}
		{
			SqliteCommand cmd("SELECT hash160, comment, nets.name FROM pubkeys JOIN nets ON pubkeys.netid=nets.netid", Eng.m_cdb.m_dbWallet);
			for (DbDataReader dr=cmd.ExecuteReader(); dr.Read();) {
				String comment = dr.GetString(1);
				if (!comment.empty()) {
					Address addr(*m_eng, HashValue160(dr.GetBytes(0)), comment);
					w.Write(make_pair(string("name"), addr.ToString()), comment);
				}
			}
		}
	}
    dbenv.txn_checkpoint(0, 0, 0);
    dbenv.lsn_reset(name.native(), 0);
}


}	// Coin::


#endif	// HAVE_BERKELEY_DB





