MessageIdTypedef=DWORD
SeverityNames=(Success=0x0:STATUS_SEVERITY_SUCCESS
               Informational=0x1:STATUS_SEVERITY_INFORMATIONAL
               Warning=0x2:STATUS_SEVERITY_WARNING
               Error=0x3:STATUS_SEVERITY_ERROR
              )
FacilityNames=(System=0x0:FACILITY_SYSTEM
               Coin=2081
               Stubs=0x3:FACILITY_STUBS
               Io=0x4:FACILITY_IO_ERROR_CODE
              )
LanguageNames=(E=0x409:MSG_EN)

MessageId=1
Severity=Warning
Facility=Coin
SymbolicName= E_COIN_BASE
Language=E
Coin Base Error
.

MessageId=
SymbolicName= E_COIN_InvalidAddress
Language=E
Invalid Address
.

MessageId=
SymbolicName= E_COIN_InsufficientAmount
Language=E
Insufficient Amount of money in Balance
.

MessageId=
SymbolicName= E_COIN_MoneyOutOfRange
Language=E
Money Out of Range
.

MessageId=
SymbolicName= E_COIN_RecentCoinbase
Language=E
Tried to spend recent coninbase
.

MessageId=
SymbolicName= E_COIN_VerifySignatureFailed
Language=E
Verify Signature Failed
.

MessageId=
SymbolicName= E_COIN_AlertVerifySignatureFailed
Language=E
Alert Verify Signature Failed
.

MessageId=
SymbolicName= E_COIN_RejectedByCheckpoint
Language=E
Block rejected by Checkpoint
.

MessageId=
SymbolicName= E_COIN_ValueInLessThanValueOut
Language=E
Value In Less than Value Out
.

MessageId=
SymbolicName= E_COIN_TxFeeIsLow
Language=E
Transaction Fee is very Low
.

MessageId=
SymbolicName= E_COIN_TxRejectedByRateLimiter
Language=E
Transaction Rejected by Rate Limiter
.

MessageId=
SymbolicName= E_COIN_TxTooLong
Language=E
Transaction is Too Long
.

MessageId=
SymbolicName= E_COIN_TxTooManySigOps
Language=E
Transaction has Too Many SigOps
.

MessageId=
SymbolicName= E_COIN_InconsistentDatabase
Language=E
Inconsistent Database
.

MessageId=
SymbolicName= E_COIN_SubsidyIsVeryBig
Language=E
Subsidy is very big
.

MessageId=
SymbolicName= E_COIN_ContainsNonFinalTx
Language=E
Block contains non-final transaction
.

MessageId=
SymbolicName= E_COIN_IncorrectProofOfWork
Language=E
Incorrect Proof of Work
.

MessageId=
SymbolicName= E_COIN_ProofOfWorkFailed
Language=E
Proof of Work failed
.

MessageId=
SymbolicName= E_COIN_TooEarlyTimestamp
Language=E
Too early Timestamp
.

MessageId=
SymbolicName= E_COIN_Misbehaving
Language=E
Peer Misbehaving
.

MessageId=
SymbolicName= E_COIN_OrphanedChain
Language=E
Orphaned Chain
.

MessageId=
SymbolicName= E_COIN_TxNotFound
Language=E
Transaction not found in Database
.

MessageId=
SymbolicName= E_COIN_DupNonSpentTx
Language=E
Duplicated Transaction Non-spent Transaction
.

MessageId=
SymbolicName= E_COIN_FirstTxIsNotTheOnlyCoinbase
Language=E
First Transaction is not the only Coinbase
.

MessageId=
SymbolicName= E_COIN_RescanDisabledDuringInitialDownload
Language=E
Rescan disabled during initial chain download
.

MessageId=
SymbolicName= E_COIN_AUXPOW_ParentHashOurChainId
Language=E
AuxPow Parent has our ChainID
.

MessageId=
SymbolicName= E_COIN_BlockDoesNotHaveOurChainId
Language=E
Block Does Not Have Our ChainID
.

MessageId=
SymbolicName= E_COIN_MerkleRootMismatch
Language=E
Merkle Root Mismatch
.

MessageId=
SymbolicName= E_COIN_BlockTimestampInTheFuture
Language=E
Block Timestam too far in the Future
.

MessageId=
SymbolicName= E_COIN_VeryBigPayload
Language=E
Very Big Payload
.

MessageId=
SymbolicName= E_COIN_RecipientAlreadyPresents
Language=E
This Recipient already presents
.

MessageId=
SymbolicName= E_COIN_XmlFileNotFound
Language=E
File coin-chains.xml not Found or corrupted
.

MessageId=
SymbolicName= E_COIN_AUXPOW_ChainMerkleBranchTooLong
Language=E
AuxPow Chain Merkle Branch too long
.

MessageId=
SymbolicName= E_COIN_AUXPOW_MerkleRootIncorrect
Language=E
AuxPow Chain Merkle Root Incorrect
.

MessageId=
SymbolicName= E_COIN_AUXPOW_MissingMerkleRootInParentCoinbase
Language=E
AuxPow Missing Merkle Root In Parent Coinbase
.

MessageId=
SymbolicName= E_COIN_AUXPOW_MutipleMergedMiningHeades
Language=E
AuxPow Multiple merged mining headers in coinbase
.

MessageId=
SymbolicName= E_COIN_AUXPOW_MergedMinignHeaderiNotJustBeforeMerkleRoot
Language=E
AuxPow Merged Minign Headeri Not Just Before Merkle Root
.

