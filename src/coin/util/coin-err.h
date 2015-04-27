/*######   Copyright (c) 2015 Ufasoft  http://ufasoft.com  mailto:support@ufasoft.com,  Sergey Pavlov  mailto:dev@ufasoft.com ####
#                                                                                                                                     #
# 		See LICENSE for licensing information                                                                                         #
#####################################################################################################################################*/

#pragma once


namespace Coin {

ENUM_CLASS(CoinErr) {
	InvalidAddress = 1
	, InsufficientAmount
	, MoneyOutOfRange
	, RecentCoinbase
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
	, IncorrectProofOfWork
	, ProofOfWorkFailed
	, TooEarlyTimestamp
	, Misbehaving
	, OrphanedChain
	, TxNotFound
	, DupNonSpentTx
	, FirstTxIsNotTheOnlyCoinbase
	, RescanDisabledDuringInitialDownload
	, BlockDoesNotHaveOurChainId
	, MerkleRootMismatch
	, BlockTimestampInTheFuture
	, VeryBigPayload
	, RecipientAlreadyPresents
	, XmlFileNotFound
	, InvalidScript
	, NoCurrencyWithThisName
	, CheckpointVerifySignatureFailed
	, BadBlockSignature
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

} END_ENUM_CLASS(CoinErr);


const error_category& coin_category();

inline error_code make_error_code(CoinErr v) { return error_code(int(v), coin_category()); }
inline error_condition make_error_condition(CoinErr v) { return error_condition(int(v), coin_category()); }


int SubmitRejectionCode(RCString rej);

} // Coin::

namespace std { template<> struct std::is_error_code_enum<Coin::CoinErr> : true_type {}; }