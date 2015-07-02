/*######   Copyright (c) 2013-2015 Ufasoft  http://ufasoft.com  mailto:support@ufasoft.com,  Sergey Pavlov  mailto:dev@ufasoft.com ####
#                                                                                                                                     #
# 		See LICENSE for licensing information                                                                                         #
#####################################################################################################################################*/

#include <el/ext.h>

#include <el/crypto/ecdsa.h>
using namespace Ext::Crypto;

#ifndef UCFG_COIN_BIP0014_USER_AGENT
#	define UCFG_COIN_BIP0014_USER_AGENT ("/" UCFG_MANUFACTURER " " VER_PRODUCTNAME_STR ":" VER_PRODUCTVERSION_STR "/")
#endif


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

		DBG_LOCAL_IGNORE_CONDITION(ExtErr::EndOfStream);

		char cmd[12];
		rd.Read(cmd, sizeof cmd);

		uint32_t len = rd.ReadUInt32();
		checksum = rd.ReadUInt32();
		if (0 != cmd[sizeof(cmd)-1] || len > MAX_PAYLOAD)
			Throw(ExtErr::Protocol_Violation);

		MFN_CreateMessage mfn;
		if (Lookup(MessageClassFactoryBase::s_map, cmd, mfn))
			r = (eng.*mfn)();
		else {
			TRC(2, "Unknown command: " << cmd);
//!!!R			r = new CoinMessage(cmd);
		}
		payload.Size = len;
		rd.Read(payload.data(), payload.Size);
	}
	CMemReadStream ms(payload);
	HashValue h = Eng().HashMessage(payload);
	if (letoh(*(uint32_t*)h.data()) != checksum)
		Throw(ExtErr::Protocol_Violation);
	ms.Position = 0;
	if (r) {
		r->Link = &link;
		try {
			DBG_LOCAL_IGNORE_CONDITION(CoinErr::Misbehaving);
			r->Read(BinaryReader(ms));
		} catch (RCExc ex) {
			link.Send(new RejectMessage(RejectReason::Malformed, r->Cmd, "error parsing message"));
			throw ex;		//!!!T
		}
	}
	return r;
}

void CoinMessage::Write(BinaryWriter& wr) const {
}

void CoinMessage::Read(const BinaryReader& rd) {
}

void CoinMessage::Print(ostream& os) const {
	os << Cmd << " ";
}

VersionMessage::VersionMessage()
	: base("version")
	, ProtocolVer(Eng().ChainParams.ProtocolVersion)
	, Services(0)
	, LastReceivedBlock(-1)
	, RelayTxes(true)
{
	CoinEng& eng = Eng();
	if (eng.Mode == EngMode::Lite)
		RelayTxes = !eng.Filter;
	else
		Services |= uint64_t(NodeServices::NODE_NETWORK);
}

void VersionMessage::Write(BinaryWriter& wr) const {
	wr << ProtocolVer << Services << int64_t(to_time_t(RemoteTimestamp)) << RemotePeerInfo << LocalPeerInfo << Nonce;
	WriteString(wr, UserAgent);
	wr << LastReceivedBlock << RelayTxes;
}

void VersionMessage::Read(const BinaryReader& rd) {
	ProtocolVer = rd.ReadUInt32();
	Services = rd.ReadUInt64();
	RemoteTimestamp = DateTime::from_time_t(rd.ReadUInt64());
	if (ProtocolVer >= 106) {
		rd >> RemotePeerInfo >>	LocalPeerInfo >> Nonce;
		UserAgent = ReadString(rd);
		if (ProtocolVer >= 209) {
			LastReceivedBlock = (int32_t)rd.ReadUInt32();
			RelayTxes = rd.BaseStream.Eof() ? true : rd.ReadByte();
		}
	}
}

void VersionMessage::Print(ostream& os) const {
	base::Print(os);
	os << "Ver: " << ProtocolVer << ", " << UserAgent << ", LastBlock: " << LastReceivedBlock << ", Services: " << Services;
}

void VerackMessage::Process(P2P::Link& link) {
	Eng().RemoveVerNonce(link);
}


