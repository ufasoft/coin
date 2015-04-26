/*######   Copyright (c) 2013-2015 Ufasoft  http://ufasoft.com  mailto:support@ufasoft.com,  Sergey Pavlov  mailto:dev@ufasoft.com ####
#                                                                                                                                     #
# 		See LICENSE for licensing information                                                                                         #
#####################################################################################################################################*/

#pragma once

#include <el/bignum.h>

#include <el/inet/p2p-net.h>

#include "../util/prime-util.h"
#include "miner.h"

namespace Coin {

const byte XPT_OPC_C_AUTH_REQ 	= 1,
	XPT_OPC_S_AUTH_ACK			= 2,
	XPT_OPC_S_WORKDATA1			= 3,
	XPT_OPC_C_SUBMIT_SHARE		= 4,
	XPT_OPC_S_SHARE_ACK			= 5,
	XPT_OPC_C_SUBMIT_POW		= 6,
	XPT_OPC_S_MESSAGE			= 7,
	XPT_OPC_S_PING				= 8;

const uint32_t XPT_ERROR_NONE = 0,
			XPT_ERROR_INVALID_LOGIN = 1,
			XPT_ERROR_INVALID_WORKLOAD = 2,
			XPT_ERROR_INVALID_COINTYPE = 3;

enum class XptAlgo {
	Sha256 = 1,
	SCrypt = 2,
	Prime = 3,
	Momentum = 4,
	Metis = 5,
	Sha3 = 6,
};

class XptPeer;
class XptClient;

XptPeer& Xpt();

class CXptThreadKeeper {
	XptPeer *m_prev;
public:
	CXptThreadKeeper(XptPeer *cur);
	~CXptThreadKeeper();
};

class XptMessage : public P2P::Message {
public:
	byte Opcode;
    
	void Write(BinaryWriter& wr) const override;
	void Read(const BinaryReader& rd) override;

	static void WriteString8(BinaryWriter& wr, RCString s);
	static void WriteString16(BinaryWriter& wr, RCString s);
	static String ReadString8(const BinaryReader& rd);
	static String ReadString16(const BinaryReader& rd);
	static Blob ReadBlob16(const BinaryReader& rd);
	static void WriteBlob16(BinaryWriter& wr, const ConstBuf& cbuf);

	static void WriteBigInteger(BinaryWriter& wr, const BigInteger& bi);
	static BigInteger ReadBigInteger(const BinaryReader& rd);
};

class AuthXptMessage : public XptMessage {
	typedef XptMessage base;
public:
	uint32_t ProtocolVersion;
	String Username, Password, ClientVersion;
	uint32_t PayloadNum;
	
	vector<pair<uint16_t, HashValue160>> DevFees;

	AuthXptMessage()
		:	ProtocolVersion(6)
		,	PayloadNum(1)
	{
		Opcode = XPT_OPC_C_AUTH_REQ;
	}

	void Write(BinaryWriter& wr) const override;
	void Read(const BinaryReader& rd) override;
	void Process(P2P::Link& link)  override;
};

class AuthAckXptMessage : public XptMessage {
	typedef XptMessage base;
public:
	uint32_t ErrorCode;
	String Reason;
	HashAlgo Algo;

	AuthAckXptMessage()
		:	ErrorCode(XPT_ERROR_NONE)
		,	Algo(HashAlgo::Prime)
	{
		Opcode = XPT_OPC_S_AUTH_ACK;
	}

	void Write(BinaryWriter& wr) const override;
	void Read(const BinaryReader& rd) override;
};

class Workdata1XptMessage : public XptMessage {
	typedef XptMessage base;
public:
	struct ShareBundle {
 		vector<HashValue> PayloadMerkles;
		uint32_t Version, Height, Bits, BitsForShare;
		DateTime Timestamp;
		HashValue PrevBlockHash;
		uint32_t FixedPrimorialMultiplier, FixedHashFactor;
		uint32_t SieveSizeMin, SieveSizeMax;
		uint32_t PrimesToSieveMin, PrimesToSieveMax;
		uint32_t NonceMin, NonceMax;		
		byte BundleFlags;
		byte SieveChainLength;

		ShareBundle()
			:	BundleFlags(2)						// Interruptible
			,	FixedPrimorialMultiplier(0)
			,	FixedHashFactor(0)
			,	SieveSizeMin(0)
			,	SieveSizeMax(0)
			,	SieveChainLength(0)
			,	NonceMin(0)
			,	NonceMax(0xFFFFFFFF)
		{}

		void Write(BinaryWriter& wr) const;
		void Read(const BinaryReader& rd);
	};

	ptr<Coin::MinerBlock> MinerBlock;
	vector<HashValue> Merkles;
	DateTime Timestamp;							// hash priority over MinerBlock->Timestamp
	Blob ExtraNonce1;
	HashValue HashTargetShare;
	uint32_t BitsForShare;				// Primecoin
	vector<ShareBundle> Bundles;
	float EarnedShareValue;

