/*######     Copyright (c) 1997-2012 Ufasoft  http://ufasoft.com  mailto:support@ufasoft.com,  Sergey Pavlov  mailto:dev@ufasoft.com #####
# This program is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published #
# by the Free Software Foundation; either version 3, or (at your option) any later version. This program is distributed in the hope that #
# it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. #
# See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with this #
# program; If not, see <http://www.gnu.org/licenses/>                                                                                    #
########################################################################################################################################*/

﻿using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Drawing;
using System.Data;
using System.Text;
using System.Windows.Forms;

using Utils;

namespace GuiComp {

    public struct Dtmf {
        public int Key;

        public override string ToString() {
            switch ((DtmfKey)Key) {
                case DtmfKey.Key_0: return "0";
                case DtmfKey.Key_1: return "1";
                case DtmfKey.Key_2: return "2";
                case DtmfKey.Key_3: return "3";
                case DtmfKey.Key_4: return "4";
                case DtmfKey.Key_5: return "5";
                case DtmfKey.Key_6: return "6";
                case DtmfKey.Key_7: return "7";
                case DtmfKey.Key_8: return "8";
                case DtmfKey.Key_9: return "9";
                case DtmfKey.Key_Star: return "*";
                case DtmfKey.Key_Pound: return "#";
            }
            return "Unknown";            
        }
    }

    public delegate void OnDialPadKey(object sender, Dtmf dtmf);

    public partial class DialPad : UserControl {
        public event OnDialPadKey OnDialPadKey;

        public DialPad() {
            InitializeComponent();
        }

        protected virtual void OnKey(Dtmf dtmf) {
            if (OnDialPadKey != null)
                OnDialPadKey(this, dtmf);
        }

        private void button1_Click(object sender, EventArgs e) {
            string s = (string)((Button)sender).Tag;
            Dtmf dtmf = new Dtmf();
            switch (s) {
                case "0": dtmf.Key = (int)DtmfKey.Key_0; break;
                case "1": dtmf.Key = (int)DtmfKey.Key_1; break;
                case "2": dtmf.Key = (int)DtmfKey.Key_2; break;
                case "3": dtmf.Key = (int)DtmfKey.Key_3; break;
                case "4": dtmf.Key = (int)DtmfKey.Key_4; break;
                case "5": dtmf.Key = (int)DtmfKey.Key_5; break;
                case "6": dtmf.Key = (int)DtmfKey.Key_6; break;
                case "7": dtmf.Key = (int)DtmfKey.Key_7; break;
                case "8": dtmf.Key = (int)DtmfKey.Key_8; break;
                case "9": dtmf.Key = (int)DtmfKey.Key_9; break;
                case "*": dtmf.Key = (int)DtmfKey.Key_Star; break;
                case "#": dtmf.Key = (int)DtmfKey.Key_Pound; break;

                default: throw new ApplicationException("Unexpected Dial Key");
            }
            OnKey(dtmf);
        }

        private void DialPad_Load(object sender, EventArgs e) {
            button2.Text = "2\nABC";
            button3.Text = "3\nDEF";
            button4.Text = "4\nGHI";
            button5.Text = "5\nJKL";
            button6.Text = "6\nMNO";
            button7.Text = "7\nPQRS";
            button8.Text = "8\nTUV";
            button9.Text = "9\nWXYZ";
            button0.Text = "0\n+";
        }


        private void DialPad_KeyDown(object sender, KeyEventArgs e) {
            
            Dtmf dtmf = new Dtmf();
            switch (e.KeyCode) {
                case Keys.D0:
                case Keys.NumPad0: dtmf.Key = (int)DtmfKey.Key_0; break;
                case Keys.D1:
                case Keys.NumPad1: dtmf.Key = (int)DtmfKey.Key_1; break;
                case Keys.D2:
                case Keys.NumPad2: dtmf.Key = (int)DtmfKey.Key_2; break;
                case Keys.D3:
                case Keys.NumPad3: dtmf.Key = (int)DtmfKey.Key_3; break;
                case Keys.D4:
                case Keys.NumPad4: dtmf.Key = (int)DtmfKey.Key_4; break;
                case Keys.D5:
                case Keys.NumPad5: dtmf.Key = (int)DtmfKey.Key_5; break;
                case Keys.D6:
                case Keys.NumPad6: dtmf.Key = (int)DtmfKey.Key_6; break;
                case Keys.D7:
                case Keys.NumPad7: dtmf.Key = (int)DtmfKey.Key_7; break;
                case Keys.D8:
                case Keys.NumPad8: dtmf.Key = (int)DtmfKey.Key_8; break;
                case Keys.D9:
                case Keys.NumPad9: dtmf.Key = (int)DtmfKey.Key_9; break;
                case Keys.Multiply: dtmf.Key = (int)DtmfKey.Key_Star; break;
     //!!!           case Keys.Nu.NumPad0: dtmf.Key = DtmfKey.Key_Pound; break;
                default:
                    return;
            }
            OnKey(dtmf);
        }

        private void DialPad_Enter(object sender, EventArgs e) {
            button1.Focus();
        }

        private void DialPad_Resize(object sender, EventArgs e) {
            int min = Math.Min(Width, Height);
            Size btnSize = new Size(min / 5, min / 5);
            int step = min/20;
            foreach (Button btn in this.Controls)
                btn.Size = btnSize;
            button1.Location = new Point(step, step);
            button2.Location = new Point(step*2+btnSize.Width, step);
            button3.Location = new Point(step * 3 + btnSize.Width*2, step);
            button4.Location = new Point(step, step*2+btnSize.Height);
            button5.Location = new Point(step * 2 + btnSize.Width, step * 2 + btnSize.Height);
            button6.Location = new Point(step * 3 + btnSize.Width * 2, step * 2 + btnSize.Height);
            button7.Location = new Point(step, step * 3 + btnSize.Height*2);
            button8.Location = new Point(step * 2 + btnSize.Width, step * 3 + btnSize.Height*2);
            button9.Location = new Point(step * 3 + btnSize.Width * 2, step * 3 + btnSize.Height*2);
            buttonStar.Location = new Point(step, step * 4 + btnSize.Height * 3);
            button0.Location = new Point(step * 2 + btnSize.Width, step * 4 + btnSize.Height * 3);
            buttonPound.Location = new Point(step * 3 + btnSize.Width * 2, step * 4 + btnSize.Height * 3);

        }

    }
}
