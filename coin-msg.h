// MessageIdTypedef=DWORD
// SeverityNames=(Success=0x0:STATUS_SEVERITY_SUCCESS
//                Informational=0x1:STATUS_SEVERITY_INFORMATIONAL
//                Warning=0x2:STATUS_SEVERITY_WARNING
//                Error=0x3:STATUS_SEVERITY_ERROR
//               )
// FacilityNames=(System=0x0:FACILITY_SYSTEM
//                Coin=2081
//                Stubs=0x3:FACILITY_STUBS
//                Io=0x4:FACILITY_IO_ERROR_CODE
//               )
// LanguageNames=(E=0x409:MSG_EN)
// MessageId=1
// Severity=Warning
// Facility=Coin
// SymbolicName= E_COIN_BASE
// Language=E
// Coin Base Error
#define E_COIN_BASE ((DWORD)0x88210001)
// MessageId=
// SymbolicName= E_COIN_InvalidAddress
// Language=E
// Invalid Address
#define E_COIN_InvalidAddress ((DWORD)0x88210002)
// MessageId=
// SymbolicName= E_COIN_InsufficientAmount
// Language=E
// Insufficient Amount of money in Balance
#define E_COIN_InsufficientAmount ((DWORD)0x88210003)
// MessageId=
// SymbolicName= E_COIN_MoneyOutOfRange
// Language=E
// Money Out of Range
#define E_COIN_MoneyOutOfRange ((DWORD)0x88210004)
// MessageId=
// SymbolicName= E_COIN_RecentCoinbase
// Language=E
// Tried to spend recent coninbase
#define E_COIN_RecentCoinbase ((DWORD)0x88210005)
// MessageId=
// SymbolicName= E_COIN_VerifySignatureFailed
// Language=E
// Verify Signature Failed
#define E_COIN_VerifySignatureFailed ((DWORD)0x88210006)
// MessageId=
// SymbolicName= E_COIN_AlertVerifySignatureFailed
// Language=E
// Alert Verify Signature Failed
#define E_COIN_AlertVerifySignatureFailed ((DWORD)0x88210007)
// MessageId=
// SymbolicName= E_COIN_RejectedByCheckpoint
// Language=E
// Block rejected by Checkpoint
#define E_COIN_RejectedByCheckpoint ((DWORD)0x88210008)
// MessageId=
// SymbolicName= E_COIN_ValueInLessThanValueOut
// Language=E
// Value In Less than Value Out
#define E_COIN_ValueInLessThanValueOut ((DWORD)0x88210009)
// MessageId=
// SymbolicName= E_COIN_TxFeeIsLow
// Language=E
// Transaction Fee is very Low
#define E_COIN_TxFeeIsLow ((DWORD)0x8821000a)
// MessageId=
// SymbolicName= E_COIN_TxRejectedByRateLimiter
// Language=E
// Transaction Rejected by Rate Limiter
#define E_COIN_TxRejectedByRateLimiter ((DWORD)0x8821000b)
// MessageId=
// SymbolicName= E_COIN_TxTooLong
// Language=E
// Transaction is Too Long
#define E_COIN_TxTooLong ((DWORD)0x8821000c)
// MessageId=
// SymbolicName= E_COIN_TxTooManySigOps
// Language=E
// Transaction has Too Many SigOps
#define E_COIN_TxTooManySigOps ((DWORD)0x8821000d)
// MessageId=
// SymbolicName= E_COIN_InconsistentDatabase
// Language=E
// Inconsistent Database
#define E_COIN_InconsistentDatabase ((DWORD)0x8821000e)
// MessageId=
// SymbolicName= E_COIN_SubsidyIsVeryBig
// Language=E
// Subsidy is very big
#define E_COIN_SubsidyIsVeryBig ((DWORD)0x8821000f)
// MessageId=
// SymbolicName= E_COIN_ContainsNonFinalTx
// Language=E
// Block contains non-final transaction
#define E_COIN_ContainsNonFinalTx ((DWORD)0x88210010)
// MessageId=
// SymbolicName= E_COIN_IncorrectProofOfWork
// Language=E
// Incorrect Proof of Work
#define E_COIN_IncorrectProofOfWork ((DWORD)0x88210011)
// MessageId=
// SymbolicName= E_COIN_ProofOfWorkFailed
// Language=E
// Proof of Work failed
#define E_COIN_ProofOfWorkFailed ((DWORD)0x88210012)
// MessageId=
// SymbolicName= E_COIN_TooEarlyTimestamp
// Language=E
// Too early Timestamp
#define E_COIN_TooEarlyTimestamp ((DWORD)0x88210013)
// MessageId=
// SymbolicName= E_COIN_Misbehaving
// Language=E
// Peer Misbehaving
#define E_COIN_Misbehaving ((DWORD)0x88210014)
// MessageId=
// SymbolicName= E_COIN_OrphanedChain
// Language=E
// Orphaned Chain
#define E_COIN_OrphanedChain ((DWORD)0x88210015)
// MessageId=
// SymbolicName= E_COIN_TxNotFound
// Language=E
// Transaction not found in Database
#define E_COIN_TxNotFound ((DWORD)0x88210016)
// MessageId=
// SymbolicName= E_COIN_DupNonSpentTx
// Language=E
// Duplicated Transaction Non-spent Transaction
#define E_COIN_DupNonSpentTx ((DWORD)0x88210017)
// MessageId=
// SymbolicName= E_COIN_FirstTxIsNotTheOnlyCoinbase
// Language=E
// First Transaction is not the only Coinbase
#define E_COIN_FirstTxIsNotTheOnlyCoinbase ((DWORD)0x88210018)
// MessageId=
// SymbolicName= E_COIN_RescanDisabledDuringInitialDownload
// Language=E
// Rescan disabled during initial chain download
#define E_COIN_RescanDisabledDuringInitialDownload ((DWORD)0x88210019)
// MessageId=
// SymbolicName= E_COIN_AUXPOW_ParentHashOurChainId
// Language=E
// AuxPow Parent has our ChainID
#define E_COIN_AUXPOW_ParentHashOurChainId ((DWORD)0x8821001a)
// MessageId=
// SymbolicName= E_COIN_BlockDoesNotHaveOurChainId
// Language=E
// Block Does Not Have Our ChainID
#define E_COIN_BlockDoesNotHaveOurChainId ((DWORD)0x8821001b)
// MessageId=
// SymbolicName= E_COIN_MerkleRootMismatch
// Language=E
// Merkle Root Mismatch
#define E_COIN_MerkleRootMismatch ((DWORD)0x8821001c)
// MessageId=
// SymbolicName= E_COIN_BlockTimestampInTheFuture
// Language=E
// Block Timestam too far in the Future
#define E_COIN_BlockTimestampInTheFuture ((DWORD)0x8821001d)
// MessageId=
// SymbolicName= E_COIN_VeryBigPayload
// Language=E
// Very Big Payload
#define E_COIN_VeryBigPayload ((DWORD)0x8821001e)
// MessageId=
// SymbolicName= E_COIN_RecipientAlreadyPresents
// Language=E
// This Recipient already presents
#define E_COIN_RecipientAlreadyPresents ((DWORD)0x8821001f)
// MessageId=
// SymbolicName= E_COIN_XmlFileNotFound
// Language=E
// File coin-chains.xml not Found or corrupted
#define E_COIN_XmlFileNotFound ((DWORD)0x88210020)
// MessageId=
// SymbolicName= E_COIN_AUXPOW_ChainMerkleBranchTooLong
// Language=E
// AuxPow Chain Merkle Branch too long
#define E_COIN_AUXPOW_ChainMerkleBranchTooLong ((DWORD)0x88210021)
// MessageId=
// SymbolicName= E_COIN_AUXPOW_MerkleRootIncorrect
// Language=E
// AuxPow Chain Merkle Root Incorrect
#define E_COIN_AUXPOW_MerkleRootIncorrect ((DWORD)0x88210022)
// MessageId=
// SymbolicName= E_COIN_AUXPOW_MissingMerkleRootInParentCoinbase
// Language=E
// AuxPow Missing Merkle Root In Parent Coinbase
#define E_COIN_AUXPOW_MissingMerkleRootInParentCoinbase ((DWORD)0x88210023)
// MessageId=
// SymbolicName= E_COIN_AUXPOW_MutipleMergedMiningHeades
// Language=E
// AuxPow Multiple merged mining headers in coinbase
#define E_COIN_AUXPOW_MutipleMergedMiningHeades ((DWORD)0x88210024)
// MessageId=
// SymbolicName= E_COIN_AUXPOW_MergedMinignHeaderiNotJustBeforeMerkleRoot
// Language=E
// AuxPow Merged Minign Headeri Not Just Before Merkle Root
#define E_COIN_AUXPOW_MergedMinignHeaderiNotJustBeforeMerkleRoot ((DWORD)0x88210025)
// MessageId=
// SymbolicName= E_COIN_AUXPOW_MerkleRootMustStartInFirstFewBytes
// Language=E
// AuxPow Merkle Root Must Start In First Few Bytes
#define E_COIN_AUXPOW_MerkleRootMustStartInFirstFewBytes ((DWORD)0x88210026)
// MessageId=
// SymbolicName= E_COIN_AUXPOW_MerkleBranchSizeIncorrect
// Language=E
// AuxPow Merkle Branch Size Incorrect
#define E_COIN_AUXPOW_MerkleBranchSizeIncorrect ((DWORD)0x88210027)
// MessageId=
// SymbolicName= E_COIN_AUXPOW_WrongIndex
// Language=E
// AuxPow Wrong Index
#define E_COIN_AUXPOW_WrongIndex ((DWORD)0x88210028)
// MessageId=
// SymbolicName= E_COIN_AUXPOW_NotAllowed
// Language=E
// AuxPow not Allowed
#define E_COIN_AUXPOW_NotAllowed ((DWORD)0x88210029)
// MessageId=
// SymbolicName= E_COIN_NAME_InvalidTx
// Language=E
// Invalid Namecoin Transaction
#define E_COIN_NAME_InvalidTx ((DWORD)0x8821002a)
// MessageId=
// SymbolicName= E_COIN_NAME_NameCoinTransactionWithInvalidVersion
// Language=E
// Non-Namecoin Transaction with Namecoin input
#define E_COIN_NAME_NameCoinTransactionWithInvalidVersion ((DWORD)0x8821002b)
// MessageId=
// SymbolicName= E_COIN_NAME_NewPointsPrevious
// Language=E
// Name New Transaction pointing to previous transaction
#define E_COIN_NAME_NewPointsPrevious ((DWORD)0x8821002c)
// MessageId=
// SymbolicName= E_COIN_NAME_ToolLongName
// Language=E
// Name is too Long
#define E_COIN_NAME_ToolLongName ((DWORD)0x8821002d)
// MessageId=
// SymbolicName= E_COIN_NAME_ExpirationError
// Language=E
// Expiration Error
#define E_COIN_NAME_ExpirationError ((DWORD)0x8821002e)
// MessageId=256
// SymbolicName= E_COIN_MINER_BAD_CB_FLAG
// Language=E
// the server detected a feature-signifying flag that it does not allow
#define E_COIN_MINER_BAD_CB_FLAG ((DWORD)0x88210100)
// MessageId=
// SymbolicName= E_COIN_MINER_BAD_CB_LENGTH
// Language=E
// the coinbase was too long (bitcoin limit is 100 bytes)
#define E_COIN_MINER_BAD_CB_LENGTH ((DWORD)0x88210101)
// MessageId=
// SymbolicName= E_COIN_MINER_BAD_CB_PREFIX
// Language=E
// the server only allows appending to the coinbase, but it was modified beyond that
#define E_COIN_MINER_BAD_CB_PREFIX ((DWORD)0x88210102)
// MessageId=
// SymbolicName= E_COIN_MINER_BAD_DIFFBITS
// Language=E
// "bits" were changed
#define E_COIN_MINER_BAD_DIFFBITS ((DWORD)0x88210103)
// MessageId=
// SymbolicName= E_COIN_MINER_BAD_PREVBLK
// Language=E
// the previous-block is not the one the server intends to build on
#define E_COIN_MINER_BAD_PREVBLK ((DWORD)0x88210104)
// MessageId=
// SymbolicName= E_COIN_MINER_BAD_TXNMRKLROOT
// Language=E
// the block header's merkle root did not match the transaction merkle tree
#define E_COIN_MINER_BAD_TXNMRKLROOT ((DWORD)0x88210105)
// MessageId=
// SymbolicName= E_COIN_MINER_BAD_TXNS
// Language=E
// the server didn't like something about the transactions in the block
#define E_COIN_MINER_BAD_TXNS ((DWORD)0x88210106)
// MessageId=
// SymbolicName= E_COIN_MINER_BAD_VERSION
// Language=E
// the version was wrong
#define E_COIN_MINER_BAD_VERSION ((DWORD)0x88210107)
// MessageId=
// SymbolicName= E_COIN_MINER_DUPLICATE
// Language=E
// the server already processed this block data
#define E_COIN_MINER_DUPLICATE ((DWORD)0x88210108)
// MessageId=
// SymbolicName= E_COIN_MINER_HIGH_HASH
// Language=E
// the block header did not hash to a value lower than the specified target
#define E_COIN_MINER_HIGH_HASH ((DWORD)0x88210109)
// MessageId=
// SymbolicName= E_COIN_MINER_REJECTED
// Language=E
// a generic rejection without details
#define E_COIN_MINER_REJECTED ((DWORD)0x8821010a)
// MessageId=
// SymbolicName= E_COIN_MINER_STALE_PREVBLK
// Language=E
// the previous-block is no longer the one the server intends to build on
#define E_COIN_MINER_STALE_PREVBLK ((DWORD)0x8821010b)
// MessageId=
// SymbolicName= E_COIN_MINER_STALE_WORK
// Language=E
// the work this block was based on is no longer accepted
#define E_COIN_MINER_STALE_WORK ((DWORD)0x8821010c)
// MessageId=
// SymbolicName= E_COIN_MINER_TIME_INVALID
// Language=E
// the time was not acceptable
#define E_COIN_MINER_TIME_INVALID ((DWORD)0x8821010d)
// MessageId=
// SymbolicName= E_COIN_MINER_TIME_TOO_NEW
// Language=E
// the time was too far in the future
#define E_COIN_MINER_TIME_TOO_NEW ((DWORD)0x8821010e)
// MessageId=
// SymbolicName= E_COIN_MINER_TIME_TOO_OLD
// Language=E
// the time was too far in the past
#define E_COIN_MINER_TIME_TOO_OLD ((DWORD)0x8821010f)
// MessageId=
// SymbolicName= E_COIN_MINER_UNKNOWN_USER
// Language=E
// the user submitting the block was not recognized
#define E_COIN_MINER_UNKNOWN_USER ((DWORD)0x88210110)
// MessageId=
// SymbolicName= E_COIN_MINER_UNKNOWN_WORK
// Language=E
// the template or workid could not be identified
#define E_COIN_MINER_UNKNOWN_WORK ((DWORD)0x88210111)
// MessageId=
// SymbolicName= E_COIN_NoCurrencyWithThisName
// Language=E
// No Currency with this Name
#define E_COIN_NoCurrencyWithThisName ((DWORD)0x88210112)
// MessageId=
// SymbolicName= E_COIN_CheckpointVerifySignatureFailed
// Language=E
// CheckPoint Verify Signature Failed
#define E_COIN_CheckpointVerifySignatureFailed ((DWORD)0x88210113)
// MessageId=
// SymbolicName= E_COIN_BadBlockSignature
// Language=E
// Bad Block Signature
#define E_COIN_BadBlockSignature ((DWORD)0x88210114)
// MessageId=
// SymbolicName= E_COIN_CoinbaseTimestampIsTooEarly
// Language=E
// Coinbase Timestamp is too early
#define E_COIN_CoinbaseTimestampIsTooEarly ((DWORD)0x88210115)
// MessageId=
// SymbolicName= E_COIN_CoinstakeInWrongPos
// Language=E
// Coinstake in wrong position
#define E_COIN_CoinstakeInWrongPos ((DWORD)0x88210116)
// MessageId=
// SymbolicName= E_COIN_TimestampViolation
// Language=E
// Timestamp Violation
#define E_COIN_TimestampViolation ((DWORD)0x88210117)
// MessageId=
// SymbolicName= E_COIN_CoinsAreTooRecent
// Language=E
// Coins are too recent
#define E_COIN_CoinsAreTooRecent ((DWORD)0x88210118)
// MessageId=
// SymbolicName= E_COIN_CoinstakeCheckTargetFailed
// Language=E
// check target failed on coinstake
#define E_COIN_CoinstakeCheckTargetFailed ((DWORD)0x88210119)
// MessageId=
// SymbolicName= E_COIN_StakeRewardExceeded
// Language=E
// Stake Reward Exceeded
#define E_COIN_StakeRewardExceeded ((DWORD)0x8821011a)
// MessageId=
// SymbolicName= E_COIN_TxOutBelowMinimum
// Language=E
// Tx Out value below minimum
#define E_COIN_TxOutBelowMinimum ((DWORD)0x8821011b)
// MessageId=
// SymbolicName= E_COIN_SizeLimits
// Language=E
// Size Limits failed
#define E_COIN_SizeLimits ((DWORD)0x8821011c)
// MessageId=
// SymbolicName= E_COIN_MINER_FPGA_PROTOCOL
// Language=E
// FPGA hardware protocol violation, try to RESET the device
#define E_COIN_MINER_FPGA_PROTOCOL ((DWORD)0x8821011d)
// MessageId=
// SymbolicName= E_COIN_UPPER
// Language=E
// Upper Coin Error
#define E_COIN_UPPER ((DWORD)0x8821011e)
