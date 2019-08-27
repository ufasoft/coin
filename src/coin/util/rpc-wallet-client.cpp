/*######   Copyright (c) 2011-2019 Ufasoft  http://ufasoft.com  mailto:support@ufasoft.com,  Sergey Pavlov  mailto:dev@ufasoft.com ####
#                                                                                                                                     #
# 		See LICENSE for licensing information                                                                                         #
#####################################################################################################################################*/

#include <el/ext.h>

#include <el/libext/ext-net.h>
#include <el/inet/http.h>
#if UCFG_WIN32
#	include <el/libext/win32/ext-win.h>
#endif
#include <el/inet/json-rpc.h>
using namespace Ext::Inet;


#include "wallet-client.h"

namespace Coin {

using namespace Ext::Inet;

static atomic<int> s_aTriedPort;

static bool InitTriedPort() {
	s_aTriedPort = 12000;
	return true;
}

static bool s_initTriedPort = InitTriedPort();


decimal64 AmountToDecimal(const VarValue& v) {
	return make_decimal64(int64_t(v.ToDouble() * 100000000), -8);						//!!! not precise;
}

uint16_t GetFreePort() {
	for (uint16_t port; (port = uint16_t(++s_aTriedPort))<20000;) {
		Socket sock(AddressFamily::InterNetwork);
		try {
			sock.Bind(IPEndPoint(IPAddress::Any, port));
			return port;
		} catch (RCExc) {
		}
	}
	Throw(HRESULT_FROM_WIN32(ERROR_NO_SYSTEM_RESOURCES));
}

WalletTxInfo ToTxInfo(const VarValue& v) {
	WalletTxInfo r;
	if (VarValue vAddress = v["address"])
		r.Address = vAddress.ToString();
	r.HashTx = HashValue(v["txid"].ToString());
	r.Confirmations = int32_t(v["confirmations"].ToInt64());
	r.Timestamp = DateTime::from_time_t(v["time"].ToInt64());
	if (r.Confirmations) {
		r.HashBlock = HashValue(v["blockhash"].ToString());
	}
	r.Amount = AmountToDecimal(v["amount"]);
	r.Generated = v.HasKey("generated") && v["generated"].ToBool();
	if (VarValue vDetails = v["details"]) {
		for (size_t i=0; i<vDetails.size(); ++i) {
			VarValue vd = vDetails[i];
			WalletTxInfo::TxDetails d;
			d.Account = vd["account"].ToString();
			d.Address = vd["address"].ToString();
			d.IsSend = vd["category"].ToString() == "send";
			d.Amount = AmountToDecimal(vd["amount"]);
			r.Details.push_back(d);
		}
	}
	return r;
}

class RpcWalletClient : public IWalletClient {
	Socket Sock;
	NetworkStream Stm;
	WebClient m_wc;

	mutex MtxRpc;
	Ext::Inet::JsonRpc JsonRpc;

public:
	Process m_process;
	bool HasSubmitBlockMethod;

	RpcWalletClient()
		: Stm(Sock)
		, HasSubmitBlockMethod(true)
	{
		m_wc.CacheLevel = RequestCacheLevel::BypassCache;
		m_wc.Credentials.UserName = Login;
		m_wc.Credentials.Password = Password;
	}

