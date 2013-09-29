using System;
using System.Collections.Generic;
using System.Collections.ObjectModel;
using System.Linq;
using System.IO;
using System.Text;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Media.Imaging;
using System.Windows.Input;
using Microsoft.Win32;
using System.Globalization;
using System.Runtime.InteropServices;
using System.Threading;
using System.Windows.Threading;
using System.Reflection;
using System.ComponentModel;
using System.Text.RegularExpressions;
using System.Windows.Data;

using GuiComp;

using Interop.coineng;


namespace Coin {
	public partial class FormMain : Window {
		public static FormMain I;
		CultureInfo CultureInfo = (CultureInfo)Thread.CurrentThread.CurrentCulture.Clone();

		public FormMain() {
			InitializeComponent();

			I = this;
			CultureInfo.NumberFormat = (NumberFormatInfo)CultureInfo.NumberFormat.Clone();
			CultureInfo.NumberFormat.CurrencySymbol = "";
			Thread.CurrentThread.CurrentCulture = CultureInfo;
		}

		public ICoinEng Eng = CoinEng.CoinCreateObj();

		Dictionary<ITransaction, object> AllTxes = new Dictionary<ITransaction, object>();
		Dictionary<IWallet, WalletForms> m_wallet2forms = new Dictionary<IWallet, WalletForms>();
		Dictionary<string, int> AllAlerts = new Dictionary<string, int>();

		public static RegistryKey UserAppRegistryKey {
			get {
				return Registry.CurrentUser.CreateSubKey(@"Software\Ufasoft\Coin");
			}
		}

		public class WalletEvent {
			public DateTime Timestamp { get; set; }
			public string Comment { get; set; }
		}

		ObservableCollection<WalletForms> ActiveWalletForms = new ObservableCollection<WalletForms>();
		ObservableCollection<WalletEvent> WalletEvents = new ObservableCollection<WalletEvent>();



		void UpdateData(WalletForms wf) {
			var w = wf.Wallet;

			int i = 0;
			foreach (var alert in w.Alerts) {
				string comment = alert.Comment;
				if (!AllAlerts.ContainsKey(comment)) {
					AllAlerts.Add(comment, 1);
					WalletEvents.Add(new WalletEvent() { Timestamp = alert.UntilTimestamp, Comment = comment });
				}
			}

			foreach (var tx in w.Transactions) {
				if (!AllTxes.ContainsKey(tx)) {
					AllTxes.Add(tx, null);
					var addr = tx.Address;
					try {
						decimal amount = tx.Amount;
						string s;
						if (amount > 0)
							s = string.Format("{0} {1} received to our ", amount, w.CurrencySymbol);
						else
							s = string.Format("{0} {1} sent to", -amount, w.CurrencySymbol);
						s += string.Format(" address {0} {1} {2}", addr.Value, addr.Comment, tx.Comment);
						WalletEvents.Add(new WalletEvent() { Timestamp = tx.Timestamp, Comment = s });
						if (++i > 10)
							break;
					}
					finally {
						Marshal.FinalReleaseComObject(addr);
					}
				}
			}
			if (wf.FormTransactions != null)
				wf.FormTransactions.UpdateTransactions();
		}


		void UpdateView() {
			WalletEvents.Clear();
			foreach (var wf in ActiveWalletForms)
				UpdateData(wf);
		}

		void AddWalletToList(WalletForms wf) {
			wf.Wallet.Enabled = true;
			if (App.CancelPendingTxes)
				wf.Wallet.CancelPendingTxes();
			wf.Wallet.MiningEnabled = wf.MiningEnabled;			
			UpdateData(wf);
			ActiveWalletForms.Add(wf);
//			LvWallet.ItemsSource = ActiveWalletForms;
		}

		bool m_Loaded;

		void SaveActiveCurrencies() {
			var list = new List<string>();
			foreach (var w in Eng.Wallets)
				if (w.Enabled)
					list.Add(w.CurrencyName + (w.MiningEnabled ? " Mining" : ""));
			UserAppRegistryKey.SetValue("ActiveCurrencies", list.ToArray());
		}


