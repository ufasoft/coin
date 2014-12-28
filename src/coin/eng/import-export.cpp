/*######     Copyright (c) 1997-2015 Ufasoft  http://ufasoft.com  mailto:support@ufasoft.com,  Sergey Pavlov  mailto:dev@ufasoft.com #########################################################################################################
#                                                                                                                                                                                                                                            #
# This program is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation;  either version 3, or (at your option) any later version.          #
# This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.   #
# You should have received a copy of the GNU General Public License along with this program; If not, see <http://www.gnu.org/licenses/>                                                                                                      #
############################################################################################################################################################################################################################################*/

#include <el/ext.h>
#include <el/xml.h>
#include <el/crypto/hash.h>



#include <el/db/bdb-reader.h>

using Ext::DB::BdbReader;



#include "coin-protocol.h"
#include "script.h"
#include "eng.h"
#include "coin-msg.h"
#include "crypter.h"


namespace Coin {

static pair<String, Blob> SplitKey(const ConstBuf& cbuf) {
	pair<String, Blob> r;
	CMemReadStream stm(cbuf);
	BinaryReader rd(stm);
	byte size;
	stm.ReadBuffer(&size, 1);
	if (size <= cbuf.Size-1) {
		Blob blobKey(0, size);
		stm.ReadBuffer(blobKey.data(), blobKey.Size);
		String k = r.first = String((const char*)blobKey.data(), blobKey.Size);
		if (k == "tx")
			r.second = rd.ReadBytes(32);
		else if (k == "pool") {
			int64_t idx = rd.ReadInt64();
		} else if (k=="name" || k=="key" || k=="ckey" || k=="mkey" || k=="setting")
			r.second = CoinSerialized::ReadBlob(rd);
		else if (k=="version" ||
			k=="minversion" ||
			k=="bestblock" ||
			k=="blockindex" ||
			k=="defaultkey" ||
			k=="orderposnext" ||
			k== "acc")
		{
		}
		else {
#ifdef _DEBUG
			Throw(E_NOTIMPL);
#endif
		}
	}
	return r;
}

void CoinDb::ImportXml(RCString filepath) {
	vector<MyKeyInfo> vKey;

#if UCFG_WIN32
	XmlDocument doc = new XmlDocument;
	doc.Load(filepath);
	XmlNodeReader rd(doc);
#else
	ifstream ifs(filepath.c_str());
	XmlTextReader rd(ifs);
#endif
	
	rd.ReadToFollowing("PrivateKeys");
	for (bool b=rd.ReadToDescendant("Key"); b; b=rd.ReadToNextSibling("Key")) {
		MyKeyInfo ki;
		ki.SetPrivData(PrivateKey(rd.GetAttribute("privkey")));
		ki.Timestamp = DateTime::ParseExact(rd.GetAttribute("timestamp"), "u");

		String s = rd.GetAttribute("comment");
		if (!s.empty())
			ki.Comment = s;
		if (rd.GetAttribute("reserved") == "1")
			ki.Comment = nullptr;
		vKey.push_back(ki);
	}

	rd.ReadToFollowing("Recipients");
	for (bool b=rd.ReadToDescendant("Recipient"); b; b=rd.ReadToNextSibling("Recipient")) {

	}

	rd.ReadToFollowing("Transactions");
	for (bool b=rd.ReadToDescendant("Transaction"); b; b=rd.ReadToNextSibling("Transaction")) {

	}

	EXT_LOCK (MtxDb) {
		TransactionScope dbtx(m_dbWallet);

		for (int i=0l; i<vKey.size(); ++i) {
			MyKeyInfo& ki = vKey[i];
			if (!SetPrivkeyComment(ki.Hash160, ki.Comment))
				AddNewKey(ki);
		}
	}
}

void CoinDb::ImportDat(RCString filepath, RCString password) {
	set<Blob> setPrivKeys;
	vector<MyKeyInfo> myKeys;
	unordered_map<HashValue160, Address> pubkeyToAddress;
	unordered_map<uint32_t, CMasterKey> masterKeys;
	vector<pair<Blob, Blob>> ckeys;
	unordered_set<HashValue160> setReserved;
	
	BdbReader rd(filepath);	
	Blob key, val;
	while (rd.Read(key, val)) {
		try {
			pair<String, Blob> pp = SplitKey(key);
			if (pp.first == "name") {
				String sa = Encoding::UTF8.GetChars(pp.second);
				IPAddress ip;
				if (IPAddress::TryParse(sa, ip)) {
					//!!!TODO  implement IP-address destinations				
				} else {
					Address a(Eng(), sa);	//!!!? Eng was nullptr
					HashValue160 h160(a);
					a.Comment = Encoding::UTF8.GetChars(CoinSerialized::ReadBlob(BinaryReader(CMemReadStream(val))));
					pubkeyToAddress.insert(make_pair(h160, a));
				}
			} else if (pp.first == "mkey") {
				uint32_t id = *(uint32_t*)pp.second.data();
				CMasterKey mkey;
				BinaryReader(CMemReadStream(val)) >> mkey;
				masterKeys.insert(make_pair(id, mkey));
			} else if (pp.first == "key") {
				CngKey cng = CngKey::Import(CoinSerialized::ReadBlob(BinaryReader(CMemReadStream(val))), CngKeyBlobFormat::OSslEccPrivateBlob);
				MyKeyInfo ki;
				ki.SetPrivData(cng.Export(CngKeyBlobFormat::OSslEccPrivateBignum), pp.second.Size == 33);
				if (ki.PubKey != pp.second)
					Throw(E_FAIL);
				if (setPrivKeys.insert(ki.PrivKey).second)
					myKeys.push_back(ki);
			} else if (pp.first == "wkey") {
				CWalletKey wkey;
				BinaryReader(CMemReadStream(val)) >> wkey;
				MyKeyInfo ki;
				ki.Timestamp = DateTime::from_time_t(wkey.nTimeCreated);
				ki.SetPrivData(wkey.vchPrivKey, pp.second.Size == 33);
				if (ki.PubKey != pp.second)
					Throw(E_FAIL);

				ki.Comment = wkey.strComment;
				if (setPrivKeys.insert(ki.PrivKey).second)
					myKeys.push_back(ki);
			} else if (pp.first == "ckey") {
				Blob pubKey = pp.second;
				Blob privKey;
				BinaryReader(CMemReadStream(val)) >> privKey;
				ckeys.push_back(make_pair(pp.second, privKey));
			} else if (pp.first == "pool") {
				setReserved.insert(Hash160(val));
			}
		} catch (RCExc) {
		}
	}

	if (!masterKeys.empty()) {
		if (password.empty())
			Throw(HRESULT_FROM_WIN32(ERROR_INVALID_PASSWORD));

		CCrypter crypter;
	    CKeyingMaterial vMasterKey;
		for (auto it=masterKeys.begin(); it!=masterKeys.end(); ++it) {
			const CMasterKey& mkey = it->second;
            if(!crypter.SetKeyFromPassphrase(explicit_cast<string>(password), mkey.vchSalt, mkey.nDeriveIterations, mkey.nDerivationMethod))
                Throw(HRESULT_FROM_WIN32(ERROR_INVALID_PASSWORD));
            if (!crypter.Decrypt(mkey.vchCryptedKey, vMasterKey))
                Throw(HRESULT_FROM_WIN32(ERROR_INVALID_PASSWORD));

			for (int i=ckeys.size(); i--;) {
				Blob& pubKey = ckeys[i].first,
					&vchCryptedSecret = ckeys[i].second;
				Blob vchSecret;
				if(DecryptSecret(vMasterKey, vchCryptedSecret, Hash(pubKey), vchSecret)) {
					MyKeyInfo ki;
					ki.SetPrivData(vchSecret, pubKey.Size == 33);
					if (ki.PubKey == pubKey) {
						Blob privKey = ki.Key.Export(CngKeyBlobFormat::OSslEccPrivateBignum);
						for (int j=0; j<myKeys.size(); ++j) {
							if (myKeys[j].PubKey == pubKey) {
								myKeys[j].SetPrivData(privKey, pubKey.Size == 33);
								goto LAB_DECRYPTED;
							}
						}
						myKeys.push_back(ki);						
LAB_DECRYPTED:
						ckeys.erase(ckeys.begin()+i);
					}
				}
			}
		}
		if (!ckeys.empty())
			Throw(HRESULT_FROM_WIN32(ERROR_INVALID_PASSWORD));
	}

	EXT_FOR (const HashValue160& hash160, setReserved) {
		auto it = pubkeyToAddress.find(hash160);
		if (it != pubkeyToAddress.end())
			it->second.Comment = nullptr;
	}

	EXT_LOCK (MtxDb) {
		TransactionScope dbtx(m_dbWallet);

		for (int i=0; i<myKeys.size(); ++i) {
			MyKeyInfo& ki = myKeys[i];
			auto it = pubkeyToAddress.find(ki.Hash160);
			if (it != pubkeyToAddress.end()) {
				ki.Comment = it->second.Comment;
				pubkeyToAddress.erase(ki.Hash160);
			}
			if (!SetPrivkeyComment(ki.Hash160, ki.Comment))
				AddNewKey(ki);
		}

		for (auto it=pubkeyToAddress.begin(); it!=pubkeyToAddress.end(); ++it) {
			const Address& a = it->second;
			EXT_LOCK (MtxNets) {
				for (int i=0; i<m_nets.size(); ++i) {
					CoinEng& eng = *static_cast<CoinEng*>(m_nets[i]);
					if (eng.ChainParams.AddressVersion == a.Ver) {
						if (eng.m_iiEngEvents) {
							try {
								TRC(2, a.Comment);
								DBG_LOCAL_IGNORE(E_COIN_RecipientAlreadyPresents);
								DBG_LOCAL_IGNORE(MAKE_HRESULT(SEVERITY_ERROR, FACILITY_SQLITE, 19));
								eng.m_iiEngEvents->AddRecipient(a);
							} catch (Exception& ex) {
								if (ToHResult(ex) != E_COIN_RecipientAlreadyPresents)
									throw;
							}
						}
						break;
					}
				}
			}
		}
	}
}

void CoinDb::ImportWallet(const path& filepath, RCString password) {
	if (ToLower(filepath.extension()) == ".xml")
		ImportXml(filepath);
	else
		ImportDat(filepath, password);
	//TODO Start rescans	
}

void CoinDb::ExportWalletToXml(const path& filepath) {
	if (NeedPassword)
		Throw(HRESULT_FROM_WIN32(ERROR_PWD_TOO_SHORT));
	
	ofstream ofs(String(filepath).ToOsString());	
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
				const MyKeyInfo& ki = Hash160ToMyKey[Hash160(ToUncompressedKey(dr.GetBytes(0)))];

				xoKey["privkey"] = PrivateKey(ki.get_PrivKey(), ki.PubKey.Size == 33).ToString();
				xoKey["timestamp"] = ki.Timestamp.ToString("u");
				if (dr.GetInt32(1))
					xoKey["reserved"] = "1";
				if (!ki.Comment.empty()) {
					xoKey["comment"] = ki.Comment;
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


} // Coin::


