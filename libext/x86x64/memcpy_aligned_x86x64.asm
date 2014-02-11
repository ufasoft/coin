;########## Copyright (c) 1997-2013 Ufasoft   http://ufasoft.com   mailto:support@ufasoft.com,  Sergey Pavlov  mailto:dev@ufasoft.com    ###################################################################################################
;                                                                                                                                                                                                                                          #
; This program is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation; either version 3, or (at your option) any later version.         #
; This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details. #
; You should have received a copy of the GNU General Public License along with this program; If not, see <http://www.gnu.org/licenses/>                                                                                                    #
;###########################################################################################################################################################################################################################################

; Memcpy implementations

include el/comp/x86x64.inc

OPTION_LANGUAGE_C

IF X64
ELSE
.XMM
ENDIF

.CODE

EXTRN g_bHasSse2:PTR BYTE

IF X64
	MemcpyAligned32	PROC PUBLIC USES ZSI ZDI, d:QWORD, s:QWORD, sz:QWORD
ELSE
	MemcpyAligned32	PROC PUBLIC USES ZSI ZDI, d:DWORD, s:DWORD, sz:DWORD
ENDIF
	mov		zsi, s
	mov		zcx, sz
	mov		zax, d
	mov		zdi, zax
	cmp		zcx, 8
	jb		$MovsImp
	prefetchnta [zsi]
	cmp		g_bHasSse2, 0
	je		$MovsImp
	add		zcx, 63
   	shr		zcx, 6
$Loop:
	prefetchnta [zsi+64*8]
	movdqa	xmm0, [zsi]
	movdqa	[zdi],		xmm0 
	movdqa	xmm1, [zsi+16]
	movdqa	[zdi+16],	xmm1 
	movdqa	xmm2, [zsi+16*2]
	movdqa	[zdi+16*2],	xmm2 
	movdqa	xmm3, [zsi+16*3]
	movdqa	[zdi+16*3],	xmm3 
	add		zsi, 64
	add		zdi, 64
	loop	$Loop
	ret
$MovsImp:
IF X64
	add		zcx, 7
	shr		zcx, 3
	rep		movsq
ELSE
	add		zcx, 3
	shr		zcx, 2
	rep		movsd
ENDIF
$Ret:
	ret
MemcpyAligned32	ENDP


		END




