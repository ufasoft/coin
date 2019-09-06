using System;
using System.Collections.Generic;
using System.Text;
using System.Windows;
using System.Reflection;


namespace GuiComp {
    public partial class DialogAbout : Window {

        Assembly ExeAssembly;

        public Uri SourceCodeUri;

        public DialogAbout() {
            InitializeComponent();

            ExeAssembly = Assembly.GetCallingAssembly();

            Title = $"About {AssemblyTitle}";
            labelProductName.Content = AssemblyProduct;
            labelVersion.Content = $"Version {AssemblyVersion}";
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
				var ver = ExeAssembly.GetName().Version;
				return ver.ToString(ver.Revision != 0 ? 4
					: ver.Build != 0 ? 3
					: 2);
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
