using System;
using System.Collections.Generic;
using System.Collections.ObjectModel;
using System.Linq;
using System.Text;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Data;
using System.Windows.Documents;

using Utils;
using GuiComp;

using Interop.coineng;

namespace Coin {
	public partial class FormAddressBook : Window {
		ListViewSortHelper LvMyAddressesSortHelper = new ListViewSortHelper(),
						LvRecipientsSortHelper = new ListViewSortHelper();

		public FormAddressBook() {
			InitializeComponent();

			LvMyAddressesSortHelper.ListView = lvMyAddresses;
			LvRecipientsSortHelper.ListView = lvRecipients;
		}

		WalletForms m_walletForms;

		public WalletForms WalletForms {
			get { return m_walletForms; }
			set {
				m_walletForms = value;
				Title = WalletForms.Wallet.CurrencyName + " Address Book";
			}
		}


		IWallet Wallet {
			get { return WalletForms.Wallet; }
		}


		public ObservableCollection<Address> MyAddresses = new ObservableCollection<Address>(),
											Recipients = new ObservableCollection<Address>();


		void AddMyAddress(IAddress a) {
			MyAddresses.Add(new Address() { m_iface = a });
		}

		void UpdateMyAddresses() {
			lvMyAddresses.ItemsSource = MyAddresses = new ObservableCollection<Address>(Wallet.MyAddresses.Select(a => new Address() { m_iface = a }));
		}

		void UpdateRecipients() {
			lvRecipients.ItemsSource = Recipients = new ObservableCollection<Address>(Wallet.Recipients.Select(a => new Address() { m_iface = a }));
		}

		private void Window_Loaded(object sender, RoutedEventArgs e) {
			UpdateMyAddresses();
			UpdateRecipients();
		}

		Address GetMySelectedAddress() {
			return (Address)lvMyAddresses.SelectedItem;
		}

		Address GetRecipientSelectedAddress() {
			return (Address)lvRecipients.SelectedItem;
		}

		private void OnMyAddressEditComment(object sender, RoutedEventArgs e) {
			Address a = GetMySelectedAddress();
			string r = FormQueryString.QueryString("Edit Comment", a.Comment);
			if (r != null) {
				a.m_iface.Comment = r;
				UpdateMyAddresses();
			}
		}

		private void menuGenerateNewAddress_Click(object sender, EventArgs e) {
			string r = FormQueryString.QueryString("Enter New Address Comment");
			if (r != null)
				AddMyAddress(Wallet.GenerateNewAddress(r));
		}

		private void FormAddressBook_Closed(object sender, EventArgs e) {
			WalletForms.FormAddressBook = null;
		}

		private void OnMyAddressGenerateNew(object sender, RoutedEventArgs e) {
			string r = FormQueryString.QueryString("Enter New Address Comment");
			if (r != null)
				AddMyAddress(Wallet.GenerateNewAddress(r));
		}

		private void OnMyAddressCopy(object sender, RoutedEventArgs e) {
			Clipboard.SetText(GetMySelectedAddress().Value);			
		}

		private void OnRemoveRecipient(object sender, RoutedEventArgs e) {
			var a = GetRecipientSelectedAddress();
			Wallet.RemoveRecipient(a.m_iface);
			Recipients.Remove(a);
		}

		private void OnAddRecipient(object sender, RoutedEventArgs e) {
			var dlg = new FormAddress();
			if (Dialog.ShowDialog(dlg, this)) {
				Wallet.AddRecipient(dlg.textAddress.Text, dlg.textComment.Text);
				UpdateRecipients();
			}
		}

		private void OnEditRecipient(object sender, RoutedEventArgs e) {
			Address a = GetRecipientSelectedAddress();
			var dlg = new FormAddress();
			dlg.textAddress.Text = a.Value;
			dlg.textComment.Text = a.Comment;
			if (Dialog.ShowDialog(dlg, this)) {
				Wallet.RemoveRecipient(a.m_iface);
				Wallet.AddRecipient(dlg.textAddress.Text, dlg.textComment.Text);
				UpdateRecipients();
			}
		}

		private void OnSendMoney(object sender, RoutedEventArgs e) {
			Address a = GetRecipientSelectedAddress();
			var dlg = new FormSendMoney();
			dlg.textAddress.Text = a.Value;
			dlg.textComment.Text = a.Comment;
			dlg.Wallet = Wallet;
			Dialog.ShowDialog(dlg, this);
		}

	}

	public class Address {
		internal IAddress m_iface;

		public string Value { get { return m_iface.Value; } }
		public string Comment { get { return m_iface.Comment; } }
	}

}
