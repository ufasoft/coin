/*######   Copyright (c) 2011-2015 Ufasoft  http://ufasoft.com  mailto:support@ufasoft.com,  Sergey Pavlov  mailto:dev@ufasoft.com ####
#                                                                                                                                     #
#       See LICENSE for licensing information                                                                                         #
#####################################################################################################################################*/

using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Data;
using System.Windows.Documents;
using System.Windows.Input;
using System.Windows.Media;
using System.Windows.Media.Imaging;
using System.Windows.Navigation;
using System.Windows.Shapes;
using System.Globalization;

using Interop.coineng;

namespace Coin {

    public partial class CtlSendMoney : UserControl {

        public event RoutedEventHandler Send;

        IWallet m_Wallet;
        decimal Fee;

        public IWallet Wallet {
            get => m_Wallet;
            set {
                m_Wallet = value;
                labelCurrencySymbol.Content = m_Wallet != null ? m_Wallet.CurrencySymbol : "";
            }
        }

        public CtlSendMoney() {
            InitializeComponent();
        }

        private void textAmount_TextChanged(object sender, TextChangedEventArgs e) {
            decimal amount;
            if (decimal.TryParse(textAmount.Text, out amount)) {
                try {
                    labelFee.Content = (Fee = Wallet.CalcFee(amount)).ToString();
                    buttonSend.IsEnabled = true;
                } catch (Exception ex) {
                    labelFee.Content = ex.Message;
                    buttonSend.IsEnabled = false;
                }
            } else
                buttonSend.IsEnabled = false;
        }

        void OnCancel(object sender, RoutedEventArgs e) {
        }

        void OnSend(object sender, RoutedEventArgs e) {
            if (FormMain.I.EnsurePassphraseUnlock()) {
                var prevCursor = Cursor;
                Cursor = Cursors.Wait;
                string address = textAddress.Text,
                    comment = textComment.Text;
                try {
                    Wallet.AddRecipient(address, comment);
                } catch (Exception) {
                }
                try {
                    var prov = new NumberFormatInfo();
                    prov.NumberDecimalSeparator = ".";
                    decimal amount = Convert.ToDecimal(textAmount.Text.Replace(',', '.'), prov);
                    Wallet.SendTo(amount, address, comment, Fee);
                } finally {
                    Cursor = prevCursor;
                }
                textAmount.Text = "";
                textAddress.Text = "";
                textComment.Text = "";
                MessageBox.Show("The coins were sent successfully");
            }

            if (Send != null)
                Send(this, null);
        }
    }

}