MessageId=
SymbolicName= E_COIN_AUXPOW_MerkleRootMustStartInFirstFewBytes
Language=E
AuxPow Merkle Root Must Start In First Few Bytes
.

MessageId=
SymbolicName= E_COIN_AUXPOW_MerkleBranchSizeIncorrect
Language=E
AuxPow Merkle Branch Size Incorrect
.

MessageId=
SymbolicName= E_COIN_AUXPOW_WrongIndex
Language=E
AuxPow Wrong Index
.

MessageId=
SymbolicName= E_COIN_AUXPOW_NotAllowed
Language=E
AuxPow not Allowed
.

MessageId=
SymbolicName= E_COIN_NAME_InvalidTx
Language=E
Invalid Namecoin Transaction
.

MessageId=
SymbolicName= E_COIN_NAME_NameCoinTransactionWithInvalidVersion
Language=E
Non-Namecoin Transaction with Namecoin input
.

MessageId=
SymbolicName= E_COIN_NAME_NewPointsPrevious
Language=E
Name New Transaction pointing to previous transaction
.

MessageId=
SymbolicName= E_COIN_NAME_ToolLongName
Language=E
Name is too Long
.

MessageId=
SymbolicName= E_COIN_NAME_ExpirationError
Language=E
Expiration Error
.

MessageId=256
SymbolicName= E_COIN_MINER_BAD_CB_FLAG
Language=E
the server detected a feature-signifying flag that it does not allow
.

MessageId=
SymbolicName= E_COIN_MINER_BAD_CB_LENGTH
Language=E
the coinbase was too long (bitcoin limit is 100 bytes)
.

MessageId=
SymbolicName= E_COIN_MINER_BAD_CB_PREFIX
Language=E
the server only allows appending to the coinbase, but it was modified beyond that
.

MessageId=
SymbolicName= E_COIN_MINER_BAD_DIFFBITS
Language=E
"bits" were changed
.

MessageId=
SymbolicName= E_COIN_MINER_BAD_PREVBLK
Language=E
the previous-block is not the one the server intends to build on
.

MessageId=
SymbolicName= E_COIN_MINER_BAD_TXNMRKLROOT
Language=E
the block header's merkle root did not match the transaction merkle tree
.

MessageId=
SymbolicName= E_COIN_MINER_BAD_TXNS
Language=E
the server didn't like something about the transactions in the block
.

MessageId=
SymbolicName= E_COIN_MINER_BAD_VERSION
Language=E
the version was wrong
.

MessageId=
SymbolicName= E_COIN_MINER_DUPLICATE
Language=E
the server already processed this block data
.

MessageId=
SymbolicName= E_COIN_MINER_HIGH_HASH
Language=E
the block header did not hash to a value lower than the specified target
.

MessageId=
SymbolicName= E_COIN_MINER_REJECTED
Language=E
a generic rejection without details
.

MessageId=
SymbolicName= E_COIN_MINER_STALE_PREVBLK
Language=E
the previous-block is no longer the one the server intends to build on
.

MessageId=
SymbolicName= E_COIN_MINER_STALE_WORK
Language=E
the work this block was based on is no longer accepted
.

MessageId=
SymbolicName= E_COIN_MINER_TIME_INVALID
Language=E
the time was not acceptable
.

MessageId=
SymbolicName= E_COIN_MINER_TIME_TOO_NEW
Language=E
the time was too far in the future
.

MessageId=
SymbolicName= E_COIN_MINER_TIME_TOO_OLD
Language=E
the time was too far in the past
.

MessageId=
SymbolicName= E_COIN_MINER_UNKNOWN_USER
Language=E
the user submitting the block was not recognized
.

MessageId=
SymbolicName= E_COIN_MINER_UNKNOWN_WORK
Language=E
the template or workid could not be identified
.

MessageId=
SymbolicName= E_COIN_NoCurrencyWithThisName
Language=E
No Currency with this Name
.

MessageId=
SymbolicName= E_COIN_CheckpointVerifySignatureFailed
Language=E
CheckPoint Verify Signature Failed
.

MessageId=
SymbolicName= E_COIN_BadBlockSignature
Language=E
Bad Block Signature
.

MessageId=
SymbolicName= E_COIN_CoinbaseTimestampIsTooEarly
Language=E
Coinbase Timestamp is too early
.

MessageId=
SymbolicName= E_COIN_CoinstakeInWrongPos
Language=E
Coinstake in wrong position
.

MessageId=
SymbolicName= E_COIN_TimestampViolation
Language=E
Timestamp Violation
.

MessageId=
SymbolicName= E_COIN_CoinsAreTooRecent
Language=E
Coins are too recent
.

MessageId=
SymbolicName= E_COIN_CoinstakeCheckTargetFailed
Language=E
check target failed on coinstake
.

MessageId=
SymbolicName= E_COIN_StakeRewardExceeded
Language=E
Stake Reward Exceeded
.

MessageId=
SymbolicName= E_COIN_TxOutBelowMinimum
Language=E
Tx Out value below minimum
.

MessageId=
SymbolicName= E_COIN_SizeLimits
Language=E
Size Limits failed
.

MessageId=
SymbolicName= E_COIN_MINER_FPGA_PROTOCOL
Language=E
FPGA hardware protocol violation, try to RESET the device
.

MessageId=
SymbolicName= E_COIN_UPPER
Language=E
Upper Coin Error
.



