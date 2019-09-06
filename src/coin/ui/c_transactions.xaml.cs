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
using System.Diagnostics;

using Utils;
using GuiComp;

using Interop.coineng;

namespace Coin {
    public partial class CtlTransactions : UserControl {
        ListViewSortHelper ListViewSortHelper = new ListViewSortHelper();

        public CtlTransactions() {
            InitializeComponent();
            ListViewSortHelper.ListView = lv;
        }


        WalletForms m_walletForms;

        public WalletForms WalletForms {
            get => m_walletForms;
            set {
                m_walletForms = value;
                //  Title = WalletForms.Wallet.CurrencyName + " Transactions";
            }
        }

        IWallet Wallet => WalletForms.Wallet;

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

        public void InitLoaded() {
            UpdateTransactions();
            lv.ItemsSource = Transactions;
        }

        private void FormTransactions_Closed(object sender, EventArgs e) {
            WalletForms.FormTransactions = null;
        }

        void ShowTxInfo(Tx tx) {
            if (tx == null)
                return;
            var dlg = new DialogTextInfo();
            dlg.Title = "Transaction Info";
            dlg.textInfo.Text =
                    $"Date Time: {tx.Timestamp}"
                + $"\nValue:     {tx.m_iTx.Amount}"
                + $"\nFee:       {tx.Fee}"
                + $"\nConfirmations: {tx.Confirmations}"
                + $"\nTo:        {tx.Address} [{tx.m_iTx.Address.Comment}]"
                + $"\nComment:   {tx.m_iTx.Comment}"
                + $"\nHash:      {tx.Hash}";
            dlg.Width = 800;
            dlg.Height = 300;

            switch (Wallet.CurrencySymbol) {
                case "GRS":
                    dlg.Hyperlink.NavigateUri = new Uri($"https://chainz.cryptoid.info/{Wallet.CurrencySymbol.ToLower()}/tx.dws?{tx.Hash}");
                    dlg.Hyperlink.Inlines.Add("Block Explorer");
                    dlg.Hyperlink.RequestNavigate += (sender, e) => { Process.Start(e.Uri.ToString()); };
                    break;
                default:
                    dlg.Hyperlink.IsEnabled = false;
                    break;
            }

            Dialog.ShowDialog(dlg, FormMain.I);
        }

        private void lv_MouseDoubleClick(object sender, MouseButtonEventArgs e) {
            ShowTxInfo(GetSelectedTransaction());
        }

        private void OnTxInfo(object sender, RoutedEventArgs e) {
            ShowTxInfo(GetSelectedTransaction());
        }

        private void OnEditComment(object sender, RoutedEventArgs e) {
            var tx = GetSelectedTransaction();
            if (tx == null)
                return;
            string r = FormQueryString.QueryString("Edit Comment", tx.Comment);
            if (r != null) {
                tx.m_iTx.Comment = r;
                UpdateTransactions();
            }
        }

        public override void EndInit() {
            base.EndInit();
            if (WalletForms != null) {
                UpdateTransactions();
                lv.ItemsSource = Transactions;
            }
        }
    }

    public class Tx {
        internal ITransaction m_iTx;

        decimal? amount;
        IAddress address;

        public DateTime Timestamp => m_iTx.Timestamp.ToLocalTime();
        public decimal Amount => amount ??= m_iTx.Amount;
        public string AmountColor => Amount > 0 ? "Black" : "Red";
        public string Address => (address ??= m_iTx.Address).Value;
        public int Confirmations => m_iTx.Confirmations;
        public string Status => m_iTx.Confirmations switch { 0 => "Pending", int v when v < 6 => v.ToString(), _ => "OK" };
        public string Hash => m_iTx.Hash;
        public string Comment => (address ??= m_iTx.Address).Comment switch { "" => "", var s => $"[{s}] " } + m_iTx.Comment;

        public string Fee {
            get {
                try {
                    return m_iTx.Fee switch { 0 => "", var v => v.ToString() };
                }
                catch (Exception) {
                    return "Unknown";
                }
            }
        }
    };

    public class DateTimeConverter : IValueConverter {
        public object Convert(object value, Type targetType, object parameter, CultureInfo culture) {
            if (value == null)
                return null;
            DateTime dateTime = (DateTime)value;
            return dateTime.ToString(CultureInfo.CurrentCulture);
        }
        public object ConvertBack(object value, Type targetType, object parameter, CultureInfo culture) {
            throw new NotImplementedException();
        }
    }
}
