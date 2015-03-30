/*######   Copyright (c) 2013-2015 Ufasoft  http://ufasoft.com  mailto:support@ufasoft.com,  Sergey Pavlov  mailto:dev@ufasoft.com ####
#                                                                                                                                     #
# 		See LICENSE for licensing information                                                                                         #
#####################################################################################################################################*/

#pragma once

#include <el/bignum.h>

#include "util.h"

namespace Coin {

struct MinerTx {
	Blob Data;
	HashValue Hash;
};


class MinerBlock : public BlockBase {
	typedef BlockBase base;
public:
	VarValue JobId;
	mutable vector<MinerTx> Txes;
	String WorkId;
	Blob ConBaseAux;
	DateTime MinTime;
	TimeSpan ServerTimeOffset;
	DateTime MyExpireTime;
	Blob CoinbaseTxn;
	int64_t CoinbaseValue;
	Blob ExtraNonce2; //!!!?
	Blob Coinb1, ExtraNonce1, CoinbaseAux, Coinb2;
	CCoinMerkleBranch MerkleBranch;
	HashValue HashTarget;
	uint32_t SizeLimit, SigopLimit;
	pair<uint32_t, uint32_t> NonceRange;
	HashAlgo Algo;
	String LongPollId, LongPollUrl;

	MinerBlock()
		:	WorkId(nullptr)
		,	CoinbaseValue(-1)
		,	SizeLimit(UINT_MAX)
		,	SigopLimit(UINT_MAX)
		,	CoinbaseTxn(nullptr)
		,	NonceRange(0, 0xFFFFFFFF)
		,	MyExpireTime(DateTime::UtcNow() + TimeSpan::FromSeconds(1000))
		,	Algo(HashAlgo::Sha256)
	{}

	static ptr<MinerBlock> FromJson(const VarValue& json) {
		ptr<MinerBlock> r = new MinerBlock;
		r->ReadJson(json);
		return r;
	}

	static ptr<MinerBlock> FromStratumJson(const VarValue& json);
	Coin::HashValue MerkleRoot(bool bSave=true) const override;
	void AssemblyCoinbaseTx(RCString address);

	void SetTimestamps(const DateTime& dt) {
		ServerTimeOffset = (Timestamp = dt) - DateTime::UtcNow();
	}
protected:
	void ReadJson(const VarValue& json);
};

BinaryWriter& operator<<(BinaryWriter& wr, const MinerBlock& minerBlock);




} // Coin::

