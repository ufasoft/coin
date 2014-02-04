#include <el/ext.h>
#include <el/xml.h>

#include "eng.h"
#include "wallet.h"
#include "coin-com_h.h"
#include "coin-com_i.c"

#include <el/libext/win32/ext-win.h>
#include <el/libext/win32/ext-atl.h>

namespace Coin {

class WalletCom;

class AddressCom : public IDispatchImpl<AddressCom, IAddress>
	, public ISupportErrorInfoImpl<IAddress>
{
public:
	DECLARE_STANDARD_UNKNOWN()

	BEGIN_COM_MAP(AddressCom)
		COM_INTERFACE_ENTRY(IAddress)
		COM_INTERFACE_ENTRY(ISupportErrorInfo)
	END_COM_MAP()

	WalletCom& m_wallet;
	Address m_addr;

	AddressCom(WalletCom& wallet, const Address& addr)
		:	m_wallet(wallet)
		,	m_addr(addr)
	{}

	~AddressCom() {
	}

	HRESULT __stdcall get_Value(BSTR *r)
	METHOD_BEGIN {
		*r = m_addr.ToString().AllocSysString();
	} METHOD_END

	HRESULT __stdcall get_Comment(BSTR *r)
	METHOD_BEGIN {
		*r = m_addr.Comment.AllocSysString();
	} METHOD_END

	HRESULT __stdcall put_Comment(BSTR s);
};

DECIMAL ToDecimal(const pair<Int64, int>& pp) {
	DECIMAL r = { 0 };
	r.sign = pp.first<0 ? DECIMAL_NEG : 0;
	r.scale = (byte)pp.second;
	r.Lo64 = std::abs(pp.first);
	return r;
}

DECIMAL ToDecimal(const decimal64& v) {
	decimal64 vv = v<0 ? -v : v;

	DECIMAL r = { 0 };
	r.sign = v<0 ? DECIMAL_NEG : 0;
	long long ll;
	while ((ll = decimal_to_long_long(vv)) != vv) {
		r.scale++;
		vv *= 10;
	}
	r.Lo64 = ll;
	return r;
}

/*!!!R
CURRENCY ToCurrency(const decimal64& v) {
	CURRENCY r;
	r.int64 = decimal_to_long_long(v * 10000);
	return r;
}*/

class TransactionCom : public IDispatchImpl<TransactionCom, ITransaction>
	, public ISupportErrorInfoImpl<ITransaction>
{
public:
	DECLARE_STANDARD_UNKNOWN()

	BEGIN_COM_MAP(TransactionCom)
		COM_INTERFACE_ENTRY(ITransaction)
		COM_INTERFACE_ENTRY(ISupportErrorInfo)
	END_COM_MAP()

	WalletCom& m_wallet;
	WalletTx m_wtx;

	TransactionCom(WalletCom& wallet)
		:	m_wallet(wallet)
	{}

	~TransactionCom();

	HRESULT __stdcall get_Timestamp(DATE *r)
	METHOD_BEGIN {
		*r = m_wtx.Timestamp.ToOADate();
	} METHOD_END

	HRESULT __stdcall get_Comment(BSTR *r)
	METHOD_BEGIN {
		*r = m_wtx.GetComment().AllocSysString();
	} METHOD_END

	HRESULT __stdcall get_Hash(BSTR *r)
	METHOD_BEGIN {
		*r = String(EXT_STR(Hash(m_wtx))).AllocSysString();
	} METHOD_END

	HRESULT __stdcall get_Amount(DECIMAL *r);
	HRESULT __stdcall get_Confirmations(int *r);
	HRESULT __stdcall put_Comment(BSTR s);
	HRESULT __stdcall get_Address(IAddress **r);
	HRESULT __stdcall get_Fee(DECIMAL *r);
};

class AlertCom : public IDispatchImpl<AlertCom, IAlert>
	, public ISupportErrorInfoImpl<IAlert>
{
public:
	DECLARE_STANDARD_UNKNOWN()

	BEGIN_COM_MAP(AlertCom)
		COM_INTERFACE_ENTRY(IAlert)
		COM_INTERFACE_ENTRY(ISupportErrorInfo)
	END_COM_MAP()

	WalletCom& m_wallet;
	Alert m_alert;

	AlertCom(WalletCom& wallet, const Alert& alert)
		:	m_wallet(wallet)
		,	m_alert(alert)
	{}

	HRESULT __stdcall get_UntilTimestamp(DATE *r)
	METHOD_BEGIN {
		*r = m_alert.RelayUntil.ToOADate();
	} METHOD_END

	HRESULT __stdcall get_Comment(BSTR *r);
};


class CConnectData : public CONNECTDATA {
public:
	CConnectData()  {
		pUnk = 0;
	}

