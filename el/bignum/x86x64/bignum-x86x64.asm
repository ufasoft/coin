;########## Copyright (c) 1997-2013 Ufasoft   http://ufasoft.com   mailto:support@ufasoft.com,  Sergey Pavlov  mailto:dev@ufasoft.com    ###################################################################################################
;                                                                                                                                                                                                                                          #
; This program is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation; either version 3, or (at your option) any later version.         #
; This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details. #
; You should have received a copy of the GNU General Public License along with this program; If not, see <http://www.gnu.org/licenses/>                                                                                                    #
;###########################################################################################################################################################################################################################################

include el/comp/x86x64.inc

.CODE

;OPTION PROLOGUE:None
;OPTION EPILOGUE:None

IF X64
ELSE
OPTION LANGUAGE: C
.XMM
ENDIF


IF X64
	ImpAddBignums	PROC PUBLIC USES ZBX ZSI ZDI
		mov     ZSI, RCX			; a
		mov     ZDI, R8				; d
		mov     ZBX, RDX			; b
		mov     ZCX, R9				; siz
ELSE
	ImpAddBignums	PROC stdcall PUBLIC USES ZBX ZSI ZDI, a:DWORD, b:DWORD , d:DWORD, siz:DWORD
		mov     ZSI, a
		mov     ZDI, d
		mov     ZBX, b
		mov     ZCX, siz
ENDIF
		sub     ZBX, ZDI
		clc
@@x:	lodsZ
		adc     ZAX, [ZBX+ZDI]
		stosZ
		loop    @@x
		setnge  CL
		neg     ZCX
		mov     [ZDI], ZCX
		ret
ImpAddBignums	ENDP

IF X64
	ImpAddBignumsEx	PROC PUBLIC USES ZBX ZSI ZDI
		mov     ZDI, RCX			; u
		mov     ZSI, RDX			; b
		mov     ZCX, R8				; siz
ELSE
	ImpAddBignumsEx	PROC stdcall PUBLIC USES ZBX ZSI ZDI, u:DWORD, v:DWORD , siz:DWORD
		mov     ZSI, v
		mov     ZDI, u
		mov     ZCX, siz
ENDIF
		clc
@@x:	lodsZ
		adc     ZAX, [ZDI]
		stosZ
		loop    @@x
		adc		ZWORD_PTR [ZDI], ZCX
		ret
ImpAddBignumsEx	ENDP

IF X64
	ImpSubBignums	PROC PUBLIC USES ZBX ZSI ZDI
		mov     ZSI, RCX			; a
		mov     ZDI, R8				; d
		mov     ZBX, RDX			; b
		mov     ZCX, R9				; siz
ELSE
	ImpSubBignums	PROC stdcall PUBLIC USES ZBX ZSI ZDI, a:DWORD, b:DWORD , d:DWORD, siz:DWORD
		mov     ZSI, a
		mov     ZDI, d
		mov     ZBX, b
		mov     ZCX, siz
ENDIF
		sub     ZBX, ZDI
		clc
@@x:	lodsZ
		sbb     ZAX, [ZBX+ZDI]
		stosZ
		loop    @@x
		setnge  CL
		neg     ZCX
		mov     [ZDI], ZCX
		ret
ImpSubBignums	ENDP


IF X64
	ImpNegBignum	PROC PUBLIC USES ZBX ZSI ZDI
		mov     ZSI, ZCX			; a
		mov     ZDI, R8				; d
		mov     ZCX, RDX			; siz
ELSE
	ImpNegBignum	PROC stdcall PUBLIC USES ZBX ZSI ZDI, a:DWORD, siz:DWORD, d:DWORD
		mov     ESI, a
		mov     EDI, d
		mov     ECX, siz
ENDIF
		clc
@@x:	lodsZ
		mov     ZBX, 0
		sbb     ZBX, ZAX
		xchg    ZAX, ZBX
		stosZ
		loop    @@x
		setnge  CL
		neg     ZCX
		mov     [ZDI], ZCX
		ret
ImpNegBignum	ENDP

IF X64
	ImpShld	PROC PUBLIC USES ZBX ZSI ZDI ZBP	
		mov		RSI, ZCX		; s
		mov		RDI, ZDX		; d
		mov		RCX, R9			; amt
		mov		RBX, R8			; siz
ELSE
	ImpShld	PROC stdcall PUBLIC USES ZBX ZSI ZDI ZBP, s:DWORD, d:DWORD, siz:DWORD , amt:DWORD
		mov		ESI, s
		mov		EDI, d
		mov		ECX, amt
		mov		EBX, siz
