/*######   Copyright (c) 2012-2015 Ufasoft  http://ufasoft.com  mailto:support@ufasoft.com,  Sergey Pavlov  mailto:dev@ufasoft.com ####
#                                                                                                                                     #
# 		See LICENSE for licensing information                                                                                         #
#####################################################################################################################################*/

#pragma once

#include <el/inet/irc-client.h>
using namespace Ext::Inet::Irc;

namespace Coin {

class CoinEng;
class IrcManager;
class CoinDb;
class IrcThread;

class ChannelClient : public Object {
public:
	typedef InterlockedPolicy interlocked_policy;

	CoinEng& Eng;
	IPEndPoint Server;
	String Channel;
	uint16_t ListeningPort;
	observer_ptr<Coin::IrcThread> IrcThread;

	ChannelClient(CoinEng& eng)
		:	Eng(eng)
		,	ListeningPort(0)
	{}

	void OnNickNamesComplete(RCString channel, const unordered_set<String>& nicks);
	void OnUserListComplete(RCString channel, const vector<IrcUserInfo>& userList);
private:
	void ProcessNick(RCString nick, const DateTime& now);
};

class IrcThread : public IrcClient {
	typedef IrcClient base;
public:
	Coin::IrcManager& IrcManager;

	typedef unordered_map<String, ptr<ChannelClient>> CChannelClients;
	CChannelClients ChannelClients;

	IrcThread(Coin::IrcManager& ircManager);
	void JoinChannel(ChannelClient *channelClient);
	void UnjoinChannel(ChannelClient *channelClient);
protected:
	void Execute() override;
	void OnCreatedConnection() override;
	void OnUserHost(RCString host) override;
	void OnNickNamesComplete(RCString channel, const unordered_set<String>& nicks) override;
	void OnUserListComplete(RCString channel, const vector<IrcUserInfo>& userList) override;
};

class IrcManager {
public:
	Coin::CoinDb& CoinDb;
	observer_ptr<thread_group> m_pTr;

	IrcManager(Coin::CoinDb& coinDb)
		:	CoinDb(coinDb)
	{}

	mutex Mtx;
	unordered_map<IPEndPoint, ptr<IrcThread>> IrcServers;

	void AttachChannelClient(IPEndPoint server, RCString channel, ChannelClient *channelClient);
	void DetachChannelClient(ChannelClient *channelClient);
};



} // Coin::

