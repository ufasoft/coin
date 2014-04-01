#pragma once

namespace Ext { namespace DB { namespace KV {

#define UDB_MAGIC "Ufasoft DB"
const UInt32 UDB_VERSION = 0x040100;
const UInt32 COMPATIBLE_UDB_VERSION = 0x040000;

struct TxRecord {
	Int64 Id;
	BeUInt32 MainDbPage;
	UInt32 _res;
};

struct DbHeader {
	char Magic[12];					// 0      UDB_MAGIC
	BeUInt32 Version;				// 4

	BeUInt16 PageSize;				// 16
	UInt16 _res1;					// 18
	BeUInt32 Salt;					// 20		random salt for Hash-Table function to prevent hash collision attacks
	BeUInt32 ChangeCounter;			// 24		some fields are compatible with SQLite3 file format
	BeUInt32 PageCount;				// 28
	BeUInt32 FreePagePoolList;		// 32
	BeUInt32 FreePageCount;			// 36
	UInt64 LastTransactionId;		// 40
	
	char AppName[12];				// 48
	BeUInt32 UserVersion;				// 60

	char FrontEndName[12];			// 64 could be SQLite or OO
	BeUInt32 FrontEndVersion;		// 76
		

	TxRecord LastTxes[11];			// 80

	BeUInt32 FreePages[32];			// 256
};

ENUM_CLASS(TableType) {
	BTree 		= 0,
	HashTable 	= 1
} END_ENUM_CLASS(TableType);

ENUM_CLASS(HashType) {
	MurmurHash3	= 0
} END_ENUM_CLASS(HashType);

#pragma pack(push, 1)

struct TableData {
	UInt32 RootPgNo;
	byte KeySize;
	byte Flags;
	byte Type;				// TableType
	byte HtType;			// HashType

	// for TableType::HashTable
	UInt64 PageMapLength;		//	number of pages

	TableData() {
		ZeroStruct(*this);
	}
};

struct PageHeader {
	UInt16 Num;
	byte Flags;
	byte Data[];

};
#pragma pack(pop)

}}} // Ext::DB::KV::
