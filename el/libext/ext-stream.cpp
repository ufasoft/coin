/*######     Copyright (c) 1997-2013 Ufasoft  http://ufasoft.com  mailto:support@ufasoft.com,  Sergey Pavlov  mailto:dev@ufasoft.com #######################################
#                                                                                                                                                                          #
# This program is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation;  #
# either version 3, or (at your option) any later version. This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the      #
# implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details. You should have received a copy of the GNU #
# General Public License along with this program; If not, see <http://www.gnu.org/licenses/>                                                                               #
##########################################################################################################################################################################*/

#include <el/ext.h>



namespace Ext {
using namespace std;

Stream::Stream() {
}

Stream::~Stream() {
}

bool Stream::Eof() const {
	Throw(E_NOTIMPL);
}

int Stream::ReadByte() const {
	if (Eof())
		return -1;
	byte b;
	ReadBuffer(&b, 1);
	return b;
}

void Stream::CopyTo(Stream& dest, size_t bufsize) const {
	vector<byte> buf(bufsize);
	while (true) {
		int i = 0;
		for (int b; i<buf.size() && (b = ReadByte()) != -1; ++i)
			buf[i] = (byte)b;
		if (!i)
			break;
		dest.WriteBuffer(&buf[0], i);
	}
}

void MemoryStream::WriteBuffer(const void *buf, size_t count) {
	size_t size = m_blob.Size;
	if (size-m_pos < count) {
		m_blob.Size = (size_t)max(max(Int64(32), Int64(size*2)), Int64(m_pos+count));
	}
	memcpy(m_blob.data()+m_pos, buf, count);
	m_pos += count;
}

bool MemoryStream::Eof() const {
	Throw(E_NOTIMPL);
}

void MemoryStream::Reset(size_t len) {
	m_blob.Size = len;
	m_pos = 0;
}

Blob MemoryStream::get_Blob() {
	class Blob r = m_blob;
	r.Size = (size_t)m_pos;
	return r;
}

void CMemReadStream::ReadBufferAhead(void *buf, size_t count) const {
	if (count > m_mb.Size-m_pos)
		Throw(E_EXT_EndOfStream);
	if (buf)
		memcpy(buf, m_mb.P+m_pos, count);
}

void CMemReadStream::ReadBuffer(void *buf, size_t count) const {
	ReadBufferAhead(buf, count);
	m_pos += count;
}

int CMemReadStream::ReadByte() const {
	return m_pos < m_mb.Size ? m_mb.P[m_pos++] : -1;
}

void AFXAPI ReadOneLineFromStream(const Stream& stm, String& beg, Stream *pDupStream) {
	const int MAX_LINE = 4096;
	char vec[MAX_LINE];
	char *p = vec;
	for (int i=0; i<MAX_LINE; i++) {		//!!!
		char ch;
		stm.ReadBuffer(&ch, 1);
		if (pDupStream)
			pDupStream->WriteBuffer(&ch, 1);
		switch (ch) {
		case '\n':
			beg += String(vec, p-vec);
			return;
		default:
			*p++ = ch;
		case '\r':
			break;
		}
	}
	Throw(E_PROXY_VeryLongLine);
}

vector<String> AFXAPI ReadHttpHeader(const Stream& stm, Stream *pDupStream) {
	for (vector<String> vec; ;) {
		String line;
		ReadOneLineFromStream(stm, line, pDupStream);
		if (line.IsEmpty())
			return vec;
		vec.push_back(line);
	}
	/*!!!

	int i = beg.Length;
	char buf[256];
	if (i >= sizeof buf)
	Throw(E_PROXY_VeryLongLine);
	strcpy(buf, beg);
	bool b = false;
	for (; i<sizeof buf; i++)
	{
	stm.ReadBuffer(buf+i, 1);
	if (buf[i] == '\n')
	if (b)
	break;
	else
	b = true;
	if (buf[i] != '\n' && buf[i] != '\r')
	b = false;
	}
	beg = String(buf, i+1);*/
}




} // Ext::

