/*######     Copyright (c) 1997-2012 Ufasoft  http://ufasoft.com  mailto:support@ufasoft.com,  Sergey Pavlov  mailto:dev@ufasoft.com #####
# This program is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published #
# by the Free Software Foundation; either version 3, or (at your option) any later version. This program is distributed in the hope that #
# it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. #
# See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with this #
# program; If not, see <http://www.gnu.org/licenses/>                                                                                    #
########################################################################################################################################*/

﻿using System;
using System.Collections.Generic;
using System.Text;

namespace Win32 {

	public enum Error : int {
		ERROR_SUCCESS = 0,
		ERROR_INVALID_FUNCTION		= 1,
		ERROR_FILE_NOT_FOUND		= 2,
		ERROR_PATH_NOT_FOUND		= 3,
		ERROR_TOO_MANY_OPEN_FILES	= 4,
		ERROR_ACCESS_DENIED			= 5,
		ERROR_INVALID_HANDLE		= 6,
		ERROR_ARENA_TRASHED			= 7,
		ERROR_NOT_ENOUGH_MEMORY		= 8,
		ERROR_INVALID_BLOCK			= 9,
		ERROR_BAD_ENVIRONMENT		= 10,
		ERROR_BAD_FORMAT			= 11,
		ERROR_INVALID_ACCESS		= 12,
		ERROR_INVALID_DATA			= 13,
		ERROR_OUTOFMEMORY			= 14,
		ERROR_INVALID_DRIVE			= 15,
		ERROR_CURRENT_DIRECTORY		= 16,
		ERROR_NOT_SAME_DEVICE		= 17,
		ERROR_NO_MORE_FILES			= 18,

        E_FAIL                      = unchecked((int)0x80004005),

	}

}
