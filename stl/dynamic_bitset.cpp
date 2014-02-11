/*######     Copyright (c) 1997-2013 Ufasoft  http://ufasoft.com  mailto:support@ufasoft.com,  Sergey Pavlov  mailto:dev@ufasoft.com #######################################
#                                                                                                                                                                          #
# This program is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation;  #
# either version 3, or (at your option) any later version. This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the      #
# implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details. You should have received a copy of the GNU #
# General Public License along with this program; If not, see <http://www.gnu.org/licenses/>                                                                               #
##########################################################################################################################################################################*/

#include <el/ext.h>

#include "dynamic_bitset"

namespace ExtSTL {
using namespace Ext;

void dynamic_bitsetBase::replace(size_type pos, bool val) {
	byte& bref = ByteRef(pos);
	bref = bref & ~(1 << (pos & 7)) | (byte(val) << (pos & 7));
}

void dynamic_bitsetBase::reset(size_type pos) {
	ByteRef(pos) &= ~(1 << (pos & 7));
}

void dynamic_bitsetBase::flip(size_type pos) {
	ByteRef(pos) ^= 1 << (pos & 7);
}


}  // ExtSTL::

