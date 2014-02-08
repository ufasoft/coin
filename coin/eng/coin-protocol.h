/*######     Copyright (c) 1997-2013 Ufasoft  http://ufasoft.com  mailto:support@ufasoft.com,  Sergey Pavlov  mailto:dev@ufasoft.com #######################################
#                                                                                                                                                                          #
# This program is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation;  #
# either version 3, or (at your option) any later version. This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the      #
# implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details. You should have received a copy of the GNU #
# General Public License along with this program; If not, see <http://www.gnu.org/licenses/>                                                                               #
##########################################################################################################################################################################*/

#pragma once


#include <el/inet/p2p-net.h>
using namespace Ext::Inet;
using P2P::Link;
using P2P::Peer;

#include "coin-model.h"
#include "eng.h"

namespace Coin {
	

class CoinEng;

ENUM_CLASS(InventoryType) {
	MSG_TX = 1,
	MSG_BLOCK,
	MSG_FILTERED_BLOCK
} END_ENUM_CLASS(InventoryType);

class Inventory : public CPersistent, public CPrintable {
public:
	InventoryType Type;
	Coin::HashValue HashValue;

	Inventory() {}

	Inventory(InventoryType typ, const Coin::HashValue& hash)
		:	Type(typ)
		,	HashValue(hash)
	{}

	bool operator==(const Inventory& inv) const {
		return Type==inv.Type && HashValue==inv.HashValue;
	}

	void Write(BinaryWriter& wr) const override {
		wr << UInt32(Type) << HashValue;
	}

