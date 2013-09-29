/*######     Copyright (c) 1997-2013 Ufasoft  http://ufasoft.com  mailto:support@ufasoft.com,  Sergey Pavlov  mailto:dev@ufasoft.com #######################################
#                                                                                                                                                                          #
# This program is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation;  #
# either version 3, or (at your option) any later version. This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the      #
# implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details. You should have received a copy of the GNU #
# General Public License along with this program; If not, see <http://www.gnu.org/licenses/>                                                                               #
##########################################################################################################################################################################*/

#include <el/ext.h>

#ifndef UCFG_BITCOIN_SOLO_MINING
#	define UCFG_BITCOIN_SOLO_MINING 0 //!!!
#endif

#include "miner.h"

#if UCFG_COIN_MINER_SERVICE
#	include <el/win/nt.cpp>
#	include <el/win/service.cpp>

#	include "miner-service.cpp"
#endif

using namespace Coin;


#if UCFG_BITCOIN_SOLO_MINING
#	include "../eng/eng.h"

class SoloWallet : public Object, public WalletBase {
	typedef WalletBase base;
public:
	ptr<CoinEng> m_pEng;
	Blob m_blobAddr;

	SoloWallet(ptr<CoinEng> eng)
		:	base(*eng)
		,	m_pEng(eng)
	{}

protected:

	pair<Blob, Blob> GetReservedPublicKey() override {
		return make_pair(Blob(), m_blobAddr);
	}

	void OnProcessBlock(const Block& block) override {
		cerr << "\r " << Eng.ChainParams.Name << " Block " << block.Height << flush;
	}
	
	void OnProcessTx(const Tx& tx) override {}

	void OnEraseTx(const HashValue& hashTx) override {}

	void OnPeriodic() override {}

	void OnSetProgress(float v) override {}
	
	void OnChange() override {}

	bool IsFromMe(const Tx& tx) override { return false; }
};


class SoloMiner : public CoinDb {
public:
	vector<pair<String, String>> Chains;
	vector<ptr<SoloWallet>> SoloWallets;

	~SoloMiner() {
		for (int i=0; i<SoloWallets.size(); ++i)
			UnregisterForMining(SoloWallets[i]);
	}

	void StartChainEngs();
};

void SoloMiner::StartChainEngs() {
	DbWalletFilePath = nullptr;
	Load();

	for (int i=0; i<Chains.size(); ++i) {
		String saddr = Chains[i].first,
			chainname = Chains[i].second;
		ptr<SoloWallet> wallet = new SoloWallet(CoinEng::CreateObject(_self, chainname));
		Address addr(saddr);
		addr.CheckVer(wallet->Eng);
		wallet->m_blobAddr = addr;
		SoloWallets.push_back(wallet);
		wallet->Eng.Start();
		RegisterForMining(wallet);
	}
}

#endif	// UCFG_BITCOIN_SOLO_MINING


#include "bitcoin-sha256.h"


#if UCFG_BITCOIN_USE_CRASHDUMP
#	pragma comment(lib, "ws2_32")
#	include <el/comp/crashdump.h>

class MyCrashDump : public CCrashDump {
public:
	MyCrashDump() {
		if (Typ == CrashDumpType::Email)
			Typ = CrashDumpType::File;
	}
} g_crashDump;
#endif

class CMinerApp : public CConApp, public BitcoinMiner {
public:
	CThreadRef m_tr;		

	CMinerApp() {
		FileDescription = VER_FILEDESCRIPTION_STR;
		SVersion			= VER_PRODUCTVERSION_STR;
		LegalCopyright = VER_LEGALCOPYRIGHT_STR;
		Url = VER_EXT_URL;

		MainBitcoinUrl = "http://127.0.0.1:8332";

#if UCFG_WIN32 && !defined(_WIN64) && UCFG_BITCOIN_USE_CRASHDUMP
		CUnhandledExceptionFilter::I->RestorePrev();
#endif
	}

	void PrintUsage() {
		cerr << "Usage: " << Path::GetFileNameWithoutExtension(System.ExeFilePath) << " {-options}"
#if UCFG_BITCOIN_SOLO_MINING
			" [bitcoin-address | {bitcoin-address chainname}]"
#endif
			    "\nOptions:\n"
#if UCFG_BITCOIN_SOLIDCOIN
				"  -a scrypt|sha256|prime|solid|<seconds>   hashing algorithm (scrypt, sha256, prime or solid), or time between getwork requests 1..60, default 15\n"
#else
				"  -a scrypt|sha256|prime|<seconds>   hashing algorithm (scrypt, sha256 or prime), or time between getwork requests 1..60, default 15\n"
#endif
				"  -A user-agent       Set custom User-agent string in HTTP header, default: Ufasoft bitcoin miner \n"
				"  -g yes|no           set \'no\' to disable GPU, default \'yes\'\n"
				"  -h                  this help\n"
				"  -i index|name       select device from Device List, can be used multiple times, default - all devices\n"
				"  -I intensity        Intensity of GPU usage [-10..10], default 0\n"
#if  UCFG_BITCOIN_LONG_POLLING
				"  -l yes|no           set \'no\' to disable Long-Polling, default \'yes\'\n"
#endif
				"  -o url              in form http://user:password@server.tld:port/path, stratum+tcp://server.tld:port, by default http://127.0.0.1:8332\n"
				"  -t threads          Number of threads for CPU mining, 0..256, by default is number of CPUs (Cores), 0 - disable CPU mining\n"
#if UCFG_BITCOIN_THERMAL_CONTROL
				"  -T temperature      max temperature in Celsius degrees, default: " + Convert::ToString(MAX_GPU_TEMPERATURE) + "\n"
#endif
#if UCFG_COIN_MINER_SERVICE
				"  -S                  Install service\n"
				"  -U                  Uninstall service\n"
#endif
			    "  -v                  Verbose output\n"			
				"  -x type=host:port   Use HTTP or SOCKS proxy. Examples: -x http=127.0.0.1:3128, -x socks=127.0.0.1:1080\n"
			 << endl;
	}

