;########## Copyright (c) 1997-2013 Ufasoft   http://ufasoft.com   mailto:support@ufasoft.com,  Sergey Pavlov  mailto:dev@ufasoft.com    ###################################################################################################
;                                                                                                                                                                                                                                          #
; This program is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation; either version 3, or (at your option) any later version.         #
; This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details. #
; You should have received a copy of the GNU General Public License along with this program; If not, see <http://www.gnu.org/licenses/>                                                                                                    #
;###########################################################################################################################################################################################################################################

; Scrypt Hash using Salsa20/8 for x86/x64/SSE2

include x86x64.inc

OPTION_LANGUAGE_C

IF X64
ELSE
.XMM
ENDIF

.CODE

salsa20_round MACRO		a, b, c, d							; contract: b==xmm4
	paddd	xmm4, a
	movdqa	xmm5, xmm4
	pslld	xmm4, 7
	psrld	xmm5, 32-7
	pxor	d, xmm4
	pxor	d, xmm5
	
	movdqa	xmm4, a
	paddd	xmm4, d
	movdqa	xmm5, xmm4
	pslld	xmm4, 9
	psrld	xmm5, 32-9
	pxor	c, xmm4
	pshufd	xmm4, d, 010010011b	; 		; xmm4 = target *1
	pxor	c, xmm5

	paddd	d, c
	movdqa	xmm5, d
	pslld	d, 13
	psrld	xmm5, 32-13
	pxor	b, d
	pxor	b, xmm5
		
	pshufd	d, b, 000111001b
	paddd	b, c
	pshufd	c, c, 001001110b
	movdqa	xmm5, b
	pslld	b, 18
	psrld	xmm5, 32-18
	pxor	a, b
	pxor	a, xmm5
	movdqa	b, xmm4
ENDM


CalcSalsa20_8 PROC
	movdqa	[zbx], xmm0
	movdqa	[zbx+16], xmm1
	movdqa	[zbx+32], xmm2
	movdqa	[zbx+48], xmm3
	movdqa	xmm4, xmm1

	salsa20_round	xmm0, xmm1, xmm2, xmm3
	salsa20_round	xmm0, xmm1, xmm2, xmm3
	salsa20_round	xmm0, xmm1, xmm2, xmm3
	salsa20_round	xmm0, xmm1, xmm2, xmm3
	salsa20_round	xmm0, xmm1, xmm2, xmm3
	salsa20_round	xmm0, xmm1, xmm2, xmm3
	salsa20_round	xmm0, xmm1, xmm2, xmm3
	salsa20_round	xmm0, xmm1, xmm2, xmm3


;	mov		ecx, 8
;SALSA_LOOP:
;	salsa20_round	xmm0, xmm1, xmm2, xmm3
;	dec		ecx
;	jnz		SALSA_LOOP
	
	paddd	xmm0, [zbx]
	paddd	xmm1, [zbx+16]
	paddd	xmm2, [zbx+32]
	paddd	xmm3, [zbx+48]

	movdqa	[zbx], xmm0
	movdqa	[zbx+16], xmm1
	movdqa	[zbx+32], xmm2
	movdqa	[zbx+48], xmm3
	
	ret
CalcSalsa20_8 ENDP

IF X64

