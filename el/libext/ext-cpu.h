/*######     Copyright (c) 1997-2013 Ufasoft  http://ufasoft.com  mailto:support@ufasoft.com,  Sergey Pavlov  mailto:dev@ufasoft.com #######################################
#                                                                                                                                                                          #
# This program is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation;  #
# either version 3, or (at your option) any later version. This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the      #
# implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details. You should have received a copy of the GNU #
# General Public License along with this program; If not, see <http://www.gnu.org/licenses/>                                                                               #
##########################################################################################################################################################################*/

#pragma once

namespace Ext {

struct CpuVendor {
	char Name[13];

	CpuVendor() {
		ZeroStruct(Name);
	}
};

class CpuInfo {
	typedef CpuInfo class_type;
public:
#if UCFG_CPU_X86_X64

	union CpuidInfo {
		struct {
			int EAX, EBX, ECX, EDX;
		};
		int m_ar[4];
	};

	struct SFamilyModelStepping {
		int Family, Model, Stepping;
	};

	CpuidInfo Cpuid(int level) {
		CpuidInfo r;
		::Cpuid(r.m_ar, level);
		return r;
	}

	CpuVendor get_Vendor() {
		int a[4];
		::Cpuid(a, 0);
		CpuVendor r;
		*(int*)r.Name = a[1];
		*(int*)(r.Name+4) = a[3];
		*(int*)(r.Name+8) = a[2];
		return r;
	}
	DEFPROP_GET(CpuVendor, Vendor);

	bool get_HasTsc() {
		return Cpuid(1).EDX & 0x10;
	}
	DEFPROP_GET(bool, HasTsc);

	struct SFamilyModelStepping get_FamilyModelStepping() {
	    int v = Cpuid(1).EAX;
		SFamilyModelStepping r = { (v>>8) & 0xF, (v>>4) & 0xF, v & 0xF };
		if (r.Family == 0xF)
			r.Family += (v>>20) & 0xFF;
		if (r.Family >= 6)
			r.Model += (v>>12) & 0xF0;
		return r;
	}
	DEFPROP_GET(SFamilyModelStepping, FamilyModelStepping);

	bool get_ConstantTsc() {
		if (UInt32(Cpuid(0x80000000).EAX) >= 0x80000007UL &&
            (Cpuid(0x80000007).EDX & 0x100))
			return true;
		if (!strcmp(get_Vendor().Name, "GenuineIntel")) {
			SFamilyModelStepping fms = get_FamilyModelStepping();
			return 0xF==fms.Family && fms.Model==3 ||							// P4, Xeon have constant TSC
					6==fms.Family && fms.Model>=0xE && fms.Model<0x1A;			// Core [2][Duo] - constant TSC
																				// Atoms - variable TSC
		}
		return false;
	}
	DEFPROP_GET(bool, ConstantTsc);

	bool get_HasSse() {
		return Cpuid(1).EDX & 0x02000000;
	}
	DEFPROP_GET(bool, HasSse);

	bool get_HasSse2() {
		return Cpuid(1).EDX & 0x04000000;
	}
	DEFPROP_GET(bool, HasSse2);

#	if UCFG_FRAMEWORK && !defined(_CRTBLD)
	String get_Name() {
		if (UInt32(Cpuid(0x80000000).EAX) < 0x80000004UL)
			return "Unknown";
    	int ar[13];
    	ZeroStruct(ar);
		::Cpuid(ar, 0x80000002);
		::Cpuid(ar+4, 0x80000003);
		::Cpuid(ar+8, 0x80000004);
		return String((const char*)ar).Trim();
	}
	DEFPROP_GET(String, Name);
#	endif

#endif // UCFG_CPU_X86_X64
};


} // Ext::

