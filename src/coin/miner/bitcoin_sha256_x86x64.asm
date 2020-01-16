;/*######   Copyright (c) 2014-2018 Ufasoft  http://ufasoft.com  mailto:support@ufasoft.com,  Sergey Pavlov  mailto:dev@ufasoft.com ####
;#                                                                                                                                     #
;# 		See LICENSE for licensing information                                                                                         #
;#####################################################################################################################################*/

; SHA-256 CPU x86/x64/SSE2 cruncher for Bincoin Miner

INCLUDE el/x86x64.inc

OPTION_LANGUAGE_C

IF X64
ELSE
.XMM
ENDIF


EXTRN s_pSha256_hinit:PTR BYTE
EXTRN s_pSha256_k:PTR BYTE


.CODE

IF X64
IFDEF __JWASM__			; UNIX calling convention
	w		EQU r9
	midstate	EQU r10
	init		EQU r11
	npar		EQU r12
	pnonce		EQU r13


	CalcSha256	PROC PUBLIC USES ZBX ZSI ZDI R9 R10 R11 R12 R13 R14
	mov	w, rdi
	mov	midstate, rsi
	mov	init, rdx
	mov	npar, rcx
	mov	pnonce, r8
ELSE
	CalcSha256	PROC PUBLIC USES ZBX ZSI ZDI R14, w:QWORD, midstate:QWORD, init:QWORD, npar:QWORD, pnonce:QWORD
	mov	w, rcx
	mov	midstate, rdx
	mov	init, r8
	mov	npar, r9
ENDIF

	mov	r14, s_pSha256_k
ELSE
	CalcSha256	PROC PUBLIC USES EBX ECX EDX ESI EDI, w:DWORD, midstate:DWORD, init:DWORD, npar:DWORD, pnonce:DWORD
ENDIF

LAB_NEXT_NONCE:
	mov	zsi, init

	mov	zbx, w
	mov	zax, pnonce
	mov	eax, [zax]
	mov	[zbx+3*4], eax

	mov	zcx, 64
	mov	zax, 18
	mov	zdi, 3
LAB_SHA:
	push	zbx
	push	zcx
	push	zdi

	push	zsi
	lea	zsi, [zbx+zax*4]
	sub	zcx, zax
LAB_CALC:
	mov	ebx, [zsi-16*4]
	add	ebx, [zsi-7*4]

	mov	eax, [zsi-15*4]			; (Rotr32(w_15, 7) ^ Rotr32(w_15, 18) ^ (w_15 >> 3))
	mov	edx, eax
	shr	edx, 3
	ror	eax, 7
	xor	edx, eax
	ror	eax, 11
	xor	edx, eax
	add	ebx, edx

	mov	eax, [zsi-2*4]			; (Rotr32(w_2, 17) ^ Rotr32(w_2, 19) ^ (w_2 >> 10))
	mov	edx, eax
	shr	edx, 10
	ror	eax, 17
	xor	edx, eax
	ror	eax, 2
	xor	edx, eax
	add	ebx, edx

	mov	[zsi], ebx
	add	zsi, 4
	loop	LAB_CALC

	pop	zsi

	mov	zdx, [zsp+ZWORD_SIZE*1]  ; nTo
	sub	zdx, [zsp]				 ; nFrom
	mov	zdi, zsp
	lea	zsp, [zsp+zdx*ZWORD_SIZE-(5+64)*ZWORD_SIZE]	;	max 64 zwords
	lodsd
	mov	[zsp+0*ZWORD_SIZE], eax
	lodsd
	mov	[zsp+1*ZWORD_SIZE], eax
	lodsd
	mov	[zsp+2*ZWORD_SIZE], eax
	lodsd
	mov	[zsp+3*ZWORD_SIZE], eax
	lodsd
	mov	[zsp+4*ZWORD_SIZE], eax
	lodsd
	mov	[zsp+5*ZWORD_SIZE], eax
	lodsd
	mov	[zsp+6*ZWORD_SIZE], eax
	lodsd
	mov	[zsp+7*ZWORD_SIZE], eax
	mov	zsi, [zdi + ZWORD_SIZE*2]
	mov	zcx, [zdi + ZWORD_SIZE*1]
	mov	zdi, [zdi]
LAB_LOOP:
	;; T t1 = h + (Rotr32(e, 6) ^ Rotr32(e, 11) ^ Rotr32(e, 25)) + ((e & f) ^ AndNot(e, g)) + Expand32<T>(g_sha256_k[j]) + w[j];
	push	zcx

	mov	eax, [zsp+4*ZWORD_SIZE+ZWORD_SIZE*1]	; e
	mov	edx, eax
	not	edx							; ~e

	and	edx, [zsp+6*ZWORD_SIZE+ZWORD_SIZE*1]	; ~e & g

	mov	ebx, [zsp+5*ZWORD_SIZE+ZWORD_SIZE*1]	; f
	and	ebx, eax					; (e & f)

	xor	edx, ebx					; (e & f) ^ AndNot(e, g)

	add	edx, [zsi+zdi*4]

