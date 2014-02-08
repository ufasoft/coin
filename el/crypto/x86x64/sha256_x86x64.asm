;########## Copyright (c) 1997-2013 Ufasoft   http://ufasoft.com   mailto:support@ufasoft.com,  Sergey Pavlov  mailto:dev@ufasoft.com    ###################################################################################################
;                                                                                                                                                                                                                                          #
; This program is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation; either version 3, or (at your option) any later version.         #
; This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details. #
; You should have received a copy of the GNU General Public License along with this program; If not, see <http://www.gnu.org/licenses/>                                                                                                    #
;###########################################################################################################################################################################################################################################

; SHA-256 for x86/x64/SSE2

include el/comp/x86x64.inc

OPTION_LANGUAGE_C

IF X64
ELSE
.XMM
ENDIF


EXTRN g_sha256_hinit:PTR BYTE
EXTRN g_sha256_k:PTR BYTE
EXTRN g_4sha256_k:PTR BYTE


.CODE

IF X64	
IFDEF __JWASM__			; UNIX calling convention
	Sha256Update_x86x64	PROC PUBLIC USES ZBX ZSI ZDI R14 ; , x:QWORD	; x - just for generating frame   rdi=state, rsi= data
	push	zdi
	mov		zdx, zdi
ELSE
	Sha256Update_x86x64	PROC PUBLIC USES ZBX ZSI ZDI R14 ; , state:QWORD, data:QWORD		;   rcx=state, rdx=data
	push	zcx
	mov		zsi, zdx
	mov		zdx, zcx
ENDIF
	lea	r14, g_sha256_k
ELSE
	Sha256Update_x86x64	PROC PUBLIC USES EBX ECX EDX ESI EDI, state:DWORD, data:DWORD
	mov		zdx, state
	push	zdx
	mov		zsi, data		
ENDIF
	push	zbp
	sub		zsp, 16*4
	mov		zcx, 16
LAB_COPYSRC:
	lodsd
	mov		[zsp+zcx*4-4], eax
	loop	LAB_COPYSRC
	mov		zsi, zdx
	sub		zsp, 8*4
	mov		zdi, zsp
	mov		zcx, 8*4/ZWORD_SIZE
	rep movsZ

	mov		zsi, zsp
	sub		zsp, 64*4

	Base	EQU		zsi
LAB_LOOP:
	mov		edx, [Base+8*4+(16-1)*4]		; w_i
	mov		edi, edx
	add		edx, [Base+7*4]				; w_i+h
	cmp		zcx, (64-16)*4
	jae		SKIP_CALC_W

	add		edi, [Base+8*4+(7-1)*4]

	mov		eax, [Base+8*4+(15-1)*4]		; (w_15>>>7) ^ (w_15>>>18) ^ (w_15 >> 3)
	mov		ebx, eax
	shr		ebx, 3
	ror		eax, 7
	xor		ebx, eax
	ror		eax, 11
	xor		ebx, eax
	add		edi, ebx

	mov		eax, [Base+8*4+(2-1)*4]		; (w_2>>>17) ^ (w_2>>>19) ^ (w_2 >> 10)
	mov		ebx, eax
	shr		ebx, 10
	ror		eax, 17
	xor		ebx, eax
	ror		eax, 2
	xor		ebx, eax
	add		edi, ebx

	mov		[Base+7*4], edi				; w_(i+16)
SKIP_CALC_W:
IF X64
	add		edx, DWORD PTR [r14+zcx]
ELSE
	add		edx, DWORD PTR g_sha256_k[zcx]  ; += k_i
ENDIF

	mov		eax, [Base+4*4]				; e

	mov		ebx, [Base+6*4]				; g
	mov		ebp, ebx
	xor		ebx, [Base+5*4]				; f ^ g
	and		ebx, eax
	xor		ebx, ebp					; Ch(e, f, g) == g ^ (e &  (f ^ g))
	add		edx, ebx					; w_i + h + Ch(e, f, g)
	
	ror		eax, 6						; (e>>>6) ^ (e>>>11) ^ (e>>>25)
	mov		ebx, eax
	ror		eax, 5
	xor		ebx, eax
	ror		eax, 14
	xor		ebx, eax
	add		edx, ebx					; t1
	add		[Base+3*4], edx				; e = d+t1

	mov		eax, [Base]                  ; a
	mov		ebx, eax
	mov		ebp, eax
	xor		ebx, [Base+4]				; a^b
	xor		ebp, [Base+2*4]				; a^c
	and		ebx, ebp
	xor		ebx, eax					; Ma(a, b, c)
	add		edx, ebx					; t1 += Ma(a, b, c)

	ror		eax, 2						; (a>>>2) ^ (a>>>13) ^ (a>>>22)
	mov		ebx, eax
	ror		eax, 11
	xor		ebx, eax
	ror		eax, 9
	xor		ebx, eax
	add		edx, ebx
	sub		zsi, 4
	mov		[zsi], edx

	add		cl, 4
	jnz		LAB_LOOP

	mov		zdi, [zsp+(64+8+16)*4+ZWORD_SIZE]
	mov		zcx, 8
