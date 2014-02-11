/*######     Copyright (c) 1997-2013 Ufasoft  http://ufasoft.com  mailto:support@ufasoft.com,  Sergey Pavlov  mailto:dev@ufasoft.com #######################################
#                                                                                                                                                                          #
# This program is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation;  #
# either version 3, or (at your option) any later version. This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the      #
# implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details. You should have received a copy of the GNU #
# General Public License along with this program; If not, see <http://www.gnu.org/licenses/>                                                                               #
##########################################################################################################################################################################*/

#include <el/ext.h>

#include <el/libext/win32/ext-win.h>

#include "setupdi.h"

#pragma comment(lib, "setupapi")

namespace Ext {

void SetupDiCheck(HDEVINFO h) {
	Win32Check(h > 0);
}

void CmCheck(CONFIGRET v) {
	if (v != CR_SUCCESS)
		Throw(MAKE_HRESULT(SEVERITY_ERROR, FACILITY_CFGMGR, v));
}

Obj_DiClassDevs::~Obj_DiClassDevs() {
	if (m_h > 0)
		Win32Check(::SetupDiDestroyDeviceInfoList(m_h));
}

void DiClassDevs::iterator::ProbeCurrentDeviceInfo() {
	if (m_idx != -1) {
		BOOL b = ::SetupDiEnumDeviceInfo(m_p->m_h, m_idx, &m_di.m_data);
		if (!b) {
			Win32Check(GetLastError()==ERROR_NO_MORE_ITEMS);
			m_idx = -1;
		}
		m_di.m_p = m_p;
	}
}

DiClassDevs::iterator& DiClassDevs::iterator::operator++() {
	m_idx++;
	ProbeCurrentDeviceInfo();
	return _self;
}

DiDeviceInfo *DiClassDevs::iterator::operator->() {
	ProbeCurrentDeviceInfo();
	if (m_idx == -1)
		Throw(E_FAIL);
	return &m_di;
}

CRegistryValue DiDeviceInfo::GetRegistryProperty(DWORD prop) {
	BYTE buf[256];
	BYTE *p = buf;
	DWORD typ, dwReq;
	BOOL b = ::SetupDiGetDeviceRegistryProperty(m_p->m_h, &m_data, prop, &typ, p, sizeof buf, &dwReq);
	if (!b) {
		Win32Check(GetLastError()==ERROR_INSUFFICIENT_BUFFER);//!!!
		p = (BYTE*)alloca(dwReq);
		Win32Check(::SetupDiGetDeviceRegistryProperty(m_p->m_h, &m_data, prop, &typ, p, dwReq, 0));
	}
	return CRegistryValue(typ, p, dwReq);
}

void DiDeviceInfo::SetRegistryProperty(DWORD prop, const CRegistryValue rv) {
	Win32Check(::SetupDiSetDeviceRegistryProperty(m_p->m_h, &m_data, prop, rv.m_blob.data(), (DWORD)rv.m_blob.Size));
}

void DiDeviceInfo::ChangeState(DWORD state) {
	CPropChangeParams params;
	params.StateChange = state;
	params.Scope = DICS_FLAG_GLOBAL; //!!!DICS_FLAG_CONFIGSPECIFIC;
	SetClassInstallParams(params);
	Win32Check(::SetupDiChangeState(m_p->m_h, &m_data));
}

String DiDeviceInfo::get_DeviceInstanceID() {	
	for (int len=256;; len*=2) {
		TCHAR *buf = (TCHAR*)alloca(len*sizeof(TCHAR));
		CONFIGRET r = ::CM_Get_Device_ID(m_data.DevInst, buf, len, 0);
		switch (r) {
		case CR_BUFFER_SMALL: continue;
		case CR_SUCCESS: return buf;
		}
		CmCheck(r);
	}
}

ULONG DiDeviceInfo::get_Status() {
	ULONG status, problem;
	CmCheck(::CM_Get_DevNode_Status(&status, &problem, m_data.DevInst, 0));
	return status;
}

HKEY DiDeviceInfo::OpenRegKey() {
	HKEY r = ::SetupDiOpenDevRegKey(m_p->m_h, &m_data, DICS_FLAG_GLOBAL, 0, DIREG_DEV, KEY_QUERY_VALUE);
	Win32Check(r != INVALID_HANDLE_VALUE);
	return r;
}

LogConf DiDeviceInfo::GetLogConf(ULONG ulFlags) {
	LOG_CONF lc;
	CmCheck(::CM_Get_First_Log_Conf(&lc, m_data.DevInst, ulFlags));
	LogConf r;
	(r.m_p = new Obj_LogConf)->m_h = lc;
	return r;
}

Obj_HwResource::~Obj_HwResource() {
	CmCheck(::CM_Free_Res_Des_Handle(m_h));
}

Blob HwResource::GetData() {
	ULONG size;
	CmCheck(::CM_Get_Res_Des_Data_Size(&size, m_p->m_h, 0));
	Blob blob(0, size);
	CmCheck(::CM_Get_Res_Des_Data(m_p->m_h, blob.data(), size, 0));
	return blob;
}

IO_DES HwResource::GetIo() {
	if (m_p->m_type != ResType_IO)
		Throw(E_FAIL);
	Blob blob = GetData();
	IO_RESOURCE *r = (IO_RESOURCE*)blob.constData();
	return r->IO_Header;
}

Obj_LogConf::~Obj_LogConf() {
	CmCheck(::CM_Free_Log_Conf_Handle(m_h));
}

Resources::iterator Resources::begin() {
	RES_DES h;
	RESOURCEID resId;
	CmCheck(::CM_Get_Next_Res_Des(&h, m_pLC->m_h, ResType_All, &resId, 0));
	iterator i;
	(i.m_resource = new Obj_HwResource)->m_h = h;
	i.m_resource->m_type = resId;
	return i;
}

Resources::iterator& Resources::iterator::operator++() {
	RES_DES h;
	RESOURCEID resId;
	CONFIGRET r = ::CM_Get_Next_Res_Des(&h, m_resource->m_h, ResType_All, &resId, 0);
	switch (r)
	{
	case CR_SUCCESS:
		(m_resource = new Obj_HwResource)->m_h = h;
		m_resource->m_type = resId;
		break;
	case CR_NO_MORE_RES_DES:
		m_resource = nullptr;
		break;
	default:
		CmCheck(r);
	}
	return _self;
}

const SP_DRVINFO_DETAIL_DATA& DiDriverInfo::GetDetailData() {
	if (m_blobDetailData.Size == 0) {
		DWORD dw;
		Win32Check(::SetupDiGetDriverInfoDetail(m_pDCD->m_h, 0, &m_drvinfo_data, 0, 0, &dw), ERROR_INSUFFICIENT_BUFFER);
		m_blobDetailData.Size = dw;
		SP_DRVINFO_DETAIL_DATA *detData = (SP_DRVINFO_DETAIL_DATA*)m_blobDetailData.data();
		detData->cbSize = sizeof(SP_DRVINFO_DETAIL_DATA);
		Win32Check(::SetupDiGetDriverInfoDetail(m_pDCD->m_h, 0, &m_drvinfo_data, detData, m_blobDetailData.Size, 0));
	}
	return *(SP_DRVINFO_DETAIL_DATA*)m_blobDetailData.data();
}

DiDrivers::DiDrivers(const GUID *pClassGuid) {
	HDEVINFO h = ::SetupDiCreateDeviceInfoList(pClassGuid, 0);
	SetupDiCheck(h);
	(m_p = new Obj_DiClassDevs)->m_h = h;

	SP_DEVINSTALL_PARAMS devInstallParam = { sizeof devInstallParam };
	devInstallParam.FlagsEx |= DI_FLAGSEX_ALLOWEXCLUDEDDRVS; //DI_FLAGSEX_INSTALLEDDRIVER; //| 
	Win32Check(::SetupDiSetDeviceInstallParams(h, 0, &devInstallParam));

	Win32Check(::SetupDiBuildDriverInfoList(h, 0, SPDIT_CLASSDRIVER));
}

DiDrivers::~DiDrivers() {
	Win32Check(::SetupDiDestroyDriverInfoList(m_p->m_h, 0, SPDIT_CLASSDRIVER));
}

void DiDrivers::iterator::ProbeCurrentDriverInfo() {
	if (m_idx != -1) {
		SP_DRVINFO_DATA diData = { sizeof diData };		
		BOOL b = ::SetupDiEnumDriverInfo(m_p->m_h, 0, SPDIT_CLASSDRIVER , m_idx, &diData);
		if (b)
			m_di = DiDriverInfo(m_p, diData);
		else {
			Win32Check(GetLastError()==ERROR_NO_MORE_ITEMS);
			m_idx = -1;
		}
	}
}

DiDrivers::iterator& DiDrivers::iterator::operator++() {
	m_idx++;
	ProbeCurrentDriverInfo();
	return _self;
}

DiDriverInfo *DiDrivers::iterator::operator->() {
	ProbeCurrentDriverInfo();
	if (m_idx == -1)
		Throw(E_FAIL);
	return &m_di;
}

} // Ext::


