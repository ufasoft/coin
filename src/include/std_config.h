#pragma once


#define _EXT

#define POSIX_MALLOC_THRESHOLD 5
#define PCRE_STATIC 1

#define __STDC__ 0 //!!!

#define LIBXML_STATIC

#define ZEXPORT __stdcall

#ifndef _M_ARM
#	define set_new_handler my_set_new_handler
#endif
//!!!? #define NOGDI

#define UCFG_FRAMEWORK 0
#define UCFG_USE_REGEX 0

#include <el/libext.h>

#ifndef _M_ARM
#	undef set_new_handler
#endif


#pragma warning(disable: 4146 4244 4291 4295 4310 4130)
#pragma warning(disable: 4232 4242 4018 4057 4090 4101 4152 4244 4245 4267 4505 4668 4700 4701 4716)


#undef DEBUG // to prevent verbose printfs
