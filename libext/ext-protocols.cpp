/*######     Copyright (c) 1997-2013 Ufasoft  http://ufasoft.com  mailto:support@ufasoft.com,  Sergey Pavlov  mailto:dev@ufasoft.com #######################################
#                                                                                                                                                                          #
# This program is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation;  #
# either version 3, or (at your option) any later version. This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the      #
# implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details. You should have received a copy of the GNU #
# General Public License along with this program; If not, see <http://www.gnu.org/licenses/>                                                                               #
##########################################################################################################################################################################*/

#include <el/ext.h>

#include <el/libext/ext-net.h>

namespace Ext {
using namespace std;

UInt16 AFXAPI CalculateWordSum(const ConstBuf& mb, UInt32 sum, bool bComplement) {
	UInt16 *p = (UInt16*)mb.P;
	for (size_t count=mb.Size>>1; count--;)
		sum += *p++;
	if (mb.Size & 1)
		sum += *(byte*)p;
	for (UInt32 w; w=sum>>16;)
		sum = (sum & 0xFFFF)+w;
	return UInt16(bComplement ? ~sum : sum);
}

String CHttpHeader::ParseHeader(const vector<String>& ar, bool bIncludeFirstLine, bool bEmailHeader) {
	Headers.clear();
	Data.Size = 0;
	m_bDataSkipped = false;
	int i=0;
	if (!bIncludeFirstLine) {
		if (ar.empty())
			Throw(E_FAIL);
		i = 1;
	}
	String prev;
	for (; i<ar.size(); i++) {
		String line = ar[i];
		if (bEmailHeader && line.Length > 0 && isspace(line[0])) {
			if (prev.IsEmpty())
				Throw(E_FAIL);
			Headers[prev].back() += " "+line.TrimLeft();
		} else {
			vector<String> vec = line.Split(":", 2);
			if (vec.size() != 2)
				Throw(E_FAIL);
			Headers[prev = vec[0].Trim().ToUpper()].push_back(vec[1].Trim());
		}
	}
	return bIncludeFirstLine ? "" : ar[0];
}

String CHttpHeader::get_Content() {
	Ext::Encoding *enc = GetEncoding();
	return enc->GetChars(Data);
}

ostream& AFXAPI operator<<(ostream& os, const CHttpHeader& header) {
	header.PrintFirstLine(os);
	for (NameValueCollection::const_iterator i=header.Headers.begin(); i!=header.Headers.end(); ++i)
		os << i->first << ": " << i->second << "\r\n";
	return os << "\r\n";
}

void CHttpRequest::Parse(const vector<String>& ar) {
	String s = ParseHeader(ar);
	vector<String> sar = s.Split();
	if (sar.size()<2)
		Throw(E_FAIL);
	Method = sar[0];
	RequestUri = sar[1];
}

void CHttpResponse::Parse(const vector<String>& ar) {
	String s = ParseHeader(ar);
	vector<String> sar = s.Split();
	if (sar.size()<2 || sar[0].Left(5) != "HTTP/")
		Throw(E_FAIL);
	Code = atoi(sar[1]);
}

void CHttpRequest::ParseParams(RCString s) {
	vector<String> pars = s.Mid(1).Split("&");
	for (int i=0; i<pars.size(); ++i) {
		String par = pars[i];
		vector<String> pp = par.Split("=");
		if (pp.size() == 2) {
			String key = pp[0],
				 pp1 = pp[1];
			pp1.Replace("+", " ");
			m_params[key.ToUpper()].push_back(Uri::UnescapeDataString(pp1));
		}
	}
}

NameValueCollection& CHttpRequest::get_Params() {
	if (!m_bParams) {
		m_bParams = true;

		String query = Uri("http://host"+RequestUri).Query;
		if (!!query) {
			if (query.Length > 1 && query[0]=='?')
				ParseParams(query);
			else if (Method == "POST")
				ParseParams(Encoding::UTF8.GetChars(Data));			
		}
	}
	return m_params;
}





} // Ext::

