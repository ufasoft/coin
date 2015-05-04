/*######   Copyright (c) 2013-2015 Ufasoft  http://ufasoft.com  mailto:support@ufasoft.com,  Sergey Pavlov  mailto:dev@ufasoft.com ####
#                                                                                                                                     #
# 		See LICENSE for licensing information                                                                                         #
#####################################################################################################################################*/

#pragma once

#include "xpt.h"

namespace Coin {

class MinerShare : public BlockBase {
	typedef BlockBase base;
public:
	HashAlgo Algo;
	HashValue MerkleRootOriginal;
	uint32_t BirthdayA, BirthdayB;
	Blob ExtraNonce;

	void WriteHeader(BinaryWriter& wr) const override;
	HashValue GetHash() const override;
	Coin::HashValue MerkleRoot(bool bSave) const override { return m_merkleRoot.get(); }
	virtual uint32_t GetDifficulty() const { Throw(E_NOTIMPL); }
	virtual Coin::HashValue GetHashPow() const;
};

#if UCFG_COIN_PRIME
class PrimeMinerShare : public MinerShare {
	typedef MinerShare base;
public:
	PrimeMinerShare() {
		Algo = HashAlgo::Prime;
	}

	BigInteger FixedMultiplier, PrimeChainMultiplier;
	uint32_t SieveSize, SieveCandidate;

protected:
	void WriteHeader(BinaryWriter& wr) const override;
	uint32_t GetDifficulty() const override;
};
#endif // UCFG_COIN_PRIME

class SubmitShareXptMessage : public XptMessage {
	typedef XptMessage base;
public:
	ptr<Coin::MinerShare> MinerShare;
	uint32_t Cookie;

	SubmitShareXptMessage()
		: Cookie(0)
	{
		Opcode = XPT_OPC_C_SUBMIT_SHARE;
	}

	void Write(BinaryWriter& wr) const override;
	void Read(const BinaryReader& rd) override;
	void Process(P2P::Link& link)  override;
};



} // Coin::

