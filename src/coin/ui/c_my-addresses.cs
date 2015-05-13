/*######   Copyright (c) 2011-2015 Ufasoft  http://ufasoft.com  mailto:support@ufasoft.com,  Sergey Pavlov  mailto:dev@ufasoft.com ####
#                                                                                                                                     #
# 		See LICENSE for licensing information                                                                                         #
#####################################################################################################################################*/

using System;
using System.Collections.Generic;
using System.Collections.ObjectModel;
using System.Windows.Controls;
using System.Linq;
using System.Windows;
using System.Windows.Data;

using Utils;
using GuiComp;

using Interop.coineng;

namespace Coin {
	public class CtlMyAddresses : ListView {

		public ObservableCollection<Address> MyAddresses = new ObservableCollection<Address>();

		ListViewSortHelper LvMyAddressesSortHelper = new ListViewSortHelper();

		public IWallet Wallet {
			get; set;
		}


		public CtlMyAddresses() {
			GridView gv = new GridView();

			GridViewColumn c = new GridViewColumn();
			c.DisplayMemberBinding = new Binding("Value");
			c.Header = new GridViewColumnHeader() { Tag="Value", Content = "Address" };
			c.Width = 250;
			gv.Columns.Add(c);

			c = new GridViewColumn();
			c.DisplayMemberBinding = new Binding("Comment");
			c.Header = new GridViewColumnHeader() { Tag = "Comment", Content = "Comment" };
			c.Width = 300;
			gv.Columns.Add(c);

			View = gv;

			var menu = new ContextMenu();

			var mi = new MenuItem() { Header = "_Copy" };
			mi.Click += (s, e) => Clipboard.SetText(GetMySelectedAddress().Value);
			menu.Items.Add(mi);

			mi = new MenuItem() { Header = "_Edit Comment" };
			mi.Click += OnMyAddressEditComment;
			menu.Items.Add(mi);

			mi = new MenuItem() { Header = "_Generate New..." };
			mi.Click += OnMyAddressGenerateNew;
			menu.Items.Add(mi);


			ContextMenu = menu;
			
			LvMyAddressesSortHelper.ListView = this;
		}


		Address GetMySelectedAddress() {
			return (Address)SelectedItem;
		}

		public void UpdateMyAddresses() {
			ItemsSource = MyAddresses = new ObservableCollection<Address>(Wallet.MyAddresses.Select(a => new Address() { m_iface = a }));
		}

		void AddMyAddress(IAddress a) {
			MyAddresses.Add(new Address() { m_iface = a });
		}

		private void OnMyAddressEditComment(object sender, RoutedEventArgs e) {
			Address a = GetMySelectedAddress();
			string r = FormQueryString.QueryString("Edit Comment", a.Comment);
			if (r != null) {
				a.m_iface.Comment = r;
				UpdateMyAddresses();
			}
		}

		private void OnMyAddressGenerateNew(object sender, RoutedEventArgs e) {
			string r = FormQueryString.QueryString("Enter New Address Comment");
			if (r != null)
				AddMyAddress(Wallet.GenerateNewAddress(r));
		}

	}
}