	void Start() override {
		if (!RpcUrl.get_Port())
			RpcUrl = Uri("http://127.0.0.1:" + Convert::ToString(GetFreePort()));
		else {
			try {
				DBG_LOCAL_IGNORE_CONDITION(errc::connection_refused);

				Call("getinfo");

				if (!PathDaemon.empty()) {
					try {
						vector<Process> ps = Process::GetProcessesByName(PathDaemon.stem().c_str());	//!!! incorrect process can be selected and then terminated
						if (!ps.empty())
							m_process = ps.back();
					} catch (RCExc) {
					}
				}
				return;
			} catch (system_error& DBG_PARAM(ex)) {
				TRC(1, ex.code() << " " << ex.what());
			}
		}

		if (PathDaemon.empty()) {
			LOG("Currency daemon for " << Name << " is unavailable");
			return;
		}
		if (m_process) {
			if (!m_process.HasExited)
				return;
			TRC(2, "Process " << m_process.get_ID() << " exited with code " << m_process.get_ExitCode());
		}

		String exeFilePath = System.get_ExeFilePath();
		ostringstream os;
		create_directories(DataDir);
		Listen ? (os << " -port=" << GetFreePort()) : (os << " -listen=0");
		os << " -rpcport=" << RpcUrl.get_Port()
			<< " -rpcallowip=127.0.0.1"
			<< " -rpcuser=" << (!Login.empty() ? Login : RpcUrl.UserName)
			<< " -rpcpassword=" << (!Password.empty() ? Password : RpcUrl.Password)
//			<< " -daemon"
			<< " -server"
			<< (IsTestNet ? " -testnet" : "")
			<< " -datadir=\"" << DataDir << "\"";
		if (EnableNotifications) {
			os << " -blocknotify=\"" << exeFilePath << " " << EpApi << " blocknotify " << Name << " %s\""
			<< " -alertnotify=\"" << exeFilePath << " " << EpApi << " alertnotify " << Name << " %s\"";
			if (WalletNotifications)
				os << " -walletnotify=\"" << exeFilePath << " " << EpApi <<  " walletnotify " << Name << " %s\"";
		}
		if (!Args.empty())
			os << " " << Args;
		ProcessStartInfo psi;
		psi.Arguments = os.str();
		psi.FileName = PathDaemon;
		psi.EnvironmentVariables["ComSpec"] = exeFilePath;										// to avoid console window
		m_process = Process::Start(psi);
	}

	void Stop() override {
		TRC(2, "Stopping " << PathDaemon);
		try {
			DBG_LOCAL_IGNORE_CONDITION(errc::connection_refused);

			Call("stop");
		} catch (RCExc) {
			if (m_process)
				m_process.Kill();
		}
	}

	void StopOrKill() override {
		try {
			Stop();
			if (m_process)
				m_process.WaitForExit(10000);
		} catch (RCExc DBG_PARAM(ex)) {
			TRC(1, ex.what());
		}
		if (m_process && !m_process.HasExited) {
			try {
				m_process.Kill();
			} catch (RCExc DBG_PARAM(ex)) {
				TRC(1, ex.what());
			}
		}
		TRC(2, "m_process = nullptr");
		m_process = Process();
	}

	int GetBestHeight() override {
		return (int)Call("getblockcount").ToInt64();
	}

	HashValue GetBlockHash(uint32_t height) override {
		return HashValue(Call("getblockhash", (int)height).ToString());
	}

	BlockInfo GetBlock(const HashValue& hashBlock) override {
		BlockInfo r;
		VarValue v = Call("getblock", EXT_STR(hashBlock));
		r.Version = (int32_t)v["version"].ToInt64();
		r.Timestamp = DateTime::from_time_t(v["time"].ToInt64());
		r.Hash = HashValue(v["hash"].ToString());
		if (VarValue vpbh = v["previousblockhash"])
			r.PrevBlockHash = HashValue(vpbh.ToString());
		r.MerkleRoot = HashValue(v["merkleroot"].ToString());
		r.Height = (int)v["height"].ToInt64();
		r.Confirmations = (int)v["confirmations"].ToInt64();
		r.Difficulty = v["difficulty"].ToDouble();
		VarValue vtx = v["tx"];
		r.HashTxes.resize(vtx.size());
		for (size_t i=0; i<r.HashTxes.size(); ++i)
			r.HashTxes[i] = HashValue(vtx[i].ToString());
		return r;
	}

	double GetDifficulty() override {
		return Call("getdifficulty").ToDouble();
	}

	WalletTxInfo GetTransaction(const HashValue& hashTx) override {
		return ToTxInfo(Call("gettransaction", EXT_STR(hashTx)));
	}

	SinceBlockInfo ListSinceBlock(const HashValue& hashBlock) override {
		SinceBlockInfo	r;
		VarValue v = !hashBlock ? Call("listsinceblock") : Call("listsinceblock", EXT_STR(hashBlock));
		r.LastBlock = HashValue(v["lastblock"].ToString());
		VarValue vtx = v["transactions"];
		r.Txes.resize(vtx.size());
		for (int i=0; i<r.Txes.size(); ++i)
			r.Txes[i] = ToTxInfo(vtx[i]);
		return r;
	}

	String GetAccountAddress(RCString account) override {
		return Call("getaccountaddress", account).ToString();
	}

	String GetNewAddress(RCString account) override {
		return (!!account ? Call("getnewaddress", account) : Call("getnewaddress")).ToString();
	}

	HashValue SendToAddress(RCString address, decimal64 amount, RCString comment) override {
		return HashValue(Call("sendtoaddress", address, decimal_to_double(amount), comment).ToString());
	}

