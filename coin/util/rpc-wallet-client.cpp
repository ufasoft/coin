#include <el/ext.h>

#include <el/libext/ext-net.h>
#include <el/libext/ext-http.h>
#if UCFG_WIN32
#	include <el/libext/win32/ext-win.h>
#endif
#include <el/inet/json-rpc.h>
using namespace Ext::Inet;


#include "wallet-client.h"

namespace Coin {

static volatile Int32 s_triedPort = 12000;

decimal64 AmountToDecimal(const VarValue& v) {
	return make_decimal64(Int64(v.ToDouble() * 100000000), -8);						//!!! not precise;
}

UInt16 GetFreePort() {
	for (UInt16 port; (port=(UInt16)Interlocked::Increment(s_triedPort))<20000;) {
		Socket sock;
		sock.Open();
		try {
			sock.Bind(IPEndPoint(IPAddress::Any, port));
			return port;
		} catch (RCExc) {
		}	
	}
	Throw(HRESULT_FROM_WIN32(ERROR_NO_SYSTEM_RESOURCES));
}

TxInfo ToTxInfo(const VarValue& v) {
	TxInfo r;
	r.Address = v["address"].ToString();
	r.HashTx = HashValue(v["txid"].ToString());
	r.Confirmations = Int32(v["confirmations"].ToInt64());
	r.Timestamp = DateTime::from_time_t(v["time"].ToInt64());
	if (r.Confirmations) {
		r.HashBlock = HashValue(v["blockhash"].ToString());
	}
	r.Amount = AmountToDecimal(v["amount"]);
	r.Generated = v.HasKey("generated") && v["generated"].ToBool();
	if (VarValue vDetails = v["details"]) {
		for (size_t i=0; i<vDetails.size(); ++i) {
			VarValue vd = vDetails[i];
			TxInfo::TxDetails d;
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
public:
	String PathDaemon;
	String Login, Password;
	Process m_process;
	bool HasSubmitBlockMethod;

	RpcWalletClient()
		:	Stm(Sock)
		,	Login("u")
		,	Password("p")
		,	HasSubmitBlockMethod(true)
	{
		m_wc.CacheLevel = RequestCacheLevel::BypassCache;
		m_wc.Credentials.UserName = Login;
		m_wc.Credentials.Password = Password;
	}	
	
	void Start() override {
		if (!EpRpc.Port)
			EpRpc = IPEndPoint(IPAddress::Loopback , GetFreePort());

		try {
			DBG_LOCAL_IGNORE_WIN32(WSAECONNREFUSED);
			Call("getinfo");			

			try {
				vector<Process> ps = Process::GetProcessesByName(Path::GetFileNameWithoutExtension(PathDaemon));	//!!! incorrect process can be selected and then terminated
				if (!ps.empty())
					m_process = ps.back();
			} catch (RCExc) {
			}
			return;
		} catch (RCExc) {
		}

		String exeFilePath = System.ExeFilePath;
		ostringstream os;
		Directory::CreateDirectory(DataDir);
		Listen ? (os << " -port=" << GetFreePort()) : (os << " -listen=0");
		os << " -rpcport=" << EpRpc.Port
			<< " -rpcallowip=127.0.0.1"
			<< " -rpcuser=" << Login
			<< " -rpcpassword=" << Password
			<< " -server"
			<< (IsTestNet ? " -testnet" : "")
			<< " -datadir=\"" << DataDir << "\""
			<< " -blocknotify=\"" << exeFilePath << " " << EpApi << " blocknotify " << Name << " %s\""
			<< " -alertnotify=\"" << exeFilePath << " " << EpApi << " alertnotify " << Name << " %s\"";		
		if (WalletNotifications)
			os << " -walletnotify=\"" << exeFilePath << " " << EpApi <<  " walletnotify " << Name << " %s\"";
		ProcessStartInfo psi;
		psi.Arguments = os.str();
		psi.FileName = PathDaemon;
		psi.EnvironmentVariables["ComSpec"] = exeFilePath;										// to avoid console window
		TRC(2, "Starting: " << psi.FileName << " " << psi.Arguments);
		m_process = Process::Start(psi);
	}

	void Stop() override {
		TRC(2, "Stopping " << PathDaemon);
		try {
			DBG_LOCAL_IGNORE_WIN32(ERROR_INTERNET_CANNOT_CONNECT);

			Call("stop");
		} catch (RCExc) {
			if (m_process)
				m_process.Kill();
		}
	}

	void StopOrKill() override {
		try {
			Stop();
			m_process.WaitForExit(10000);
		} catch (RCExc ex) {
			TRC(1, ex.what());
		}
		if (m_process && !m_process.HasExited) {
			try {
				m_process.Kill();
			} catch (RCExc ex) {
				TRC(1, ex.what());
			}
		}
		TRC(2, "m_process = nullptr");
		m_process = Process();
	}

	int GetBestHeight() override {
		return (int)Call("getblockcount").ToInt64();
	}

	HashValue GetBlockHash(UInt32 height) override {
		return HashValue(Call("getblockhash", (int)height).ToString());
	}

	BlockInfo GetBlock(const HashValue& hashBlock) {
		BlockInfo r;
		VarValue v = Call("getblock", EXT_STR(hashBlock));
		r.Version = (Int32)v["version"].ToInt64();
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

	TxInfo GetTransaction(const HashValue& hashTx) {
		return ToTxInfo(Call("gettransaction", EXT_STR(hashTx)));
	}

	SinceBlockInfo ListSinceBlock(const HashValue& hashBlock) override {
		SinceBlockInfo	r;
		VarValue v = hashBlock==HashValue() ? Call("listsinceblock") : Call("listsinceblock", EXT_STR(hashBlock));
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

	HashValue SendToAddress(RCString address, decimal64 amount, RCString comment) override {
		return HashValue(Call("sendtoaddress", address, decimal_to_double(amount), comment).ToString());
	}

	ptr<MinerBlock> GetBlockTemplate(const vector<String>& capabilities) override {
		VarValue caps;
		for (int i=0; i<capabilities.size(); ++i)
			caps.Set(i, capabilities[i]);
		VarValue par;
		par.Set("capabilities", caps);
		VarValue jr = Call("getblocktemplate", par);
		return MinerBlock::FromJson(jr);
	}

	void SubmitBlock(const ConstBuf& data) {
		String sdata = EXT_STR(data);
		if (HasSubmitBlockMethod) {
			try {
				DBG_LOCAL_IGNORE_NAME(E_EXT_JSON_RPC_MethodNotFound, ignE_EXT_JSON_RPC_MethodNotFound);
				Call("submitblock", sdata);
			} catch (const JsonRpcExc& ex) {
				TRC(1, ex.what());
				if (ex.HResult == E_EXT_JSON_RPC_MethodNotFound)
					HasSubmitBlockMethod = false;
				else
					throw;
			}
		}
		if (!HasSubmitBlockMethod) {
			VarValue par;
			par.Set("data", sdata);
			Call("getblocktemplate", par);
		}
	}

	String GetNewAddress(RCString account) {
		return (!!account ? Call("getnewaddress", account) : Call("getnewaddress")).ToString();
	}
protected:
	WebClient GetWebClient() {
		return m_wc;
	}

	VarValue Call(RCString method, const vector<VarValue>& params = vector<VarValue>()) {
		String url = EXT_STR("http://" << EpRpc);
		Ext::WebClient wc = GetWebClient();
		String sjson;
		try {
			DBG_LOCAL_IGNORE_NAME(MAKE_HRESULT(SEVERITY_ERROR, FACILITY_HTTP, 500), ignoreHttp500);
			sjson = wc.UploadString(url, JsonRpc.Request(method, params));
		} catch (WebException& ex) {
			sjson = StreamReader(ex.Response.GetResponseStream()).ReadToEnd();
		}

		VarValue v = ParseJson(sjson);
		JsonResponse resp = JsonRpc.Response(v);
		if (resp.Success)
			return resp.Result;
		JsonRpcExc exc(resp.Code);
		exc.m_message = resp.JsonMessage;
		exc.Data = resp.Data;
		throw exc;		
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
	Socket Sock;
	NetworkStream Stm;
	WebClient m_wc;
	Ext::WebClient WebClient;

	mutex MtxRpc;
	Ext::Inet::JsonRpc JsonRpc;
};


ptr<IWalletClient> CreateRpcWalletClient(RCString name, RCString pathDaemon) {
	ptr<RpcWalletClient> r = new RpcWalletClient;
	r->PathDaemon = pathDaemon;
	r->Name = name;
	return r.get();
}

} // Coin::