		void Currency_CheckChanged(object s, RoutedEventArgs e) {
			var wf1 = (WalletForms)((MenuItem)s).Tag;
			if (wf1.MenuItem.IsChecked) {
				AddWalletToList(wf1);
			} else {
				wf1.Wallet.Enabled = false;
				ActiveWalletForms.Remove(wf1);
				//						LvWallet.ItemsSource = ActiveWalletForms;
				wf1.Free();
			}
			if (m_Loaded)
				SaveActiveCurrencies();
		}


		DispatcherTimer timer1 = new DispatcherTimer() { Interval = TimeSpan.FromSeconds(2) };

		void RegisterUriHandler() {
			try {
				if ((int)UserAppRegistryKey.GetValue("UrlRegAsked", 0) == 0) {
					UserAppRegistryKey.SetValue("UrlRegAsked", 1);
					RegistryKey keyClasses = Registry.CurrentUser.OpenSubKey("Software").OpenSubKey("Classes", true);
					string toMe = string.Format("\"{0}\" %1", Assembly.GetCallingAssembly().Location);
					var subKey = keyClasses.OpenSubKey("bitcoin");
					if (subKey == null) {
						keyClasses.CreateSubKey("bitcoin").CreateSubKey("shell").CreateSubKey("open").CreateSubKey("command").SetValue(null, toMe);
					}  else {
						var keyCommand = subKey.OpenSubKey("shell").OpenSubKey("open").OpenSubKey("command");
						string path = (string)keyCommand.GetValue(null, "");
						if (path != toMe) {
							if (MessageBox.Show("Register Bitcoin URI for this Application", "Coin", MessageBoxButton.YesNo, MessageBoxImage.Question) == MessageBoxResult.Yes)
								keyCommand.SetValue(null, toMe);
						}
					}
				}
			} catch (Exception) {

			}		
		}

		private void Window_Loaded(object sender, RoutedEventArgs e) {
			LvWallet.ItemsSource = ActiveWalletForms;
			LvEvent.ItemsSource = WalletEvents;

			var port = UserAppRegistryKey.GetValue("LocalPort");
			if (port != null) {
				try {
					Eng.LocalPort = Convert.ToUInt16(port);
				} catch (Exception) {
				}
			}
			
			Wallet[] wallets = null;
			try {
				wallets = Eng.Wallets;
			} catch (Exception x) {
				MessageBox.Show(x.Message, "Error", MessageBoxButton.OK, MessageBoxImage.Error);
				Application.Current.Shutdown();
				return;
			}

			foreach (var wallet in wallets) {
                string currencyName = wallet.CurrencyName;

				var wf = new WalletForms();
				wf.Wallet = wallet;
                MenuItem mi = new MenuItem();
                wf.MenuItem = mi;
				mi.Header = string.Format("{0} ({1})", currencyName, wallet.CurrencySymbol);                
//                mi.Icon = new Image() { Source = new BitmapImage(new Uri(String.Format("images/{0}.ico", currencyName), UriKind.Relative)) };
				menuCurrency.Items.Add(mi);
				mi.IsCheckable = true;
				mi.Tag = wf;
				mi.Checked += Currency_CheckChanged;
				mi.Unchecked += Currency_CheckChanged;
				m_wallet2forms[wallet] = wf;
			}

			timer1.Tick += (s, e1) => {
				bool b = false;
				foreach (var wf in ActiveWalletForms)
					b |= wf.CheckForChanges();
				if (b)
					UpdateView();
			};
			timer1.Start();
			List<WalletForms> ar = new List<WalletForms>();
			var obj = UserAppRegistryKey.GetValue("ActiveCurrencies", null);
			if (obj == null) {
				ar = (from de in m_wallet2forms select de.Value).Take(2).ToList();
				//!!!R				foreach (var de in m_wallet2forms)
				//!!!R					ar.Add(de.Value);
			} else {
				foreach (var s in (string[])obj) {
					string[] ss = s.Split();
					string name = ss[0];
					try {
						WalletForms wf = null;
						foreach (var pp in m_wallet2forms)
							if (pp.Key.CurrencyName == name) {
								wf = pp.Value;
								break;
							}
						if (wf != null) {
							wf.MiningEnabled = false;
							foreach (var subs in ss) {
								//!!! wf.MiningEnabled |= subs == "Mining";  // CRASH danger when Mining on Start
							}
							ar.Add(wf);
						}
					} catch (Exception) {
					}
				}
			}
			foreach (var wf in ar) {
				try {
					wf.MenuItem.IsChecked = true;
				} catch (Exception ex) {
					MessageBox.Show(ex.Message, "Coin", MessageBoxButton.OK, MessageBoxImage.Error);
				}
			}
			m_Loaded = true;
			UpdateView();
			CheckForCommands();
			RegisterUriHandler();

//			OnFileImport(null, null);//!!!D
		}

