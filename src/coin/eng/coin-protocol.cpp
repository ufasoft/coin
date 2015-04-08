/*######   Copyright (c) 2013-2015 Ufasoft  http://ufasoft.com  mailto:support@ufasoft.com,  Sergey Pavlov  mailto:dev@ufasoft.com ####
#                                                                                                                                     #
# 		See LICENSE for licensing information                                                                                         #
#####################################################################################################################################*/

#include <el/ext.h>

#include <el/crypto/ecdsa.h>
using namespace Ext::Crypto;


#include "coin-protocol.h"
#include "eng.h"

namespace Coin {

typedef CoinMessage* (CoinEng::*MFN_CreateMessage)();

class MessageClassFactoryBase {
public:
	static unordered_map<String, MFN_CreateMessage> s_map;

	MessageClassFactoryBase(const char *name, MFN_CreateMessage mfn) {
		s_map.insert(make_pair(String(name), mfn));
	}

//!!!	virtual CoinMessage *CreateObject() { return new CoinMessage; }
};

unordered_map<String, MFN_CreateMessage> MessageClassFactoryBase::s_map;

/*!!!
template <class T> 
class MessageClassFactory : public MessageClassFactoryBase {
	typedef MessageClassFactoryBase base;
public:
	MessageClassFactory(const char *name)
		:	base(name)
	{}

	CoinMessage *CreateObject() override { return new T; }
};
*/

static MessageClassFactoryBase
	s_factoryVersion	("version"		, &CoinEng::CreateVersionMessage	),
	s_factoryVerack		("verack"		, &CoinEng::CreateVerackMessage		),
	s_factoryAddr		("addr"			, &CoinEng::CreateAddrMessage		),
	s_factoryInv		("inv"			, &CoinEng::CreateInvMessage		),
	s_factoryGetData	("getdata"		, &CoinEng::CreateGetDataMessage	),
	s_factoryNotFound	("notfound"		, &CoinEng::CreateNotFoundMessage	),
	s_factoryGetBlocks	("getblocks"	, &CoinEng::CreateGetBlocksMessage	),
	s_factoryGetHeaders	("getheaders"	, &CoinEng::CreateGetHeadersMessage	),
	s_factoryTx			("tx"			, &CoinEng::CreateTxMessage			),
	s_factoryBlock		("block"		, &CoinEng::CreateBlockMessage		),
	s_factoryHeaders	("headers"		, &CoinEng::CreateHeadersMessage	),
	s_factoryGetAddr	("getaddr"		, &CoinEng::CreateGetAddrMessage	),
	s_factoryCheckOrder("checkorder"	, &CoinEng::CreateCheckOrderMessage	),
	s_factorySubmitOrder("submitorder"	, &CoinEng::CreateSubmitOrderMessage),
	s_factoryReply		("reply"		, &CoinEng::CreateReplyMessage		),
	s_factoryPing		("ping"			, &CoinEng::CreatePingMessage		),
	s_factoryPong		("pong"			, &CoinEng::CreatePongMessage		),
	s_factoryMemPool	("mempool"		, &CoinEng::CreateMemPoolMessage	),
	s_factoryAlert		("alert"		, &CoinEng::CreateAlertMessage		),
	s_factoryMerkleBlock("merkleblock"	, &CoinEng::CreateMerkleBlockMessage),
	s_factoryFilterLoad	("filterload"	, &CoinEng::CreateFilterLoadMessage	),
	s_factoryFilterAdd	("filteradd"	, &CoinEng::CreateFilterAddMessage	),
	s_factoryFilterClear("filterclear"	, &CoinEng::CreateFilterClearMessage),
	s_factoryReject		("reject"		, &CoinEng::CreateRejectMessage		),

