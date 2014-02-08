/*######     Copyright (c) 1997-2013 Ufasoft  http://ufasoft.com  mailto:support@ufasoft.com,  Sergey Pavlov  mailto:dev@ufasoft.com #######################################
#                                                                                                                                                                          #
# This program is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation;  #
# either version 3, or (at your option) any later version. This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the      #
# implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details. You should have received a copy of the GNU #
# General Public License along with this program; If not, see <http://www.gnu.org/licenses/>                                                                               #
##########################################################################################################################################################################*/

#pragma once

namespace Ext {

UInt16 AFXAPI CalculateWordSum(const ConstBuf& mb, UInt32 sum = 0, bool bComplement = false);

class /*!!!R EXT_API*/ CHttpHeader {
	typedef CHttpHeader class_type;
public:
	NameValueCollection Headers;
	Blob Data;
	CBool m_bDataSkipped;

	virtual ~CHttpHeader() {}
	virtual void PrintFirstLine(std::ostream& os) const {} //!!!
	String ParseHeader(const std::vector<String>& ar, bool bIncludeFirstLine = false, bool bEmailHeader = false);
	virtual void Parse(const std::vector<String>& ar) {}
	
	Encoding *GetEncoding();

	String get_Content();
	DEFPROP_GET(String, Content);	
};

std::ostream& AFXAPI operator<<(std::ostream& os, const CHttpHeader& header);

class /*!!!R EXT_API*/ CHttpRequest : public CHttpHeader {
	typedef CHttpRequest class_type;
public:
	String Method;
	String RequestUri;

	NameValueCollection& get_Params();
	DEFPROP_GET(NameValueCollection&, Params);	

	void PrintFirstLine(std::ostream& os) const {
		os << Method << " " << RequestUri << " HTTP/1.1\r\n";
	}
	void Parse(const std::vector<String>& ar);
protected:
	CBool m_bParams;
	NameValueCollection m_params;
	
	void ParseParams(RCString s);
};

class /*!!!R EXT_API*/ CHttpResponse : public CHttpHeader {
public:
	DWORD Code;

	void PrintFirstLine(std::ostream& os) const {
		os << Code << " HTTP/1.1\r\n";
	}
	void Parse(const std::vector<String>& ar);
};



void AFXAPI ReadOneLineFromStream(const Stream& stm, String& beg, Stream *pDupStream = 0);
EXT_API std::vector<String> AFXAPI ReadHttpHeader(const Stream& stm, Stream *pDupStream = 0);



} // Ext::



