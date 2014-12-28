/*######     Copyright (c) 1997-2015 Ufasoft  http://ufasoft.com  mailto:support@ufasoft.com,  Sergey Pavlov  mailto:dev@ufasoft.com #########################################################################################################
#                                                                                                                                                                                                                                            #
# This program is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation;  either version 3, or (at your option) any later version.          #
# This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.   #
# You should have received a copy of the GNU General Public License along with this program; If not, see <http://www.gnu.org/licenses/>                                                                                                      #
############################################################################################################################################################################################################################################*/

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
	typedef Interlocked interlocked_policy;

	CoinEng& Eng;
	IPEndPoint Server;
	String Channel;
	uint16_t ListeningPort;
	CPointer<Coin::IrcThread> IrcThread;

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
	CPointer<thread_group> m_pTr;

	IrcManager(Coin::CoinDb& coinDb)
		:	CoinDb(coinDb)
	{}

	mutex Mtx;
	unordered_map<IPEndPoint, ptr<IrcThread>> IrcServers;

	void AttachChannelClient(IPEndPoint server, RCString channel, ChannelClient *channelClient);
	void DetachChannelClient(ChannelClient *channelClient);
};



} // Coin::