CalcSalsa20_8_x64 PROC
	pxor	xmm0, xmm8
	pxor	xmm1, xmm9
	pxor	xmm2, xmm10
	pxor	xmm3, xmm11

	movdqa	xmm12, xmm0
	movdqa	xmm13, xmm1
	movdqa	xmm14, xmm2
	movdqa	xmm15, xmm3

	movdqa	xmm4, xmm1
	salsa20_round	xmm0, xmm1, xmm2, xmm3
	salsa20_round	xmm0, xmm1, xmm2, xmm3
	salsa20_round	xmm0, xmm1, xmm2, xmm3
	salsa20_round	xmm0, xmm1, xmm2, xmm3
	salsa20_round	xmm0, xmm1, xmm2, xmm3
	salsa20_round	xmm0, xmm1, xmm2, xmm3
	salsa20_round	xmm0, xmm1, xmm2, xmm3
	salsa20_round	xmm0, xmm1, xmm2, xmm3

	paddd	xmm0, xmm12
	paddd	xmm1, xmm13
	paddd	xmm2, xmm14
	paddd	xmm3, xmm15


	pxor	xmm8, xmm0
	pxor	xmm9, xmm1
	pxor	xmm10, xmm2
	pxor	xmm11, xmm3

	movdqa	xmm12, xmm8
	movdqa	xmm13, xmm9
	movdqa	xmm14, xmm10
	movdqa	xmm15, xmm11

	movdqa	xmm4, xmm9
	salsa20_round	xmm8, xmm9, xmm10, xmm11
	salsa20_round	xmm8, xmm9, xmm10, xmm11
	salsa20_round	xmm8, xmm9, xmm10, xmm11
	salsa20_round	xmm8, xmm9, xmm10, xmm11
	salsa20_round	xmm8, xmm9, xmm10, xmm11
	salsa20_round	xmm8, xmm9, xmm10, xmm11
	salsa20_round	xmm8, xmm9, xmm10, xmm11
	salsa20_round	xmm8, xmm9, xmm10, xmm11

	paddd	xmm8, xmm12
	paddd	xmm9, xmm13
	paddd	xmm10, xmm14
	paddd	xmm11, xmm15

	ret
CalcSalsa20_8_x64 ENDP

ENDIF						; X64


IF X64
IFDEF __JWASM__			; UNIX calling convention
	ScryptCore_x86x64	PROC PUBLIC USES ZBX ZCX ZDX ZSI, w:QWORD, scratch:QWORD
ELSE
	ScryptCore_x86x64 PROC PUBLIC USES ZBX ZCX ZDX ZSI ZDI	;;; , w:QWORD, scratch:QWORD
	mov		zdi, zcx		; w
	mov		zsi, zdx		; scratch
ENDIF
ELSE
	ScryptCore_x86x64	PROC PUBLIC USES EBX ECX EDX ESI EDI, w:DWORD, scratch:DWORD
	mov		zdi, w
	mov		zsi, scratch
ENDIF

IF X64
	lea		zdx, [zsi+1024*128]

	movdqa	xmm0, [zdi]
	movdqa	xmm1, [zdi+16]
	movdqa	xmm2, [zdi+32]
	movdqa	xmm3, [zdi+48]
	movdqa	xmm8, [zdi+64]
	movdqa	xmm9, [zdi+80]
	movdqa	xmm10, [zdi+96]
	movdqa	xmm11, [zdi+112]	

	mov		zbx, zsi
ELSE
	lea		zdx, [zsi+1024*128+64]

	movdqa	xmm0, [zdi+64]
	movdqa	xmm1, [zdi+80]
	movdqa	xmm2, [zdi+96]
	movdqa	xmm3, [zdi+112]
	movdqa	xmm4, [zdi]
	movdqa	xmm5, [zdi+16]
	movdqa	xmm6, [zdi+32]
	movdqa	xmm7, [zdi+48]

	lea		zbx, [zsi+64]

	movdqa	[zbx], xmm0
	movdqa	[zbx+16], xmm1
	movdqa	[zbx+32], xmm2
	movdqa	[zbx+48], xmm3
	movdqa	[zbx-64], xmm4
	movdqa	[zbx-48], xmm5
	movdqa	[zbx-32], xmm6
	movdqa	[zbx-16], xmm7
ENDIF
LAB_LOOP1:
IF X64
	movdqa	[zbx], xmm0
	movdqa	[zbx+16], xmm1
	movdqa	[zbx+32], xmm2
	movdqa	[zbx+48], xmm3
	movdqa	[zbx+64], xmm8
	movdqa	[zbx+80], xmm9
	movdqa	[zbx+96], xmm10
	movdqa	[zbx+112], xmm11

	call	CalcSalsa20_8_x64
	add		zbx, 128
