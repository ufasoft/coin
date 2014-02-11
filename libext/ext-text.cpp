/*######     Copyright (c) 1997-2013 Ufasoft  http://ufasoft.com  mailto:support@ufasoft.com,  Sergey Pavlov  mailto:dev@ufasoft.com #######################################
#                                                                                                                                                                          #
# This program is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation;  #
# either version 3, or (at your option) any later version. This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the      #
# implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details. You should have received a copy of the GNU #
# General Public License along with this program; If not, see <http://www.gnu.org/licenses/>                                                                               #
##########################################################################################################################################################################*/

#include <el/ext.h>

#if UCFG_WIN32
#	include <el/libext/win32/extmfc.h>
#	include <richedit.h>
#endif

namespace Ext {
using namespace std;

void AFXAPI AfxInitRichEdit() {
	static CDynamicLibrary *dllRichEdit = new CDynamicLibrary("RICHED20.DLL");
}

static StaticWRegex s_reRtf("^\\{.rtf");

String AFXAPI RtfToText(RCString rtf) {
	if (!regex_search(rtf, *s_reRtf))
		return rtf;
#if UCFG_USE_POSIX || !UCFG_EXTENDED || UCFG_WCE
	string srtf;
	istringstream is(srtf.c_str());
	vector<String::Char> v;
	int codepage = 0;
	for (char ch; is.get(ch);) {
		switch (ch) {
			case '{':
			case '}':
				break;
			case '\\':
				if (is.get(ch)) {
					if (ch == '\'') {
						Encoding enc(codepage);
						char ar[3];
						is.get(ar, 2);
						byte b = (byte)Convert::ToUInt32(ar);
						vector<String::Char> vv = Encoding(codepage).GetChars(ConstBuf(&b, 1));
						v.insert(v.end(), vv.begin(), vv.end());
					} else {
						String tag;
						while (isalpha(ch)) {
							tag += String(ch);
							if (!is.get(ch))
								break;
						}
						is.putback(ch);
						if (tag == "par")
							v.push_back('\n');
						else if (tag == "tab")
							v.push_back('\t');
						else if (tag == "lang")
							is >> codepage;
					}
				}
				break;
			default:
				v.push_back(ch);
		}
	}
	return v.size() ? String(&v[0], v.size()) : String();
#else
	static class CRichEditRegistrator : public CWnd
	{
	public:
		CRichEditRegistrator() {
			AfxInitRichEdit();
			CreateEx(0, RICHEDIT_CLASS, _T("RichEdit"), ES_MULTILINE, 0, 0, 0, 0, 0, 0, 0);
		}

		~CRichEditRegistrator() {
			Detach(); //!!! because exception if we DestroyWindow in DLL
		}
	} s_richEditRegistrator;
	SETTEXTEX st = { ST_DEFAULT, CP_ACP };
	if (s_richEditRegistrator.SendMessage(EM_SETTEXTEX, (WPARAM)&st, (LPARAM)(const char*)rtf))
		return s_richEditRegistrator.Text;
	else
		return rtf;
#endif
}

static StaticRegex s_reBr("<br\\s*/?>", regex_constants::icase),
			s_reHtmlTags("<(/?(HTML|B|BR|((BODY|FONT|IMG)(\\s+[^=\\>]+=\"[^\">]+\")*)))\\s*/?>", regex_constants::icase);

String AFXAPI HtmlToText(RCString html) {
	Blob blob = Encoding::UTF8.GetBytes(html);
	string s1 = regex_replace(string((const char*)blob.constData(), blob.Size), *s_reBr, string(" "));
	string s2 = regex_replace(s1, *s_reHtmlTags, string(""));
	return Encoding::UTF8.GetChars(ConstBuf(s2.data(), s2.size()));
}


} // Ext::


