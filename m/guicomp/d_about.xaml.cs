/*######     Copyright (c) 1997-2012 Ufasoft  http://ufasoft.com  mailto:support@ufasoft.com,  Sergey Pavlov  mailto:dev@ufasoft.com #####
# This program is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published #
# by the Free Software Foundation; either version 3, or (at your option) any later version. This program is distributed in the hope that #
# it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. #
# See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with this #
# program; If not, see <http://www.gnu.org/licenses/>                                                                                    #
########################################################################################################################################*/

﻿using System;
using System.Collections.Generic;
using System.Text;
using System.Windows;
using System.Reflection;


namespace GuiComp {
	public partial class DialogAbout : Window {

		Assembly ExeAssembly;

		public DialogAbout() {
			InitializeComponent();

			ExeAssembly = Assembly.GetCallingAssembly();

			Title = String.Format("About {0}", AssemblyTitle);
			labelProductName.Content = AssemblyProduct;
			labelVersion.Content = String.Format("Version {0}", AssemblyVersion);
			labelCopyright.Content = AssemblyCopyright;
			labelCompanyName.Content = AssemblyCompany;
			textBlockDescription.Text = AssemblyDescription;
		}


		public string AssemblyTitle {
			get {
				// Get all Title attributes on this assembly
				object[] attributes = ExeAssembly.GetCustomAttributes(typeof(AssemblyTitleAttribute), false);
				// If there is at least one Title attribute
				if (attributes.Length > 0) {
					// Select the first one
					AssemblyTitleAttribute titleAttribute = (AssemblyTitleAttribute)attributes[0];
					// If it is not an empty string, return it
					if (titleAttribute.Title != "")
						return titleAttribute.Title;
				}
				// If there was no Title attribute, or if the Title attribute was the empty string, return the .exe name
				return System.IO.Path.GetFileNameWithoutExtension(ExeAssembly.CodeBase);
			}
		}

		public string AssemblyVersion {
			get {
				return ExeAssembly.GetName().Version.ToString();
			}
		}

		public string AssemblyDescription {
			get {
				// Get all Description attributes on this assembly
				object[] attributes = ExeAssembly.GetCustomAttributes(typeof(AssemblyDescriptionAttribute), false);
				// If there aren't any Description attributes, return an empty string
				if (attributes.Length == 0)
					return "";
				// If there is a Description attribute, return its value
				return ((AssemblyDescriptionAttribute)attributes[0]).Description;
			}
		}

		public string AssemblyProduct {
			get {
				// Get all Product attributes on this assembly
				object[] attributes = ExeAssembly.GetCustomAttributes(typeof(AssemblyProductAttribute), false);
				// If there aren't any Product attributes, return an empty string
				if (attributes.Length == 0)
					return "";
				// If there is a Product attribute, return its value
				return ((AssemblyProductAttribute)attributes[0]).Product;
			}
		}

		public string AssemblyCopyright {
			get {
				// Get all Copyright attributes on this assembly
				object[] attributes = ExeAssembly.GetCustomAttributes(typeof(AssemblyCopyrightAttribute), false);
				// If there aren't any Copyright attributes, return an empty string
				if (attributes.Length == 0)
					return "";
				// If there is a Copyright attribute, return its value
				return ((AssemblyCopyrightAttribute)attributes[0]).Copyright;
			}
		}

		public string AssemblyCompany {
			get {
				
				// Get all Company attributes on this assembly
				object[] attributes = ExeAssembly.GetCustomAttributes(typeof(AssemblyCompanyAttribute), false);
				// If there aren't any Company attributes, return an empty string
				if (attributes.Length == 0)
					return "";
				// If there is a Company attribute, return its value
				return ((AssemblyCompanyAttribute)attributes[0]).Company;
			}
		}

		private void OnOk(object sender, RoutedEventArgs e) {
			DialogResult = true;
		}
	}
}
