AC_PREREQ([2.67])
AC_INIT(ufasoft_coin, 0.72, support@ufasoft.com,, http://ufasoft.com/coin)
AC_CANONICAL_SYSTEM
AM_INIT_AUTOMAKE([subdir-objects dist-xz])
AM_SILENT_RULES([yes])


AC_CONFIG_SRCDIR([coin/miner/miner.cpp])
AC_CONFIG_MACRO_DIR([m4])
AC_CONFIG_HEADER([config.h])

AC_PROG_CC
AC_PROG_CXX
AM_PROG_CC_C_O
AC_LANG([C++])


AC_PATH_PROG([COMPILER], [$CXX])
if ! test -x "${COMPILER}"; then
	AC_MSG_ERROR([No C++ compiler found. Please install a C++ compiler.])
fi

case "$CC" in
*clang*)
    CLANG=yes
    ;;
*)
    CLANG=no
    ;;
esac

AM_CONDITIONAL([X86_X64], [test "x$target_cpu" = "xi686" || test "x$target_cpu" = "xx86_64" ])


case "${host_os}" in
freebsd*)
	CPPFLAGS="$CPPFLAGS -I/usr/local/include -I/usr/local/include/libxml2"
	LDFLAGS="$LDFLAGS -L/usr/local/lib"
	;;
esac


AC_CHECK_LIB(pcre, pcre_compile,, AC_MSG_ERROR("Library PCRE not found: install libpcre3-dev / pcre-devel"))
AC_CHECK_LIB(pthread, pthread_create,, AC_MSG_ERROR("Library pthread not found"))
AC_CHECK_LIB(gmp, __gmpz_init, , have_gmp=false)

PKG_PROG_PKG_CONFIG()

PKG_CHECK_MODULES([LIBCURL], [libcurl >= 7.21], ,[AC_MSG_ERROR([Missing required libcurl-devel >= 7.21])])
AC_SUBST(LIBCURL_CFLAGS)
AC_SUBST(LIBCURL_LIBS)

PKG_CHECK_MODULES([JANSSON], [jansson >= 2.0], AC_DEFINE([HAVE_JANSSON], [1], [Defined to 1 if Jansson is present on the system]), [AC_MSG_ERROR([Missing required libjansson-dev / jansson-devel >= 2.0])])
AC_SUBST(JANSSON_CFLAGS)
AC_SUBST(JANSSON_LIBS)

PKG_CHECK_MODULES([SQLITE3], [sqlite3 >= 3.7], AC_DEFINE([HAVE_SQLITE3], [1], [Defined to 1 if Sqlite is present on the system]), have_sqlite=false)

AC_SUBST(SQLITE3_LIBS)

PKG_CHECK_MODULES([XML], [libxml-2.0 >= 2.8], ,have_libxml=false)
AC_SUBST(XML_CFLAGS)
AC_SUBST(XML_LIBS)

PKG_CHECK_MODULES([OPENSSL], [openssl >= 1.0],,have_openssl=false)
AC_SUBST(OPENSSL_CFLAGS)
AC_SUBST(OPENSSL_LIBS)

PKG_CHECK_MODULES([UDEV], [libudev >= 200], [AC_DEFINE([HAVE_LIBUDEV], [1], [Defined to 1 if libudev is present on the system]) libudev=yes],libudev=no)
AM_CONDITIONAL([HAVE_LIBUDEV], [test "x$libudev" != xno])
AC_SUBST(UDEV_LIBS)

AC_CHECK_LIB(rt, clock_gettime,, AC_MSG_ERROR("Library librt not found"))
AC_SEARCH_LIBS([dlopen], [dl dld], []			, [AC_MSG_ERROR([unable to find the dlopen() function])				])
AC_SEARCH_LIBS([iconv],  [iconv], []			, [AC_MSG_ERROR([unable to find the iconv() function])				])