		public bool EnsurePassphraseUnlock() {
			if (Eng.NeedPassword) {
				var dlg = new FormPassphrase();
				dlg.labelRetype.Visibility = Visibility.Hidden;
				dlg.textRetype.Visibility = Visibility.Hidden;
				dlg.Title = "Wallet Password";
				if (Dialog.ShowDialog(dlg, this)) {
					Eng.Password = dlg.textPassword.Password;
				} else
					return false;
			}
			return true;
		}

		void ReleaseComObjects() {
			foreach (var pp in AllTxes)				//!!!?
				Marshal.FinalReleaseComObject(pp.Key);
			AllTxes.Clear();
			while (m_wallet2forms.Count > 0) {
				IWallet w = null;
				foreach (var de in m_wallet2forms) {
					w = de.Key;
					de.Value.Free();
					break;
				}
				m_wallet2forms.Remove(w);
				Marshal.FinalReleaseComObject(w);
			}
			Marshal.FinalReleaseComObject(Eng);
		}

		void OnFileExit(object sender, RoutedEventArgs argg) {
			Close();
		}

		void OnToolsOptions(object sender, RoutedEventArgs argg) {
			var d = new FormOptions();
			d.textListeningPort.Text = Eng.LocalPort.ToString();
			if (Dialog.ShowDialog(d, this)) {
				UInt16 port = Convert.ToUInt16(d.textListeningPort.Text);
				Eng.LocalPort = port;
				UserAppRegistryKey.SetValue("LocalPort", port, RegistryValueKind.DWord);
			}
		}

		WalletForms SelectedWallet() {
			return (WalletForms)LvWallet.SelectedItem;
		}

		WalletForms FindWallet(string netName) {
			var r = ActiveWalletForms.FirstOrDefault(w => w.Wallet.CurrencyName.ToUpper() == netName.ToUpper());
			if (r == null)
				throw new ApplicationException(string.Format("No active Wallet with name {0}", netName));
			return r;
		}

		public void SendMoney(string netName, string address, decimal amount, string label, string comment) {
			var dlg = new FormSendMoney();
			dlg.Wallet = FindWallet(netName).Wallet;
			try {
				dlg.Wallet.AddRecipient(address, label);
			} catch (Exception) {
			}
			if (comment == "")
				comment = label;
			dlg.textAddress.Text = address;
			dlg.textAmount.Text = amount.ToString();
			dlg.textComment.Text = comment;
			Dialog.ShowDialog(dlg, this);
		}

		void OnSendMoney(object sender, RoutedEventArgs argg) {
			var dlg = new FormSendMoney();
			dlg.Wallet = SelectedWallet().Wallet;
			Dialog.ShowDialog(dlg, this);
		}