ENDIF
		xor		ZBP, ZBP
@@x:	mov		ZDX, ZBP
		lodsZ
		mov		ZBP, ZAX
		shld	ZAX, ZDX, CL
		stosZ
		dec		ZBX
		jnz		@@x
		xchg	ZAX, ZBP
		cZY
		shld	ZDX, ZAX, CL
		mov		[ZDI], ZDX
		ret
ImpShld	ENDP

IF X64
	ImpShrd	PROC PUBLIC USES ZBX ZSI ZDI, s:QWORD, d:QWORD, siz:QWORD, amt:QWORD, prev:QWORD
		mov		s, RCX
		mov		d, RDX
		mov		siz, R8
		mov		amt, R9		
		mov		RSI, ZCX		; s
		mov		RDI, ZDX		; d
		mov		RCX, R9			; amt
		mov		RBX, R8			; siz
ELSE
	ImpShrd	PROC stdcall PUBLIC USES ZBX ZSI ZDI, s:DWORD, d:DWORD, siz:DWORD , amt:DWORD, prev: DWORD
		mov		ESI, s
		mov		EDI, d
		mov		ECX, amt
		mov		EBX, siz
ENDIF
		dec		ZBX
		shl		ZBX, ZWORD_BITS-3
		add		ZSI, ZBX
		add		ZDI, ZBX
		mov		ZBX, siz
		push	ZBP
		mov		ZDX, prev
		std
@@x:	lodsZ
		mov		ZBP, ZAX
		shrd	ZAX, ZDX, CL
		stosZ
		mov		ZDX, ZBP
		dec		ZBX
		jnz		@@x
		pop		ZBP
		cld
		ret
ImpShrd	ENDP

IF X64
	ImpRcl	PROC PUBLIC
ELSE
	ImpRcl	PROC stdcall PUBLIC USES ZBX, siz:DWORD, p:DWORD
		mov	EDX, p
		mov	ECX, siz
ENDIF
		clc
@@x:	rcl		ZWORD_PTR [ZBX], 1
		lahf
		add		ZDX, ZWORD_SIZE
		sahf
		loop		@@x
		setc		AL
		ret
ImpRcl	ENDP


IF X64
	ImpBT	PROC PUBLIC
		mov	ZBX, ZCX
		mov	ZAX, ZDX
ELSE
	ImpBT	PROC C PUBLIC,  p:DWORD, i:DWORD
		mov		ZBX, p
		mov		ZAX, i
ENDIF
		bt		[ZBX], ZAX
		setc		AL
        ret        
ImpBT 	ENDP

IF X64
	ImpMulBignums	PROC PUBLIC USES ZBX ZSI ZDI, a:QWORD, m:QWORD, b:QWORD, n: QWORD, d:QWORD
		mov		a, RCX
		mov		m, RDX
		mov		b, R8
		mov		n, R9
ELSE
	ImpMulBignums	PROC stdcall PUBLIC USES  ZBX ZSI ZDI, a:DWORD, m:DWORD, b:DWORD, n:DWORD, d:DWORD
ENDIF
		mov		ZCX, m
		add		ZCX, n
		mov		ZDI, d
		xor		ZAX, ZAX
		rep stosZ
		mov		ZSI, b
		mov		ZDI, d
		mov		ZCX, n
@@x:	lodsZ
		push	ZCX
		push	ZSI
		push	ZDI
		push	ZBP
		xchg	ZAX, ZBX
		mov		ZSI, a
		mov		ZCX, m
		xor		ZBP, ZBP
@@y:	lodsZ
		mul		ZBX
		add		ZAX, ZBP
		adc		ZDX, 0
		add		[ZDI], ZAX
		adc		ZDX, 0
		add		ZDI, ZWORD_SIZE
		mov		ZBP, ZDX		
		loop	@@y
		mov		[ZDI], ZBP
		pop		ZBP
		pop		ZDI
		add		ZDI, ZWORD_SIZE
		pop		ZSI
		pop		ZCX
		loop		@@x
        ret        
ImpMulBignums 	ENDP


EXTRN g_bHasSse2:PTR BYTE

IF X64
	ImpMulAddBignums	PROC PUBLIC USES ZBX ZSI ZDI, r:QWORD, a:QWORD, n:QWORD, w: QWORD
		mov		r, RCX
		mov		a, RDX
		mov		n, R8
		mov		w, R9
ELSE
	ImpMulAddBignums	PROC stdcall PUBLIC USES ZBX ZSI ZDI, r:DWORD, a:DWORD, n:DWORD, w:DWORD
