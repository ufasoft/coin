#ifndef _AFXDLL
#	ifdef _WIN32
#		define UCFG_COM 1
#		define UCFG_OLE 1
#	endif

#	define UCFG_GUI 0
#	define UCFG_EXTENDED 0
#	define UCFG_XML 0
#	define UCFG_WND 0
#	define UCFG_STLSOFT 0
#	define UCFG_USE_PCRE 0
#	define UCFG_LIB_DECLS 0
#	define UCFG_REDEFINE_MAIN 0
#endif

#if !defined(UCFG_CUSTOM) || !UCFG_CUSTOM
#	define CUDA_FORCE_API_VERSION 3010		//!!!
#endif

#define UCFG_COIN_LIB_DECLS 0

#ifndef _WIN32
#	define UCFG_BITCOIN_SOLO_MINING 0
#	define UCFG_BITCOIN_SOLIDCOIN 0
#endif

#define UCFG_BITCOIN_TRACE 1

#ifdef _DEBUG
#	define UCFG_BITCOIN_USERAGENT_INFO 1 //!!!D
#else
#	define UCFG_BITCOIN_USERAGENT_INFO 0
#endif

#define VER_FILEDESCRIPTION_STR "coin-miner"
#define VER_INTERNALNAME_STR "coin-miner"
#define VER_ORIGINALFILENAME_STR "coin-miner.exe"

#define VER_PRODUCTNAME_STR "xCoin Miner"


#define UCFG_CRASHDUMP_COMPRESS 0

#if defined(_AFXDLL) || !defined(_WIN32)
#	define UCFG_BITCOIN_USE_RESOURCE 0
#else
#	define UCFG_BITCOIN_USE_RESOURCE 1
#endif

#ifndef UCFG_COIN_MINER_CHECK_EXE_NAME
#	define UCFG_COIN_MINER_CHECK_EXE_NAME 1
#endif