	Workdata1XptMessage()
		:	EarnedShareValue(0)
	{
		Opcode = XPT_OPC_S_WORKDATA1;
	}

	void Write(BinaryWriter& wr) const override;
	void Read(const BinaryReader& rd) override;
	void Process(P2P::Link& link) override;
};

class XptWorkData : public BitcoinWorkData {
public:
	XptWorkData() {
		HashAlgo = Coin::HashAlgo::Prime;
	}

	observer_ptr<XptClient> Client;
	uint32_t BitsForShare, SieveSize, SieveCandidate;
	uint32_t FixedPrimorialMultiplier, FixedHashFactor;
	uint32_t SieveSizeMin, SieveSizeMax;
	uint32_t PrimesToSieveMin, PrimesToSieveMax;
	uint32_t SieveChainLength;
	bool TimestampRoll, Interruptible;
	BigInteger FixedMultiplier, ChainMultiplier;

	XptWorkData *Clone() const override { return new XptWorkData(*this); }

//	HashValue Hash() const;
};

ptr<XptMessage> CreateSubmitShareXptMessage();
	
class ShareAckXptMessage : public XptMessage {
	typedef XptMessage base;
public:
	uint32_t ErrorCode;
	String Reason;
	float ShareValue;

	ShareAckXptMessage()
		:	ErrorCode(0)
	{
		Opcode = XPT_OPC_S_SHARE_ACK;
	}

	void Write(BinaryWriter& wr) const override;
	void Read(const BinaryReader& rd) override;	
	void Process(P2P::Link& link) override;
};   

struct NonceRange {
	uint32_t ChainLength;
	uint32_t Multiplier;
	uint32_t Nonce, NonceEnd;
	byte Depth;
	PrimeChainType ChainType;

	NonceRange() {
		ZeroStruct(_self);
	}

	void Write(BinaryWriter& wr) const;
	void Read(const BinaryReader& rd);
};

struct Pow {
	HashValue PrevBlockHash,
			MerkleRoot;
	DateTime DtStart;
	uint32_t SieveSize, PrimesToSieve;
	vector<NonceRange> Ranges;

	void Write(BinaryWriter& wr) const;
	void Read(const BinaryReader& rd);
};

class SubmitPowXptMessage : public XptMessage {
	typedef XptMessage base;
public:
	vector<Pow> Pows;

	SubmitPowXptMessage() {
		Opcode = XPT_OPC_C_SUBMIT_POW;
	}

	void Write(BinaryWriter& wr) const override;
	void Read(const BinaryReader& rd) override;	
};

class MessageXptMessage : public XptMessage {
	typedef XptMessage base;
public:
	byte Type;
	String Message;

	MessageXptMessage()
		:	Type(0)
	{
		Opcode = XPT_OPC_S_MESSAGE;
	}

	void Write(BinaryWriter& wr) const override;
	void Read(const BinaryReader& rd) override;	
	void Process(P2P::Link& link) override;
};

class PingXptMessage : public XptMessage {
	typedef XptMessage base;
public:
	uint32_t Version;
	uint64_t TimestampMs;

	PingXptMessage(uint64_t timestampMs = 0)
		:	Version(0)
		,	TimestampMs(timestampMs)
	{
		Opcode = XPT_OPC_S_PING;
	}

	void Write(BinaryWriter& wr) const override;
	void Read(const BinaryReader& rd) override;	
	void Process(P2P::Link& link) override;
};

class XptPeer : public P2P::Link {
	typedef P2P::Link base;
public:
	uint32_t ProtocolVersion;
	HashAlgo Algo;
	BinaryWriter W;
	BinaryReader R;

	XptPeer(thread_group *tr = 0);
	void Send(ptr<P2P::Message> m) override;
protected:
	mutex MtxSend;

	virtual void SendBuf(const ConstBuf& cbuf);
};

class XptClient : public SocketThreadWrap<XptPeer>, public ConnectionClient {
	typedef SocketThreadWrap<XptPeer> base;
public:
	IPEndPoint EpServer;

	mutex MtxData;
	//----------------------------------------------------------------
	queue<ptr<BitcoinWorkData>> JobQueue;
	ptr<BitcoinWorkData> LastShare;

	typedef unordered_map<pair<HashValue, HashValue>, Pow> CPowMap;
	CPowMap m_powMap;
	//----------------------------------------------------------------

	array<atomic<int>, 20> aLenStats;
	CBool ConnectionEstablished;	

	XptClient(Coin::BitcoinMiner& miner);
	ptr<XptMessage> Recv();
	void PrintStats();
	void Submit(BitcoinWorkData *wd) override;
protected:
	DateTime DtNextSendPow;

	void SetEpServer(const IPEndPoint& epServer) override { EpServer = epServer; }
	void Execute() override;
	void OnPeriodic() override;
	ptr<BitcoinWorkData> GetWork() override;

	bool HasWorkData() override {
		return EXT_LOCKED(MtxData, !JobQueue.empty()) || ConnectionClient::HasWorkData();
	}
};

ptr<BitcoinWorkData> GetPrimeTestData();

} // Coin::

