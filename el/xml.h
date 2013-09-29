/*######     Copyright (c) 1997-2013 Ufasoft  http://ufasoft.com  mailto:support@ufasoft.com,  Sergey Pavlov  mailto:dev@ufasoft.com #######################################
#                                                                                                                                                                          #
# This program is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation;  #
# either version 3, or (at your option) any later version. This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the      #
# implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details. You should have received a copy of the GNU #
# General Public License along with this program; If not, see <http://www.gnu.org/licenses/>                                                                               #
##########################################################################################################################################################################*/

#pragma once

#include EXT_HEADER(stack)

#if UCFG_WIN32
#	include <msxml2.h>

#	include <el/libext/win32/ext-win.h>
#endif

#if UCFG_USE_LIBXML

#	if (defined(_EXT) || defined(_BUILD_XML)) && defined(_WIN32)
#		define LIBXML_STATIC
#	endif

#	include <libxml/xmlreader.h>
#	include <libxml/xpath.h>
#endif

#if defined(_AFXDLL)
#	ifdef _BUILD_XML
#		define EXT_XML_API DECLSPEC_DLLEXPORT
#	elif !UCFG_WCE
//!!!T #		pragma comment(lib, "xml")
#		define EXT_XML_API DECLSPEC_IMPORT
#	endif
#else
#	define EXT_XML_API
#endif

namespace Ext {

class XmlElement;
class XmlAttributeCollection;
class XmlNodeList;
class XmlDocument;
class XmlReader;
class XmlAttribute;

void AFXAPI XmlCheck(HRESULT hr, bool bAllowedFALSE = false);

ENUM_CLASS(XmlNodeType)  {
	None,
	Element,
	Attribute,
	Text,
	CDATA,
	EntityReference,
	Entity,
	ProcessingInstruction,
	Comment,
	Document,
	DocumentType,
	DocumentFragment,
	Notation,
	Whitespace,
	SignificantWhitespace,
	EndElement,
	EndEntity,
	XmlDeclaration
} END_ENUM_CLASS(XmlNodeType);
	
class XmlException : public SystemException {
	typedef SystemException base;
public:
	typedef XmlException class_type;

	XmlException(HRESULT hr = E_EXT_XMLError, RCString message = String())
		:	base(hr, message)
	{}

	XmlException(RCString message)
		:	base(E_EXT_XMLError, message)
	{}

	virtual long get_LineNumber() const { Throw(E_NOTIMPL); }
	DEFPROP_VIRTUAL_GET_CONST(long, LineNumber);

	virtual long get_LinePosition() const { Throw(E_NOTIMPL); }
	DEFPROP_VIRTUAL_GET_CONST(long, LinePosition);

	virtual String get_Url() const { Throw(E_NOTIMPL); }
	DEFPROP_VIRTUAL_GET_CONST(String, Url);

	virtual String get_Reason() const { Throw(E_NOTIMPL); }
	DEFPROP_VIRTUAL_GET_CONST(String, Reason);
};

class CStrPtr {
public:
	CStrPtr(const CStrPtr& cs)
		:	m_p(cs.m_p)
	{}

	CStrPtr(const char *p)
		:	m_p(p)
	{}

	CStrPtr(RCString s)
		:	m_p(s.c_str())
	{}

	CStrPtr(const std::string& s)
		:	m_p(s.c_str())
	{}

	const char* c_str() const { return m_p; }
private:
	const char *m_p;
};

interface IXmlAttributeCollection {
	virtual String GetAttribute(const CStrPtr& name) const =0;
	virtual String GetAttribute(int idx) const { Throw(E_NOTIMPL); } 
};


#if UCFG_WIN32

void AFXAPI ThrowXmlException(IXMLDOMDocument *doc);

class ComXmlException : public CComPtr<IXMLDOMParseError>, public XmlException {
public:
	long get_LineNumber() const {
		long r;
		XmlCheck((*this)->get_line(&r));
		return r;
	}

