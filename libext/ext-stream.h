/*######     Copyright (c) 1997-2013 Ufasoft  http://ufasoft.com  mailto:support@ufasoft.com,  Sergey Pavlov  mailto:dev@ufasoft.com #######################################
#                                                                                                                                                                          #
# This program is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation;  #
# either version 3, or (at your option) any later version. This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the      #
# implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details. You should have received a copy of the GNU #
# General Public License along with this program; If not, see <http://www.gnu.org/licenses/>                                                                               #
##########################################################################################################################################################################*/

#pragma once

namespace Ext {

ENUM_CLASS(SeekOrigin) {
	Begin,
	Current,
	End
} END_ENUM_CLASS(SeekOrigin);


class EXTCLASS Stream {
public:
	typedef Stream class_type;

	virtual ~Stream();
	virtual void WriteBuffer(const void *buf, size_t count) { Throw(E_NOTIMPL); }

	void WriteBuf(const ConstBuf& mb) { WriteBuffer(mb.P, mb.Size); }

	virtual bool Eof() const;
	virtual size_t Read(void *buf, size_t count) const { Throw(E_NOTIMPL); }
	virtual void ReadBuffer(void *buf, size_t count) const;
	virtual int ReadByte() const;
	virtual Int64 Seek(Int64 offset, SeekOrigin origin) const { Throw(E_NOTIMPL); }		// mandatory to implement where put_Position implemented
	
	virtual void ReadBufferAhead(void *buf, size_t count) const { Throw(E_NOTIMPL); }
	virtual void Close() const {}
	virtual void Flush() {}

	virtual UInt64 get_Length() const { 
		UInt64 curPos = Position;
		UInt64 endPos = Seek(0, SeekOrigin::End);
		put_Position(curPos);
		return endPos;
	}
	DEFPROP_VIRTUAL_GET_CONST(UInt64, Length);

	virtual UInt64 get_Position() const {
		return Seek(0, SeekOrigin::Current);		
	}
	virtual void put_Position(UInt64 pos) const {
		Seek(pos, SeekOrigin::Begin);
	}
	DEFPROP_VIRTUAL_CONST_CONST(UInt64, Position);	

	void CopyTo(Stream& dest, size_t bufsize = 4096) const;

protected:
	Stream();
private:

};

ENUM_CLASS(CompressionMode) {
	Decompress, Compress
} END_ENUM_CLASS(CompressionMode);

class CompressStream : public Stream {
	typedef CompressStream class_type;
public:
	CompressionMode Mode;

	EXT_API CompressStream(Stream& stm, CompressionMode mode);
	~CompressStream();

	size_t Read(void *buf, size_t count) const override;
	void WriteBuffer(const void *buf, size_t count) override;
	bool Eof() const override;
	void SetByByteMode(bool v);
protected:
	mutable CBool m_bInited, m_bByByteMode;
	void *m_pimpl;
	mutable int m_bufpos;
	mutable std::vector<byte> m_sbuf, m_dbuf;
	Stream& m_stmBase;

	virtual void InitImp() const;
private:
	void ReadPortion() const;
	void WritePortion(const void *buf, size_t count, int flush);
};

class GZipStream : public CompressStream {
	typedef CompressStream base;
public:
	GZipStream(Stream& stm, CompressionMode mode)
		:	base(stm, mode)
	{
	}
protected:
	void InitImp() const;
};



} // Ext::