	s_factoryCheckPoint	("checkpoint"	, &CoinEng::CreateCheckPointMessage	);		// PPCoin

ptr<CoinMessage> CoinMessage::ReadFromStream(P2P::Link& link, const BinaryReader& rd) {
	uint32_t checksum = 0;
	ptr<CoinMessage> r;
	Blob payload(nullptr);
	{
		CoinEng& eng = Eng();

		DBG_LOCAL_IGNORE(E_EXT_EndOfStream);

		char cmd[12];
		rd.Read(cmd, sizeof cmd);

		uint32_t len = rd.ReadUInt32();
		checksum = rd.ReadUInt32();
		if (0 != cmd[sizeof(cmd)-1] || len > MAX_PAYLOAD)
			Throw(E_EXT_Protocol_Violation);

		MFN_CreateMessage mfn;
		if (Lookup(MessageClassFactoryBase::s_map, cmd, mfn))
			r = (eng.*mfn)();
		else {
			TRC(2, "Unknown command: " << cmd);

			r = new CoinMessage(cmd);
		}
		payload.Size = len;
		rd.Read(payload.data(), payload.Size);
	}
	CMemReadStream ms(payload);
	HashValue h = Eng().HashMessage(payload);
	if (*(uint32_t*)h.data() != checksum)
		Throw(E_EXT_Protocol_Violation);
	ms.Position = 0;
	r->Link = &link;

	try {
		DBG_LOCAL_IGNORE(E_COIN_Misbehaving);
		r->Read(BinaryReader(ms));
	} catch (RCExc ex) {
		link.Send(new RejectMessage(RejectReason::Malformed, r->Command, "error parsing message"));
		throw ex;		//!!!T
	}
	return r;
}

void CoinMessage::Write(BinaryWriter& wr) const {
}

void CoinMessage::Read(const BinaryReader& rd) {
}

String CoinMessage::ToString() const {
	return "Msg "+Command;
}

void GetAddrMessage::Process(P2P::Link& link) {
	EXT_LOCK (link.Mtx) {
		link.m_setPeersToSend.clear();
	}
	EXT_FOR (const ptr<Peer>& peer, link.Net->GetPeers(1000)) {
		link.Push(peer);
	}
}

void AddrMessage::Write(BinaryWriter& wr) const {
	CoinSerialized::Write(wr, PeerInfos);

	/*!!!
	WriteVarInt(wr, PeerInfos.size());
	for (int i=0; i<PeerInfos.size(); ++i)
		WritePeerInfo(wr, PeerInfos[i], true);*/
}

void AddrMessage::Read(const BinaryReader& rd) {
	CoinSerialized::Read(rd, PeerInfos);
	/*!!!R
	PeerInfos.resize((size_t)ReadVarInt(rd));
	for (int i=0; i<PeerInfos.size(); ++i)
		PeerInfos[i] = ReadPeerInfo(rd, true);*/
}

void AddrMessage::Process(P2P::Link& link) {
	if (PeerInfos.size() > 1000)
		throw PeerMisbehavingException(20);
	DateTime now = DateTime::UtcNow(),
		lowerLimit(1974, 1, 1),
		upperLimit(now + TimeSpan::FromMinutes(10)),
		dtDefault(now + TimeSpan::FromDays(5));
	EXT_FOR (const PeerInfo& pi, PeerInfos) {
		link.Net->Add(pi.Ep, pi.Services, pi.Timestamp > lowerLimit && pi.Timestamp < upperLimit ? pi.Timestamp : dtDefault);
	}
}

int LocatorHashes::FindIndexInMainChain() const {
	CoinEng& eng = Eng();
	
	EXT_FOR (const HashValue& hash, _self) {
		if (Block block = eng.LookupBlock(hash))
			if (block.IsInMainChain())
				return block.Height;
	}
	return 0;
}

int LocatorHashes::get_DistanceBack() const {
	CoinEng& eng = Eng();

	int r = 0,
		step = 1;
	EXT_FOR (const HashValue& hash, _self) {
		if (Block block = eng.LookupBlock(hash))
			if (block.IsInMainChain())
				break;
		if ((r += step) > 10)
			step *= 2;
	}
	return r;
}

void HeadersMessage::Write(BinaryWriter& wr) const {
	CoinSerialized::WriteVarInt(wr, Headers.size());
	EXT_FOR (const Block& block, Headers) {
		block.WriteHeader(wr);
		CoinSerialized::WriteVarInt(wr, 0);
	}
}

void HeadersMessage::Read(const BinaryReader& rd) {
	Headers.resize((size_t)CoinSerialized::ReadVarInt(rd));
	for (int i=0; i<Headers.size(); ++i)
		Headers[i].ReadHeader(rd);
}

void GetHeadersMessage::Write(BinaryWriter& wr) const {
	CoinSerialized::Write(wr, Locators);
	wr << HashStop;
}

void GetHeadersMessage::Read(const BinaryReader& rd) {
	CoinSerialized::Read(rd, Locators);
	rd >> HashStop;
}

void GetHeadersMessage::Process(P2P::Link& link) {
	CoinEng& eng = Eng();

	ptr<HeadersMessage> m = new HeadersMessage;
	if (Locators.empty()) {
		if (Block block = eng.LookupBlock(HashStop)) {
			if (block.IsInMainChain())
				m->Headers.push_back(block);
			else
				return;			
		} else
			return;
	} else {
		m->Headers = eng.Db->GetBlocks(Locators, HashStop);
	}
	link.Send(m);
}

GetBlocksMessage::GetBlocksMessage(const Block& blockLast, const HashValue& hashStop)
	:	base("getblocks")
	,	Ver(Eng().ChainParams.ProtocolVersion)
{
	Set(blockLast, hashStop);
}

void GetBlocksMessage::Set(const Block& blockLast, const HashValue& hashStop) {
	CoinEng& eng = Eng();

	Locators.clear();
	int step = 1;
	for (Block block=blockLast; block;) {
		Locators.push_back(Hash(block));
		if (block.IsInMainChain()) {
			int idxPrev = block.Height-step;
			block = idxPrev > 0 ? eng.GetBlockByHeight(idxPrev) : nullptr;
		} else {
			for (int i = 0; block && i < step; i++)
				block = block.GetPrevBlock();
		}
		if (Locators.size() > 10)
			step *= 2;
	}
	Locators.push_back(eng.ChainParams.Genesis);
#ifdef X_DEBUG//!!!D
	for (int i=0; i<Locators.size(); ++i) {
		Block b = eng.LookupBlock(Locators[i]);
		TRC(1, Locators[i] << " " << (b ? b.Height : -2));
	}
#endif
	HashStop = hashStop;
}

void GetBlocksMessage::Write(BinaryWriter& wr) const  {
	wr << Ver;
	CoinSerialized::Write(wr, Locators);
	wr << HashStop;
}

void GetBlocksMessage::Read(const BinaryReader& rd) {
	rd >> Ver;
	CoinSerialized::Read(rd, Locators);
	rd >> HashStop;
}

void GetBlocksMessage::Process(P2P::Link& link) {
	CoinEng& eng = Eng();
	CoinLink& clink = static_cast<CoinLink&>(link);

	if (eng.Mode==EngMode::Lite || eng.Mode==EngMode::BlockParser)
		return;

	int idx = Locators.FindIndexInMainChain();
	int limit = 500 + Locators.DistanceBack;
	for (int i=idx+1; i<limit+idx; ++i) {
		if (i > eng.BestBlockHeight())
			break;
		Block block = eng.GetBlockByHeight(i);
		HashValue hash = Hash(block);
		if (hash == HashStop) //!!!R || Thread::CurrentThread->m_bStop)
			break;
		clink.Push(Inventory(InventoryType::MSG_BLOCK, hash));
	}
}

String GetBlocksMessage::ToString() const {
	ostringstream os;
	os << base::ToString() << " ";
	for (int i=0; i<Locators.size(); ++i) {
		if (i >= 2) {
			os << " ... ";
			break;
		}
		os << Locators[i] << " ";
	}
	os << " HashStop: " << HashStop;
	return os.str();
}

void Alert::Read(const BinaryReader& rd) {
	int64_t relayUntil, expiration;
	rd >> Ver >> relayUntil >> expiration >> NId >> NCancel >> SetCancel >> MinVer >> MaxVer;
	RelayUntil = DateTime::from_time_t(relayUntil);
	Expiration = DateTime::from_time_t(expiration);

	int n = rd.ReadSize();
	for (int i=0; i<n; ++i)
		SetSubVer.insert(CoinSerialized::ReadString(rd));
	rd >> Priority;
	Comment = CoinSerialized::ReadString(rd);
	StatusBar = CoinSerialized::ReadString(rd);
	Reserved = CoinSerialized::ReadString(rd);
}

void AlertMessage::Process(P2P::Link& link) {
	CoinEng& eng = Eng();

	HashValue hv = Hash(Payload);
	for (int i=0; i<eng.ChainParams.AlertPubKeys.size(); ++i) {
		ECDsa dsa(CngKey::Import(eng.ChainParams.AlertPubKeys[i], CngKeyBlobFormat::OSslEccPublicBlob));
		if (dsa.VerifyHash(hv, Signature))
			goto LAB_VERIFIED;
	}
	Throw(E_COIN_AlertVerifySignatureFailed);
LAB_VERIFIED:

	CMemReadStream stm(Payload);
	(Alert = new Coin::Alert)->Read(BinaryReader(stm));

	if (DateTime::UtcNow() < Alert->Expiration) {
		TRC(2, Alert->Expiration << "\t" << Alert->StatusBar);

		if (eng.m_iiEngEvents)
			eng.m_iiEngEvents->OnAlert(Alert);

		EXT_LOCK (eng.MtxPeers) {
			for (CoinEng::CLinks::iterator it=begin(eng.Links); it!=end(eng.Links); ++it)
				if (*it != &link)
					dynamic_cast<CoinLink*>((*it).get())->Send(this);
		}
	}
}

void CoinLink::OnPeriodic() {
	CoinEng& eng = static_cast<CoinEng&>(*Net);
	CCoinEngThreadKeeper engKeeper(&eng);

	if (eng.m_iiEngEvents)
		eng.m_iiEngEvents->OnPeriodicForLink(_self);

	bool bTrickled = Net->IsRandomlyTrickled();
	ptr<InvMessage> m = new InvMessage;
	EXT_LOCK (Mtx) {
		for (CInvertorySetToSend::iterator it=InvertorySetToSend.begin(), e=InvertorySetToSend.end(), tit; it!=e && m->Invs.size()<MAX_INV_SZ;) {
			const Inventory& inv = *(tit = it++);
			if (inv.Type!=InventoryType::MSG_TX || bTrickled) {
				m->Invs.push_back(inv);
				KnownInvertorySet.insert(inv);
				InvertorySetToSend.erase(tit);
			}
		}
	}
	if (!m->Invs.empty()) {
		TRC(2, "Sending " << m->Invs.size() << " Invs");
		Send(m);
	}

	if (bTrickled) {
		ptr<AddrMessage> m;
		EXT_LOCK (Mtx) {
			if (!m_setPeersToSend.empty()) {
				m = new AddrMessage;
				EXT_FOR (const ptr<P2P::Peer>& peer, m_setPeersToSend) {
					if (m->PeerInfos.size() < 1000) {
						PeerInfo pi;
						pi.Ep = peer->EndPoint;
						pi.Timestamp = peer->LastLive;
						pi.Services = peer->Services;
						m->PeerInfos.push_back(pi);
					}
				}
				m_setPeersToSend.clear();
			}
		}
		if (m)
			Send(m);
	}
}

void CoinLink::Push(const Inventory& inv) {
	EXT_LOCK (Mtx) {
		if (!KnownInvertorySet.count(inv))
			InvertorySetToSend.insert(inv);
	}
}

void CoinLink::Send(ptr<P2P::Message> msg) {
	CoinMessage& m = *static_cast<CoinMessage*>(msg.get());
	CoinEng& eng = static_cast<CoinEng&>(*Net);

	TRC(2, Peer->get_EndPoint() << " send " << m);
	
	Blob blob = EXT_BIN(m);
	MemoryStream qs2;
	BinaryWriter wrQs2(qs2);
	wrQs2 << eng.ChainParams.ProtocolMagic;
	char cmd[12] = { 0 };
	strcpy(cmd, m.Command);
	wrQs2.WriteStruct(cmd);
	wrQs2 << uint32_t(blob.Size) << letoh(*(uint32_t*)Eng().HashMessage(blob).data());	
	wrQs2.Write(blob.constData(), blob.Size);
	SendBinary(qs2);
}

void RejectMessage::Write(BinaryWriter& wr) const {
	WriteString(wr, Command);
	wr << (byte)Code;
	WriteString(wr, Reason);
	if (Command=="block" || Command=="tx")
		wr << Hash;
}

void RejectMessage::Read(const BinaryReader& rd) {
	Command = ReadString(rd);
	Code = (RejectReason)rd.ReadByte();
	Reason = ReadString(rd);
	if (Command=="block" || Command=="tx")
		rd >> Hash;
}

String RejectMessage::ToString() const {
	ostringstream os;
	if (Link)
		os << "From " << Link->Peer->get_EndPoint() << ": ";
	os << "Reject to command " << Command << ": " << Reason;
#ifdef _DEBUG//!!!D
	if (Reason != "insufficient priority")
		os << "";
#endif
	if (Command=="block" || Command=="tx")
		os << " " << Hash;
	return String(os.str());
}


} // Coin::

