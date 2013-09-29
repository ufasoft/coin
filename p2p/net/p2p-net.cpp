/*######     Copyright (c) 1997-2013 Ufasoft  http://ufasoft.com  mailto:support@ufasoft.com,  Sergey Pavlov  mailto:dev@ufasoft.com #######################################
#                                                                                                                                                                          #
# This program is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation;  #
# either version 3, or (at your option) any later version. This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the      #
# implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details. You should have received a copy of the GNU #
# General Public License along with this program; If not, see <http://www.gnu.org/licenses/>                                                                               #
##########################################################################################################################################################################*/

#include <el/ext.h>


#include "p2p-net.h"

namespace P2P {

const int PING_SECONDS = 20*60;
const int INACTIVE_PEER_SECONDS = 90*60;

#if UCFG_P2P_SEND_THREAD

LinkSendThread::LinkSendThread(P2P::Link& link)
	:	Link(link)
	,	base(link.m_owner)
{
}

void LinkSendThread::Execute() {
	Name = "LinkSendThread";

	DateTime dtLast;

	while (!m_bStop) {
		vector<ptr<Message>> messages;
		EXT_LOCK (Link.m_cs) {
			Link.OutQueue.swap(messages);
		}
		DateTime now = DateTime::UtcNow();
		if (messages.empty()) {
			if (now-dtLast > TimeSpan::FromSeconds(PERIODIC_SEND_SECONDS)) {
				Link.OnPeriodic();
				dtLast = now;

				EXT_LOCK (Link.m_cs) {
					if (now-Link.Peer->LastLive > TimeSpan::FromSeconds(INACTIVE_PEER_SECONDS))
						Link.Stop();
				}
			} else {
				if (now-m_dtLastSend > TimeSpan::FromSeconds(PING_SECONDS))
					if (ptr<Message> m = Link.CreatePingMessage())
						Link.Send(m);
				m_ev.Lock(PERIODIC_SEND_SECONDS * 1000);
			}
		} else {
			for (int i=0; i<messages.size(); ++i)
				Link.Net.SendMessage(Link, *messages[i]);
			m_dtLastSend = now;
		}
	}
}

#endif // UCFG_P2P_SEND_THREAD

void Link::SendBinary(const ConstBuf& buf) {
	EXT_LOCK (Mtx) {
		m_dtLastSend = DateTime::UtcNow();
		bool bHasToSend = DataToSend.Size;
		DataToSend += buf;
		if (!bHasToSend) {
			int rc = Tcp.Client.Send(DataToSend.constData(), DataToSend.Size);
			if (rc >= 0)
				DataToSend.Replace(0, rc, ConstBuf(0, 0));
		}
	}
}

void Link::Send(ptr<P2P::Message> msg) {
#if UCFG_P2P_SEND_THREAD
	EXT_LOCK (Mtx) {
		OutQueue.push_back(msg);
		SendThread->m_ev.Set();
	}
#else
	SendBinary(EXT_BIN(*msg));
#endif
}

void Link::OnSelfLink() {
	Stop();

	EXT_LOCK (Net->MtxPeers) {
		this->NetManager.AddLocal(Peer->get_EndPoint().Address);
	}
}

void Link::Execute() {
	Name = "LinkThread";

	IPEndPoint epRemote;
	try {
		if (!Incoming) {
			IPEndPoint ep = Peer->EndPoint;
			Tcp.Client.Create(ep.Address.AddressFamily, SocketType::Stream, ProtocolType::Tcp);
		}

		SocketKeeper sockKeeper(_self, Tcp.Client);

		DBG_LOCAL_IGNORE_WIN32(WSAECONNREFUSED);	
		DBG_LOCAL_IGNORE_WIN32(WSAECONNABORTED);	
		DBG_LOCAL_IGNORE_WIN32(WSAECONNRESET);
		DBG_LOCAL_IGNORE_WIN32(WSAENOTSOCK);

		bool bMagicReceived = false;
		if (Incoming) {			
			DBG_LOCAL_IGNORE_NAME(E_EXT_EndOfStream, ignE_EXT_EndOfStream);

			UInt32 magic = BinaryReader(Tcp.Stream).ReadUInt32();
			EXT_LOCK (NetManager.MtxNets) {
				EXT_FOR (P2P::Net *net, NetManager.m_nets) {
					if (net->Listen && net->ProtocolMagic == magic) {
						Net = net;
						goto LAB_FOUND;
					}
				}
				return;
LAB_FOUND:
				ThreadBase::Delete();
				m_owner = &Net->m_tr;
				EXT_LOCK (m_owner->m_cs) {
					m_owner->m_ar.push_back(this);
					m_owner->ExternalAddRef();
				}
			}
			bMagicReceived = true;

			Peer = Net->CreatePeer();						// temporary
			Peer->EndPoint = Tcp.Client.RemoteEndPoint;
		} else {
			epRemote = Peer->EndPoint;
			TRC(3, "Connecting to " << epRemote);
			DBG_LOCAL_IGNORE_WIN32(WSAETIMEDOUT);
			DBG_LOCAL_IGNORE_WIN32(WSAEADDRNOTAVAIL);
			DBG_LOCAL_IGNORE_WIN32(WSAENETUNREACH);
			
			Tcp.Client.ReuseAddress = true;
			Tcp.Client.Bind(epRemote.Address.AddressFamily==AddressFamily::InterNetwork ? IPEndPoint(IPAddress::Any, NetManager.LocalEp4.Port) : IPEndPoint(IPAddress::IPv6Any, NetManager.LocalEp6.Port));
			Tcp.Client.ReceiveTimeout = P2P_CONNECT_TIMEOUT;
			Tcp.Client.Connect(epRemote);
			Tcp.Client.ReceiveTimeout = 0;
			m_dtCheckLastRecv = DateTime::UtcNow()+TimeSpan::FromMinutes(1);
			Net->Attempt(Peer);
		}
		Net->AddLink(this);

#if UCFG_P2P_SEND_THREAD
		(SendThread = new LinkSendThread(_self))->Start();
#endif

		epRemote = Tcp.Client.RemoteEndPoint;

		Net->OnInitLink(_self);
		Tcp.Client.Blocking = false;

		DateTime dtNextPeriodic = DateTime::UtcNow()+TimeSpan::FromSeconds(P2P::PERIODIC_SEND_SECONDS);
		const size_t cbHdr = Net->GetMessageHeaderSize();
		Blob blobMessage(0, cbHdr);
		bool bReceivingPayload = false;
		size_t cbReceived = 0;

		if (bMagicReceived) {
			cbReceived = sizeof(UInt32);
			*(UInt32*)blobMessage.data() = Net->ProtocolMagic;
		}

		Socket::BlockingHandleAccess hp(Tcp.Client);

		while (!m_bStop) {
			bool bHasToSend = EXT_LOCKED(Mtx, DataToSend.Size);
			fd_set readfds, writefds;
			FD_ZERO(&readfds);
			FD_ZERO(&writefds);
			FD_SET((SOCKET)hp, &readfds);
			FD_SET((SOCKET)hp, &writefds);

			timeval timeout;
			TimeSpan::FromSeconds(std::min(std::min(PING_SECONDS, INACTIVE_PEER_SECONDS), P2P::PERIODIC_SEND_SECONDS)).ToTimeval(timeout);
			SocketCheck(::select(2, &readfds, (bHasToSend ? &writefds : 0), 0, &timeout));

			DateTime now = DateTime::UtcNow();

			if (FD_ISSET(hp, &readfds)) {
				while (!m_bStop) {
					int rc = Tcp.Client.Receive(blobMessage.data()+cbReceived, blobMessage.Size-cbReceived);
					if (!rc)
						goto LAB_EOF;
					if (rc < 0)
						break;
				
					if ((cbReceived += rc) == blobMessage.Size) {
						do {
							if (!bReceivingPayload) {
								bReceivingPayload = true;
								if (size_t cbPayload = Net->GetMessagePayloadSize(blobMessage)) {
									blobMessage.put_Size(blobMessage.Size + cbPayload);
									break;
								}
							}					
							if (bReceivingPayload) {
								ptr<Message> msg;
								CMemReadStream stm(blobMessage);
								try {
//!!!									DBG_LOCAL_IGNORE_NAME(E_EXT_Protocol_Violation, ignE_EXT_Protocol_Violation);

									msg = Net->RecvMessage(_self, BinaryReader(stm));
								} catch (RCExc&) {
									Peer->Misbehavings += 100;
									NetManager.BanPeer(*Peer);
									throw;
								}

								blobMessage.Size = cbHdr;
								bReceivingPayload = false;
								cbReceived = 0;
								msg->Link = this;
								EXT_LOCK (Mtx) {
									m_dtLastRecv = now;
									Peer->LastLive = now;
								}
								Net->OnMessageReceived(msg);
							}
						} while (false);
					}
				}
			}

			if (m_bStop)
				break;

			if (bHasToSend && FD_ISSET(hp, &writefds)) {
				EXT_LOCK (Mtx) {
					while (DataToSend.Size) {
						int rc = Tcp.Client.Send(DataToSend.constData(), DataToSend.Size);
						if (rc < 0)
							break;
						DataToSend.Replace(0, rc, ConstBuf(0, 0));
					}
				}
			}

			EXT_LOCK (Mtx) {
				if (now-Peer->LastLive > TimeSpan::FromSeconds(INACTIVE_PEER_SECONDS))
					break;
			}
			if (now > dtNextPeriodic) {
				OnPeriodic();
				dtNextPeriodic = now+TimeSpan::FromSeconds(P2P::PERIODIC_SEND_SECONDS);
			}
			if (now-m_dtLastSend > TimeSpan::FromSeconds(PING_SECONDS)) {
				if (ptr<Message> m = CreatePingMessage())
					Send(m);
			}
		}
	} catch (RCExc) {
	}
LAB_EOF:
	TRC(3, "Disconnecting " << epRemote);
#if UCFG_P2P_SEND_THREAD
	if (SendThread) {
		if (!SendThread->m_bStop)
			SendThread->Stop();
		SendThread->Join();
	}
#endif
	if (Net)
		Net->OnCloseLink(_self);
}

ListeningThread::ListeningThread(P2P::NetManager& netManager, CThreadRef& tr, AddressFamily af)
	:	base(&tr)
	,	NetManager(netManager)
	,	m_af(af)
{
//	StackSize = UCFG_THREAD_STACK_SIZE;
	m_bSleeping = true;
}

void ListeningThread::Execute() {
	Name = "ListeningThread";

	try {

		if (m_af == AddressFamily::InterNetwork) {
			{
				Socket sock;
				sock.Create(m_af, SocketType::Stream, ProtocolType::Tcp);
				try {
					DBG_LOCAL_IGNORE_WIN32(WSAEADDRINUSE);

					sock.Bind(IPEndPoint(IPAddress::Any, (UInt16)NetManager.ListeningPort));
				} catch (RCExc e) {
					if (e.HResult != HRESULT_FROM_WIN32(WSAEADDRINUSE))
						throw;
					sock.Bind(IPEndPoint(IPAddress::Any, 0));
				}		
				NetManager.ListeningPort = sock.get_LocalEndPoint().Port;
			}

			DBG_LOCAL_IGNORE_WIN32(WSAEAFNOSUPPORT);
			if (Socket::OSSupportsIPv6) {
				(new ListeningThread(NetManager, *m_owner, AddressFamily::InterNetworkV6))->Start();
			}
			m_sock.Create(m_af, SocketType::Stream, ProtocolType::Tcp);
			m_sock.ReuseAddress = true;
			m_sock.Bind(IPEndPoint(IPAddress::Any, (UInt16)NetManager.ListeningPort));
		} else {
			m_sock.Create(m_af, SocketType::Stream, ProtocolType::Tcp);
			m_sock.ReuseAddress = true;
			m_sock.Bind(IPEndPoint(IPAddress::IPv6Any, (UInt16)NetManager.ListeningPort));
			NetManager.LocalEp6 = m_sock.LocalEndPoint;
		}

		SocketKeeper sockKeeper(_self, m_sock);
		DBG_LOCAL_IGNORE_WIN32(WSAEINTR);

		m_sock.Listen();
		while (!m_bStop) {
			Socket s;
			m_sock.Accept(s);
			IPEndPoint epRemote = s.RemoteEndPoint;
			if (NetManager.IsBanned(epRemote.Address)) {
				TRC(2, "Denied connect from banned " << epRemote);
			} else if (NetManager.IsTooManyLinks()) {
				TRC(2, "Incoming connection refused: Too many links");
			} else {
				TRC(3, "Connected from " << epRemote);

				ptr<Link> link = NetManager.CreateLink(*m_owner);
				link->Incoming = true;
				link->Tcp.Client = move(s);
			//!!!		link->Peer = peer;
				link->Start();
			}
		}
	} catch (RCExc) {
	}
}

/*!!!R
ptr<Peer> Net::GetPeer(const IPEndPoint& ep) {
	ptr<Peer> r;
	EXT_LOCK (MtxPeers) {
		if (!Lookup(Peers, ep.Address, r)) {
			r = CreatePeer();
			r->LastLive = DateTime::UtcNow();
			Peers[(r->EndPoint = ep).Address] = r;
		}
	}
	return r;
}*/

Link *NetManager::CreateLink(CThreadRef& tr) {
	return new Link(_self, tr);
}

bool NetManager::IsTooManyLinks() {
	int links = 0, limSum = 0;
	EXT_LOCK (MtxNets) {
		for (int i=0; i<m_nets.size(); ++i) {
			PeerManager& pm = *m_nets[i];
			EXT_LOCK (pm.MtxPeers) {
				links += pm.Links.size();
			}
			limSum += pm.MaxLinks;
		}
		return links >= limSum;
	}
}

} // P2P::