void CoinEng::SendVersionMessage(Link& link) {
	ptr<VersionMessage> m = (VersionMessage*)CreateVersionMessage();
	m->UserAgent = UCFG_COIN_BIP0014_USER_AGENT;		// BIP 0014
	m->RemoteTimestamp = Clock::now();
	GetSystemURandomReader() >> m->Nonce;
	EXT_LOCK(m_mtxVerNonce) {
		m_nonce2link[m->Nonce] = &link;
	}
	if (Listen)
		m->LocalPeerInfo.Ep = NetManager.LocalEp4;		//!!!? IPv6
	m->RemotePeerInfo.Ep = link.Tcp.Client.RemoteEndPoint;
	m->LastReceivedBlock = BestBlockHeight();
	link.Send(m);
}

void GetAddrMessage::Process(P2P::Link& link) {
	if (link.Incoming) {
		EXT_LOCK(link.Mtx) {
			link.m_setPeersToSend.clear();
		}
		vector<ptr<Peer>> peers = link.Net->GetPeers(1000);
		EXT_FOR(const ptr<Peer>& peer, peers) {
			link.Push(peer);
		}
	}
}

void AddrMessage::Write(BinaryWriter& wr) const {
	CoinSerialized::Write(wr, PeerInfos);
}

void AddrMessage::Read(const BinaryReader& rd) {
	CoinSerialized::Read(rd, PeerInfos);
}

void AddrMessage::Print(ostream& os) const {
	base::Print(os);
	os << PeerInfos.size() << " peers: ";
	for (int i=0; i<PeerInfos.size(); ++i) {
		if (i >= 2) {
			os << " ... ";
			break;
		}
		os << PeerInfos[i].Ep.Address << " ";
	}
}

void AddrMessage::Process(P2P::Link& link) {
	if (PeerInfos.size() > 1000)
		throw PeerMisbehavingException(20);
	DateTime now = Clock::now(),
		lowerLimit(1974, 1, 1),
		upperLimit(now + TimeSpan::FromMinutes(10)),
		dtDefault(now + TimeSpan::FromDays(5));
	EXT_FOR (const PeerInfo& pi, PeerInfos) {
		link.Net->Add(pi.Ep, pi.Services, pi.Timestamp > lowerLimit && pi.Timestamp < upperLimit ? pi.Timestamp : dtDefault);
	}
}

int LocatorHashes::FindHeightInMainChain() const {
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
	EXT_FOR (const BlockHeader& header, Headers) {
		header.WriteHeader(wr);
		CoinSerialized::WriteVarInt(wr, 0);
	}
}

void HeadersMessage::Read(const BinaryReader& rd) {
	CoinEng& eng = Eng();

	Headers.resize((size_t)CoinSerialized::ReadVarInt(rd));
	for (int i=0; i<Headers.size(); ++i) {
		(Headers[i] = BlockHeader(eng.CreateBlockObj())).ReadHeader(rd);
		CoinSerialized::ReadVarInt(rd);		// tx count unused
	}
}

void HeadersMessage::Print(ostream& os) const {
	base::Print(os);
	os << Headers.size() << " headers:";
	for (int i=0; i<Headers.size(); ++i) {
		if (i < 2 || i == Headers.size()-1)
			os << " " << Hash(Headers[i]);
		else if (i == 2)
			os << " ...";
	}
}

void GetHeadersGetBlocksMessage::Write(BinaryWriter& wr) const {
	wr << Ver;
	CoinSerialized::Write(wr, Locators);
	wr << HashStop;
}

void GetHeadersGetBlocksMessage::Read(const BinaryReader& rd) {
	rd >> Ver;
	CoinSerialized::Read(rd, Locators);
	rd >> HashStop;
}

void GetHeadersGetBlocksMessage::Print(ostream& os) const {
	base::Print(os);
	for (int i=0; i<Locators.size(); ++i) {
		if (i >= 2) {
			os << " ... ";
			break;
		}
		os << Locators[i] << " ";
	}
	os << " HashStop: " << HashStop;
}

