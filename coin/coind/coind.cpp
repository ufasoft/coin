/*######     Copyright (c) 1997-2013 Ufasoft  http://ufasoft.com  mailto:support@ufasoft.com,  Sergey Pavlov  mailto:dev@ufasoft.com #######################################
#                                                                                                                                                                          #
# This program is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation;  #
# either version 3, or (at your option) any later version. This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the      #
# implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details. You should have received a copy of the GNU #
# General Public License along with this program; If not, see <http://www.gnu.org/licenses/>                                                                               #
##########################################################################################################################################################################*/

#include <el/ext.h>
using namespace std;

#include "../eng/wallet.h"
using namespace Coin;

const char g_help[] = 
	"Commands:\n"
	"	getbalance\t- Print balance\n"
	"	help\t- this manual\n"
	"	quit\t- exit command loop\n"
	"	select <currency>\t- select currency (BTC, NMC, LTC...)\n";

class CCoindApp : public CConApp {
	typedef CConApp base;
public:
	CCoindApp() {
	}
private:
	ptr<Coin::WalletEng> WalletEng;
	ptr<Wallet> SelectedWallet;

	void ExecuteCommand(const vector<String>& args) {
		String cmd = args[0];
		if (cmd == "help") {
			cout << g_help << endl;
		} else if (cmd == "getbalance") {
			decimal64 balance = SelectedWallet->Balance;
			cout << balance << endl;
		} else if (cmd == "select") {
			String cname = args.at(1).ToUpper();
			for (int i=0; i<WalletEng->Wallets.size(); ++i) {
				ptr<Wallet> w = WalletEng->Wallets[i];
				if (w->m_eng->ChainParams.Name.ToUpper() == cname || w->m_eng->ChainParams.Symbol.ToUpper() == cname) {
					(SelectedWallet = w)->Start();
					return;
				}
			}
			Throw(E_COIN_NoCurrencyWithThisName);
		} else
			Throw(HRESULT_FROM_WIN32(ERROR_INVALID_COMMAND_LINE));
	}

	void Repl() {
		WalletEng = new Coin::WalletEng();
		SelectedWallet = WalletEng->Wallets.at(0);
		SelectedWallet->Start();
		cout << "Selected currency: " << SelectedWallet->m_eng->ChainParams.Name << endl;

		while (true) {
			if (isatty(fileno(stdin)))
				cout << "> " << flush;
			String line;
			if (!getline(cin, line))
				break;
			vector<String> args = line.Split();
			if (args.empty())
				continue;
			String cmd = args[0].ToLower();
			if (cmd == "quit")
				break;
			try {
				ExecuteCommand(args);
			} catch (RCExc& ex) {
				cerr << ex.Message << "\n type HELP command" << endl;
			}
		}
	}

	void Execute() override	{
		if (m_argv.empty())
			Repl();
		else
			ExecuteCommand(m_argv);
	}
} theApp;


EXT_DEFINE_MAIN(theApp)

