/*######     Copyright (c) 1997-2013 Ufasoft  http://ufasoft.com  mailto:support@ufasoft.com,  Sergey Pavlov  mailto:dev@ufasoft.com #######################################
#                                                                                                                                                                          #
# This program is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation;  #
# either version 3, or (at your option) any later version. This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the      #
# implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details. You should have received a copy of the GNU #
# General Public License along with this program; If not, see <http://www.gnu.org/licenses/>                                                                               #
##########################################################################################################################################################################*/

#include <el/ext.h>

#include <el/xml.h>

#if UCFG_USE_LIBXML
#	include <libxml/xmlreader.h>
#endif


namespace Ext {

#if UCFG_USE_LIBXML

static void XMLCDECL EXT_xmlGenericErrorFunc(void *ctx, const char *msg, ...) {
}

static xmlGenericErrorFunc s_pfnEXT_xmlGenericErrorFunc = &EXT_xmlGenericErrorFunc;
static bool s_initGenericErrorFunc = (initGenericErrorDefaultFunc(&s_pfnEXT_xmlGenericErrorFunc), true);

#ifdef _DEBUG
	static bool s_initDefaultGenericErrorFunc = (initGenericErrorDefaultFunc(0), true);
#endif

int XmlCheck(int r) {
	if (r == -1) {
		if (xmlError *err = xmlGetLastError())
			throw LibxmlXmlException(err);
		else
			throw XmlException("Unknown XML error");
	}
	return r;
}

template <typename T>
T *XmlCheck(T *p) {
	if (!p)
		XmlCheck(-1);
	return p;
}

LibxmlXmlException::LibxmlXmlException(xmlError *err)
	:	base(MAKE_HRESULT(SEVERITY_ERROR, FACILITY_LIBXML, err->code), err->message)
{
	ZeroStruct(m_xmlError);
	XmlCheck(xmlCopyError(err, &m_xmlError));
}

LibxmlXmlException::LibxmlXmlException(const LibxmlXmlException& e) {
	ZeroStruct(m_xmlError);
	XmlCheck(xmlCopyError(const_cast<xmlError*>(&e.m_xmlError), &m_xmlError));
}

String LibxmlXmlException::get_Message() const {
	String r;
	if (!get_Reason().IsEmpty())
		r += get_Reason();
	if (!get_Url().IsEmpty())
		r +=" at "+get_Url()+" ";
	r += EXT_STR("(line: " << LineNumber << ", pos: " << LinePosition << ")");
	return r;
}

static int __cdecl xmlInputReadCallback	(void * context, char * buffer, int len) {
	istream& is = *(istream*)context;
	if (is) {
		is.read(buffer, len);
		return (int)is.gcount();
	} else
		return 0;
}

static int __cdecl xmlInputCloseCallback(void * context) {
	return 0;
}

static inline xmlTextReaderPtr ToReader(void *p) {
	return static_cast<xmlTextReaderPtr>(p);
}

XmlTextReader XmlTextReader::CreateFromString(RCString s) {
	XmlTextReader r;
	r.m_str = s;
	r.m_p = XmlCheck(xmlReaderForDoc(BAD_CAST r.m_str.c_str(), 0, 0, XML_PARSE_NOBLANKS));
	return r;
}

void XmlTextReader::Close() const {
	if (m_p)
		XmlCheck(xmlTextReaderClose(ToReader(exchange(m_p, nullptr))));	
//!!!R	ReadState = Ext::ReadState::Closed;
	
}

int XmlTextReader::get_AttributeCount() const {
	return XmlCheck(xmlTextReaderAttributeCount(ToReader(m_p)));
}

String XmlTextReader::get_Name() const {
	XmlString xs(xmlTextReaderName(ToReader(m_p)));
	if (!xs.m_p)
		throw XmlException( "can not get Name");
	return xs;
}

XmlNodeType XmlTextReader::get_NodeType() const {
	return (XmlNodeType)XmlCheck(xmlTextReaderNodeType(ToReader(m_p)));
}

bool XmlTextReader::get_HasValue() const {
	return !!XmlString(xmlTextReaderValue(ToReader(m_p)));
}

String XmlTextReader::get_Value() const {
	XmlString xs(xmlTextReaderValue(ToReader(m_p)));
	if (!xs.m_p)
		throw XmlException( "can not get Value");
	return xs;
}

int XmlTextReader::get_Depth() const {
	return XmlCheck(xmlTextReaderDepth(ToReader(m_p)));
}

ReadState XmlTextReader::get_ReadState() const {
	if (m_p)
		return (Ext::ReadState)XmlCheck(xmlTextReaderReadState(ToReader(m_p)));
	return Ext::ReadState::Closed;	
}

String XmlTextReader::GetAttribute(int idx) const {
	return XmlString(xmlTextReaderGetAttributeNo(ToReader(m_p), idx));
}

String XmlTextReader::GetAttribute(const CStrPtr& name) const {
	return XmlString(xmlTextReaderGetAttribute(ToReader(m_p), BAD_CAST name.c_str()));
}

bool XmlTextReader::IsEmptyElement() const {
	return XmlCheck(xmlTextReaderIsEmptyElement(ToReader(m_p)));
}

int XmlTextReader::LineNumber() const {
	return xmlTextReaderGetParserLineNumber(ToReader(m_p));
}

int XmlTextReader::LinePosition() const {
	return xmlTextReaderGetParserColumnNumber(ToReader(m_p));
}

XmlTextReader::XmlTextReader(RCString uri) {
	Init(xmlReaderForFile(uri.c_str(), 0, XML_PARSE_NOBLANKS));
}

XmlTextReader::XmlTextReader(istream& is) {
	Init(xmlReaderForIO(xmlInputReadCallback, xmlInputCloseCallback, &is, 0, 0, XML_PARSE_NOBLANKS));
}

XmlTextReader::~XmlTextReader() {
	Close();
}

bool XmlTextReader::MoveToElement() const {
	return XmlCheck(xmlTextReaderMoveToElement(ToReader(m_p)));
}

bool XmlTextReader::MoveToFirstAttribute() const {
	return XmlCheck(xmlTextReaderMoveToFirstAttribute(ToReader(m_p)));
}

bool XmlTextReader::MoveToNextAttribute() const {
	return XmlCheck(xmlTextReaderMoveToNextAttribute(ToReader(m_p)));
}

bool XmlTextReader::Read() const {
	if (Ext::ReadState::Closed == get_ReadState())
		return false;
//!!!R	ReadState = ReadState::Interactive;
	return XmlCheck(xmlTextReaderRead(ToReader(m_p)));
}

void XmlTextReader::Skip() const {
	XmlCheck(xmlTextReaderNext(ToReader(m_p)));
/*!!!
	int depth = Depth();
	while (Read() && Depth()>depth)
		;
	if (Depth() > depth)
		throw runtime_error("invalid XML");*/
}


XmlIterator::XmlIterator(XmlReader& reader, RCString name)
	:	m_reader(reader)
	,	m_name(name)
{
	operator++();
}

XmlIterator::operator bool() {
	return m_bState;
}

XmlIterator& XmlIterator::operator ++() {
/*!!!

	int ret = xmlTextReaderRead(m_p);
	if (!ret)
		Throw(E_FAIL);
	Xml::XmlCharPtr cur_node_name( xmlTextReaderName(m_p) );	 
	if ( !cur_node_name.get() )
		throw runtime_error("invalid node name retreived");
	string s( (char*)cur_node_name.get() );
	const int nodetype = xmlTextReaderNodeType(m_p);
	if ( s == m_name && nodetype == 15 )
		m_p = 0;
*/
	return (*this);
}

#if 0
xmlNodePtr GetXmlNodePtrFromString( const string& xml_string ) {	
	xmlNodePtr root_node = 0;
	Xml::DocPtr doc_( xmlParseDoc( BAD_CAST xml_string.c_str() ) );

	if ( doc_ ) {
		root_node = xmlDocGetRootElement( doc_ );
    }
	
	if ( root_node )
	{
		return xmlCopyNode( root_node, 1 );
    }

	return 0;
}

/*!!!R
void dumpXmlToString( xmlDocPtr doc, string& st ) {
	Xml::OutputBufferPtr writer( xmlAllocOutputBuffer( 0 ) );
	writer->context = (void*)(&st);
	writer->writecallback = string_writer;
	xmlSaveFormatFileTo( writer, doc, "utf-8", 1 );
	writer.release();
}*/

xmlNodePtr createRootNode( xmlDocPtr doc, const char* name, const char* data ) {
	Xml::NodePtr root( xmlNewDocNode( doc, 0, BAD_CAST name, BAD_CAST data ) );
	xmlDocSetRootElement( doc, root );

	return root.release();
}

void validateXmlString(const string& xml_string) {
    xmlDocPtr document_ptr = xmlParseMemory(xml_string.c_str(), xml_string.size());

    if(document_ptr == 0) {
		xmlErrorPtr error_ptr = xmlGetLastError();
		if(error_ptr) {
			stringstream formatter;
			formatter << "malformed xml: " << error_ptr->message
					  << " at line " << error_ptr->line;

			xmlFreeDoc(document_ptr);

			throw runtime_error(formatter.str());
		}
    }

	xmlFreeDoc(document_ptr);
}
#endif

const xmlChar* ToXmlChar(RCString s) {
	return (const xmlChar*)s.c_str();
}

XPathDocument::XPathDocument(RCString filename) {
	m_doc = XmlCheck(xmlParseFile(filename));
}

XPathDocument::XPathDocument(istream& is) {
	vector<char> v;
	for (int ch; (ch=is.get())!=EOF;)
		v.push_back((char)ch);
	m_doc = XmlCheck(xmlParseMemory(&v[0], v.size()));
}

ptr<XPathNavigator> XPathDocument::CreateNavigator() {
	ptr<XPathNavigator> r = new XPathNavigator;
	r->m_ctx = xmlXPathNewContext(m_doc);
	return r;
}

ptr<XPathNodeIterator> XPathNavigator::Select(RCString xpath) {
	if (!m_ctx) {
		m_ctx = xmlXPathNewContext(m_node->doc);
		m_ctx->node = m_node;
	}
	ptr<XPathNodeIterator> r = new XPathNodeIterator;
	r->m_p = xmlXPathEvalExpression(ToXmlChar(xpath), m_ctx);
	return r;
}

ptr<XPathNavigator> XPathNavigator::SelectSingleNode(RCString xpath) {
	ptr<XPathNodeIterator> r = Select(xpath);
	if (r->MoveNext())
		return r->Current;
	else
		return nullptr;
}

String XPathNavigator::GetAttribute(RCString name) {
	return XmlString(xmlGetProp(m_node, ToXmlChar(name)), String());
}

String XPathNavigator::get_Value() {
	return XmlString(xmlNodeListGetString(m_node->doc, m_node->children, 1), String());
}

bool XPathNodeIterator::MoveNext() {
	if (!m_p || xmlXPathNodeSetIsEmpty(m_p->nodesetval) || m_i>=m_p->nodesetval->nodeNr-1)
		return false;
	++m_i;
	return true;
}

ptr<XPathNavigator> XPathNodeIterator::get_Current() {
	ptr<XPathNavigator> r = new XPathNavigator;
	r->m_node = m_p->nodesetval->nodeTab[m_i];
	return r;
}

#endif // UCFG_USE_LIBEXML




} // Ext::