IF X64
	add	edx, DWORD PTR [r14+zdi*4]
ELSE
	mov	ebx, s_pSha256_k
	add	edx, DWORD PTR [ebx+edi*4]
ENDIF
	inc	zdi
	add	edx, [zsp+7*ZWORD_SIZE+ZWORD_SIZE*1]	; +h

	ror	eax, 6				; Rotr32(e, 6) ^ Rotr32(e, 11) ^ Rotr32(e, 25)
	mov	ebx, eax
	ror	eax, 5
	xor	ebx, eax
	ror	eax, 14
	xor	ebx, eax

	add	edx, ebx			; t1

	add	[zsp+3*ZWORD_SIZE+ZWORD_SIZE*1], edx	; e = d+t1

	mov	eax, [zsp+0*ZWORD_SIZE+ZWORD_SIZE*1]	; a
	mov	ebx, eax
	mov	ecx, eax

	xor	ebx, [zsp+1*ZWORD_SIZE+ZWORD_SIZE*1]	; a^b
	xor	ecx, [zsp+2*ZWORD_SIZE+ZWORD_SIZE*1]	; a^c
	and	ebx, ecx
	xor	ebx, eax					; Ma(a, b, c)
	add	edx, ebx					; t1 += Ma(a, b, c)

	ror	eax, 2
	mov	ecx, eax
	ror	eax, 11
	xor	ecx, eax
	ror	eax, 9
	xor	ecx, eax
	add	edx, ecx

	pop	zcx
	push	zdx

	cmp	zdi, zcx
	jb	LAB_LOOP

	cmp	zcx, 64
	jne	LAB_DONE

	mov	zdi, w
	add	zdi, 64*4
	mov	zbx, zdi
	mov	zsi, midstate

	lodsd
	add	eax, DWORD PTR [zsp+0*ZWORD_SIZE]
	stosd
	lodsd
	add	eax, DWORD PTR [zsp+1*ZWORD_SIZE]
	stosd
	lodsd
	add	eax, DWORD PTR [zsp+2*ZWORD_SIZE]
	stosd
	lodsd
	add	eax, DWORD PTR [zsp+3*ZWORD_SIZE]
	stosd
	lodsd
	add	eax, DWORD PTR [zsp+4*ZWORD_SIZE]
	stosd
	lodsd
	add	eax, DWORD PTR [zsp+5*ZWORD_SIZE]
	stosd
	lodsd
	add	eax, DWORD PTR [zsp+6*ZWORD_SIZE]
	stosd
	lodsd
	add	eax, DWORD PTR [zsp+7*ZWORD_SIZE]
	stosd

	lea	zsp, [zsp+(64+8)*ZWORD_SIZE]

	mov	zcx, 61
	mov	zax, 16
	xor	zdi, zdi
	mov	zsi, s_pSha256_hinit
	jmp	LAB_SHA
LAB_DONE:
	mov	eax, [zsp+4*ZWORD_SIZE]		; +e
	lea	zsp, [zsp+(64+8)*ZWORD_SIZE]
	mov	zbx, s_pSha256_hinit
	add	eax, DWORD PTR [zbx + 7*4]
	jz	LAB_RET
	mov	zbx, pnonce
	inc	DWORD PTR [zbx]
	dec	npar
	jnz	LAB_NEXT_NONCE
	mov	eax, -1
LAB_RET:
	inc	eax
	ret
CalcSha256	ENDP

EXTRN s_p4Sha256_k:PTR BYTE

IF X64
IFDEF __JWASM__			; UNIX calling convention
	CalcSha256Sse	PROC PUBLIC USES ZBX ZSI ZDI R9 R10 R11 R12 R13 R15, x:QWORD	; x - just for generating frame
	mov	rax, x
	mov	w, rdi
	mov	midstate, rsi
	mov	init, rdx
	mov	npar, rcx
	mov	pnonce, r8
ELSE
	CalcSha256Sse	PROC PUBLIC USES ZBX ZSI ZDI R9 R10 R11 R12 R13 R15, w:QWORD, midstate:QWORD, init:QWORD, npar:QWORD, pnonce:QWORD
	mov	w, rcx
	mov	midstate, rdx
	mov	init, r8
	mov	npar, r9
ENDIF
	mov	r15, s_p4sha256_k
