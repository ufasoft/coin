#define _USRDLL

#define UCFG_USE_RTTI 1

#define UCFG_TRC 1

#ifdef _AFXDLL
#	define UCFG_USE_TOR UCFG_WIN32_FULL
#else
#	define UCFG_USE_TOR 0
#endif

#define UCFG_EXPORT_COIN

#if defined(UCFG_CUSTOM) && UCFG_CUSTOM || !defined(_WIN32)
#	define UCFG_COIN_GENERATE 0
#endif

#define VER_FILEDESCRIPTION_STR "Coin Engine"
#define VER_INTERNALNAME_STR "Coin"
#define VER_ORIGINALFILENAME_STR "coineng.dll"


