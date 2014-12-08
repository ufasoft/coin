/*######     Copyright (c) 1997-2013 Ufasoft  http://ufasoft.com  mailto:support@ufasoft.com,  Sergey Pavlov  mailto:dev@ufasoft.com #######################################
#                                                                                                                                                                          #
# This program is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation;  #
# either version 3, or (at your option) any later version. This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the      #
# implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details. You should have received a copy of the GNU #
# General Public License along with this program; If not, see <http://www.gnu.org/licenses/>                                                                               #
##########################################################################################################################################################################*/

#pragma once

#include "miner.h"

namespace Coin {

class StopException : public Exception {
};

class SerialDeviceThread : public WorkerThreadBase {
	typedef WorkerThreadBase base;
public:
	String m_devpath;
	CBool Detected;
	CBool AutoStart;
	AutoResetEvent m_evDetected, m_evStartMining;
	int MsTimeout;

	SerialDeviceThread(BitcoinMiner& miner, thread_group *tr)
		:	base(miner, tr)
		,	m_file(0)
		,	MsTimeout(1000)
	{
	}

	void Write(const ConstBuf& cbuf);
	Blob Command(const ConstBuf cmd, size_t replySize);
	void Stop() override;
protected:
	FILE *m_file;

	void CloseFile() {
		if (FILE *file = exchange(m_file, nullptr))
			fclose(file);
	}

	virtual void CommunicateDevice() =0;
	void Execute() override;
};




} // Coin::

