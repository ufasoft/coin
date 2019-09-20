/*######   Copyright (c) 2013-2019 Ufasoft  http://ufasoft.com  mailto:support@ufasoft.com,  Sergey Pavlov  mailto:dev@ufasoft.com ####
#                                                                                                                                     #
# 		See LICENSE for licensing information                                                                                         #
#####################################################################################################################################*/

#include <el/ext.h>
#include <el/xml.h>
#include <el/crypto/hash.h>


#include <el/db/bdb-reader.h>

using Ext::DB::BdbReader;


#include "coin-protocol.h"
#include "script.h"
#include "eng.h"
#include "crypter.h"


namespace Coin {

static String ReadKey(Stream& stm) {
	uint8_t size;
	stm.ReadBuffer(&size, 1);
	if (size <= stm.Length - 1) {
		Blob blobKey(0, size);
		stm.ReadBuffer(blobKey.data(), blobKey.size());
		return String((const char*)blobKey.data(), blobKey.size()).c_str();	//!!! to avoid all-zeroes string
	}
	return String();
}

void CoinDb::ImportXml(const path& filepath) {
	vector<KeyInfo> vKey;

#if UCFG_WIN32
	XmlDocument doc = new XmlDocument;
	doc.Load(filepath.native());
	XmlNodeReader rd(doc);
#else
	ifstream ifs(filepath.c_str());
	XmlTextReader rd(ifs);
#endif

	rd.ReadToFollowing("PrivateKeys");
	for (bool b=rd.ReadToDescendant("Key"); b; b=rd.ReadToNextSibling("Key")) {
		KeyInfo ki;
		ki->SetPrivData(PrivateKey(rd.GetAttribute("privkey")));
		ki->Timestamp = DateTime::ParseExact(rd.GetAttribute("timestamp"), "u");

		String s = rd.GetAttribute("comment");
		if (!s.empty())
			ki->Comment = s;
		if (rd.GetAttribute("reserved") == "1")
			ki->Comment = nullptr;
		vKey.push_back(ki);
	}

	rd.ReadToFollowing("Recipients");
	for (bool b = rd.ReadToDescendant("Recipient"); b; b = rd.ReadToNextSibling("Recipient")) {

	}

	rd.ReadToFollowing("Transactions");
	for (bool b=rd.ReadToDescendant("Transaction"); b; b=rd.ReadToNextSibling("Transaction")) {

	}

	EXT_LOCK (MtxDb) {
		TransactionScope dbtx(m_dbWallet);

		for (auto& ki : vKey)
			if (!SetPrivkeyComment(ki->ToAddress(), ki->Comment))
				AddNewKey(ki);
	}
}

struct KeyPoolInfo {
	DateTime Timestamp;
	int64_t Index;
};

void CoinDb::ImportDat(const path& filepath, RCString password) {
	CoinEng& eng = Eng();

	set<PrivateKey> setPrivKeys;
	vector<KeyInfo> myKeys;
	unordered_map<Blob, KeyPoolInfo> pubKeyToKeyPoolInfo;
	unordered_map<int64_t, KeyInfo> indexToKeyInfo;
	unordered_set<Address> addresses;
	unordered_map<uint32_t, CMasterKey> masterKeys;
	vector<pair<Blob, Blob>> ckeys;		// <pubkey,privkey>

	unsigned nKey = 0, nWkey = 0, nCKey = 0, nPool = 0, nMKey = 0, nName = 0, nDestdata = 0;

	BdbReader rd(filepath);
	Blob key, val;
	while (rd.Read(key, val)) {
		try {
			DBG_LOCAL_IGNORE_CONDITION(ExtErr::InvalidUTF8String);

			CMemReadStream stm(key);
			String k = ReadKey(stm);
			BinaryReader rd(stm);
			switch (Convert::ToMultiChar(k)) {
			case 'name': {
				++nName;
				String sa = Encoding::UTF8.GetChars(CoinSerialized::ReadBlob(rd));
				IPAddress ip;
				if (IPAddress::TryParse(sa, ip)) {
					//!!!TODO  implement IP-address destinations
				} else {
					String name;
					BinaryReader(CMemReadStream(val)) >> name;
					Address a(eng, sa, name);	//!!!? Eng was nullptr
					addresses.insert(a);
				}
			} break;
			case 'mkey': {
				++nMKey;
				uint32_t id = rd.ReadUInt32();
				CMasterKey mkey;
				BinaryReader(CMemReadStream(val)) >> mkey;
				masterKeys.insert(make_pair(id, mkey));
			} break;
			case 'key': {
				++nKey;
				KeyInfo ki;
				ki->FromDER(CoinSerialized::ReadBlob(BinaryReader(CMemReadStream(val))), CoinSerialized::ReadBlob(rd));
				if (setPrivKeys.insert(ki.PrivKey).second)
					myKeys.push_back(ki);
			} break;
			case 'wkey': {
				++nWkey;
				CWalletKey wkey;
				BinaryReader(CMemReadStream(val)) >> wkey;
				KeyInfo ki;
				ki->Timestamp = DateTime::from_time_t(wkey.nTimeCreated);
				Blob pubkey = CoinSerialized::ReadBlob(rd);
				ki->SetPrivData(PrivateKey(wkey.vchPrivKey, pubkey.size() == 33));
				if (!Equal(ki.PubKey.Data, pubkey))
					Throw(E_FAIL);

				ki->Comment = wkey.strComment;
				if (setPrivKeys.insert(ki.PrivKey).second)
					myKeys.push_back(ki);
			} break;
			case 'ckey': {
				++nCKey;
				Blob pubKey = CoinSerialized::ReadBlob(rd);
				Blob privKey;
				BinaryReader(CMemReadStream(val)) >> privKey;
				ckeys.push_back(make_pair(pubKey, privKey));
			} break;
			case 'pool': {
				++nPool;
				CMemReadStream stmVal(val);
				BinaryReader rdVal(stmVal);
				uint32_t ver;
				int64_t nTime;
				Blob pubKey;
				rdVal >> ver >> nTime >> pubKey;
				bool bInternal = !stmVal.Eof() && rdVal.ReadBoolean();
				bool bPreSplit = !stmVal.Eof() && rdVal.ReadBoolean();
				KeyPoolInfo keyPoolInfo;
				keyPoolInfo.Index = rd.ReadInt64();
				keyPoolInfo.Timestamp = DateTime::from_time_t(nTime);
				pubKeyToKeyPoolInfo[pubKey] = keyPoolInfo;
			} break;
			case 'tx': {
				Blob data = rd.ReadBytes(32);
			} break;
			default:
				if (k == "destdata") {
					++nDestdata;
					String s1, s2;
					rd >> s1 >> s2;
					String sval;
					BinaryReader(CMemReadStream(val)) >> sval;
					KeyInfo ki;
					ki->SetPrivData(PrivateKey(s2));
					Address a(eng, s1);
					ki.AddressType = a.Type;
				} else if (k == "setting") {
					Blob blob = CoinSerialized::ReadBlob(rd);
				} else if (k == "version" ||
					k == "minversion" ||
					k == "bestblock" ||
					k == "bestblock_nomerkle" ||
					k == "blockindex" ||
					k == "defaultkey" ||
					k == "orderposnext" ||
					k == "keymeta" ||
					k == "watchmeta" ||
					k == "hdchain" ||
					k == "flags" ||
					k == "purpose" ||
					k == "cscript" ||
					k == "acc")
				{
				} else if (k == "") {

				} else {
#ifdef _DEBUG
					Throw(E_NOTIMPL);
#endif
				}
			}
		} catch (RCExc) {
		}
	}

#ifdef _DEBUG//!!!D
	unordered_set<Blob> ckeySet;
	for (auto& pp : ckeys)
		ckeySet.insert(pp.second);
	auto nUniqCkey = ckeySet.size();

#endif

	if (!masterKeys.empty()) {
		{
			DBG_LOCAL_IGNORE_CONDITION(ExtErr::InvalidPassword);
			if (password.empty())
				Throw(ExtErr::InvalidPassword);
		}

		CCrypter crypter;
	    CKeyingMaterial vMasterKey;
		for (auto it = masterKeys.begin(); it != masterKeys.end(); ++it) {
			const CMasterKey& mkey = it->second;
			if (!crypter.SetKeyFromPassphrase(explicit_cast<string>(password), mkey.vchSalt, mkey.nDeriveIterations, mkey.nDerivationMethod) ||
					!crypter.Decrypt(mkey.vchCryptedKey, vMasterKey))
				Throw(ExtErr::InvalidPassword);

			for (int i = ckeys.size(); i--;) {
				Blob& pubKey = ckeys[i].first,
					&vchCryptedSecret = ckeys[i].second;
				Blob vchSecret;
				if (DecryptSecret(vMasterKey, vchCryptedSecret, eng.HashForWallet(pubKey), vchSecret)) {
					KeyInfo ki;
					PrivateKey privKey(vchSecret, pubKey.size() == 33);
					ki->SetPrivData(privKey);
					ki->Comment = nullptr;
					if (auto oPool = Lookup(pubKeyToKeyPoolInfo, pubKey)) {
						ki->Timestamp = oPool.value().Timestamp;
						ki->Reserved = true;
					}

					Blob pubKeyData(Span(ki.PubKey.Data));
					if (Equal(pubKeyData, pubKey)) {
						PrivateKey privKey = ki.PrivKey; //!!!R ki.Key.Export(CngKeyBlobFormat::OSslEccPrivateBignum);
						for (int j = 0; j < myKeys.size(); ++j) {
							if (Equal(myKeys[j].PubKey.Data, pubKey)) {
								myKeys[j]->SetPrivData(privKey, pubKey.size() == 33);
								goto LAB_DECRYPTED;
							}
						}
						myKeys.push_back(ki);
LAB_DECRYPTED:
						ckeys.erase(ckeys.begin() + i);
					}
				} else
					Throw(ExtErr::Crypto);
			}
		}
		if (!ckeys.empty())
 			Throw(ExtErr::InvalidPassword);
	}

	for (auto& kv : pubKeyToKeyPoolInfo) {
		auto jt = indexToKeyInfo.find(kv.second.Index);
		if (jt != indexToKeyInfo.end()) {
			jt->second->Timestamp = kv.second.Timestamp;
			jt->second->Comment = nullptr;
		}
	}

	EXT_LOCK (MtxDb) {
		TransactionScope dbtx(m_dbWallet);

		for (auto& ki : myKeys) {
			auto a = ki->ToAddress();
			auto it = addresses.find(a);
			if (it != addresses.end()) {
				ki->Comment = it->Comment;
				addresses.erase(it);
			}

			a = Address(eng, AddressType::P2SH, ki.PubKey.ScriptHash);
			if ((it = addresses.find(a)) != addresses.end()) {
				ki->Comment = it->Comment;
				addresses.erase(it);
			}

			HashValue160 pubKeyHash = ki.PubKey.Hash160;

			a = Address(eng, AddressType::Bech32, pubKeyHash);
			if ((it = addresses.find(a)) != addresses.end()) {
				ki->Comment = it->Comment;
				ki->AddressType = AddressType::Bech32;
				addresses.erase(it);
			}

			Blob redeemScript = Address(eng, AddressType::P2PKH, pubKeyHash)->ToScriptPubKey();
			a = Address(eng, AddressType::Bech32, SHA256().ComputeHash(redeemScript));
			if ((it = addresses.find(a)) != addresses.end()) {
				ki->Comment = it->Comment;
				addresses.erase(it);
			}

			uint8_t bufP2WPKH_P2SH[22] = { 00, 20 };
			memcpy(bufP2WPKH_P2SH + 2, pubKeyHash.data(), 20);
			a = Address(eng, AddressType::P2SH, Hash160(bufP2WPKH_P2SH));
			if ((it = addresses.find(a)) != addresses.end()) {
				ki.AddressType = AddressType::P2WPKH_IN_P2SH;
				ki->Comment = it->Comment;
				addresses.erase(it);
			}

			if (!SetPrivkeyComment(ki->ToAddress(), ki->Comment))
				AddNewKey(ki);
		}

		for (auto a : addresses) {
			String sa = a.ToString();
			String comment = a.Comment;
			EXT_LOCK (MtxNets) {
				for (int i = 0; i < m_nets.size(); ++i) {
					CoinEng& eng = *static_cast<CoinEng*>(m_nets[i]);
					//!!!? if (a.Type == AddressType::P2PKH)
					{
						try {
							TRC(2, a.Comment);
							DBG_LOCAL_IGNORE_CONDITION(CoinErr::RecipientAlreadyPresents);
							DBG_LOCAL_IGNORE(MAKE_HRESULT(SEVERITY_ERROR, FACILITY_SQLITE, 19));
							eng.Events.AddRecipient(a);
						} catch (Exception& ex) {
							if (ex.code() != CoinErr::RecipientAlreadyPresents)
								throw;
						}
						break;
					}
				}
			}
		}
	}
}

void CoinDb::ImportWallet(const path& filepath, RCString password) {
	if (ToLower(filepath.extension().native()) == ".xml")
		ImportXml(filepath);
	else
		ImportDat(filepath, password);
	//TODO Start rescans
}

void CoinDb::ExportWalletToXml(const path& filepath) {
	if (NeedPassword)
		Throw(ExtErr::PasswordTooShort);

	ofstream ofs(filepath);
	XmlTextWriter w(ofs);
	w.Formatting = XmlFormatting::Indented;
	w.WriteStartDocument();

	XmlOut xoCoinWallet(w, "CoinWallet");
	EXT_LOCK (MtxDb) {
		{
			XmlOut xoPrivateKeys(w, "PrivateKeys");
			SqliteCommand cmd("SELECT pubkey, reserved FROM privkeys ORDER BY id", m_dbWallet);
			for (DbDataReader dr=cmd.ExecuteReader(); dr.Read();) {
				XmlOut xoKey(w, "Key");
				KeyInfo ki = Hash160ToKey[Hash160(CanonicalPubKey::FromCompressed(dr.GetBytes(0)).Data)];

				xoKey["privkey"] = PrivateKey(ki.get_PrivKey(), ki.PubKey.IsCompressed()).ToString();
				xoKey["timestamp"] = ki->Timestamp.ToString("u");
				if (dr.GetInt32(1))
					xoKey["reserved"] = "1";
				if (!ki->Comment.empty()) {
					xoKey["comment"] = ki->Comment;
				}
			}
		}
		{
			XmlOut xoPrivateKeys(w, "Recipients");
			SqliteCommand cmd("SELECT hash160, comment, nets.name FROM pubkeys JOIN nets ON pubkeys.netid=nets.netid", m_dbWallet);
			for (DbDataReader dr=cmd.ExecuteReader(); dr.Read();) {
				XmlOut xo(w, "Recipient");
				xo["netname"] = dr.GetString(2);
				xo["hash160"] = EXT_STR(Blob(dr.GetBytes(0)));
				String comment = dr.GetString(1);
				if (!comment.empty())
					xo["comment"] = comment;
			}
		}
		{
			XmlOut xoPrivateKeys(w, "Transactions");
			SqliteCommand cmd("SELECT hash, timestamp, nets.name, comment FROM mytxes JOIN nets ON mytxes.netid=nets.netid", m_dbWallet);
			for (DbDataReader dr=cmd.ExecuteReader(); dr.Read();) {
				XmlOut xo(w, "Transactions");
				xo["hash"] = EXT_STR(HashValue(dr.GetBytes(0)));
				xo["timestamp"] = DateTime::from_time_t(dr.GetInt64(1)).ToString("u");
				xo["netname"] = dr.GetString(2);
				String comment = dr.GetString(3);
				if (!comment.empty())
					xo["comment"] = comment;
			}
		}
	}
}

void ExportKeysThread::Execute() {
	Name = "ExportKeysThread";
	CCoinEngThreadKeeper engKeeper(&Eng);
	CEngStateDescription stateDesc(Eng, EXT_STR("Exporting keys to " << Path));

	vector<KeyInfo> keys;
	EXT_LOCK(Eng.m_cdb.MtxDb) {
		keys.reserve(Eng.m_cdb.Hash160ToKey.size());
		EXT_FOR(const CoinDb::CHash160ToKey::value_type& pp, Eng.m_cdb.Hash160ToKey) {
			keys.push_back(pp.second);
		}
	}
	ofstream ofs(Path);
	for (auto& key : keys) {
		if (m_bStop)
			break;
		ofs << key->ToString(Password) << endl;
	}
}

} // Coin::