	ptr<MinerBlock> GetBlockTemplate(const vector<String>& capabilities) override {
		VarValue caps;
		for (int i = 0; i < capabilities.size(); ++i)
			caps.Set(i, capabilities[i]);
		VarValue par;
		par.Set("capabilities", caps);
		VarValue jr = Call("getblocktemplate", par);
		return MinerBlock::FromJson(jr);
	}

	void ProcessSubmitResult(const VarValue& v) {
		switch (v.type()) {
		case VarType::Null:
			break;
		case VarType::String:
			{
				String rej = v.ToString();
				throw system_error(SubmitRejectionCode(rej), coin_category(), rej);
			}
		case VarType::Map:
			throw system_error(SubmitRejectionCode("rejected"), coin_category(), String::Join(", ", v.Keys()));
		default:
			Throw(errc::invalid_argument);
		}
	}

	void SubmitBlock(RCSpan data, RCString workid) override {
		String sdata = EXT_STR(data);
		if (HasSubmitBlockMethod) {
			try {
				DBG_LOCAL_IGNORE_CONDITION(ExtErr::JSON_RPC_MethodNotFound);
				VarValue par;
				if (!workid.empty())
					par.Set("workid", workid);
				ProcessSubmitResult(Call("submitblock", sdata, par));
				return;
			} catch (const system_error& ex) {
				if (ex.code() != json_rpc_errc::MethodNotFound) {
					TRC(1, ex.what());
					throw;
				}
				HasSubmitBlockMethod = false;
			}
		}
		if (!HasSubmitBlockMethod) {
			VarValue par;
			par.Set("data", sdata);
			ProcessSubmitResult(Call("getblocktemplate", par));
		}
	}

	void GetWork(RCSpan data) override {
		ProcessSubmitResult(Call("getwork", EXT_STR(data)));
	}
protected:
	WebClient GetWebClient() {
		return m_wc;
	}

	VarValue Call(RCString method, const vector<VarValue>& params = vector<VarValue>()) {
		WebClient wc = GetWebClient();
		wc.Proxy = nullptr;		//!!!?
		wc.CacheLevel = RequestCacheLevel::BypassCache;
		wc.Credentials.UserName = !Login.empty() ? Login : RpcUrl.UserName;
		wc.Credentials.Password = !Password.empty() ? Password : RpcUrl.Password;

		String sjson;
		try {
			DBG_LOCAL_IGNORE_CONDITION_OBJ(error_condition(500, http_category()));

			sjson = wc.UploadString(RpcUrl.ToString(), JsonRpc.Request(method, params));
		} catch (WebException& ex) {
			if (ex.code() == HttpStatusCode::Unauthorized)
				throw;
			sjson = ex.Result;
		}
#ifdef X_DEBUG//!!!D
		cout << sjson << endl;
#endif

		EXT_LOCKED(MtxHeaders, Headers = wc.get_ResponseHeaders());
/*!!!R			XStratum = h.Get("X-Stratum");
			XSwitchTo = h.Get("X-Switch-To");
			XHostList = h.Get("X-Host-List");*/

		return JsonRpc.ProcessResponse(ParseJson(sjson));
	}

	VarValue Call(RCString method, const VarValue& v0) {
		return Call(method, vector<VarValue>(1, v0));
	}

	VarValue Call(RCString method, const VarValue& v0, const VarValue& v1) {
		vector<VarValue> params(2);
		params[0] = v0;
		params[1] = v1;
		return Call(method, params);
	}

	VarValue Call(RCString method, const VarValue& v0, const VarValue& v1, const VarValue& v2) {
		vector<VarValue> params(3);
		params[0] = v0;
		params[1] = v1;
		params[2] = v2;
		return Call(method, params);
	}

	VarValue Call(RCString method, const String& v0) {
		return Call(method, VarValue(v0));
	}

	VarValue Call(RCString method, const String& v0, const String& v1) {
		return Call(method, VarValue(v0), VarValue(v1));
	}
private:
	friend ptr<IWalletClient> CreateRpcWalletClient(RCString name, const path& pathDaemon, WebClient *pwc);
};


ptr<IWalletClient> CreateRpcWalletClient(RCString name, const path& pathDaemon, WebClient *pwc) {
	ptr<RpcWalletClient> r = new RpcWalletClient;
	if (pwc)
		r->m_wc = *pwc;
	r->PathDaemon = pathDaemon;
	r->Name = name;
	return r.get();
}

} // Coin::
