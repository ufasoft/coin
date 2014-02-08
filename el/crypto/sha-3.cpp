/*######     Copyright (c) 1997-2013 Ufasoft  http://ufasoft.com  mailto:support@ufasoft.com,  Sergey Pavlov  mailto:dev@ufasoft.com #######################################
#                                                                                                                                                                          #
# This program is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation;  #
# either version 3, or (at your option) any later version. This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the      #
# implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details. You should have received a copy of the GNU #
# General Public License along with this program; If not, see <http://www.gnu.org/licenses/>                                                                               #
##########################################################################################################################################################################*/

#include <el/ext.h>

#include "hash.h"

#if UCFG_IMP_SHA3=='S'
#	include "sph-config.h"			//!!!
#	include <sphlib/sph_keccak.h>
#endif


using namespace Ext;

namespace Ext { namespace Crypto {

hashval SHA3<256>::ComputeHash(const ConstBuf& mb) {
#if UCFG_IMP_SHA3=='S'
	UInt32 hash[8];

    sph_keccak256_context ctx;
    sph_keccak256_init(&ctx);
    sph_keccak256(&ctx, mb.P, mb.Size);
    sph_keccak256_close(&ctx, hash);
	return hashval((const byte*)hash, sizeof hash);
#else
	Throw(E_NOTIMPL);
#endif
}

hashval SHA3<256>::ComputeHash(Stream& stm) {
	MemoryStream ms;
	stm.CopyTo(ms);
	return ComputeHash(ms);
}




}} // Ext::Crypto::


