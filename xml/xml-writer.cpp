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

string quote(const string& input) {
	for (string::const_iterator c = input.begin(); c != input.end(); ++c ) {
		switch ( *c )
		{
		case '&':
		case '<':
		case '>':
		case '\"':
			goto LAB_ANALYZE;
		default:
			if ((unsigned char)*c < 32)
				switch (*c)
			{
				case '\t':
				case '\n':
				case '\r':
					break;
				default:
					goto LAB_ANALYZE;
			}
		}
	}
	return input;
LAB_ANALYZE:
	ostringstream os;			
	for (string::const_iterator c = input.begin(); c != input.end(); ++c ) {
		switch (*c) {
		case '&':
			os << "&amp;";
			break;			
		case '<':
			os <<  "&lt;";
			break;
		case '>':
			os <<  "&gt;";
			break;			
		case '\"':
			os <<  "&quot;";
			break;			
		default:
			if ((unsigned char)*c < 32) {
				switch (*c) {
				case '\t':
				case '\n':
				case '\r':
					break;
				default:
					//							LOG_DEBUG("Invalid char for XML: " << hex << (int)(unsigned char)*c);
					os << "&#" << (int)(unsigned char)*c << ";";
					continue;
				}
			}
			os << *c;
			break;
		}
	}
	return os.str();
}


void XmlWriter::Close() {
	while (!m_elements.empty())
		WriteEndElement();
}


#if 0
void XmlWriter::WriteNode(const XmlReader& r) {
	while (true) {
		switch (r.NodeType()) {
			case XmlNodeType::None:
				if (!r.Read())
					return;
				break;
			case XmlNodeType::Element:
				{
					bool bEmpty = r.IsEmptyElement();
					string name = r.Name();
					cerr << "ElemName=" << name << endl;
					XmlOut xo((*this), name);
					bool bHasAttribs = false;
					/*!!!				while (r.MoveToNextAttribute())
					{
					cerr << "AttrName=" << r.Name() << endl;
					xo[r.Name()] = r.Value();
					bHasAttribs = true;
					}*/
					if (bEmpty)
						r.Skip();
					else {
						//!!!					r.MoveToElement();
						cerr << "NodeType=" << r.NodeType() << r.Name() << endl;
						r.ReadStartElement(); //!!!
						/*
						if (bHasAttribs)
						r.Read();
						else	
						r.ReadStartElement(); //!!!
						*/

						WriteNode(r);
						r.ReadEndElement();
					}
				}
				break;
			case XmlNodeType::Text:
				WriteString(r.ReadString());
				break;
			case XmlNodeType::SignificantWhitespace:
				r.Read();
				break;
			case XmlNodeType::CDATA:
				WriteCData(r.ReadString());
				break;
			case XmlNodeType::EndElement:
				return;
			default:
				throw logic_error("not implemented"); //!!!
		}
	}
}
#endif

void XmlTextWriter::WriteRaw(RCString s) {
	CloseElement(false);
	m_os << s;
}

void XmlTextWriter::CommonInit() {
	Formatting = XmlFormatting::None;
	IndentChar = '\t';
	Indentation = 1;
}

void XmlTextWriter::Indent() {
	if (XmlFormatting::Indented == Formatting && m_elements.size()) {
		m_os << std::string(Indentation*m_elements.size(), IndentChar);
		m_bEoled = false;
	}
}

void XmlTextWriter::Eol() {
	if (XmlFormatting::Indented == Formatting)
		m_os << '\n';
#ifdef _X_DEBUG
	m_os << flush;
#endif
	m_bEoled = true;
}

void XmlTextWriter::CloseElement(bool bEol) {
	if (m_bOpenedElement) {
		m_os << '>';
		m_bOpenedElement = false;
	}
}

void XmlTextWriter::Close() {
	XmlWriter::Close();
	m_os.flush();
	m_ofs.close();
}

void XmlTextWriter::WriteQuotedString(RCString value) {
	Blob blob = Encoding->GetBytes(value);
	m_os << quote(string((char*)blob.constData(), blob.Size));
}

void XmlTextWriter::WriteAttributeString(const char *name, RCString value) {
	if (!m_bOpenedElement)
		throw logic_error(string(__FUNCTION__)+string(" at closed element"));

	m_os << ' ' << name << "=\"";
	WriteQuotedString(value);
	m_os << '\"';
}

void XmlTextWriter::WriteAttributeString(RCString name, RCString value) {
	Blob blob = Encoding->GetBytes(name);
	WriteAttributeString((const char*)blob.constData(), value);
}

void XmlTextWriter::WriteCData(RCString text) {
	if (strstr(text.c_str(), "]]"))
		throw logic_error("Not allowed \"]]\" in CDATA"); //!!! XmlException
	CloseElement(false);
	m_os << "<![CDATA[" << text << "]]>";
	m_bEoled = false;
}

void XmlTextWriter::WriteString(RCString text) {
	CloseElement(false);
	WriteQuotedString(text);
	m_bEoled = false;
}

void XmlTextWriter::WriteEndElement() {
	if (m_bOpenedElement) {
		m_os << "/";
		CloseElement();
		m_elements.pop();
	} else {
		if (m_elements.empty())
			throw logic_error(string(__FUNCTION__)+ string(" at root element"));
		string tag = m_elements.top();
		m_elements.pop();
		if (m_bEoled)
			Indent();
		m_os << "</" << tag << ">";
	}
	Eol();
}

void XmlTextWriter::WriteElementString(RCString name, RCString value) {
	WriteStartElement(name);
	WriteString(value);
	WriteEndElement();
}

void XmlTextWriter::WriteProcessingInstruction(RCString name, RCString text) {
	m_os << "<?" << name << " " << text << "?>";
	Eol();
}

void XmlTextWriter::WriteStartDocument() {
	WriteProcessingInstruction("xml", "version=\"1.0\" encoding=\"utf-8\"");
}

void XmlTextWriter::WriteStartElement(RCString name) {
	CloseElement();
	if (!m_bEoled)
		Eol();
	Indent();
	m_os << '<' << name;
	m_elements.push(name.c_str()); //!!!
	m_bOpenedElement = true;
	m_bEoled = false;
}

} // Ext::
