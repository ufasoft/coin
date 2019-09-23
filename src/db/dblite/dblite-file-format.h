/*######   Copyright (c) 2014-2019 Ufasoft  http://ufasoft.com  mailto:support@ufasoft.com,  Sergey Pavlov  mailto:dev@ufasoft.com ####
#                                                                                                                                     #
# 		See LICENSE for licensing information                                                                                         #
#####################################################################################################################################*/

#pragma once

namespace Ext { namespace DB { namespace KV {

#define UDB_MAGIC "Ufasoft DB"
const uint32_t UDB_VERSION = 0x040100;
const uint32_t COMPATIBLE_UDB_VERSION = 0x040000;

struct TxRecord {
	int64_t Id;
	BeUInt32 MainDbPage;
	uint32_t _res;
};

struct DbHeader {
	char Magic[12];					// 0      UDB_MAGIC
	BeUInt32 Version;				// 4

	BeUInt16 PageSize;				// 16
	uint16_t _res1;					// 18
	BeUInt32 Salt;					// 20		random salt for Hash-Table function to prevent hash collision attacks
	BeUInt32 ChangeCounter;			// 24		some fields are compatible with SQLite3 file format
	BeUInt32 PageCount;				// 28
	BeUInt32 FreePagePoolList;		// 32
	BeUInt32 FreePageCount;			// 36
	uint64_t LastTransactionId;		// 40
	
	char AppName[12];				// 48
	BeUInt32 UserVersion;				// 60

	char FrontEndName[12];			// 64 could be SQLite or OO
	BeUInt32 FrontEndVersion;		// 76
		

	TxRecord LastTxes[11];			// 80

	BeUInt32 FreePages[32];			// 256
};

ENUM_CLASS(TableType) {
	BTree 		= 0
	, HashTable	= 1
} END_ENUM_CLASS(TableType);

ENUM_CLASS(HashType) {
	MurmurHash3	= 0
	, RevIdentity = 1  			// Deprecated
	, Identity = 2,
} END_ENUM_CLASS(HashType);

#pragma pack(push, 1)

struct TableData {
	uint32_t RootPgNo;
	uint8_t KeySize;
	uint8_t Flags;
	uint8_t Type; // TableType
	uint8_t HtType; // HashType

	// for TableType::HashTable
	uint64_t PageMapLength;		//	number of pages

	TableData() {
		ZeroStruct(*this);
	}
};

struct PageHeader {
	uint16_t Num;
	uint8_t Flags;
	uint8_t Data[];

	static const uint8_t
		FLAG_BRANCH 		= 1,
		FLAG_LEAF 			= 2,
		FLAG_OVERLOW 		= 4,
		FLAG_FREE 			= 8,
		FLAGS_KEY_OFFSET = 0x70;	// if HashType == Identity

	uint8_t KeyOffset() const EXT_FAST_NOEXCEPT {
		return (Flags & FLAGS_KEY_OFFSET) >> 4;
	}

	void SetKeyOffset(uint8_t v) EXT_FAST_NOEXCEPT {
		Flags = (Flags & ~FLAGS_KEY_OFFSET) | (v << 4);
	}
};
#pragma pack(pop)

}}} // Ext::DB::KV::
