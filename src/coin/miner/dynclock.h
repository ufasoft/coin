/*######     Copyright (c) 1997-2013 Ufasoft  http://ufasoft.com  mailto:support@ufasoft.com,  Sergey Pavlov  mailto:dev@ufasoft.com #######################################
#                                                                                                                                                                          #
# This program is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation;  #
# either version 3, or (at your option) any later version. This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the      #
# implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details. You should have received a copy of the GNU #
# General Public License along with this program; If not, see <http://www.gnu.org/licenses/>                                                                               #
##########################################################################################################################################################################*/

#pragma once

namespace Coin {

struct FreqData {
	double ErrorRate, MaxErrorRate, ErrorWeight;
	double ErrorCount;

	FreqData()
		:	ErrorRate(0)
		,	MaxErrorRate(0)
		,	ErrorWeight(0)
		,	ErrorCount(0)
	{}
};

class DynClockDevice {
public:
	vector<FreqData> FreqDatas;
	int m_clockUnits, m_maxClockUnits, m_defaultClockUnits;

	virtual ~DynClockDevice() {}

	void Update(double errPortion);

	virtual int GetClock() { return m_clockUnits; }
	virtual void SetClock(int mhz) { m_clockUnits = mhz; }
	
protected:

	virtual void ChangeFrequency(int units) =0;

private:

};



} // Coin::