void GetHeadersGetBlocksMessage::Set(const HashValue& hashLast, const HashValue& hashStop) {
	CoinEng& eng = Eng();

	Locators.clear();
	if (!hashLast)
		HashStop = eng.ChainParams.Genesis;
	else {
		BlockTreeItem bti = eng.Tree.GetItem(hashLast);
		int step = 1, h = bti.Height;
		for (HashValue hash=hashLast; hash; hash=eng.Tree.GetAncestor(hash, h)) {	//!!!? bool
			Locators.push_back(hash);
			if ((h -= step) <= 0)
				break;
			if (Locators.size() > 10)
				step *= 2;
		}
		Locators.push_back(eng.ChainParams.Genesis);
		HashStop = hashStop;
	}
}

GetHeadersMessage::GetHeadersMessage(const HashValue& hashLast, const HashValue& hashStop)
	: base("getheaders")
{
	Set(hashLast, hashStop);
}

void GetHeadersMessage::Process(P2P::Link& link) {
	CoinEng& eng = Eng();

	ptr<HeadersMessage> m = new HeadersMessage;
	if (!Locators.empty())
		m->Headers = eng.Db->GetBlockHeaders(Locators, HashStop);
	else if (Block block = eng.LookupBlock(HashStop)) {
		if (block.IsInMainChain())
			m->Headers.push_back(block);
	}
	if (!m->Headers.empty())
		link.Send(m);
}

void Inventory::Print(ostream& os) const {
	switch (Type) {
	case InventoryType::MSG_TX: os << "MSG_TX "; break;
	case InventoryType::MSG_BLOCK: os << "MSG_BLOCK "; break;
	case InventoryType::MSG_FILTERED_BLOCK: os << "MSG_FILTERED_BLOCK "; break;
	}
	os << HashValue;
}

GetBlocksMessage::GetBlocksMessage(const HashValue& hashLast, const HashValue& hashStop)
	:	base("getblocks")
{
	Set(hashLast, hashStop);
}

