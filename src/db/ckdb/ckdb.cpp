/*######   Copyright (c) 2014-2015 Ufasoft  http://ufasoft.com  mailto:support@ufasoft.com,  Sergey Pavlov  mailto:dev@ufasoft.com ####
#                                                                                                                                     #
# 		See LICENSE for licensing information                                                                                         #
#####################################################################################################################################*/

#include <el/ext.h>

#include <db/dblite/dblite.h>

using namespace Ext::DB::KV;

class CkStorage : public KVStorage {
	typedef KVStorage base;
public:

	void CheckPgno(uint32_t pgno) {
		if (pgno >= BmUsedPages.size())
			BmUsedPages.resize(pgno+1);
		if (!BmUsedPages.test(pgno))
			BmUsedPages.set(pgno);
	}

	void Check();
	void CheckFreePages();
	void PrintReport();
protected:
	dynamic_bitset<> BmUsedPages;

	void OnOpenPage(uint32_t pgno) override {
		CheckPgno(pgno);
	}
};



class CCkDbApp : public CConApp {
public:
	void PrintUsage() {
		cerr << "Usage:\n"
			<< "  " << path(Argv[0]).filename() << " <filename>.udb" << endl;
	}

	void Execute() override {
		if (Argc < 2) {
			PrintUsage();
			Environment::ExitCode = 1;
			return;
		}
		CkStorage storage;
		storage.UseMMapPager = false;
		storage.ReadOnly = true;										//!!! Mapping don't work for unaligned ReadOnly file
		storage.m_accessViewMode = ViewMode::Window;			// to count Pages; ViewMode::Full avoids OpenPage() calls
		storage.Open(Argv[1]);
		storage.Check();
		storage.CheckFreePages();
		storage.PrintReport();
	}
} theApp;

EXT_DEFINE_MAIN(theApp)

#include <db/dblite/hash-table.h>

void CkStorage::Check() {
#ifdef X_DEBUG//!!!D
	DbTable t("txes");
	DbReadTransaction tx(_self);
	DbCursor c(tx, t);
	UInt32 nPage = 0xE9A2E31;
	while (true) {

		if (!c.SeekToPrev())
			break;

		ConstBuf key = c.get_Key();
		cout << "Key " << key << "  Hash: " << hex << MurmurHash3_32(key, Salt);
		ConstBuf data = c.get_Data();
		cout << key.Size << " " << data.Size << "\n";

		HtCursor *cht = dynamic_cast<HtCursor*>(c.m_pimpl.get());
	LAB_AGAIN:
		if (cht->NPage == nPage) {
			nPage &= ~(1 << (BitOps::ScanReverse(nPage) - 1));
			cht->NPage = nPage;
			cht->Top().Page = cht->Map->Tx.OpenPage(cht->Ht->GetPgno(cht->NPage));
			cht->Top().Pos = NumKeys(cht->Top().Page);
			if (cht->Top().Pos == 0)
				goto LAB_AGAIN;
			cht = cht;
			
		}
	}
	return;
#endif


	cout << "Checking file " << FilePath << "\n";
	uint32_t ver = letoh(DbHeaderRef().Version);
	cout << "Version: " << Version((ver >> 16) & 255, (ver >> 8) & 255)
		<< "\tPageSize: " << int(DbHeaderRef().PageSize) << "\n"
		<< "App: " << AppName << " " << UserVersion << "\n";

	DbReadTransaction txS(_self);
	int nProgress = m_stepProgress;


	struct Space_Throusands : numpunct<char> { 
   		char do_thousands_sep() const override { return ' '; } 
		string do_grouping() const override { return "\3"; }
	};
	cout.imbue(locale(locale(), new Space_Throusands));

	cout << "\nTable           Key Type     Records        Pages        Bytes"
			"\n--------------- --- -------- -------------- ------------ ------------\n";
	for (DbCursor ct(txS, DbTable::Main()); ct.SeekToNext();) {
		String tableName = Encoding::UTF8.GetChars(ct.Key);
		const TableData& td = *(const TableData*)ct.get_Data().P;
		ostringstream os;

		os << tableName;

		os << string((max)(1, 16 - (int)os.tellp()), ' ');
		os << (td.KeySize ? Convert::ToString(int(td.KeySize)) : String("var"));
		
		os << string((max)(1, 20 - (int)os.tellp()), ' ');
		switch ((TableType)td.Type) {
		case TableType::BTree: 		os << "B-Tree"; break;
		case TableType::HashTable: 	os << "Hash/";
			switch ((HashType)td.HtType) {
			case HashType::MurmurHash3: os << "Mur"; break;
			case HashType::RevIdentity: os << "Rev"; break;
			case HashType::Identity: os << "Id"; break;
			default:
				os << "Unk";
			}
			break;
		default:
			os << "UNKNOWN";
		}

		os << string((max)(1, 29 - (int)os.tellp()), ' ');
		cout << os.str();

		DbTable tS(tableName);
		int64_t nRecords = 0, 
			bytes = 0;
		size_t nPages = BmUsedPages.count();
		for (DbCursor c(txS, tS); c.SeekToNext();) {
			c.get_Key();
			c.get_Data();
			if (m_pfnProgress && !--nProgress) {
				if (m_pfnProgress(m_ctxProgress))
					Throw(errc::operation_canceled);
				nProgress = m_stepProgress;
			}
			++nRecords;
			bytes += c.get_Key().Size + c.get_Data().Size;
		}
		nPages = BmUsedPages.count() - nPages;
		cout << setw(13) << nRecords << " " << setw(10) << nPages << " " << setw(15) << bytes << "\n";
	}
	cout << endl;
	cout.imbue(locale());
}

void CkStorage::CheckFreePages() {
	for (uint32_t pgno : FreePages)
		CheckPgno(pgno);
}

void CkStorage::PrintReport() {
	cout << "Free Pages: " << FreePages.size() << "\n";
	set<uint32_t> setLeaked;
	for (uint32_t pgno=2; pgno<PageCount; ++pgno) {
		if (pgno>=BmUsedPages.size() || !BmUsedPages[pgno])
			setLeaked.insert(pgno);
	}
	if (setLeaked.empty())
		cout << "No Leaks" << endl;
	else {
		cout << "Leaks:\n";
		for (uint32_t pgno : setLeaked)
			cout << pgno << " ";
		cout << endl;
	}
}


