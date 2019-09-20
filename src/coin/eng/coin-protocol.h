/*######   Copyright (c) 2012-2019 Ufasoft  http://ufasoft.com  mailto:support@ufasoft.com,  Sergey Pavlov  mailto:dev@ufasoft.com ####
#                                                                                                                                     #
# 		See LICENSE for licensing information                                                                                         #
#####################################################################################################################################*/

#pragma once

#include <el/inet/p2p-net.h>
using namespace Ext::Inet;
using P2P::Peer;

#include "coin-model.h"
#include "eng.h"

namespace Coin {

class ProtocolVersion {
public:
	static const uint32_t
		PROTOCOL_VERSION			= 70015
		, INVALID_CB_NO_BAN_VERSION	= 70015
		, SHORT_IDS_BLOCKS_VERSION	= 70014
		, FEEFILTER_VERSION			= 70013		// BIP133
		, SENDHEADERS_VERSION		= 70012
		, NO_BLOOM_VERSION			= 70011
		, MIN_PEER_PROTO_VERSION	= 31800;	//  "getheaders" message
};

class ProtocolParam {
public:
	static const unsigned
		MAX_PROTOCOL_MESSAGE_LENGTH = 4 * 1000 * 1000
		, MAX_INV_SZ			= 50000
		, MAX_ADDR_SZ			= 1000
		, MAX_LOCATOR_SZ		= 101
		, MAX_HEADERS_RESULTS = 2000;
};

struct SMessageHeader {
	uint32_t Magic;
	char Command[12];
	uint32_t Length, Checksum;
};

class CoinEng;

ENUM_CLASS(InventoryType) {
	MSG_TX 						= 1
	, MSG_BLOCK 				= 2
	, MSG_FILTERED_BLOCK 		= 3
	, MSG_CMPCT_BLOCK 			= 4
	, MSG_WITNESS_FLAG 			= 1 << 30
	, MSG_WITNESS_BLOCK 		= MSG_BLOCK 			| MSG_WITNESS_FLAG
	, MSG_WITNESS_TX 			= MSG_TX 				| MSG_WITNESS_FLAG
	, MSG_FILTERED_WITNESS_BLOCK = MSG_FILTERED_BLOCK 	| MSG_WITNESS_FLAG
} END_ENUM_CLASS(InventoryType);

class Inventory : public CPersistent, public CPrintable {
public:
	Coin::HashValue HashValue;
	InventoryType Type;

	Inventory() {}

	Inventory(InventoryType typ, const Coin::HashValue& hash)
		: HashValue(hash)
		, Type(typ)
	{}

	bool operator==(const Inventory& inv) const {
		return Type == inv.Type && HashValue == inv.HashValue;
	}

