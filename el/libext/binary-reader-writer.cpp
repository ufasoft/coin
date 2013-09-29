/*######     Copyright (c) 1997-2013 Ufasoft  http://ufasoft.com  mailto:support@ufasoft.com,  Sergey Pavlov  mailto:dev@ufasoft.com #######################################
#                                                                                                                                                                          #
# This program is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation;  #
# either version 3, or (at your option) any later version. This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the      #
# implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details. You should have received a copy of the GNU #
# General Public License along with this program; If not, see <http://www.gnu.org/licenses/>                                                                               #
##########################################################################################################################################################################*/

#include <el/ext.h>

#if UCFG_WIN32
#	include <oleauto.h>

#	include <el/libext/win32/ext-win.h>
#	if UCFG_COM
#		include <el/libext/win32/ext-com.h>
#	endif
#endif

namespace Ext {
using namespace std;

#if UCFG_OLE
void BinaryWriter::WriteVariantOfType(const VARIANT& v, VARTYPE vt) {
	switch (vt) {
	case VT_EMPTY:
	case VT_NULL:
		break;
	case VT_UI1:
		_self << v.bVal;
		break;
	case VT_I2:
		_self << v.iVal;
		break;
	case VT_I4:
		_self << DWORD(v.lVal);
		break;
	case VT_R4:
		_self << v.fltVal;
		break;
	case VT_R8:
		_self << v.dblVal;
		break;
	case VT_CY:
		_self << v.cyVal;
		break;
	case VT_DATE:
		_self << v.date;
		break;
	case VT_BOOL:
		_self << bool(v.boolVal);
		break;
	case VT_BSTR:
		{
			DWORD len = SysStringByteLen(v.bstrVal);
			_self << len;
			Write(v.bstrVal, len);
		}
		break;
	case VT_VARIANT:
		_self << v;
		break;
	default:
		Throw(E_EXT_VartypeNotSupported);
	}
}

void BinaryWriter::Write(const CY& v) {
	Write(&v, sizeof v);
}

void BinaryWriter::Write(const VARIANT& v) {
	switch (v.vt)
	{
	case VT_EMPTY:
	case VT_NULL:
	case VT_UI1:
	case VT_I2:
	case VT_I4:
	case VT_R4:
	case VT_R8:
	case VT_CY:
	case VT_BOOL:
	case VT_BSTR:
		_self << byte(v.vt);
		WriteVariantOfType(v, v.vt);
		break;
	default:
		if (VarIsArray(v)) {
			CSafeArray sa((SAFEARRAY*&)v.parray);
			_self << VT_ARRAY_EX;
			byte elType = byte(v.vt);
			_self << elType;
			byte dims = (byte)sa.DimCount;
			_self << dims;
			switch (dims)
			{
			case 1:
				{ 
					LONG dim1 = sa.GetLBound(),
						dim2 = sa.GetUBound();
					_self << dim1 << dim2;
					for (int i=dim1; i<=dim2; i++)
						WriteVariantOfType(GetElement(v, i), elType);
					break;
				}
			case 2:
				{
					LONG dim1 = sa.GetLBound(),
						dim2 = sa.GetUBound(),
						dim3 = sa.GetLBound(2),
						dim4 = sa.GetUBound(2);
					_self << dim1 << dim2 << dim3 << dim4;
					for (int i=dim1; i<=dim2; i++)
						for (int j=dim3; j<=dim4; j++)
							WriteVariantOfType(GetElement(v, i, j), elType);
					break;
				}
			default:
				Throw(E_EXT_InvalidDimCount);
			}
		} else
			Throw(E_EXT_VartypeNotSupported);
	}
}
#endif	// UCFG_OLE

BinaryWriter& BinaryWriter::operator<<(const ConstBuf& mb) {
	WriteSize(mb.Size);
	BaseStream.WriteBuffer(mb.P, mb.Size);
	return _self;
}

const BinaryReader& BinaryReader::operator>>(Blob& blob) const {
	const size_t INITIAL_BUFSIZE = 512;

	size_t size = ReadSize();
	if (size <= INITIAL_BUFSIZE) {
		blob.Size = size;
		Read(blob.data(), size);
	} else {																			// to prevent OutOfMemory exception if just error in the Size field
		for (size_t curSize = INITIAL_BUFSIZE, offset=0; offset<size; offset=curSize) {
			blob.Size = curSize = min(size_t(curSize*2), size);
			Read(blob.data()+offset, curSize-offset);
		}
	}
	return _self;
}

void BinaryWriter::WriteString(RCString v) {
	_self << Encoding.GetBytes(v);
}

String BinaryReader::ReadString() const {
	Blob blob;
	operator>>(blob);
	return Encoding.GetChars(blob);
}

/*!!!
BinaryWriter& BinaryWriter::operator<<(const CStringVector& ar) {
	size_t count = ar.size();
	WriteSize(count);
	for (int i=0; i<count; i++)
		_self << ar[i];
	return _self;
}

const BinaryReader& BinaryReader::operator>>(CStringVector& ar) const {
	size_t count = ReadSize();
	ar.resize(count);
	for (int i=0; i<count; i++)
		_self >> ar[i];
	return _self;
}*/

Blob BinaryReader::ReadToEnd() const {
	MemoryStream ms;
	BaseStream.CopyTo(ms);
	return ms.Blob;
}

UInt64 AFXAPI Read7BitEncoded(const byte *&p) {
	UInt64 r = *p++;										// optimization for 1-byte records
	if (!((byte)r & 0x80))
		return r;
	r &= 0x7F;
	int shift = 7;
	do {
		byte b = *p++;
		r |= UInt64(b & 0x7F) << shift;
		if (!(b & 0x80))
			return r;
	} while ((shift+=7) < 64);
	Throw(E_FAIL);
}

void AFXAPI Write7BitEncoded(byte *&p, UInt64 v) {
	do {
		byte b = v & 0x7F;
		if (v >>= 7)
			b |= 0x80;
		*p++ = b;
	} while (v);
}

void BinaryWriter::Write7BitEncoded(UInt64 v) {
	byte buf[10], *p = buf;
	Ext::Write7BitEncoded(p, v);
	Write(buf, p-buf);
}

UInt64 BinaryReader::Read7BitEncoded() const {
	UInt64 r = 0;
	for (int shift=0; shift<64; shift+=7) {
		byte b = ReadByte();
		r |= UInt64(b & 0x7F) << shift;
		if (!(b & 0x80))
			return r;
	}
	Throw(E_FAIL);
}

} // Ext::

