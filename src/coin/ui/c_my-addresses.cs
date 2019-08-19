/*######   Copyright (c) 2011-2015 Ufasoft  http://ufasoft.com  mailto:support@ufasoft.com,  Sergey Pavlov  mailto:dev@ufasoft.com ####
#                                                                                                                                     #
#       See LICENSE for licensing information                                                                                         #
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

        public IWallet Wallet { get; set; }

        public CtlMyAddresses() {
            GridView gv = new GridView();

            gv.Columns.Add(new GridViewColumn {
                DisplayMemberBinding = new Binding("Value")
                , Header = new GridViewColumnHeader() { Tag = "Value", Content = "Address" }
                , Width = 255
            });

            gv.Columns.Add(new GridViewColumn {
                DisplayMemberBinding = new Binding("Comment")
                , Header = new GridViewColumnHeader() { Tag = "Comment", Content = "Comment" }
                , Width = 300
            });

            View = gv;

            var menu = new ContextMenu();

            var mi = new MenuItem() { Header = "_Copy" };
            mi.Click += (s, e) => Clipboard.SetText(GetMySelectedAddress().Value);
            menu.Items.Add(mi);

            mi = new MenuItem() { Header = "_Edit Comment" };
            mi.Click += OnMyAddressEditComment;
            menu.Items.Add(mi);

            var subMenu = new MenuItem() { Header = "_Generate New" };
            subMenu.Items.Add(mi = new MenuItem { Header = "Legacy (P2P_KH)" });
            mi.Click += (s, e) => OnMyAddressGenerateNew(EAddressType.Legacy);
            subMenu.Items.Add(mi = new MenuItem { Header = "P2_SH" });
            mi.Click += (s, e) => OnMyAddressGenerateNew(EAddressType.P2SH);
            subMenu.Items.Add(mi = new MenuItem { Header = "_Bech32" });
            mi.Click += (s, e) => OnMyAddressGenerateNew(EAddressType.Bech32);
            menu.Items.Add(subMenu);

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

        void OnMyAddressGenerateNew(EAddressType type) {
            string r = FormQueryString.QueryString("Enter New Address Comment");
            if (r != null)
                AddMyAddress(Wallet.GenerateNewAddress(type, r));
        }

    }
}
