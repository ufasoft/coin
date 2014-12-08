/*######     Copyright (c) 1997-2013 Ufasoft  http://ufasoft.com  mailto:support@ufasoft.com,  Sergey Pavlov  mailto:dev@ufasoft.com #######################################
#                                                                                                                                                                          #
# This program is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation;  #
# either version 3, or (at your option) any later version. This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the      #
# implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details. You should have received a copy of the GNU #
# General Public License along with this program; If not, see <http://www.gnu.org/licenses/>                                                                               #
##########################################################################################################################################################################*/

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
	Int64 CoinbaseValue;
	size_t SizeExtraNonce2;
	Blob ExtraNonce2; //!!!?
	Blob Coinb1, ExtraNonce1, CoinbaseAux, Coinb2;
	CCoinMerkleBranch MerkleBranch;
	HashValue HashTarget;
	UInt32 SizeLimit, SigopLimit;
	pair<UInt32, UInt32> NonceRange;
	HashAlgo Algo;

	MinerBlock()
		:	WorkId(nullptr)
		,	CoinbaseValue(-1)
		,	SizeLimit(UINT_MAX)
		,	SigopLimit(UINT_MAX)
		,	CoinbaseTxn(nullptr)
		,	SizeExtraNonce2(4)
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


class MinerShare : public BlockBase {
	typedef BlockBase base;
public:
	HashAlgo Algo;
	HashValue MerkleRootOriginal;
	UInt32 BirthdayA, BirthdayB;
	Blob ExtraNonce;
	
	void WriteHeader(BinaryWriter& wr) const override;
	HashValue GetHash() const override;
	Coin::HashValue MerkleRoot(bool bSave) const override { return m_merkleRoot.get(); }
	virtual UInt32 GetDifficulty() const { Throw(E_NOTIMPL); }
	virtual Coin::HashValue GetHashPow() const;
protected:
};

class PrimeMinerShare : public MinerShare {
	typedef MinerShare base;
public:
	PrimeMinerShare() {
		Algo = HashAlgo::Prime;
	}

	BigInteger FixedMultiplier, PrimeChainMultiplier;
	UInt32 SieveSize, SieveCandidate;

protected:
	void WriteHeader(BinaryWriter& wr) const override;
	UInt32 GetDifficulty() const override;
};



} // Coin::

