#include <el/libext.h>

#pragma warning(disable: 4242 4244)

#define SPH_NO_64 0
#define SPH_BIG_ENDIAN 0		//!!!

#if UCFG_MSC_VERSION
#	define SPH_I386_GCC 0
#	define SPH_AMD64_GCC 0
#	define SPH_SPARCV9_GCC 0
#endif