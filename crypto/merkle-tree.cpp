/*######     Copyright (c) 1997-2013 Ufasoft  http://ufasoft.com  mailto:support@ufasoft.com,  Sergey Pavlov  mailto:dev@ufasoft.com #######################################
#                                                                                                                                                                          #
# This program is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation;  #
# either version 3, or (at your option) any later version. This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the      #
# implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details. You should have received a copy of the GNU #
# General Public License along with this program; If not, see <http://www.gnu.org/licenses/>                                                                               #
##########################################################################################################################################################################*/

#include <el/ext.h>

#include "hash.h"

namespace Ext { namespace Crypto {

void PartialMerkleTreeBase::TraverseAndBuild(int height, size_t pos, const void *ar, const dynamic_bitset<byte>& vMatch) {
    // determine whether this node is the parent of at least one matched txid
    bool fParentOfMatch = false;
    for (size_t p=pos<<height; p < (pos+1)<<height && p< NItems; p++)
        fParentOfMatch |= vMatch[p];
	Bitset.push_back(fParentOfMatch);
    if (height==0 || !fParentOfMatch) {
		AddHash(height, pos, ar);
    } else {
        // otherwise, don't store any hash, but descend into the subtrees
        TraverseAndBuild(height-1, pos*2, ar, vMatch);
        if (pos*2+1 < CalcTreeWidth(height-1))
            TraverseAndBuild(height-1, pos*2+1, ar, vMatch);
    }

}

}} // Ext::Crypto

