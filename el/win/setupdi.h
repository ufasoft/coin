/*######     Copyright (c) 1997-2013 Ufasoft  http://ufasoft.com  mailto:support@ufasoft.com,  Sergey Pavlov  mailto:dev@ufasoft.com #######################################
#                                                                                                                                                                          #
# This program is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation;  #
# either version 3, or (at your option) any later version. This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the      #
# implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details. You should have received a copy of the GNU #
# General Public License along with this program; If not, see <http://www.gnu.org/licenses/>                                                                               #
##########################################################################################################################################################################*/

#pragma once

#include <winuser.h>
#include <winreg.h>

#pragma warning(push)
#	pragma warning(disable: 4668) // SYMBOL is not defined as a preprocessor macro, replacing with '0' for '#if/#elif'
#	include <setupapi.h>
#pragma warning(pop)

#include <cguid.h>
#include <cfgmgr32.h>

namespace Ext {

class DiDrivers;

void SetupDiCheck(HDEVINFO h);

class Obj_DiClassDevs : public Object {
public:
	HDEVINFO m_h;

	~Obj_DiClassDevs();
};

class CClassInstallHeader : public SP_CLASSINSTALL_HEADER {
public:
	CClassInstallHeader(DI_FUNCTION dif) {
		cbSize = sizeof(SP_CLASSINSTALL_HEADER);
		InstallFunction = dif;
	}
};

class CPropChangeParams : public SP_PROPCHANGE_PARAMS {
public:
	CPropChangeParams() {
		ZeroStruct(_self);
		new(this) CClassInstallHeader(DIF_PROPERTYCHANGE);
	}
};

class Obj_LogConf : public Object {
public:
	LOG_CONF m_h;

	~Obj_LogConf();
};

class Obj_HwResource : public Object {
public:
	RES_DES m_h;
	RESOURCEID m_type;

	~Obj_HwResource();
};

class HwResource {
	Blob GetData();
public:
	ptr<Obj_HwResource> m_p;

	HwResource(ptr<Obj_HwResource> p = nullptr)
		:	m_p(p)
	{}

	RESOURCEID get_Type() { return m_p->m_type; }
	DEFPROP_GET(RESOURCEID,Type);

	IO_DES GetIo();
};

class Resources {
public:
	ptr<Obj_LogConf> m_pLC;

	class iterator {
	public:
		ptr<Obj_HwResource> m_resource;

		bool operator==(iterator i) { return m_resource == i.m_resource; }
		bool operator!=(iterator i) { return !operator==(i); }

		iterator& operator++();

		iterator operator++(int) {
			iterator r = _self;
			operator++();
			return r;
		}

		HwResource operator*()
		{
			return m_resource;
		}
	};

	iterator begin();

	iterator end() {
		return iterator();
	}
};

class LogConf {
public:
	ptr<Obj_LogConf> m_p;

	Resources get_Resources()
	{
		class Resources r;
		r.m_pLC = m_p;
		return r;
	}
	DEFPROP_GET(Resources,Resources);
};

class DiDeviceInfo {
	CRegistryValue GetRegistryProperty(DWORD prop);
	void SetRegistryProperty(DWORD prop, const CRegistryValue rv);

	vector<String> GetOptionalRegistryProperty(DWORD prop) {
		try {
			DBG_LOCAL_IGNORE(HRESULT_FROM_WIN32(ERROR_INVALID_DATA));
			return GetRegistryProperty(prop);
		} catch (RCExc e) {
			if (e.HResult == HRESULT_FROM_WIN32(ERROR_INVALID_DATA))
				return vector<String>();
			throw;
		}
	}

	void ChangeState(DWORD state);
public:
	ptr<Obj_DiClassDevs> m_p;
	SP_DEVINFO_DATA m_data;

	DiDeviceInfo() {
		m_data.cbSize = sizeof m_data;
	}

	String get_DeviceDesc() { return GetRegistryProperty(SPDRP_DEVICEDESC); }
	DEFPROP_GET(String, DeviceDesc);

	String get_FrienlyName() { return GetRegistryProperty(SPDRP_FRIENDLYNAME); }
	DEFPROP_GET(String, FrienlyName);

	String get_Manufacturer() { return GetRegistryProperty(SPDRP_MFG); }
	DEFPROP_GET(String, Manufacturer);	

	vector<String> get_CompatibleIDs() { return GetOptionalRegistryProperty(SPDRP_COMPATIBLEIDS); }
	DEFPROP_GET(vector<String>, CompatibleIDs);

	vector<String> get_HardwareIDs() { return GetOptionalRegistryProperty(SPDRP_HARDWAREID); }
	DEFPROP_GET(vector<String>, HardwareIDs);

