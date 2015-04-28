#include <el/ext.h>

#include "miner.h"


namespace Coin {

ptr<MinerBlock> MinerBlock::FromStratumJson(const VarValue& json) {
	ptr<MinerBlock> r = new MinerBlock;
	r->JobId = json[0];

	Blob blob = Swab32(Blob::FromHexString(json[5].ToString()));
	if (blob.Size != 4)
		Throw(ExtErr::Protocol_Violation);
	r->Ver = letoh(*(uint32_t*)blob.constData());
		
	r->PrevBlockHash = HashValue(Swab32(Blob::FromHexString(json[1].ToString())));

	r->Coinb1 = Blob::FromHexString(json[2].ToString());
	r->Coinb2 = Blob::FromHexString(json[3].ToString());

	r->MerkleBranch.Index = 0;
	r->MerkleBranch.m_h2 = &HashValue::Combine;
	VarValue vMerk = json[4];
	r->MerkleBranch.Vec.resize(vMerk.size());
	for (int i=0; i<r->MerkleBranch.Vec.size(); ++i)
		r->MerkleBranch.Vec[i] = HashValue(Blob::FromHexString(vMerk[i].ToString()));
		
	if ((blob = Swab32(Blob::FromHexString(json[6].ToString()))).Size != 4)
		Throw(ExtErr::Protocol_Violation);
	r->DifficultyTargetBits = letoh(*(uint32_t*)blob.constData());

	if ((blob = Swab32(Blob::FromHexString(json[7].ToString()))).Size != 4)
		Throw(ExtErr::Protocol_Violation);
	r->SetTimestamps(DateTime::from_time_t(letoh(*(uint32_t*)blob.constData())));

	return r;
}

StratumClient::StratumClient(Coin::BitcoinMiner& miner)
	:	ConnectionClient(miner)
	,	State(0)
	,	CurrentDifficulty(1)
{
	W.NewLine = "\n";
	m_owner = miner.m_tr;
	StackSize = 128*1024;
}

void StratumClient::Call(RCString method, const vector<VarValue>& params, ptr<StratumTask> task) {
	if (!task)
		task = new StratumTask(State);
	Send(JsonRpc.Request(method, params, task));
}

void StratumClient::Submit(BitcoinWorkData *wd) {
	HashValue merkle = HashValue(ConstBuf(wd->Data.constData()+36, 32));
	Coin::MinerBlock& mblock = *wd->MinerBlock;
	/*!!!R
	BitcoinMiner::CLastBlockCache::iterator it = Miner.m_lastBlockCache.find(merkle);
	if (it == Miner.m_lastBlockCache.end())
		return;
	Coin::MinerBlock& mblock = it->second.first;
	*/

	vector<VarValue> params;
	params.push_back(Miner.Login);
	params.push_back(mblock.JobId);
	params.push_back(String(EXT_STR(mblock.ExtraNonce2)));
	params.push_back(Convert::ToString(to_time_t(mblock.Timestamp), "X8"));
	params.push_back(Convert::ToString(wd->Nonce, "X8"));
	ptr<StratumTask> task = new StratumTask(State);
	task->m_wd = wd;
#ifdef X_DEBUG//!!!D
	uint32_t ts = to_time_t(mblock.Timestamp);
	uint32_t nonce = wd->Nonce;
#endif
	Call("mining.submit", params, task);
}

void StratumClient::SetDifficulty(double difficulty) {
	HashTarget = HashValue::FromShareDifficulty(CurrentDifficulty=difficulty, Miner.HashAlgo);
}

void StratumClient::OnLine(RCString line) {
	VarValue v = ParseJson(line);
	JsonRpcRequest req;
	if (JsonRpc.TryAsRequest(v, req)) {
		if (req.Method == "mining.set_difficulty") {
			SetDifficulty(req.Params[0].ToDouble());
		} else {
			if (State < 1)
				Throw(ExtErr::Protocol_Violation);
			if (req.Method == "mining.notify") {
				EXT_LOCK (MtxData) {
#ifdef X_DEBUG//!!!D
					ExtraNonce1 = Blob::FromHexString("f8003a06");
					req.Params = ParseJson("{\"params\": [\"153c\", \"6cbbcc3a33d13a9059303cdde12d6ddecfb5573bb39595051e50422d00000000\",	\"01000000010000000000000000000000000000000000000000000000000000000000000000ffffffff270319a108062f503253482f048aea3b5508\",          \"0d2f7374726174756d506f6f6c2f0000000001fe0db062000000001976a91448b925a28f91cd8931cfa3c72698026389b91df788ac00000000\", [], \"00000070\", \"1c1eaa62\", \"553bea8a\", true], \"id\": null, \"method\": \"mining.notify\"}")["params"];
#endif

					MinerBlock = MinerBlock::FromStratumJson(req.Params);
					MinerBlock->Algo = Miner.HashAlgo;
					MinerBlock->HashTarget = HashTarget;
					MinerBlock->ExtraNonce1 = ExtraNonce1;
					MinerBlock->ExtraNonce2 = ExtraNonce2;

					if (req.Params[8].ToBool()) {
						EXT_LOCK (Miner.m_csCurrentData) {
							Miner.TaskQueue.clear();
							Miner.WorksToSubmit.clear();
						}
						*Miner.m_pTraceStream << "New Stratum data with Clean. Diff: " << CurrentDifficulty << "                   " << endl;
					}
				}
				Miner.m_evGetWork.Set();
				TRC(2, "New Stratum data");
			}
		}
	} else {
		JsonResponse resp = JsonRpc.Response(v);
		String method = resp.Request->Method;
		if (method == "mining.subscribe") {
			ExtraNonce1 = Blob::FromHexString(resp.Result[1].ToString());
			int cbExtraNonce2 = (int)resp.Result[2].ToInt64();
			if (cbExtraNonce2 == 0)
				Throw(ExtErr::Protocol_Violation);
			ExtraNonce2 = Blob(0, cbExtraNonce2);
			++State;
			vector<VarValue> params;
			params.push_back(Miner.Login);
			params.push_back(Miner.Password);
			Call("mining.authorize", params);
		} else if (method == "mining.authorize") {
			if (resp.Result != VarValue(true)) {
				m_bStop = true;
				Throw(HRESULT_FROM_WIN32(ERROR_INVALID_PASSWORD));
			}
		} else if (method == "mining.submit") {	
			StratumTask *task = (StratumTask*)resp.Request.get();
			String msg = resp.JsonMessage;
			if (msg.empty() && resp.ErrorVal.type()==VarType::Array && resp.ErrorVal.size()>=2 && resp.ErrorVal[1].type()==VarType::String)
				msg = resp.ErrorVal[1].ToString();
			++Miner.aSubmittedCount;
			Miner.Print(*task->m_wd, resp.Success, msg);
		} else {
			TRC(2, "Unknown JSON-RPC method: " << method);
		}
	}
}

void StratumClient::Execute() {
	Name = "Stratum Thread";

	while (!m_bStop) {
		SetDifficulty(1);
		try {
			SocketKeeper sockKeeper(_self, Tcp.Client);
			TRC(1, "EpServer = " << EpServer);
			Tcp.Client.Connect(EpServer);
			Miner.m_msWait = NORMAL_WAIT;
			vector<VarValue> args;
//!!!			args.push_back(Miner.UserAgentString);
			Call("mining.subscribe", args);
			base::Execute();
		} catch (RCExc ex) {
			*Miner.m_pTraceStream << ex.what() << endl;
			if (!m_bStop)
				Sleep(exchange(Miner.m_msWait, std::min(MAX_WAIT, Miner.m_msWait*2)));
		}
	}
}

} // Coin::

