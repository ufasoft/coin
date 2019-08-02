/*######   Copyright (c) 2011-2015 Ufasoft  http://ufasoft.com  mailto:support@ufasoft.com,  Sergey Pavlov  mailto:dev@ufasoft.com ####
#                                                                                                                                     #
# 		See LICENSE for licensing information                                                                                         #
#####################################################################################################################################*/

using System;
using System.Collections.Generic;
using System.Configuration;
using System.Linq;
using System.Windows;
using System.Runtime.InteropServices;
using System.Runtime.InteropServices.ComTypes;

using Interop.coineng;
using GuiComp;


namespace Coin {

	public class App : Application {
		App() {
			this.DispatcherUnhandledException += (s, e) => {
				MessageBox.Show(e.Exception.Message, "Error", MessageBoxButton.OK, MessageBoxImage.Error);
				e.Handled = true;
			};
		}

		public static Uri SendUri;

        public static bool
            CancelPendingTxes,
            Testnet;

		void ProcessCommandLine(string[] args) {
			SendUri = null;
			for (int i = 0; i < args.Length; ++i) {
				string arg = args[i];
				switch (arg) {
					case "-cancelpending":
						CancelPendingTxes = true;
						break;
                    case "-testnet":
                        Testnet = true;
                        break;
                    default:
						if (Uri.IsWellFormedUriString(arg, UriKind.Absolute)) {
							Uri uri = new Uri(arg);
							if (uri.Scheme != "bitcoin")
								throw new ApplicationException("URI should be in form bitcoin:addresss");
							SendUri = uri;
						} else {
							MessageBox.Show("Invalid command line argument: " + arg, "Error", MessageBoxButton.OK, MessageBoxImage.Error);
							Environment.Exit(2);
						}
						break;
				}
			}
		}

		protected override void OnStartup(StartupEventArgs e) {
			base.OnStartup(e);
			ProcessCommandLine(e.Args);
		}

		internal void OnStartupNextInstance(string[] args) {
			//            base.OnStartupNextInstance(e);
			MainWindow.Activate();
			ProcessCommandLine(args);
			FormMain.I.CheckForCommands();
		}

		[System.Diagnostics.DebuggerNonUserCodeAttribute()]
//		[System.CodeDom.Compiler.GeneratedCodeAttribute("PresentationBuildTasks", "4.0.0.0")]
		public void InitializeComponent() {
			this.StartupUri = new System.Uri("f_main.xaml", System.UriKind.Relative);
		}

		[System.STAThreadAttribute()]
//		[System.Diagnostics.DebuggerNonUserCodeAttribute()]
//		[System.CodeDom.Compiler.GeneratedCodeAttribute("PresentationBuildTasks", "4.0.0.0")]
		public static void Main() {
			Coin.App app = new Coin.App();
			app.InitializeComponent();

//			app.Run();
			new SingleInstanceApplicationWrapper(app).Run(Environment.GetCommandLineArgs());
		}

	}


	class SingleInstanceApplicationWrapper : Microsoft.VisualBasic.ApplicationServices.WindowsFormsApplicationBase {
		App App;

		public SingleInstanceApplicationWrapper(App app) {
			App = app;
			IsSingleInstance = true;
		}

		public int ReturnCode {
			get;
			private set;
		}

		protected override bool OnStartup(Microsoft.VisualBasic.ApplicationServices.StartupEventArgs eventArgs) {
			ReturnCode = App.Run();
			return false;
		}

		protected override void OnStartupNextInstance(Microsoft.VisualBasic.ApplicationServices.StartupNextInstanceEventArgs eventArgs) {
			System.Collections.ObjectModel.ReadOnlyCollection<string> argsCollection = eventArgs.CommandLine;
			int count = argsCollection.Count - 1;
			string[] args = new string[count];
			for (int i = 0; i < count; ++i)
				args[i] = argsCollection[i + 1];
			App.OnStartupNextInstance(args);
		}
	}

	class Const {
		public const Int64 BTC_DIV = 100000000;
	};

	class CoinEng {
		[DllImport("coineng.dll")]
		public extern static ICoinEng CoinCreateObj();
	}


}

