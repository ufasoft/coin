/*######     Copyright (c) 1997-2013 Ufasoft  http://ufasoft.com  mailto:support@ufasoft.com,  Sergey Pavlov  mailto:dev@ufasoft.com #######################################
#                                                                                                                                                                          #
# This program is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation;  #
# either version 3, or (at your option) any later version. This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the      #
# implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details. You should have received a copy of the GNU #
# General Public License along with this program; If not, see <http://www.gnu.org/licenses/>                                                                               #
##########################################################################################################################################################################*/

#pragma once

namespace Ext { namespace DB {

class BdbReader {
public:
	FileStream m_fs;

	BdbReader(RCString path);	
	bool Read(Blob& key, Blob& value);
protected:
	Blob m_curPage;
	int m_pgno;
	int m_idx;
	int m_lastPage;
	int m_entries;

	bool LoadNextPage(int pgno);
};



}} // Ext::DB::
