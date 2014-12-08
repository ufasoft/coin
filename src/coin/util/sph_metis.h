#ifndef SPH_METIS_H__
#define SPH_METIS_H__

#include <stddef.h>
#include <sphlib/sph_types.h>

#ifdef __cplusplus
extern "C"{
#endif

#define SPH_SIZE_metis224   224

#define SPH_SIZE_metis256   256

#define SPH_SIZE_metis384   384

#define SPH_SIZE_metis512   512

typedef struct {
#ifndef DOXYGEN_IGNORE
	sph_u32 partial;
	unsigned partial_len;
	unsigned round_shift;
	sph_u32 S[36];
#if SPH_64
	sph_u64 bit_count;
#else
	sph_u32 bit_count_high, bit_count_low;
#endif
#endif
} sph_metis_context;

typedef sph_metis_context sph_metis224_context;

typedef sph_metis_context sph_metis256_context;

typedef sph_metis_context sph_metis384_context;

typedef sph_metis_context sph_metis512_context;

void sph_metis224_init(void *cc);

void sph_metis224(void *cc, const void *data, size_t len);

void sph_metis224_close(void *cc, void *dst);

void sph_metis224_addbits_and_close(
	void *cc, unsigned ub, unsigned n, void *dst);

void sph_metis256_init(void *cc);

void sph_metis256(void *cc, const void *data, size_t len);

void sph_metis256_close(void *cc, void *dst);

void sph_metis256_addbits_and_close(
	void *cc, unsigned ub, unsigned n, void *dst);

void sph_metis384_init(void *cc);

void sph_metis384(void *cc, const void *data, size_t len);

void sph_metis384_close(void *cc, void *dst);

void sph_metis384_addbits_and_close(
	void *cc, unsigned ub, unsigned n, void *dst);

void sph_metis512_init(void *cc);

void sph_metis512(void *cc, const void *data, size_t len);

void sph_metis512_close(void *cc, void *dst);

void sph_metis512_addbits_and_close(
	void *cc, unsigned ub, unsigned n, void *dst);

#ifdef __cplusplus
}
#endif	
	
#endif