if test "x$GCC" = xyes -a "x$CLANG" = xno; then
AC_MSG_CHECKING([for gcc])
AC_RUN_IFELSE([AC_LANG_PROGRAM([],[
        if( ! ( ( __GNUC__ > 4 ) || ( __GNUC__ == 4 ) && ( __GNUC_MINOR__ >= 6 ) ) ) return 1;])],
    [AC_MSG_RESULT(yes)],
	[AC_MSG_ERROR([requires gcc 4.6 or later])
]) # end AC_RUN_IFELSE
fi

AX_CHECK_COMPILE_FLAG([-std=c++1y], [CXXFLAGS="$CXXFLAGS -std=c++1y"], [CXXFLAGS="$CXXFLAGS -std=c++0x"])

AC_CHECK_FUNCS([pthread_setname_np])

OPENCL_LIBS="-lOpenCL"

case $target in
  amd64-*)
    have_x86_64=true
    ;;
  x86_64-*)
    have_x86_64=true
    ;;
  *)
    have_x86_64=false
    ;;
esac


if test "x$AMDAPPSDKROOT" != x; then
	if test "x$have_x86_64" = xtrue; then
		ARCH_DIR=x86_64
	else
		ARCH_DIR=x86
	fi
	OPENCL_FLAGS="-I$AMDAPPSDKROOT/include $OPENCL_FLAGS"
	OPENCL_LIBS="-L$AMDAPPSDKROOT/lib/$ARCH_DIR $OPENCL_LIBS"
fi

opencl="yes"

AC_ARG_ENABLE([opencl],
	[AC_HELP_STRING([--disable-opencl],[Override detection and disable building with opencl])],
	[opencl=$enableval]
	)
if test "x$opencl" != xno; then
	# Check for OpenCL (the long way needed on mingw32 due to calling conventions)
	AC_MSG_CHECKING([for OpenCL])
	SAVED_LIBS=$LIBS
	SAVED_CPPFLAGS=$CPPFLAGS
	LIBS="$LIBS $OPENCL_LIBS"
	CPPFLAGS="$CPPFLAGS $OPENCL_FLAGS"
	AC_LINK_IFELSE(
	[AC_LANG_PROGRAM([[
		#ifdef __APPLE_CC__
		#include <OpenCL/opencl.h>
		#else
		#include <CL/cl.h>
		#endif
	]],
	[[return clSetKernelArg(0, 0, 0, 0); ]])],
	[AC_MSG_RESULT(yes)
	AC_DEFINE([HAVE_OPENCL], [1], [Defined to 1 if OpenCL is present on the system.])
	found_opencl=1
	],
	[AC_MSG_RESULT(no)
	OPENCL_FLAGS=
	OPENCL_LIBS=
	found_opencl=0])
	LIBS=$SAVED_LIBS
	CPPFLAGS=$SAVED_CPPFLAGS
else
	OPENCL_FLAGS=""
	OPENCL_LIBS=""
fi

AM_CONDITIONAL([USE_AMDSDK], [test $found_opencl == 1])


AC_ARG_ENABLE([cuda], AS_HELP_STRING([--disable-cuda], [Disable CUDA GPU functionality]))
AM_CONDITIONAL([USE_CUDA], [test "x${enable_cuda}" != xno])

AM_CONDITIONAL([USE_CAL], false)


AS_IF([test "x${opencl}" != "x"], [
#	CPPFLAGS="${CPPFLAGS} -I${AMDAPPSDKROOT}/include"    
#if test x$target_cpu = xx86_64 ; then
#	LDFLAGS="${LDFLAGS} -L${AMDAPPSDKROOT}/lib/x86_64 -L${AMDAPPSDKROOT}/lib"
#else
#	LDFLAGS="${LDFLAGS} -L${AMDAPPSDKROOT}/lib/x86 -L${AMDAPPSDKROOT}/lib"
#fi
#	AC_CHECK_LIB(OpenCL, clGetPlatformInfo,, AC_MSG_ERROR("Library not found"))
#	AC_CHECK_LIB(aticalrt, calInit,, AC_MSG_ERROR("Library not found"))
#	AC_CHECK_LIB(aticalcl, calclCompile,, AC_MSG_ERROR("Library not found"))

	AC_CHECK_HEADER([cal.h],
		[
			AC_CHECK_PROG(CLISP, clisp, clisp, no)
			AM_CONDITIONAL([USE_CAL], [ test "$CLISP" != "no" ])
			if test "$CLISP" != "no"; then
				AC_DEFINE([UCFG_BITCOIN_USE_CAL], [1], [Defined to 1 if OpenCL is present on the system.])
			fi
		],
		, ${AMDAPPSDKROOT}/include)
],
[
	AC_DEFINE([UCFG_BITCOIN_USE_CAL], [0], [Defined to 1 if OpenCL is present on the system.])
]
)

