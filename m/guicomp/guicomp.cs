/*######     Copyright (c) 1997-2012 Ufasoft  http://ufasoft.com  mailto:support@ufasoft.com,  Sergey Pavlov  mailto:dev@ufasoft.com #####
# This program is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published #
# by the Free Software Foundation; either version 3, or (at your option) any later version. This program is distributed in the hope that #
# it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. #
# See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with this #
# program; If not, see <http://www.gnu.org/licenses/>                                                                                    #
########################################################################################################################################*/

using System;
using System.ComponentModel;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using Microsoft.Win32;
using System.Windows;
using System.Windows.Media;
using System.Windows.Documents;
using System.Windows.Controls;

namespace GuiComp {

	public class Dialog {
		public static bool ShowDialog(CommonDialog d, Window owner) {
			var r = d.ShowDialog();
			return r.HasValue && r.Value;
		}

		public static bool ShowDialog(Window d, Window owner) {
			d.Owner = owner;
			var r = d.ShowDialog();
			return r.HasValue && r.Value;
		}
	}
	
	public class guicomp {
	}


	public class SortAdorner : Adorner {
		private readonly static Geometry _AscGeometry =
			Geometry.Parse("M 0,0 L 10,0 L 5,5 Z");
		private readonly static Geometry _DescGeometry =
			Geometry.Parse("M 0,5 L 10,5 L 5,0 Z");

		public ListSortDirection Direction { get; private set; }

		public SortAdorner(UIElement element, ListSortDirection dir)
			: base(element) { Direction = dir; }

		protected override void OnRender(DrawingContext drawingContext) {
			base.OnRender(drawingContext);

			if (AdornedElement.RenderSize.Width < 20)
				return;

			drawingContext.PushTransform(
				new TranslateTransform(
				  AdornedElement.RenderSize.Width - 15,
				  (AdornedElement.RenderSize.Height - 5) / 2));

			drawingContext.DrawGeometry(Brushes.Black, null,
				Direction == ListSortDirection.Ascending ?
				  _AscGeometry : _DescGeometry);

			drawingContext.Pop();
		}
	}

	public class ListViewSortHelper {
		ListView m_listView;

		public ListView ListView {
			get { return m_listView; }
			set {
				if ((m_listView = value) != null)  {
					foreach (var h in (m_listView.View as GridView).Columns.Where(c => c.Header is GridViewColumnHeader).Select(c => c.Header as GridViewColumnHeader))
						h.Click += OnSort;
				}
			}
		}

		public GridViewColumnHeader _CurSortCol = null;
		public SortAdorner _CurAdorner = null;

		public void OnSort(object sender, RoutedEventArgs e) {
			GridViewColumnHeader column = sender as GridViewColumnHeader;
			String field = column.Tag as String;

			if (_CurSortCol != null) {
				AdornerLayer.GetAdornerLayer(_CurSortCol).Remove(_CurAdorner);
				ListView.Items.SortDescriptions.Clear();
			}

			ListSortDirection newDir = ListSortDirection.Ascending;
			if (_CurSortCol == column && _CurAdorner.Direction == newDir)
				newDir = ListSortDirection.Descending;

			_CurSortCol = column;
			_CurAdorner = new SortAdorner(_CurSortCol, newDir);
			AdornerLayer.GetAdornerLayer(_CurSortCol).Add(_CurAdorner);
			ListView.Items.SortDescriptions.Add(
				new SortDescription(field, newDir));
		}
	}
}
