/*######   Copyright (c) 2013-2019 Ufasoft  http://ufasoft.com  mailto:support@ufasoft.com,  Sergey Pavlov  mailto:dev@ufasoft.com ####
#                                                                                                                                     #
# 		See LICENSE for licensing information                                                                                         #
#####################################################################################################################################*/

#include <el/ext.h>

#include <el/crypto/ecdsa.h>
using namespace Ext::Crypto;

#ifndef UCFG_COIN_BIP0014_USER_AGENT
#	define UCFG_COIN_BIP0014_USER_AGENT ("/" UCFG_MANUFACTURER " " VER_PRODUCTNAME_STR ":" VER_PRODUCTVERSION_STR3 "/")
#endif


#include "coin-protocol.h"
#include "eng.h"

namespace Coin {

typedef CoinMessage* (CoinEng::*MFN_CreateMessage)();

class MessageClassFactoryBase {
public:
	static unordered_map<String, MFN_CreateMessage> s_map;

	MessageClassFactoryBase(const char* name, MFN_CreateMessage mfn);

//!!!	virtual CoinMessage *CreateObject() { return new CoinMessage; }
};

unordered_map<String, MFN_CreateMessage> MessageClassFactoryBase::s_map;

MessageClassFactoryBase::MessageClassFactoryBase(const char* name, MFN_CreateMessage mfn) {
	s_map.insert(make_pair(String(name), mfn));
}


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
    s_factorySendHeaders("sendheaders"  , &CoinEng::CreateSendHeadersMessage),
    s_factoryFeeFilter  ("feefilter"    , &CoinEng::CreateFeeFilterMessage),
    s_factorySendCompactBlock("sendcmpct", &CoinEng::CreateSendCompactBlockMessage),
    s_factoryCompactBlock("cmpctblock"  , &CoinEng::CreateCompactBlockMessage),
    s_factoryGetBlockTransactions("getblocktxn" , &CoinEng::CreateGetBlockTransactionsMessage),
    s_factoryBlockTransactions("blocktxn"  , &CoinEng::CreateBlockTransactionsMessage),

