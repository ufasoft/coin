/*######   Copyright (c) 2019      Ufasoft  http://ufasoft.com  mailto:support@ufasoft.com,  Sergey Pavlov  mailto:dev@ufasoft.com ####
#                                                                                                                                     #
# 		See LICENSE for licensing information                                                                                         #
#####################################################################################################################################*/

#include <el/ext.h>

#include <el/crypto/ecdsa.h>
using namespace Ext::Crypto;


#include "coin-protocol.h"

namespace Coin {

bool Link::HasHeader(const BlockHeader& header) {
	CoinEng& eng = Eng();
	HashValue hashBlock = Hash(header);

	return HashBlockBestKnown && hashBlock == Hash(eng.Tree.GetAncestor(HashBlockBestKnown, header.Height))
		|| HashBestHeaderSent && hashBlock == Hash(eng.Tree.GetAncestor(HashBestHeaderSent, header.Height));
}

void CoinDb::OnBestBlock(const Block& block) {
	CoinEng& eng = Eng();
	vector<ptr<Link>> sendTo;

	EXT_LOCK(eng.MtxPeers) {
		for (auto& plink : eng.Links) {
			Link& link = static_cast<Link&>(*plink);
			if (link.PeerVersion >= ProtocolVersion::INVALID_CB_NO_BAN_VERSION
				&& link.PreferHeaderAndIDs
				&& !link.HasHeader(block) && link.HasHeader(block.GetPrevHeader()))
			{
				sendTo.push_back(&link);
			}
		}
	}

	if (sendTo.empty())
		return;
	ptr<CompactBlockMessage> m = new CompactBlockMessage(block, true);
	for (auto& plink : sendTo) {
		plink->Send(m);
	}
}


} // Coin::
