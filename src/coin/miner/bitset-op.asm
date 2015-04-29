;/*######   Copyright (c) 2014-2015 Ufasoft  http://ufasoft.com  mailto:support@ufasoft.com,  Sergey Pavlov  mailto:dev@ufasoft.com ####
;#                                                                                                                                     #
;# 		See LICENSE for licensing information                                                                                         #
;#####################################################################################################################################*/

INCLUDE el/x86x64.inc

OPTION_LANGUAGE_C

IF X64
ELSE
.XMM
ENDIF


.CODE


IF X64
IFDEF __JWASM__			; UNIX calling convention
	BitsetPeriodicSetBts	PROC PUBLIC USES RBX R8 R9 R10 R11
	bitset	EQU R8
	sz		EQU R9
	idx 	EQU R10
	period	EQU R11
	mov		bitset, rdi
	mov		sz, rsi
	mov		idx, rdx
	mov		period, rcx
ELSE
	BitsetPeriodicSetBts	PROC PUBLIC USES ZBX, bitset:QWORD, sz:QWORD, idx:QWORD, period:QWORD
	mov		bitset, rcx
	mov		sz, rdx
	mov		idx, r8
	mov		period, r9
ENDIF
ELSE
	BitsetPeriodicSetBts	PROC PUBLIC USES EBX ECX EDX, bitset:DWORD, sz:DWORD, idx:DWORD, period:DWORD
ENDIF
	mov		zbx, period
	mov		zcx, sz
	mov		zax, zcx
	mov		zdx, 0
	sub		zax, idx
	div		ebx							; 32-bit period
	cmp		zdx, 0
	jne		$SkipSubPeriod
	sub		zcx, zbx
$SkipSubPeriod:
	sub		zcx, zdx
	mov		zdx, bitset
$Loop:	
	bts		[zdx], zcx
	sub		zcx, zbx
	jae		$Loop
	ret
BitsetPeriodicSetBts	ENDP


IF X64
IFDEF __JWASM__			; UNIX calling convention
	BitsetPeriodicSetOr	PROC PUBLIC USES RBX R8 R9 R10 R11
	bitset	EQU R8
	sz		EQU R9
	idx 	EQU R10
	period	EQU R11
	mov		bitset, rdi
	mov		sz, rsi
	mov		idx, rdx
	mov		period, rcx
ELSE
	BitsetPeriodicSetOr	PROC PUBLIC USES ZBX ZSI, bitset:QWORD, sz:QWORD, idx:QWORD, period:QWORD
	mov		bitset, rcx
	mov		sz, rdx
	mov		idx, r8
	mov		period, r9
ENDIF
ELSE
	BitsetPeriodicSetOr	PROC PUBLIC USES EBX ECX EDX ZSI, bitset:DWORD, sz:DWORD, idx:DWORD, period:DWORD
ENDIF
	mov		zbx, period
	mov		zsi, sz
	mov		zax, zsi
	mov		zdx, 0
	sub		zax, idx
	div		ebx							; 32-bit period
	cmp		zdx, 0
	jne		$SkipSubPeriod
	sub		zsi, zbx
$SkipSubPeriod:
	sub		zsi, zdx
	mov		zcx, zsi
	mov		ch, 1
	and		cl, 7
	shl		ch, cl
	mov		cl, bl
	and		cl, 7
	mov		zdx, bitset
	mov		zax, zsi
$Loop:
	shr		zax, 3
	or		byte ptr [zdx+zax], ch
	ror		ch, cl
	sub		zsi, zbx
	mov		zax, zsi
	jae		$Loop
	ret
BitsetPeriodicSetOr	ENDP


IF X64
IFDEF __JWASM__			; UNIX calling convention
	BitsetOr	PROC PUBLIC USES ZBX R9 R10 R11 R12
	LOCAL 	off:QWORD
	dst		EQU R9
	bitset	EQU R10
	n 		EQU R11
	prime	EQU R12

	mov		dst, rdi
	mov		bitset, rsi
	mov		n, rdx
	mov		prime, rcx
	mov		off, r8
ELSE
	BitsetOr	PROC PUBLIC USES ZBX ZSI ZDI, dst:QWORD, bitset:QWORD, n:QWORD, prime:QWORD, off:QWORD
	mov		dst, rcx
	mov		bitset, rdx
	mov		n, r8
	mov		prime, r9
ENDIF
ELSE
	BitsetOr	PROC PUBLIC USES EBX ECX EDX ESI EDI, dst:DWORD, bitset:DWORD, n:DWORD, prime:DWORD, off:DWORD
ENDIF
	mov		zdi, dst
	mov		zbx, 0
$Outer:	
	mov		zsi, bitset
	mov		zcx, prime
	cmp		n, zcx
	jae		$SubN
	mov		zcx, n
$SubN:
	sub		n, zcx
	dec		zcx
	shl		zcx, 8
	mov		cl, byte ptr off
$Inner:
	lodsZ
	mov		zdx, zax
	shld	zax, zbx, cl
	or		zax, [zdi]
	mov		zbx, zdx
	sub		zcx, 256
	stosZ
	jae		$Inner
	cmp		n, 0
	jne		$Outer	
	ret
BitsetOr	ENDP


		END