ELSE
	pxor	xmm0, [zbx-64]
	pxor	xmm1, [zbx-48]
	pxor	xmm2, [zbx-32]
	pxor	xmm3, [zbx-16]
	add		zbx, 64
	call	CalcSalsa20_8
ENDIF
	cmp		zbx, zdx
	jb		LAB_LOOP1

IF X64

ELSE
	movdqa	xmm4, [zbx-64]
	movdqa	xmm5, [zbx-48]
	movdqa	xmm6, [zbx-32]
	movdqa	xmm7, [zbx-16]

	movdqa	[zdi+64], xmm0
	movdqa	[zdi+80], xmm1
	movdqa	[zdi+96], xmm2
	movdqa	[zdi+112], xmm3
	movdqa	[zdi], xmm4
	movdqa	[zdi+16], xmm5
	movdqa	[zdi+32], xmm6
	movdqa	[zdi+48], xmm7
ENDIF
	
	mov		edx, 1024
LAB_LOOP2:
IF X64
	movd	eax, xmm8
	and		eax, 1023
	shl		eax, 7
	cdqe
	lea		zbx, [zsi+zax]

	pxor	xmm0, [zbx+0]
	pxor	xmm1, [zbx+16]
	pxor	xmm2, [zbx+32]
	pxor	xmm3, [zbx+48]
	pxor	xmm8, [zbx+64]
	pxor	xmm9, [zbx+80]
	pxor	xmm10, [zbx+96]
	pxor	xmm11, [zbx+112]

	call	CalcSalsa20_8_x64
ELSE
	movd	eax, xmm0
	and		eax, 1023
	shl		eax, 7
	lea		zbx, [zsi+zax]

	movdqa	xmm4, [zdi]
	movdqa	xmm5, [zdi+16]
	movdqa	xmm6, [zdi+32]
	movdqa	xmm7, [zdi+48]

	pxor	xmm0, [zbx+64]
	pxor	xmm1, [zbx+80]
	pxor	xmm2, [zbx+96]
	pxor	xmm3, [zbx+112]
	pxor	xmm4, [zbx+0]
	pxor	xmm5, [zbx+16]
	pxor	xmm6, [zbx+32]
	pxor	xmm7, [zbx+48]

	movdqa	[zdi+64], xmm0
	movdqa	[zdi+80], xmm1
	movdqa	[zdi+96], xmm2
	movdqa	[zdi+112], xmm3

	pxor	xmm0, xmm4
	pxor	xmm1, xmm5
	pxor	xmm2, xmm6
	pxor	xmm3, xmm7

	lea		zbx, [zdi]
	call	CalcSalsa20_8

	pxor	xmm0, [zdi+64]
	pxor	xmm1, [zdi+80]
	pxor	xmm2, [zdi+96]
	pxor	xmm3, [zdi+112]

	lea		zbx, [zdi+64]
	call	CalcSalsa20_8
ENDIF
	dec		edx
	jnz		LAB_LOOP2
IF X64
	movdqa	[zdi], xmm0
	movdqa	[zdi+16], xmm1 
	movdqa	[zdi+32], xmm2 
	movdqa	[zdi+48], xmm3
	movdqa	[zdi+64], xmm8
	movdqa	[zdi+80], xmm9 
	movdqa	[zdi+96], xmm10
	movdqa	[zdi+112], xmm11

ENDIF
	ret
ScryptCore_x86x64	ENDP

IF X64
salsa20_sub_round_x64 MACRO		x, x1, x2, y, y1, y2, z, z1, z2, off
	movdqa	xmm12, x1
	movdqa	xmm13, y1
	movdqa	xmm14, z1
	paddd	xmm12, x2
	paddd	xmm13, y2
	paddd	xmm14, z2

	movdqa	xmm15, xmm12
	pslld	xmm12, off
	psrld	xmm15, 32-off
	pxor	x, xmm12
	pxor	x, xmm15

	movdqa	xmm15, xmm13
	pslld	xmm13, off
	psrld	xmm15, 32-off
	pxor	y, xmm13
	pxor	y, xmm15

	movdqa	xmm15, xmm14
	pslld	xmm14, off
	psrld	xmm15, 32-off
	pxor	z, xmm14
	pxor	z, xmm15
