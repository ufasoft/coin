/*######   Copyright (c) 2013-2019 Ufasoft  http://ufasoft.com  mailto:support@ufasoft.com,  Sergey Pavlov  mailto:dev@ufasoft.com ####
#                                                                                                                                     #
# 		See LICENSE for licensing information                                                                                         #
#####################################################################################################################################*/

#include <el/ext.h>

#include "miner-share.h"

namespace Coin {

EXT_THREAD_PTR(XptPeer) t_pXptPeer;

XptPeer& Xpt() {
	return *t_pXptPeer;
}

CXptThreadKeeper::CXptThreadKeeper(XptPeer *cur) {
	m_prev = t_pXptPeer;
	t_pXptPeer = cur;
}

CXptThreadKeeper::~CXptThreadKeeper() {
	t_pXptPeer = m_prev;
}

HashAlgo XptAlgoToHashAlgo(XptAlgo algo) {
	switch (algo) {
	case XptAlgo::Sha256: return HashAlgo::Sha256;
	case XptAlgo::SCrypt: return HashAlgo::SCrypt;
	case XptAlgo::Prime: return HashAlgo::Prime;
	case XptAlgo::Momentum: return HashAlgo::Momentum;
	case XptAlgo::Metis: return HashAlgo::Metis;
	case XptAlgo::Sha3: return HashAlgo::Sha3;
	default:
		Throw(errc::invalid_argument);
	}
}

XptAlgo HashAlgoToXptAlgo(HashAlgo algo) {
	switch (algo) {
	case HashAlgo::Sha256: return XptAlgo::Sha256;
	case HashAlgo::SCrypt: return XptAlgo::SCrypt;
	case HashAlgo::Prime: return XptAlgo::Prime;
	case HashAlgo::Momentum: return XptAlgo::Momentum;
	case HashAlgo::Metis: return XptAlgo::Metis;
	case HashAlgo::Sha3: return XptAlgo::Sha3;
	default:
		Throw(errc::invalid_argument);
	}
}

void XptMessage::Write(BinaryWriter& wr) const  {
}

void XptMessage::Read(const BinaryReader& rd) {
}

void XptMessage::WriteString8(BinaryWriter& wr, RCString s) {
	Blob blob = Encoding::UTF8.GetBytes(s);
	uint8_t len = (uint8_t)blob.size();
	(wr << len).BaseStream.WriteBuffer(blob.constData(), len);
}

void XptMessage::WriteString16(BinaryWriter& wr, RCString s) {
	Blob blob = Encoding::UTF8.GetBytes(s);
	uint16_t len = (uint16_t)blob.size();
	(wr << len).BaseStream.WriteBuffer(blob.constData(), len);
}

String XptMessage::ReadString8(const BinaryReader& rd) {
	uint8_t len = rd.ReadByte();
	return Encoding::UTF8.GetChars(rd.ReadBytes(len));
}

String XptMessage::ReadString16(const BinaryReader& rd) {
	uint16_t len = rd.ReadUInt16();
	return Encoding::UTF8.GetChars(rd.ReadBytes(len));
}

Blob XptMessage::ReadBlob16(const BinaryReader& rd) {
	return rd.ReadBytes(rd.ReadUInt16());
}

void XptMessage::WriteBlob16(BinaryWriter& wr, RCSpan cbuf) {
	uint16_t len = (uint16_t)cbuf.size();
	(wr << len).BaseStream.WriteBuffer(cbuf.data(), len);
}

void XptMessage::WriteBigInteger(BinaryWriter& wr, const BigInteger& bi) {
	wr << bi;
}

BigInteger XptMessage::ReadBigInteger(const BinaryReader& rd) {
	BigInteger r;
	rd >> r;
	return r;
}

void AuthXptMessage::Write(BinaryWriter& wr) const {
	wr << ProtocolVersion;
	WriteString8(wr, Username);
	WriteString8(wr, Password);
	if (ProtocolVersion < 6)
		wr << PayloadNum;
	if (ProtocolVersion >= 4)
		WriteString8(wr, ClientVersion);
	if (ProtocolVersion >= 6) {
		wr << uint8_t(DevFees.size());
		for (const auto& pp : DevFees) {
			wr << pp.first;
			wr.BaseStream.WriteBuffer(pp.second.data(), 20);
		}
	}
}

void AuthXptMessage::Read(const BinaryReader& rd) {
	rd >> ProtocolVersion;
	Username = ReadString8(rd);
	Password = ReadString8(rd);
	if (ProtocolVersion < 6)
		rd >> PayloadNum;
	if (ProtocolVersion >= 4)
		ClientVersion = ReadString8(rd);
	if (ProtocolVersion >= 6) {
		DevFees.resize(rd.ReadByte());
		for (size_t i=0; i<DevFees.size(); ++i) {
			rd >> DevFees[i].first;
			rd.BaseStream.ReadBuffer(DevFees[i].second.data(), 20);
		}
	}
}

void AuthAckXptMessage::Write(BinaryWriter& wr) const {
	wr << ErrorCode;
	WriteString16(wr, Reason);
	if (Xpt().ProtocolVersion >= 5)
		wr << (uint8_t)HashAlgoToXptAlgo(Algo);
}

void AuthAckXptMessage::Read(const BinaryReader& rd) {
	rd >> ErrorCode;
	Reason = ReadString16(rd);
	if (Xpt().ProtocolVersion >= 5)
		Xpt().Algo = Algo = XptAlgoToHashAlgo((XptAlgo)rd.ReadByte());
}

void Workdata1XptMessage::ShareBundle::Write(BinaryWriter& wr) const {
	wr << Version << Height << Bits << BitsForShare << (uint32_t)to_time_t(Timestamp) << BundleFlags << PrevBlockHash << FixedPrimorialMultiplier << FixedHashFactor
		<< SieveSizeMin << SieveSizeMax << PrimesToSieveMin << PrimesToSieveMax << SieveChainLength << NonceMin << NonceMax << (uint32_t)PayloadMerkles.size();
	for (size_t i=0; i<PayloadMerkles.size(); ++i)
		wr << PayloadMerkles[i];
}

void Workdata1XptMessage::ShareBundle::Read(const BinaryReader& rd) {
	rd >> Version >> Height >> Bits >> BitsForShare;
	Timestamp = DateTime::from_time_t(rd.ReadUInt32());
	rd >> BundleFlags >> PrevBlockHash >> FixedPrimorialMultiplier >> FixedHashFactor
		>> SieveSizeMin >> SieveSizeMax >> PrimesToSieveMin >> PrimesToSieveMax >> SieveChainLength >> NonceMin >> NonceMax;
	PayloadMerkles.resize(rd.ReadUInt32());
	for (size_t i=0; i<PayloadMerkles.size(); ++i)
		rd >> PayloadMerkles[i];
}

void Workdata1XptMessage::Write(BinaryWriter& wr) const {
	XptPeer& xpt = Xpt();
	if (xpt.ProtocolVersion >= 5 && xpt.Algo != HashAlgo::Prime) {
		wr << MinerBlock->Ver << MinerBlock->Height << MinerBlock->DifficultyTargetBits;
		if (xpt.ProtocolVersion >= 6)
			wr << MinerBlock->HashTarget.ToDifficultyBits() << HashTargetShare.ToDifficultyBits();
		else
			wr << MinerBlock->HashTarget << HashTargetShare;
		wr << (uint32_t)to_time_t(Timestamp) << MinerBlock->PrevBlockHash << MinerBlock->MerkleRoot();
#ifdef X_DEBUG//!!!D
		TRC(1, "time_t: " << (uint32_t)to_time_t(Timestamp) << "   0x" << hex << (uint32_t)to_time_t(Timestamp));
#endif
		WriteBlob16(wr, MinerBlock->Coinb1 + ExtraNonce1);
		WriteBlob16(wr, MinerBlock->Coinb2);
		wr << uint16_t(MinerBlock->Txes.size()-1);
		for (size_t i=1; i<MinerBlock->Txes.size(); ++i)
			wr << MinerBlock->Txes[i].Hash;
	} else if (xpt.ProtocolVersion == 4) {
		wr << EarnedShareValue << (uint32_t)Bundles.size();
		for (size_t i=0; i<Bundles.size(); ++i)
			Bundles[i].Write(wr);
	} else {
		wr << MinerBlock->Ver << MinerBlock->Height << MinerBlock->DifficultyTargetBits << BitsForShare << (uint32_t)to_time_t(Timestamp) << MinerBlock->PrevBlockHash
			<< (uint32_t)Merkles.size();
		for (size_t i=0; i<Merkles.size(); ++i)
			wr << Merkles[i];
	}
}

void Workdata1XptMessage::Read(const BinaryReader& rd) {
	XptPeer& xpt = Xpt();
	if (xpt.ProtocolVersion >= 5 && xpt.Algo != HashAlgo::Prime) {
		MinerBlock = new Coin::MinerBlock;
		MinerBlock->Algo = xpt.Algo;
		rd >> MinerBlock->Ver >> MinerBlock->Height >> MinerBlock->DifficultyTargetBits;
		if (xpt.ProtocolVersion >= 6) {
			MinerBlock->HashTarget = HashValue::FromDifficultyBits(rd.ReadUInt32());
			HashTargetShare = HashValue::FromDifficultyBits(rd.ReadUInt32());
		} else
			rd >> MinerBlock->HashTarget >> HashTargetShare;
		Timestamp = DateTime::from_time_t(rd.ReadUInt32());
		HashValue merkleRoot;
		rd >> MinerBlock->PrevBlockHash >> merkleRoot;
		MinerBlock->m_merkleRoot = merkleRoot;

		MinerBlock->ExtraNonce2 = Blob(0, 4);
		MinerBlock->Coinb1 = ReadBlob16(rd);
		MinerBlock->Coinb2 = ReadBlob16(rd);
		MinerBlock->Txes.resize(rd.ReadUInt16()+1);
		for (size_t i=1; i<MinerBlock->Txes.size(); ++i)
			rd >> MinerBlock->Txes[i].Hash;
	} else if (xpt.ProtocolVersion == 4) {
		rd >> EarnedShareValue;
		Bundles.resize(rd.ReadUInt32());
		for (size_t i=0; i<Bundles.size(); ++i)
			Bundles[i].Read(rd);
	} else {
		MinerBlock = new Coin::MinerBlock;
		MinerBlock->Algo = xpt.Algo;
		rd >> MinerBlock->Ver >> MinerBlock->Height >> MinerBlock->DifficultyTargetBits >> BitsForShare;
		Timestamp = DateTime::from_time_t(rd.ReadUInt32());
		rd >> MinerBlock->PrevBlockHash;
		Merkles.resize(rd.ReadUInt32());
		for (size_t i=0; i<Merkles.size(); ++i)
			rd >> Merkles[i];
	}
}

void SubmitShareXptMessage::Write(BinaryWriter& wr) const {
	XptPeer& xpt = Xpt();

	ASSERT(xpt.Algo == MinerShare->Algo);

	wr << MinerShare->m_merkleRoot.value() << MinerShare->PrevBlockHash << MinerShare->Ver << (uint32_t)to_time_t(MinerShare->Timestamp) << MinerShare->Nonce << MinerShare->DifficultyTargetBits;
	switch (Xpt().Algo) {
	case HashAlgo::Momentum:
		wr << MinerShare->BirthdayA << MinerShare->BirthdayB;
	case HashAlgo::Sha256:
	case HashAlgo::Sha3:
	case HashAlgo::SCrypt:
	case HashAlgo::Metis:
		wr << MinerShare->MerkleRootOriginal << MinerShare->ExtraNonce;
		break;
#if UCFG_COIN_PRIME
	case HashAlgo::Prime:
		{
			PrimeMinerShare *pms = dynamic_cast<PrimeMinerShare*>(MinerShare.get());
			wr << pms->SieveSize << pms->SieveCandidate;
			WriteBigInteger(wr, pms->FixedMultiplier);
			WriteBigInteger(wr, pms->PrimeChainMultiplier);
		}
		break;
#endif
	}
	if (xpt.ProtocolVersion >= 5)
		wr << Cookie;
}

void SubmitShareXptMessage::Read(const BinaryReader& rd) {
	XptPeer& xpt = Xpt();
	auto algo = xpt.Algo;
#if UCFG_COIN_PRIME
	PrimeMinerShare *pms = 0;
	if (algo == HashAlgo::Prime)
		MinerShare = pms = new PrimeMinerShare;
	else
#endif
		MinerShare = new Coin::MinerShare;
	MinerShare->Algo = algo;
	HashValue merkleRoot;
	rd >> merkleRoot;
	MinerShare->m_merkleRoot = merkleRoot;
	rd >> MinerShare->PrevBlockHash >> MinerShare->Ver;
	uint32_t timeT = rd.ReadUInt32();
#ifdef X_DEBUG//!!!D
		TRC(1, timeT);
#endif
	MinerShare->Timestamp = DateTime::from_time_t(timeT);
	rd >> MinerShare->Nonce >> MinerShare->DifficultyTargetBits;
	switch (algo) {
	case HashAlgo::Momentum:
		rd >> MinerShare->BirthdayA >> MinerShare->BirthdayB;
	case HashAlgo::Sha256:
	case HashAlgo::Sha3:
	case HashAlgo::SCrypt:
	case HashAlgo::Metis:
		rd >> MinerShare->MerkleRootOriginal;
		MinerShare->ExtraNonce.resize(rd.ReadByte());
		if (MinerShare->ExtraNonce.size() > 16)
			Throw(ExtErr::Protocol_Violation);
		MinerShare->ExtraNonce = rd.ReadBytes(MinerShare->ExtraNonce.size());
		break;
#if UCFG_COIN_PRIME
	case HashAlgo::Prime:
		rd >> pms->SieveSize >> pms->SieveCandidate;
		pms->FixedMultiplier = ReadBigInteger(rd);
		pms->PrimeChainMultiplier = ReadBigInteger(rd);
		break;
#endif
	}
	if (xpt.ProtocolVersion >= 5)
		rd >> Cookie;
}

ptr<XptMessage> CreateSubmitShareXptMessage() {
	return new SubmitShareXptMessage;
}

void ShareAckXptMessage::Write(BinaryWriter& wr) const {
	wr << ErrorCode;
	WriteString16(wr, Reason);
	wr << ShareValue;
}

void ShareAckXptMessage::Read(const BinaryReader& rd) {
	rd >> ErrorCode;
	Reason = ReadString16(rd);
	rd >> ShareValue;
}

void NonceRange::Write(BinaryWriter& wr) const {
	wr << ChainLength << Multiplier << Nonce << Depth << uint8_t(ChainType);
}

void NonceRange::Read(const BinaryReader& rd) {
	rd >> ChainLength >> Multiplier >> Nonce >> Depth;
	ChainType = (PrimeChainType)rd.ReadByte();
}

void Pow::Write(BinaryWriter& wr) const {
	wr << (uint16_t)(Ranges.size()-1) << PrevBlockHash << MerkleRoot << (uint32_t)to_time_t(DtStart) << SieveSize << PrimesToSieve << Ranges.back().NonceEnd;
	for (size_t i=0; i<Ranges.size(); ++i)
		Ranges[i].Write(wr);
}

void Pow::Read(const BinaryReader& rd) {
	Ranges.resize(rd.ReadUInt16()+1);
	rd >> PrevBlockHash >> MerkleRoot;
	DtStart = DateTime::from_time_t(rd.ReadUInt32());
	rd >> SieveSize >> PrimesToSieve >> Ranges.back().NonceEnd;
	for (size_t i=0; i<Ranges.size(); ++i)
		Ranges[i].Read(rd);
}

void SubmitPowXptMessage::Write(BinaryWriter& wr) const {
	wr << (uint16_t)Pows.size();
	for (size_t i=0; i<Pows.size(); ++i)
		Pows[i].Write(wr);
}

void SubmitPowXptMessage::Read(const BinaryReader& rd) {
	Pows.resize(rd.ReadUInt16()+1);
	for (size_t i=0; i<Pows.size(); ++i)
		Pows[i].Read(rd);
}

void MessageXptMessage::Write(BinaryWriter& wr) const {
	wr << Type;
	WriteString16(wr, Message);
}

void MessageXptMessage::Read(const BinaryReader& rd) {
	rd >> Type;
	Message = ReadString16(rd);
}

void PingXptMessage::Write(BinaryWriter& wr) const {
	if (Xpt().ProtocolVersion < 6)
		wr << Version;
	wr << TimestampMs;
}

void PingXptMessage::Read(const BinaryReader& rd) {
	if (Xpt().ProtocolVersion < 6)
		rd >> Version;
	rd >> TimestampMs;
}


XptPeer::XptPeer(thread_group *tr)
	:	base(nullptr, tr)
	,	W(Tcp.Stream)
	,	R(Tcp.Stream)
	,	Algo(HashAlgo::Prime)
{
}


void XptPeer::SendBuf(RCSpan cbuf) {
	EXT_LOCK (MtxSend) {
		W.BaseStream.Write(cbuf);
	}
}

void XptPeer::Send(ptr<P2P::Message> m) {
	CXptThreadKeeper xptKeeper(this);

	MemoryStream ms;
	BinaryWriter(ms).Ref() << (uint32_t)static_cast<XptMessage*>(m.get())->Opcode << *m;
	Span blob = ms.AsSpan();
	*(uint32_t*)blob.data() |= htole(uint32_t(blob.size() - 4) << 8);
	SendBuf(blob);
}



} // Coin::

