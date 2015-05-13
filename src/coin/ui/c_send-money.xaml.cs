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

		public IWallet Wallet {
			get { return m_Wallet; }
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
					labelFee.Content = Wallet.CalcFee(amount).ToString();
				} catch (Exception ex) {
					labelFee.Content = ex.Message;
				}
			}
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
					Wallet.SendTo(amount, address, comment);
				} finally {
					Cursor = prevCursor;
				}
			}

			if (Send != null)
				Send(this, null);
		}
	}

}
