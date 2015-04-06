/*######   Copyright (c) 2011-2015 Ufasoft  http://ufasoft.com  mailto:support@ufasoft.com,  Sergey Pavlov  mailto:dev@ufasoft.com ####
#                                                                                                                                     #
# 		See LICENSE for licensing information                                                                                         #
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
using System.Globalization;

using Interop.coineng;

namespace Coin {
	public partial class FormSendMoney : Window {
		public FormSendMoney() {
			InitializeComponent();
		}

		public IWallet Wallet;

        private void Window_Loaded(object sender, RoutedEventArgs e) {
            labelCurrencySymbol.Content = Wallet.CurrencySymbol;
        }

        private void Window_Closed(object sender, EventArgs e) {
			if (DialogResult == true) {
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
						Wallet.SendTo(amount, address, comment);
					} finally {
						Cursor = prevCursor;
					}
				}
			}
		}

		private void OnSend(object sender, RoutedEventArgs e) {
			DialogResult = true;
		}

		private void textAmount_TextChanged(object sender, TextChangedEventArgs e) {
			//			return; //!!!D
			decimal amount;
			if (decimal.TryParse(textAmount.Text, out amount)) {
				try {
					labelFee.Content = Wallet.CalcFee(amount).ToString();
				} catch (Exception ex) {
					labelFee.Content = ex.Message;
				}
			}
		}

	}
}
