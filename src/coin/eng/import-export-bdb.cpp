/*######   Copyright (c) 2013-2019 Ufasoft  http://ufasoft.com  mailto:support@ufasoft.com,  Sergey Pavlov  mailto:dev@ufasoft.com ####
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
	CanonicalPubKey PubKey;
};

class BdbWriter : protected BinaryWriter {
	typedef BinaryWriter base;
public:
	BdbWriter(Stream& stm)
		: base(stm)
	{}

	void WriteCompactSize(size_t v) {
		if (v < 253)
			_self << uint8_t(v);
		else if (v < numeric_limits<uint16_t>::max())
			_self << uint8_t(253) << uint16_t(v);
		else if (v < numeric_limits<uint32_t>::max())
			_self << uint8_t(254) << uint32_t(v);
		else
			_self << uint8_t(255) << uint64_t(v);
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

	void Write(const vector<uint8_t>& v) {
		WriteCompactSize(v.size());
		if (!v.empty())
			base::Write(&v[0], v.size());
	}

	void Write(const Blob& blob) {
		WriteCompactSize(blob.size());
		base::Write(blob.constData(), blob.size());
	}

	template <typename T, typename U>
	void Write(const pair<T, U>& pp) {
		Write(pp.first);
		Write(pp.second);
	}

	void Write(const KeyPool& pool) {
		Write(CLIENT_VERSION);
		Write(to_time_t(pool.Timestamp));
		Write(Span(pool.PubKey.Data));
	}
};


class BdbWallet {
public:
	Db m_db;

	BdbWallet(DbEnv& env, RCString name)
		: m_db(&env, 0)
	{
		m_db.open(0, name, "main", DB_BTREE, DB_CREATE, 0);

		Write(string("minversion"), int(60000));
	}

	template <typename K, typename V>
	void Write(const K& key, const V& val) {
		MemoryStream stmKey;
		BdbWriter(stmKey).Write(key);
		Blob blobKey = Span(stmKey);

		MemoryStream stmVal;
		BdbWriter(stmVal).Write(val);
		Blob blobVal = Span(stmVal);

		Dbt datKey(blobKey.data(), blobKey.size()),
			datVal(blobVal.data(), blobVal.size());
        m_db.put(0, &datKey, &datVal, 0);
	}
};

void Wallet::ExportWalletToBdb(const path& filepath) {
	CCoinEngThreadKeeper engKeeper(&Eng);

	if (Eng.m_cdb.NeedPassword)
		Throw(ExtErr::PasswordTooShort);
	if (exists(filepath))
		Throw(HRESULT_FROM_WIN32(ERROR_FILE_EXISTS));

	path dir = filepath.parent_path(),
		name = filepath.filename();
    DbEnv dbenv(0);
//	dbenv.set_lg_dir(dir.string().c_str());
//	dbenv.set_errfile(fopen((dir / "db.log").string().c_str(), "a")); /// debug
    dbenv.set_flags(DB_AUTO_COMMIT, 1);
    dbenv.set_flags(DB_TXN_WRITE_NOSYNC, 1);
	dbenv.log_set_config(DB_LOG_AUTO_REMOVE | DB_LOG_IN_MEMORY, 1);

	int ret = dbenv.open(dir.string().c_str()
		, DB_CREATE | DB_INIT_TXN | DB_INIT_LOCK
		  | DB_INIT_LOG | DB_INIT_MPOOL
		//| DB_THREAD
		| DB_RECOVER					
		, S_IRUSR | S_IWUSR);

	EXT_LOCK (Eng.m_cdb.MtxDb) {
		BdbWallet w(dbenv, name.native());
		{
			SqliteCommand cmd("SELECT pubkey, reserved FROM privkeys ORDER BY id", Eng.m_cdb.m_dbWallet);
			int64_t nPool = 0;
			for (DbDataReader dr = cmd.ExecuteReader(); dr.Read();) {
				KeyInfo ki = Eng.m_cdb.Hash160ToKey[Hash160(CanonicalPubKey::FromCompressed(dr.GetBytes(0)).Data)];
				w.Write(make_pair(string("key"), Span(ki.PubKey.Data)), ki->ToPrivateKeyDER());

				if (dr.GetInt32(1)) {
					KeyPool pool = { ki->Timestamp, ki.PubKey };
					w.Write(make_pair(string("pool"), ++nPool), pool);
				}

				if (!ki->Comment.empty()) {
					Address addr = ki->ToAddress();
					w.Write(make_pair(string("name"), addr.ToString()), ki->Comment);
				}
			}
		}
		{
			SqliteCommand cmd("SELECT hash160, type, comment, nets.name FROM pubkeys JOIN nets ON pubkeys.netid=nets.netid", Eng.m_cdb.m_dbWallet);
			for (DbDataReader dr = cmd.ExecuteReader(); dr.Read();) {
				String comment = dr.GetString(2);
				if (!comment.empty()) {
					AddressType typ = AddressType(dr.GetInt32(1));
					Span data = dr.GetBytes(0);
					Address a = typ == AddressType::Bech32
						? TxOut::CheckStandardType(data)
						: Address(*m_eng, typ, HashValue160(data));
					a->Type = typ;			// override for Bech32
					a->Comment = comment;
					w.Write(make_pair(string("name"), a.ToString()), comment);
				}
			}
		}
	}
    dbenv.txn_checkpoint(0, 0, 0);
    dbenv.lsn_reset(name.string().c_str(), 0);

	char** listp = 0;
	int rcArch = dbenv.log_archive(&listp, DB_ARCH_ABS|DB_ARCH_LOG);
	dbenv.close(0);
	if (listp) {
		for (; *listp; ++listp)
			filesystem::remove(*listp);
	}

	DbEnv dbenv2(0);
	dbenv2.remove(dir.string().c_str(), 0);
}


}	// Coin::


#endif	// HAVE_BERKELEY_DB



