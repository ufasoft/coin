/*######     Copyright (c) 1997-2013 Ufasoft  http://ufasoft.com  mailto:support@ufasoft.com,  Sergey Pavlov  mailto:dev@ufasoft.com #######################################
#                                                                                                                                                                          #
# This program is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation;  #
# either version 3, or (at your option) any later version. This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the      #
# implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details. You should have received a copy of the GNU #
# General Public License along with this program; If not, see <http://www.gnu.org/licenses/>                                                                               #
##########################################################################################################################################################################*/

#pragma once

#include EXT_HEADER_DYNAMIC_BITSET

namespace Ext { namespace Crypto {
using namespace std;

class BloomFilter {
public:
 	dynamic_bitset<byte> Bitset;
	int HashNum;

//	int N;

	BloomFilter()
		:	HashNum(1)
	{}

	virtual ~BloomFilter() {}

	bool Contains(const ConstBuf& key) const;
	void Insert(const ConstBuf& key);
protected:
	virtual size_t Hash(const ConstBuf& cbuf, int n) const =0;

};



}} // Ext::Crypto::

