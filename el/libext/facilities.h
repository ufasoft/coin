/*######     Copyright (c) 1997-2013 Ufasoft  http://ufasoft.com  mailto:support@ufasoft.com,  Sergey Pavlov  mailto:dev@ufasoft.com #######################################
#                                                                                                                                                                          #
# This program is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation;  #
# either version 3, or (at your option) any later version. This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the      #
# implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details. You should have received a copy of the GNU #
# General Public License along with this program; If not, see <http://www.gnu.org/licenses/>                                                                               #
##########################################################################################################################################################################*/

#pragma once

enum Facility {
		FACILITY_SIMPLE_MAPI= 0x800,
		FACILITY_CFGMGR    	= 0x801,
		FACILITY_C 			= 0x802, //!!!
		FACILITY_WIFI 		= 0x803,
		FACILITY_LINE_NUMBER= 0x804,
		FACILITY_ELFIO      = 0x805,
		FACILITY_SQLITE     = 0x806,
		FACILITY_UNKNOWN    = 0x807,
		FACILITY_LIBXML     = 0x808,
		FACILITY_PCRE       = 0x809,
		FACILITY_GSL		= 0x80A,
		FACILITY_OS			= 0x80B,
			
		FACILITY_EXT  		= 0x80C,
		FACILITY_WIFI_PRISM = 0x80D,
		FACILITY_WIFI_ATHEROS = 0x80E,
		FACILITY_JSON_RPC	= 0x80F,
		FACILITY_MDB		= 0x810,
			
		FACILITY_EXT_DRIVER = 0x816,
		FACILITY_LISP  		= 0x817,
		FACILITY_SNIF  		= 0x818,
		FACILITY_GPU_CAL  	= 0x819,
		FACILITY_GPU_ADL  	= 0x81A,
		FACILITY_CURL  		= 0x81B,
		FACILITY_JNI  		= 0x81C,
		FACILITY_OPENCL  	= 0x81D,
		FACILITY_GPU_CU  	= 0x81E,
		FACILITY_GPU_CUDA  	= 0x81F,
		FACILITY_OPENSSL  	= 0x820,
		FACILITY_COIN  		= 0x821,
};


