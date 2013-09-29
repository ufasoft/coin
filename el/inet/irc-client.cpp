/*######     Copyright (c) 1997-2013 Ufasoft  http://ufasoft.com  mailto:support@ufasoft.com,  Sergey Pavlov  mailto:dev@ufasoft.com #######################################
#                                                                                                                                                                          #
# This program is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation;  #
# either version 3, or (at your option) any later version. This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the      #
# implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details. You should have received a copy of the GNU #
# General Public License along with this program; If not, see <http://www.gnu.org/licenses/>                                                                               #
##########################################################################################################################################################################*/

#include <el/ext.h>


#include "irc-client.h"

namespace Ext { namespace Inet { namespace Irc {

IrcClient::IrcClient() {
	Tcp.Client.Create();
}

void IrcClient::Connect() {
	if (Username.IsEmpty())
		Username = Nick;
	if (RealName.IsEmpty())
		RealName = Username;
	TRC(1, EpServer);
	Tcp.Client.Connect(EpServer);
}

void IrcClient::Disconnect() {
	Tcp.Client.Close();
}

void IrcClient::SetNick(RCString nick) {
	Send("NICK "+nick);
}

/*!!!
vector<IrcUserInfo> IrcClient::Who(RCString channelName) {
	EXT_LOCK (Mtx) {
		m_userList.clear();
	}
//	m_ev.Reset();
	W.WriteLine("WHO "+channelName);
//	m_ev.Lock();
	return EXT_LOCKED(Mtx, m_userList);
}*/

static regex s_reMessage("^(?::(\\S+)\\s)?(?:(\\d\\d\\d)|([A-Za-z]+))(?:(\\s.*)?)");	// prefix - 1   reply - 2   command - 3  params - 4
static regex s_reParams(" +:([^:]*)| +([^ :]*)");
static regex s_reUserhost("=[-+](?:[^@]+@)?([\\S]+)");

void IrcClient::OnLine(RCString line) {
	smatch m;
	string sline = explicit_cast<string>(line);
	if (regex_search(sline, m, s_reMessage)) {
		String prefix = m[1];
		String c = m[3];
		String reply = m[2];
		string pars = m[4];
		vector<String> params;
		for (regex_iterator<string::const_iterator> it(pars.begin(), pars.end(), s_reParams), e; it!=e; ++it) {
			params.push_back((*it)[1].matched ? (*it)[1] : (*it)[2]);
		}
		if (!reply.IsEmpty()) {
			int nReply = atoi(reply);
			switch (nReply) {

			case RPL_CREATED:
				SendPendingData();
				ConnectionEstablished = true;
				OnCreatedConnection();
				break;
			case RPL_USERHOST:
				{
					smatch m;
					if (regex_search(pars, m, s_reUserhost)) {
						String host = m[1];
						OnUserHost(host);
					}
				}
				break;
			case RPL_WHOREPLY:
				if (params.size() > 7) {
					String channel = params.at(1).Substring(1);
					String realname = params.at(7).Split("", 2).at(1).Trim();
					IrcUserInfo info = { params.at(2), params.at(3), params.at(4), params.at(5), realname };
					m_whoLists[channel].push_back(info);
				}
				break;
			case RPL_NAMREPLY: 
				{
					String channel = params.at(2).Substring(1);
					vector<String> nicks = params.at(3).Split();
					m_nameList[channel].insert(nicks.begin(), nicks.end());
				}
				break;
			case RPL_ENDOFNAMES:
				{
					String channel = params.at(1).Substring(1);
					OnNickNamesComplete(channel, m_nameList[channel]);
				}
				break;
			case RPL_ENDOFWHO:
				{
					String channel = params.at(1).Substring(1);
					CWhoList::iterator it = m_whoLists.find(channel);
					if (it != m_whoLists.end()) {
						vector<IrcUserInfo> vec = it->second;
						m_whoLists.erase(it);
						OnUserListComplete(channel, vec);
					}
				}
				break;
			}
		} else if (!c.IsEmpty()) {
			if (c == "NOTICE") {
				if (params.size() > 0) {
					if (params[0] == "AUTH") {
						OnAuth();
					}
				}
			} else if (c == "PING") {
				String s1 = params.at(0), s2;
				if (params.size() >= 2)
					s2 = params.at(1);
				OnPing(s1, s2);
			}
		}
	} else
		Throw(E_FAIL);
}

void IrcClient::OnAuth() {
	if (!m_bUsernameSent) {
		SetNick(Nick);
		Send("USER "+Username+" 8 * : "+RealName);
		m_bUsernameSent = true;
	}
}

void IrcClient::OnPing(RCString server1, RCString server2) {
	String s = "PONG "+server1;
	if (!server2.IsEmpty())
		s += " "+server2;
	Send(s);
}


}}} // Ext::Inet::Irc::