ENDM

salsa20_double_round_x64 MACRO
	salsa20_sub_round_x64	xmm3, xmm0, xmm1,  xmm7, xmm4, xmm5,  xmm11, xmm8, xmm9,	  7
	salsa20_sub_round_x64	xmm2, xmm3, xmm0,  xmm6, xmm7, xmm4,  xmm10, xmm11, xmm8,    9
	salsa20_sub_round_x64	xmm1, xmm2, xmm3,  xmm5, xmm6, xmm7,  xmm9, xmm10, xmm11,    13
	salsa20_sub_round_x64	xmm0, xmm1, xmm2,  xmm4, xmm5, xmm6,  xmm8, xmm9, xmm10,	  18

	pshufd	xmm2,  xmm2,  001001110b
	pshufd	xmm6,  xmm6,  001001110b
	pshufd	xmm10, xmm10, 001001110b

	pshufd	xmm1, xmm1, 000111001b
	pshufd	xmm5, xmm5, 000111001b
	pshufd	xmm9, xmm9, 000111001b

	pshufd	xmm3, xmm3,   010010011b
	pshufd	xmm7, xmm7,   010010011b
	pshufd	xmm11, xmm11, 010010011b

	salsa20_sub_round_x64	xmm1, xmm0, xmm3,  xmm5, xmm4, xmm7,  xmm9, xmm8, xmm11,	  7
	salsa20_sub_round_x64	xmm2, xmm1, xmm0,  xmm6, xmm5, xmm4,  xmm10, xmm9, xmm8,    9
	salsa20_sub_round_x64	xmm3, xmm2, xmm1,  xmm7, xmm6, xmm5,  xmm11, xmm10, xmm9,    13
	salsa20_sub_round_x64	xmm0, xmm3, xmm2,  xmm4, xmm7, xmm6,  xmm8, xmm11, xmm10,	  18

	pshufd	xmm2,  xmm2,  001001110b
	pshufd	xmm6,  xmm6,  001001110b
	pshufd	xmm10, xmm10, 001001110b

	pshufd	xmm1, xmm1, 010010011b           
	pshufd	xmm5, xmm5, 010010011b           
	pshufd	xmm9, xmm9, 010010011b           

	pshufd	xmm3, xmm3,   000111001b
	pshufd	xmm7, xmm7,   000111001b
	pshufd	xmm11, xmm11, 000111001b
ENDM

salsa20_rounds_x64 MACRO
	
	 LOCAL lbl
	mov		ecx, 4
lbl:
	salsa20_double_round_x64
	dec		ecx
	jnz		lbl
ENDM