	long get_LinePosition() const {
		long r;
		XmlCheck((*this)->get_linepos(&r));
		return r;
	}

	String get_Url() const {
		CComBSTR bstr;
		XmlCheck((*this)->get_url(&bstr));
		return bstr;
	}

	String get_Reason() const {
		CComBSTR bstr;
		XmlCheck((*this)->get_reason(&bstr));
		return bstr;
	}

	String get_Message() const override;
};

template <class B, class I> class XmlWrap : public B {
	typedef XmlWrap class_type;
public:
	I& get_R() const {
		return *(I*)m_unk;
	}
	DEFPROP_GET(I&, R);
protected:
  	I **operator&() {
    	if (m_unk)
      		Throw(E_EXT_InterfaceAlreadyAssigned);
    	return (I**)&m_unk;
  	}

friend class XmlNode;
friend class XmlNodeList;
friend class XmlNamedNodeMap;
friend class XmlDocument;
friend class XmlElement;
friend class XmlAttributeCollection;
};

class XmlNode : public XmlWrap<CComPtr<IXMLDOMNode>, IXMLDOMNode> {
	typedef XmlNode class_type;
	typedef XmlWrap<CComPtr<IXMLDOMNode>, IXMLDOMNode>  base;
public:
	typedef XmlNode class_type;

	operator XmlElement();
	operator XmlNodeList();
	operator XmlAttribute();

	XmlNode(nullptr_t null = nullptr)
		:	base()
	{}

	XmlNodeType get_NodeType() {
		DOMNodeType r;
		XmlCheck(R.get_nodeType(&r));
		return (XmlNodeType)r;
	}
	DEFPROP_GET(XmlNodeType, NodeType);

	String get_Name() {
		CComBSTR r;
		XmlCheck(R.get_nodeName(&r));
		return r;
	}
	DEFPROP_GET(String, Name);

	String get_Value();
	void put_Value(RCString v);
	DEFPROP(String, Value);

	XmlNode get_ParentNode() {
		XmlNode r;
		XmlCheck(R.get_parentNode(&r));
		return r;
	}
	DEFPROP_GET(XmlNode, ParentNode);

	XmlNode CloneNode(bool deep) {
		XmlNode r;
		XmlCheck(R.cloneNode(deep, &r));
		return r;
	}

	XmlNode Clone() { return CloneNode(true); }

	XmlNode InsertBefore(XmlNode newChild, XmlNode refChild) {
		XmlNode r;
		XmlCheck(R.insertBefore(newChild, COleVariant(refChild), &r));
		return r;
	}

	XmlAttributeCollection get_Attributes();
	DEFPROP_GET(XmlAttributeCollection, Attributes);

	XmlDocument get_OwnerDocument();
	DEFPROP_GET(XmlDocument, OwnerDocument);

	XmlNodeList get_ChildNodes();
	DEFPROP_GET(XmlNodeList, ChildNodes);

	XmlNode get_NextSibling() {
		XmlNode r;
		OleCheck(R.get_nextSibling(&r));
		return r;
	}
	DEFPROP_GET(XmlNode, NextSibling);

	XmlNode AppendChild(XmlNode n);
	XmlNode RemoveChild(XmlNode n);
	void RemoveAll();

	String get_InnerText();
	void put_InnerText(RCString v);
	DEFPROP(String, InnerText);

	XmlNode get_FirstChild() {
		XmlNode r;
		OleCheck(R.get_firstChild(&r));
		return r;
	}
	DEFPROP_GET(XmlNode, FirstChild);

	String get_OuterXml() {
		CComBSTR bstr;
		XmlCheck(R.get_xml(&bstr));
		return bstr;
	}
	DEFPROP_GET(String, OuterXml);

	XmlNodeList SelectNodes(RCString xpath);

	XmlNode SelectSingleNode(RCString xpath) {
		XmlNode r;
		OleCheck(R.selectSingleNode(Bstr(xpath), &r));
		return r;
	}