ELSE
	CalcSha256Sse	PROC PUBLIC USES EBX ECX EDX ESI EDI, w:DWORD, midstate:DWORD, init:DWORD, npar:DWORD, pnonce:DWORD
ENDIF

	sub	ZSP, 9*16
	and	ZSP, NOT 63

LAB_NEXT_NONCE:
	mov	zdx, init

	mov	zbx, w
	mov	zax, pnonce
	mov	eax, [zax]
	mov	[zbx+3*16], eax
	inc	eax
	mov	[zbx+3*16+4], eax
	inc	eax
	mov	[zbx+3*16+8], eax
	inc	eax
	mov	[zbx+3*16+12], eax

	mov	zcx, 64*4
	mov	zax, 18*4
	mov	zdi, 3*4
LAB_SHA:
	push	zcx
	lea	zsi, [zbx+zax*4]
	lea	zcx, [zbx+zcx*4]
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

	add	zsi, 16
	movdqa	[zsi-16], xmm0
	cmp	zsi, zcx
	jb	LAB_CALC
	pop	zcx

	mov	zax, zdi	; nFrom
	mov	zsi, zbx

	movd	xmm0, DWORD PTR [zdx+4*4]		; xmm0 == e
	movd	xmm3, DWORD PTR [zdx+3*4]		; xmm3 == d
	movd	xmm4, DWORD PTR [zdx+2*4]		; xmm4 == c
	movd	xmm5, DWORD PTR [zdx+1*4]		; xmm5 == b
	movd	xmm7, DWORD PTR [zdx+0*4]		; xmm7 == a
	pshufd  xmm0, xmm0, 0
	pshufd  xmm3, xmm3, 0
	pshufd  xmm4, xmm4, 0
	pshufd  xmm5, xmm5, 0
	pshufd  xmm7, xmm7, 0
IF X64
	movd	xmm8, DWORD PTR [zdx+5*4]		; xmm8 == f
	movd	xmm9, DWORD PTR [zdx+6*4]		; xmm9 == g
	movd	xmm10, DWORD PTR [zdx+7*4]		; xmm10 == h
	pshufd  xmm8, xmm8, 0
	pshufd  xmm9, xmm9, 0
	pshufd  xmm10, xmm10, 0
ELSE
	movd	xmm1, DWORD PTR [zdx+5*4]
	movd	xmm2, DWORD PTR [zdx+6*4]
	movd	xmm6, DWORD PTR [zdx+7*4]
	pshufd  xmm1, xmm1, 0
	pshufd  xmm2, xmm2, 0
	pshufd  xmm6, xmm6, 0
	movdqa	[zsp+5*16], xmm1
	movdqa	[zsp+6*16], xmm2
	movdqa	[zsp+7*16], xmm6
ENDIF

	movdqa	xmm6, xmm4
	pxor	xmm6, xmm5
	movdqa	[zsp+8*16], xmm6		; b ^ c
LAB_LOOP:

	;; T t1 = h + (Rotr32(e, 6) ^ Rotr32(e, 11) ^ Rotr32(e, 25)) + ((e & f) ^ AndNot(e, g)) + Expand32<T>(g_sha256_k[j]) + w[j];
	movdqa	xmm6, [zsi+zax*4]

IF X64
	paddd	xmm6, xmm10				; +h
ELSE
	paddd	xmm6, [zsp+7*16]		; +h
ENDIF

	movdqa	xmm1, xmm0
IF X64
	movdqa	xmm2, xmm9
	paddd	xmm6, OWORD PTR [r15+zax*4]
ELSE
	movdqa	xmm2, [zsp+6*16]
	mov	zdx, s_p4Sha256_k
	paddd	xmm6, OWORD PTR [zdx + zax*4]
ENDIF
	pandn	xmm1, xmm2				; ~e & g

	add	zax, 4

IF X64
	movdqa	xmm10, xmm2				; h = g
	movdqa	xmm2, xmm8				; f
	movdqa	xmm9, xmm2				; g = f
ELSE
	movdqa	[zsp+7*16], xmm2		; h = g
	movdqa	xmm2, [zsp+5*16]		; f
	movdqa	[zsp+6*16], xmm2		; g = f
ENDIF

	pand	xmm2, xmm0				; e & f
	pxor	xmm1, xmm2				; (e & f) ^ (~e & g)
IF X64
	movdqa	xmm8, xmm0				; f = e
ELSE
	movdqa	[zsp+5*16], xmm0		; f = e