CalcSalsa20_8_3way_x64 PROC
	movdqa	[zbx    +16*0], xmm0  
	movdqa	[zbx    +16*1], xmm1  
	movdqa	[zbx    +16*2], xmm2  
	movdqa	[zbx    +16*3], xmm3  
	movdqa	[zbx+128+16*0],	xmm4  
	movdqa	[zbx+128+16*1],	xmm5  
	movdqa	[zbx+128+16*2],	xmm6  
	movdqa	[zbx+128+16*3],	xmm7  
	movdqa	[zbx+256+16*0],	xmm8  
	movdqa	[zbx+256+16*1],	xmm9  
	movdqa	[zbx+256+16*2],	xmm10 
	movdqa	[zbx+256+16*3],	xmm11 

	salsa20_rounds_x64

	paddd	xmm0,  [zbx+16*0]
	paddd	xmm1,  [zbx+16*1]
	paddd	xmm2,  [zbx+16*2]
	paddd	xmm3,  [zbx+16*3]
	paddd	xmm4,  [zbx+128+16*0]
	paddd	xmm5,  [zbx+128+16*1]
	paddd	xmm6,  [zbx+128+16*2]
	paddd	xmm7,  [zbx+128+16*3]
	paddd	xmm8,  [zbx+256+16*0]
	paddd	xmm9,  [zbx+256+16*1]
	paddd	xmm10, [zbx+256+16*2]
	paddd	xmm11, [zbx+256+16*3]

	movdqa	[zbx    +16*0], xmm0  
	movdqa	[zbx    +16*1], xmm1  
	movdqa	[zbx    +16*2], xmm2  
	movdqa	[zbx    +16*3], xmm3  
	movdqa	[zbx+128+16*0],	xmm4  
	movdqa	[zbx+128+16*1],	xmm5  
	movdqa	[zbx+128+16*2],	xmm6  
	movdqa	[zbx+128+16*3],	xmm7  
	movdqa	[zbx+256+16*0],	xmm8  
	movdqa	[zbx+256+16*1],	xmm9  
	movdqa	[zbx+256+16*2],	xmm10 
	movdqa	[zbx+256+16*3],	xmm11 

	pxor	xmm0,  [zsi+64+16*0]
	pxor	xmm1,  [zsi+64+16*1]
	pxor	xmm2,  [zsi+64+16*2]
	pxor	xmm3,  [zsi+64+16*3]
	pxor	xmm4,  [zsi+192+16*0]
	pxor	xmm5,  [zsi+192+16*1]
	pxor	xmm6,  [zsi+192+16*2]
	pxor	xmm7,  [zsi+192+16*3]
	pxor	xmm8,  [zsi+320+16*0]
	pxor	xmm9,  [zsi+320+16*1]
	pxor	xmm10, [zsi+320+16*2]
	pxor	xmm11, [zsi+320+16*3]

	movdqa	[zbx+64+16*0], xmm0  
	movdqa	[zbx+64+16*1], xmm1  
	movdqa	[zbx+64+16*2], xmm2  
	movdqa	[zbx+64+16*3], xmm3  
	movdqa	[zbx+192+16*0],	xmm4  
	movdqa	[zbx+192+16*1],	xmm5  
	movdqa	[zbx+192+16*2],	xmm6  
	movdqa	[zbx+192+16*3],	xmm7  
	movdqa	[zbx+320+16*0],	xmm8  
	movdqa	[zbx+320+16*1],	xmm9  
	movdqa	[zbx+320+16*2],	xmm10 
	movdqa	[zbx+320+16*3],	xmm11 

	salsa20_rounds_x64

	paddd	xmm0,  [zbx+64+16*0]
	paddd	xmm1,  [zbx+64+16*1]
	paddd	xmm2,  [zbx+64+16*2]
	paddd	xmm3,  [zbx+64+16*3]
	paddd	xmm4,  [zbx+192+16*0]
	paddd	xmm5,  [zbx+192+16*1]
	paddd	xmm6,  [zbx+192+16*2]
	paddd	xmm7,  [zbx+192+16*3]
	paddd	xmm8,  [zbx+320+16*0]
	paddd	xmm9,  [zbx+320+16*1]
	paddd	xmm10, [zbx+320+16*2]
	paddd	xmm11, [zbx+320+16*3]

	movdqa	[zbx+64+16*0],  xmm0  
	movdqa	[zbx+64+16*1],  xmm1  
	movdqa	[zbx+64+16*2],  xmm2  
	movdqa	[zbx+64+16*3],  xmm3  
	movdqa	[zbx+192+16*0],	xmm4  
	movdqa	[zbx+192+16*1],	xmm5  
	movdqa	[zbx+192+16*2],	xmm6  
	movdqa	[zbx+192+16*3],	xmm7  
	movdqa	[zbx+320+16*0],	xmm8  
	movdqa	[zbx+320+16*1],	xmm9  
	movdqa	[zbx+320+16*2],	xmm10 
	movdqa	[zbx+320+16*3],	xmm11 

	ret
CalcSalsa20_8_3way_x64 ENDP