	s_factoryCheckPoint	("checkpoint"	, &CoinEng::CreateCheckPointMessage	);		// PPCoin

CoinMessage *CoinEng::CreateVersionMessage() {
	return new VersionMessage;
}

CoinMessage *CoinEng::CreateVerackMessage() {
	return new VerackMessage;
}

CoinMessage *CoinEng::CreateAddrMessage() {
	return new AddrMessage;
}

CoinMessage *CoinEng::CreateInvMessage() {
	return new InvMessage;
}

CoinMessage *CoinEng::CreateGetDataMessage() {
	return new GetDataMessage;
}

CoinMessage *CoinEng::CreateNotFoundMessage() {
	return new NotFoundMessage;
}

CoinMessage *CoinEng::CreateGetBlocksMessage() {
	return new GetBlocksMessage;
}

CoinMessage *CoinEng::CreateGetHeadersMessage() {
	return new GetHeadersMessage;
}

CoinMessage *CoinEng::CreateTxMessage() {
	return new TxMessage;
}

CoinMessage *CoinEng::CreateBlockMessage() {
	return new BlockMessage;
}

CoinMessage *CoinEng::CreateHeadersMessage() {
	return new HeadersMessage;
}

CoinMessage *CoinEng::CreateGetAddrMessage() {
	return new GetAddrMessage;
}

CoinMessage *CoinEng::CreateCheckOrderMessage() {
	return new CheckOrderMessage;
}

CoinMessage *CoinEng::CreateSubmitOrderMessage() {
	return new SubmitOrderMessage;
}

CoinMessage *CoinEng::CreateReplyMessage() {
	return new ReplyMessage;
}

CoinMessage *CoinEng::CreatePingMessage() {
	return new PingMessage;
}

CoinMessage *CoinEng::CreatePongMessage() {
	return new PongMessage;
}

CoinMessage *CoinEng::CreateMemPoolMessage() {
	return new MemPoolMessage;
}

CoinMessage *CoinEng::CreateAlertMessage() {
	return new AlertMessage;
}

CoinMessage *CoinEng::CreateMerkleBlockMessage() {
	return new MerkleBlockMessage;
}

CoinMessage *CoinEng::CreateFilterLoadMessage() {
	return new FilterLoadMessage;
}

CoinMessage *CoinEng::CreateFilterAddMessage() {
	return new FilterAddMessage;
}

CoinMessage *CoinEng::CreateFilterClearMessage() {
	return new FilterClearMessage;
}

CoinMessage *CoinEng::CreateRejectMessage() {
	return new RejectMessage;
}

CoinMessage* CoinEng::CreateSendHeadersMessage() { return new SendHeadersMessage(); }
CoinMessage* CoinEng::CreateFeeFilterMessage() { return new FeeFilterMessage(); }
CoinMessage* CoinEng::CreateGetBlockTransactionsMessage() { return new GetBlockTransactionsMessage(); }
CoinMessage* CoinEng::CreateBlockTransactionsMessage() { return new BlockTransactionsMessage(); }
CoinMessage* CoinEng::CreateSendCompactBlockMessage() { return new SendCompactBlockMessage(); }
CoinMessage* CoinEng::CreateCompactBlockMessage() { return new CompactBlockMessage(); }

CoinMessage *CoinEng::CreateCheckPointMessage() {
	return new CoinMessage("checkpoint");
}

String NodeServices::ToString(uint64_t s) {
	ostringstream os;
	if (s & NODE_NETWORK)
		os << "NODE_NETWORK";
	if (s & NODE_GETUTXO)
		os << " NODE_GETUTXO";
	if (s & NODE_BLOOM)
		os << " NODE_BLOOM";
	if (s & NODE_WITNESS)
		os << " NODE_WITNESS";
	if (s & NODE_XTHIN)
		os << " NODE_XTHIN";
	if (s & NODE_NETWORK_LIMITED)
		os << " NODE_NETWORK_LIMITED";
	if (s & ~(NODE_NETWORK | NODE_GETUTXO | NODE_BLOOM | NODE_WITNESS | NODE_XTHIN | NODE_NETWORK_LIMITED))
		os << " " << hex << showbase << s;
	return os.str();
}

ptr<CoinMessage> CoinMessage::ReadFromStream(Link& link, const BinaryReader& rd) {
	CoinEng& eng = Eng();

	ptr<CoinMessage> r;
	Blob payload(nullptr);
	{
		DBG_LOCAL_IGNORE_CONDITION(ExtErr::EndOfStream);

		char cmd[12];
		rd.Read(cmd, sizeof cmd);
		uint32_t len = rd.ReadUInt32(),
			checksum = rd.ReadUInt32();

		if (0 != cmd[sizeof(cmd)-1] || len > ProtocolParam::MAX_PROTOCOL_MESSAGE_LENGTH)
			Throw(ExtErr::Protocol_Violation);

		if (auto omfn = Lookup(MessageClassFactoryBase::s_map, String(cmd)))
			r = (eng.*(omfn.value()))();
		else {
			TRC(2, "Unknown command: " << cmd);
//!!!R			r = new CoinMessage(cmd);
		}
		payload = rd.ReadBytes(len);
		HashValue h = eng.HashMessage(payload);
		if (letoh(*(uint32_t*)h.data()) != checksum)
			Throw(ExtErr::Protocol_Violation);
	}
	CMemReadStream ms(payload);
	if (r) {
		r->LinkPtr = &link;
		try {
			DBG_LOCAL_IGNORE_CONDITION(CoinErr::Misbehaving);
			ProtocolReader prd(ms, link.HasWitness);
			r->Read(prd);
		} catch (RCExc ex) {
			link.Send(new RejectMessage(RejectReason::Malformed, r->Cmd, "error parsing message"));
			throw ex;		//!!!T
		}
	}
	return r;
}

void CoinMessage::Write(ProtocolWriter& wr) const {
}

void CoinMessage::Read(const ProtocolReader& rd) {
}

void CoinMessage::Print(ostream& os) const {
	os << Cmd << " ";
}

void CoinMessage::Trace(Coin::Link& link, bool bSend) const {
	TRC(4, (bSend ? "Sending  " : "Received ") << link.Peer->get_EndPoint().Address << ": " << _self);
}

void CoinMessage::ProcessMsg(P2P::Link& link) {
	return Process(static_cast<Link&>(link));
}

VersionMessage::VersionMessage()
	: base("version")
	, ProtocolVer(Eng().ChainParams.ProtocolVersion)
	, Services(NodeServices::NODE_WITNESS | NodeServices::NODE_BLOOM)
	, LastReceivedBlock(-1)
	, RelayTxes(true)
{
	CoinEng& eng = Eng();
	if (eng.Mode == EngMode::Lite)
		RelayTxes = !eng.Filter;
	else
		Services |= NodeServices::NODE_NETWORK;
}

void VersionMessage::Write(ProtocolWriter& wr) const {
	wr << ProtocolVer << Services << int64_t(to_time_t(RemoteTimestamp)) << RemotePeerInfo << LocalPeerInfo << Nonce;
	WriteString(wr, UserAgent);
	wr << LastReceivedBlock << RelayTxes;
}

void VersionMessage::Read(const ProtocolReader& rd) {
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
	os << "Ver: " << ProtocolVer << ", " << UserAgent
		<< ", LastBlock: " << LastReceivedBlock;
	if (Timestamp != DateTime())
		os << ", Time offset: " << duration_cast<seconds>(RemoteTimestamp - Timestamp).count() << "s";
	os << ", Services: " << NodeServices::ToString(Services);
}

void VerackMessage::Process(Link& link) {
	CoinEng& eng = Eng();
	eng.RemoveVerNonce(link);

	if (link.PeerVersion < ProtocolVersion::SENDHEADERS_VERSION)
		return;
	link.Send(new SendHeadersMessage);

	if (link.PeerVersion < ProtocolVersion::FEEFILTER_VERSION)
		return;
	if (g_conf.MinRelayTxFee)
		link.Send(new FeeFilterMessage(g_conf.MinRelayTxFee));

	if (link.PeerVersion < ProtocolVersion::SHORT_IDS_BLOCKS_VERSION)
		return;
	if (link.HasWitness)
		link.Send(new SendCompactBlockMessage(2, false));
	link.Send(new SendCompactBlockMessage(1, false));
}

void CoinEng::SendVersionMessage(Link& link) {
	ptr<VersionMessage> m = (VersionMessage*)CreateVersionMessage();
	m->UserAgent = UCFG_COIN_BIP0014_USER_AGENT;		// BIP 0014
	GetSystemURandomReader() >> m->Nonce;
	EXT_LOCK(m_mtxVerNonce) {
		m_nonce2link[m->Nonce] = &link;
	}
	if (Listen)
		m->LocalPeerInfo.Ep = NetManager.LocalEp4;		//!!!? IPv6
	m->RemotePeerInfo.Ep = link.Tcp.Client.RemoteEndPoint;
	m->LastReceivedBlock = BestBlockHeight();
	m->RemoteTimestamp = Clock::now();
	link.Send(m);
}

void GetAddrMessage::Process(Link& link) {
	if (link.Incoming && !link.SentOtherPeersAddresses) {
		link.SentOtherPeersAddresses = true;
		EXT_LOCK(link.Mtx) {
			link.m_setPeersToSend.clear();
		}
		vector<ptr<Peer>> peers = link.Net->GetPeers(1000);
		EXT_FOR(const ptr<Peer>& peer, peers) {
			link.PushPeer(peer);
		}
	}
}

void AddrMessage::Write(ProtocolWriter& wr) const {
	CoinSerialized::Write(wr, PeerInfos);
}

void AddrMessage::Read(const ProtocolReader& rd) {
	CoinSerialized::Read(rd, PeerInfos);
}

void AddrMessage::Print(ostream& os) const {
	base::Print(os);
	os << PeerInfos.size() << " peers: ";
	for (int i = 0; i < PeerInfos.size(); ++i) {
		if (i >= 2) {
			os << " ... ";
			break;
		}
		os << PeerInfos[i].Ep.Address << " ";
	}
}

void AddrMessage::Process(Link& link) {
	if (PeerInfos.size() > ProtocolParam::MAX_ADDR_SZ)
		throw PeerMisbehavingException(20);
	if (!g_conf.Connect.empty())			// Ignore "addr" if have connect= option
		return;
	DateTime lowerLimit(1974, 1, 1),
		upperLimit(Timestamp + TimeSpan::FromMinutes(10)),
		dtDefault(Timestamp + TimeSpan::FromDays(5));
	EXT_FOR (const PeerInfo& pi, PeerInfos) {
		link.Net->Add(pi.Ep, pi.Services, pi.Timestamp > lowerLimit && pi.Timestamp < upperLimit ? pi.Timestamp : dtDefault);
	}
}

int LocatorHashes::FindHeightInMainChain(bool bFullBlocks) const {
	CoinEng& eng = Eng();

	EXT_FOR (const HashValue& hash, _self) {
		if (BlockHeader header = eng.Db->FindHeader(hash)) {
			if (bFullBlocks && header.Height <= eng.Db->GetMaxHeight() || !bFullBlocks)
				return header.Height;
		}
	}
	return 0;
}

void BlockObj::WriteHeaderInMessage(ProtocolWriter& wr) const {
	WriteHeader(wr);
	CoinSerialized::WriteCompactSize(wr, 0);
}

void BlockObj::ReadHeaderInMessage(const ProtocolReader& rd) {
	ReadHeader(rd, false, 0);
	CoinSerialized::ReadCompactSize64(rd);		// tx count unused
}

void HeadersMessage::Write(ProtocolWriter& wr) const {
	CoinSerialized::WriteCompactSize(wr, Headers.size());
	EXT_FOR (const BlockHeader& header, Headers) {
		header->WriteHeaderInMessage(wr);
	}
}

void HeadersMessage::Read(const ProtocolReader& rd) {
	CoinEng& eng = Eng();

	Headers.resize(CoinSerialized::ReadCompactSize(rd));
	for (int i = 0; i < Headers.size(); ++i) {
		(Headers[i] = BlockHeader(eng.CreateBlockObj()))->ReadHeaderInMessage(rd);
	}
}

void HeadersMessage::Print(ostream& os) const {
	CoinEng& eng = Eng();

	base::Print(os);
	os << Headers.size() << " headers:";
	for (int i = 0; i < Headers.size(); ++i) {
		if (i < 2 || i == Headers.size()-1)
			os << " " << eng.BlockStringId(Hash(Headers[i]));
		else if (i == 2)
			os << " ...";
	}
}

void GetHeadersGetBlocksMessage::Write(ProtocolWriter& wr) const {
	wr << Ver;
	CoinSerialized::Write(wr, Locators);
	wr << HashStop;
}

void GetHeadersGetBlocksMessage::Read(const ProtocolReader& rd) {
	rd >> Ver;
	CoinSerialized::Read(rd, Locators);
	rd >> HashStop;
}

void GetHeadersGetBlocksMessage::Print(ostream& os) const {
	CoinEng& eng = Eng();

	base::Print(os);
	bool bDotsPrinted = false;
	for (int i = 0; i < Locators.size(); ++i) {
		if (i >= 2 && i + 2 < Locators.size()) {
			if (!exchange(bDotsPrinted, true))
				os << " ... ";
		} else
			os << eng.BlockStringId(Locators[i]) << "  ";
	}
	os << " Stop block: " << eng.BlockStringId(HashStop);
}

void GetHeadersGetBlocksMessage::Set(const HashValue& hashLast, const HashValue& hashStop) {
	CoinEng& eng = Eng();

	Locators.clear();
	if (!hashLast)
		HashStop = eng.ChainParams.Genesis;
	else {
		BlockTreeItem bti = eng.Tree.GetHeader(hashLast);
		int step = 1, h = bti.Height;
		for (HashValue hash=hashLast; hash; hash=Hash(eng.Tree.GetAncestor(hash, h))) {	//!!!? bool
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

void GetHeadersMessage::Process(Link& link) {
	CoinEng& eng = Eng();

	ptr<HeadersMessage> m = new HeadersMessage;
	if (!Locators.empty())
		m->Headers = eng.Db->GetBlockHeaders(Locators, HashStop);
	else if (BlockHeader header = eng.Tree.FindHeader(HashStop)) {
		if (header.IsInTrunk())
			m->Headers.push_back(header);
	}
	if (!m->Headers.empty())
		link.Send(m);
}

void Inventory::Write(BinaryWriter& wr) const {
	wr << uint32_t(Type) << HashValue;
}

void Inventory::Read(const BinaryReader& rd) {
	uint32_t typ;
	rd >> typ >> HashValue;
	Type = InventoryType(typ);
}

void Inventory::Print(ostream& os) const {
	switch (Type) {
	case InventoryType::MSG_TX:						os << "MSG_TX";						break;
	case InventoryType::MSG_WITNESS_TX:				os << "MSG_WITNESS_TX";				break;
	case InventoryType::MSG_BLOCK:					os << "MSG_BLOCK";					break;
	case InventoryType::MSG_WITNESS_BLOCK:			os << "MSG_WITNESS_BLOCK";			break;
	case InventoryType::MSG_FILTERED_BLOCK:			os << "MSG_FILTERED_BLOCK";			break;
	case InventoryType::MSG_FILTERED_WITNESS_BLOCK: os << "MSG_FILTERED_WITNESS_BLOCK"; break;
	default:										os << hex << showbase << (int)Type;
	}
	os << " ";
	if (((int)Type & ~(int)InventoryType::MSG_WITNESS_FLAG) == (int)InventoryType::MSG_BLOCK || ((int)Type & ~(int)InventoryType::MSG_WITNESS_FLAG) == (int)InventoryType::MSG_FILTERED_BLOCK)
		os << Eng().BlockStringId(HashValue);
	else
		os << HashValue;
}

GetBlocksMessage::GetBlocksMessage(const HashValue& hashLast, const HashValue& hashStop)
	: base("getblocks")
{
	Set(hashLast, hashStop);
}

void GetBlocksMessage::Process(Link& link) {
	CoinEng& eng = Eng();

	if (eng.Mode == EngMode::Lite || eng.Mode == EngMode::BlockParser)
		return;

	if (Locators.size() > ProtocolParam::MAX_LOCATOR_SZ)
		Throw(CoinErr::GetBlocksLocatorSize);

	int idx = Locators.FindHeightInMainChain(true);
	int limit = 500; //!!!R +Locators.DistanceBack;
	for (int i = idx + 1; i < limit + idx; ++i) {
		if (i > eng.BestBlockHeight())
			break;
		Block block = eng.GetBlockByHeight(i);
		HashValue hash = Hash(block);
		if (hash == HashStop) //!!!R || Thread::CurrentThread->m_bStop)
			break;
		link.Push(Inventory(InventoryType::MSG_BLOCK, hash));
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

void AlertMessage::Write(ProtocolWriter& wr) const {
	WriteSpan(wr, Payload);
	WriteSpan(wr, Signature);
}

void AlertMessage::Read(const ProtocolReader& rd) {
	Payload = ReadBlob(rd);
	Signature = ReadBlob(rd);
}

void AlertMessage::Process(Link& link) {
	CoinEng& eng = Eng();

	HashValue hv = Hash(Payload);
	EXT_FOR(const Blob& pubKey, eng.ChainParams.AlertPubKeys) {
		if (eng.VerifyHash(pubKey, hv, Signature))
			goto LAB_VERIFIED;
	}
	Throw(CoinErr::AlertVerifySignatureFailed);
LAB_VERIFIED:

	CMemReadStream stm(Payload);
	(Alert = new Coin::Alert)->Read(BinaryReader(stm));

	if (Timestamp < Alert->Expiration) {
		TRC(2, Alert->Expiration << "\t" << Alert->StatusBar);

		eng.Events.OnAlert(Alert);

		EXT_LOCK (eng.MtxPeers) {
			for (CoinEng::CLinks::iterator it = begin(eng.Links); it != end(eng.Links); ++it)
				if (*it != &link)
					dynamic_cast<Link*>((*it).get())->Send(this);
		}
	}
}

Link::Link(P2P::NetManager& netManager, thread_group& tr)
	: base(&netManager, &tr)
	, LastReceivedBlock(-1)
	, m_curMerkleBlock(nullptr)
	, MinFeeRate(0)
	, PingNonceSent(0)
{
}

void Link::OnPeriodic(const DateTime& now) {
	if (!PeerVersion)							// wait for "version" message
		return;

	CoinEng& eng = static_cast<CoinEng&>(*Net);
	CCoinEngThreadKeeper engKeeper(&eng);

	eng.Events.OnPeriodicForLink(_self);

	bool bTrickled = Net->IsRandomlyTrickled();
	ptr<InvMessage> m = new InvMessage;
	EXT_LOCK (Mtx) {
		for (CInvertorySetToSend::iterator it = InvertorySetToSend.begin(), e = InvertorySetToSend.end(), tit; it != e && m->Invs.size() < ProtocolParam::MAX_INV_SZ;) {
			const Inventory& inv = *(tit = it++);
			if (inv.Type != InventoryType::MSG_TX || bTrickled) {
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
	if (HasWitness || eng.BestBlock().SafeHeight < eng.ChainParams.SegwitHeight)
		RequestBlocks();
}

void Link::Push(const Inventory& inv) {
	EXT_LOCK (Mtx) {
		if (!KnownInvertorySet.count(inv)) {
#ifdef _DEBUG//!!!D
			if (inv.Type == InventoryType::MSG_BLOCK) {
				int _wd = 2;
			}
#endif
			InvertorySetToSend.insert(inv);
		}
	}
}

void Link::Send(ptr<P2P::Message> msg) {
	CoinMessage& m = *static_cast<CoinMessage*>(msg.get());
	CoinEng& eng = static_cast<CoinEng&>(*Net);

	m.Trace(_self, true);

    MemoryStream stm;
	m.Write(ProtocolWriter(stm, m.WitnessAware));
    Span s(stm);

	SMessageHeader header = {
		htole(eng.ChainParams.ProtocolMagic),
		{ 0 },
		htole(uint32_t(s.size())),
		*(uint32_t*)eng.HashMessage(s).data()
	};
	strncpy(header.Command, m.Cmd, sizeof header.Command);

	MemoryStream stmMessage(sizeof header + s.size());
	BinaryWriter(stmMessage).WriteStruct(header);
	stmMessage.Write(s);
	SendBinary(stmMessage);
}

void FilterLoadMessage::Write(ProtocolWriter& wr) const {
	wr << *Filter;
}

void FilterLoadMessage::Read(const ProtocolReader& rd) {
	rd >> *(Filter = new CoinFilter);
	if ((Filter->Bitset.size() + 7) / 8 > MAX_BLOOM_FILTER_SIZE || Filter->HashNum > MAX_HASH_FUNCS)
		throw PeerMisbehavingException(100);
}

void FilterLoadMessage::Process(Link& link) {
	EXT_LOCK(link.MtxFilter) {
		link.Filter = Filter;
	}
	link.RelayTxes = true;
}

void FilterAddMessage::Write(ProtocolWriter& wr) const {
	CoinSerialized::WriteSpan(wr, Data);
}

void FilterAddMessage::Read(const ProtocolReader& rd) {
	Data = CoinSerialized::ReadBlob(rd);
	if (Data.size() > MAX_SCRIPT_ELEMENT_SIZE)
		throw PeerMisbehavingException(100);
}

void FilterAddMessage::Process(Link& link) {
	EXT_LOCK(link.MtxFilter) {
		if (!link.Filter)
			throw PeerMisbehavingException(100);
		link.Filter->Insert(Data);
	}
}

void RejectMessage::Write(ProtocolWriter& wr) const {
	WriteString(wr, Command);
	wr << (uint8_t)Code;
	WriteString(wr, Reason);
	if (Command == "block" || Command == "tx")
		wr << Hash;
}

void RejectMessage::Read(const ProtocolReader& rd) {
	Command = ReadString(rd);
	Code = (RejectReason)rd.ReadByte();
	Reason = ReadString(rd);
	if (Command == "block" || Command == "tx")
		rd >> Hash;
}

void RejectMessage::Print(ostream& os) const {
	base::Print(os);
	if (LinkPtr)
		os << "From " << LinkPtr->Peer->get_EndPoint() << ": ";
	os << "to command " << Command << ": " << Reason;
	if (Command == "block" || Command == "tx")
		os << " " << Hash;
}

void RejectMessage::Process(Link& link) {
	TRC(2, ToString());
}


void InvGetDataMessage::Write(ProtocolWriter& wr) const {
	CoinSerialized::Write(wr, Invs);
}

void InvGetDataMessage::Read(const ProtocolReader& rd) {
	CoinSerialized::Read(rd, Invs);
}

void InvGetDataMessage::Process(Link& link) {
	if (Invs.size() > ProtocolParam::MAX_INV_SZ)
		throw PeerMisbehavingException(20);
}

void InvGetDataMessage::Print(ostream& os) const {
	base::Print(os);
	os << Invs.size() << " invs: ";
	int n = 0;
	for (auto& inv : Invs) {
		if (n > 18) {
			os << "...";
			break;
		}
		if (inv.Type != InventoryType::MSG_TX && inv.Type != InventoryType::MSG_WITNESS_TX || (CTrace::s_nLevel & (1 << TRC_LEVEL_TX_MESSAGE))) {
			os << (n ? ", " : "   ") << inv;
			++n;
		}
	}
}

void InvGetDataMessage::Trace(Link& link, bool bSend) const {
	if (!(CTrace::s_nLevel & (1 << TRC_LEVEL_TX_MESSAGE))) {
		for (auto& inv : Invs)
			if (inv.Type != InventoryType::MSG_TX && inv.Type != InventoryType::MSG_WITNESS_TX)
				goto LAB_TRACE;
		return;
	}
LAB_TRACE:
	base::Trace(link, bSend);
}

vector<Tx> MerkleBlockMessage::Init(const Coin::Block& block, CoinFilter& filter) {
	Block = block;
	const CTxes& txes = block.get_Txes();
	PartialMT.NItems = txes.size();

	vector<HashValue> vHash(PartialMT.NItems);
	dynamic_bitset<uint8_t> bsMatch(PartialMT.NItems);
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

void MerkleBlockMessage::Write(ProtocolWriter& wr) const {
	Block->WriteHeader(wr);
	wr << uint32_t(PartialMT.NItems);
	CoinSerialized::Write(wr, PartialMT.Items);
	vector<uint8_t> v;
	to_block_range(PartialMT.Bitset, back_inserter(v));
	CoinSerialized::WriteSpan(wr, Span(&v[0], v.size()));
}

void MerkleBlockMessage::Read(const ProtocolReader& rd) {
	Block->ReadHeader(rd, false, 0);
	Block->m_bTxesLoaded = true;
	PartialMT.NItems = rd.ReadUInt32();
	CoinSerialized::Read(rd, PartialMT.Items);
	Blob blob = CoinSerialized::ReadBlob(rd);
	const uint8_t *p = blob.constData();
	PartialMT.Bitset.resize(blob.size() * 8);
	for (int i = 0; i < blob.size(); ++i)
		for (int j = 0; j < 8; ++j) {
			PartialMT.Bitset.replace(i * 8 + j, (p[i] >> j) & 1);
		}
}

void TxMessage::Write(ProtocolWriter& wr) const {
    Tx.Write(wr);
}

void TxMessage::Read(const ProtocolReader& rd) {
	Tx.Read(rd);
}

void TxMessage::Print(ostream& os) const {
	base::Print(os);
	os << Hash(Tx);
}

void BlockMessage::Write(ProtocolWriter& wr) const {
	if (Block)
		Block.Write(wr);
	else
		Eng().Db->CopyTo(Offset, Size, wr.BaseStream);
}

void BlockMessage::Read(const ProtocolReader& rd) {
	Block = Coin::Block();
	Block.Read(rd);
}

void BlockMessage::Print(ostream& os) const {
	base::Print(os);
	if (Block)
		os << Eng().BlockStringId(Hash(Block));
	else
		os << "Offset: " << Offset << ", Size: " << Size;
}

bool CoinEng::CheckSelfVerNonce(uint64_t nonce) {
	bool bSelf = false;
	EXT_LOCK (m_mtxVerNonce) {
		 if (bSelf = m_nonce2link.count(nonce))
			m_nonce2link[nonce]->OnSelfLink();
	}
	return !bSelf;
}

void CoinEng::RemoveVerNonce(Link& link) {
	EXT_LOCK (m_mtxVerNonce) {
		for (CNonce2link::iterator it = m_nonce2link.begin(), e = m_nonce2link.end(); it != e; ++it)
			if (it->second.get() == &link) {
				m_nonce2link.erase(it);
				break;
			}
	}
}

JumpAction CoinEng::TryJumpToBlockchain(int sym, Link *link) {
	String symbol = Convert::MulticharToString(sym);
	if (symbol == ChainParams.Symbol)
		return JumpAction::Continue;
	TRC(2, symbol);
	if (!link)
		return JumpAction::Break;
	EXT_LOCK(link->NetManager->MtxNets) {
		EXT_FOR(P2P::Net * net, link->NetManager->m_nets) {
			CoinEng* e = dynamic_cast<CoinEng*>(net);
			if (e->ChainParams.Symbol == symbol) {
				link->Net.reset(net);
				return JumpAction::Retry;
			}
		}
		link->Stop();
		return JumpAction::Break;
	}
}

void VersionMessage::Process(Link& link) {
	if (ProtocolVer < ProtocolVersion::MIN_PEER_PROTO_VERSION)
		Throw(ExtErr::ObsoleteProtocolVersion);

	int nRepeated = 0;
LAB_REPEAT:
	CoinEng& eng = static_cast<CoinEng&>(*link.Net);
	if (link.PeerVersion)
		Throw(CoinErr::DupVersionMessage);

	if (auto sym = DetectBlockchain(UserAgent)) {
		switch (eng.TryJumpToBlockchain(sym, &link)) {
		case JumpAction::Retry:
			if (!nRepeated++)
				goto LAB_REPEAT;
		case JumpAction::Break:
			return;
		}
	}

	if (eng.CheckSelfVerNonce(Nonce)) {
		link.PeerVersion = ProtocolVer;
		link.TimeOffset = RemoteTimestamp - Timestamp;
		link.LastReceivedBlock = LastReceivedBlock;
		link.RelayTxes = RelayTxes;
		link.IsClient = !(Services & NodeServices::NODE_NETWORK) && !(Services & NodeServices::NODE_NETWORK_LIMITED);
		link.IsLimitedNode = !(Services & NodeServices::NODE_NETWORK) && (Services & NodeServices::NODE_NETWORK_LIMITED);
        link.HasWitness = Services & NodeServices::NODE_WITNESS;
		link.Peer->Services = Services;

		eng.aPreferredDownloadPeers += (link.IsPreferredDownload = !link.Incoming && !link.IsOneShot && !link.IsClient);

		if (link.Incoming)
			eng.SendVersionMessage(link);
		link.Send(new VerackMessage);

		if (!link.Incoming)
			link.Send(new GetAddrMessage);

		if (eng.Filter) {
			link.Send(new FilterLoadMessage(eng.Filter.get()));
		}
		link.RequestHeaders();
	}
	if (link.Incoming)
		if (ptr<Peer> p = link.Net->Add(link.Tcp.Client.RemoteEndPoint, Services, Timestamp))
			link.Peer = p;
	eng.Good(link.Peer);
}

void SubmitOrderMessage::Write(ProtocolWriter& wr) const {
	wr << TxHash;
}

void SubmitOrderMessage::Read(const ProtocolReader& rd) {
	rd >> TxHash;
}

void ReplyMessage::Write(ProtocolWriter& wr) const {
	wr << Code;
}

void ReplyMessage::Read(const ProtocolReader& rd) {
	Code = rd.ReadUInt32();
}

void PingPongMessage::Write(ProtocolWriter& wr) const {
	wr << Nonce;
}

void PingPongMessage::Read(const ProtocolReader& rd) {
	rd >> Nonce;
}

void PingMessage::Read(const ProtocolReader& rd) {
	if (LinkPtr->PeerVersion >= 60001 && !rd.BaseStream.Eof())
		base::Read(rd);
}

void PingMessage::Process(Link& link) {
	ptr<PongMessage> m = new PongMessage;
	m->Nonce = Nonce;
	link.Send(m);
}

void PongMessage::Process(Link& link) {
	if (link.PingNonceSent == Nonce) {
		link.MinPingTime = (min)(link.MinPingTime, Timestamp - link.LastPingTimestamp);
	} else {
		TRC(2, link.PingNonceSent ? "Nonce mismatch" : "Unsolicited pong without ping");
	}
	link.PingNonceSent = 0;
}

void SendHeadersMessage::Process(Link& link) {
    link.PreferHeaders = true;
}

void FeeFilterMessage::Print(ostream& os) const {
	base::Print(os);
	os << "MinFeeRate: " << MinFeeRate;
}

void FeeFilterMessage::Write(ProtocolWriter& wr) const {
    wr << MinFeeRate;
}

void FeeFilterMessage::Read(const ProtocolReader& rd) {
    rd >> MinFeeRate;
}

void FeeFilterMessage::Process(Link& link) {
	link.MinFeeRate.store(MinFeeRate);
}

ProtocolWriter& operator<<(ProtocolWriter& wr, const CompactSize& cs) {
    CoinSerialized::WriteCompactSize(wr, cs);
    return wr;
}

const ProtocolReader& operator>>(const ProtocolReader& rd, CompactSize& cs) {
    auto v = CoinSerialized::ReadCompactSize64(rd);
    if (v > UINT16_MAX)
        Throw(ExtErr::Protocol_Violation);
    cs = CompactSize((uint16_t)v);
    return rd;
}

void SendCompactBlockMessage::Write(ProtocolWriter& wr) const {
    wr << (uint8_t)Announce << CompactBlockVersion;
}

void SendCompactBlockMessage::Read(const ProtocolReader& rd) {
    rd >> Announce >> CompactBlockVersion;
}

void SendCompactBlockMessage::Process(Link& link) {
    if (CompactBlockVersion == 1 || link.Peer->Services & (uint64_t)NodeServices::NODE_WITNESS && CompactBlockVersion == 2) {
		link.PreferHeaderAndIDs = Announce;
		link.SupportsDesiredCmpctVersion = true;
        if (CompactBlockVersion == 2)
            link.WantsCompactWitness = true;
    }
}

// BIP152
CompactBlockMessage::CompactBlockMessage(const Block& block, bool bUseWtxid)
	: CompactBlockMessage()
{
	CoinEng& eng = Eng();

	Header = block;
	GetSystemURandomReader() >> Nonce;
	auto& txes = block.get_Txes();
	ShortTxIds.resize(txes.size() - 1);
	PrefilledTxes.push_back(make_pair(uint16_t(0), txes[0]));

	MemoryStream ms;
	block.WriteHeader(ProtocolWriter(ms));
	hashval hv = SHA256().ComputeHash(ms.AsSpan());
	uint64_t keys[2] = {
		letoh(*(uint64_t*)(hv.data())),
		letoh(*(uint64_t*)(hv.data() + 8)),
	};
	for (size_t i = 1; i < txes.size(); ++i) {
		ShortTxIds[i-1] = GetShortTxId(eng.HashFromTx(txes[i], bUseWtxid), keys);
	}
}

ShortTxId CompactBlockMessage::GetShortTxId(const HashValue& hash, const uint64_t keys[2]) {
	hashval hv = SipHash2_4(keys[0], keys[1]).ComputeHash(hash.ToSpan());
	ShortTxId r;
	memcpy(r.Data, hv.data(), 6);
	return r;
}

void CompactBlockMessage::Write(ProtocolWriter& wr) const {
    Header.WriteHeader(wr);
    wr << Nonce;
    CoinSerialized::Write(wr, ShortTxIds);

    CoinSerialized::WriteCompactSize(wr, PrefilledTxes.size());
    uint16_t prev = uint16_t(-1);
    for (auto& pp : PrefilledTxes) {
        CoinSerialized::WriteCompactSize(wr, pp.first - exchange(prev, pp.first) - 1);
        pp.second.Write(wr);
    }
}

void CompactBlockMessage::Read(const ProtocolReader& rd) {
    CoinEng& eng = Eng();

    Header.m_pimpl = eng.CreateBlockObj();
    Header.ReadHeader(rd);
    rd >> Nonce;
    CoinSerialized::Read(rd, ShortTxIds);

    auto size = CoinSerialized::ReadCompactSize64(rd);
    if (size > UINT16_MAX)
        Throw(ExtErr::Protocol_Violation);
    PrefilledTxes.resize((size_t)size);
    for (size_t i = 0; i < PrefilledTxes.size(); ++i) {
        auto off = CoinSerialized::ReadCompactSize64(rd);
        auto idx = off + (i ? PrefilledTxes[i - 1].first + 1 : 0);
        if (off > UINT16_MAX || idx > UINT16_MAX)
            Throw(ExtErr::Protocol_Violation);
        PrefilledTxes[i].second.Read(rd);
    }
}

void CompactBlockMessage::Print(ostream& os) const {
	base::Print(os);
	os << Eng().BlockStringId(Hash(Header));
}

void CompactBlockMessage::Process(Link& link) {
	CoinEng& eng = Eng();

	if (!eng.Tree.FindInMap(Header.PrevBlockHash)) {
		BlockHeader bh = eng.BestHeader();
		link.Send(new GetHeadersMessage(bh ? Hash(bh) : HashValue()));
		return;
	}

	vector<BlockHeader> headers(1, Header);
	if (BlockHeader headerLast = eng.ProcessNewBlockHeaders(headers, &link)) {
		link.UpdateBlockAvailability(Hash(headerLast));
	} else
		return;
}

void GetBlockTransactionsMessage::Write(ProtocolWriter& wr) const {
    wr << HashBlock;
    CoinSerialized::WriteCompactSize(wr, Indexes.size());
    uint16_t prev = uint16_t(-1);
    for (auto idx : Indexes)
        CoinSerialized::WriteCompactSize(wr, idx - exchange(prev, idx) - 1);
}

void GetBlockTransactionsMessage::Read(const ProtocolReader& rd) {
    rd >> HashBlock;
    auto n = CoinSerialized::ReadCompactSize64(rd);
    if (n > UINT16_MAX)
        Throw(ExtErr::Protocol_Violation);
    Indexes.resize(n);
    for (size_t i = 0; i < n; ++i) {
        auto off = CoinSerialized::ReadCompactSize64(rd);
        auto idx = off + (i ? Indexes[i - 1] + 1 : 0);
        if (off > UINT16_MAX || idx > UINT16_MAX)
            Throw(ExtErr::Protocol_Violation);
        Indexes[i] = (uint16_t)idx;
    }
}

void GetBlockTransactionsMessage::Process(Link& link) {
    CoinEng& eng = Eng();
	if (auto block = eng.Tree.FindBlock(HashBlock)) {
		ptr<BlockTransactionsMessage> m = new BlockTransactionsMessage;
		m->HashBlock = HashBlock;
		m->Txes.resize(Indexes.size());
		for (size_t i = 0; i < Indexes.size(); ++i)
			m->Txes[i] = block.get_Txes().at(Indexes[i]);
		link.Send(m);
	} else {
		TRC(2, "Unexpcted GETBLOCKTXN for block  " << HashBlock);
	}
}

void BlockTransactionsMessage::Write(ProtocolWriter& wr) const {
/*!!!?    CoinLink& clink = static_cast<CoinLink&>(link);
    if (clink.WantsCompactWitness)
    	wr.WitnessAware = true; */
    wr << HashBlock;
    CoinSerialized::Write(wr, Txes);
}

void BlockTransactionsMessage::Read(const ProtocolReader& rd) {
    rd >> HashBlock;
    CoinSerialized::Read(rd, Txes, UINT16_MAX);
}

void BlockTransactionsMessage::Process(Link& link) {
}

} // Coin::
