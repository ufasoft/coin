/*######   Copyright (c) 2013-2019 Ufasoft  http://ufasoft.com  mailto:support@ufasoft.com,  Sergey Pavlov  mailto:dev@ufasoft.com ####
#                                                                                                                                     #
# 		See LICENSE for licensing information                                                                                         #
#####################################################################################################################################*/

#include <el/ext.h>

#include "block-template.h"


namespace Coin {

    ProtocolWriter& operator<<(ProtocolWriter& wr, const MinerBlock& block) {
	block.WriteHeader(wr);

	CoinSerialized::WriteCompactSize(wr, block.Txes.size());
	EXT_FOR (const MinerTx& mtx, block.Txes) {
		wr.BaseStream.Write(mtx.Data);
	}
	return wr;
}

void MinerBlock::ReadJson(const VarValue& json) {
//	TRC(4, "Bits: " << json["bits"].ToString());

	Ver = (uint32_t)json["version"].ToInt64();
	Height = uint32_t(json["height"].ToInt64());
	PrevBlockHash = HashValue(json["previousblockhash"].ToString());

	DifficultyTargetBits = Convert::ToUInt32(json["bits"].ToString(), 16);
	HashTarget = HashValue::FromDifficultyBits(DifficultyTargetBits);

	VarValue txes = json["transactions"];
	Txes.resize(txes.size() + 1);

	SetTimestamps(DateTime::from_time_t(json["curtime"].ToInt64()));
#ifdef X_DEBUG//!!!D
	TRC(1, "Time_t: " << json["curtime"].ToInt64());
#endif

	if (json.HasKey("mintime"))
		MinTime = DateTime::from_time_t(json["mintime"].ToInt64());

	if (json.HasKey("sizelimit"))
		SizeLimit = uint32_t(json["sizelimit"].ToInt64());
	if (json.HasKey("sigoplimit"))
		SigopLimit = uint32_t(json["sigoplimit"].ToInt64());

	NonceRange = FromOptionalNonceRange(json);

	MyExpireTime = Clock::now() + TimeSpan::FromSeconds(json.HasKey("expires") ? int32_t(json["expires"].ToInt64()) : 600);

	if (VarValue coinbaseAux = json["coinbaseaux"]) {
		MemoryStream ms;
		vector<String> keys = coinbaseAux.Keys();
		EXT_FOR(RCString key, keys) {
			ms.Write(Blob::FromHexString(coinbaseAux[key].ToString()));
		}
		CoinbaseAux = ms.AsSpan();
	}

	/*
	uint8_t arPfx[] = { 1, 0, 0, 0, 1,
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 255, 255, 255, 255,
		9, 0x03, uint8_t(Height), uint8_t(Height >> 8), uint8_t(Height >> 16), 4 };
	Coinb1 = Blob(arPfx, sizeof arPfx);
	ExtraNonce2 = Blob(0, 4);
	*/

	for (int i=0; i<txes.size(); ++i) {
		MinerTx& mtx = Txes[i+1];
		VarValue jtx = txes[i];
		mtx.Data = Blob::FromHexString(jtx["data"].ToString());
		if (VarValue vHash = jtx["hash"]) {
			mtx.Hash = HashValue(vHash.ToString());
#ifdef _DEBUG
			if (Hash(mtx.Data) != mtx.Hash)
				Throw(E_FAIL);
#endif
		} else
			mtx.Hash = Hash(mtx.Data);
	}

	struct TxHasher {
		const Coin::MinerBlock *minerBlock;

		HashValue operator()(const MinerTx& mtx, int n) const {
			return &mtx == &minerBlock->Txes[0] ? Hash(mtx.Data) : mtx.Hash;
		}
	} txHasher =  { this };
	MerkleBranch = BuildMerkleTree<Coin::HashValue>(Txes, txHasher, &HashValue::Combine).GetBranch(0);

	if (json.HasKey("workid"))
		WorkId = json["workid"].ToString();

	if (VarValue vv = json["coinbasevalue"])
		CoinbaseValue = vv.ToInt64();

	if (VarValue jtx = json["coinbasetxn"]) {
		Blob blob = Blob::FromHexString(jtx["data"].ToString());
		CoinbaseTxn = blob;
		if (blob.size() <= 36 + 4 + 1)
			Throw(E_FAIL);
		uint8_t& lenScript = *(blob.data()+36+4+1);
		lenScript += 4;
		size_t cbFirst = 36+4+1+1+lenScript-4;

		Coinb1 = Blob(blob.data(), cbFirst);
//		Extranonce1 = Blob(0, 0);
		Coinb2 = Blob(blob.data() + cbFirst, blob.size() - cbFirst);
	}

	if (json.HasKey("longpollid")) {
		LongPollId = json["longpollid"].ToString();
		LongPollUrl = nullptr;
		if (json.HasKey("longpolluri"))
			LongPollUrl = json["longpolluri"].ToString();
		else if (json.HasKey("longpoll"))
			LongPollUrl = json["longpoll"].ToString();
	}

}

HashValue MinerBlock::MerkleRoot(bool bSave) const {
	if (!m_merkleRoot) {
		if (Txes.empty())
			Txes.resize(1);
		Txes[0].Data = Coinb1 + ExtraNonce1 + ExtraNonce2 + Coinb2;
		SHA256 sha;
		hashval hv = sha.ComputeHash(Txes[0].Data);
		switch (Algo) {
		case HashAlgo::Sha3:
		case HashAlgo::Groestl:
			break;
		default:
			hv = sha.ComputeHash(hv);
		}
		m_merkleRoot = MerkleBranch.Apply(HashValue(hv));
#ifdef X_DEBUG//!!!D
		cout << "\nTx0:" << Txes[0].Data << "\n";
		cout << "\nTx0 Hash:" << Hash(Txes[0].Data) << "\n";
		cout << "\nBranch:\n";
		for (int i=0; i<MerkleBranch.Vec.size(); ++i)
			cout << "  " << MerkleBranch.Vec[i] << endl;
#endif

	}
	return m_merkleRoot.value();
}

void MinerBlock::AssemblyCoinbaseTx(RCString address) {
	MemoryStream msScript(64);
	BinaryWriter wrScript(msScript);
	wrScript << BigInteger(Height) << uint8_t(8);
	size_t coinb1Size = (size_t)msScript.Position;
	wrScript << int64_t(0);					// place for ExtraNonce1, ExtraNonce2;
	if (CoinbaseAux.size()) {
		ASSERT(CoinbaseAux.size() < 75);	//!!!TODO

		uint8_t firstByte = CoinbaseAux.constData()[0];
		if (firstByte == 0 || firstByte != CoinbaseAux.size() - 1)
			wrScript << uint8_t(CoinbaseAux.size());
		msScript.Write(CoinbaseAux);
	}
	Blob scriptIn = Blob(msScript.AsSpan());

	MemoryStream ms(128);
	BinaryWriter wr(ms);
	wr << uint32_t(1) << uint8_t(1) << HashValue::Null() << uint32_t(-1);
	CoinSerialized::WriteCompactSize(wr, scriptIn.size());
	coinb1Size += int(ms.Position);
	ms.Write(scriptIn);
	wr << uint32_t(-1) << uint8_t(1) << CoinbaseValue								// seq, NOut
		<< uint8_t(25)																// len(PkScript)
		<< uint8_t(0x76) << uint8_t(0xA9) << uint8_t(20);									// OP_DUP OP_HASH160, hash160Len
	ms.Write(ConstBuf(ConvertFromBase58(address, false).constData()+1, 20));
	wr << uint8_t(0x88) << uint8_t(0xAC);												//OP_EQUALVERIFY OP_CHECKSIG
	wr << uint32_t(0);		// locktime;
	Blob coinbasetxn = ms.AsSpan();
	Coinb1 = Blob(coinbasetxn.constData(), coinb1Size);
	Coinb2 = Blob(coinbasetxn.constData() + coinb1Size + 8, coinbasetxn.size() - coinb1Size - 8);
}




} // Coin::
