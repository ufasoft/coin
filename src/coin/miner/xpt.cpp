/*######   Copyright (c) 2013-2019 Ufasoft  http://ufasoft.com  mailto:support@ufasoft.com,  Sergey Pavlov  mailto:dev@ufasoft.com ####
#                                                                                                                                     #
# 		See LICENSE for licensing information                                                                                         #
#####################################################################################################################################*/

#include <el/ext.h>

#include "miner-share.h"

namespace Coin {

// params
TimeSpan XPT_PING_PERIOD = TimeSpan::FromMinutes(1);

void AuthXptMessage::ProcessMsg(P2P::Link& link) {	Throw(E_NOTIMPL); }
void SubmitShareXptMessage::ProcessMsg(P2P::Link& link) {	Throw(E_NOTIMPL); }

const size_t MAX_JOB_QUEUE_SIZE = 400;

static XptClient& ToClient(P2P::Link& link) {
	return static_cast<XptClient&>(link);
}

void Workdata1XptMessage::ProcessMsg(P2P::Link& link) {
	XptClient& client = ToClient(link);
	EXT_LOCK (client.MtxData) {
		if (client.ProtocolVersion != 4) {
			ostringstream os;
			os << Clock::now().ToLocalTime() << " WorkData. Height: " << int(MinerBlock->Height) << " tx count: " << MinerBlock->Txes.size()-1;
			switch (client.Miner.HashAlgo) {
			case HashAlgo::Prime:
				os <<  "  Diff: " << setprecision(5) << double(BitsForShare)/0x1000000 << "/" << double(MinerBlock->DifficultyTargetBits)/0x1000000 << (EarnedShareValue!=0 ? EXT_STR(", ShareValue: " << EarnedShareValue) : String());
				break;
			default:
				;
				//!!!TODO calc difficulty

			}
			os << "\n";
			*client.Miner.m_pTraceStream << os.str() << flush;
			
			ShareBundle b; // default template
			if (client.Miner.HashAlgo == HashAlgo::Prime) {
				if (b.SieveSizeMin > MAX_SIEVE_SIZE) {
					*client.Miner.m_pTraceStream << EXT_STR("MinSieveSize: " << b.SieveSizeMin << " violates client's max " << MAX_SIEVE_SIZE << "\n") << flush;
					Throw(E_FAIL);
				}
			}
			client.Miner.MaxHeight = max((uint32_t)client.Miner.MaxHeight, MinerBlock->Height);
			if (Merkles.empty()) {
				MinerBlock->SetTimestamps(Timestamp);
				MinerBlock->HashTarget = HashTargetShare;

				struct TxHasher {
					const Coin::MinerBlock *minerBlock;
					HashValue operator()(const MinerTx& mtx, int n) const {
						return mtx.Hash;
					}
				} txHasher ={ MinerBlock.get() };
#ifdef X_DEBUG//!!!D
				HashValue h1("a747369c975f0d5186d7594ec695a7ccbe6e5219ef55c01a5bfe7cccd4670d5b");
				HashValue h2("418bcb30996c3ff3db2ed8a2ed36477afafafd4322c8d93b0f4a5cb39aa8ddcf");
				uint8_t buf[64];
				memcpy(buf, h1.data(), h1.size());
				memcpy(buf+32, h2.data(), h2.size());
				SHA256 sha;
				HashValue single = HashValue(sha.ComputeHash(ConstBuf(buf, 64)));
				HashValue doub = HashValue(sha.ComputeHash(sha.ComputeHash(ConstBuf(buf, 64))));
				cout << single << endl;
				cout << doub << endl;
#endif


				MinerBlock->MerkleBranch = BuildMerkleTree<Coin::HashValue>(MinerBlock->Txes, txHasher, &HashValue::Combine).GetBranch(0);

				client.MinerBlock = MinerBlock;
				EXT_LOCK (client.Miner.m_csCurrentData) {
					client.Miner.TaskQueue.clear();
					client.Miner.WorksToSubmit.clear();
				}
				client.Miner.m_evGetWork.Set();
			} else {
				for (int i=0; i<Merkles.size(); ++i) {
					if (client.JobQueue.size() >= MAX_JOB_QUEUE_SIZE)
						return;
					ptr<XptWorkData> wd = new XptWorkData;
					wd->Client.reset(&client);
					wd->Data.resize(128);
					wd->Height = MinerBlock->Height;
					wd->Version = MinerBlock->Ver;
					wd->Bits = MinerBlock->DifficultyTargetBits;
					wd->BitsForShare = BitsForShare;
					wd->BlockTimestamp = Timestamp;
					wd->PrevBlockHash = MinerBlock->PrevBlockHash;
					wd->MerkleRoot = Merkles[i];
					wd->FirstNonce = MinerBlock->NonceRange.first;
					wd->LastNonce = MinerBlock->NonceRange.second;
					wd->FixedPrimorialMultiplier = b.FixedPrimorialMultiplier;
					wd->FixedHashFactor = b.FixedHashFactor;
				
					wd->SieveSizeMin = b.SieveSizeMin;
					wd->SieveSizeMax = b.SieveSizeMax != 0 ? b.SieveSizeMax : DEFAULT_SIEVE_SIZE;
					wd->SieveSize = max(wd->SieveSizeMin, min(wd->SieveSizeMax, (uint32_t)MAX_SIEVE_SIZE));

					if (client.Miner.HashAlgo == HashAlgo::Prime) {
						wd->PrimesToSieveMin = b.PrimesToSieveMin;
						wd->PrimesToSieveMax = b.PrimesToSieveMax;
						wd->SieveChainLength = b.SieveChainLength;
						wd->TimestampRoll = b.BundleFlags & 1;
						wd->Interruptible = b.BundleFlags & 2;
					}
					client.JobQueue.push(wd);
				}
			}
		} else {
			for (int i=0; i<Bundles.size(); ++i) {
				const ShareBundle& b = Bundles[i];

				*client.Miner.m_pTraceStream << EXT_STR(Clock::now().ToLocalTime() << " WorkData. Height: " << int(b.Height) << "  Diff: " << setprecision(5) << double(b.BitsForShare)/0x1000000 << "/" << double(b.Bits)/0x1000000 << (EarnedShareValue!=0 ? EXT_STR(", ShareValue: " << EarnedShareValue) : String()) << "         \n") << flush;

				if (b.SieveSizeMin > MAX_SIEVE_SIZE) {
					*client.Miner.m_pTraceStream << EXT_STR("MinSieveSize: " << b.SieveSizeMin << " violates client's max " << MAX_SIEVE_SIZE << "\n") << flush;
					Throw(E_FAIL);
				}

				client.Miner.MaxHeight = (max)((uint32_t)client.Miner.MaxHeight, b.Height);
				for (int j=0; j<b.PayloadMerkles.size(); ++j) {
					if (client.JobQueue.size() >= MAX_JOB_QUEUE_SIZE)
						return;

					ptr<XptWorkData> wd = new XptWorkData;
					wd->Client.reset(&client);
					wd->Data.resize(128);
					wd->Height = b.Height;
					wd->Version = b.Version;
					wd->Bits = b.Bits;
					wd->BitsForShare = b.BitsForShare;
					wd->BlockTimestamp = b.Timestamp;
					wd->PrevBlockHash = b.PrevBlockHash;
					wd->MerkleRoot = b.PayloadMerkles[j];
					wd->FirstNonce = b.NonceMin;
					wd->LastNonce = b.NonceMax;
					wd->FixedPrimorialMultiplier = b.FixedPrimorialMultiplier;
					wd->FixedHashFactor = b.FixedHashFactor;
				
					wd->SieveSizeMin = b.SieveSizeMin;
					wd->SieveSizeMax = b.SieveSizeMax != 0 ? b.SieveSizeMax : DEFAULT_SIEVE_SIZE;
					wd->SieveSize = max(wd->SieveSizeMin, min(wd->SieveSizeMax, (uint32_t)MAX_SIEVE_SIZE));
				
					wd->PrimesToSieveMin = b.PrimesToSieveMin;
					wd->PrimesToSieveMax = b.PrimesToSieveMax;
					wd->SieveChainLength = b.SieveChainLength;
					wd->TimestampRoll = b.BundleFlags & 1;
					wd->Interruptible = b.BundleFlags & 2;

					client.JobQueue.push(wd);			
				}
			}
		}
	}
	client.Miner.m_evGetWork.Set();
}

void ShareAckXptMessage::ProcessMsg(P2P::Link& link) {
	XptClient& client = ToClient(link);

	ptr<BitcoinWorkData> wd = EXT_LOCKED(client.MtxData, client.LastShare);
	client.Miner.Print(*wd, !ErrorCode, Reason + (ErrorCode ? String() : EXT_STR("  " << "ShareValue: " << ShareValue)));
	++client.Miner.aSubmittedCount;
}

void MessageXptMessage::ProcessMsg(P2P::Link& link) {
	XptClient& client = ToClient(link);
	*client.Miner.m_pWTraceStream << "\nXPT Server Message: " << Message << endl;
}

void PingXptMessage::ProcessMsg(P2P::Link& link) {

}

XptClient::XptClient(Coin::BitcoinMiner& miner)
	:	base()
	,	ConnectionClient(miner)
	,	DtNextSendPow(Clock::now() + TimeSpan::FromSeconds(70))
{
	switch (miner.HashAlgo) {
	case HashAlgo::Metis:
	case HashAlgo::Sha3:
		ProtocolVersion = 6;
		break;
	default:
		ProtocolVersion = 5;
		break;			
	}
	std::fill(begin(aLenStats), end(aLenStats), 0);
}

ptr<XptMessage> XptClient::Recv() {
   	uint32_t opSize = R.ReadUInt32();
   	Blob blob(0, opSize >> 8);
   	R.BaseStream.ReadBuffer(blob.data(), blob.size());
   	ptr<XptMessage> m;
   	switch (uint8_t opcode = (uint8_t)opSize) {
   	case XPT_OPC_C_AUTH_REQ:		m = new AuthXptMessage;			break;
   	case XPT_OPC_S_AUTH_ACK:		m = new AuthAckXptMessage;		break;
   	case XPT_OPC_S_WORKDATA1:		m = new Workdata1XptMessage;	break;
   	case XPT_OPC_C_SUBMIT_SHARE:	m = new SubmitShareXptMessage;	break;
   	case XPT_OPC_S_SHARE_ACK:		m = new ShareAckXptMessage;		break;
   	case XPT_OPC_C_SUBMIT_POW:		m = new SubmitPowXptMessage;	break;
   	case XPT_OPC_S_MESSAGE:			m = new MessageXptMessage;		break;
	case XPT_OPC_S_PING:			m = new PingXptMessage;			break;
   	default:
   		Throw(ExtErr::Protocol_Violation);
   	}
	CXptThreadKeeper xptKeeper(this);
   	BinaryReader(CMemReadStream(blob)) >> *m;
	return m;
}

void XptClient::PrintStats() {
	switch (Miner.HashAlgo) {
	case HashAlgo::Prime:
		{
			ostringstream os;
		for (int len = 5; len < aLenStats.size(); ++len) {
				os << (len>5 ? "   " : "") << len << "ch: " << aLenStats[len].load();
				if (len<aLenStats.size()-1 &&  0==aLenStats[len] && 0==aLenStats[len+1])
					break;
			}
			os << "                  \n";
			*Miner.m_pTraceStream << os.str() << flush;
		}
		break;
	}
}

void XptClient::OnPeriodic() {
	DateTime dt = Clock::now();
	if (dt > DtNextSendPow) {
		if (ProtocolVersion == 4) {
			ptr<SubmitPowXptMessage> m = new SubmitPowXptMessage;
			EXT_LOCK (MtxData) {
				for (CPowMap::iterator it=m_powMap.begin(); it!=m_powMap.end(); ++it)
					m->Pows.push_back(it->second);
				m_powMap.clear();
			}
			Send(m);
		}
		DtNextSendPow = dt + TimeSpan::FromSeconds(70 - rand()%20);
		PrintStats();
	}
}

void XptClient::Execute() {
	Name = "XptClient Thread";

	while (!m_bStop) {
		try {
			SocketKeeper sockKeeper(_self, Tcp.Client);
			TRC(1, "EpServer = " << EpServer);
			Tcp.Client.Connect(EpServer);
			Miner.m_msWait = NORMAL_WAIT;

			ptr<AuthXptMessage> m = new AuthXptMessage;
			m->ProtocolVersion = ProtocolVersion;
			m->Username = Miner.Login;
			m->Password = Miner.Password;
			m->PayloadNum = Miner.ThreadCount;

			m->ClientVersion = Miner.UserAgentString;
			m->ClientVersion = "xptMiner 1.4";									//!!!T

			Send(m);
			ptr<AuthAckXptMessage> ack = dynamic_cast<AuthAckXptMessage*>(XptClient::Recv().get());
			if (!ack)
				Throw(ExtErr::Protocol_Violation);
			if (ack->ErrorCode)
				throw Exception(0x8FFF0000 | ack->ErrorCode, ack->Reason);
			DateTime dtNextPing = Clock::now() + XPT_PING_PERIOD;
			while (!m_bStop) {
				XptClient::Recv()->ProcessMsg(_self);
				DateTime now = Clock::now();
				if (now > dtNextPing) {
					dtNextPing = now + XPT_PING_PERIOD;
					Send(new PingXptMessage(to_time_t(now)*1000));
				}
			}
		} catch (RCExc ex) {
			*Miner.m_pTraceStream << ex.what() << endl;
			if (!m_bStop)
				Sleep(exchange(Miner.m_msWait, std::min(MAX_WAIT, Miner.m_msWait*2)));
		}
	}
}

void XptClient::Submit(BitcoinWorkData *wd) {
	ptr<MinerShare> share;
#if UCFG_COIN_PRIME
	if (XptWorkData *xwd = dynamic_cast<XptWorkData*>(wd)) {
		ptr<PrimeMinerShare> xshare = new PrimeMinerShare;
		share = xshare.get();
		xshare->SieveSize = xwd->SieveSize;
		xshare->SieveCandidate = xwd->SieveCandidate;
		xshare->FixedMultiplier = xwd->FixedMultiplier;
		xshare->PrimeChainMultiplier = xwd->ChainMultiplier;
	} else
#endif
		share = new MinerShare;
	share->Algo = wd->HashAlgo;
	share->Ver = wd->Version;
	share->m_merkleRoot = wd->MerkleRoot;
	share->PrevBlockHash = wd->PrevBlockHash;
	share->Timestamp = wd->BlockTimestamp;
	share->Nonce = wd->Nonce;
	share->DifficultyTargetBits = wd->Bits;
	share->ExtraNonce = wd->MinerBlock->ExtraNonce2;
	EXT_LOCKED(MtxData, LastShare = wd);
	ptr<SubmitShareXptMessage> m = new SubmitShareXptMessage;
	m->MinerShare = share;
	Send(m);
}

ptr<BitcoinWorkData> XptClient::GetWork() {
	EXT_LOCK (MtxData) {
		if (!JobQueue.empty()) {
			ptr<BitcoinWorkData> r = JobQueue.front();
			JobQueue.pop();
			return r.get();
		}
	}
	return ConnectionClient::GetWork();
}

ptr<BitcoinWorkData> GetPrimeTestData() {
	ptr<XptWorkData> r = new XptWorkData;

	return r;
}

} // Coin::