	String TransformNode(XmlNode stylesheet) {
		CComBSTR bstr;
		XmlCheck(R.transformNode(stylesheet, &bstr));
		return bstr;
	}

	void TransformNodeToObject(XmlNode stylesheet, const COleVariant& v) {
		XmlCheck(R.transformNodeToObject(stylesheet, v));
	}
protected:
	XmlNode FindChild(XmlNodeType type) {
		XmlNode r;
		for (r=FirstChild; r && r.NodeType!=type; r=r.NextSibling)
			;
		return r;
	}
private:
	void AppendChildText(String& s);
};

class XmlNodeList : public XmlWrap<CComPtr<IXMLDOMNodeList>, IXMLDOMNodeList> {
public:
	typedef XmlNodeList class_type;

	long get_Count() {
		long r;
		XmlCheck(R.get_length(&r));
		return r;
	}
	DEFPROP_GET(long, Count);

	XmlNode NextNode() {
		XmlNode r;
		OleCheck(R.nextNode(&r));
		return r;
	}

	XmlNode operator[](int idx) {
		XmlNode r;
		XmlCheck(R.get_item(idx, &r));
		return r;
	}

	void Reset() {
		XmlCheck(R.reset());
	}
};

class XmlNamedNodeMap : public XmlWrap<CComPtr<IXMLDOMNamedNodeMap>, IXMLDOMNamedNodeMap> {
public:
	typedef XmlNamedNodeMap class_type;

	long get_Count() {
		long r;
		XmlCheck(R.get_length(&r));
		return r;
	}
	DEFPROP_GET(long, Count);

	XmlNode Item(int idx) {
		XmlNode r;
		XmlCheck(R.get_item(idx, &r));
		return r;
	}

	XmlNode GetNamedItem(RCString name) {
		XmlNode r;
		XmlCheck(R.getNamedItem(Bstr(name), &r));
		return r;
	}

	XmlNode RemoveNamedItem(RCString name) {
		XmlNode r;
		XmlCheck(R.removeNamedItem(Bstr(name), &r));
		return r;
	}

	XmlNode SetNamedItem(XmlNode n) {
		XmlNode r;
		XmlCheck(R.setNamedItem(n, &r));
		return r;
	}
};

class XmlAttribute : public XmlWrap<XmlNode, IXMLDOMAttribute> {
	typedef XmlAttribute class_type;
public:
	XmlElement get_OwnerElement();
	DEFPROP_GET(XmlElement, OwnerElement);
};

class XmlAttributeCollection : public XmlNamedNodeMap {
public:
	XmlAttribute operator[](int idx) {
		XmlAttribute a;
		a.Assign(Item(idx));
		return a;
	}

	XmlAttribute operator[](RCString name) {
		XmlNode n;
		XmlCheck(R.getNamedItem(Bstr(name), &n), true);
		return XmlAttribute(n);
	}

	void RemoveAll() {
		while (Count > 0)
			RemoveNamedItem(Item(0).Name);
	}
};

class XmlLinkedNode : public XmlNode {
};

class XmlElement : public XmlWrap<XmlLinkedNode, IXMLDOMElement>, public IXmlAttributeCollection {
	typedef XmlElement class_type;
public:	
	String GetAttribute(const CStrPtr& name) const override;
	String GetAttribute(int idx) const override { Throw(E_NOTIMPL); } //!!!

	void SetAttribute(RCString name, RCString v) {
		XmlCheck(R.setAttribute(Bstr(name), COleVariant(v)));
	}

	XmlAttribute GetAttributeNode(RCString name) {
		XmlAttribute a;
		XmlCheck(R.getAttributeNode(Bstr(name), &a), true);
		return a;
	}

	bool HasAttribute(RCString name) { return GetAttributeNode(name); }
	
	bool get_IsEmpty() { return !FirstChild; }
	DEFPROP_GET(bool, IsEmpty);

};

class XmlCharacterData : public XmlWrap<XmlLinkedNode, IXMLDOMCharacterData> {
};

class XmlText : public XmlWrap<XmlCharacterData, IXMLDOMText> {
};

class XmlDocument : public XmlWrap<XmlNode, IXMLDOMDocument> {
public:
	typedef XmlDocument class_type;