	vector<String> get_LowerFilters() { return GetOptionalRegistryProperty(SPDRP_LOWERFILTERS); }
	void put_LowerFilters(const vector<String> ar) { SetRegistryProperty(SPDRP_LOWERFILTERS,ar); }
	DEFPROP(vector<String>, LowerFilters);

	vector<String> get_UpperFilters() { return GetOptionalRegistryProperty(SPDRP_UPPERFILTERS); }
	void put_UpperFilters(const vector<String> ar) { SetRegistryProperty(SPDRP_UPPERFILTERS,ar); }
	DEFPROP(vector<String>, UpperFilters);

	String get_Service() { return GetRegistryProperty(SPDRP_SERVICE); }
	DEFPROP_GET(String, Service);

	String get_DeviceInstanceID();
	DEFPROP_GET(String, DeviceInstanceID);

	ULONG get_Status();
	DEFPROP_GET(ULONG, Status);

	HKEY OpenRegKey();

	template <class T> void SetClassInstallParams(T& params) {
		Win32Check(::SetupDiSetClassInstallParams(m_p->m_h,&m_data,&params.ClassInstallHeader, sizeof params));
	}

	void Enable() {
		ChangeState(DICS_ENABLE);
	}

	void Disable() {
		ChangeState(DICS_DISABLE);
	}

	LogConf GetLogConf(ULONG ulFlags);

	LogConf get_AllocLogConf() { return GetLogConf(ALLOC_LOG_CONF); }
	DEFPROP_GET(LogConf,AllocLogConf);
};

class DiClassDevs {
public:
	DiClassDevs(DWORD flags = DIGCF_PRESENT | DIGCF_ALLCLASSES, Guid guidClass = GUID_NULL) {
		const GUID *pguid = guidClass==GUID_NULL ? 0 : &guidClass;
		HDEVINFO h = ::SetupDiGetClassDevs(pguid, 0, 0, flags);
		SetupDiCheck(h);
		(m_p = new Obj_DiClassDevs)->m_h = h;
	}

	class iterator {
	public:
		ptr<Obj_DiClassDevs> m_p;
		int m_idx;
		DiDeviceInfo m_di;

		bool operator==(iterator i) { return m_idx == i.m_idx; }
		bool operator!=(iterator i) { return m_idx != i.m_idx; }

		iterator& operator++();

		iterator operator++(int) {
			iterator r = _self;
			operator++();
			return r;
		}

		DiDeviceInfo *operator->();

		DiDeviceInfo operator*() {
			return *operator->();
		}

		void ProbeCurrentDeviceInfo();
	};

	iterator begin() {
		iterator i;
		i.m_p = m_p;
		i.m_idx = 0;
		i.ProbeCurrentDeviceInfo();
		return i;
	}

	iterator end() {
		iterator i;
		i.m_p = m_p;
		i.m_idx = -1;
		return i;
	}
private:
	ptr<Obj_DiClassDevs> m_p;
};


class DiDriverInfo {
public:
	SP_DRVINFO_DATA m_drvinfo_data;

	DiDriverInfo() {
	}

	DiDriverInfo(ptr<Obj_DiClassDevs> pDCD, const SP_DRVINFO_DATA& drvinfo_data)
		:	m_pDCD(pDCD)
		,	m_drvinfo_data(drvinfo_data)
	{}

	const SP_DRVINFO_DETAIL_DATA& GetDetailData();

	String get_Description() {
		return m_drvinfo_data.Description;
	}
	DEFPROP_GET(String, Description);

	String get_MfgName() {
		return m_drvinfo_data.MfgName;
	}
	DEFPROP_GET(String, MfgName);

	String get_InfFileName() {
		return GetDetailData().InfFileName;
	}
	DEFPROP_GET(String, InfFileName);
private:
	ptr<Obj_DiClassDevs> m_pDCD;	
	Blob m_blobDetailData;
};

class DiDrivers {
public:
	DiDrivers(const GUID *pClassGuid = 0);
	~DiDrivers();

	class iterator {
	public:
		ptr<Obj_DiClassDevs> m_p;
		int m_idx;
		DiDriverInfo m_di;

		bool operator==(iterator i) { return m_idx == i.m_idx; }
		bool operator!=(iterator i) { return m_idx != i.m_idx; }

		iterator& operator++();

		iterator operator++(int) {
			iterator r = _self;
			operator++();
			return r;
		}

		DiDriverInfo *operator->();

		DiDriverInfo operator*() {
			return *operator->();
		}

		void ProbeCurrentDriverInfo();
	};

	iterator begin() {
		iterator i;
		i.m_p = m_p;
		i.m_idx = 0;
		i.ProbeCurrentDriverInfo();
		return i;
	}

	iterator end() {
		iterator i;
		i.m_p = m_p;
		i.m_idx = -1;
		return i;
	}
private:
	ptr<Obj_DiClassDevs> m_p;
	SP_DEVINFO_DATA m_diData;
};

} // Ext::