AC_CHECK_PROG(JWASM, jwasm, jwasm, no)
AM_CONDITIONAL([HAVE_JWASM], [ test "$JWASM" = "jwasm" && ( test "x$target_cpu" = "xi686"  || test "x$target_cpu" = "xx86_64"  )  ])

if [ [ "x$target_cpu" = "xx86_64" ] ;] then
	JWASMFLAGS="-DX64=1 -10 -elf64"
else
	JWASMFLAGS=-elf
fi

AC_SUBST(JWASMFLAGS)

if [ [ "$JWASM" = "no" ]  && ( [ "x$target_cpu" = "xi686" ] || [ "x$target_cpu" = "xx86_64"  ] )  ;] then
	AC_DEFINE([UCFG_USE_MASM], [0], [Defined to 1 if some MASM-compatible assembler present])

	AC_MSG_WARN([Cannot find jwasm, building slow version. For better performance install it from here: http://www.japheth.de/JWasm.html])
fi

AC_SUBST(OPENCL_LIBS)
AC_SUBST(OPENCL_FLAGS)

if test x$have_gmp != xfalse && test x$have_openssl != xfalse && test x$have_libxml != xfalse && test x$have_sqlite != xfalse; then
	build_coind=true
fi

AM_CONDITIONAL([BUILD_COIND], [test x$build_coind = xtrue])


AC_OUTPUT(Makefile)

echo
echo
echo
echo "------------------------------------------------------------------------"
echo "$PACKAGE $VERSION"
echo "------------------------------------------------------------------------"
echo
echo
echo "Configuration Options Summary:"
echo



if test "x$opencl" != xno; then
	if test $found_opencl = 1; then
		echo "  OpenCL...............: FOUND. GPU mining support enabled"
	else
		echo "  OpenCL...............: NOT FOUND. GPU mining support DISABLED"
		if test "x$cpumining$bitforce$icarus$ztex$modminer" = xnonononono; then
			AC_MSG_ERROR([No mining configured in])
		fi
	fi
else
	echo "  OpenCL...............: Detection overrided. GPU mining support DISABLED"
fi

echo "  libudev..............: $libudev"


echo
if test x$build_coind = xtrue; then
	echo "coind build enabled"
else
	echo "coind build disabled, following libs are missing:"
	if test x$have_openssl = xfalse; then
		echo "  OpenSSL (libopenssl-dev)"
	fi
	if test x$have_gmp = xfalse; then
		echo "  GMP (libgmp)"
	fi
	if test x$have_libxml = xfalse; then
		echo "  libxml2-dev"
	fi
	if test x$have_sqlite = xfalse; then
		echo "  SQLite3"
	fi
fi
echo

echo
echo "Compilation............: make (gmake on FreeBSD)"
echo "  CPPFLAGS.............: $CPPFLAGS"
echo "  CFLAGS...............: $CFLAGS $LIBCURL_CFLAGS $JANSSON_CFLAGS"
echo "  CXXFLAGS.............. $CXXFLAGS"
echo "  LDFLAGS..............: $LDFLAGS $PTHREAD_FLAGS $USB_FLAGS"
echo "  LDADD................: $LIBCURL_LIBS$JANSSON_LIBS$OPENCL_LIBS$NCURSES_LIBS$PDCURSES_LIBS$UDEV_LIBS$USB_LIBS"
echo
echo "Installation...........: make install (as root if needed, with 'su' or 'sudo')"
echo "  prefix...............: $prefix"
echo