scrypt_part2_x64 MACRO	bas, a, b, c, d, off
	movdqa	xmm12, [bas+0]
	movdqa	xmm13, [bas+16*1]
	movdqa	xmm14, [bas+16*2]
	movdqa	xmm15, [bas+16*3]

	pxor	a, [bas+16*4]
	pxor	b, [bas+16*5]
	pxor	c, [bas+16*6]
	pxor	d, [bas+16*7]

	pxor	xmm12, [zbx+128*off]
	pxor	xmm13, [zbx+128*off+16]
	pxor	xmm14, [zbx+128*off+16*2]
	pxor	xmm15, [zbx+128*off+16*3]

	movdqa	[zbx+128*off+16*4], a
	movdqa	[zbx+128*off+16*5], b
	movdqa	[zbx+128*off+16*6], c
	movdqa	[zbx+128*off+16*7], d

	pxor	a, xmm12
	pxor	b, xmm13
	pxor	c, xmm14
	pxor	d, xmm15
ENDM


	ScryptCore_x64_3way PROC PUBLIC USES ZBX ZCX ZDX R8 R9 ZSI ZDI R14	;;; , w:QWORD, scratch:QWORD
IFDEF __JWASM__			; UNIX calling convention
	mov		r14, zsi		;	scratch
ELSE
	mov		zdi, zcx		;	w
	mov		r14, zdx		;	scratch
ENDIF
	push	zbp
	mov		zbp, zsp

	mov		zdx, zdi

	mov		zcx, 3*32*4/8
	mov		zdi, r14
	mov		zsi, zdx
	rep movsq

	mov		zdi, zdx
	lea		zdx, [r14+1024*384]	
	mov		zbx, r14

	movdqa	xmm0,  [zbx+64+16*0]  
	movdqa	xmm1,  [zbx+64+16*1]  
	movdqa	xmm2,  [zbx+64+16*2]  
	movdqa	xmm3,  [zbx+64+16*3]  
	movdqa	xmm4,  [zbx+192+16*0]	
	movdqa	xmm5,  [zbx+192+16*1]	
	movdqa	xmm6,  [zbx+192+16*2]	
	movdqa	xmm7,  [zbx+192+16*3]	
	movdqa	xmm8,  [zbx+320+16*0]	
	movdqa	xmm9,  [zbx+320+16*1]	
	movdqa	xmm10, [zbx+320+16*2]	
	movdqa	xmm11, [zbx+320+16*3]	
LAB_LOOP1:
	pxor	xmm0,  [zbx+16*0]
	pxor	xmm1,  [zbx+16*1]
	pxor	xmm2,  [zbx+16*2]
	pxor	xmm3,  [zbx+16*3]
	pxor	xmm4,  [zbx+128+16*0]
	pxor	xmm5,  [zbx+128+16*1]
	pxor	xmm6,  [zbx+128+16*2]
	pxor	xmm7,  [zbx+128+16*3]
	pxor	xmm8,  [zbx+256+16*0]
	pxor	xmm9,  [zbx+256+16*1]
	pxor	xmm10, [zbx+256+16*2]
	pxor	xmm11, [zbx+256+16*3]

	mov		zsi, zbx
	add		zbx, 384
	call	CalcSalsa20_8_3way_x64

	cmp		zbx, zdx
	jb		LAB_LOOP1

	mov		zcx, 3*32*4/8
	mov		zsi, zbx
	rep movsq
	sub		zdi, 3*32*4

	mov		r9, 1024
	mov		r8, 128*3
	mov		zbx, zdi
	mov		zsi, zbx
LAB_LOOP2:
	movd	eax, xmm0
	and		rax, 1023
	mul		r8
	lea		zcx, [r14+zax]

	movd	eax, xmm4
	and		rax, 1023
	mul		r8
	lea		zdi, [r14+zax+128]

	movd	eax, xmm8
	and		rax, 1023
	mul		r8
	lea		zax, [r14+zax+256]

	scrypt_part2_x64	zcx, xmm0, xmm1, xmm2, xmm3, 0
	scrypt_part2_x64	zdi, xmm4, xmm5, xmm6, xmm7, 1
	scrypt_part2_x64	zax, xmm8, xmm9, xmm10, xmm11, 2

	call	CalcSalsa20_8_3way_x64

	dec		r9
	jnz		LAB_LOOP2

	leave
	ret
ScryptCore_x64_3way	ENDP

ENDIF ; X64

		END