	XmlDocument() {}
	XmlDocument(XmlDocument *doc);
	static XmlDocument New();

	XmlElement get_DocumentElement() {
		XmlElement r;
		XmlCheck(R.get_documentElement(&r));
		return r;

	}
	DEFPROP_GET(XmlElement, DocumentElement);

	ComXmlException get_ParseError();
	DEFPROP_GET(ComXmlException, ParseError);

	XmlAttribute CreateAttribute(RCString name) {
		XmlAttribute r;
		XmlCheck(R.createAttribute(Bstr(name), &r));
		return r;
	}

	XmlElement CreateElement(RCString name) {
		XmlElement r;
		XmlCheck(R.createElement(Bstr(name), &r));
		return r;
	}

	XmlText CreateTextNode(RCString text) {
		XmlText r;
		XmlCheck(R.createTextNode(Bstr(text), &r));
		return r;
	}

	bool Load(RCString filename);

	bool Load(IStream *iStm) {
		VARIANT_BOOL b;
		OleCheck(R.load(COleVariant(iStm), &b));
		return b;
//!!!		if (!b)
//!!!			ThrowXmlException(&R);
	}

	bool LoadXml(RCString xml) {
		VARIANT_BOOL b;
		OleCheck(R.loadXML(Bstr(xml), &b));
		return b;
//!!!		if (!b)
//!!!			ThrowXmlException(&R);
	}

	void Save(RCString filename) {
		XmlCheck(R.save(COleVariant(filename)));
	}

	void Save(IStream *iStm) {
		XmlCheck(R.save(COleVariant(iStm)));
	}

	void SaveIndented(RCString filename);
};

typedef IXMLHTTPRequest IXMLHttpRequest; //!!! IXMLHTTPRequest is new Interface

class XmlHttpRequest : public XmlWrap<CComPtr<IXMLHttpRequest>, IXMLHttpRequest> {
public:
	typedef XmlHttpRequest class_type;

	XmlHttpRequest() {}
	XmlHttpRequest(XmlHttpRequest *req);
	void Open(RCString method, RCString url, bool bAsync = true, RCString user = nullptr, RCString password = nullptr);
	void Send(const COleVariant& body = "");

	XmlDocument get_ResponseXml() {
		CComPtr<IDispatch> r;
		XmlCheck(R.get_responseXML(&r));
		XmlDocument doc;
		doc.Assign(CComPtr<IXMLDOMDocument>(r));
		return doc;
	}
	DEFPROP_GET(XmlDocument, ResponseXml);
};
#endif // UCFG_WIN32


#if UCFG_USE_LIBXML

class LibxmlXmlException : public XmlException {
	typedef XmlException base;
public:
	LibxmlXmlException(xmlError *err);
	LibxmlXmlException(const LibxmlXmlException& e);

	~LibxmlXmlException() {
		xmlResetError(&m_xmlError);
	}

	long get_LineNumber() const {
		return m_xmlError.line;
	}

	long get_LinePosition() const {
		return m_xmlError.int2;
	}

	String get_Url() const {
		return m_xmlError.file;
	}

	String get_Reason() const {
		return m_xmlError.message;
	}

	String get_Message() const override;
private:
	xmlError m_xmlError;
	
	LibxmlXmlException& operator=(const LibxmlXmlException&);
};

ENUM_CLASS(ReadState) {				// compatible with xmlTextReaderMode
	Initial = 0,
	Interactive = 1,
	Error = 2,
	EndOfFile = 3,
	Closed = 4
} END_ENUM_CLASS(ReadState);

class XmlReader : public IXmlAttributeCollection {
	typedef XmlReader class_type;
public:
//!!!	mutable Ext::ReadState ReadState;

	XmlReader()
		:	m_p(0)
//!!!R		,	ReadState(Ext::ReadState::Initial)
	{}

