/*######     Copyright (c) 1997-2013 Ufasoft  http://ufasoft.com  mailto:support@ufasoft.com,  Sergey Pavlov  mailto:dev@ufasoft.com #######################################
#                                                                                                                                                                          #
# This program is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation;  #
# either version 3, or (at your option) any later version. This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the      #
# implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details. You should have received a copy of the GNU #
# General Public License along with this program; If not, see <http://www.gnu.org/licenses/>                                                                               #
##########################################################################################################################################################################*/

#include <el/ext.h>

#include "sph-config.h"
#include <sphlib/sph_keccak.h>
#include <sphlib/sph_shavite.h>

#include "util.h"

#include "sph_metis.h"

namespace Coin {

HashValue MetisHash(const ConstBuf& cbuf) {
	sph_keccak512_context	 ctx_keccak;
	sph_shavite512_context	 ctx_shavite;
	sph_metis512_context	 ctx_metis;
    static unsigned char pblank[1];

       
    byte hash[3][64];

    sph_keccak512_init(&ctx_keccak);
    sph_keccak512 (&ctx_keccak, (!cbuf.Size ? pblank : cbuf.P), cbuf.Size);
    sph_keccak512_close(&ctx_keccak, static_cast<void*>(&hash[0]));
    
    sph_shavite512_init(&ctx_shavite);
    sph_shavite512(&ctx_shavite, static_cast<const void*>(&hash[0]), 64);
    sph_shavite512_close(&ctx_shavite, static_cast<void*>(&hash[1]));

	sph_metis512_init(&ctx_metis);
    sph_metis512(&ctx_metis, static_cast<const void*>(&hash[1]), 64);
    sph_metis512_close(&ctx_metis, static_cast<void*>(&hash[2]));

    return HashValue(&hash[2][0]);
}

} // Coin::

