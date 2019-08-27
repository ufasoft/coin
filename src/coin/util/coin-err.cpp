/*######   Copyright (c) 2011-2019 Ufasoft  http://ufasoft.com  mailto:support@ufasoft.com,  Sergey Pavlov  mailto:dev@ufasoft.com ####
#                                                                                                                                     #
# 		See LICENSE for licensing information                                                                                         #
#####################################################################################################################################*/

#include <el/ext.h>

#include "coin-err.h"

namespace Coin {

static const CodeMessage<CoinErr> s_coinMessageTable[] {
	{ CoinErr::InvalidAddress									, "Invalid Address"								}
	, { CoinErr::InsufficientAmount								, "Insufficient Amount of money in Balance"		}
	, { CoinErr::MoneyOutOfRange								, "Money Out of Range"							}
	, { CoinErr::VerifySignatureFailed							, "Verify Signature Failed"						}
	, { CoinErr::AlertVerifySignatureFailed						, "Alert Verify Signature Failed"				}
	, { CoinErr::RejectedByCheckpoint							, "Block rejected by Checkpoint"				}
	, { CoinErr::ValueInLessThanValueOut						, "Value-In Less than Value-Out"				}
	, { CoinErr::TxFeeIsLow										, "Transaction Fee is very Low"					}
	, { CoinErr::TxRejectedByRateLimiter						, "Transaction Rejected by Rate Limiter"		}
	, { CoinErr::TxTooLong										, "Transaction is Too Long"						}
	, { CoinErr::TxTooManySigOps								, "Transaction has Too Many SigOps"				}
	, { CoinErr::InconsistentDatabase							, "Inconsistent Database"						}
	, { CoinErr::SubsidyIsVeryBig								, "Subsidy is very big"							}
	, { CoinErr::ContainsNonFinalTx								, "Block contains non-final transaction"		}
	, { CoinErr::BadWitnessNonceSize							, "Bad Witness Nonce size"						}
	, { CoinErr::BadWitnessMerkleMatch							, "Bad Witness Merkle match"					}
	, { CoinErr::UnexpectedWitness								, "Unexpected Witness"							}
	, { CoinErr::IncorrectProofOfWork							, "Incorrect Proof of Work"						}
	, { CoinErr::ProofOfWorkFailed								, "Proof of Work failed"						}
	, { CoinErr::TooEarlyTimestamp								, "Too early Timestamp"							}
	, { CoinErr::Misbehaving									, "Peer Misbehaving"							}
	, { CoinErr::OrphanedChain									, "Orphaned Chain"								}
	, { CoinErr::TxNotFound										, "Transaction not found in Database"			}
	, { CoinErr::DupNonSpentTx									, "Duplicated Transaction Non-spent Transaction"	}
	, { CoinErr::FirstTxIsNotTheOnlyCoinbase					, "First Transaction is not the only Coinbase"	}
	, { CoinErr::RescanIsDisabledDuringInitialDownload			, "Rescan is disabled during initial chain download"	}
	, { CoinErr::BlockDoesNotHaveOurChainId						, "Block Does Not Have Our ChainID"				}
	, { CoinErr::MerkleRootMismatch								, "Merkle Root Mismatch"						}
	, { CoinErr::BlockTimestampInTheFuture						, "Block Timestamp too far in the Future"		}
	, { CoinErr::VeryBigPayload									, "Very Big Payload"							}
	, { CoinErr::RecipientAlreadyPresents						, "This Recipient already presents"				}
	, { CoinErr::XmlFileNotFound								, "File coin-chains.xml not Found or corrupted"	}
	, { CoinErr::NoCurrencyWithThisName							, "No Currency with this Name"					}
	, { CoinErr::CheckpointVerifySignatureFailed				, "CheckPoint Verify Signature Failed"			}
	, { CoinErr::BadBlockSignature								, "Bad Block Signature"							}
	, { CoinErr::CoinbaseTimestampIsTooEarly					, "Coinbase Timestamp is too early"				}
	, { CoinErr::CoinstakeInWrongPos							, "Coinstake in wrong position"					}
	, { CoinErr::TimestampViolation								, "Timestamp Violation"							}
	, { CoinErr::CoinsAreTooRecent								, "Coins are too recent"						}
	, { CoinErr::CoinstakeCheckTargetFailed						, "check target failed on coinstake"			}
	, { CoinErr::StakeRewardExceeded							, "Stake Reward Exceeded"						}
	, { CoinErr::TxOutBelowMinimum								, "Tx Out value below minimum"					}
	, { CoinErr::SizeLimits										, "Size Limits failed"							}
	, { CoinErr::BlockNotFound									, "Block Not Found"								}
	, { CoinErr::AllowedErrorDuringInitialDownload				, "Allowed Error during Initial Download"		}
	, { CoinErr::UNAUTHORIZED_WORKER							, "Unauthorized worker"							}
	, { CoinErr::CURRENCY_NOT_SUPPORTED							, "This currency not supported"					}
	, { CoinErr::DupTxInputs									, "Duplicate transaction inputs"				}
	, { CoinErr::DupVersionMessage								, "Duplicate Version message"					}
	, { CoinErr::InputsAlreadySpent								, "Inputs already Spent"						}
	, { CoinErr::RejectedV1Block								, "Version 1 block rejected"					}
	, { CoinErr::BlockHeightMismatchInCoinbase					, "Block Height Mismatch in Coinbase"			}
	, { CoinErr::InvalidBootstrapFile							, "Invalid Bootstrap File"						}
	, { CoinErr::TxAmountTooSmall								, "Transaction amount too small"				}
	, { CoinErr::FirstTxIsNotCoinbase							, "First Transaction is not Coinbase"			}
	, { CoinErr::BadCbLength									, "Invalid Coinbase Transaction Script size"	}
	, { CoinErr::BadTxnsPrevoutNull								, "Transaction PrevOut is Null"					}
	, { CoinErr::BadTxnsVinEmpty								, "Transaction has no Inputs"					}
	, { CoinErr::BadTxnsVoutEmpty								, "Transaction has no Outputs"					}
	, { CoinErr::BadTxnsVoutNegative							, "Transaction out value is Negative"			}
	, { CoinErr::BadPrevBlock									, "Previous Block not found"					}
	, { CoinErr::InvalidPrivateKey								, "Invalid Private Key"							}
	, { CoinErr::VersionMessageMustBeFirst						, "Version message must be first"				}
	, { CoinErr::GetBlocksLocatorSize							, "getblocks message has too many locators"		}
	, { CoinErr::TxMissingInputs								, "Missing inputs"								}
	, { CoinErr::TxPrematureSpend								, "Premature Spend"								}
	, { CoinErr::TxOrdering										, "Transaction order is invalid"				}
	, { CoinErr::CannotReorganizeBeyondPrunedBlocks				, "Cannot reorganize beyond pruned blocks"		}
	, { CoinErr::NonCanonicalCompactSize						, "Non-canonical CompactSize"					}

	, { CoinErr::SCRIPT_ERR_UNKNOWN_ERROR						, "Unknown Script error"						}
	, { CoinErr::SCRIPT_ERR_EVAL_FALSE							, "Script evaluated without error but finished with a false/empty top stack element"	}
	, { CoinErr::SCRIPT_ERR_VERIFY								, "Script failed an OP_VERIFY operation"		}
	, { CoinErr::SCRIPT_ERR_EQUALVERIFY							, "Script failed an OP_EQUALVERIFY operation"	}
	, { CoinErr::SCRIPT_ERR_CHECKMULTISIGVERIFY					, "Script failed an OP_CHECKMULTISIGVERIFY operation"	}
	, { CoinErr::SCRIPT_ERR_CHECKSIGVERIFY						, "Script failed an OP_CHECKSIGVERIFY operation"	}
	, { CoinErr::SCRIPT_ERR_NUMEQUALVERIFY						, "Script failed an OP_NUMEQUALVERIFY operation"	}
	, { CoinErr::SCRIPT_ERR_SCRIPT_SIZE							, "Script is too big"							}
	, { CoinErr::SCRIPT_ERR_PUSH_SIZE							, "Push value size limit exceeded"				}
	, { CoinErr::SCRIPT_ERR_OP_COUNT							, "Operation limit exceeded"					}
	, { CoinErr::SCRIPT_ERR_STACK_SIZE							, "Stack size limit exceeded"					}
	, { CoinErr::SCRIPT_ERR_SIG_COUNT							, "Signature count negative or greater than pubkey count"	}
	, { CoinErr::SCRIPT_ERR_PUBKEY_COUNT						, "Pubkey count negative or limit exceeded"		}
	, { CoinErr::SCRIPT_ERR_BAD_OPCODE							, "Opcode missing or not understood"			}
	, { CoinErr::SCRIPT_ERR_DISABLED_OPCODE						, "Attempted to use a disabled opcode"			}
	, { CoinErr::SCRIPT_ERR_INVALID_STACK_OPERATION				, "Operation not valid with the current stack size"		}
	, { CoinErr::SCRIPT_ERR_INVALID_ALTSTACK_OPERATION			, "Operation not valid with the current altstack size"	}
	, { CoinErr::SCRIPT_ERR_OP_RETURN							, "OP_RETURN was encountered"					}
	, { CoinErr::SCRIPT_ERR_UNBALANCED_CONDITIONAL				, "Invalid OP_IF construction"					}
	, { CoinErr::SCRIPT_ERR_NEGATIVE_LOCKTIME					, "Negative locktime"							}
	, { CoinErr::SCRIPT_ERR_UNSATISFIED_LOCKTIME				, "Locktime requirement not satisfied"			}
	, { CoinErr::SCRIPT_ERR_SIG_HASHTYPE						, "Signature hash type missing or not understood"	}
	, { CoinErr::SCRIPT_ERR_SIG_DER								, "Non-canonical DER signature"						}
	, { CoinErr::SCRIPT_ERR_MINIMALDATA							, "Data push larger than necessary"					}
	, { CoinErr::SCRIPT_ERR_SIG_PUSHONLY						, "Only non-push operators allowed in signatures"	}
	, { CoinErr::SCRIPT_ERR_SIG_HIGH_S							, "Non-canonical signature: S value is unnecessarily high"	}
	, { CoinErr::SCRIPT_ERR_SIG_NULLDUMMY						, "Dummy CHECKMULTISIG argument must be zero"		}
	, { CoinErr::SCRIPT_ERR_MINIMALIF							, "OP_IF/NOTIF argument must be minimal"			}
	, { CoinErr::SCRIPT_ERR_SIG_NULLFAIL						, "Signature must be zero for failed CHECK(MULTI)SIG operation"	}
	, { CoinErr::SCRIPT_ERR_DISCOURAGE_UPGRADABLE_NOPS			, "NOPx reserved for soft-fork upgrades"			}
	, { CoinErr::SCRIPT_ERR_DISCOURAGE_UPGRADABLE_WITNESS_PROGRAM	, "Witness version reserved for soft-fork upgrades"	}
	, { CoinErr::SCRIPT_ERR_PUBKEYTYPE							, "Public key is neither compressed or uncompressed"	}
	, { CoinErr::SCRIPT_ERR_CLEANSTACK							, "Extra items left on stack after execution"		}
	, { CoinErr::SCRIPT_ERR_WITNESS_PROGRAM_WRONG_LENGTH		, "Witness program has incorrect length"			}
	, { CoinErr::SCRIPT_ERR_WITNESS_PROGRAM_WITNESS_EMPTY		, "Witness program was passed an empty witness"		}
	, { CoinErr::SCRIPT_ERR_WITNESS_PROGRAM_MISMATCH			, "Witness program hash mismatch"					}
	, { CoinErr::SCRIPT_ERR_WITNESS_MALLEATED					, "Witness requires empty scriptSig"				}
	, { CoinErr::SCRIPT_ERR_WITNESS_MALLEATED_P2SH				, "Witness requires only-redeemscript scriptSig"	}
	, { CoinErr::SCRIPT_ERR_WITNESS_UNEXPECTED					, "Witness provided for non-witness script"			}
	, { CoinErr::SCRIPT_ERR_WITNESS_PUBKEYTYPE					, "Using non-compressed keys in segwit"				}
	, { CoinErr::SCRIPT_ERR_OP_CODESEPARATOR					, "Using OP_CODESEPARATOR in non-witness script"	}
	, { CoinErr::SCRIPT_ERR_SIG_FINDANDDELETE					, "Signature is found in scriptCode"				}
	, { CoinErr::SCRIPT_DISABLED_OPCODE							, "Disabled OpCode"									}
	, { CoinErr::SCRIPT_RANGE									, "Invalid range"									}
	, { CoinErr::SCRIPT_INVALID_ARG								, "Invalid argument"								}

	, { CoinErr::RPC_MISC_ERROR									, "Exception thrown in command handling"		}
	, { CoinErr::RPC_FORBIDDEN_BY_SAFE_MODE						, "Server is in safe mode, and command is not allowed in safe mode"	}
	, { CoinErr::RPC_TYPE_ERROR									, "Unexpected type was passed as parameter"		}
	, { CoinErr::RPC_WALLET_ERROR								, "Unspecified problem with wallet (key not found etc.)"	}
	, { CoinErr::RPC_INVALID_ADDRESS_OR_KEY						, "Invalid address or key"						}
	, { CoinErr::RPC_WALLET_INSUFFICIENT_FUNDS					, "Not enough funds in wallet or account"		}
	, { CoinErr::RPC_OUT_OF_MEMORY								, "Ran out of memory during operation"			}
	, { CoinErr::RPC_INVALID_PARAMETER							, "Invalid, missing or duplicate parameter"		}
	, { CoinErr::RPC_CLIENT_NOT_CONNECTED						, "Client is not connected"						}
	, { CoinErr::RPC_CLIENT_IN_INITIAL_DOWNLOAD					, "Still downloading initial blocks"			}
	, { CoinErr::RPC_WALLET_INVALID_ACCOUNT_NAME				, "Invalid account name"						}
	, { CoinErr::RPC_WALLET_KEYPOOL_RAN_OUT						, "Keypool ran out, call keypoolrefill first"	}
	, { CoinErr::RPC_WALLET_UNLOCK_NEEDED						, "Enter the wallet passphrase with walletpassphrase first"	}
	, { CoinErr::RPC_WALLET_PASSPHRASE_INCORRECT				, "The wallet passphrase entered was incorrect"	}
	, { CoinErr::RPC_WALLET_WRONG_ENC_STATE						, "Command given in wrong wallet encryption state (encrypting an encrypted wallet etc.)"	}
	, { CoinErr::RPC_WALLET_ENCRYPTION_FAILED					, "Failed to encrypt the wallet"				}
	, { CoinErr::RPC_WALLET_ALREADY_UNLOCKED					, "Wallet is already unlocked"					}
	, { CoinErr::RPC_DATABASE_ERROR								, "Database error"								}
	, { CoinErr::RPC_DESERIALIZATION_ERROR						, "Error parsing or validating structure in raw format"	}

	, { CoinErr::AUXPOW_ParentHashOurChainId					, "AuxPow Parent has our ChainID"				}
	, { CoinErr::AUXPOW_ChainMerkleBranchTooLong				, "AuxPow Chain Merkle Branch too long"			}
	, { CoinErr::AUXPOW_MerkleRootIncorrect						, "AuxPow Chain Merkle Root Incorrect"			}
	, { CoinErr::AUXPOW_MissingMerkleRootInParentCoinbase		, "AuxPow Missing Merkle Root In Parent Coinbase"	}
	, { CoinErr::AUXPOW_MutipleMergedMiningHeades				, "AuxPow Multiple merged mining headers in coinbase"	}
	, { CoinErr::AUXPOW_MergedMinignHeaderiNotJustBeforeMerkleRoot	, "AuxPow Merged Minign Headeri Not Just Before Merkle Root"	}
	, { CoinErr::AUXPOW_MerkleRootMustStartInFirstFewBytes		, "AuxPow Merkle Root Must Start In First Few Bytes"	}
	, { CoinErr::AUXPOW_MerkleBranchSizeIncorrect				, "AuxPow Merkle Branch Size Incorrect"			}
	, { CoinErr::AUXPOW_WrongIndex								, "AuxPow Wrong Index"							}
	, { CoinErr::AUXPOW_NotAllowed								, "AuxPow not Allowed"							}

	, { CoinErr::NAME_InvalidTx									, "Invalid Namecoin Transaction"				}
	, { CoinErr::NAME_NameCoinTransactionWithInvalidVersion		, "Non-Namecoin Transaction with Namecoin input"	}
	, { CoinErr::NAME_NewPointsPrevious							, "Name New Transaction pointing to previous transaction"	}
	, { CoinErr::NAME_ToolLongName								, "Name is too Long"							}
	, { CoinErr::NAME_ExpirationError							, "Expiration Error"							}

	, { CoinErr::MINER_BAD_CB_FLAG								, "the server detected a feature-signifying flag that it does not allow"				}
	, { CoinErr::MINER_BAD_CB_LENGTH							, "the coinbase was too long (bitcoin limit is 100 bytes)"								}
	, { CoinErr::MINER_BAD_CB_PREFIX							, "the server only allows appending to the coinbase, but it was modified beyond that"	}
	, { CoinErr::MINER_BAD_DIFFBITS								, "\"bits\" were changed"																}
	, { CoinErr::MINER_BAD_PREVBLK								, "the previous-block is not the one the server intends to build on"					}
	, { CoinErr::MINER_BAD_TXNMRKLROOT							, "the block header's merkle root did not match the transaction merkle tree"			}
	, { CoinErr::MINER_BAD_TXNS									, "the server didn't like something about the transactions in the block"				}
	, { CoinErr::MINER_BAD_VERSION								, "the version was wrong"																}
	, { CoinErr::MINER_DUPLICATE								, "the server already processed this block data"										}
	, { CoinErr::MINER_HIGH_HASH								, "the block header did not hash to a value lower than the specified target"			}
	, { CoinErr::MINER_REJECTED									, "a generic rejection without details"													}
	, { CoinErr::MINER_STALE_PREVBLK							, "the previous-block is no longer the one the server intends to build on"				}
	, { CoinErr::MINER_STALE_WORK								, "Stale share, the work this block was based on is no longer accepted"					}
	, { CoinErr::MINER_TIME_INVALID								, "the time was not acceptable"															}
	, { CoinErr::MINER_TIME_TOO_NEW								, "the time was too far in the future"													}
	, { CoinErr::MINER_TIME_TOO_OLD								, "the time was too far in the past"													}
	, { CoinErr::MINER_UNKNOWN_USER								, "the user submitting the block was not recognized"									}
	, { CoinErr::MINER_UNKNOWN_WORK								, "the template or workid could not be identified"										}
	, { CoinErr::MINER_FPGA_PROTOCOL							, "FPGA hardware protocol violation, try to RESET the device"							}
};



static class CoinCategory : public ErrorCategoryBase {
	typedef ErrorCategoryBase base;
public:
	unordered_map<CoinErr, const char*> m_map;