	XmlReader(const XmlReader& r)
		:	m_p(r.m_p)
		,	m_str(r.m_str)
	{
	}

	virtual void Close() const =0;

	virtual int get_AttributeCount() const =0;
	DEFPROP_VIRTUAL_GET_CONST(int, AttributeCount);

	virtual String get_Name() const { Throw(E_NOTIMPL); }
	DEFPROP_VIRTUAL_GET_CONST(String, Name);

	virtual XmlNodeType get_NodeType() const =0;
	DEFPROP_VIRTUAL_GET_CONST(XmlNodeType, NodeType);

	virtual bool get_HasValue() const =0;
	DEFPROP_VIRTUAL_GET_CONST(bool, HasValue);

	virtual String get_Value() const =0;
	DEFPROP_VIRTUAL_GET_CONST(String, Value);

	virtual int get_Depth() const =0;
	DEFPROP_VIRTUAL_GET_CONST(int, Depth);

	virtual Ext::ReadState get_ReadState() const =0;
	DEFPROP_VIRTUAL_GET_CONST(Ext::ReadState, ReadState);

	virtual bool get_Eof() const;
	DEFPROP_VIRTUAL_GET_CONST(bool, Eof);

	virtual bool IsEmptyElement() const =0;
	virtual bool IsStartElement() const;
	virtual bool IsStartElement(const CStrPtr& name) const;
	virtual XmlNodeType MoveToContent() const;
	virtual bool MoveToElement() const =0;
	virtual bool MoveToFirstAttribute() const =0;
	virtual bool MoveToNextAttribute() const =0;
	virtual bool Read() const =0;
	virtual void ReadEndElement() const;
	virtual void ReadStartElement() const;
	virtual void ReadStartElement(const CStrPtr& name) const;
	virtual String ReadString() const;
	virtual String ReadElementString() const;
	virtual String ReadElementString(RCString name) const;
	virtual bool ReadToDescendant(RCString name) const;
	virtual bool ReadToFollowing(RCString name) const;
	virtual bool ReadToNextSibling(RCString name) const;
	virtual void Skip() const =0;
protected:
	mutable void *m_p;
	String m_str;

	void Init(void *p);
};

class XmlTextReader : public XmlReader {
public:
	XmlTextReader() {}
	EXT_XML_API XmlTextReader(std::istream& is);
	XmlTextReader(RCString uri);
	~XmlTextReader();

	static XmlTextReader CreateFromString(RCString s);

	void Close() const override;
	int get_AttributeCount() const override;
	String get_Name() const override;
	XmlNodeType get_NodeType() const override;
	bool get_HasValue() const override;
	String get_Value() const override;
	int get_Depth() const override;
	Ext::ReadState get_ReadState() const override;
	
	String GetAttribute(int idx) const override;
	String GetAttribute(const CStrPtr& name) const override;
	bool IsEmptyElement() const override;
	bool MoveToElement() const override;
	bool MoveToFirstAttribute() const override;
	bool MoveToNextAttribute() const override;
	bool Read() const override;
	void Skip() const override;

	int LineNumber() const;
	int LinePosition() const;
};

#endif // UCFG_USE_XML

class XmlWriter {
public:
	virtual ~XmlWriter() {
	}

	virtual void Close();
	virtual void WriteAttributeString(const char *name, RCString value) =0;
	virtual void WriteAttributeString(RCString name, RCString value) =0;
	virtual void WriteCData(RCString text) =0;
	virtual void WriteElementString(RCString name, RCString value) =0;
	virtual void WriteEndElement() =0;
	virtual void WriteStartDocument() =0;
	virtual void WriteProcessingInstruction(RCString name, RCString text) =0;
	virtual void WriteStartElement(RCString name) =0;
	virtual void WriteString(RCString text) =0;
	void WriteNode(const XmlReader& r);
	virtual void WriteRaw(RCString s) =0;
protected:
	std::stack<std::string> m_elements;
};

class XmlOut {
public:
	class AttributeAssigner {
	public:
		AttributeAssigner(XmlWriter& xmlWriter, RCString attrName)
			:	_xmlWriter(xmlWriter)
			,	_attrName(attrName)
			,	_pch(0)
		{}

