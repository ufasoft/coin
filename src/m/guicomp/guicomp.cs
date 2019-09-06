using System;
using System.ComponentModel;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.IO;
using System.Runtime.InteropServices;
using System.Runtime.Serialization;
using System.Runtime.Serialization.Formatters.Binary;
using System.Windows.Interop;
using Microsoft.Win32;
using System.Windows;
using System.Windows.Media;
using System.Windows.Documents;
using System.Windows.Controls;
using System.Windows.Markup;


using Win32;

//provides simplified declaration in XAML
[assembly: XmlnsPrefix("http://www.hardcodet.net/taskbar", "tb")]
[assembly: XmlnsDefinition("http://www.hardcodet.net/taskbar", "Hardcodet.Wpf.TaskbarNotification")]

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

    public static class WindowPlacement {
        private static IFormatter serializer = new BinaryFormatter();

        public static void SetPlacement(IntPtr windowHandle, string placementXml) {
            if (!string.IsNullOrEmpty(placementXml)) {
                try {
                    Win32.WINDOWPLACEMENT placement = (Win32.WINDOWPLACEMENT)serializer.Deserialize(new MemoryStream(Convert.FromBase64String(placementXml)));
                    placement.length = Marshal.SizeOf(typeof(Win32.WINDOWPLACEMENT));
                    placement.flags = 0;
                    placement.showCmd = (placement.showCmd == (int)SW.SW_SHOWMINIMIZED ? (int)SW.SW_SHOWNORMAL : placement.showCmd);
                    Api.SetWindowPlacement(windowHandle, ref placement);
                } catch (Exception) {
                }
            }
        }

        public static string GetPlacement(IntPtr windowHandle) {
            Win32.WINDOWPLACEMENT placement = new Win32.WINDOWPLACEMENT();
            Api.GetWindowPlacement(windowHandle, out placement);

            using (MemoryStream memoryStream = new MemoryStream()) {
                serializer.Serialize(memoryStream, placement);
                return Convert.ToBase64String(memoryStream.ToArray());
            }
        }

        public static void SetPlacement(this Window window, string placementXml) {
            WindowPlacement.SetPlacement(new WindowInteropHelper(window).Handle, placementXml);
        }

        public static string GetPlacement(this Window window) {
            return WindowPlacement.GetPlacement(new WindowInteropHelper(window).Handle);
        }
    }

}

