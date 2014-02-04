#include <el/ext.h>
#include <el/xml.h>
#include <el/crypto/hash.h>


#ifdef HAVE_BERKELEY_DB

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
		else if (v < numeric_limits<UInt16>::max())
			_self << byte(253) << UInt16(v);
		else if (v < numeric_limits<UInt32>::max())
			_self << byte(254) << UInt32(v);
		else
			_self << byte(255) << UInt64(v);
	}

	void Write(Int32 v) {
		_self << v;
	}

	void Write(Int64 v) {
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

void Wallet::ExportWalletToBdb(RCString filepath) {
	CCoinEngThreadKeeper engKeeper(&Eng);
	
	if (Eng.m_cdb.NeedPassword)
		Throw(HRESULT_FROM_WIN32(ERROR_PWD_TOO_SHORT));
	if (File::Exists(filepath))
		Throw(HRESULT_FROM_WIN32(ERROR_FILE_EXISTS));

	String dir = Path::GetDirectoryName(filepath);
	String name = Path::GetFileName(filepath);
    DbEnv dbenv(0);
    dbenv.set_flags(DB_AUTO_COMMIT, 1);
    dbenv.set_flags(DB_TXN_WRITE_NOSYNC, 1);
	dbenv.log_set_config(DB_LOG_AUTO_REMOVE, 1);

	const int S_IRUSR = 0400,
			S_IWUSR   = 0200;

	int ret = dbenv.open(dir, 
					DB_CREATE     |
                     DB_INIT_LOCK  |
                     DB_INIT_LOG   |
                     DB_INIT_MPOOL |
                     DB_INIT_TXN   |
                     DB_THREAD     |
					DB_RECOVER  ,

					S_IRUSR | S_IWUSR);

	EXT_LOCK (Eng.m_cdb.MtxDb) {
		BdbWallet w(dbenv, name);
		{
			SqliteCommand cmd("SELECT pubkey, reserved FROM privkeys ORDER BY id", Eng.m_cdb.m_dbWallet);
			Int64 nPool = 0;
			for (DbDataReader dr=cmd.ExecuteReader(); dr.Read();) {
				const MyKeyInfo& ki = Eng.m_cdb.Hash160ToMyKey[Hash160(ToUncompressedKey(dr.GetBytes(0)))];
				CngKey key = ki.Key;
				Blob blobPrivateKey = key.Export(ki.IsCompressed() ? CngKeyBlobFormat::OSslEccPrivateCompressedBlob : CngKeyBlobFormat::OSslEccPrivateBlob);
				w.Write(make_pair(string("key"), ki.PubKey), blobPrivateKey);

				if (dr.GetInt32(1)) {
					KeyPool pool = { ki.Timestamp, ki.PubKey };
					w.Write(make_pair(string("pool"), ++nPool), pool);
				}

				if (!ki.Comment.IsEmpty()) {
					Address addr = ki.ToAddress();
					w.Write(make_pair(string("name"), addr.ToString()), ki.Comment);
				}
			}
		}
		{
			SqliteCommand cmd("SELECT hash160, comment, nets.name FROM pubkeys JOIN nets ON pubkeys.netid=nets.netid", Eng.m_cdb.m_dbWallet);
			for (DbDataReader dr=cmd.ExecuteReader(); dr.Read();) {
				String comment = dr.GetString(1);
				if (!comment.IsEmpty()) {
					Address addr(HashValue160(dr.GetBytes(0)), comment);
					w.Write(make_pair(string("name"), addr.ToString()), comment);
				}
			}
		}
	}
    dbenv.txn_checkpoint(0, 0, 0);
    dbenv.lsn_reset(name, 0);
}


}	// Coin::


#endif	// HAVE_BERKELEY_DB





