#include <el/ext.h>

#include <sphlib/sph-config.h>
#include <sphlib/sph_keccak.h>
#include <sphlib/sph_shavite.h>

#include "util.h"

#include "sph_metis.h"

namespace Coin {

HashValue MetisHash(RCSpan cbuf) {
	sph_keccak512_context	 ctx_keccak;
	sph_shavite512_context	 ctx_shavite;
	sph_metis512_context	 ctx_metis;
    static unsigned char pblank[1];


    uint8_t hash[3][64];

    sph_keccak512_init(&ctx_keccak);
    sph_keccak512 (&ctx_keccak, (!cbuf.size() ? pblank : cbuf.data()), cbuf.size());
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

