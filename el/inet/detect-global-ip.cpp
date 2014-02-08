/*######     Copyright (c) 1997-2013 Ufasoft  http://ufasoft.com  mailto:support@ufasoft.com,  Sergey Pavlov  mailto:dev@ufasoft.com #######################################
#                                                                                                                                                                          #
# This program is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation;  #
# either version 3, or (at your option) any later version. This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the      #
# implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details. You should have received a copy of the GNU #
# General Public License along with this program; If not, see <http://www.gnu.org/licenses/>                                                                               #
##########################################################################################################################################################################*/

#include <el/ext.h>

#include <el/libext/ext-http.h>

#include "detect-global-ip.h"

namespace Ext { namespace Inet { namespace P2P {

static wregex s_reCurrentIp(L"IP Address:\\s+(\\d+\\.\\d+\\.\\d+\\.\\d+)");

DetectIpThread::~DetectIpThread() {
}

void DetectIpThread::Execute() {
	Name = "DetectIpThread";

	try {
		while (!m_bStop) {
			try {
				DBG_LOCAL_IGNORE_WIN32(ERROR_INTERNET_NAME_NOT_RESOLVED);
				DBG_LOCAL_IGNORE_WIN32(ERROR_INTERNET_TIMEOUT);
				DBG_LOCAL_IGNORE_WIN32(ERROR_HTTP_INVALID_SERVER_RESPONSE);

				String s = WebClient().DownloadString("http://checkip.dyndns.org");
				Smatch m;
				if (regex_search(s, m, s_reCurrentIp)) {
					IPAddress ip = IPAddress::Parse(m[1]);
					if (ip.IsGlobal())
						DetectGlobalIp.OnIpDetected(ip);
					break;
				}
			} catch (RCExc) {
			}
			Sleep(60000);
		}
	} catch (RCExc) {
	}
	EXT_LOCK (DetectGlobalIp.m_mtx) {
		DetectGlobalIp.Thread = nullptr;
	}
}

DetectGlobalIp::~DetectGlobalIp() {
	ptr<DetectIpThread> t;

	EXT_LOCK (m_mtx) {
		t = Thread;
	}
	if (t) {
		t->interrupt();
		t->Join();
	}
}


}}} // Ext::Inet::P2P::

