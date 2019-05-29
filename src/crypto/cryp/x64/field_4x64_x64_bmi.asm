;/*######   Copyright (c)      2018 Ufasoft  http://ufasoft.com  mailto:support@ufasoft.com,  Sergey Pavlov  mailto:dev@ufasoft.com ####
;#                                                                                                                                     #
;# 		See LICENSE for licensing information                                                                                  #
;#####################################################################################################################################*/


;;; BMI2 implementation (using MULX instruction)
;;;   secp256k1_fe_mul_inner
;;;   secp256k1_fe_sqr_inner
;;;   secp256k1_fe_4x64_negate_inner
;;;   secp256k1_fe_4x64_mul_int_inner
;;;   secp256k1_fe_4x64_add_inner

IFDEF __JWASM__
	OPTION LANGUAGE: C
ENDIF

MOD_0	EQU	0FFFFFFFEFFFFFC2Fh	; following code will not work for MOD{1,2,3} != 0FFFFFFFFFFFFFFFFh
MOD_1	EQU	0FFFFFFFFFFFFFFFFh	; == MOD_2, MOD_3

MOD_RECIP	EQU	(-MOD_0)	;01000003D1h	 1/mod


.CODE


;; INPUT:  [RCX R11 R10 R9 R8]
;; RDX: MOD_RECIP
;; OUTPUT: RDI
secp256k1_fe_normalize_after_mul_int_bmi2 PROC
	mulx	rcx, rax, rcx
	add	r8, rax
 	mov	[rdi+0*8], r8
	adc	r9, rcx
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
@@sub:	add	r8, rdx
	adc	r9, 0
	adc	r10, 0
	adc	r11, 0
 	mov	[rdi+0*8], r8
	mov	[rdi+1*8], r9
	mov	[rdi+2*8], r10
	mov	[rdi+3*8], r11
@@ret:	ret
secp256k1_fe_normalize_after_mul_int_bmi2 ENDP

secp256k1_fe_4x64_add_inner_bmi2 PROC PUBLIC USES RDI		; r:QWORD, a:QWORD
IFDEF __JWASM__
	mov	rdx, rsi
ELSE
	mov	rdi, rcx
ENDIF
	mov	r8, [rdi+0*8]
	mov	r9, [rdi+1*8]
	mov	r10, [rdi+2*8]
	mov	r11, [rdi+3*8]
	add	r8, [rdx+0*8]
	adc	r9, [rdx+1*8]
	adc	r10, [rdx+2*8]
	adc	r11, [rdx+3*8]
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
@@sub: 	mov	rax, MOD_RECIP
	add	r8, rax
	adc	r9, 0
	adc	r10, 0
	adc	r11, 0
@@ret: 	mov	[rdi+0*8], r8
	mov	[rdi+1*8], r9
	mov	[rdi+2*8], r10
	mov	[rdi+3*8], r11
	ret
secp256k1_fe_4x64_add_inner_bmi2 ENDP

secp256k1_fe_4x64_negate_inner_bmi2 PROC PUBLIC USES RDI RSI	; r:QWORD, a:QWORD
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
	mov	[rdi+0*8], r8
	sbb	r9, [rsi+1*8]
	mov	[rdi+1*8], r9
	sbb	r10, [rsi+2*8]
	mov	[rdi+2*8], r10
	sbb	r11, [rsi+3*8]
	mov	[rdi+3*8], r11
	cmp	r11, MOD_1
	jb	@@ret
	cmp	r10, MOD_1
	jb	@@ret
	cmp	r9, MOD_1
	jb	@@ret
	mov	rax, MOD_0
	cmp	r8, rax
	jb	@@ret
	xor	r8, r8
	xor	r9, r9
	xor	r10, r10
	xor	r11, r12
	mov	[rdi+0*8], r8
	mov	[rdi+1*8], r9
	mov	[rdi+2*8], r10
	mov	[rdi+3*8], r11
@@ret:	ret
secp256k1_fe_4x64_negate_inner_bmi2 ENDP

secp256k1_fe_4x64_mul_int_inner_bmi2 PROC PUBLIC USES RDI	; r:QWORD, a:QWORD
IFDEF __JWASM__
	mov	rdx, rsi
ELSE
	mov	rdi, rcx
ENDIF
	mulx	rax, r8, [rdi+0*8]
	mulx	rcx, r9, [rdi+1*8]
	mulx	r11, r10, [rdi+2*8]
	add	r9, rax
	adc	r10, rcx
	mulx	rcx, rax, [rdi+3*8]
	mov	rdx, MOD_RECIP
	adc	r11, rax
	adc	rcx, 0

	call	secp256k1_fe_normalize_after_mul_int_bmi2

	ret
secp256k1_fe_4x64_mul_int_inner_bmi2 ENDP

secp256k1_fe_4x64_mul_inner_bmi2 PROC PUBLIC USES RBX RBP RDI RSI R12 R13 R14 R15	; r:QWORD, a:QWORD, b:QWORD
IFDEF __JWASM__
	push	rdi			; r
	mov	rdi, rsi		; a
	mov	rsi, rdx		; b

	TMP_N0	EQU	[rsp-1*8]
	TMP_N1	EQU	[rsp-2*8]
