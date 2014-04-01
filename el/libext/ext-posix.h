#pragma once

#define _EXT_POSIX_H


typedef char *caddr_t;
typedef unsigned char uchar;
typedef unsigned char u_char;

#define HAVE_U_INT8_T 1
#define HAVE_INT8_T 1

#ifndef _STDINT

typedef int int32_t;
typedef signed __int8 int8_t;
typedef unsigned __int8 uint8_t;

typedef __int16 int16_t;
typedef unsigned __int16 uint16_t;

typedef unsigned __int32 uint32_t;

typedef unsigned __int64 uint64_t;
typedef __int64 int64_t;

#endif

typedef unsigned __int8 u_int8_t;
typedef unsigned __int16 u_int16_t;
typedef unsigned __int32 u_int32_t;
typedef unsigned __int64 u_int64_t;

typedef unsigned short ushort;
typedef unsigned __int16 n_short;		// short as received from the net 

typedef unsigned int uint;
typedef unsigned long ulong;
typedef unsigned __int32 n_long;		// long as received from the net 
typedef	unsigned __int32 n_time;		// ms since 00:00 GMT, byte rev 
typedef unsigned long	paddr_t;

typedef	u_int64_t	u_quad_t;	// quads 
typedef	int64_t		quad_t;

typedef int pid_t;

#define	__BIT_TYPES_DEFINED__


#ifndef _DEV_T_DEFINED
	typedef unsigned int _dev_t;            /* device code */
	#define _DEV_T_DEFINED
#endif



#ifndef _INO_T_DEFINED
	typedef unsigned short _ino_t;          /* i-node number (not used on DOS) */
	#define _INO_T_DEFINED
	typedef _ino_t ino_t;
#else

#	if     __STDC__
	/* Non-ANSI name for compatibility */
	typedef unsigned short ino_t;
#	endif

#endif

#if !defined(_OFF_T_DEFINED) && !UCFG_WDM
	typedef long _off_t;                    /* file offset value */
	#define _OFF_T_DEFINED
	typedef _off_t off_t;
#endif

#ifdef	_BSD_CLOCKID_T_
typedef	_BSD_CLOCKID_T_	clockid_t;
#undef	_BSD_CLOCKID_T_
#endif

typedef unsigned __int8 u8;
typedef unsigned __int16 u16;
typedef unsigned __int32 u32;


typedef u32 dma_addr_t;

/* Non-ANSI name for compatibility */
#if !UCFG_EXT_C  && !defined(_OFF_T_DEFINED)//!!!
	typedef __int64 off_t;
#	define _OFF_T_DEFINED
#endif

#if UCFG_WIN32
#	define SHUT_RD   	SD_RECEIVE
#	define SHUT_WR		SD_SEND
#	define SHUT_RDWR	SD_BOTH



_inline void bzero(void *p, size_t size) { memset(p, 0, size); }
_inline void bcopy(const void *s1, void *s2, size_t n) { memmove(s2, s1, n); }

#endif

__BEGIN_DECLS

#if	UCFG_WDM
	void * __cdecl _alloca(size_t);
#	define alloca _alloca
	int __cdecl atoi(const char *str);
#endif  // UCFG_WDM


#if UCFG_MSC_VERSION && UCFG_MSC_VERSION < 1800
#	define _DENORM    (-2)
#	define _FINITE    (-1)
#	define _INFCODE   1
#	define _NANCODE   2

#	define FP_INFINITE  _INFCODE
#	define FP_NAN       _NANCODE
#	define FP_NORMAL    _FINITE
#	define FP_SUBNORMAL _DENORM
#	define FP_ZERO      0


#endif // UCFG_MSC_VERSION && UCFG_MSC_VERSION < 1800




#if UCFG_WIN32_FULL || UCFG_WDM

typedef int useconds_t;

void __cdecl usleep(useconds_t useconds);
void __cdecl delay(int us);
__inline void __cdecl udelay(int us) { delay(us); }


#	ifdef WIN32

#		define O_NONBLOCK	  04000

#		define F_GETFL		3	/* get file->f_flags */
#		define F_SETFL		4	/* set file->f_flags */

	__inline int fsync(int fd) { return _commit(fd); }
	__inline int fdatasync(int fd) { return _commit(fd); }

#	if UCFG_USE_POSIX
	__inline int fcntl(int fd, int f, int v) { return -1; } //!!!NOTIMPL
#	endif


#if !UCFG_EXT_C && !UCFG_WCE
__inline int isatty(int fd) {
	return _isatty(fd);
}
#endif

char * AFXAPI strsep(char **stringp, const char *delim);

#	endif  // WIN32
#endif  // _WIN32


#ifndef SIGINT
#	define SIGINT          2       /* interrupt */
#endif

#ifndef SIGBREAK
#	define SIGBREAK        21      /* Ctrl-Break sequence */
#endif

#ifndef SIGTERM
#	define SIGTERM         15      /* Software termination signal from kill */
#endif



struct option
{
# if (defined __STDC__ && __STDC__) || defined __cplusplus
	const char *name;
# else
	char *name;
# endif
	/* has_arg can't be an enum because some compilers complain about
	type mismatches in all the code that assumes it is an int.  */
	int has_arg;
	int *flag;
	int val;
};

