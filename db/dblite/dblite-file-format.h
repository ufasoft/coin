/*######     Copyright (c) 1997-2013 Ufasoft  http://ufasoft.com  mailto:support@ufasoft.com,  Sergey Pavlov  mailto:dev@ufasoft.com #######################################
#                                                                                                                                                                          #
# This program is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation;  #
# either version 3, or (at your option) any later version. This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the      #
# implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details. You should have received a copy of the GNU #
# General Public License along with this program; If not, see <http://www.gnu.org/licenses/>                                                                               #
##########################################################################################################################################################################*/

#pragma once

namespace Ext { namespace DB { namespace KV {

#define UDB_MAGIC "Ufasoft DB"
const UInt32 UDB_VERSION = 0x040000;

struct TxRecord {
	Int64 Id;
	BeUInt32 MainDbPage;
	UInt32 _res;
};

struct DbHeader {
	char Magic[12];					// 0      UDB_MAGIC
	BeUInt32 Version;				// 4

	BeUInt16 PageSize;				// 16
	UInt16 _res1;					// 18
	UInt32 _res2;					// 20
	BeUInt32 ChangeCounter;			// 24		some fielads are compatible with SQLite3 file format
	BeUInt32 PageCount;				// 28
	BeUInt32 FreePagePoolList;		// 32
	BeUInt32 FreePageCount;			// 36
	UInt64 LastTransactionId;		// 40
	
	char AppName[12];				// 48
	BeUInt32 UserVersion;				// 60

	char FrontEndName[12];			// 64 could be SQLite or OO
	BeUInt32 FrontEndVersion;		// 76
		

	TxRecord LastTxes[11];			// 80

	BeUInt32 FreePages[32];			// 256
};



}}} // Ext::DB::KV::
