/*######     Copyright (c) 1997-2013 Ufasoft  http://ufasoft.com  mailto:support@ufasoft.com,  Sergey Pavlov  mailto:dev@ufasoft.com #######################################
#                                                                                                                                                                          #
# This program is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation;  #
# either version 3, or (at your option) any later version. This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the      #
# implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details. You should have received a copy of the GNU #
# General Public License along with this program; If not, see <http://www.gnu.org/licenses/>                                                                               #
##########################################################################################################################################################################*/

#include <el/ext.h>

#include "ext-openssl.h"
#include "hash.h"

#include <openssl/ripemd.h>
#include <openssl/sha.h>


using namespace Ext;

namespace Ext { namespace Crypto {

hashval SHA1::ComputeHash(Stream& stm) {
	MemoryStream ms;
	stm.CopyTo(ms);
	return ComputeHash(ms);
}

hashval SHA1::ComputeHash(const ConstBuf& mb) {
	byte buf[20];
	SslCheck(::SHA1(mb.P, mb.Size, buf));
	return hashval(buf, sizeof buf);
}

hashval RIPEMD160::ComputeHash(Stream& stm) {
	MemoryStream ms;
	stm.CopyTo(ms);
	return ComputeHash(ms);
}

hashval RIPEMD160::ComputeHash(const ConstBuf& mb) {
	byte buf[20];
	SslCheck(::RIPEMD160(mb.P, mb.Size, buf));
	return hashval(buf, sizeof buf);
}



}} // Ext::Crypto


