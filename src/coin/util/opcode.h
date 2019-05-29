/*######   Copyright (c) 2013-2019 Ufasoft  http://ufasoft.com  mailto:support@ufasoft.com,  Sergey Pavlov  mailto:dev@ufasoft.com ####
#                                                                                                                                     #
# 		See LICENSE for licensing information                                                                                         #
#####################################################################################################################################*/

#pragma once

namespace Coin {

enum class Opcode : uint8_t {
	// push value
	OP_0 = 0
	, OP_FALSE = OP_0

	, OP_PUSHDATA1 = 0x4C
	, OP_PUSHDATA2 = 0x4D
	, OP_PUSHDATA4 = 0x4E
	, OP_1NEGATE	= 0x4F
	, OP_RESERVED	= 0x50
	, OP_1			= 0x51
	, OP_TRUE	= OP_1
	, OP_2			= 0x52
	, OP_3			= 0x53
	, OP_4
	, OP_5
	, OP_6
	, OP_7
	, OP_8
	, OP_9			= 0x59
	, OP_10
	, OP_11
	, OP_12
	, OP_13
	, OP_14
	, OP_15
	, OP_16			= 0x60

	// control
    , OP_NOP		= 0x61
	, OP_VER		= 0x62
	, OP_IF			= 0x63
	, OP_NOTIF
	, OP_VERIF
	, OP_VERNOTIF
	, OP_ELSE		= 0x67
	, OP_ENDIF		= 0x68
	, OP_VERIFY		= 0x69
	, OP_RETURN		= 0x6A

    // stack ops
    , OP_TOALTSTACK = 0x6B
	, OP_FROMALTSTACK
	, OP_2DROP
	, OP_2DUP
	, OP_3DUP
	, OP_2OVER		= 0x70
	, OP_2ROT
	, OP_2SWAP
	, OP_IFDUP
	, OP_DEPTH		= 0x74
	, OP_DROP		= 0x75
	, OP_DUP		= 0x76

	, OP_NIP		= 0x77
	, OP_OVER
	, OP_PICK
	, OP_ROLL
	, OP_ROT
	, OP_SWAP = 0x7C
	, OP_TUCK

	// splice ops
    , OP_CAT	= 0x7E
	, OP_SUBSTR
	, OP_LEFT	= 0x80
	, OP_RIGHT
	, OP_SIZE	= 0x82

    // bit logic
    , OP_INVERT = 0x83
	, OP_AND
	, OP_OR
	, OP_XOR
	, OP_EQUAL			= 0x87
	, OP_EQUALVERIFY	= 0x88
	, OP_RESERVED1
	, OP_RESERVED2

    // numeric
    , OP_1ADD = 0x8B
	, OP_1SUB
	, OP_2MUL
	, OP_2DIV
	, OP_NEGATE
	, OP_ABS = 0x90
	, OP_NOT
	, OP_0NOTEQUAL = 0x92

    , OP_ADD = 0x93
	, OP_SUB
	, OP_MUL
	, OP_DIV
	, OP_MOD
	, OP_LSHIFT
	, OP_RSHIFT = 0x99

    , OP_BOOLAND = 0x9A
	, OP_BOOLOR
	, OP_NUMEQUAL
	, OP_NUMEQUALVERIFY
	, OP_NUMNOTEQUAL
	, OP_LESSTHAN
	, OP_GREATERTHAN		= 0xA0
	, OP_LESSTHANOREQUAL	= 0xA1
	, OP_GREATERTHANOREQUAL = 0xA2
	, OP_MIN				= 0xA3
	, OP_MAX				= 0xA4

    , OP_WITHIN				= 0xA5

    // crypto
    , OP_RIPEMD160				= 0xA6
	, OP_SHA1					= 0xA7
	, OP_SHA256					= 0xA8
	, OP_HASH160				= 0xA9
	, OP_HASH256				= 0xAA
	, OP_CODESEPARATOR			= 0xAB
	, OP_CHECKSIG				= 0xAC
	, OP_CHECKSIGVERIFY			= 0xAD
	, OP_CHECKMULTISIG			= 0xAE
	, OP_CHECKMULTISIGVERIFY	= 0xAF

    // expansion
    , OP_NOP1					= 0xB0
    , OP_CHECKLOCKTIMEVERIFY	= 0xB1
    , OP_CHECKSEQUENCEVERIFY	= 0xB2
    , OP_NOP4					= 0xB3
	, OP_NOP5
	, OP_NOP6
	, OP_NOP7
	, OP_NOP8
	, OP_NOP9
	, OP_NOP10					= 0xB9

    , OP_INVALIDOPCODE			= 0xFF
};


} // Coin::