#define no_argument		0
#define required_argument	1
#define optional_argument	2

size_t AFXAPI strlcpy(char *dst, const char *src, size_t siz);
size_t __cdecl strlcat(char *dst, const char *src, size_t siz);
int AFXAPI getopt(int nargc, char * const *nargv, const char *ostr);
int AFXAPI getopt_long (int argc, char *const *argv, const char *options, const struct option *long_options, int *opt_index);
int __cdecl ftruncate(int fd, off_t length);
int __stdcall gettimeofday(struct timeval *tp, void *);

#ifdef extern
#	undef EXT_DATA
#	define EXT_DATA
#endif

extern EXT_DATA char	*optarg;
extern EXT_DATA int optind, optopt, optreset, opterr;


_inline int getuid() { return 0; } //!!!
_inline int geteuid() { return 0; } //!!!
_inline void alarm(int a) {} //!!!

__int32 _cdecl gmt2local(time_t);
#define gmt2local_h		// to prevend call-conv conflict

#if UCFG_WCE

#define EFAULT          14

__time64_t __cdecl _mktime64(struct tm *);

errno_t __cdecl _wsplitpath_s(const wchar_t *, _Out_opt_z_cap_(driveSizeInCharacters) wchar_t *, size_t driveSizeInCharacters, _Out_opt_z_cap_(dirSizeInCharacters) wchar_t *,
									  size_t dirSizeInCharacters, _Out_opt_z_cap_(nameSizeInCharacters) wchar_t *, size_t nameSizeInCharacters, _Out_opt_z_cap_(extSizeInCharacters) wchar_t *, size_t extSizeInCharacters);

#define _tsplitpath_s   _wsplitpath_s

int *ErrnoRef();

#define errno (*ErrnoRef())

void _cdecl abort();
int remove(const char *path);
#endif // UCFG_WCE

struct tm;
struct tm * __cdecl gmtime_r(const time_t *timer, struct tm *result);

__END_DECLS

enum { STDIN_FILENO = 0, STDOUT_FILENO = 1, STDERR_FILENO = 2 };


#define vsnprintf _vsnprintf

#define wcscasecmp _wcsicmp

#define copysign _copysign

#ifndef EWOULDBLOCK
#	define EWOULDBLOCK             WSAEWOULDBLOCK
#	define EINPROGRESS             WSAEINPROGRESS
#	define EALREADY                WSAEALREADY
#	define ENOTSOCK                WSAENOTSOCK
#	define EDESTADDRREQ            WSAEDESTADDRREQ
#	define EMSGSIZE                WSAEMSGSIZE
#	define EPROTOTYPE              WSAEPROTOTYPE
#	define ENOPROTOOPT             WSAENOPROTOOPT
#	define EPROTONOSUPPORT         WSAEPROTONOSUPPORT
#	define ESOCKTNOSUPPORT         WSAESOCKTNOSUPPORT
#	define EOPNOTSUPP              WSAEOPNOTSUPP
#	define EPFNOSUPPORT            WSAEPFNOSUPPORT
#	define EAFNOSUPPORT            WSAEAFNOSUPPORT
#	define EADDRINUSE              WSAEADDRINUSE
#	define EADDRNOTAVAIL           WSAEADDRNOTAVAIL
#	define ENETDOWN                WSAENETDOWN
#	define ENETUNREACH             WSAENETUNREACH
#	define ENETRESET               WSAENETRESET
#	define ECONNABORTED            WSAECONNABORTED
#	define ECONNRESET              WSAECONNRESET
#	define ENOBUFS                 WSAENOBUFS
#	define EISCONN                 WSAEISCONN
#	define ENOTCONN                WSAENOTCONN
#	define ETOOMANYREFS            WSAETOOMANYREFS
#	define ETIMEDOUT               WSAETIMEDOUT
#	define ECONNREFUSED            WSAECONNREFUSED
#	define ELOOP                   WSAELOOP
#	define EHOSTDOWN               WSAEHOSTDOWN
#	define EHOSTUNREACH            WSAEHOSTUNREACH
#	define EPROCLIM                WSAEPROCLIM
#	define EUSERS                  WSAEUSERS
#	define EDQUOT                  WSAEDQUOT
#	define ESTALE                  WSAESTALE
#	define EREMOTE                 WSAEREMOTE
#endif	// EWOULDBLOCK

#ifndef ESHUTDOWN
#	define ESHUTDOWN               WSAESHUTDOWN
#endif

//duplicate sys/stat.h for __STDC__
#define S_IFMT   _S_IFMT
#define S_IFDIR  _S_IFDIR
#define S_IFCHR  _S_IFCHR
#define S_IFREG  _S_IFREG
#define S_IREAD  _S_IREAD
#define S_IWRITE _S_IWRITE
#define S_IEXEC  _S_IEXEC


typedef __int64 off64_t;
typedef int mode_t;

#define _LARGEFILE64_SOURCE 1
#define _LFS64_LARGEFILE 1

#define HAVE_VSWPRINTF 1
#define HAVE_STRLCAT 1
