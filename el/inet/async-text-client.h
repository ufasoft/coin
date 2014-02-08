/*######     Copyright (c) 1997-2013 Ufasoft  http://ufasoft.com  mailto:support@ufasoft.com,  Sergey Pavlov  mailto:dev@ufasoft.com #######################################
#                                                                                                                                                                          #
# This program is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation;  #
# either version 3, or (at your option) any later version. This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the      #
# implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details. You should have received a copy of the GNU #
# General Public License along with this program; If not, see <http://www.gnu.org/licenses/>                                                                               #
##########################################################################################################################################################################*/

#pragma once

#include <el/libext/ext-net.h>

namespace Ext { namespace Inet {
using namespace Ext;

class AsyncTextClient : public SocketThread {
	typedef SocketThread base;
public:
	TcpClient Tcp;
	IPEndPoint EpServer;
	StreamWriter W;
	StreamReader R;
	CBool ConnectionEstablished;

	AsyncTextClient()
		:	base(0)
		,	W(Tcp.Stream)
		,	R(Tcp.Stream)
		,	FirstByte(-1)
	{}

	void Send(RCString cmd);
protected:
	mutex MtxSend;
	String DataToSend;
	int FirstByte;
	
	void SendPendingData();
	void Execute() override;
	virtual void OnLine(RCString line) {}
	virtual void OnLastLine(RCString line) { OnLine(line); }
};



}} // Ext::Inet::

