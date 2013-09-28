/*######     Copyright (c) 1997-2013 Ufasoft  http://ufasoft.com  mailto:support@ufasoft.com,  Sergey Pavlov  mailto:dev@ufasoft.com #######################################
#                                                                                                                                                                          #
# This program is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation;  #
# either version 3, or (at your option) any later version. This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the      #
# implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details. You should have received a copy of the GNU #
# General Public License along with this program; If not, see <http://www.gnu.org/licenses/>                                                                               #
##########################################################################################################################################################################*/

#include <el/ext.h>

#include "dynclock.h"

namespace Coin {

const double MAX_ERROR_RATE = 0.05;
const double ERROR_HYSTERESIS = 0.1;

const int MIN_GOOD_SAMPLES = 150;

void DynClockDevice::Update(double errPortion) {
	FreqData& df = FreqDatas.at(m_clockUnits);
	
	df.ErrorCount = df.ErrorCount*0.995 + errPortion;
	df.ErrorWeight = df.ErrorWeight*0.995 + 1;
	df.ErrorRate = df.ErrorCount / df.ErrorWeight * min(df.ErrorWeight / 100, 1.0);
	df.MaxErrorRate = max(df.MaxErrorRate, df.ErrorRate);

	for (int i=1; i<m_maxClockUnits; ++i)
		FreqDatas[i+1].MaxErrorRate = max(FreqDatas[i+1].MaxErrorRate, FreqDatas[i].MaxErrorRate * (1 + 20.0/i));

	int maxM = 0;
	for (int defFreq=min(m_defaultClockUnits, m_maxClockUnits);  maxM < defFreq && FreqDatas[maxM+1].MaxErrorRate < MAX_ERROR_RATE;)
		++maxM;
	while (maxM < m_maxClockUnits && FreqDatas[maxM+1].MaxErrorRate < MAX_ERROR_RATE && FreqDatas[maxM].ErrorWeight >= MIN_GOOD_SAMPLES)
		++maxM;
	
	int bestM = 0;
	double bestR = 0;
	for (int i=0; i <= maxM; ++i) {
		double r = (i + 1 + (i==m_clockUnits ? ERROR_HYSTERESIS : 0)) * (1 - FreqDatas[i].MaxErrorRate);
		if (r > bestR) {
			bestM = i;
			bestR = r;
		}
	}
	if (bestM != m_clockUnits)
		ChangeFrequency(bestM);
}


} // Coin::

