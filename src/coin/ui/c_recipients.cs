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
    public class CtlRecipients : ListView {
        ListViewSortHelper      LvRecipientsSortHelper = new ListViewSortHelper();


        public ObservableCollection<Address> Recipients = new ObservableCollection<Address>();

        public IWallet Wallet {
            get; set;
        }


        public CtlRecipients() {
            GridView gv = new GridView();

            GridViewColumn c = new GridViewColumn();
            c.DisplayMemberBinding = new Binding("Value");
            c.Header = new GridViewColumnHeader() { Tag = "Value", Content = "Address" };
            c.Width = 250;
            gv.Columns.Add(c);

            c = new GridViewColumn();
            c.DisplayMemberBinding = new Binding("Comment");
            c.Header = new GridViewColumnHeader() { Tag = "Comment", Content = "Comment" };
            c.Width = 300;
            gv.Columns.Add(c);

            View = gv;
            LvRecipientsSortHelper.ListView = this;

            var menu = new ContextMenu();

            var mi = new MenuItem() { Header = "_Send" };
            mi.Click += OnSendMoney;
            menu.Items.Add(mi);

            menu.Items.Add(new Separator());

            mi = new MenuItem() { Header = "_Add" };
            mi.Click += OnAddRecipient;
            menu.Items.Add(mi);

            mi = new MenuItem() { Header = "_Edit" };
            mi.Click += OnEditRecipient;
            menu.Items.Add(mi);


            mi = new MenuItem() { Header = "_Remove" };
            mi.Click += OnRemoveRecipient;
            menu.Items.Add(mi);


            ContextMenu = menu;
        }

        public void UpdateRecipients() {
            ItemsSource = Recipients = new ObservableCollection<Address>(Wallet.Recipients.Select(a => new Address() { m_iface = a }));
        }

        Address GetRecipientSelectedAddress() {
            return (Address)SelectedItem;
        }

        void OnRemoveRecipient(object sender, RoutedEventArgs e) {
            var a = GetRecipientSelectedAddress();
            Wallet.RemoveRecipient(a.m_iface);
            Recipients.Remove(a);
        }

        void OnAddRecipient(object sender, RoutedEventArgs e) {
            var dlg = new FormAddress();
            if (Dialog.ShowDialog(dlg, Window.GetWindow(this))) {
                Wallet.AddRecipient(dlg.textAddress.Text, dlg.textComment.Text);
                UpdateRecipients();
            }
        }

        void OnEditRecipient(object sender, RoutedEventArgs e) {
            Address a = GetRecipientSelectedAddress();
            var dlg = new FormAddress();
            dlg.textAddress.Text = a.Value;
            dlg.textComment.Text = a.Comment;
            if (Dialog.ShowDialog(dlg, Window.GetWindow(this))) {
                Wallet.RemoveRecipient(a.m_iface);
                Wallet.AddRecipient(dlg.textAddress.Text, dlg.textComment.Text);
                UpdateRecipients();
            }
        }

        public event SendToRecipientEventHandler SendToRecipient;

        void OnSendMoney(object sender, RoutedEventArgs e) {
            Address a = GetRecipientSelectedAddress();
            if (SendToRecipient != null)
                SendToRecipient(this, Wallet, a.Value, null, a.Comment);
        }

    }

    public delegate void SendToRecipientEventHandler(object sender, IWallet wallet, string address, decimal? amount, string comment);

    public class Address {
        internal IAddress m_iface;

        public string Value => m_iface.Value;
        public string Comment => m_iface.Comment;
    }

}
