/*######   Copyright (c) 2011-2015 Ufasoft  http://ufasoft.com  mailto:support@ufasoft.com,  Sergey Pavlov  mailto:dev@ufasoft.com ####
#                                                                                                                                     #
#       See LICENSE for licensing information                                                                                         #
#####################################################################################################################################*/

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

        public FormAddressBook(WalletForms wf) {
            WalletForms = wf;
            InitializeComponent();

        }

        WalletForms WalletForms { get; set; }

        IWallet Wallet => WalletForms.Wallet;

        ListViewSortHelper LvMyAddressesSortHelper = new ListViewSortHelper();


        private void Window_Loaded(object sender, RoutedEventArgs e) {
            lvMyAddresses.Wallet = Wallet;
            lvMyAddresses.UpdateMyAddresses();

            lvRecipients.Wallet = Wallet;
            lvRecipients.UpdateRecipients();
        }

        Address GetMySelectedAddress() {
            return (Address)lvMyAddresses.SelectedItem;
        }


        private void FormAddressBook_Closed(object sender, EventArgs e) {
            WalletForms.FormAddressBook = null;
        }

        private void OnSendToRecipient(object sender, IWallet wallet, string address, decimal? amount, string comment) {
            var dlg = new FormSendMoney();
            var c = dlg.CtlSend;
            c.textAddress.Text = address;
            c.textComment.Text = comment;
            c.Wallet = wallet;
            if (amount.HasValue)
                c.textAmount.Text = amount.ToString();
            Dialog.ShowDialog(dlg, this);
        }
    }


}
