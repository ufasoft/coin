/*######     Copyright (c) 1997-2013 Ufasoft  http://ufasoft.com  mailto:support@ufasoft.com,  Sergey Pavlov  mailto:dev@ufasoft.com #######################################
#                                                                                                                                                                          #
# This program is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation;  #
# either version 3, or (at your option) any later version. This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the      #
# implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details. You should have received a copy of the GNU #
# General Public License along with this program; If not, see <http://www.gnu.org/licenses/>                                                                               #
##########################################################################################################################################################################*/

#pragma once

namespace Ext { namespace DB { namespace KV {

const int FILL_THRESHOLD_PERCENTS = 25;
const size_t MAX_KEYS_PER_PAGE = 1024;
const byte ENTRY_FLAG_BIGDATA = 1;

struct EntryDesc : Buf {
	ConstBuf Key() {
		return ConstBuf(P+1, P[0]);
	}

	UInt64 DataSize;
	ConstBuf LocalData;
	UInt32 PgNo;
	bool Overflowed;
};



EntryDesc GetEntryDesc(const PagePos& pp, byte keySize);
LiteEntry GetLiteEntry(const PagePos& pp, byte keySize);
pair<size_t, bool> GetDataEntrySize(size_t ksize, UInt64 dsize, size_t pageSize);	// <size, isBigData>

struct TableData {
	UInt32 RootPgNo;
	byte KeySize;
	byte Flags;
	byte Res0, Res1;

	TableData() {
		ZeroStruct(*this);
	}
};



}}} // Ext::DB::KV::
