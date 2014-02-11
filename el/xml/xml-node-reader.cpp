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

int XmlNodeReader::get_Depth() const {
return !m_cur ? 0
	: m_cur==m_linkedNode ? m_depth
	: m_cur.NodeType==XmlNodeType::Attribute ? m_depth+1 : m_depth+2;
}

bool XmlNodeReader::get_Eof() const {
	switch (get_ReadState()) {
	case Ext::ReadState::EndOfFile:
	case Ext::ReadState::Error:
		return true;
	}
	return false;
}

XmlNodeType XmlNodeReader::get_NodeType() const {
	return !m_cur ? XmlNodeType::None
		: m_bEndElement ? XmlNodeType::EndElement
		: m_cur.NodeType;
}

bool XmlNodeReader::get_HasValue() const {
	switch (NodeType) {
	case XmlNodeType::Element:
	case XmlNodeType::EntityReference:
	case XmlNodeType::Document:
	case XmlNodeType::DocumentFragment:
	case XmlNodeType::Notation:
	case XmlNodeType::EndElement:
	case XmlNodeType::EndEntity:
		return false;
	}
	return true;
}

String XmlNodeReader::get_Value() const {
	return //!!!? NodeType == XmlNodeType::DocumentType ? XmlDocumentType(m_cur).InternalSubset :
		HasValue ? m_cur.Value : nullptr;
}

String XmlNodeReader::get_Name() const {
	switch (m_cur ? m_cur.NodeType : XmlNodeType::None) {
	case XmlNodeType::Attribute:
	case XmlNodeType::DocumentType:
	case XmlNodeType::Element:
	case XmlNodeType::EntityReference:
	case XmlNodeType::ProcessingInstruction:
	case XmlNodeType::XmlDeclaration:
		return m_cur.Name;
	}
	return String();
}

bool XmlNodeReader::IsEmptyElement() const {
	return m_cur && m_cur.NodeType==XmlNodeType::Element && XmlElement(m_cur).IsEmpty;
}

int XmlNodeReader::get_AttributeCount() const {
	if (get_ReadState() != ReadState::Interactive || m_bEndElement || !m_cur)
		return 0;
	XmlAttributeCollection attrs = m_linkedNode.Attributes;
	return attrs ? attrs.Count : 0;
}

String XmlNodeReader::GetAttribute(int idx) const {
	if (m_bEndElement || !m_cur)
		return nullptr;
	if (XmlAttributeCollection attrs = m_linkedNode.Attributes) {
		XmlAttribute a = attrs[idx];
		return a ? a.Value : nullptr;
	} else
		return nullptr;
}

String XmlNodeReader::GetAttribute(const CStrPtr& name) const {
	if (m_bEndElement || !m_cur)
		return nullptr;
	if (XmlAttributeCollection attrs = m_linkedNode.Attributes) {
		XmlAttribute a = attrs[name.c_str()];
		return a ? a.Value : nullptr;
	} else
		return nullptr;
}

bool XmlNodeReader::MoveToElement() const {
	return m_cur && exchange(m_cur, m_linkedNode) != m_linkedNode;
}

bool XmlNodeReader::MoveToFirstAttribute() const {
	if (!m_cur)
		return false;
	XmlAttributeCollection attrs = m_linkedNode.Attributes;
	return attrs && attrs.Count > 0 ? (m_cur = attrs[0]) : false;
}

bool XmlNodeReader::MoveToNextAttribute() const {
	if (!m_cur)
		return false;
	XmlNode anode = m_cur;
	if (m_cur.NodeType != XmlNodeType::Attribute) {
		XmlNode parent = m_cur.ParentNode;
		if (!parent || parent.NodeType != XmlNodeType::Attribute)
			return MoveToFirstAttribute();
		anode = parent;			
	}

	XmlAttributeCollection attrs = XmlAttribute(anode).OwnerElement.Attributes;	
	for (int i=0, count=attrs.Count; i<count-1; ++i) {
		XmlAttribute a = attrs[i];
		if (a == anode) {
			if (++i == count)
				return false;
			return m_cur = attrs[i];
		}
	}
	return false;
}

bool XmlNodeReader::Read() const {
	switch (get_ReadState()) {
	case Ext::ReadState::EndOfFile:
	case Ext::ReadState::Error:
	case Ext::ReadState::Closed:
		return false;
	case Ext::ReadState::Initial:		
		m_linkedNode = m_cur = m_startNode;
		m_readState = Ext::ReadState::Interactive;
		return bool(m_cur);
	}
	MoveToElement();

	if (XmlNode firstChild = !m_bEndElement ? m_cur.FirstChild : XmlNode()) {
		++m_depth;
		m_linkedNode = m_cur = firstChild;
		return true;
	}
	if (m_cur == m_startNode) {
		if (!(m_bEndElement = !IsEmptyElement() && !m_bEndElement)) {
			m_cur = XmlNode();
			m_readState = ReadState::EndOfFile;
		}
		m_linkedNode = m_cur;
		return m_bEndElement;
	}
	if (!m_bEndElement && !IsEmptyElement() && m_cur.NodeType==XmlNodeType::Element)
		return m_bEndElement = true;
	if (XmlNode next = m_cur.NextSibling) {
		m_linkedNode = m_cur = next;
		return !(m_bEndElement = false);
	}
	XmlNode parent = m_cur.ParentNode;
	if (m_bEndElement = parent && parent!=m_startNode) {
		m_cur = parent;
		--m_depth;
	} else {
		m_cur = nullptr;
		m_readState = ReadState::EndOfFile;
	}
	m_linkedNode = m_cur;
	return m_bEndElement;
}

void XmlNodeReader::Skip() const {
	if (ReadState == ReadState::Interactive) {
		MoveToElement ();
		if (NodeType != XmlNodeType::Element || IsEmptyElement())
			Read();
		else {
			for (int depth=Depth; Read() && depth<Depth; )
				;
			if (NodeType == XmlNodeType::EndElement)
				Read ();
		}
	}
}

void XmlNodeReader::Close() const {
	m_cur = nullptr;
	m_readState = ReadState::Closed;
}



} // Ext::