		public void CheckForCommands() {
			if (App.SendUri != null) {
				Uri uri = App.SendUri;
				string address = uri.AbsolutePath;
				string label = "", message = "";
				decimal amount = 0;
				Regex s_reParam = new Regex(@"\??([^=]+)=(.*)");
				foreach (var pp in uri.Query.Split('&')) {
					Match m = s_reParam.Match(pp);
					if (m.Success) {
						string name = m.Groups[1].Value,
							val = Uri.UnescapeDataString(m.Groups[2].Value);
						switch (name) {
							case "amount":
								amount = decimal.Parse(val);
								break;
							case "label":
								label = val;
								break;
							case "message":
								message = val;
								break;
						}
					}
				}
				SendMoney(uri.Scheme, address, amount, label, message);
			}
		}

		private void Window_Closed(object sender, EventArgs e) {
			Eng.Stop();
			ReleaseComObjects();
		}

		void ShowTransactions() {
			WalletForms wf = SelectedWallet();
			if (wf.FormTransactions == null)
				wf.FormTransactions = new FormTransactions() { WalletForms = wf };
			wf.FormTransactions.Show();
			wf.FormTransactions.Activate();
		}

		private void OnTransactions(object sender, RoutedEventArgs e) {
			ShowTransactions();
		}

		private void LvWallet_MouseDoubleClick(object sender, MouseButtonEventArgs e) {
			ShowTransactions();
		}

		private void OnAddressBook(object sender, RoutedEventArgs e) {
			WalletForms wf = SelectedWallet();
			if (wf.FormAddressBook == null)
				wf.FormAddressBook = new FormAddressBook() { WalletForms = wf };
			wf.FormAddressBook.Show();
			wf.FormAddressBook.Activate();
		}

		private void OnHelpAbout(object sender, RoutedEventArgs e) {
			var d = new GuiComp.DialogAbout();
			d.Image.Source = new BitmapImage(new Uri("/coin;component/coin-3.ico", UriKind.Relative));
			Dialog.ShowDialog(d, this);
		}

		private void OnToolsRescan(object sender, RoutedEventArgs e) {
			foreach (var wf in ActiveWalletForms)
				wf.Wallet.Rescan();
		}

