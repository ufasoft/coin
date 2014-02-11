;########## Copyright (c) 1997-2013 Ufasoft   http://ufasoft.com   mailto:support@ufasoft.com,  Sergey Pavlov  mailto:dev@ufasoft.com    ###################################################################################################
;                                                                                                                                                                                                                                          #
; This program is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation; either version 3, or (at your option) any later version.         #
; This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details. #
; You should have received a copy of the GNU General Public License along with this program; If not, see <http://www.gnu.org/licenses/>                                                                                                    #
;###########################################################################################################################################################################################################################################

; Blake Hash for x86/x64/SSE2

include x86x64.inc

OPTION_LANGUAGE_C

IF X64
ELSE
.XMM
ENDIF


EXTRN g_blake_sigma:PTR BYTE
EXTRN g_blake512_c:PTR QWORD


.CODE


IF X64

ELSE

Blake512Round PROC PUBLIC USES ZBX ZCX ZDX ZSI ZDI, sigma0:DWORD, sigma1:DWORD, pa:DWORD, pb:DWORD, pc:DWORD, pd:DWORD, m:DWORD
	sub		esp, 3*8
	a	EQU		esp
	cc	EQU		esp+8
	d	EQU		esp+16
	
	mov		edi, sigma0
	mov		ecx, sigma1

	mov		esi, m 					
	mov		eax, [esi+edi*8]
	mov		edx, [esi+edi*8+4]
	xor		eax, g_blake512_c[ecx*8]
	xor		edx, g_blake512_c[ecx*8+4]
	lea		esi, [esi+ecx*8]	; ESI = &m[sigma1]

	mov		ecx, pb
	add		eax, [ecx]
	adc		edx, [ecx+4]
	
	mov		ebx, pa
	add		eax, [ebx]
	adc		edx, [ebx+4]		
	mov		[a], eax
	mov		[a+4], edx      ; a += b + (m[idx2i] ^ blakeC[idx2i1]);	

	mov		ebx, pd
	xor		eax, [ebx]
	xor		edx, [ebx+4]
	mov		[d], edx
	mov		[d+4], eax		; EAX:EDX = d = (d ^ a) >>> 32

	mov		ebx, pc
	add		edx, [ebx]
	adc		eax, [ebx+4]
	mov		[cc], edx
	mov		[cc+4], eax      ; c += d
	
	xor		edx, [ecx]
	xor		eax, [ecx+4]

	mov		ebx, eax
	shrd	eax, edx, 25
	shrd	edx, ebx, 25
	mov		[ecx], edx
	mov		[ecx+4], eax		; b = (b ^ c) >>> 25

	mov		eax, [esi]
	mov		edx, [esi+4]
	xor		eax, g_blake512_c[edi*8]
	xor		edx, g_blake512_c[edi*8+4]
	add		eax, [ecx]
	adc		edx, [ecx+4]
	
	add		eax, [a]
	adc		edx, [a+4]		
	mov		ebx, pa
	mov		[ebx], eax
	mov		[ebx+4], edx    ; a += b + (m[idx2i1] ^ blakeC[idx2i])	

	xor		eax, [d]
	xor		edx, [d+4]
	mov		ebx, eax
	shrd	eax, edx, 16
	shrd	edx, ebx, 16
	mov		ebx, pd
	mov		[ebx], eax
	mov		[ebx+4], edx	; d = (d ^ a) >>> 16

	add		eax, [cc]
	adc		edx, [cc+4]
	mov		ebx, pc
	mov		[ebx], eax
	mov		[ebx+4], edx	; c += d

	xor		eax, [ecx]
	xor		edx, [ecx+4]

	mov		ebx, eax
	shrd	eax, edx, 11
	shrd	edx, ebx, 11
	mov		[ecx], eax
	mov		[ecx+4], edx	; b = (b ^ c) >>> 11

	add		esp, 3*8	
	ret
Blake512Round	ENDP

ENDIF ; X64

		END



