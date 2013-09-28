/*######     Copyright (c) 1997-2013 Ufasoft  http://ufasoft.com  mailto:support@ufasoft.com,  Sergey Pavlov  mailto:dev@ufasoft.com #######################################
#                                                                                                                                                                          #
# This program is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation;  #
# either version 3, or (at your option) any later version. This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the      #
# implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details. You should have received a copy of the GNU #
# General Public License along with this program; If not, see <http://www.gnu.org/licenses/>                                                                               #
##########################################################################################################################################################################*/

#include <el/ext.h>

#include <el/bignum.h>

#include <el/crypto/hash.h>
#include <el/crypto/ecdsa.h>
using namespace Ext::Crypto;

#include "coin-model.h"
#include "eng.h"
#include "coin-msg.h"
#include "coin-protocol.h"
#include "script.h"


namespace Coin {

void PeerInfoBase::Write(BinaryWriter& wr) const {
	wr << Services;
	Blob blob = Ep.Address.GetAddressBytes();
	if (blob.Size == 4) {
		const byte buf[12] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0xFF, 0xFF };
		wr.Write(buf, sizeof buf);
	}
	wr.Write(blob.constData(), blob.Size);
	wr << UInt16(_byteswap_ushort(Ep.Port));
}

void PeerInfoBase::Read(const BinaryReader& rd) {
	Services = rd.ReadUInt64();
	byte buf[16];
	rd.Read(buf, sizeof buf);
	UInt16 port = _byteswap_ushort(rd.ReadUInt16());
	Ep = IPEndPoint(IPAddress(!memcmp(buf, "\0\0\0\0\0\0\0\0\0\0\xFF\xFF", 12) ? ConstBuf(buf+12, 4) : ConstBuf(buf, 16)), port);
}


void PeerInfo::Write(BinaryWriter& wr) const {
	wr << UInt32(Timestamp.UnixEpoch);
	base::Write(wr);
}

void PeerInfo::Read(const BinaryReader& rd) {
	Timestamp = DateTime::FromUnix(rd.ReadUInt32());
	base::Read(rd);
}

HashValue Hash(const CPersistent& pers) {
	return Hash(EXT_BIN(pers));
}

HashValue Hash(const Tx& tx) {
#ifdef X_DEBUG//!!!D
	if (tx.m_pimpl->m_hash != HashValue() && tx.m_pimpl->m_hash != Hash(EXT_BIN(tx))) {
		HashValue hv = Hash(EXT_BIN(tx));
		MemoryStream ms;
		BinaryWriter wr(ms);
		wr << tx;
		hv = hv;
	}
	ASSERT(tx.m_pimpl->m_nBytesOfHash != 32 || tx.m_pimpl->m_hash == Hash(EXT_BIN(tx)));
#endif
	if (tx.m_pimpl->m_nBytesOfHash != 32)
		tx.SetHash(Hash(EXT_BIN(tx)));
	return tx.m_pimpl->m_hash;
}

void MerkleTx::Write(BinaryWriter& wr) const {
	base::Write(wr);
	wr << BlockHash << MerkleBranch;
}

void MerkleTx::Read(const BinaryReader& rd) {
	base::Read(rd);
	rd >> BlockHash >> MerkleBranch;
}

void AuxPow::Write(BinaryWriter& wr) const {
	base::Write(wr);
	wr << ChainMerkleBranch;
	ParentBlock.WriteHeader(wr);
}

void AuxPow::Read(const BinaryReader& rd) {
	base::Read(rd);
	rd >> ChainMerkleBranch;
	ParentBlock.ReadHeader(rd, true);
}

int CalcMerkleIndex(int merkleSize, int chainId, UInt32 nonce) {
    return (((nonce * 1103515245 + 12345) + chainId) * 1103515245 + 12345) % merkleSize;
}

const byte s_mergedMiningHeader[4] = { 0xFA, 0xBE, 'm', 'm' } ;

