/*######   Copyright (c) 2015-2019 Ufasoft  http://ufasoft.com  mailto:support@ufasoft.com,  Sergey Pavlov  mailto:dev@ufasoft.com ####
#                                                                                                                                     #
# 		See LICENSE for licensing information                                                                                         #
#####################################################################################################################################*/

#pragma once


namespace Coin {

enum class CoinErr {
	InvalidAddress = 1
	, InsufficientAmount
	, MoneyOutOfRange
	, VerifySignatureFailed
	, AlertVerifySignatureFailed
	, RejectedByCheckpoint
	, ValueInLessThanValueOut
	, TxFeeIsLow
	, TxRejectedByRateLimiter
	, TxTooLong
	, TxTooManySigOps
	, InconsistentDatabase
	, SubsidyIsVeryBig
	, ContainsNonFinalTx
    , BadWitnessNonceSize
    , BadWitnessMerkleMatch
    , UnexpectedWitness
	, IncorrectProofOfWork
	, ProofOfWorkFailed
	, TooEarlyTimestamp
    , BlockTimestampInTheFuture
	, Misbehaving
	, OrphanedChain
	, TxNotFound
	, DupNonSpentTx
	, FirstTxIsNotTheOnlyCoinbase
	, RescanIsDisabledDuringInitialDownload
	, BlockDoesNotHaveOurChainId
	, MerkleRootMismatch
	, VeryBigPayload
	, RecipientAlreadyPresents
	, XmlFileNotFound
	, NoCurrencyWithThisName
	, CheckpointVerifySignatureFailed
	, BadBlockSignature
    , BadBlockVersion
    , BadBlockWeight
	, CoinbaseTimestampIsTooEarly
	, CoinstakeInWrongPos
	, TimestampViolation
	, CoinsAreTooRecent
	, CoinstakeCheckTargetFailed
	, StakeRewardExceeded
	, TxOutBelowMinimum
	, SizeLimits
	, BlockNotFound
	, AllowedErrorDuringInitialDownload
	, UNAUTHORIZED_WORKER
	, CURRENCY_NOT_SUPPORTED
	, DupTxInputs
	, DupVersionMessage
	, InputsAlreadySpent
	, RejectedV1Block
	, BlockHeightMismatchInCoinbase
	, InvalidBootstrapFile
	, TxAmountTooSmall
	, FirstTxIsNotCoinbase
	, BadCbLength
	, BadTxnsPrevoutNull
	, BadTxnsVinEmpty
	, BadTxnsVoutEmpty
	, BadTxnsVoutNegative
	, BadPrevBlock
	, InvalidPrivateKey
	, VersionMessageMustBeFirst
	, GetBlocksLocatorSize
	, NonStandardTx
	, TxMissingInputs
	, TxPrematureSpend
	, TxOrdering
	, CannotReorganizeBeyondPrunedBlocks
	, NonCanonicalCompactSize

	, SCRIPT_ERR_UNKNOWN_ERROR = 501
    , SCRIPT_ERR_EVAL_FALSE
    , SCRIPT_ERR_OP_RETURN
    , SCRIPT_ERR_SCRIPT_SIZE
    , SCRIPT_ERR_PUSH_SIZE
    , SCRIPT_ERR_OP_COUNT
    , SCRIPT_ERR_STACK_SIZE
    , SCRIPT_ERR_SIG_COUNT
    , SCRIPT_ERR_PUBKEY_COUNT
    , SCRIPT_ERR_VERIFY
    , SCRIPT_ERR_EQUALVERIFY
    , SCRIPT_ERR_CHECKMULTISIGVERIFY
    , SCRIPT_ERR_CHECKSIGVERIFY
    , SCRIPT_ERR_NUMEQUALVERIFY
    , SCRIPT_ERR_BAD_OPCODE
    , SCRIPT_ERR_DISABLED_OPCODE
    , SCRIPT_ERR_INVALID_STACK_OPERATION
    , SCRIPT_ERR_INVALID_ALTSTACK_OPERATION
    , SCRIPT_ERR_UNBALANCED_CONDITIONAL
    , SCRIPT_ERR_NEGATIVE_LOCKTIME
    , SCRIPT_ERR_UNSATISFIED_LOCKTIME
    , SCRIPT_ERR_SIG_HASHTYPE
    , SCRIPT_ERR_SIG_DER
    , SCRIPT_ERR_MINIMALDATA
    , SCRIPT_ERR_SIG_PUSHONLY
    , SCRIPT_ERR_SIG_HIGH_S
    , SCRIPT_ERR_SIG_NULLDUMMY
    , SCRIPT_ERR_PUBKEYTYPE
    , SCRIPT_ERR_CLEANSTACK
    , SCRIPT_ERR_MINIMALIF
    , SCRIPT_ERR_SIG_NULLFAIL
    , SCRIPT_ERR_DISCOURAGE_UPGRADABLE_NOPS
	, SCRIPT_ERR_DISCOURAGE_UPGRADABLE_WITNESS_PROGRAM
    , SCRIPT_ERR_WITNESS_PROGRAM_WRONG_LENGTH
    , SCRIPT_ERR_WITNESS_PROGRAM_WITNESS_EMPTY
    , SCRIPT_ERR_WITNESS_PROGRAM_MISMATCH
    , SCRIPT_ERR_WITNESS_MALLEATED
    , SCRIPT_ERR_WITNESS_MALLEATED_P2SH
    , SCRIPT_ERR_WITNESS_UNEXPECTED
    , SCRIPT_ERR_WITNESS_PUBKEYTYPE
    , SCRIPT_ERR_OP_CODESEPARATOR
    , SCRIPT_ERR_SIG_FINDANDDELETE
	, SCRIPT_DISABLED_OPCODE
	, SCRIPT_RANGE
	, SCRIPT_INVALID_ARG

