/*######     Copyright (c) 1997-2013 Ufasoft  http://ufasoft.com  mailto:support@ufasoft.com,  Sergey Pavlov  mailto:dev@ufasoft.com #######################################
#                                                                                                                                                                          #
# This program is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation;  #
# either version 3, or (at your option) any later version. This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the      #
# implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details. You should have received a copy of the GNU #
# General Public License along with this program; If not, see <http://www.gnu.org/licenses/>                                                                               #
##########################################################################################################################################################################*/

#include <el/ext.h>

#include "async-text-client.h"

namespace Ext { namespace Inet { 

void AsyncTextClient::Execute() {
	DBG_LOCAL_IGNORE_WIN32(WSAECONNREFUSED);
	DBG_LOCAL_IGNORE_WIN32(WSAECONNABORTED);
	DBG_LOCAL_IGNORE_WIN32(WSAECONNRESET);
	DBG_LOCAL_IGNORE_NAME(E_EXT_EndOfStream, ignE_EXT_EndOfStream);

	for (pair<String, bool> pp; !m_bStop && (pp=R.ReadLineEx()).first!=nullptr;) {
		if (FirstByte != -1)
			pp.first = String((char)std::exchange(FirstByte, -1), 1) + pp.first;
		if (pp.second)
			OnLine(pp.first);
		else
			OnLastLine(pp.first);										// last line can be broken
	}
}

void AsyncTextClient::SendPendingData() {
	EXT_LOCK (MtxSend) {
		if (!DataToSend.IsEmpty())
			Tcp.Stream.WriteBuf(W.Encoding.GetBytes(exchange(DataToSend, String())));
	}
}

void AsyncTextClient::Send(RCString cmd) {
	EXT_LOCK (MtxSend) {
		if (Tcp.Client.Valid())
			W.WriteLine(cmd);
		else
			DataToSend += cmd+W.NewLine;
	}
}


}} // Ext::Inet::