LOOP_ADD:
	lodsd
	add		[zdi], eax
	add		zdi, 4
	loop	LOOP_ADD
	add		zsp, (64+8+16)*4
	pop		zbp
	pop		zax
	ret
Sha256Update_x86x64 ENDP


IF X64	
IFDEF __JWASM__			; UNIX calling convention
	Sha256Update_4way_x86x64Sse2	PROC PUBLIC USES ZBX ZSI ZDI R9 R10 R11 R12 R13 R15 ; , x:QWORD	; x - just for generating frame   rdi=state, rsi= data
	mov		zdx, zdi
ELSE
	Sha256Update_4way_x86x64Sse2	PROC PUBLIC USES ZBX ZSI ZDI R9 R10 R11 R12 R13 R15 ; , state:QWORD, data:QWORD		;   rcx=state, rdx=data
	mov		zsi, zdx
	mov		zdx, zcx
ENDIF
	lea	r15, g_4sha256_k
ELSE
	Sha256Update_4way_x86x64Sse2	PROC PUBLIC USES EBX ECX EDX ESI EDI, state:DWORD, data:DWORD
	mov		zdx, state
	mov		zsi, data		
ENDIF
	push	zbp
	mov		zbp, zsp

	sub		zsp, 64*16
	and		zsp, NOT 63

	mov		zdi, zsp
	mov		zcx, 16*16/ZWORD_SIZE
	rep movsZ
	lea		zsi, [zsp+16*16]
	mov		zcx, 48
LAB_CALC:
	movdqa	xmm0, [zsi-15*16]
	movdqa	xmm2, xmm0					; (Rotr32(w_15, 7) ^ Rotr32(w_15, 18) ^ (w_15 >> 3))
	psrld	xmm0, 3
	movdqa	xmm1, xmm0
	pslld	xmm2, 14
	psrld	xmm1, 4
	pxor	xmm0, xmm1
	pxor	xmm0, xmm2
	pslld	xmm2, 11
	psrld	xmm1, 11
	pxor	xmm0, xmm1
	pxor	xmm0, xmm2

	paddd	xmm0, [zsi-16*16]

	movdqa	xmm3, [zsi-2*16]
	movdqa	xmm2, xmm3					; (Rotr32(w_2, 17) ^ Rotr32(w_2, 19) ^ (w_2 >> 10))

	paddd	xmm0, [zsi-7*16]

	psrld	xmm3, 10
	movdqa	xmm1, xmm3
	pslld	xmm2, 13
	psrld	xmm1, 7
	pxor	xmm3, xmm1
	pxor	xmm3, xmm2
	pslld	xmm2, 2
	psrld	xmm1, 2
	pxor	xmm3, xmm1
	pxor	xmm3, xmm2
	paddd	xmm0, xmm3
	
	add		zsi, 16
	movdqa	[zsi-16], xmm0
	dec		zcx
	jnz		LAB_CALC
	
	mov		zsi, zsp
	sub		zsp, 9*16

	dotimes_i_8		< movdqa xmm_i, [zdx+i*16] >			; xmm0  = a
IF X64														; xmm1  = b
	movdqa	xmm8, xmm5										; xmm2  = c
	movdqa	xmm9, xmm6										; xmm3  = d
	movdqa	xmm10, xmm7										; xmm4  = e
ELSE														; (x64 xmm8  = f)
	movdqa	[zsp+5*16], xmm5								; (x64 xmm9  = g)
	movdqa	[zsp+6*16], xmm6								; (x64 xmm10 = h)
	movdqa	[zsp+7*16], xmm7
ENDIF

	movdqa	xmm7, xmm2
	pxor	xmm7, xmm1
