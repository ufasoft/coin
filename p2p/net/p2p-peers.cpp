/*######     Copyright (c) 1997-2013 Ufasoft  http://ufasoft.com  mailto:support@ufasoft.com,  Sergey Pavlov  mailto:dev@ufasoft.com #######################################
#                                                                                                                                                                          #
# This program is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation;  #
# either version 3, or (at your option) any later version. This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the      #
# implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details. You should have received a copy of the GNU #
# General Public License along with this program; If not, see <http://www.gnu.org/licenses/>                                                                               #
##########################################################################################################################################################################*/

#include <el/ext.h>

#include "p2p-peers.h"
#include "p2p-net.h"

namespace P2P {

bool Peer::IsTerrible(const DateTime& now) const {
	if (get_LastTry().Ticks && (now-LastTry) < TimeSpan::FromMinutes(1))
		return false;
	return !get_LastPersistent().Ticks
		|| (now-get_LastPersistent()) > TimeSpan::FromDays(30)
		|| !get_LastLive().Ticks && Attempts>=3
		|| (now - LastLive) > TimeSpan::FromDays(7) && Attempts>=10;
}

double Peer::GetChance(const DateTime& now) const {
	TimeSpan sinceLastSeen = std::max(TimeSpan(), now-LastPersistent);
	double r = 1 - sinceLastSeen.TotalSeconds/(600 + sinceLastSeen.TotalSeconds);
	if (now-LastTry < TimeSpan::FromMinutes(10))
		r /= 100;
	return r * pow(0.6, Attempts);
}

Blob Peer::GetGroup() const {
	vector<byte> v;
	const IPAddress& ip = get_EndPoint().Address;
	Blob blob = ip.GetAddressBytes();
	if (4 == blob.Size) {
		v.push_back(4);
		v.push_back(blob.data()[0]);
		v.push_back(blob.data()[1]);
	} else if (ip.IsIPv4MappedToIPv6) {
		v.push_back(4);
		v.push_back(blob.data()[12]);
		v.push_back(blob.data()[13]);
	} else if (ip.IsIPv6Teredo) {
		v.push_back(4);
		v.push_back(blob.data()[12] ^ 0xFF);
		v.push_back(blob.data()[13] ^ 0xFF);
	} else {
		v.push_back(6);
		v.push_back(blob.data()[0]);
		v.push_back(blob.data()[1]);
		v.push_back(blob.data()[2]);
		v.push_back(blob.data()[3]);
	}
	return Blob(&v[0], v.size());
}

size_t Peer::GetHash() const {
	return hash<Blob>()(GetGroup()) + hash<IPEndPoint>()(EndPoint);
}

void PeerManager::Remove(Peer *peer) {
	IpToPeer.erase(peer->get_EndPoint().Address);
	Ext::Remove(VecRandom, peer);
}

void PeerBucket::Shrink() {
	DateTime now = DateTime::UtcNow();
	ptr<Peer> peerOldest;
	for (int i=0; i<m_peers.size(); ++i) {
		ptr<Peer>& peer = m_peers[i];
		if (peer->IsTerrible(now)) {
			Buckets->Manager.Remove(peer.get());
			m_peers.erase(m_peers.begin()+i);
			return;
		}
		if (!peerOldest || peer->LastPersistent < peerOldest->LastPersistent)
			peerOldest = peer;
	}
	Buckets->Manager.Remove(peerOldest);
	Remove(m_peers, peerOldest);
}

size_t PeerBuckets::size() const {
	size_t r = 0;
	for (int i=0; i<m_vec.size(); ++i)
		r += m_vec[i].m_peers.size();
	return r;
}

bool PeerBuckets::Contains(Peer *peer) {
	ptr<Peer> p = peer;
	for (int i=0; i<m_vec.size(); ++i)
		if (ContainsInLinear(m_vec[i].m_peers, p))
			return true;
	return false;
}

ptr<Peer> PeerBuckets::Select() {
	Ext::Random rng;
	DateTime now = DateTime::UtcNow();
	for (double fac=1;;) {
		PeerBucket& bucket = m_vec[rng.Next(m_vec.size())];
		if (!bucket.m_peers.empty()) {
			const ptr<Peer>& p = bucket.m_peers[rng.Next(bucket.m_peers.size())];
			if (p->GetChance(now)*fac > 1)
				return p;
			fac *= 1.2;
		}
	}
}

vector<ptr<Peer>> PeerManager::GetPeers(int nMax) {
	EXT_LOCK (MtxPeers) {
		std::random_shuffle(VecRandom.begin(), VecRandom.end());
		return vector<ptr<Peer>>(VecRandom.begin(), VecRandom.begin()+std::min(size_t(nMax), VecRandom.size()));
	}
}

ptr<Peer> PeerManager::Select() {
	int size = TriedBuckets.size() + NewBuckets.size();
	if (!size)
		return nullptr;
	PeerBuckets& buckets = Ext::Random().Next(size) < TriedBuckets.size() ? TriedBuckets : NewBuckets;
	return buckets.Select();
}

void PeerManager::Attempt(Peer *peer) {
	EXT_LOCK (MtxPeers) {
		peer->LastTry = DateTime::UtcNow();
		peer->Attempts++;
	}
}

void PeerManager::Good(Peer *peer) {
	EXT_LOCK (MtxPeers) {
		DateTime now = DateTime::UtcNow();
		peer->LastTry = now;
		peer->LastLive = now;
		peer->LastPersistent = now;
		peer->Attempts = 0;
		if (!TriedBuckets.Contains(peer)) {
			ptr<Peer> p = peer;
			for (int i=0; i<NewBuckets.m_vec.size(); ++i)
				Ext::Remove(NewBuckets.m_vec[i].m_peers, p);

			PeerBucket& bucket = TriedBuckets.m_vec[peer->GetHash() % TriedBuckets.m_vec.size()];
			if (bucket.m_peers.size() < ADDRMAN_TRIED_BUCKET_SIZE) {
				bucket.m_peers.push_back(peer);
				return;
			}

			ptr<Peer> peerOldest;
			for (int i=0; i<bucket.m_peers.size(); ++i) {
				const ptr<Peer>& peer = bucket.m_peers[i];
				if (!peerOldest || peer->LastPersistent < peerOldest->LastPersistent)
					peerOldest = peer;
			}
			Ext::Remove(bucket.m_peers, peerOldest);
			NewBuckets.m_vec[peerOldest->GetHash() % NewBuckets.m_vec.size()].m_peers.push_back(peerOldest);
		}
	}
}

ptr<Peer> PeerManager::Find(const IPAddress& ip) {
	CPeerMap::iterator it = IpToPeer.find(ip);
	return it!=IpToPeer.end() ? it->second : nullptr;
}


/*
void PeerManager::AddPeer(Peer& peer) {
	TRC(3, peer.EndPoint);

	
	bool bSave = false;
	EXT_LOCK (MtxPeers) {
		auto pp = Peers.insert(CPeerMap::value_type(peer.EndPoint.Address, &peer));
		if (pp.second)
			m_nPeersDirty = true;		
		else {
			Peer& peerFound = *pp.first->second;
			if ((peer.LastLive-peerFound.LastLive).TotalHours > ((DateTime::UtcNow()-peer.LastLive).TotalHours < 24 ? 1 : 24)) {
				m_nPeersDirty = true;
				peerFound.LastLive = peer.LastLive;
			}
		}
	}
}
*/

bool PeerManager::IsRoutable(const IPAddress& ip) {
	return !ip.IsEmpty()
		&& ip.IsGlobal()
		&& !NetManager.IsLocal(ip);
}

ptr<Peer> PeerManager::Add(const IPEndPoint& ep, UInt64 services, DateTime dt, TimeSpan penalty) {
//	TRC(3, ep);

	if (!IsRoutable(ep.Address))
		return nullptr;

	if (NetManager.IsBanned(ep.Address)) {
		TRC(2, "Banned peer ignored " << ep);
		return nullptr;
	}

	m_nPeersDirty = true;
	ptr<Peer> peer;
	EXT_LOCK (MtxPeers) {
		if (peer = Find(ep.Address)) {
			if (dt.Ticks)
				peer->LastPersistent = std::max(DateTime(), dt-penalty);
			peer->put_Services(peer->get_Services() | services);
			if (TriedBuckets.Contains(peer))
				return nullptr;
		} else {
			peer = CreatePeer();
			peer->EndPoint = ep;
			peer->Services = services;
			peer->LastPersistent = std::max(DateTime(), dt-penalty);
			IpToPeer.insert(make_pair(ep.Address, peer));
			VecRandom.push_back(peer.get());
		}
		PeerBucket& bucket = NewBuckets.m_vec[peer->GetHash() % NewBuckets.m_vec.size()];
		if (!ContainsInLinear(bucket.m_peers, peer)) {
			if (bucket.m_peers.size() >= ADDRMAN_NEW_BUCKET_SIZE)
				bucket.Shrink();
			bucket.m_peers.push_back(peer);
		}
	}
	peer->IsDirty = true;
	return peer;
}

void PeerManager::OnPeriodic() {
	EXT_LOCK (MtxPeers) {
		DateTime now = DateTime::UtcNow();
		int nOutgoing = 0;
		for (int i=0; i<Links.size(); ++i) {
			Link& link = *static_cast<Link*>(Links[i].get());
			if (link.m_dtLastRecv==DateTime() && now > link.m_dtCheckLastRecv)
				link.Stop();
			nOutgoing += !link.Incoming;
		}

		if (Links.size() >= MaxLinks)
			return;

		unordered_set<Blob> setConnectedSubnet;
		for (int i=0; i<Links.size(); ++i)
			setConnectedSubnet.insert(Links[i]->Peer->GetGroup());

#ifdef X_DEBUG//!!!D
		MaxOutboundConnections = 1;
#endif

		for (int n = std::max(MaxOutboundConnections-nOutgoing, 0); n--;) {
#ifdef X_DEBUG//!!!D
			ptr<Peer> peer = CreatePeer();
			peer->EndPoint = IPEndPoint::Parse("127.0.0.1:18333");
			peer->Services = 1;
			if (peer) {
#else
			if (ptr<Peer> peer = Select()) {
#endif
				if (setConnectedSubnet.insert(peer->GetGroup()).second) {
					ptr<Link> link = NetManager.CreateLink(*m_owner);
					link->Net = dynamic_cast<Net*>(this);
					link->Peer = peer;
					peer->LastTry = now;
					link->Start();
				} else {
					TRC(3, "Subnet already connected for " << peer->get_EndPoint().Address);
				}
			}
		}
	}

	if (Interlocked::CompareExchange(m_nPeersDirty, 0, 1))
		SavePeers();
}


void PeerManager::AddLink(LinkBase *link) {
	EXT_LOCK (MtxPeers) {
		Links.push_back(link);
	}
}

void PeerManager::OnCloseLink(LinkBase& link) {
	EXT_LOCK (MtxPeers) {
		Ext::Remove(Links, &link);
	}
}


} // P2P::

