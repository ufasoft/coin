/*######     Copyright (c) 1997-2014 Ufasoft  http://ufasoft.com  mailto:support@ufasoft.com,  Sergey Pavlov  mailto:dev@ufasoft.com #######################################
#                                                                                                                                                                          #
# This program is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation;  #
# either version 3, or (at your option) any later version. This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the      #
# implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details. You should have received a copy of the GNU #
# General Public License along with this program; If not, see <http://www.gnu.org/licenses/>                                                                               #
##########################################################################################################################################################################*/

#define _USRDLL

#define UCFG_USE_RTTI 1

#define UCFG_EXPORT_COIN

#if defined(UCFG_CUSTOM) && UCFG_CUSTOM || !defined(_WIN32)
#	define UCFG_COIN_GENERATE 0
#endif

#define VER_FILEDESCRIPTION_STR "Coin Engine"
#define VER_INTERNALNAME_STR "CoinEng"
#define VER_ORIGINALFILENAME_STR "coineng.dll"