	~CConnectData()  {
		if (pUnk)
			pUnk->Release();
	}
};

class WalletCom : public IDispatchImpl<WalletCom, IWallet>
	, public IConnectionPointContainerImpl<WalletCom>
	, public IConnectionPointImpl<WalletCom, _IWalletEvents>
	, public ISupportErrorInfoImpl<IWallet>
	, public IIWalletEvents
{
public:
	DECLARE_STANDARD_UNKNOWN()

	BEGIN_COM_MAP(WalletCom)
		COM_INTERFACE_ENTRY(IWallet)
		COM_INTERFACE_ENTRY(IConnectionPointContainer)
		COM_INTERFACE_ENTRY(ISupportErrorInfo)
	END_COM_MAP()

	BEGIN_CONNECTION_POINT_MAP(WalletCom)
		CONNECTION_POINT_ENTRY(_IWalletEvents)
	END_CONNECTION_POINT_MAP()

	Wallet& m_wallet;

	typedef unordered_map<String, CComPtr<IAddress>> CStr2Addr;
	CStr2Addr m_str2addr;

	Int32 StateChanged;

	WalletCom(Wallet& wallet)
		:	m_wallet(wallet)
		,	StateChanged(1)
	{
		m_wallet.m_iiWalletEvents = this;
	}

	~WalletCom() {
	}

	HRESULT __stdcall get_CurrencyName(BSTR *r)
	METHOD_BEGIN {
		*r = m_wallet.Eng.ChainParams.Name.AllocSysString();
	} METHOD_END

	HRESULT __stdcall get_CurrencySymbol(BSTR *r)
	METHOD_BEGIN {
		*r = m_wallet.Eng.ChainParams.Symbol.AllocSysString();
	} METHOD_END

	HRESULT __stdcall get_Balance(DECIMAL *r)
	METHOD_BEGIN {
		*r = ToDecimal(m_wallet.Balance);
	} METHOD_END

	HRESULT __stdcall SendTo(DECIMAL amount, BSTR addr, BSTR comment) 
	METHOD_BEGIN {
		CCoinEngThreadKeeper engKeeper(&m_wallet.Eng);
		if (amount.sign || amount.Lo64==0)
			Throw(E_INVALIDARG);
		m_wallet.SendTo(make_decimal64(amount.Lo64, -amount.scale), addr, comment);
	} METHOD_END

	HRESULT __stdcall GenerateNewAddress(BSTR comment, IAddress** r)
	METHOD_BEGIN {
		CCoinEngThreadKeeper engKeeper(&m_wallet.Eng);
		CComPtr<IAddress> iA = new AddressCom(_self, m_wallet.Eng.m_cdb.GenerateNewAddress(comment).ToAddress());
		*r = iA.Detach();
	} METHOD_END

	HRESULT __stdcall get_Transactions(SAFEARRAY **r)		// Transactions in descending timestamp order
	METHOD_BEGIN {
		CCoinEngThreadKeeper engKeeper(&m_wallet.Eng);
		EXT_LOCK (Mtx) {
			if (m_saTxes.vt == VT_EMPTY) {
				vector<WalletTx> vec;
				EXT_LOCK (m_wallet.Eng.m_cdb.MtxDb) {
					SqliteCommand cmd(EXT_STR("SELECT mytxes.timestamp, mytxes.data, mytxes.blockord, mytxes.comment, value, addr160 FROM usertxes INNER JOIN mytxes ON txid=mytxes.id WHERE mytxes.netid=" << m_wallet.m_dbNetId << " ORDER BY mytxes.timestamp DESC"), m_wallet.Eng.m_cdb.m_dbWallet);
					for (DbDataReader dr=cmd.ExecuteReader(); dr.Read();) {
						WalletTx wtx;
						wtx.LoadFromDb(dr, true);
						if (!m_wallet.Eng.m_cdb.Hash160ToMyKey.count(wtx.To))
							wtx.Amount = -wtx.Amount;
						vec.push_back(wtx);
					}
				}
				m_saTxes.CreateOneDim(VT_DISPATCH, vec.size());
				for (long idx=0; idx<vec.size(); ++idx) {
					WalletTx& wtx = vec[idx];
					CComPtr<ITransaction> iTx;
					if (!Lookup(m_keyToTxCom, wtx.UniqueKey(), iTx)) {
						auto ptx = new TransactionCom(_self);
						iTx = (ITransaction*)ptx;
						m_keyToTxCom.insert(make_pair(wtx.UniqueKey(), iTx));						
					}
					TransactionCom *txCom = dynamic_cast<TransactionCom*>(iTx.P);
					txCom->m_wtx = wtx;
					m_saTxes.PutElement(&idx, static_cast<IDispatch*>(iTx));
				}
			}
			*r = COleVariant(m_saTxes).Detach().parray;
		}
	} METHOD_END

	CComPtr<IAddress> GetAddressByString(RCString s) {
		Address a(s, &Eng());
		CStr2Addr::iterator it = m_str2addr.find(s);
		if (it == m_str2addr.end())
			return m_str2addr[s] = static_cast<IAddress*>(new AddressCom(_self, a));
		return it->second;
	}

	CComPtr<IAddress> GetAddressByString(const Address& a) {
		CStr2Addr::iterator it = m_str2addr.find(a.ToString());
		if (it == m_str2addr.end())
			return m_str2addr[a.ToString()] = static_cast<IAddress*>(new AddressCom(_self, a));
		return it->second;
	}

	HRESULT __stdcall get_MyAddresses(SAFEARRAY **r)
	METHOD_BEGIN {
		CCoinEngThreadKeeper engKeeper(&m_wallet.Eng);

		COleSafeArray sa;
		vector<Address> ar = m_wallet.MyAddresses;
		sa.CreateOneDim(VT_DISPATCH, ar.size());
		for (long i=0; i<ar.size(); ++i)
			sa.PutElement(&i, GetAddressByString(ar[i]).Detach());
		*r = sa.Detach().parray;
	} METHOD_END

	HRESULT __stdcall get_Alerts(SAFEARRAY **r)
	METHOD_BEGIN {
		CCoinEngThreadKeeper engKeeper(&m_wallet.Eng);

		COleSafeArray sa;
		EXT_LOCK (m_wallet.Mtx) {
			sa.CreateOneDim(VT_DISPATCH, m_wallet.Alerts.size());
			for (long i=0; i<m_wallet.Alerts.size(); ++i)
				sa.PutElement(&i, static_cast<IAlert*>(new AlertCom(_self, *m_wallet.Alerts[i])));
		}
		*r = sa.Detach().parray;
	} METHOD_END

	HRESULT __stdcall Rescan()
	METHOD_BEGIN {
		m_wallet.Rescan();
	} METHOD_END

    HRESULT __stdcall GetAndResetStateChangedFlag(VARIANT_BOOL *r)
	METHOD_BEGIN {
		*r = bool(Interlocked::CompareExchange(StateChanged, 0, 1));
		if (*r) {
			EXT_LOCK (Mtx) {
				m_saTxes.Clear();
			}
		}
	} METHOD_END

	HRESULT __stdcall CalcFee(DECIMAL amount, DECIMAL *r)
	METHOD_BEGIN {
		CCoinEngThreadKeeper engKeeper(&m_wallet.Eng);
		*r = ToDecimal(m_wallet.CalcFee(make_decimal64(amount.Lo64, -amount.scale)));
	} METHOD_END

	HRESULT __stdcall get_Enabled(VARIANT_BOOL *r)
	METHOD_BEGIN {
		*r = m_wallet.Eng.Runned;
	} METHOD_END

	HRESULT __stdcall put_Enabled(VARIANT_BOOL r)
	METHOD_BEGIN {
		CCoinEngThreadKeeper engKeeper(&m_wallet.Eng);
		if (r) {
			m_wallet.Start();

			EXT_FOR (const Address& addr, m_wallet.MyAddresses) {
				m_str2addr[addr.ToString()] = static_cast<IAddress*>(new AddressCom(_self, addr));
			}	

			EXT_FOR (const Address& addr, m_wallet.Recipients) {
				m_str2addr[addr.ToString()] = static_cast<IAddress*>(new AddressCom(_self, addr));
			}	

		} else {
			m_wallet.Stop();
		}
	} METHOD_END

	HRESULT __stdcall get_LastBlock(int *r)
	METHOD_BEGIN {
		*r = m_wallet.Eng.BestBlockHeight();
	} METHOD_END

	HRESULT __stdcall get_MiningAllowed(VARIANT_BOOL *r)
	METHOD_BEGIN {
		*r = m_wallet.Eng.MiningAllowed();
		StateChanged = 1;
	} METHOD_END

	HRESULT __stdcall get_MiningEnabled(VARIANT_BOOL *r)
	METHOD_BEGIN {
		*r = m_wallet.MiningEnabled;
		StateChanged = 1;
	} METHOD_END

	HRESULT __stdcall put_MiningEnabled(VARIANT_BOOL r)
	METHOD_BEGIN {
		m_wallet.MiningEnabled = r;
	} METHOD_END

	HRESULT __stdcall get_Progress(float *r)
	METHOD_BEGIN {
		*r = m_wallet.Progress;
	} METHOD_END

	HRESULT __stdcall get_State(BSTR *r)
	METHOD_BEGIN {
		ostringstream os;
		if (m_wallet.Eng.CriticalException != std::exception_ptr()) {		//!!!? EXT_LOC(m_wallet.Eng.Mtx)
			try {
				std::rethrow_exception(m_wallet.Eng.CriticalException);
			} catch (RCExc ex) {
				os << ex.what();
			}
		} else {
			do {
				if (m_wallet.Eng.UpgradingDatabaseHeight) {
					os << "Upgrading Database of " << m_wallet.Eng.UpgradingDatabaseHeight << " blocks";
					break;
				}
				if (ptr<CompactThread> t = m_wallet.CompactThread) {
					os << "Compacting Database  " << Int64(t->m_i)*100/t->m_count << "%";
					break;
				}

				if (Block bestBlock = m_wallet.Eng.BestBlock()) {
					TimeSpan span = DateTime::UtcNow()-bestBlock.Timestamp;
					if (span.TotalMinutes > 60) {
						int n;
						if (span.TotalHours < 24)
							os << (n = int(span.TotalHours)) << " hour";
						else
							os << (n = span.Days) << " day";
						if (n > 1)
							os << "s";
						os << " ago";
					} else 
						os << bestBlock.Timestamp.ToLocalTime();
				}

				if (ptr<RescanThread> rt = m_wallet.RescanThread) {
					os << ", Rescanning block " << m_wallet.CurrentHeight;
				}
				if (m_wallet.MiningEnabled) {
					os << ", Mining " << setprecision(1) << fixed << m_wallet.Speed/1000000 << " MHash/s";
				}
			} while (false);
		}
		*r = String(os.str()).AllocSysString();
	} METHOD_END

	HRESULT __stdcall get_Peers(int *r)
	METHOD_BEGIN {
		EXT_LOCK (m_wallet.Eng.MtxPeers) {
			*r = m_wallet.Eng.Links.size();
		}
	} METHOD_END

	HRESULT __stdcall get_Recipients(SAFEARRAY **r)
	METHOD_BEGIN {
		COleSafeArray sa;
		vector<Address> ar = m_wallet.Recipients;
		sa.CreateOneDim(VT_DISPATCH, ar.size());
		for (long i=0; i<ar.size(); ++i)
			sa.PutElement(&i, GetAddressByString(ar[i]).Detach());
		*r = sa.Detach().parray;
	} METHOD_END

	HRESULT __stdcall AddRecipient(BSTR addr, BSTR comment, IAddress **r)
	METHOD_BEGIN {
		CCoinEngThreadKeeper engKeeper(&m_wallet.Eng);
		Address a(addr, &m_wallet.Eng);
		m_str2addr.erase(addr);
		a.Comment = comment;
		m_wallet.AddRecipient(a);
		*r = GetAddressByString(a).Detach();
	} METHOD_END

	HRESULT __stdcall RemoveRecipient(IAddress *addr)
	METHOD_BEGIN {
		CComBSTR bstr;
		OleCheck(addr->get_Value(&bstr));
		String s(bstr);
		m_wallet.RemoveRecipient(s);
		m_str2addr.erase(s);
	} METHOD_END

	HRESULT __stdcall CompactDatabase()
	METHOD_BEGIN {
		m_wallet.StartCompactDatabase();
	} METHOD_END

	HRESULT __stdcall ExportWalletToBdb(BSTR filepath)
	METHOD_BEGIN {
		m_wallet.ExportWalletToBdb(filepath);
	} METHOD_END

	HRESULT __stdcall CancelPendingTxes()
	METHOD_BEGIN {
		m_wallet.CancelPendingTxes();
	} METHOD_END
protected:
	CComClass *GetComClass() {
		return CComObjectRootBase::GetComClass();
	}

	void OnStateChanged() override {
		/*!!!
		CComPtr<IConnectionPointContainer> pCPC = this;
		CComPtr<IConnectionPoint> cp;
		OleCheck(pCPC->FindConnectionPoint(DIID__IWalletEvents, &cp));
		CComPtr<IEnumConnections> en;
		OleCheck(cp->EnumConnections(&en));
		DWORD dw;
		HRESULT hr;
		while (true) {
			CConnectData cd;
			if ((hr = en->Next(1, &cd, &dw)) != S_OK)
				break;
			CComPtr<_IWalletEvents> p = cd.pUnk;
			OleCheck(p->OnStateChangedEvent());
		}
		*/

/*!!!R		EXT_LOCK (Mtx) {
			m_saTxes.Clear();
		} */
		StateChanged = 1;
	}

private:
	mutex Mtx;
	COleSafeArray m_saTxes;
	unordered_map<String, CComPtr<ITransaction>> m_keyToTxCom;
};

HRESULT __stdcall AddressCom::put_Comment(BSTR s)
METHOD_BEGIN {
	CCoinEngThreadKeeper engKeeper(&m_wallet.m_wallet.Eng);

	m_addr.Comment = s;
	m_wallet.m_wallet.SetAddressComment(m_addr, m_addr.Comment);
} METHOD_END

class WalletAndEng : public Wallet {
	typedef Wallet base;
public:
	CComPtr<IWallet> m_iWallet;

