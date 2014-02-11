/*######     Copyright (c) 1997-2013 Ufasoft  http://ufasoft.com  mailto:support@ufasoft.com,  Sergey Pavlov  mailto:dev@ufasoft.com #######################################
#                                                                                                                                                                          #
# This program is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation;  #
# either version 3, or (at your option) any later version. This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the      #
# implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details. You should have received a copy of the GNU #
# General Public License along with this program; If not, see <http://www.gnu.org/licenses/>                                                                               #
##########################################################################################################################################################################*/

#include <el/ext.h>

#include <el/xml.h>

namespace Ext {

String ComXmlException::get_Message() const {
	return Reason+" at line "+Convert::ToString(LineNumber)+", position "+Convert::ToString(LinePosition); //!!!+" in "+Url;
}

void AFXAPI XmlCheck(HRESULT hr, bool bAllowedFALSE) {
	if (bAllowedFALSE && hr==S_FALSE)
		return;
	if (OleCheck(hr) != S_OK)
		Throw(E_EXT_XMLError);
}

void AFXAPI ThrowXmlException(IXMLDOMDocument *doc) {
	ComXmlException e;
	OleCheck(doc->get_parseError(&e));
	throw e;
}

XmlNode::operator XmlElement() {
	XmlElement r;
	if (_self && !TryQueryInterface(&r))
		Throw(E_EXT_InvalidCast);
	return r;
}

XmlNode::operator XmlNodeList() {
	XmlNodeList r;
	if (_self && !TryQueryInterface(&r))
		Throw(E_EXT_InvalidCast);
	return r;
}

XmlNode::operator XmlAttribute() {
	XmlAttribute r;
	if (_self && !TryQueryInterface(&r))
		Throw(E_EXT_InvalidCast);
	return r;
}

void XmlNode::AppendChildText(String& s) {
	for (XmlNode n=FirstChild; n; n=n.NextSibling) {
		if (n.FirstChild)
			n.AppendChildText(s);
		else {
			switch (n.NodeType)
			{
			case XmlNodeType::Text:
			case XmlNodeType::CDATA:
			case XmlNodeType::Whitespace:
			case XmlNodeType::SignificantWhitespace:
				s += n.InnerText;
			}
		}
	}
}

String XmlNode::get_Value() {
	COleVariant v;
	XmlCheck(R.get_nodeValue(&v));
	return Convert::ToString(v);
}

void XmlNode::put_Value(RCString v) {
	XmlCheck(R.put_nodeValue(COleVariant(v)));
}

XmlNodeList XmlNode::SelectNodes(RCString xpath) {
	XmlNodeList r;
	XmlCheck(R.selectNodes(Bstr(xpath), &r));
	return r;
}

XmlAttributeCollection XmlNode::get_Attributes() {
	XmlAttributeCollection r;
	XmlCheck(R.get_attributes(&r));
	return r;
}

XmlNodeList XmlNode::get_ChildNodes() {
	XmlNodeList r;
	XmlCheck(R.get_childNodes(&r));
	return r;
}

XmlDocument XmlNode::get_OwnerDocument() {
	XmlDocument r;
	XmlCheck(R.get_ownerDocument(&r));
	return r;
}

XmlNode XmlNode::AppendChild(XmlNode n) {
	XmlNode r;
	XmlCheck(R.appendChild(n, &r));
	return r;
}

XmlNode XmlNode::RemoveChild(XmlNode n) {
	XmlNode r;
	XmlCheck(R.removeChild(n, &r));
	return r;
}

void XmlNode::RemoveAll() {
	for (XmlNode n=FirstChild; n;)
		RemoveChild(exchange(n, n.NextSibling));
}

String XmlNode::get_InnerText() {
	if (XmlNode n = FirstChild) {
		if (!n.NextSibling) {
			switch (n.NodeType)
			{
			case XmlNodeType::Text:
			case XmlNodeType::CDATA:
			case XmlNodeType::Whitespace:
			case XmlNodeType::SignificantWhitespace:
				return n.Value;
			}
		}
		String s;
		AppendChildText(s);
		return s;
	}
	return "";
}

void XmlNode::put_InnerText(RCString v) {
	if (XmlNode n = FirstChild) {
		if (!n.NextSibling && n.NodeType == XmlNodeType::Text) {
			n.Value = v;
			return;
		}
	}
	RemoveAll();
	AppendChild(OwnerDocument.CreateTextNode(v));
}

String XmlElement::GetAttribute(const CStrPtr& name) const {
	COleVariant r;
	OleCheck(R.getAttribute(Bstr(name.c_str()), &r));
	if (r.vt == VT_NULL)
		return "";
	return Convert::ToString(r);
}

XmlDocument::XmlDocument(XmlDocument *doc) {
	delete doc;
	_self = New();
}

XmlDocument XmlDocument::New() {
	XmlDocument doc;
	doc.Assign(CreateComObject("microsoft.xmldom"), &__uuidof(IXMLDOMDocument));
	return doc;
}

const char g_xsltIndent[] = "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
"<xsl:stylesheet xmlns:xsl=\"http://www.w3.org/1999/XSL/Transform\" version=\"1.0\">\n"
"<xsl:output method=\"xml\" indent=\"yes\" encoding=\"UTF-8\"/>\n"
"<xsl:template match=\"@* | node()\">\n"
"<xsl:copy>\n"
"<xsl:apply-templates select=\"@* | node()\" />\n"
"</xsl:copy>\n"
"</xsl:template>\n"
"</xsl:stylesheet>";

void XmlDocument::SaveIndented(RCString filename) {
	XmlDocument xslt = new XmlDocument;
	try {
		xslt.LoadXml(g_xsltIndent);
	} catch (ComXmlException e) {
//!!!		cerr <<	e.Message << endl; //!!!
		Throw(E_FAIL);//!!!
	}
	XmlDocument doc = new XmlDocument;
	TransformNodeToObject(xslt, &doc.R);
	doc.Save(filename);
}

ComXmlException XmlDocument::get_ParseError() {
	ComXmlException r;
	XmlCheck(R.get_parseError(&r));
	return r;
}

bool XmlDocument::Load(RCString filename) {
	VARIANT_BOOL b;
	OleCheck(R.load(COleVariant(filename), &b));
	if (!b)
		ThrowXmlException(&R);
	return b;
}

XmlHttpRequest::XmlHttpRequest(XmlHttpRequest *req) {
	delete req;
	Assign(CreateComObject(CLSID_XMLHTTPRequest));
}

void XmlHttpRequest::Open(RCString method, RCString url, bool bAsync, RCString user, RCString password) {
	XmlCheck(R.open(Bstr(method), Bstr(url), COleVariant(bAsync), COleVariant(user), COleVariant(password)));
}

void XmlHttpRequest::Send(const COleVariant& body) {
	XmlCheck(R.send(body));
}

XmlElement XmlAttribute::get_OwnerElement() {
	return XmlElement(ParentNode);
}


} // Ext::