void GetBlocksMessage::Process(P2P::Link& link) {
	CoinEng& eng = Eng();
	CoinLink& clink = static_cast<CoinLink&>(link);

	if (eng.Mode==EngMode::Lite || eng.Mode==EngMode::BlockParser)
		return;

	int idx = Locators.FindHeightInMainChain();
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

void AlertMessage::Write(BinaryWriter& wr) const {
	WriteBlob(wr, Payload);
	WriteBlob(wr, Signature);
}

void AlertMessage::Read(const BinaryReader& rd) {
	Payload = ReadBlob(rd);
	Signature = ReadBlob(rd);
}

void AlertMessage::Process(P2P::Link& link) {
	CoinEng& eng = Eng();

	HashValue hv = Hash(Payload);
	EXT_FOR(const Blob& pubKey, eng.ChainParams.AlertPubKeys) {
		if (KeyInfoBase::VerifyHash(pubKey, hv, Signature))
			goto LAB_VERIFIED;
	}
	Throw(CoinErr::AlertVerifySignatureFailed);
LAB_VERIFIED:

	CMemReadStream stm(Payload);
	(Alert = new Coin::Alert)->Read(BinaryReader(stm));

	if (Clock::now() < Alert->Expiration) {
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

CoinLink::CoinLink(P2P::NetManager& netManager, thread_group& tr)
	: base(&netManager, &tr)
	, LastReceivedBlock(-1)
	, m_curMerkleBlock(nullptr)
{
}

void CoinLink::OnPeriodic() {
	if (!PeerVersion)							// wait for "version" message
		return;

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

	RequestHeaders();
	RequestBlocks();
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

	TRC(2, Peer->get_EndPoint().Address << " send " << m);
	
	Blob blob = EXT_BIN(m);
	MemoryStream qs2;
	BinaryWriter wrQs2(qs2);
	wrQs2 << eng.ChainParams.ProtocolMagic;
	char cmd[12] = { 0 };
	strcpy(cmd, m.Cmd);
	wrQs2.WriteStruct(cmd);
	wrQs2 << uint32_t(blob.Size) << letoh(*(uint32_t*)Eng().HashMessage(blob).data());	
	wrQs2.Write(blob.constData(), blob.Size);
	SendBinary(qs2);
}


void FilterLoadMessage::Write(BinaryWriter& wr) const {
	wr << *Filter;
}

void FilterLoadMessage::Read(const BinaryReader& rd) {
	rd >> *(Filter = new CoinFilter);
	if ((Filter->Bitset.size()+7)/8 > MAX_BLOOM_FILTER_SIZE || Filter->HashNum > MAX_HASH_FUNCS)
		throw PeerMisbehavingException(100);
}

void FilterLoadMessage::Process(P2P::Link& link) {
	CoinLink& clink = (CoinLink&)link;
	EXT_LOCK(clink.MtxFilter) {
		clink.Filter = Filter;
	}
	clink.RelayTxes = true;
}

void FilterAddMessage::Write(BinaryWriter& wr) const {
	CoinSerialized::WriteBlob(wr, Data);
}

void FilterAddMessage::Read(const BinaryReader& rd) {
	Data = CoinSerialized::ReadBlob(rd);
	if (Data.Size > MAX_SCRIPT_ELEMENT_SIZE)
		throw PeerMisbehavingException(100);
}

void FilterAddMessage::Process(P2P::Link& link) {
	CoinLink& clink = (CoinLink&)link;
	EXT_LOCK(clink.MtxFilter) {
		if (!clink.Filter)
			throw PeerMisbehavingException(100);
		clink.Filter->Insert(Data);
	}
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

void RejectMessage::Print(ostream& os) const {
	base::Print(os);
	if (Link)
		os << "From " << Link->Peer->get_EndPoint() << ": ";
	os << "Reject to command " << Command << ": " << Reason;
#ifdef _DEBUG//!!!D
	if (Reason != "insufficient priority")
		os << "";
#endif
	if (Command=="block" || Command=="tx")
		os << " " << Hash;
}

void InvGetDataMessage::Write(BinaryWriter& wr) const {
	CoinSerialized::Write(wr, Invs);
}

void InvGetDataMessage::Read(const BinaryReader& rd) {
	CoinSerialized::Read(rd, Invs);
}

void InvGetDataMessage::Process(P2P::Link& link) {
	if (Invs.size() > MAX_INV_SZ)
		throw PeerMisbehavingException(20);
}

void InvGetDataMessage::Print(ostream& os) const {
	base::Print(os);
	os << Invs.size() << " invs: ";
	for (int i=0; i<Invs.size(); ++i) {
		if (i > 18) {
			os << " ... ";
			break;
		}
		os << (i ? ", " : "     ") << Invs[i];
	}
}

vector<Tx> MerkleBlockMessage::Init(const Coin::Block& block, CoinFilter& filter) {
	Block = block;
	const CTxes& txes = block.get_Txes();
	PartialMT.NItems = txes.size();

	vector<HashValue> vHash(PartialMT.NItems);
	dynamic_bitset<byte> bsMatch(PartialMT.NItems);
	vector<Tx> r;
	for (size_t i=0; i<PartialMT.NItems; ++i) {
		const Tx& tx = txes[i];
		vHash[i] = Hash(tx);
		if (filter.IsRelevantAndUpdate(tx)) {
			r.push_back(tx);
			bsMatch.set(i);
		}
	}
	PartialMT.Init(vHash, bsMatch);
	return r;
}

void MerkleBlockMessage::Write(BinaryWriter& wr) const {
	Block.m_pimpl->WriteHeader(wr);
	wr << uint32_t(PartialMT.NItems);
	CoinSerialized::Write(wr, PartialMT.Items);
	vector<byte> v;
	to_block_range(PartialMT.Bitset, back_inserter(v));
	CoinSerialized::WriteBlob(wr, ConstBuf(&v[0], v.size()));
}

void MerkleBlockMessage::Read(const BinaryReader& rd) {
	Block.m_pimpl->ReadHeader(rd, false, 0);
	Block.m_pimpl->m_bTxesLoaded = true;
	PartialMT.NItems = rd.ReadUInt32();
	CoinSerialized::Read(rd, PartialMT.Items);
	Blob blob = CoinSerialized::ReadBlob(rd);
	const byte *p = blob.constData();
	PartialMT.Bitset.resize(blob.Size*8);
	for (int i=0; i<blob.Size; ++i)
		for (int j=0; j<8; ++j) {
			PartialMT.Bitset.replace(i*8+j, (p[i] >> j) & 1);
		}
}

void TxMessage::Write(BinaryWriter& wr) const {
	wr << Tx;
}

void TxMessage::Read(const BinaryReader& rd) {
	rd >> Tx;
}

void TxMessage::Print(ostream& os) const {
	base::Print(os);
	os << Hash(Tx);
}

void BlockMessage::Write(BinaryWriter& wr) const {
	wr << Block;
}

void BlockMessage::Read(const BinaryReader& rd) {
	Block = Coin::Block();
	rd >> Block;
}

void BlockMessage::Print(ostream& os) const {
	base::Print(os);
	if (Block.Height >= 0)
		os << Block.Height;
	os << " " << Hash(Block);
}

bool CoinEng::CheckSelfVerNonce(uint64_t nonce) {
	bool bSelf = false;
	EXT_LOCK (m_mtxVerNonce) {
		 if (bSelf = m_nonce2link.count(nonce))
			m_nonce2link[nonce]->OnSelfLink();
	}
	return !bSelf;
}

void CoinEng::RemoveVerNonce(P2P::Link& link) {
	EXT_LOCK (m_mtxVerNonce) {
		for (CNonce2link::iterator it=m_nonce2link.begin(), e=m_nonce2link.end(); it!=e; ++it)
			if (it->second.get() == &link) {
				m_nonce2link.erase(it);
				break;
			}
	}
}

void VersionMessage::Process(P2P::Link& link) {
	if (ProtocolVer < MIN_PEER_PROTO_VERSION)
		Throw(ExtErr::ObsoleteProtocolVersion);

	CoinEng& eng = Eng();
	CoinLink& clink = static_cast<CoinLink&>(link);
	if (clink.PeerVersion)
		Throw(CoinErr::DupVersionMessage);

	DateTime now = Clock::now();
	if (eng.CheckSelfVerNonce(Nonce)) {
		clink.PeerVersion = ProtocolVer;
		clink.TimeOffset = RemoteTimestamp - now;
		clink.LastReceivedBlock = LastReceivedBlock;
		clink.RelayTxes = RelayTxes;
		clink.IsClient = !(Services & uint64_t(NodeServices::NODE_NETWORK));

		eng.aPreferredDownloadPeers += (clink.IsPreferredDownload = !link.Incoming && !link.IsOneShot && !clink.IsClient);

		if (link.Incoming)
			eng.SendVersionMessage(link);
		clink.Send(eng.CreateVerackMessage());

		if (!link.Incoming)
			link.Send(new GetAddrMessage);

		if (eng.Filter) {
			link.Send(new FilterLoadMessage(eng.Filter.get()));
		}
		clink.RequestHeaders();
	}
	if (link.Incoming)
		if (ptr<Peer> p = link.Net->Add(link.Tcp.Client.RemoteEndPoint, Services, now))
			link.Peer = p;
	eng.Good(link.Peer);
}

void SubmitOrderMessage::Write(BinaryWriter& wr) const {
	wr << TxHash;
}

void SubmitOrderMessage::Read(const BinaryReader& rd) {
	rd >> TxHash;
}

void ReplyMessage::Write(BinaryWriter& wr) const {
	wr << Code;
}

void ReplyMessage::Read(const BinaryReader& rd) {
	Code = rd.ReadUInt32();
}


void PingPongMessage::Write(BinaryWriter& wr) const {
	wr << Nonce;
}

void PingPongMessage::Read(const BinaryReader& rd) {
	rd >> Nonce;
}

void PingMessage::Read(const BinaryReader& rd) {
	if (Link->PeerVersion >= 60001 && !rd.BaseStream.Eof())
		base::Read(rd);
}

void PingMessage::Process(P2P::Link& link) {
	ptr<PongMessage> m = new PongMessage;
	m->Nonce = Nonce;
	link.Send(m);
}


} // Coin::