	void Execute() override	{
#if !UCFG_WIN32
		CPreciseTimeBase::s_pCurrent = nullptr;
#endif

#if UCFG_WIN32
		CUsingSockets usingSockets;
#endif

		bool bMine = false;

		vector<String> selectedDevs;

		for (int arg; (arg = getopt(Argc, Argv, "a:A:g:hi:I:"
#if  UCFG_BITCOIN_LONG_POLLING
			"l:"
#endif
#if UCFG_BITCOIN_THERMAL_CONTROL
			"T:"
#endif
#if UCFG_COIN_MINER_SERVICE
			"SU"
#endif
			"o:t:u:p:vx:")) != EOF;) {
			switch (arg) {
			case 'a':
				if (atoi(optarg)) {
					GetworkPeriod = atoi(optarg);
					if (GetworkPeriod<1 || GetworkPeriod>60) {
						cerr << "\nERROR: Time between getwork requests should be 1..60 seconds\n" << endl;
						PrintUsage();
						return;
					}
				} else if (String(optarg) == "scrypt")
					HashAlgo = Coin::HashAlgo::SCrypt;
				else if (String(optarg) == "sha256")
					HashAlgo = Coin::HashAlgo::Sha256;
				else if (String(optarg) == "prime")
					HashAlgo = Coin::HashAlgo::Prime;
#if UCFG_BITCOIN_SOLIDCOIN
				else if (String(optarg) == "solid")
					HashAlgo = Coin::HashAlgo::Solid;
#endif
				else {
#if UCFG_BITCOIN_SOLIDCOIN
					cerr << "\nERROR: Unknown hashing algorithm, should be scrypt, sha256 or solid\n" << endl;
#else
					cerr << "\nERROR: Unknown hashing algorithm, should be scrypt or sha256\n" << endl;
#endif
					PrintUsage();
					return;
				}
				break;
			case 'A':
				UserAgentString = optarg;
				break;
			case 'g':
				if (String(optarg) == "no")
					m_bTryGpu = false;
				break;
			case 'h':
				PrintUsage();
				return;
			case 'i':
				selectedDevs.push_back(optarg);
				bMine = true;
				break;
			case 'I':
				SetIntensity(atoi(optarg));
				break;
#if  UCFG_BITCOIN_LONG_POLLING
			case 'l':
				if (String(optarg) == "no")
					m_bLongPolling = false;
				break;
#endif
			case 'o':
				MainBitcoinUrl = optarg;
				bMine = true;
				break;
			case 'u':
				Login = optarg;
				break;
			case 'p':
				Password = optarg;
				break;
			case 't':
				ThreadCount = atoi(optarg);
				break;
#if UCFG_BITCOIN_THERMAL_CONTROL
			case 'T':
				MaxGpuTemperature = Temperature::FromCelsius((float)atoi(optarg));
				break;
#endif
			case 'v':
				++Verbosity;
				break;
			case 'x':
				ProxyString = optarg;
				break;
#if UCFG_COIN_MINER_SERVICE
			case 'S':
				InstallMinerService();
				return;
			case 'U':
				UninstallMinerService();
				return;
#endif
			default:
				PrintUsage();
				Throw(1);
			}
		}

#if defined(_M_IX86) && UCFG_WIN32
		if (System.Is64BitNativeSystem()) {
//annonying			cerr << "Warning: You have 64-bit OS. Run " << Path::GetFileNameWithoutExtension(System.ExeFilePath)+"-64"+Path::GetExtension(System.ExeFilePath) << " for better performance" << endl;
		}
#endif

		if (Coin::HashAlgo::Prime == HashAlgo)
			m_bTryGpu = false;
		if (Coin::HashAlgo::Sha256 != HashAlgo)
			m_bTryFpga = false;

		BitcoinMiner *miner = this;

#if UCFG_BITCOIN_SOLO_MINING
		SoloMiner soloMiner;
		if (optind < Argc) {
			if (optind == Argc-1)
				soloMiner.Chains.push_back(make_pair(String(Argv[optind]), String("bitcoin")));
			else {
				for (; optind < Argc-1; optind+=2)
					soloMiner.Chains.push_back(make_pair(String(Argv[optind]), String(Argv[optind+1])));
			}
			miner = &soloMiner;
			soloMiner.StartChainEngs();
		}
#endif
		if (Verbosity)
			CTrace::s_nLevel = (1 << Verbosity)-1;

		if (miner->ThreadCount > 256) {
			cerr << "Thread number should be 0..256" << endl;
			Throw(2);
		}

		miner->m_tr = &m_tr;
		miner->InitDevices(selectedDevs);

		if (!bMine) {
			PrintUsage();
			
			cerr << "Device List:\n";
			for (int i=0; i<Devices.size(); ++i) {
				ComputationDevice& dev = *Devices[i];
				cerr << setw(3) << i+1 << ". " << dev.Name << "\t" << dev.Description << "\n";
			}
			cerr << endl;
			m_tr.StopChilds();
			return;
		}


		if (selectedDevs.empty())  {
			if (Devices.size() > 1)
				Devices.erase(Devices.begin());							// disable CPU if we have other devices		
		} else {
			vector<ptr<ComputationDevice>> devices;
			for (int i=0; i<selectedDevs.size(); ++i) {
				String s = selectedDevs[i];
				if (int idx = atoi(s)) {
					if (idx > Devices.size())
						Throw(E_FAIL);
					devices.push_back(Devices[idx-1]);
				} else {
					for (int j=0; j<Devices.size(); ++j) {
						if (Devices[j]->Name == s) {
							devices.push_back(Devices[j]);
							break;
						}
					}
				}
			}
			Devices = devices;
		}

#if UCFG_COIN_MINER_SERVICE
		try {
			DBG_LOCAL_IGNORE_WIN32(ERROR_ACCESS_DENIED);
			DBG_LOCAL_IGNORE_WIN32(ERROR_FAILED_SERVICE_CONTROLLER_CONNECT);

			RunMinerAsService(*miner);
			return;
		} catch (AccessDeniedExc) {
		} catch (RCExc ex) {
			if (ex.HResult != HRESULT_FROM_WIN32(ERROR_FAILED_SERVICE_CONTROLLER_CONNECT))
				throw;
		}		
#endif

		cout << "Mining for " << MainBitcoinUrl << endl;		
		bool bComma = false;
		cout << "Using ";
		for (int i=0; i<Devices.size(); ++i) {
			if (exchange(bComma, true))
				cout << ", ";
			String name = Devices[i]->Name;
			cout << name;
			if (name == "CPU")
				cout << " (" << ThreadCount << " threads)";
		}
		cout << endl;

		miner->Start(&m_tr);

		while (!s_bSigBreak) {
			Thread::Sleep(2000);			
			ostringstream os;
			os << "\r" << setprecision(4);
			if (miner->HashAlgo == Coin::HashAlgo::Prime)
				os << (int)Speed << " Prime";
			else if (Speed < 100000)
				os << Speed/1000 << " kHash";
			else if (Speed < 100000000)
				os << Speed/1000000 << " MHash";
			else
				os << Speed/1000000000 << " GHash";
			os << "/s " << String(' ' , 20 - int(os.tellp()));

			struct NTemp {
				int N;
				Temperature Temp;

				NTemp()
					:	N(0)
				{}
			};

			map<String, NTemp> m;
			EXT_LOCK (miner->MtxDevices) {
				for (int i=0; i<miner->Devices.size(); ++i) {
					String name = miner->Devices[i]->Name;
					if (name == "CPU" && miner->ThreadCount > 0)
						m["CPU thread"].N = miner->ThreadCount;
					else {
						NTemp& ntemp = m[name];
						ntemp.N++;
						ntemp.Temp = miner->Devices[i]->Temperature;
					}
				}
			}
			for (map<String, NTemp>::iterator it=m.begin(), e=m.end(); it!=e; ++it) {
				if (it != m.begin())
					os << ", ";
				os << it->second.N << " " << it->first;
				if (it->second.N > 1)
					os << "s";
				if (it->second.Temp != Temperature::Zero) {
					os << " T=" << it->second.Temp;
				}
			}
			int pos = int(os.tellp());
			if (pos < 79)
				os << String(' ' , 79-pos);
			cerr << os.str() << flush;
		}
		miner->Stop();		//!!! we can't stop LongPolling with easy libcurl

		double sec = (DateTime::UtcNow()-DtStart).TotalSeconds;
		cerr << "\nProcessed: " << EntireHashCount/1000000 << " Mhash, " << int(sec) << " s with average Rate: " << miner->Speed/1000000  << " MHash/s"
			<< "\nAccepted: " << AcceptedCount << ", average: " << AcceptedCount*60/sec << " shares/min"
			<< endl;
	}

	bool OnSignal(int sig) override {
		CConApp::OnSignal(sig);
		return true;
	}
protected:

} theApp;



EXT_DEFINE_MAIN(theApp)