ENDIF
		mov		ZSI, a
		mov		ZCX, n
		mov		ZDI, r
		mov		ZBX, w
IF X64
ELSE
		cmp		BYTE PTR g_bHasSse2, 0
		je		LAB_BeginMulAddBignums

		pxor	xmm2, xmm2		; carry
		mov		zdx, zcx
		xor		zax, zax
		and		zdx, 3
		shr		zcx, 2
		jz		@@b
		movd		xmm7, zbx
		pshufd		xmm7, xmm7, 0
		pxor		xmm0, xmm0		; const zero
@@c:	movdqu		xmm5, [zsi+zax]		; xmm5 = (a[3] a[2] a[1] a[0])
		movdqu		xmm6, [zdi+zax]		; xmm6 = (r[3] r[2] r[1] r[0])

		pshufd		xmm1, xmm5, 000010000b	; xmm1 = (? a[1] ? a[0])
		pmuludq		xmm1, xmm7				; xmm1 = ({a[1]*w}  {a[0]*w})

		movdqa		xmm3, xmm6
		unpcklps	xmm3, xmm0				; xmm3 = (0 r[1] 0 r[0])
		paddq		xmm1, xmm3		
		paddq		xmm1, xmm2				; xmm1 = ({a[1]*w+r[1]}  {a[0]*w+r[0]+prevCarry})

		unpckhpd	xmm2, xmm1				; xmm2 = ({a[1]*w+r[1]} 0 0)
		unpcklps	xmm1, xmm0				; xmm1 = (0 high({a[0]*w+r[0]+prevCarry} 0 low({a[0]*w+r[0]+prevCarry})
		paddq		xmm2, xmm1
		pshufd		xmm4, xmm2, 010000000b	; xmm4 = (d1 d0 ? ?)
		pshufd		xmm2, xmm2, 001010111b	; xmm2 = (0 0 0 carry)

		pshufd		xmm1, xmm5, 000110010b	; xmm1 = (? a[3] ? a[2])
		pmuludq		xmm1, xmm7				; xmm1 = ({a[3]*w}  {a[2]*w})

		unpckhps	xmm6, xmm0				; xmm6 = (0 r[3] 0 r[2])
		paddq		xmm1, xmm6	
		paddq		xmm1, xmm2				; xmm1 = ({a[3]*w+r[3]}  {a[2]*w+r[2]+prevCarry})
		
		unpckhpd	xmm2, xmm1				; xmm2 = ({a[3]*w+r[3]} 0 0)
		unpcklps	xmm1, xmm0				; xmm1 = (0 high({a[2]*w+r[2]+prevCarry} 0 low({a[2]*w+r[2]+prevCarry})
		paddq		xmm2, xmm1
		pshufd		xmm1, xmm2, 010000000b
		unpckhpd	xmm4, xmm1				; xmm4 = (d3 d2 d1 d0)
		pshufd		xmm2, xmm2, 001010111b	; xmm2 = (0 0 0 carry)

		movdqu		[zdi+zax], xmm4
		add		zax, 16
		loop	@@c
@@b:	add		zcx, zdx
		jnz		@@d
		movd	zax, xmm2
        ret
@@d:	add		zsi, zax
		add		zdi, zax
		push	zbp
		movd	zdx, xmm2
		jmp		LAB_ContinueMulAddBignums
ENDIF

LAB_BeginMulAddBignums:
		push	zbp
		xor		zdx, zdx
LAB_ContinueMulAddBignums:
@@a:	lodsZ
		mov		zbp, zdx
		mul		zbx
		add		zax, zbp		
		adc		zdx, 0
		add		zax, [zdi]
		adc		zdx, 0
		stosZ
		loop	@@a
		mov		zax, zdx
		pop		zbp
        ret        
ImpMulAddBignums 	ENDP


IF X64
	ImpMulSubBignums	PROC PUBLIC USES ZBX ZSI ZDI, u:QWORD, v:QWORD, n:QWORD, q: QWORD
		mov		u, RCX
		mov		v, RDX		;!!!? was n
		mov		n, R8
		mov		q, R9
ELSE
	ImpMulSubBignums	PROC stdcall PUBLIC USES ZBX ZSI ZDI, u:DWORD, v:DWORD, n:DWORD, q:DWORD
ENDIF
		mov		ZSI, v									;!!! Contract:  lowest bit of u is 0, TODO: change thit to ECX as in ImpMulAdd
		mov		ZCX, n
		mov		ZDI, u
		mov		ZBX, q
		push	ZBP
		xor		ZBP, ZBP
		clc
		rcr		ZDI, 1
@@a:	lodsZ
		mul		ZBX
		add		ZAX, ZBP
		adc		ZDX, 0
							; CF=0 here
		rcl		ZDI, 1			
		sbb		[ZDI], ZAX
		rcr		ZDI, 1
		mov		ZBP, ZDX
		add		ZDI, ZWORD_SIZE/2
		loop	@@a
							; CF=0 here
		rcl		ZDI, 1
		sbb		[ZDI], ZBP
		setc	AL
		pop		ZBP
        ret        
ImpMulSubBignums 	ENDP


IF X64
	ImpShortDiv	PROC PUBLIC USES ZBX ZSI ZDI, s:QWORD, d:QWORD, siz:QWORD, q: QWORD
		mov		s, RCX
		mov		d, RDX
		mov		siz, R8
		mov		q, R9
ELSE
	ImpShortDiv	PROC stdcall PUBLIC USES ZBX ZSI ZDI, s:DWORD, d:DWORD, siz:DWORD, q: DWORD
ENDIF
		mov		ZCX, siz
		lea		ZSI, [ZCX*ZWORD_SIZE-ZWORD_SIZE]
		mov		ZDI, ZSI
		add		ZSI, s
		add		ZDI, d
		mov		ZBX, q
		mov		ZDX, [ZSI+ZWORD_SIZE]
		std
@@a:	lodsZ
		div		ZBX
		stosZ
		loop	@@a
		cld
		mov		ZAX, ZDX
		ret
ImpShortDiv	ENDP


IF X64
	ImpDivBignums	PROC PUBLIC USES ZBX ZSI ZDI, a:QWORD, b:QWORD, d:QWORD, n: QWORD
		mov		a, RCX
		mov		b, RDX
		mov		d, R8
		mov		n, R9
ELSE
	ImpDivBignums	PROC stdcall PUBLIC USES ZBX ZSI ZDI, a:DWORD, b:DWORD, d:DWORD, n: DWORD
ENDIF
		mov		ZCX, n
		shl		ZCX, ZWORD_BITS-2
		sub		ZSP, ZCX
		shr		ZCX, ZWORD_BITS-3
		mov		ZDI, ZSP
		xor		EAX, EAX
		rep		stosZ
		mov		ZCX, n
		shl		ZCX, ZWORD_BITS-3		
		sub		ZDI, ZCX
		mov		ZDX, n
		shl		ZDX, ZWORD_BITS
		clc
		mov		ZSI, ZSP
@@x:	mov		ZCX, n
		mov		ZBX, a
@@y:	rcl		ZWORD_PTR [ZBX], 1
		lahf
		add		ZBX, ZWORD_SIZE
		sahf
		loop	@@y
		dec		ZDX
		mov		ZCX, n
		js		@@t
		push	ZSI
@@z:	rcl		ZWORD_PTR [ZSI], 1
		lodsZ
		loop	@@z
		pop		ZSI
		mov		ZCX, n
		clc
		push	ZSI
		push	ZDI
		mov		ZBX, b
@@w:	lodsZ
		sbb		ZAX, [ZBX]
		stosZ
		lahf
		add		ZBX, ZWORD_SIZE
		sahf
		loop	@@w
		pop		ZDI
		pop		ZSI
		cmc
		jnc		@@x
		xchg	ZSI, ZDI
		jmp		@@x
@@t:	mov		ZDI, d
		rep movsZ
		lea		ZSP, [ZBP-3*ZWORD_SIZE]
		ret
ImpDivBignums	ENDP

extrn memcpy: NEAR

IF X64	
ELSE
	MemcpySse	PROC PUBLIC
	push	ecx
	push	esi
	push	edi
	mov		edi, [esp+4*4]		; dst
	mov		esi, [esp+5*4]		; src
	mov		ecx, [esp+6*4]		; cnt
	jecxz	LAB_MEMCPY
	test	ecx, 127
	jnz		LAB_MEMCPY	
	mov		eax, esi
	or		eax, edi
	test	eax, 0Fh
	jnz		LAB_MEMCPY
	shr		ecx, 7
	mov		eax, edi
LAB_LOOP:
	dotimes_i_8		< movdqa xmm_i, [esi+i*16] >
	dotimes_i_8		< movntps [edi+i*16], xmm_i >
	add		esi, 128
	add		edi, 128
	loop	LAB_LOOP
	pop		edi
	pop		esi
	pop		ecx
	ret		
LAB_MEMCPY:
	pop		edi
	pop		esi
	pop		ecx
	jmp		memcpy
MemcpySse ENDP
ENDIF


		END
		