	, RPC_MISC_ERROR			= 1001
	, RPC_FORBIDDEN_BY_SAFE_MODE
	, RPC_TYPE_ERROR
	, RPC_WALLET_ERROR
	, RPC_INVALID_ADDRESS_OR_KEY
	, RPC_WALLET_INSUFFICIENT_FUNDS
	, RPC_OUT_OF_MEMORY
	, RPC_INVALID_PARAMETER
	, RPC_CLIENT_NOT_CONNECTED
	, RPC_CLIENT_IN_INITIAL_DOWNLOAD
	, RPC_WALLET_INVALID_ACCOUNT_NAME
	, RPC_WALLET_KEYPOOL_RAN_OUT
	, RPC_WALLET_UNLOCK_NEEDED
	, RPC_WALLET_PASSPHRASE_INCORRECT
	, RPC_WALLET_WRONG_ENC_STATE
	, RPC_WALLET_ENCRYPTION_FAILED
	, RPC_WALLET_ALREADY_UNLOCKED
	, RPC_DATABASE_ERROR		= 1020
	, RPC_DESERIALIZATION_ERROR	= 1022

	, AUXPOW_ParentHashOurChainId	= 2000
	, AUXPOW_ChainMerkleBranchTooLong
	, AUXPOW_MerkleRootIncorrect
	, AUXPOW_MissingMerkleRootInParentCoinbase
	, AUXPOW_MutipleMergedMiningHeades
	, AUXPOW_MergedMinignHeaderiNotJustBeforeMerkleRoot
	, AUXPOW_MerkleRootMustStartInFirstFewBytes
	, AUXPOW_MerkleBranchSizeIncorrect
	, AUXPOW_WrongIndex
	, AUXPOW_NotAllowed

	, NAME_InvalidTx = 3000
	, NAME_NameCoinTransactionWithInvalidVersion
	, NAME_NewPointsPrevious
	, NAME_ToolLongName
	, NAME_ExpirationError

	, MINER_BAD_CB_FLAG = 4000
	, MINER_BAD_CB_LENGTH
	, MINER_BAD_CB_PREFIX
	, MINER_BAD_DIFFBITS
	, MINER_BAD_PREVBLK
	, MINER_BAD_TXNMRKLROOT
	, MINER_BAD_TXNS
	, MINER_BAD_VERSION
	, MINER_DUPLICATE
	, MINER_HIGH_HASH
	, MINER_REJECTED
	, MINER_STALE_PREVBLK
	, MINER_STALE_WORK
	, MINER_TIME_INVALID
	, MINER_TIME_TOO_NEW
	, MINER_TIME_TOO_OLD
	, MINER_UNKNOWN_USER
	, MINER_UNKNOWN_WORK
	, MINER_FPGA_PROTOCOL

};


const error_category& coin_category();

inline error_code make_error_code(CoinErr v) { return error_code(int(v), coin_category()); }
inline error_condition make_error_condition(CoinErr v) { return error_condition(int(v), coin_category()); }


int SubmitRejectionCode(RCString rej);

} // Coin::

namespace std { template<> struct std::is_error_code_enum<Coin::CoinErr> : true_type {}; }
