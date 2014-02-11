/*######     Copyright (c) 1997-2013 Ufasoft  http://ufasoft.com  mailto:support@ufasoft.com,  Sergey Pavlov  mailto:dev@ufasoft.com #######################################
#                                                                                                                                                                          #
# This program is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation;  #
# either version 3, or (at your option) any later version. This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the      #
# implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details. You should have received a copy of the GNU #
# General Public License along with this program; If not, see <http://www.gnu.org/licenses/>                                                                               #
##########################################################################################################################################################################*/

#ifndef _LIBEXT_EXT_H
#define _LIBEXT_EXT_H

#pragma once

//#pragma warning(disable: 4786)  //!!! name truncated



#include <el/libext.h>



#if UCFG_USING_DEFAULT_NS
using namespace std;
using namespace Ext;

using _TR1_NAME(hash);

#endif

//!!!R #define _POST_CONFIG

/*!!!R
#if UCFG_EXTENDED
#	include <el/inc/inc_configs.h>
#endif
*/



#endif // _LIBEXT_EXT_H