ENDIF
	paddd	xmm6, xmm1

	movdqa	xmm1, xmm0
	psrld	xmm0, 6
	movdqa	xmm2, xmm0
	pslld	xmm1, 7
	psrld	xmm2, 5
	pxor	xmm0, xmm1
	pxor	xmm0, xmm2
	pslld	xmm1, 14
	psrld	xmm2, 14
	pxor	xmm0, xmm1
	pxor	xmm0, xmm2
	pslld	xmm1, 5
	pxor	xmm0, xmm1				; Rotr32(e, 6) ^ Rotr32(e, 11) ^ Rotr32(e, 25)
	paddd	xmm6, xmm0				; xmm6 = t1

	movdqa	xmm0, xmm3			; d
	paddd	xmm0, xmm6			; e = d+t1

	movdqa	xmm3, xmm4			; d = c
	movdqa	xmm4, xmm5			; c = b

	pxor	xmm5, xmm7			; a ^ b
	movdqa	xmm2, xmm5
	pand	xmm5, [zsp+8*16]
	movdqa	[zsp+8*16], xmm2
	pxor	xmm5, xmm4			; maj(a, b, c)

	paddd	xmm6, xmm5			; t1 +  maj(a, b, c)
	movdqa	xmm5, xmm7			; b = a

	movdqa	xmm2, xmm7
	psrld	xmm7, 2
	movdqa	xmm1, xmm7
	pslld	xmm2, 10
	psrld	xmm1, 11
	pxor	xmm7, xmm2
	pxor	xmm7, xmm1
	pslld	xmm2, 9
	psrld	xmm1, 9
	pxor	xmm7, xmm2
	pxor	xmm7, xmm1
	pslld	xmm2, 11
	pxor	xmm7, xmm2
	paddd	xmm7, xmm6			; a = t1 + (Rotr32(a, 2) ^ Rotr32(a, 13) ^ Rotr32(a, 22)) + maj(a, b, c);

	cmp	zax, zcx
	jb	LAB_LOOP

	cmp	zcx, 64*4
	jne	LAB_DONE

	add	zbx, 64*16
	mov	zsi, midstate

	paddd	xmm7, [zsi]
	paddd	xmm5, [zsi+16]
	paddd	xmm4, [zsi+2*16]
	paddd	xmm3, [zsi+3*16]
	paddd	xmm0, [zsi+4*16]

	movdqa	[zbx+0*16], xmm7
	movdqa	[zbx+16], xmm5
	movdqa	[zbx+2*16], xmm4
	movdqa	[zbx+3*16], xmm3
	movdqa	[zbx+4*16], xmm0
IF X64
	paddd	xmm8, [zsi+5*16]
	paddd	xmm9, [zsi+6*16]
	paddd	xmm10, [zsi+7*16]

	movdqa	[zbx+5*16], xmm8
	movdqa	[zbx+6*16], xmm9
	movdqa	[zbx+7*16], xmm10
ELSE
	movdqa	xmm3, [zsp+5*16]
	movdqa	xmm4, [zsp+6*16]
	movdqa	xmm5, [zsp+7*16]

	paddd	xmm3, [zsi+5*16]
	paddd	xmm4, [zsi+6*16]
	paddd	xmm5, [zsi+7*16]

	movdqa	[zbx+5*16], xmm3
	movdqa	[zbx+6*16], xmm4
	movdqa	[zbx+7*16], xmm5
ENDIF
	mov	zcx, 61*4
	mov	zax, 16*4
	xor	zdi, zdi
	mov	zdx, s_pSha256_hinit
	jmp	LAB_SHA
LAB_DONE:
	mov	zax, s_pSha256_hinit
	movd	xmm1, DWORD PTR [zax + 7*4]
	pshufd  xmm1, xmm1, 0
	paddd	xmm0, xmm1
	pxor	xmm1, xmm1
	pcmpeqd xmm0, xmm1
	movdqa	[zsp], xmm0
	mov	eax, [zsp]
	or	eax, [zsp+4]
	or	eax, [zsp+8]
	or	eax, [zsp+12]
	jz	LAB_NO_ZERO
	mov	eax, 1
	test	BYTE PTR [zsp], 1
	jnz	LAB_RET
	mov	zbx, pnonce
	inc	DWORD PTR [zbx]
	test	BYTE PTR [zsp+4], 1
	jnz	LAB_RET
	inc	DWORD PTR [zbx]
	test	BYTE PTR [zsp+8], 1
	jnz	LAB_RET
	inc	DWORD PTR [zbx]
	jmp	LAB_RET
LAB_NO_ZERO:
	mov	zbx, pnonce
	add	DWORD PTR [zbx], 4
	sub	npar, 4
	jnz	LAB_NEXT_NONCE
	xor	eax, eax
LAB_RET:

IF X64
	lea	ZSP, [ZBP - ZWORD_SIZE*9]
ELSE
	lea	ZSP, [ZBP - ZWORD_SIZE*5]
ENDIF

	ret
CalcSha256Sse	ENDP

END
