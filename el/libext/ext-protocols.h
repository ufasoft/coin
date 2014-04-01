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

class CHttpRequest : public CHttpHeader {
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
	void ParseParams(RCString s);
protected:
	CBool m_bParams;
	NameValueCollection m_params;	
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



