/*######     Copyright (c) 1997-2013 Ufasoft  http://ufasoft.com  mailto:support@ufasoft.com,  Sergey Pavlov  mailto:dev@ufasoft.com #######################################
#                                                                                                                                                                          #
# This program is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation;  #
# either version 3, or (at your option) any later version. This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the      #
# implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details. You should have received a copy of the GNU #
# General Public License along with this program; If not, see <http://www.gnu.org/licenses/>                                                                               #
##########################################################################################################################################################################*/

#pragma once

namespace Ext { namespace Inet { namespace P2P {

class DetectGlobalIp;

class DetectIpThread : public Thread {
	typedef Thread base;
public:
	P2P::DetectGlobalIp& DetectGlobalIp;

	DetectIpThread(P2P::DetectGlobalIp& dgi, thread_group *tr)
		:	base(tr)
		,	DetectGlobalIp(dgi)
	{
	}

	~DetectIpThread();
	void Execute() override;
};

class DetectGlobalIp {
public:
    ptr<DetectIpThread> Thread;

	~DetectGlobalIp();
	
	void Start(thread_group *tr) {
		Thread = new DetectIpThread(_self, tr);
		Thread->Start();	
	}
protected:	
	virtual void OnIpDetected(const IPAddress& ip) {}

private:
	mutex m_mtx;

friend class DetectIpThread;
};


}}} // Ext::Inet::P2P::

