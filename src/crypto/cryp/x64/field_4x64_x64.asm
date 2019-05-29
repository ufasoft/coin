;/*######   Copyright (c)      2018 Ufasoft  http://ufasoft.com  mailto:support@ufasoft.com,  Sergey Pavlov  mailto:dev@ufasoft.com ####
;#                                                                                                                                     #
;# 		See LICENSE for licensing information                                                                                  #
;#####################################################################################################################################*/

;;; secp256k1_fe_mul_inner
;;; secp256k1_fe_sqr_inner
;;; secp256k1_fe_4x64_negate_inner
;;; secp256k1_fe_4x64_mul_int_inner
;;; secp256k1_fe_4x64_add_inner

IFDEF __JWASM__
	OPTION LANGUAGE: C
ENDIF

.CONST


MOD_0	EQU	0FFFFFFFEFFFFFC2Fh	; following code will not work for MOD{1,2,3} != 0FFFFFFFFFFFFFFFFh
MOD_1	EQU	0FFFFFFFFFFFFFFFFh	; == MOD_2, MOD_3

MOD_RECIP	EQU	(-MOD_0)	;01000003D1h	 1/mod


.CODE


;; INPUT:  [RDX R11 R10 R9 R8]
;; OUTPUT: RDI
secp256k1_fe_normalize_after_mul_int PROC
	mov	rax, MOD_RECIP
	mul	rdx
	add	r8, rax
 	mov	[rdi+0*8], r8
	adc	r9, rdx
	mov	[rdi+1*8], r9
	adc	r10, 0
	mov	[rdi+2*8], r10
	adc	r11, 0
	mov	[rdi+3*8], r11
	jc	@@sub
	cmp	r11, MOD_1
	jb	@@ret
	cmp	r10, MOD_1
	jb	@@ret
	cmp	r9, MOD_1
	jb	@@ret
	mov	rax, MOD_0
	cmp	r8, rax
	jb	@@ret
@@sub:	mov	rax, MOD_RECIP
	add	r8, rax
	adc	r9, 0
	adc	r10, 0
	adc	r11, 0
 	mov	[rdi+0*8], r8
	mov	[rdi+1*8], r9
	mov	[rdi+2*8], r10
	mov	[rdi+3*8], r11
@@ret:	ret
secp256k1_fe_normalize_after_mul_int ENDP


secp256k1_fe_4x64_add_inner PROC PUBLIC USES RBX RDI RSI		; r:QWORD, a:QWORD
IFDEF __JWASM__
ELSE
	mov	rdi, rcx
	mov	rsi, rdx
ENDIF
	mov	r8, [rdi+0*8]
	add	r8, [rsi+0*8]
	mov	r9, [rdi+1*8]
	adc	r9, [rsi+1*8]
	mov	r10, [rdi+2*8]
	adc	r10, [rsi+2*8]
	mov	r11, [rdi+3*8]
	adc	r11, [rsi+3*8]
	mov	rdx, 0
	adc	rdx, 0
	call	secp256k1_fe_normalize_after_mul_int
	ret
secp256k1_fe_4x64_add_inner ENDP

secp256k1_fe_4x64_negate_inner PROC PUBLIC USES RBX RDI RSI	; r:QWORD, a:QWORD
IFDEF __JWASM__
ELSE
	mov	rdi, rcx
	mov	rsi, rdx
ENDIF
	mov	r8, MOD_0
	mov	r9, MOD_1
	mov	r10, r9
	mov	r11, r9
	sub	r8, [rsi+0*8]
	sbb	r9, [rsi+1*8]
	sbb	r10, [rsi+2*8]
	sbb	r11, [rsi+3*8]
	xor	rdx, rdx
	call	secp256k1_fe_normalize_after_mul_int
	ret
secp256k1_fe_4x64_negate_inner ENDP

secp256k1_fe_4x64_mul_int_inner PROC PUBLIC USES RBX RDI RSI		; r:QWORD, a:QWORD
IFDEF __JWASM__
ELSE
	mov	rdi, rcx
	mov	rsi, rdx
ENDIF
	mov	rax, [rdi+0*8]
	mul	rsi
	mov	r8, rax
	mov	r9, rdx

	mov	rax, [rdi+1*8]
	mul	rsi
	add	r9, rax
	adc	rdx, 0
	mov	r10, rdx

	mov	rax, [rdi+2*8]
	mul	rsi
	add	r10, rax
	adc	rdx, 0
	mov	r11, rdx

	mov	rax, [rdi+3*8]
	mul	rsi
	add	r11, rax
	adc	rdx, 0

	call	secp256k1_fe_normalize_after_mul_int

	ret
secp256k1_fe_4x64_mul_int_inner ENDP

