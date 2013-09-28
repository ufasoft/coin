/*######     Copyright (c) 1997-2013 Ufasoft  http://ufasoft.com  mailto:support@ufasoft.com,  Sergey Pavlov  mailto:dev@ufasoft.com #######################################
#                                                                                                                                                                          #
# This program is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation;  #
# either version 3, or (at your option) any later version. This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the      #
# implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details. You should have received a copy of the GNU #
# General Public License along with this program; If not, see <http://www.gnu.org/licenses/>                                                                               #
##########################################################################################################################################################################*/

#pragma once


#ifdef __cplusplus

namespace Coin {

class IMiner {
public:
	virtual ~IMiner() {}
	virtual void Release() =0; 
	virtual void SetMainUrl(const char *url) =0;
	virtual void SetLogin(const char *login) =0;
	virtual void SetPassword(const char *password) =0;
	virtual void SetThreadCount(int n) =0;					// 0= disable, by default if GPU present ThreadCount == std::thread::hardware_concurrency() else 0
	virtual void SetIntensity(int v) =0;					// GPU usage intensity [-10..10], default 0
	virtual void SetHashAlgo(int a) =0;
	virtual void Start() =0;
	virtual void Stop() =0; 
	virtual double GetSpeed() =0;
};

} // Coin::

typedef Coin::IMiner* HCoinMiner;

extern "C" {

#else

typedef void* HCoinMiner;

#endif // __cplusplus

#define COIN_HASH_ALGO_SHA256 0
#define COIN_HASH_ALGO_SCrypt 1


HCoinMiner __cdecl CoinMiner_CreateObject();
void __cdecl CoinMiner_Release(HCoinMiner h);
void __cdecl CoinMiner_SetMainUrl(HCoinMiner h, const char *url);
void __cdecl CoinMiner_SetLogin(HCoinMiner h, const char *login);
void __cdecl CoinMiner_SetPassword(HCoinMiner h, const char *password);
void __cdecl CoinMiner_SetThreadCount(HCoinMiner h, int n);
void __cdecl CoinMiner_SetIntensity(HCoinMiner h, int n);
void __cdecl CoinMiner_SetHashAlgo(HCoinMiner h, int a);
void __cdecl CoinMiner_Start(HCoinMiner h);
void __cdecl CoinMiner_Stop(HCoinMiner h);
double __cdecl CoinMiner_GetSpeed(HCoinMiner h);

#ifdef __cplusplus
} // "C"
#endif

