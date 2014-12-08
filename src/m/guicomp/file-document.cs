/*######     Copyright (c) 1997-2012 Ufasoft  http://ufasoft.com  mailto:support@ufasoft.com,  Sergey Pavlov  mailto:dev@ufasoft.com #####
# This program is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published #
# by the Free Software Foundation; either version 3, or (at your option) any later version. This program is distributed in the hope that #
# it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. #
# See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with this #
# program; If not, see <http://www.gnu.org/licenses/>                                                                                    #
########################################################################################################################################*/

using System;
using System.IO;
using System.Windows.Forms;


namespace GuiComp {

public interface IDocumentData {
    void Read(Stream stm);
    void Write(Stream stm);
    void Updated();
}


public class FileDocument {
    public bool Dirty;
    public string FilePath;
    public Form OwnerForm;
    public string Filter;

    public IDocumentData DocumentData;

    public bool Open(string fileName) {
        FilePath = fileName;
        if (FilePath == null) {
            var d = new OpenFileDialog();
            d.Filter = Filter;
            if (d.ShowDialog(OwnerForm) != DialogResult.OK)
                return false;
			FilePath = d.FileName;
        }
        using (var stm = new FileStream(FilePath, FileMode.Open)) {
            DocumentData.Read(stm);
        }
        DocumentData.Updated();
        return true;
    }

    public bool Save() {
        if (Dirty) {
            if (FilePath == null) {
                var d = new SaveFileDialog();
                d.Filter = Filter;
                if (d.ShowDialog(OwnerForm) != DialogResult.OK)
                     return false;
                 FilePath = d.FileName;
            }
            using (var stm = new FileStream(FilePath, FileMode.Create)) {
                DocumentData.Write(stm);
            }            
            Dirty = false;
        }
        return true;
    }

    public bool Close() {
        if (Dirty) {
            DialogResult res = MessageBox.Show(OwnerForm, "Save changes?", Application.ProductName, MessageBoxButtons.YesNoCancel);
            switch (res) {
                case DialogResult.Yes: return Save();
                case DialogResult.No: return true;
                case DialogResult.Cancel: return false;
            }
        }
        return true;        
    }
}



}



