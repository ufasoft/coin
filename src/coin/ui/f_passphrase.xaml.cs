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
using System.Windows.Input;
using System.Windows.Shapes;

namespace Coin {
    public partial class FormPassphrase : Window {
        public FormPassphrase() {
            InitializeComponent();
            labelOldPassword.Visibility = Visibility.Hidden;
            textOldPassword.Visibility = Visibility.Hidden;
        }

        bool CheckFields() {
            if (labelRetype.Visibility == Visibility.Visible) {
                string s = textPassword.Password;
                if (s != textRetype.Password)
                    throw new ApplicationException("Retyped string is not the same as Password");
                if (s.Length == 0) {
                    if (MessageBox.Show("New password is Empty. Are you sure to make your wallet unencrypted?", "Coin", MessageBoxButton.YesNo, MessageBoxImage.Question) == MessageBoxResult.No)
                        return false;
                }
            }
            return true;
        }

        private void OnOk(object sender, RoutedEventArgs e) {
            DialogResult = CheckFields();
        }

    }
}