ELSE
	push	rcx			; r
	mov	rdi, rdx		; a
	mov	rsi, r8			; b

	TMP_N0	EQU	[rsp+10*8]
	TMP_N1	EQU	[rsp+11*8]
ENDIF
	mov	rdx, [rdi+0*8]
	mov	r8, [rsi+0*8]
	mov	r9, [rsi+1*8]

	mulx	r14, rax, r8
	mov	r10, [rsi+2*8]
	mulx	r15, rcx, r9
	mov	TMP_N0, rax
	mov	r11, [rsi+3*8]
	mulx	r12, rsi, r10
	add	r14, rcx
	mulx	r13, rdx, r11
	adc	r15, rsi
	adc	r12, rdx
	mov	rdx, [rdi+1*8]
	mulx	rbx, rax, r8
	adc	r13, 0

	add	rax, r14
	mulx	rsi, rcx, r9
	mov	TMP_N1, rax
	adc	r15, rbx
	mulx	rbp, rax, r10
	adc	r12, rsi
	mulx	r14, rdx, r11
	adc	r13, rbp
	adc	r14, 0
	add	r15, rcx
	adc	r12, rax
	adc	r13, rdx
	adc	r14, 0

	mov	rdx, [rdi+2*8]
	mulx	rbx, rsi, r8
	add	rsi, r15
	mulx	rax, rcx, r9
	adc	r12, rbx
	adc	r13, rax
	mulx	rbp, rax, r10
	mulx	r15, rdx, r11
	adc	r14, rbp
	adc	r15, 0
	add	r12, rcx
	adc	r13, rax
	adc	r14, rdx
	mov	rdx, [rdi+3*8]
	mulx	r8, rax, r8
	adc	r15, 0
	add	rax, r12
	mulx	r9, rbx, r9
	adc	r13, r8
	mulx	r10, rcx, r10
	adc	r14, r9
	mulx	rdi, rbp, r11
	mov	rdx, MOD_RECIP
	adc	r15, r10
	adc	rdi, 0
	add	r13, rbx
	adc	r14, rcx
	mulx	r13, r8, r13
	adc	r15, rbp
	mulx	r14, r9, r14
	adc	rdi, 0
	add	r8, TMP_N0
	mulx	r15, r10, r15
	adc	r9, TMP_N1
	mulx	rcx, r11, rdi
	adc	r10, rsi
	pop	rdi
	adc	r11, rax
	adc	rcx, 0

	add	r9, r13
	adc	r10, r14
	adc	r11, r15
	adc	rcx, 0

	call	secp256k1_fe_normalize_after_mul_int_bmi2

	ret
secp256k1_fe_4x64_mul_inner_bmi2 ENDP

secp256k1_fe_4x64_sqr_inner_bmi2	PROC PUBLIC USES RBX RBP RDI RSI R12 R13 R14 R15 ; r:QWORD, a:QWORD
IFDEF __JWASM__
	push	rdi				; r
	mov	rdi, rsi			; a
ELSE
	push	rcx				; r
	mov	rdi, rdx			; a
ENDIF
	mov	rdx, [rdi+0*8]
	mov	r13, [rdi+1*8]
	mov	rbx, [rdi+2*8]
	mov	r15, [rdi+3*8]

	mulx	r9, r8, rdx
	mulx	r10, rax, r13
	mulx	r11, r14, rbx
	mulx	r12, rcx, r15
	add	r10, r14
	mov	rdx, r13
	mulx	rdi, rsi, rbx
	adc	r11, rcx
	mulx	r13, rbp, r15
	adc	r12, rdi
	adc	r13, 0
	add	r11, rsi
	mulx	rdi, rsi, rdx
	adc	r12, rbp
	mov	rdx, rbx
	mulx	r14, rbp, r15
	adc	r13, rbp
	adc	r14, 0
	mov	rbp, 0
	shld	rbp, r14, 1
	shld	r14, r13, 1
	shld	r13, r12, 1
	shld	r12, r11, 1
	shld	r11, r10, 1
	shld	r10, rax, 1
	shl	rax, 1
	add	r9, rax
	adc	r10, rsi
	adc	r11, rdi
	mulx	rdi, rsi, rdx
	mov	rdx, r15
	adc	r12, rsi
	mulx	r15, rbx, rdx
	adc	r13, rdi
	mov	rdx, MOD_RECIP
	adc	r14, rbx
	mulx	r12, rax, r12
	adc	r15, rbp
	add	r8, rax
	mulx	r13, rbx, r13
	adc	r9, rbx
	mulx	r14, rsi, r14
	adc	r10, rsi
	adc	r11, r14
	mulx	rcx, r15, r15
	adc	rcx, 0
	add	r9, r12
	adc	r10, r13
	adc	r11, r15
	adc	rcx, 0

	pop	rdi			; r
	call	secp256k1_fe_normalize_after_mul_int_bmi2

	ret
secp256k1_fe_4x64_sqr_inner_bmi2	ENDP

END
