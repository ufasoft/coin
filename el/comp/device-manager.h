/*######     Copyright (c) 1997-2013 Ufasoft  http://ufasoft.com  mailto:support@ufasoft.com,  Sergey Pavlov  mailto:dev@ufasoft.com #######################################
#                                                                                                                                                                          #
# This program is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation;  #
# either version 3, or (at your option) any later version. This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the      #
# implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details. You should have received a copy of the GNU #
# General Public License along with this program; If not, see <http://www.gnu.org/licenses/>                                                                               #
##########################################################################################################################################################################*/

#pragma once

#if UCFG_USE_POSIX
#	include <libudev.h>
#endif

#if UCFG_WIN32
#	include <el/win/setupdi.h>
#endif

namespace Ext {

class DeviceManager;

class SystemDevice {
	typedef SystemDevice class_type;
public:
	DeviceManager& Dm;
	String Path;

	SystemDevice(DeviceManager& dm)
		:	Dm(dm)
	{}

#if UCFG_USE_POSIX
	udev_device *m_h;

	SystemDevice(const SystemDevice& dev)
		:	Dm(dev.Dm)
		,	m_h(::udev_device_ref(dev.m_h))
		,	Path(dev.Path)
	{
	}

	~SystemDevice() {
		::udev_device_unref(m_h);
	}

	SystemDevice& operator=(const SystemDevice& dev) {
		::udev_device_unref(exchange(m_h, ::udev_device_ref(dev.m_h)));
		return *this;
	}

	String operator[](RCString sysattr) const {
		return ::udev_device_get_sysattr_value(m_h, sysattr);
	}

	String get_SysName() const { return ::udev_device_get_sysname(m_h); }
	DEFPROP_GET(String, SysName);

	String get_SysPath() const { return ::udev_device_get_syspath(m_h); }
	DEFPROP_GET(String, SysPath);

	String get_DevNode() const { return ::udev_device_get_devnode(m_h); }
	DEFPROP_GET(String, DevNode);

	String get_DevType() const { return ::udev_device_get_devtype(m_h); }
	DEFPROP_GET(String, DevType);

	String get_SubSystem() const { return ::udev_device_get_subsystem(m_h); }
	DEFPROP_GET(String, SubSystem);



#endif // UCFG_USE_POSIX

private:
#if UCFG_WIN32
	DiDeviceInfo Di;
#else

#endif

	friend class DeviceManager;
	friend class SystemDevices;
};

class DeviceManager : NonCopiable {
public:
#if UCFG_USE_POSIX
	DeviceManager()
		:	m_udev(::udev_new())
	{}

	~DeviceManager() {
		::udev_unref(m_udev);
	}
	
	udev *m_udev;
#endif // UCFG_USE_POSIX
};

class SystemDevices : NonCopiable {
	DeviceManager& Dm;
public:
#if UCFG_USE_POSIX
	udev_enumerate *m_enum;
#endif


#if UCFG_USE_POSIX

	class iterator {
	public:
		DeviceManager& Dm;

		udev_list_entry *m_le;

		iterator(DeviceManager& dm, udev_list_entry *le = 0)
			:	Dm(dm)
			,	m_le(le)
		{}

		iterator& operator++() {
			m_le = ::udev_list_entry_get_next(m_le);
			return *this;
		}

		bool operator==(const iterator& it) const { return m_le == it.m_le; }

		bool operator!=(const iterator& it) const { return !operator==(it); }

        SystemDevice operator*() {
			SystemDevice r(Dm);
			const char *path = ::udev_list_entry_get_name(m_le);
			r.Path = path;
			r.m_h = ::udev_device_new_from_syspath(Dm.m_udev, path);
			return r;
		}
	};

    iterator begin() {
		return iterator(Dm, ::udev_enumerate_get_list_entry(m_enum));
	}

    iterator end() { return iterator(Dm); }

    void AddMatchSysname(RCString sysname) {
		NegCCheck(::udev_enumerate_add_match_sysname(m_enum, sysname));
	}

    void AddMatchSubSystem(RCString subsystem) {
		NegCCheck(::udev_enumerate_add_match_subsystem(m_enum, subsystem));
	}

	void AddMatchProperty(RCString name, RCString value) {
		NegCCheck(::udev_enumerate_add_match_property(m_enum, name, value));
	}

	void ScanDevices() {
		NegCCheck(::udev_enumerate_scan_devices(m_enum));
	}
	
#endif // UCFG_USE_POSIX

	SystemDevices(DeviceManager& dm)
		:	Dm(dm)
	{
#if UCFG_USE_POSIX
		m_enum = udev_enumerate_new(Dm.m_udev);
#endif
	}

	~SystemDevices() {
#if UCFG_USE_POSIX
		::udev_enumerate_unref(m_enum);
#endif
	}


};



} // Ext::