	void Write(BinaryWriter& wr) const override;
	void Read(const BinaryReader& rd) override;
protected:
	void Print(ostream& os) const override;
};

} namespace std {
	template <> struct hash<Coin::Inventory> {
		size_t operator()(const Coin::Inventory& inv) const {
			return hash<Coin::HashValue>()(inv.HashValue) + int(inv.Type);
		}
	};
} namespace Coin {

class Link : public P2P::Link {
	typedef P2P::Link base;
public:
	HashValue
		HashContinue,
		HashBlockLastUnknown,
		HashBlockLastCommon,
		HashBlockBestKnown,
		HashBestHeaderSent;

	LruCache<Inventory> KnownInvertorySet;

	typedef unordered_set<Inventory> CInvertorySetToSend;
	CInvertorySetToSend InvertorySetToSend;
	//---- Under Mtx

	Block m_curMerkleBlock;						// accessed in Link thread
	vector<HashValue> m_curMatchedHashes;		// accessed in Link thread

	//-- locked by Eng.Mtx
	BlocksInFlightList BlocksInFlight;
	//----

	mutex MtxFilter;
	ptr<CoinFilter> Filter;
	atomic<int64_t> MinFeeRate;
	uint64_t PingNonceSent;
	int32_t LastReceivedBlock;

	CBool
		RelayTxes,
		IsClient,
		IsLimitedNode,
        IsPreferredDownload,
        IsSyncStarted,
        HasWitness,
        PreferHeaders,
		PreferHeaderAndIDs,
		SupportsDesiredCmpctVersion,
        WantsCompactWitness;

	Link(P2P::NetManager& netManager, thread_group& tr);
	void Push(const Inventory& inv);
	void Send(ptr<P2P::Message> msg) override;
	void UpdateBlockAvailability(const HashValue& hashBlock);
	void RequestHeaders();
	void RequestBlocks();
	bool HasHeader(const BlockHeader& header);
protected:
	void OnPeriodic(const DateTime& now) override;
private:
	void SetHashBlockBestKnown(const HashValue& hash);
};

class VersionMessage : public CoinMessage {
	typedef CoinMessage base;
public:
	PeerInfoBase RemotePeerInfo, LocalPeerInfo;
	uint64_t Nonce, Services;
	mutable DateTime RemoteTimestamp;
	String UserAgent;
	uint32_t ProtocolVer;
	int32_t LastReceivedBlock;
	bool RelayTxes;

	VersionMessage();
protected:
	void Write(ProtocolWriter& wr) const override;
	void Read(const ProtocolReader& rd) override;
	void Process(Link& link) override;
	void Print(ostream& os) const override;
};

class VerackMessage : public CoinMessage {
	typedef CoinMessage base;
public:
	VerackMessage()
		: base("verack")
	{
	}
protected:
	void Process(Link& link) override;
};

class GetAddrMessage : public CoinMessage {
	typedef CoinMessage base;
public:
	GetAddrMessage()
		: base("getaddr")
	{}
protected:
	void Process(Link& link) override;
};

class AddrMessage : public CoinMessage {
	typedef CoinMessage base;
public:
	AddrMessage()
		: base("addr")
	{}

	vector<PeerInfo> PeerInfos;
protected:
	void Write(ProtocolWriter& wr) const override;
	void Read(const ProtocolReader& rd) override;
	void Print(ostream& os) const override;
	void Process(Link& link) override;
};

class InvGetDataMessage : public CoinMessage {
	typedef CoinMessage base;
public:
	vector<Inventory> Invs;
protected:
	InvGetDataMessage(const char *cmd)
		: base(cmd)
	{}

	void Print(ostream& os) const override;
	void Trace(Link& link, bool bSend) const override;
	void Write(ProtocolWriter& wr) const override;
	void Read(const ProtocolReader& rd) override;
	void Process(Link& link) override;
};

class InvMessage : public InvGetDataMessage {
	typedef InvGetDataMessage base;
public:
	InvMessage()
		: base("inv")
	{}
protected:
	void Process(Link& link) override;
};

class GetDataMessage : public InvGetDataMessage {
	typedef InvGetDataMessage base;
public:
	GetDataMessage()
		: base("getdata")
	{}

	void Process(Link& link) override;
};

class NotFoundMessage : public InvGetDataMessage {
	typedef InvGetDataMessage base;
public:
	NotFoundMessage()
		: base("notfound")
	{}

	void Process(Link& link) override;
};

class SendHeadersMessage : public CoinMessage {
	typedef CoinMessage base;
public:
	SendHeadersMessage() : base("sendheaders") {}
protected:
	void Process(Link& link) override;
};

class HeadersMessage : public CoinMessage {
	typedef CoinMessage base;
public:
	vector<BlockHeader> Headers;

	HeadersMessage()
		: base("headers")
	{}
protected:
	void Write(ProtocolWriter& wr) const override;
	void Read(const ProtocolReader& rd) override;
	void Print(ostream& os) const override;
	void Process(Link& link) override;
};

class GetHeadersGetBlocksMessage : public CoinMessage {
	typedef CoinMessage base;
public:
	HashValue HashStop;
	LocatorHashes Locators;
	uint32_t Ver;

	GetHeadersGetBlocksMessage(const char *cmd)
		: base(cmd)
		, Ver(Eng().ChainParams.ProtocolVersion)
	{}

	void Set(const HashValue& hashLast, const HashValue& hashStop);
protected:
	void Write(ProtocolWriter& wr) const override;
	void Read(const ProtocolReader& rd) override;
	void Print(ostream& os) const override;
};

class GetHeadersMessage : public GetHeadersGetBlocksMessage {
	typedef GetHeadersGetBlocksMessage base;
public:
	GetHeadersMessage()
		: base("getheaders")
	{}

	GetHeadersMessage(const HashValue& hashLast, const HashValue& hashStop = HashValue::Null());
protected:
	void Process(Link& link) override;
};

class GetBlocksMessage : public GetHeadersGetBlocksMessage {
	typedef GetHeadersGetBlocksMessage base;
public:
	GetBlocksMessage()
		: base("getblocks")
	{}

	GetBlocksMessage(const HashValue& hashLast, const HashValue& hashStop);
protected:
	void Process(Link& link) override;
};

class TxMessage : public CoinMessage {
	typedef CoinMessage base;
public:
	Coin::Tx Tx;

	TxMessage(const Coin::Tx& tx = Coin::Tx())
		: base("tx")
		, Tx(tx)
	{}

	void Process(Link& link) override;
protected:
	void Trace(Link& link, bool bSend) const override;
	void Write(ProtocolWriter& wr) const override;
	void Read(const ProtocolReader& rd) override;
	void Print(ostream& os) const override;
};

class BlockMessage : public CoinMessage {
	typedef CoinMessage base;
public:
	Coin::Block Block;
	uint64_t Offset;
	uint32_t Size;

	BlockMessage(const Coin::Block& block = nullptr, const char *cmd = "block")
		: base(cmd)
		, Block(block)
	{}

	void Write(ProtocolWriter& wr) const override;
	void Read(const ProtocolReader& rd) override;
protected:
	virtual void ProcessContent(Link& link, bool bRequested);
	void Process(Link& link) override;
	void Print(ostream& os) const override;
};

class MerkleBlockMessage : public BlockMessage {
	typedef BlockMessage base;
public:
	CoinPartialMerkleTree PartialMT;

	MerkleBlockMessage()
		: base(Coin::Block(), "merkleblock")
	{}

	vector<Tx> Init(const Coin::Block& block, CoinFilter& filter);
	void Write(ProtocolWriter& wr) const override;
	void Read(const ProtocolReader& rd) override;
protected:
	void ProcessContent(Link& link, bool bRequested) override;
	void Process(Link& link) override;
};

class CheckOrderMessage : public CoinMessage {
	typedef CoinMessage base;
public:
	CheckOrderMessage()
		: base("checkorder")
	{}
};

class SubmitOrderMessage : public CoinMessage {
	typedef CoinMessage base;
public:
	HashValue TxHash;

	SubmitOrderMessage()
		: base("submitorder")
	{}

	void Write(ProtocolWriter& wr) const override;
	void Read(const ProtocolReader& rd) override;
};

class ReplyMessage : public CoinMessage {
	typedef CoinMessage base;
public:
	uint32_t Code;

	ReplyMessage()
		: base("reply")
	{}

	void Write(ProtocolWriter& wr) const override;
	void Read(const ProtocolReader& rd) override;
};

class PingPongMessage : public CoinMessage {
	typedef CoinMessage base;
public:
	uint64_t Nonce;

	PingPongMessage(const char *cmd)
		: base(cmd)
	{}

	void Write(ProtocolWriter& wr) const override;
	void Read(const ProtocolReader& rd) override;
};

class PingMessage : public PingPongMessage {
	typedef PingPongMessage base;
public:
	PingMessage(int64_t nonce = 0)
		: base("ping")
	{
		Nonce = nonce;
	}

	void Read(const ProtocolReader& rd) override;
	void Process(Link& link) override;
};

class PongMessage : public PingPongMessage {
	typedef PingPongMessage base;
public:
	PongMessage()
		: base("pong")
	{
		Nonce = 0;
	}

	void Process(Link& link) override;
};

class MemPoolMessage : public CoinMessage {
	typedef CoinMessage base;
public:
	MemPoolMessage()
		: base("mempool")
	{}

	void Process(Link& link) override;
};

class AlertMessage : public CoinMessage {
	typedef CoinMessage base;
public:
	Blob Payload;
	Blob Signature;
	ptr<Coin::Alert> Alert;

	AlertMessage()
		: base("alert")
	{}

	void Write(ProtocolWriter& wr) const override;
	void Read(const ProtocolReader& rd) override;
protected:
	void Process(Link& link) override;
};

class FilterLoadMessage : public CoinMessage {
	typedef CoinMessage base;
public:
	ptr<CoinFilter> Filter;

	FilterLoadMessage(CoinFilter *filter = 0)
		: base("filterload")
		, Filter(filter)
	{}

	void Write(ProtocolWriter& wr) const override;
	void Read(const ProtocolReader& rd) override;
protected:
	void Process(Link& link) override;
};

class FilterAddMessage : public CoinMessage {
	typedef CoinMessage base;
public:
	Blob Data;

	FilterAddMessage()
		: base("filteradd")
	{}

	void Write(ProtocolWriter& wr) const override;
	void Read(const ProtocolReader& rd) override;
protected:
	void Process(Link& link) override;
};

class FilterClearMessage : public CoinMessage {
	typedef CoinMessage base;
public:
	FilterClearMessage()
		: base("filterclear")
	{}

protected:
	void Process(Link& link) override {
		EXT_LOCKED(link.MtxFilter, link.Filter = nullptr);
		link.RelayTxes = true;
	}
};

ENUM_CLASS(RejectReason) {
	Malformed 			= 1
	, Invalid 			= 0x10
	, Obsolete			= 0x11
	, Duplicate			= 0x12
	, NonStandard		= 0x40
	, Dust				= 0x41
	, InsufficientFee	= 0x42
	, CheckPoint		= 0x43
	, TxMissingInputs	= 0x44
	, TxPrematureSpend	= 0x45
} END_ENUM_CLASS(RejectReason);

class RejectMessage : public CoinMessage {
	typedef CoinMessage base;
public:
	HashValue Hash;
	String Command, Reason;
	RejectReason Code;

	RejectMessage(RejectReason code = RejectReason::Malformed, RCString command = String(), RCString reason = String())
		: base("reject")
		, Code(code)
		, Command(command)
		, Reason(reason)
	{}

	void Write(ProtocolWriter& wr) const override;
	void Read(const ProtocolReader& rd) override;
	void Print(ostream& os) const override;
protected:
	void Process(Link& link) override;
};

class FeeFilterMessage : public CoinMessage {
    typedef CoinMessage base;
public:
    int64_t MinFeeRate;

    FeeFilterMessage(int64_t minFeeRate = 0)
		: base("feefilter")
		, MinFeeRate(minFeeRate)
	{}
protected:
	void Print(ostream& os) const override;
    void Write(ProtocolWriter& wr) const override;
    void Read(const ProtocolReader& rd) override;
    void Process(Link& link) override;
};

class SendCompactBlockMessage : public CoinMessage {
    typedef CoinMessage base;
public:
	uint64_t CompactBlockVersion;
	bool Announce;

    SendCompactBlockMessage(uint64_t compactBlockVersion = 0, bool announce = false)
		: base("sendcmpct")
		, CompactBlockVersion(compactBlockVersion)
		, Announce(announce)
	{}
protected:
    void Write(ProtocolWriter& wr) const override;
    void Read(const ProtocolReader& rd) override;
    void Process(Link& link) override;
};

class CompactSize {
public:
    uint16_t Value;

    explicit CompactSize(uint16_t v) : Value(v) {}
    operator uint16_t() const { return Value; }
};

ProtocolWriter& operator<<(ProtocolWriter& wr, const CompactSize& cs);
const ProtocolReader& operator>>(const ProtocolReader& rd, CompactSize& cs);

class CompactBlockMessage : public CoinMessage {
    typedef CoinMessage base;
public:
    BlockHeader Header;
    uint64_t Nonce;
    vector<ShortTxId> ShortTxIds;
    vector<pair<uint16_t, Tx>> PrefilledTxes;

    CompactBlockMessage() : base("cmpctblock") {}
	CompactBlockMessage(const Block& block, bool bUseWtxid);
protected:
    void Write(ProtocolWriter& wr) const override;
    void Read(const ProtocolReader& rd) override;
	void Print(ostream& os) const override;
	void Process(Link& link) override;
private:
	static ShortTxId GetShortTxId(const HashValue& hash, const uint64_t keys[2]);
};

class GetBlockTransactionsMessage : public CoinMessage {
    typedef CoinMessage base;
public:
    HashValue HashBlock;
    vector<uint16_t> Indexes;

    GetBlockTransactionsMessage() : base("getblocktxn") {}
protected:
    void Write(ProtocolWriter& wr) const override;
    void Read(const ProtocolReader& rd) override;
    void Process(Link& link) override;
};

class BlockTransactionsMessage : public CoinMessage {
    typedef CoinMessage base;
public:
    HashValue HashBlock;
    vector<Tx> Txes;

    BlockTransactionsMessage() : base("blocktxn") {}
protected:
    void Write(ProtocolWriter& wr) const override;
    void Read(const ProtocolReader& rd) override;
    void Process(Link& link) override;
};

} // Coin::