secp256k1_fe_4x64_mul_inner PROC PUBLIC USES RBX RDI RSI R12 R13 R14 R15	; r:QWORD, a:QWORD, b:QWORD
	push	rbp
IFDEF __JWASM__
	push	rdi			; r
	mov	rdi, rsi		; a
	mov	rsi, rdx		; b
ELSE
	push	rcx			; r
	mov	rdi, rdx		; a
	mov	rsi, r8			; b
ENDIF
	xor	r10, r10
	xor	r11, r11
	xor	r12, r12
	xor	r13, r13
	xor	r14, r14
	xor	r15, r15

; *= a[0]
	mov	rbx, [rdi+0*8]
	mov	rax, rbx
	mul	QWORD PTR [rsi+0*8]
	mov	r8, rax
	mov	r9, rdx

	mov	rax, rbx
	mul	QWORD PTR [rsi+1*8]
	add	r9, rax
	adc	r10, rdx

	mov	rax, rbx
	mul	QWORD PTR [rsi+2*8]
	add	r10, rax
	adc	r11, rdx

	mov	rax, rbx
	mul	QWORD PTR [rsi+3*8]
	add	r11, rax
	adc	r12, rdx

; *= a[1]
	mov	rbx, [rdi+1*8]
	mov	rax, rbx
	mul	QWORD PTR [rsi+0*8]
	add	r9, rax
	adc	r10, rdx
	adc	r11, 0
	adc	r12, 0
	adc	r13, 0

	mov	rax, rbx
	mul	QWORD PTR [rsi+1*8]
	add	r10, rax
	adc	r11, rdx
	adc	r12, 0
	adc	r13, 0

	mov	rax, rbx
	mul	QWORD PTR [rsi+2*8]
	add	r11, rax
	adc	r12, rdx
	adc	r13, 0

	mov	rax, rbx
	mul	QWORD PTR [rsi+3*8]
	add	r12, rax
	adc	r13, rdx

; *= a[2]
	mov	rbx, [rdi+2*8]
	mov	rax, rbx
	mul	QWORD PTR [rsi+0*8]
	add	r10, rax
	adc	r11, rdx
	adc	r12, 0
	adc	r13, 0
	adc	r14, 0

	mov	rax, rbx
	mul	QWORD PTR [rsi+1*8]
	add	r11, rax
	adc	r12, rdx
	adc	r13, 0
	adc	r14, 0

	mov	rax, rbx
	mul	QWORD PTR [rsi+2*8]
	add	r12, rax
	adc	r13, rdx
	adc	r14, 0

	mov	rax, rbx
	mul	QWORD PTR [rsi+3*8]
	add	r13, rax
	adc	r14, rdx

; *= a[3]
	mov	rbx, [rdi+3*8]
	mov	rax, rbx
	mul	QWORD PTR [rsi+0*8]
	add	r11, rax
	adc	r12, rdx
	adc	r13, 0
	adc	r14, 0
	adc	r15, 0

	mov	rax, rbx
	mul	QWORD PTR [rsi+1*8]
	add	r12, rax
	adc	r13, rdx
	adc	r14, 0
	adc	r15, 0

	mov	rax, rbx
	mul	QWORD PTR [rsi+2*8]
	add	r13, rax
	adc	r14, rdx
	adc	r15, 0

	mov	rax, rbx
	mul	QWORD PTR [rsi+3*8]
	add	r14, rax
	adc	r15, rdx

	mov	rcx, MOD_RECIP	; 1/mod
	mov	rax, r12
	mul	rcx
	add	r8, rax
	adc	r9, rdx
	adc	r10, 0
	adc	r11, 0
	mov	r12, 0
	adc	r12, 0

	mov	rax, r13
	mul	rcx
	add	r9, rax
	adc	r10, rdx
	adc	r11, 0
	adc	r12, 0

	mov	rax, r14
	mul	rcx
	add	r10, rax
	adc	r11, rdx
	adc	r12, 0

	mov	rax, r15
	mul	rcx
	add	r11, rax
	adc	r12, rdx

	mov	rax, r12
	mul	rcx
	add	r8, rax
	adc	r9, rdx
	adc	r10, 0
	adc	r11, 0
	mov	rdx, 0
	adc	rdx, rdx

	pop	rdi			; r
	call	secp256k1_fe_normalize_after_mul_int

	pop	rbp
	ret
secp256k1_fe_4x64_mul_inner ENDP

secp256k1_fe_4x64_sqr_inner	PROC PUBLIC
IFDEF __JWASM__
	mov	rdx, rsi	; b = a
ELSE
	mov	r8, rdx		; b = a
ENDIF
	jmp	secp256k1_fe_4x64_mul_inner
secp256k1_fe_4x64_sqr_inner	ENDP

END
