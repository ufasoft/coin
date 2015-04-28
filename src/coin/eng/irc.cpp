/*######   Copyright (c) 2013-2015 Ufasoft  http://ufasoft.com  mailto:support@ufasoft.com,  Sergey Pavlov  mailto:dev@ufasoft.com ####
#                                                                                                                                     #
# 		See LICENSE for licensing information                                                                                         #
#####################################################################################################################################*/

#include <el/ext.h>

#include "../util/util.h"
#include "irc.h"
#include "eng.h"

namespace Coin {

void ChannelClient::ProcessNick(RCString nick, const DateTime& now) {
	if (nick[0] == 'u') {
		Blob blob;
		try {
			DBG_LOCAL_IGNORE_WIN32(ERROR_CRC);
//!!!R				DBG_LOCAL_IGNORE(E_FAIL);
			DBG_LOCAL_IGNORE(E_INVALIDARG);
			blob = ConvertFromBase58ShaSquare(nick.substr(1));
		} catch (RCExc) {
		}
		if (blob.Size == 6) {
			CMemReadStream stm(blob);
			BinaryReader rd(stm);
			int32_t ip4 = rd.ReadInt32();
			uint16_t port = ntohs(rd.ReadUInt16());
			IPEndPoint ep(ip4, port);

			TRC(7, ep);

			Eng.Add(ep, NODE_NETWORK, now, TimeSpan::FromMinutes(51));
		} else {
			TRC(2, "Nick decode error: " << nick);
		}
	}
}

void ChannelClient::OnNickNamesComplete(RCString channel, const unordered_set<String>& nicks) {
	DateTime now = DateTime::UtcNow();
	EXT_FOR (const String& nick, nicks) {
		if (nick != IrcThread->Nick)
			ProcessNick(nick, now);
	}
}

void ChannelClient::OnUserListComplete(RCString channel, const vector<IrcUserInfo>& userList) {
	DateTime now = DateTime::UtcNow();
	for (int i=0; i<userList.size(); ++i) {
		const IrcUserInfo& ui = userList[i];
		TRC(2, "IRC-client Host: " << ui.Host);
		
		if (ui.RealName != IrcThread->RealName)
			ProcessNick(ui.Nick, now);
	}
}

IrcThread::IrcThread(Coin::IrcManager& ircManager)
	:	IrcManager(ircManager)
{
//	StackSize = UCFG_THREAD_STACK_SIZE;
	m_owner = ircManager.m_pTr;
	Tcp.ProxyString = ircManager.CoinDb.ProxyString;
}

void IrcThread::Execute() {
	Name = "IrcThread";

	DBG_LOCAL_IGNORE_CONDITION(errc::timed_out); //!!!
	DBG_LOCAL_IGNORE_CONDITION(errc::not_a_socket);

	try {
		struct ThreadPointerCleaner {
			Coin::IrcManager *ircManager;
			IPEndPoint ep;

			~ThreadPointerCleaner() {
				EXT_LOCK (ircManager->Mtx) {
					ircManager->IrcServers.erase(ep);
				}
			}
		} threadPointerCleaner = { &IrcManager, EpServer };

		TRC(1, "IrcThread started Fam=" << (int)IrcManager.CoinDb.LocalIp4.get_AddressFamily());

		EXT_LOCK (IrcManager.CoinDb.MtxDb) {
			Blob blob = IrcManager.CoinDb.LocalIp4.GetAddressBytes();
			if (blob.Size==4 && 0!=*(uint32_t*)blob.constData()) {
				MemoryStream ms;
				ms.WriteBuf(blob);
				uint16_t port = 0;
				if (!ChannelClients.empty())
					port = uint16_t(ChannelClients.begin()->second->ListeningPort);
				BinaryWriter(ms).Ref() << uint16_t(htons(port));
				Nick = "u" + ConvertToBase58ShaSquare(ms);
			} else
				Nick = "x"+Convert::ToString(Ext::Random().Next(1000000000));
		}

		SocketKeeper sockKeeper(_self, Tcp.Client);
		DBG_LOCAL_IGNORE_CONDITION(errc::network_unreachable);
		Connect();
		base::Execute();
	} catch (RCExc) {
//		TRC(1, ex.Message);
	}
}

void IrcThread::OnCreatedConnection() {
	Send("USERHOST "+Nick);
	EXT_LOCK (Mtx) {
		for (CChannelClients::iterator it=ChannelClients.begin(); it!=ChannelClients.end(); ++it) {
			String channel = (it->second)->Channel;
			Send("JOIN #"+channel);
//!!!R			Send("WHO #"+channel);
		}
	}
}

void IrcThread::OnUserHost(RCString host) {
	IPAddress ip = IPAddress::Parse(host);
	ip.Normalize();
	if (ip.IsGlobal())
		IrcManager.CoinDb.AddLocal(ip);
}

void IrcThread::JoinChannel(ChannelClient *channelClient) {
	bool bStart;
	EXT_LOCK (Mtx) {
		bStart = ChannelClients.empty();
		ChannelClients[channelClient->Channel] = channelClient;
		if (ConnectionEstablished) {
			Send("JOIN #"+channelClient->Channel);
//!!!R			Send("WHO #"+channelClient->Channel);
		}
	}
	channelClient->IrcThread.reset(this);
	if (bStart)
		Start();
}

void IrcThread::UnjoinChannel(ChannelClient *channelClient) {
	EXT_LOCK (Mtx) {
		ChannelClients.erase(channelClient->Channel);
		if (ConnectionEstablished)
			Send("PART #"+channelClient->Channel);
		if (ChannelClients.empty())
			Stop();
	}
}

void IrcThread::OnNickNamesComplete(RCString channel, const unordered_set<String>& nicks) {
	ptr<ChannelClient> channelClient;
	EXT_LOCK (Mtx) {
		Lookup(ChannelClients, channel, channelClient);
	}
	if (channelClient)
		channelClient->OnNickNamesComplete(channel, nicks);
}

void IrcThread::OnUserListComplete(RCString channel, const vector<IrcUserInfo>& userList) {
	ptr<ChannelClient> channelClient;
	EXT_LOCK (Mtx) {
		Lookup(ChannelClients, channel, channelClient);
	}
	if (channelClient)
		channelClient->OnUserListComplete(channel, userList);
}


void IrcManager::AttachChannelClient(IPEndPoint server, RCString channel, ChannelClient *channelClient) {
	channelClient->Server = server;
	channelClient->Channel = channel;
	ptr<IrcThread> ircThread;
	EXT_LOCK (Mtx) {
		if (!Lookup(IrcServers, server, ircThread)) {
			ircThread = new IrcThread(_self);
			ircThread->m_owner = m_pTr;
			ircThread->EpServer = server;
			IrcServers[server] = ircThread;
		}
		ircThread->JoinChannel(channelClient);
	}
}

void IrcManager::DetachChannelClient(ChannelClient *channelClient) {
	ptr<IrcThread> ircThread;
	EXT_LOCK (Mtx) {
		if (Lookup(IrcServers, channelClient->Server, ircThread))
			ircThread->UnjoinChannel(channelClient);
	}
}

} // Coin::