void AuxPow::Check(const Block& blockAux) {
	if (blockAux.ChainId == ParentBlock.ChainId)
		Throw(E_COIN_AUXPOW_ParentHashOurChainId);
	if (ChainMerkleBranch.Vec.size() > 30)
		Throw(E_COIN_AUXPOW_ChainMerkleBranchTooLong);

	HashValue rootHash = ChainMerkleBranch.Apply(Hash(blockAux));
	std::reverse(rootHash.begin(), rootHash.end());						// correct endian

	if (MerkleBranch.Apply(Hash(Tx(_self))) != ParentBlock.get_MerkleRoot())
		Throw(E_COIN_AUXPOW_MerkleRootIncorrect);

	Blob script = TxIns().at(0).Script();
	const byte *pcHead = Search(ConstBuf(script), ConstBuf(s_mergedMiningHeader, sizeof s_mergedMiningHeader)),
		       *pc = Search(ConstBuf(script), ConstBuf(rootHash));
	if (pc == script.end())
		Throw(E_COIN_AUXPOW_MissingMerkleRootInParentCoinbase);
	if (pcHead != script.end()) {
		if (script.end() != std::search(pcHead+1, script.end(), begin(s_mergedMiningHeader), end(s_mergedMiningHeader)))
			Throw(E_COIN_AUXPOW_MutipleMergedMiningHeades);
		if (pc != pcHead + sizeof(s_mergedMiningHeader))
			Throw(E_COIN_AUXPOW_MergedMinignHeaderiNotJustBeforeMerkleRoot);		
	} else if (pc-script.begin() > 20)
		Throw(E_COIN_AUXPOW_MerkleRootMustStartInFirstFewBytes);
	CMemReadStream stm(ConstBuf(pc + rootHash.size(), script.end()-pc-rootHash.size()));
	Int32 size, nonce;
	BinaryReader(stm) >> size >> nonce;
	if (size != (1 << ChainMerkleBranch.Vec.size()))
		Throw(E_COIN_AUXPOW_MerkleBranchSizeIncorrect);
	if (ChainMerkleBranch.Index != CalcMerkleIndex(size, blockAux.ChainId, nonce))
		Throw(E_COIN_AUXPOW_WrongIndex);
}

Address::Address() {
	Ver = Eng().ChainParams.AddressVersion;
	memset(data(), 0, size());
}

Address::Address(const HashValue160& hash, RCString comment)
	:	Comment(comment)
{
	Ver = Eng().ChainParams.AddressVersion;
	HashValue160::operator=(hash);
}

Address::Address(const HashValue160& hash, byte ver)
	:	Ver(ver)
{
	HashValue160::operator=(hash);
}

Address::Address(RCString s, CoinEng *eng) {
	try {
		Blob blob = ConvertFromBase58(s.Trim());
		if (blob.Size < 21)
			Throw(E_COIN_InvalidAddress);
		Ver = blob.constData()[0];
		memcpy(data(), blob.constData()+1, 20);
	} catch (RCExc) {
		Throw(E_COIN_InvalidAddress);
	}
	if (eng)
		CheckVer(*eng);
}

void Address::CheckVer(CoinEng& eng) const {
	if (Ver != eng.ChainParams.AddressVersion)
		Throw(E_COIN_InvalidAddress);
}

String Address::ToString() const {
	vector<byte> v(21);
	v[0] = Ver;
	memcpy(&v[1], data(), 20);
	return ConvertToBase58(ConstBuf(&v[0], v.size()));
}

PrivateKey::PrivateKey(RCString s) {
	try {
		Blob blob = ConvertFromBase58(s.Trim());
		if (blob.Size < 10)
			Throw(E_COIN_InvalidAddress);
		byte ver = blob.constData()[0];
//!!! common ver for all Nets		if (ver != Eng().ChainParams.AddressVersion+128)
		if (!(ver & 128))
			Throw(E_COIN_InvalidAddress);
		m_blob = Blob(blob.constData()+1, blob.Size-1);
	} catch (RCExc) {
		Throw(E_COIN_InvalidAddress);
	}
}

PrivateKey::PrivateKey(const ConstBuf& cbuf, bool bCompressed) {
	m_blob = cbuf;
	if (bCompressed) {
		m_blob.Size = m_blob.Size+1;
		m_blob.data()[m_blob.Size-1] = 1;
	}
}

pair<Blob, bool> PrivateKey::GetPrivdataCompressed() const {
	return pair<Blob, bool>(ConstBuf(m_blob.constData(), 32), m_blob.Size == 33);
}

String PrivateKey::ToString() const {
	byte ver = 128;		//!!!  for all nets, because Private Keys are common // Eng().ChainParams.AddressVersion+128;
	Blob blob = ConstBuf(&ver, 1)+m_blob;
	return ConvertToBase58(blob);
}

ChainCaches::ChainCaches()
	:	m_bestBlock(nullptr)
	,	HashToBlockCache(64)
	,	HeightToHashCache(1024)
	,	HashToTxCache(1024)			// Number of Txes in the block usually >256
	,	m_cachePkIdToPubKey(4096)			// should be more than usual value (Txes per Block)
	,	PubkeyCacheEnabled(true)
{
}

} // Coin::