	void Read(const BinaryReader& rd) override {
		UInt32 typ;
		rd >> typ >> HashValue;
		Type = InventoryType(typ);
	}
protected:
	String ToString() const override;
};

} namespace std {
	template <> struct hash<Coin::Inventory> {
		size_t operator()(const Coin::Inventory& inv) const {
			return hash<Coin::HashValue>()(inv.HashValue) + int(inv.Type);
		}
	};
} namespace Coin {

class CoinLink : public P2P::Link {
	typedef P2P::Link base;
public:
	Int32 LastReceivedBlock;
	LruCache<Inventory> KnownInvertorySet;
	
	typedef unordered_set<Inventory> CInvertorySetToSend;
	CInvertorySetToSend InvertorySetToSend;

	HashValue HashContinue;

	mutex MtxFilter;
	ptr<CoinFilter> Filter;
	CBool RelayTxes;	

	CoinLink(P2P::NetManager& netManager, thread_group& tr)
		:	base(&netManager, &tr)
		,	LastReceivedBlock(-1)
	{}

	void Push(const Inventory& inv);
	void Send(ptr<P2P::Message> msg) override;
protected:
	void OnPeriodic() override;
	P2P::Message *CreatePingMessage() override;
};


class VersionMessage : public CoinMessage {
	typedef CoinMessage base;
public:
	UInt64 Nonce, Services;
	UInt32 ProtocolVer;
	String UserAgent;
	Int32 LastReceivedBlock;
	mutable DateTime RemoteTimestamp;
	PeerInfoBase RemotePeerInfo, LocalPeerInfo;
	bool RelayTxes;

	VersionMessage();
protected:
	void Write(BinaryWriter& wr) const override;
	void Read(const BinaryReader& rd) override;
	void Process(P2P::Link& link) override;
	String ToString() const override;
};

class VerackMessage : public CoinMessage {
	typedef CoinMessage base;
public:
	VerackMessage()
		:	base("verack")
	{
//!!!R?		m_bHasChecksum = false;
	}
protected:
	void Process(P2P::Link& link) override;
};

class GetAddrMessage : public CoinMessage {
	typedef CoinMessage base;
public:
	GetAddrMessage()
		:	base("getaddr")
	{}
protected:
	void Process(P2P::Link& link) override;
};

class AddrMessage : public CoinMessage {
	typedef CoinMessage base;
public:
	AddrMessage()
		:	base("addr")
	{}

	vector<PeerInfo> PeerInfos;
protected:
	void Write(BinaryWriter& wr) const override;
	void Read(const BinaryReader& rd) override;
	void Process(P2P::Link& link) override;
};

class InvGetDataMessage : public CoinMessage {
	typedef CoinMessage base;
public:
	vector<Inventory> Invs;
protected:
	InvGetDataMessage(RCString cmd)
		:	base(cmd)
	{}

	void Write(BinaryWriter& wr) const override;
	void Read(const BinaryReader& rd) override;
	void Process(P2P::Link& link) override;
	String ToString() const override;
};

class InvMessage : public InvGetDataMessage {
	typedef InvGetDataMessage base;
public:
	InvMessage()
		:	base("inv")
	{}
protected:
	void Process(P2P::Link& link) override;
};

class GetDataMessage : public InvGetDataMessage {
	typedef InvGetDataMessage base;
public:
	GetDataMessage()
		:	base("getdata")
	{}

	void Process(P2P::Link& link) override;
};

class HeadersMessage : public CoinMessage {
	typedef CoinMessage base;
public:
	vector<Block> Headers;

	HeadersMessage()
		:	base("headers")
	{}
protected:
	void Write(BinaryWriter& wr) const override;
	void Read(const BinaryReader& rd) override;
};

class GetHeadersMessage : public CoinMessage {
	typedef CoinMessage base;
public:
	LocatorHashes Locators;
	HashValue HashStop;

	GetHeadersMessage()
		:	base("getheaders")
	{}

protected:
	void Write(BinaryWriter& wr) const override;
	void Read(const BinaryReader& rd) override;
	void Process(P2P::Link& link) override;
};

class GetBlocksMessage : public CoinMessage {
	typedef CoinMessage base;
public:
	UInt32 Ver;
	LocatorHashes Locators;
	HashValue HashStop;

	GetBlocksMessage()
		:	base("getblocks")
		,	Ver(Eng().ChainParams.ProtocolVersion)
	{}	

	GetBlocksMessage(const Block& blockLast, const HashValue& hashStop);
	void Set(const Block& blockLast, const HashValue& hashStop);
protected:
	void Write(BinaryWriter& wr) const override;
	void Read(const BinaryReader& rd) override;
	void Process(P2P::Link& link) override;
	String ToString() const override;
};

class TxMessage : public CoinMessage {
	typedef CoinMessage base;
public:
	Coin::Tx Tx;

	TxMessage(const Coin::Tx& tx = Coin::Tx())
		:	base("tx")
		,	Tx(tx)
	{}

	void Write(BinaryWriter& wr) const override {
		wr << Tx;
	}

	void Read(const BinaryReader& rd) override {
		rd >> Tx;
	}

	void Process(P2P::Link& link) override;
protected:
	String ToString() const override;
};

class BlockMessage : public CoinMessage {
	typedef CoinMessage base;
public:
	Coin::Block Block;

	BlockMessage(const Coin::Block& block = nullptr)
		:	base("block")
		,	Block(block)
	{}

	void Write(BinaryWriter& wr) const override {
		wr << Block;
	}

	void Read(const BinaryReader& rd) override {
		Block = Coin::Block();
		rd >> Block;
	}
protected:
	void Process(P2P::Link& link) override;
	String ToString() const override;
};

class CheckOrderMessage : public CoinMessage {
	typedef CoinMessage base;
public:
	CheckOrderMessage()
		:	base("checkorder")
	{}
};

class SubmitOrderMessage : public CoinMessage {
	typedef CoinMessage base;
public:
	HashValue TxHash;

	SubmitOrderMessage()
		:	base("submitorder")
	{}

	void Write(BinaryWriter& wr) const override {
		wr << TxHash;
	}

	void Read(const BinaryReader& rd) override {
		rd >> TxHash;
	}
};

class ReplyMessage : public CoinMessage {
	typedef CoinMessage base;
public:
	UInt32 Code;

	ReplyMessage()
		:	base("reply")
	{}

	void Write(BinaryWriter& wr) const override {
		wr << Code;
	}

	void Read(const BinaryReader& rd) override {
		Code = rd.ReadUInt32();
	}
};

class PingPongMessage : public CoinMessage {
	typedef CoinMessage base;
public:
	UInt64 Nonce;

	PingPongMessage(RCString command)
		:	base(command)
	{}

	void Write(BinaryWriter& wr) const override {
		wr << Nonce;
	}

	void Read(const BinaryReader& rd) override {
		rd >> Nonce;
	}
};

class PingMessage : public PingPongMessage {
	typedef PingPongMessage base;
public:
	PingMessage()
		:	base("ping")
	{
		Ext::Random().NextBytes(Buf(&Nonce, sizeof Nonce));
	}

	void Read(const BinaryReader& rd) override {
		if (Link->PeerVersion >= 60001 && !rd.BaseStream.Eof())
			base::Read(rd);
	}

	void Process(P2P::Link& link) override;
};

class PongMessage : public PingPongMessage {
	typedef PingPongMessage base;
public:
	PongMessage()
		:	base("pong")
	{
		Nonce = 0;
	}
};

class MemPoolMessage : public CoinMessage {
	typedef CoinMessage base;
public:
	MemPoolMessage()
		:	base("mempool")
	{}

	void Process(P2P::Link& link) override;
};

class AlertMessage : public CoinMessage {
	typedef CoinMessage base;
public:
	Blob Payload;
	Blob Signature;

	ptr<Coin::Alert> Alert;

	AlertMessage()
		:	base("alert")
	{}

	void Write(BinaryWriter& wr) const override {
		WriteBlob(wr, Payload);
		WriteBlob(wr, Signature);
	}

	void Read(const BinaryReader& rd) override {
		Payload = ReadBlob(rd);
		Signature = ReadBlob(rd);
	}
protected:
	void Process(P2P::Link& link) override;
};

class MerkleBlockMessage : public CoinMessage {
	typedef CoinMessage base;
public:
	HashValue PrevBlockHash;
	HashValue MerkleRoot;
	DateTime Timestamp;
	UInt32 DifficultyTargetBits;
	UInt32 Ver;
	UInt32 Height;
	UInt32 Nonce;
	UInt32 NTransactions;
	CoinPartialMerkleTree PartialMT;

	MerkleBlockMessage()
		:	base("merkleblock")
	{}

	vector<Tx> Init(const Block& block, CoinFilter& filter);
	void Write(BinaryWriter& wr) const override;
	void Read(const BinaryReader& rd) override;
protected:
	void Process(P2P::Link& link) override;
};

class FilterLoadMessage : public CoinMessage {
	typedef CoinMessage base;
public:
	ptr<CoinFilter> Filter;

	FilterLoadMessage(CoinFilter *filter = 0)
		:	base("filterload")
		,	Filter(filter)
	{}

	void Write(BinaryWriter& wr) const override {
		wr << *Filter;
	}

	void Read(const BinaryReader& rd) override {
		rd >> *(Filter = new CoinFilter);
		if ((Filter->Bitset.size()+7)/8 > MAX_BLOOM_FILTER_SIZE || Filter->HashNum > MAX_HASH_FUNCS)
			throw PeerMisbehavingExc(100);
	}
protected:
	void Process(P2P::Link& link) override {
		CoinLink& clink = (CoinLink&)link;
		EXT_LOCK (clink.MtxFilter) {
			clink.Filter = Filter;
		}
		clink.RelayTxes = true;
	}
};

class FilterAddMessage : public CoinMessage {
	typedef CoinMessage base;
public:
	Blob Data;

	FilterAddMessage()
		:	base("filteradd")
	{}

	void Write(BinaryWriter& wr) const override {
		CoinSerialized::WriteBlob(wr, Data);
	}

	void Read(const BinaryReader& rd) override {
		Data = CoinSerialized::ReadBlob(rd);
		if (Data.Size > MAX_SCRIPT_ELEMENT_SIZE)
			throw PeerMisbehavingExc(100);
	}
protected:
	void Process(P2P::Link& link) override {
		CoinLink& clink = (CoinLink&)link;
		EXT_LOCK (clink.MtxFilter) {
			if (!clink.Filter)
				throw PeerMisbehavingExc(100);
			clink.Filter->Insert(Data);
		}
	}
};

class FilterClearMessage : public CoinMessage {
	typedef CoinMessage base;
public:
	FilterClearMessage()
		:	base("filterclear")
	{}
	
protected:
	void Process(P2P::Link& link) override {
		CoinLink& clink = (CoinLink&)link;
		EXT_LOCK (clink.MtxFilter) {
			clink.Filter = nullptr;
		}
		clink.RelayTxes = true;
	}
};



} // Coin::