		AttributeAssigner(XmlWriter& xmlWriter, const char *pch)
			:	_xmlWriter(xmlWriter)
			,	_pch(pch)
		{}

		AttributeAssigner& operator=(RCString value) {
			if (_pch)
				_xmlWriter.WriteAttributeString(_pch, value);
			else
				_xmlWriter.WriteAttributeString(_attrName, value);
			return (*this);
		}

		AttributeAssigner& operator=(const char *s) {
			if (_pch)
				_xmlWriter.WriteAttributeString(_pch, s);
			else
				_xmlWriter.WriteAttributeString(_attrName, s);
			return (*this);
		}

		AttributeAssigner& operator=(int value) {
			return operator=(Convert::ToString(value));
		}

		AttributeAssigner& operator=(long value) {
			return operator=(Convert::ToString(value));
		}

		AttributeAssigner& operator=(size_t value) {
			return operator=((int)value); //!!!
		}

		AttributeAssigner& operator=(bool value) {
			return operator=(std::string(value ? "1" : "0"));
		}
	private:
		XmlWriter& _xmlWriter;
		String _attrName;
		const char *_pch;

		AttributeAssigner& operator=(void *); // disabled
	};


	class XmlWriter& XmlWriter;

	XmlOut(class XmlWriter& xmlWriter, RCString name = "")
		:	XmlWriter(xmlWriter)
		,	m_bElement(!name.empty())
	{
		if (m_bElement)
			XmlWriter.WriteStartElement(name);
	}

	~XmlOut() {
		if (m_bElement)
			XmlWriter.WriteEndElement();
	}

	AttributeAssigner operator[](RCString attrName) {
		return AttributeAssigner(XmlWriter, attrName);
	}

	AttributeAssigner operator[](const char *attrName) {
		return AttributeAssigner(XmlWriter, attrName);
	}
private:
	CBool m_bElement;
};

ENUM_CLASS(XmlFormatting) {
	Indented,
	None
} END_ENUM_CLASS(XmlFormatting);

class XmlTextWriter : public XmlWriter {
public:
	CPointer<Ext::Encoding> Encoding;
	int Indentation;
	char IndentChar;

	Ext::XmlFormatting Formatting;

	XmlTextWriter(std::ostream& os, Ext::Encoding *enc = &Ext::Encoding::UTF8)
		:	m_os(os)
		,	Encoding(enc)
	{
		CommonInit();
	}

	XmlTextWriter(RCString filename, Ext::Encoding *enc = &Ext::Encoding::UTF8)
		:	m_ofs(filename.c_str())
		,	m_os(m_ofs)
		,	Encoding(enc)
	{
		CommonInit();
	}

	void Close() override;
	void WriteAttributeString(const char *name, RCString value) override;
	void WriteAttributeString(RCString name, RCString value) override;
	void WriteCData(RCString text) override;
	void WriteElementString(RCString name, RCString value) override;
	void WriteEndElement() override;
	void WriteRaw(RCString s) override;
	void WriteStartDocument() override;
	void WriteProcessingInstruction(RCString name, RCString text) override;
	void WriteStartElement(RCString name) override;
	void WriteString(RCString text) override;
private:
	std::ofstream m_ofs;
	std::ostream& m_os;
	CBool m_bOpenedElement,
		  m_bEoled;

	void CommonInit();
	void Indent();
	void Eol();
	void CloseElement(bool bEol = true);
	void WriteQuotedString(RCString value);
};


class XmlIterator {
public:
	XmlIterator(XmlReader& reader, RCString name);
	operator bool();
	XmlIterator& operator ++();
private:
	XmlReader& m_reader;
	CBool m_bState;
	std::string m_name;
};


std::string quote(const std::string& input);
std::string unquote(const char *str);
std::string unquote(const std::string& input);
std::string quote_normalize(const std::string& input);

#if UCFG_WIN32

class XmlNodeReader : public XmlReader {
	typedef XmlReader base;
public:
	XmlNodeReader(const XmlNode& startNode)
		:	m_startNode(startNode)
		,	m_readState(Ext::ReadState::Initial)
		,	m_depth(0)
	{}

