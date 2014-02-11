/*######     Copyright (c) 1997-2013 Ufasoft  http://ufasoft.com  mailto:support@ufasoft.com,  Sergey Pavlov  mailto:dev@ufasoft.com #######################################
#                                                                                                                                                                          #
# This program is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation;  #
# either version 3, or (at your option) any later version. This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the      #
# implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details. You should have received a copy of the GNU #
# General Public License along with this program; If not, see <http://www.gnu.org/licenses/>                                                                               #
##########################################################################################################################################################################*/

#include <el/ext.h>

using namespace Ext;

#undef main
#undef wmain

extern "C" int __cdecl _my_wmain(int argc, wchar_t *argv[], wchar_t *envp[]);
extern "C" int __cdecl _my_main(int argc, char *argv[], char *envp[]);

int _cdecl ext_main(int argc, argv_char_t *argv[], argv_char_t *envp[]) {
#if UCFG_WCE
	RegistryKey(HKEY_LOCAL_MACHINE, "Drivers\\Console").SetValue("OutputTo", 0);
#endif

	atexit(MainOnExit);

#if UCFG_ARGV_UNICODE
	return _my_wmain(argc, argv, envp);
#else
	return _my_main(argc, argv, envp);
#endif
}

#if UCFG_WCE
#	if UCFG_ARGV_UNICODE
#		pragma comment(linker, "/ENTRY:mainWCRTStartup")
#	else
#		pragma comment(linker, "/ENTRY:mainACRTStartup")
#	endif
#endif


