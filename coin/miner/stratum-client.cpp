/*######     Copyright (c) 1997-2013 Ufasoft  http://ufasoft.com  mailto:support@ufasoft.com,  Sergey Pavlov  mailto:dev@ufasoft.com #######################################
#                                                                                                                                                                          #
# This program is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation;  #
# either version 3, or (at your option) any later version. This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the      #
# implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details. You should have received a copy of the GNU #
# General Public License along with this program; If not, see <http://www.gnu.org/licenses/>                                                                               #
##########################################################################################################################################################################*/

#include <el/ext.h>

#include "miner.h"


namespace Coin {

ptr<MinerBlock> MinerBlock::FromStratumJson(const VarValue& json) {
	ptr<MinerBlock> r = new MinerBlock;
	r->JobId = json[0];

	Blob blob = Swab32(Blob::FromHexString(json[5].ToString()));
	if (blob.Size != 4)
		Throw(E_EXT_Protocol_Violation);
	r->Ver = letoh(*(UInt32*)blob.constData());
		
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
		Throw(E_EXT_Protocol_Violation);
	r->DifficultyTargetBits = letoh(*(UInt32*)blob.constData());

	if ((blob = Swab32(Blob::FromHexString(json[7].ToString()))).Size != 4)
		Throw(E_EXT_Protocol_Violation);
	r->SetTimestamps(DateTime::from_time_t(letoh(*(UInt32*)blob.constData())));

	return r;
}

StratumClient::StratumClient(Coin::BitcoinMiner& miner)
	:	ConnectionClient(miner)
	,	State(0)
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
	UInt32 ts = to_time_t(mblock.Timestamp);
	UInt32 nonce = wd->Nonce;
#endif
	Call("mining.submit", params, task);
}

void StratumClient::SetDifficulty(double difficulty) {
	HashTarget = HashValue::FromShareDifficulty(difficulty, Miner.HashAlgo);
}

void StratumClient::OnLine(RCString line) {
	VarValue v = ParseJson(line);
	JsonRpcRequest req;
	if (JsonRpc.TryAsRequest(v, req)) {
		if (req.Method == "mining.set_difficulty") {
			SetDifficulty(req.Params[0].ToDouble());
		} else {
			if (State < 1)
				Throw(E_EXT_Protocol_Violation);
			if (req.Method == "mining.notify") {
				EXT_LOCK (MtxData) {
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
						*Miner.m_pTraceStream << "New Stratum data with Clean                    " << endl;
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
				Throw(E_EXT_Protocol_Violation);
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
			if (msg.IsEmpty() && resp.ErrorVal.type()==VarType::Array && resp.ErrorVal.size()>=2 && resp.ErrorVal[1].type()==VarType::String)
				msg = resp.ErrorVal[1].ToString();
			Interlocked::Increment(Miner.SubmittedCount);
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

