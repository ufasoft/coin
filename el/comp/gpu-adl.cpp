/*######     Copyright (c) 1997-2013 Ufasoft  http://ufasoft.com  mailto:support@ufasoft.com,  Sergey Pavlov  mailto:dev@ufasoft.com #######################################
#                                                                                                                                                                          #
# This program is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation;  #
# either version 3, or (at your option) any later version. This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the      #
# implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details. You should have received a copy of the GNU #
# General Public License along with this program; If not, see <http://www.gnu.org/licenses/>                                                                               #
##########################################################################################################################################################################*/

#include <el/ext.h>

#include "gpu-adl.h"

namespace Ext { namespace Gpu {

int AdlCheck(int v) {
	if (v < ADL_OK) {
		throw AdlException(MAKE_HRESULT(SEVERITY_ERROR, FACILITY_GPU_ADL, -v));
	}
	return v;
}

static void * __stdcall ADL_Main_Memory_Alloc(int iSize) {
	return Malloc(iSize);
}


AdlEngine::AdlEngine() {
	String dllName;
#if UCFG_USE_POSIX
	dllName = "Libatiadlxx.so";
#elif defined(_WIN64)
	dllName = "atiadlxx.dll";
#else
	dllName = System.Is64BitNativeSystem() ? "atiadlxy.dll" : "atiadlxx.dll";
#endif
	try {
		m_dll.Load(dllName);
		m_bLoaded = true;
	} catch (RCExc) {
		return;
	}
	ADL_Main_Control_Create.Init(			m_dll, "ADL_Main_Control_Create");
	ADL_Main_Control_Destroy.Init(			m_dll, "ADL_Main_Control_Destroy");
	ADL_Main_Control_Refresh.Init(			m_dll, "ADL_Main_Control_Refresh");
	ADL_Adapter_NumberOfAdapters_Get.Init(	m_dll, "ADL_Adapter_NumberOfAdapters_Get");
	ADL_Adapter_Active_Get.Init(			m_dll, "ADL_Adapter_Active_Get");
	ADL_Adapter_AdapterInfo_Get.Init(		m_dll, "ADL_Adapter_AdapterInfo_Get");
	ADL_Overdrive5_Temperature_Get.Init(	m_dll, "ADL_Overdrive5_Temperature_Get");
	ADL_Adapter_ID_Get.Init(				m_dll, "ADL_Adapter_ID_Get");

	AdlCheck(ADL_Main_Control_Create(&ADL_Main_Memory_Alloc, 0));

	RefreshAdapterInfo();
}

void AdlEngine::RefreshAdapterInfo() {
	int count;
	AdlCheck(ADL_Adapter_NumberOfAdapters_Get(&count));
	TRC(3, count << " adapters");

	m_vAdapterInfo.resize(count);
	if (count)
		AdlCheck(ADL_Adapter_AdapterInfo_Get(&m_vAdapterInfo[0], count*sizeof(AdapterInfo)));
}

int AdlEngine::get_DeviceCount() {
	return m_vAdapterInfo.size();
}

AdlDevice AdlEngine::GetDevice(int idx) {
	if (idx >= m_vAdapterInfo.size())
		Throw(E_FAIL);
	return AdlDevice(_self, idx);
}

AdlDevice::AdlDevice(AdlEngine& eng, int idx)
	:	m_eng(eng)
	,	Index(idx)
{
	UDID = eng.m_vAdapterInfo[Index].strUDID;
}


bool AdlDevice::get_Active() {
	int r;
	AdlCheck(m_eng.ADL_Adapter_Active_Get(Index, &r));
	return r;
}

void AdlDevice::UpdateIndex() {
	m_eng.RefreshAdapterInfo();
	for (int i=0; i<m_eng.m_vAdapterInfo.size(); ++i) {
		if (UDID == m_eng.m_vAdapterInfo[i].strUDID) {
			Index = i;
			return;
		}
	}
	Throw(E_FAIL);
}

Temperature AdlDevice::get_Temperature() {
	ADLTemperature t = { sizeof t };
	int rc = m_eng.ADL_Overdrive5_Temperature_Get(Index, 0, &t);
	if (ADL_ERR_INVALID_ADL_IDX == rc) {
		UpdateIndex();
		AdlCheck(m_eng.ADL_Overdrive5_Temperature_Get(Index, 0, &t));
	}
	return Temperature::FromCelsius(float(t.iTemperature)/1000);
}



}} // Ext::Gpu::

