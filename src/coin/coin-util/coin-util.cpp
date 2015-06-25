#include <el/ext.h>


#include "../util/util.h"

namespace Coin {

struct CoinData {
	String Symbol;
	byte AddressType;
	byte ScriptAddressVersion;
};

class UtilHasherEng : public HasherEng {
	typedef HasherEng base;
public:
	const String Symbol;

	UtilHasherEng(RCString symbol)
		: Symbol(symbol) {
		AddressVersion = Symbol == "GRS" ? 36
			: Symbol == "LTC" ? 48
			: Symbol == "PPC" ? 117
			: Symbol == "DVC" ? 4
			: Symbol == "DOGE" ? 30
			: Symbol == "EAC" ? 93
			: Symbol == "FTC" ? 14
			: Symbol == "IXC" ? 138
			: Symbol == "NMC" ? 52
			: Symbol == "NVC" ? 8
			: Symbol == "XPM" ? 23
			: Symbol == "PTS" ? 56
			: 0;
	}	

protected:
	HashValue HashForAddress(const ConstBuf& cbuf) override {
		if (Symbol == "GRS")
			return GroestlHash(cbuf);
		return base::HashForAddress(cbuf);
	}
};

static HasherEng *g_hasher;

class UtilApp : public CConApp {
public:
	void PrintUsage() {
		cout << "Usage: " << System.get_ExeFilePath().stem() << " {-options} command args"
			"\nOptions:\n"
			"  -a currencyName     Bitcoin|BTC|Litecoin|LTC|Groestlcoin|GRS|...\n"
			"  -h                  this help\n"
			"\nCommands:\n"
			"  pubkey-to-addr <pubkey-as-hexadecimal> [address-ver]\n"
			"    Outputs address of given Public Key\n"
			"      <address-ver> optional integer arg, by default depends on currencyName (0 for Bitcoin)"
			"\n";
	}

	void PubKeyToAddress() {
		if (optind >= Argc) {
			PrintUsage();
			Throw(2);
		}
		Blob pubKey = Blob::FromHexString(Argv[optind++]);
		if (optind < Argc)
			g_hasher->AddressVersion = (byte)atoi(Argv[optind++]);
		Address addr(*g_hasher, Hash160(pubKey));
		cout << addr << endl;
	}

	String GetCurrencySymbol(RCString s) {
		String x = s.ToUpper();
		if (x.Right(4) == "COIN")
			x = s.substr(0, s.length() - 4);
		if (x == "BIT")
			return "BTC";
		if (x == "LITE")
			return "LTC";
		if (x == "GROESTL")
			return "GRS";
		if (x == "PEER" || x == "PP")
			return "PPC";
		if (x == "PRIME")
			return "XPM";
		if (x == "BTC" || x=="LTC" || x=="GRS" || x=="PPC" || x=="XPM")
			return x;
		Throw(E_NOTIMPL);
	}

	HasherEng *CreateHasherEng(RCString currency) {
		return new UtilHasherEng(currency);
	}

	void Execute() override {
		String currency = "BITCOIN";

		for (int arg; (arg = getopt(Argc, Argv, "a:h"))!=EOF;) {
			switch (arg) {
			case 'a':
				currency = optarg;
				break;
			case 'h':
				PrintUsage();
				return;
			default:
				PrintUsage();
				Throw(1);
			}
		}
		if (optind >= Argc) {
			PrintUsage();
			Throw(2);
		}
		g_hasher = CreateHasherEng(GetCurrencySymbol(currency));
		CHasherEngThreadKeeper hasherKeeper(g_hasher);
		
		String command = Argv[optind++];
		if (command == "pubkey-to-addr")
			PubKeyToAddress();
		else {
			PrintUsage();
			Throw(2);
		}
	}


} theApp;

} // Coin::

EXT_DEFINE_MAIN(Coin::theApp)