	CoinCategory()
		: base("Coin", FACILITY_COIN)
	{
		for (auto& m : s_coinMessageTable) {
			m_map[m.Code] = m.Msg;
		}
	}

	string message(int errval) const override {
		auto it = m_map.find((CoinErr)errval);
		return it != m_map.end() ? it->second : "Unknown Coin error";
	}
} s_coin_category;

const error_category& coin_category() {
	return s_coin_category;
}

static const char * const s_reasons[] = {
	"bad-cb-flag", "bad-cb-length", "bad-cb-prefix", "bad-diffbits", "bad-prevblk", "bad-txnmrklroot", "bad-txns", "bad-version", "duplicate", "high-hash", "rejected", "stale-prevblk", "stale-work",
	"time-invalid", "time-too-new", "time-too-old", "unknown-user", "unknown-work"
};

void ThrowRejectionError(RCString reason) {
	for (int i=0; i<size(s_reasons); ++i)
		if (s_reasons[i] == reason)
			Throw(CoinErr(int(CoinErr::MINER_BAD_CB_FLAG) + i));
	Throw(E_FAIL);
}

int SubmitRejectionCode(RCString rej) {
	for (int i=0; i<size(s_reasons); ++i)
		if (s_reasons[i] == rej)
			return (int(CoinErr::MINER_BAD_CB_FLAG) & 0xFFFF) + i;
	return int(CoinErr::MINER_REJECTED);
}

} // Coin::