IF X64
	movdqa	xmm15, xmm7
ELSE
	movdqa	[zsp+8*16], xmm7		; b ^ c
ENDIF
	mov		zax, 0
	mov		zcx, 64
LAB_LOOP:

	;; T t1 = h + (Rotr32(e, 6) ^ Rotr32(e, 11) ^ Rotr32(e, 25)) + ((e & f) ^ AndNot(e, g)) + Expand32<T>(g_sha256_k[j]) + w[j];
	movdqa	xmm7, [zsi+zax*4]

IF X64
	paddd	xmm7, xmm10				; +h
ELSE
	paddd	xmm7, [zsp+7*16]		; +h
ENDIF

	movdqa	xmm5, xmm4
IF X64
	movdqa	xmm6, xmm9
	paddd	xmm7, OWORD PTR [r15+zax*4]
ELSE
	movdqa	xmm6, [zsp+6*16]
	paddd	xmm7, OWORD PTR g_4sha256_k[zax*4]
ENDIF
	pandn	xmm5, xmm6				; ~e & g
	
	add		zax, 4

IF X64
	movdqa	xmm10, xmm6				; h = g
	movdqa	xmm6, xmm8				; f
	movdqa	xmm9, xmm6				; g = f
ELSE
	movdqa	[zsp+7*16], xmm6		; h = g
	movdqa	xmm6, [zsp+5*16]		; f
	movdqa	[zsp+6*16], xmm6		; g = f
ENDIF

	pand	xmm6, xmm4				; e & f
	pxor	xmm5, xmm6				; (e & f) ^ (~e & g)
IF X64
	movdqa	xmm8, xmm4				; f = e
ELSE
	movdqa	[zsp+5*16], xmm4		; f = e
ENDIF
	paddd	xmm7, xmm5

	movdqa	xmm5, xmm4
	psrld	xmm4, 6
	movdqa	xmm6, xmm4
	pslld	xmm5, 7
	psrld	xmm6, 5
	pxor	xmm4, xmm5
	pxor	xmm4, xmm6
	pslld	xmm5, 14
	psrld	xmm6, 14
	pxor	xmm4, xmm5
	pxor	xmm4, xmm6
	pslld	xmm5, 5
	pxor	xmm4, xmm5				; Rotr32(e, 6) ^ Rotr32(e, 11) ^ Rotr32(e, 25)
	paddd	xmm7, xmm4				; xmm7 = t1

	movdqa	xmm4, xmm3			; d
	paddd	xmm4, xmm7			; e = d+t1

	movdqa	xmm3, xmm2			; d = c
	movdqa	xmm2, xmm1			; c = b

	pxor	xmm1, xmm0			; a ^ b
	movdqa	xmm6, xmm1
IF X64
	pand	xmm1, xmm15
	movdqa	xmm15, xmm6
ELSE
	pand	xmm1, [zsp+8*16]
	movdqa	[zsp+8*16], xmm6
ENDIF
	pxor	xmm1, xmm2			; maj(a, b, c)

	paddd	xmm7, xmm1			; t1 +  maj(a, b, c)
	movdqa	xmm1, xmm0			; b = a
		
	movdqa	xmm6, xmm0
	psrld	xmm0, 2
	movdqa	xmm5, xmm0	
	pslld	xmm6, 10
	psrld	xmm5, 11
	pxor	xmm0, xmm6
	pxor	xmm0, xmm5
	pslld	xmm6, 9
	psrld	xmm5, 9
	pxor	xmm0, xmm6
	pxor	xmm0, xmm5
	pslld	xmm6, 11
	pxor	xmm0, xmm6
	paddd	xmm0, xmm7			; a = t1 + (Rotr32(a, 2) ^ Rotr32(a, 13) ^ Rotr32(a, 22)) + maj(a, b, c);	

	dec		zcx
	jnz		LAB_LOOP	

IF X64
	movdqa	xmm5, xmm8
	movdqa	xmm6, xmm9
	movdqa	xmm7, xmm10
ELSE
	movdqa	xmm5, [zsp+5*16]
	movdqa	xmm6, [zsp+6*16]
	movdqa	xmm7, [zsp+7*16]
ENDIF
	dotimes_i_8		< paddd xmm_i, [zdx+i*16] >
	dotimes_i_8		< movdqa [zdx+i*16], xmm_i >

	leave
	ret
Sha256Update_4way_x86x64Sse2 ENDP


		END