	WalletAndEng(CoinDb& cdb, RCString name);	
	~WalletAndEng();
};

WalletAndEng::WalletAndEng(CoinDb& cdb, RCString name)
	:	base(cdb, name)
	,	m_iWallet(static_cast<IWallet*>(new WalletCom(_self)))
{
	EXT_LOCK (cdb.MtxDb) {
		SqliteCommand cmd("SELECT * FROM nets WHERE name=?", cdb.m_dbWallet);
		cmd.Bind(1, name);
		if (!cmd.ExecuteReader().Read()) {
			SqliteCommand("INSERT INTO nets (name) VALUES(?)", cdb.m_dbWallet)
				.Bind(1, name)
				.ExecuteNonQuery();
		}
	}
}

WalletAndEng::~WalletAndEng() {
}

class CoinEngCom : public IDispatchImpl<CoinEngCom, ICoinEng>
	,	public ISupportErrorInfoImpl<ICoinEng>
{
public:
	DECLARE_STANDARD_UNKNOWN()

	BEGIN_COM_MAP(CoinEngCom)
		COM_INTERFACE_ENTRY(ICoinEng)
		COM_INTERFACE_ENTRY(ISupportErrorInfo)
	END_COM_MAP()

	CoinDb m_cdb;

	vector<ptr<WalletAndEng>> m_vWalletEng;
	
	CoinEngCom() {
	}

	~CoinEngCom() {
	}

	HRESULT __stdcall Start()
	METHOD_BEGIN {
//		m_eng.Start();
	} METHOD_END

	HRESULT __stdcall Stop()
	METHOD_BEGIN {
		TRC(1, "");

		for (int i=0; i<m_vWalletEng.size(); ++i) {
			m_vWalletEng[i]->m_peng->Stop();
			m_vWalletEng[i]->Close();
		}
		m_cdb.Close();
	} METHOD_END

	HRESULT __stdcall get_Wallets(SAFEARRAY **r)
	METHOD_BEGIN {
		if (m_vWalletEng.empty()) {
			m_cdb.Load();
			vector<String> names;
			{
				String pathXml = Path::Combine(Path::GetDirectoryName(System.ExeFilePath), "coin-chains.xml");
				if (!File::Exists(pathXml))
					Throw(E_COIN_XmlFileNotFound);
#if UCFG_WIN32
				XmlDocument doc = new XmlDocument;
				doc.Load(pathXml);
				XmlNodeReader rd(doc);
#else
				ifstream ifs(pathXml.c_str());
				XmlTextReader rd(ifs);
#endif
				if (rd.ReadToDescendant("Chain")) {
					do {
						String name = rd.GetAttribute("Name");
						names.push_back(name);
					} while (rd.ReadToNextSibling("Chain"));
				}
			}
			EXT_FOR (const String& name, names) {
				m_vWalletEng.push_back(new WalletAndEng(m_cdb, name));
			}
		}
		COleSafeArray sa;
		sa.CreateOneDim(VT_DISPATCH, m_vWalletEng.size());
		for (long idx=0; idx<m_vWalletEng.size(); ++idx)
			sa.PutElement(&idx, m_vWalletEng[idx]->m_iWallet);
		*r = sa.Detach().parray;
	} METHOD_END

	HRESULT __stdcall get_WalletDatabaseExists(VARIANT_BOOL *r)
	METHOD_BEGIN {
		*r = m_cdb.WalletDatabaseExists;
	} METHOD_END

	HRESULT __stdcall get_NeedPassword(VARIANT_BOOL *r)
	METHOD_BEGIN {
		*r = m_cdb.NeedPassword;
	} METHOD_END

	HRESULT __stdcall put_Password(BSTR password)
	METHOD_BEGIN {
		m_cdb.Password = password;
	} METHOD_END

	HRESULT __stdcall put_LiteMode(VARIANT_BOOL b)
	METHOD_BEGIN {
		for (int i=0; i<m_vWalletEng.size(); ++i) {
			m_vWalletEng[i]->m_peng->Mode = b ? EngMode::Lite : EngMode::Normal;
		}
	} METHOD_END

	HRESULT __stdcall ChangePassword(BSTR oldPassword, BSTR newPassword)
	METHOD_BEGIN {
		m_cdb.ChangePassword(oldPassword, newPassword);
	} METHOD_END

	HRESULT __stdcall ImportWallet(BSTR filepath, BSTR password)
	METHOD_BEGIN {
		m_cdb.ImportWallet(filepath, password);
	} METHOD_END

	HRESULT __stdcall ExportWalletToXml(BSTR filepath)
	METHOD_BEGIN {
		m_cdb.ExportWalletToXml(filepath);
	} METHOD_END

	HRESULT __stdcall get_LocalPort(int *r)
	METHOD_BEGIN {
		*r = m_cdb.ListeningPort;
	} METHOD_END

	HRESULT __stdcall put_LocalPort(int v)
	METHOD_BEGIN {
		m_cdb.ListeningPort = (UInt16)v;
	} METHOD_END

	HRESULT __stdcall get_AppDataDirectory(BSTR *r)
	METHOD_BEGIN {
		*r = Path::GetDirectoryName(m_cdb.DbWalletFilePath).AllocSysString();
	} METHOD_END
};

TransactionCom::~TransactionCom() {
}

HRESULT __stdcall TransactionCom::get_Amount(DECIMAL *r)
METHOD_BEGIN {
	CCoinEngThreadKeeper engKeeper(&m_wallet.m_wallet.Eng);
	*r = ToDecimal(m_wtx.GetDecimalAmount());
} METHOD_END

HRESULT __stdcall TransactionCom::get_Fee(DECIMAL *r)
METHOD_BEGIN {
	DBG_LOCAL_IGNORE_NAME(E_COIN_TxNotFound, ignE_COIN_TxNotFound);
	
	CCoinEngThreadKeeper engKeeper(&m_wallet.m_wallet.Eng);
	*r = ToDecimal(m_wtx.GetDecimalFee());
} METHOD_END

HRESULT TransactionCom::get_Confirmations(int *r)
METHOD_BEGIN {
	*r = 0;
	Block bestBlock = m_wallet.m_wallet.Eng.BestBlock();
	if (m_wtx.Height >= 0 && bestBlock)
		*r = bestBlock.Height - m_wtx.Height+1;
} METHOD_END

HRESULT TransactionCom::put_Comment(BSTR s)
METHOD_BEGIN {
	m_wtx.Comment = s;
	EXT_LOCK (m_wallet.m_wallet.Eng.m_cdb.MtxDb) {
		SqliteCommand(EXT_STR("UPDATE mytxes SET comment=? WHERE netid=" << m_wallet.m_wallet.m_dbNetId << " AND hash=?"), m_wallet.m_wallet.Eng.m_cdb.m_dbWallet)
			.Bind(1, m_wtx.Comment)
			.Bind(2, Hash(m_wtx))
			.ExecuteNonQuery();
	}
} METHOD_END

HRESULT __stdcall TransactionCom::get_Address(IAddress **r)
METHOD_BEGIN {
	CCoinEngThreadKeeper engKeeper(&m_wallet.m_wallet.Eng);

	*r = m_wallet.GetAddressByString(m_wtx.To.ToString()).Detach();
} METHOD_END


HRESULT __stdcall AlertCom::get_Comment(BSTR *r)
METHOD_BEGIN {
	*r = (m_wallet.m_wallet.Eng.ChainParams.Name + " Alert: " + m_alert.Comment + " " + m_alert.StatusBar).AllocSysString();
} METHOD_END


} // Coin::

using namespace Coin;


extern "C" ICoinEng * __stdcall CoinCreateObj() {
	AFX_MANAGE_STATE(AfxGetStaticModuleState());

	CComPtr<ICoinEng> iCoin = new CoinEngCom;
	return iCoin.Detach();
}