		private void OnFileImport(object sender, RoutedEventArgs e) {
			//			Eng.Password = "1"; //!!!D
//						Eng.ImportWallet("C:\\work\\coin\\wallet.dat", "123"); //!!D
//						return; //!!D

			if (!EnsurePassphraseUnlock())
				return;
			var d = new OpenFileDialog();
			d.InitialDirectory = Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.ApplicationData), "Bitcoin");
			d.Filter = "Bitcoin Wallet format|wallet.dat|All Files|*.*";
			d.FileName = "wallet.dat";
			if (Dialog.ShowDialog(d, this)) {
				string password = "";
				while (true) {
					try {
						Eng.ImportWallet(d.FileName, password);
						break;
					} catch (Exception) {
						var dlg = new FormPassphrase();
						dlg.labelRetype.Visibility = Visibility.Hidden;
						dlg.textRetype.Visibility = Visibility.Hidden;
						dlg.Title = "Enter Passphrase for imported wallet.dat";
						if (!Dialog.ShowDialog(dlg, this))
							break;
						password = dlg.textPassword.Password;
					}
				}
			}
		}

		private void OnFileExport(object sender, RoutedEventArgs e) {
			if (MessageBox.Show("Exported wallet file will contain unencrypted keys. You can lose all your Coins if this file will be stolen!\nPlease save it to secure place and remove from the Working computer.\nAre you sure to export the Wallet?",
					"Coin Security Warning", MessageBoxButton.OKCancel, MessageBoxImage.Warning) == MessageBoxResult.OK) {
				if (!EnsurePassphraseUnlock())
					return;
				var d = new SaveFileDialog();
				d.InitialDirectory = Eng.AppDataDirectory;
				d.Filter = "Bitcoin Wallet format|wallet.dat|Ufasoft Coin XML|*.xml";
				d.FileName = "wallet-backup";
				if (Dialog.ShowDialog(d, this)) {
					if (File.Exists(d.FileName))
						File.Delete(d.FileName);
					switch (d.FilterIndex) {
						case 1: {
								var wf = SelectedWallet();
								if (wf == null)
									MessageBox.Show("No Currency selected in the List", "Wallet Export", MessageBoxButton.OK, MessageBoxImage.Stop);
								else
									wf.Wallet.ExportWalletToBdb(d.FileName);
							}
							break;
					case 2:
						Eng.ExportWalletToXml(d.FileName);
						break;
					}
				}
			}
		}

		private void OnHelp(object sender, RoutedEventArgs e) {
			string exe = Assembly.GetExecutingAssembly().Location;
			System.Windows.Forms.Help.ShowHelpIndex(null, Path.Combine(Path.GetDirectoryName(exe), Path.GetFileNameWithoutExtension(exe) + ".chm"));
		}

		private void OnFileCompact(object sender, RoutedEventArgs e) {
			foreach (var wf in ActiveWalletForms)
				wf.Wallet.CompactDatabase();
		}

		private void ContextMenu_Opened(object sender, RoutedEventArgs e) {
			ContextMenu menu = (ContextMenu)sender;
			menu.DataContext = SelectedWallet();
		}

		private void menuMining_Checked(object sender, RoutedEventArgs e) {
			SaveActiveCurrencies();
		}

		private void OnChangeWalletPassword(object sender, RoutedEventArgs e) {
			var dlg = new FormPassphrase();
			dlg.labelOldPassword.Visibility = Visibility.Visible;
			dlg.textOldPassword.Visibility = Visibility.Visible;
			dlg.textOldPassword.Focus();
			if (Dialog.ShowDialog(dlg, this)) {
				Eng.ChangePassword(dlg.textOldPassword.Text, dlg.textPassword.Password);
			}
		}
	}

	public class WalletForms : INotifyPropertyChanged {
		public IWallet Wallet;
		public FormTransactions FormTransactions;
		public FormAddressBook FormAddressBook;
		public MenuItem MenuItem;

		public event PropertyChangedEventHandler PropertyChanged;

		public void Free() {
			if (FormTransactions != null) {
				FormTransactions.Close();
				FormTransactions = null;
			}
			if (FormAddressBook != null) {
				FormAddressBook.Close();
				FormAddressBook = null;
			}
		}

        public Uri IconUri { get { return new Uri("images/" + Wallet.CurrencyName + ".ico", UriKind.Relative); } }
        public string CurrencySymbol { get { return Wallet.CurrencySymbol; } }
		public string Balance { get { return Wallet.Balance.ToString("0.########"); } }
		public int BlockHeight { get { return Wallet.LastBlock; } }
		public string State { get { return Wallet.State; } }
		public int Peers { get { return Wallet.Peers; } }
		public bool MiningEnabled { get { return Wallet.MiningEnabled; } set { Wallet.MiningEnabled = value; } }
		public bool MiningAllowed { get { return Wallet.MiningAllowed; } }

		public bool CheckForChanges() {
			bool r = Wallet.GetAndResetStateChangedFlag();
			if (r) {
				if (PropertyChanged != null) {
					PropertyChanged(this, new PropertyChangedEventArgs("Balance"));
					PropertyChanged(this, new PropertyChangedEventArgs("BlockHeight"));
					PropertyChanged(this, new PropertyChangedEventArgs("State"));
					PropertyChanged(this, new PropertyChangedEventArgs("Peers"));
				}
			}
			return r;
		}
	}

	public class Win32WindowHandle : System.Windows.Forms.IWin32Window {
		private IntPtr _handle;

		public Win32WindowHandle(IntPtr handle) {
			_handle = handle;
		}

		public IntPtr Handle {
			get { return _handle; }
		}
	}

    public class MyImageConverter : IValueConverter {
        public object Convert(object value, Type targetType, object parameter, CultureInfo culture) {
            string imageName = (string)value;
            return new BitmapImage(new Uri("images/" + imageName + ".ico", UriKind.Relative));
        }

        public object ConvertBack(object value, Type targetType, object parameter, CultureInfo culture) {
            throw new NotImplementedException();
        }
    }
}