	Ext::ReadState get_ReadState() const override { return m_readState; }

	int get_Depth() const override;
	bool get_Eof() const override;
	XmlNodeType get_NodeType() const override;
	bool get_HasValue() const override;
	String get_Value() const override;
	String get_Name() const override;
	bool IsEmptyElement() const override;
	int get_AttributeCount() const override;
	String GetAttribute(int idx) const override;
	String GetAttribute(const CStrPtr& name) const override;
	bool MoveToElement() const override;
	bool MoveToFirstAttribute() const override;
	bool MoveToNextAttribute() const override;
	bool Read() const override;
	void Skip() const override;
	void Close() const override;
private:
	XmlNode m_startNode;
	mutable XmlNode m_cur, m_linkedNode;
	mutable Ext::ReadState m_readState;
	mutable CBool m_bEndElement;
	mutable int m_depth;
};
#endif // UCFG_WIN32

#if UCFG_USE_LIBXML

class XmlString {
public:
	xmlChar *m_p;
	String m_sNull;
	
	XmlString(xmlChar *p, RCString sNull = nullptr)
		:	m_p(p)
		,	m_sNull(sNull)
	{}

	~XmlString() {
		xmlFree(m_p);
	}

	operator String() const {
		return m_p ? Encoding::UTF8.GetChars(ConstBuf(m_p, strlen((const char*)m_p))) : m_sNull;
	}

	bool operator!() const { return !operator String(); }
private:
	XmlString(const XmlString&);
	XmlString& operator=(const XmlString&);
};


class XPathNodeIterator;
class XPathNavigator;

template<> struct ptr_traits<XPathNavigator> {
	typedef Object::interlocked_policy interlocked_policy;
};

template<> struct ptr_traits<XPathNodeIterator> {
	typedef Object::interlocked_policy interlocked_policy;
};

class XPathDocument : public Object {
	typedef XPathDocument class_type;
public:
	XPathDocument(RCString filename);
	EXT_XML_API XPathDocument(std::istream& is);

	~XPathDocument() {
		if (m_doc)
			xmlFreeDoc(m_doc);
	}

	ptr<XPathNavigator> CreateNavigator();
private:
	CPointer<Ext::Encoding> m_pEncoding;
	xmlDocPtr m_doc;		

};

class XPathNavigator : public Object {
	typedef XPathNavigator class_type;
public:
	XPathNavigator()
		:	m_ctx(0)
		,	m_node(0)
	{}

	~XPathNavigator() {
		if (m_ctx)
			xmlXPathFreeContext(m_ctx);
	}

	ptr<XPathNodeIterator> Select(RCString xpath);
	ptr<XPathNavigator> SelectSingleNode(RCString xpath);
	String GetAttribute(RCString name);	

	String get_Value();
	DEFPROP_GET(String, Value);
private:
	xmlXPathContextPtr m_ctx;
	xmlNodePtr m_node;

	friend class XPathDocument;
	friend class XPathNodeIterator;
};

class XPathNodeIterator : public Object {
	typedef XPathNodeIterator class_type;
public:
	XPathNodeIterator()
		:	m_p(0)
		,	m_i(-1)
	{}

	~XPathNodeIterator() {
		if (m_p)
			xmlXPathFreeObject(m_p);
	}

	bool MoveNext();

	ptr<XPathNavigator> get_Current();
	DEFPROP_GET(ptr<XPathNavigator>, Current);
private:
	xmlXPathObjectPtr m_p;
	int m_i;

	friend class XPathNavigator;
};

#endif // UCFG_USE_LIBXML



} // Ext::

