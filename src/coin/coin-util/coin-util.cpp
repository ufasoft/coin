#include <el/ext.h>


#include "../util/util.h"

namespace Coin {

struct CoinData {
	byte AddressType;
};

class UtilHasherEng : public HasherEng {
	typedef HasherEng base;
public:
	String Symbol;

	UtilHasherEng(RCString symbol)
		: Symbol(symbol) {
	}	

protected:
	HashValue HashForAddress(const ConstBuf& cbuf) override {
		if (Symbol == "GRS")
			return GroestlHash(cbuf);
		return base::HashForAddress(cbuf);
	}
};

class UtilApp : public CConApp {
public:
	void PrintUsage() {
		cout << "Usage: " << System.get_ExeFilePath().stem() << " {-options} command args"
			"\nOptions:\n"
			"  -a currencyName     Bitcoin|Litecoin|Groestlcoin...\n"
			"  -h                  this help\n"
			"\nCommands:\n"
			"  pubkey-to-address <pubkey-as-hexadecimal>\n"
			"    Outputs address of geven PublickKey\n"

	}

	void PubKeyToAddress() {
		if (optind < Argc) {
			PrintUsage();
			Throw(2);
		}
		Blob pubKey = Blob::FromHexString(Argv[optind]);
	}

	String GetCanonicCurrencyName(RCString s) {
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

		for (int arg; (arg = getopt(Argc, Argv, "a:"));) {
		case 'a':
			currency = optarg;
			break;
		case 'h':
			PrintUsage();
			return;

		}
		if (optind < Argc) {
			PrintUsage();
			Throw(2);
		}
		HasherEng *hasher = CreateHasherEng(GetCanonicCurrencySymbol(currency));
		CHasherEngThreadKeeper hasherKeeper(hasher);


		String command = Argv[optind++];
		if (command == "pubkey-to-address")
			PubKeyToAddress();
		else {
			PrintUsage();
			Throw(2);
		}
	}


} threApp;

} // Coin::

EXT_DEFINE_MAIN(Coin::theApp)
