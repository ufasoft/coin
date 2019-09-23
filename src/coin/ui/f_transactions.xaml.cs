/*######   Copyright (c) 2011-2019 Ufasoft  http://ufasoft.com  mailto:support@ufasoft.com,  Sergey Pavlov  mailto:dev@ufasoft.com ####
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
using System.Windows.Input;
using System.Windows.Media;
using System.Globalization;

using Utils;
using GuiComp;

using Interop.coineng;

namespace Coin {
    public partial class FormTransactions : Window {

        public FormTransactions() {
            InitializeComponent();

        }

        /*
        WalletForms m_walletForms;

        public WalletForms WalletForms {
            get => m_walletForms;
            set {
                m_walletForms = value;
                Title = WalletForms.Wallet.CurrencyName + " Transactions";
            }
        }

        IWallet Wallet {
            get { return WalletForms.Wallet; }
        }

        public ObservableCollection<Tx> Transactions = new ObservableCollection<Tx>();

        Tx GetSelectedTransaction() {
            return (Tx)lv.SelectedItem;
        }

        public void UpdateTransactions() {
            ITransaction iSelectedTx = null;
            if (lv.SelectedItem != null)
                iSelectedTx = (lv.SelectedItem as Tx).m_iTx;
            Transactions.Clear();
            foreach (var tx in Wallet.Transactions) {
                var x = new Tx() { m_iTx = tx };
                Transactions.Add(x);
                if (iSelectedTx == tx)
                    lv.SelectedItem = x;
            }
        }

        private void Window_Loaded(object sender, RoutedEventArgs e) {
            UpdateTransactions();
            lv.ItemsSource = Transactions;
        }


        void ShowTxInfo(Tx tx) {
            var dlg = new DialogTextInfo();
            dlg.Title = "Transaction Info";
            dlg.textInfo.Text = $"DateTime:  {tx.Timestamp}\nValue:   {tx.m_iTx.Amount}\nFee:  {tx.Fee}\nConfirmations: {tx.Confirmations.ToString()}\nTo:   {tx.Address + " " + tx.m_iTx.Address.Comment}\nComment:  {tx.m_iTx.Comment}\nHash:   {tx.Hash}";
            dlg.Width = 600;
            dlg.Height = 200;
            Dialog.ShowDialog(dlg, this);
        }

        private void lv_MouseDoubleClick(object sender, MouseButtonEventArgs e) {
            ShowTxInfo(GetSelectedTransaction());
        }

        private void OnTxInfo(object sender, RoutedEventArgs e) {
            ShowTxInfo(GetSelectedTransaction());
        }

        private void OnEditComment(object sender, RoutedEventArgs e) {
            var tx = GetSelectedTransaction();
            string r = FormQueryString.QueryString("Edit Comment", tx.Comment);
            if (r != null) {
                tx.m_iTx.Comment = r;
                UpdateTransactions();
            }
        }*/

        private void FormTransactions_Closed(object sender, EventArgs e) {
            CtlTxes.WalletForms.FormTransactions = null;
        }

    }
}
