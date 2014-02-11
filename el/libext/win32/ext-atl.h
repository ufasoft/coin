/*######     Copyright (c) 1997-2013 Ufasoft  http://ufasoft.com  mailto:support@ufasoft.com,  Sergey Pavlov  mailto:dev@ufasoft.com #######################################
#                                                                                                                                                                          #
# This program is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation;  #
# either version 3, or (at your option) any later version. This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the      #
# implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details. You should have received a copy of the GNU #
# General Public License along with this program; If not, see <http://www.gnu.org/licenses/>                                                                               #
##########################################################################################################################################################################*/

#pragma once

namespace ATL {

#if UCFG_COM_IMPLOBJ

typedef  Ext::EXT_ATL_CREATORFUNC _ATL_CREATORFUNC;
typedef  Ext::_ATL_CREATORARGFUNC _ATL_CREATORARGFUNC;
typedef Ext::_ATL_INTMAP_ENTRY _ATL_INTMAP_ENTRY;

#endif

} // ATL::

#define BEGIN_COM_MAP _EXT_BEGIN_COM_MAP
#define END_COM_MAP _EXT_END_COM_MAP
#define DECLARE_GET_CONTROLLING_UNKNOWN _EXT_DECLARE_GET_CONTROLLING_UNKNOWN
#define COM_INTERFACE_ENTRY _EXT_COM_INTERFACE_ENTRY
#define COM_INTERFACE_ENTRY_CHAIN _EXT_COM_INTERFACE_ENTRY_CHAIN
#define DECLARE_REGISTRY _EXT_DECLARE_REGISTRY
#define BEGIN_OBJECT_MAP _EXT_BEGIN_OBJECT_MAP
#define END_OBJECT_MAP _EXT_END_OBJECT_MAP
#define OBJECT_ENTRY _EXT_OBJECT_ENTRY
#define BEGIN_CONNECTION_POINT_MAP _EXT_BEGIN_CONNECTION_POINT_MAP
#define CONNECTION_POINT_ENTRY _EXT_CONNECTION_POINT_ENTRY
#define END_CONNECTION_POINT_MAP _EXT_END_CONNECTION_POINT_MAP

