/*######     Copyright (c) 1997-2013 Ufasoft  http://ufasoft.com  mailto:support@ufasoft.com,  Sergey Pavlov  mailto:dev@ufasoft.com #######################################
#                                                                                                                                                                          #
# This program is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation;  #
# either version 3, or (at your option) any later version. This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the      #
# implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details. You should have received a copy of the GNU #
# General Public License along with this program; If not, see <http://www.gnu.org/licenses/>                                                                               #
##########################################################################################################################################################################*/

#pragma once 

namespace Ext { namespace Crypto {

ENUM_CLASS(CngKeyBlobFormat) {
	OSslEccPrivateBlob,
	OSslEccPublicBlob,
	OSslEccPrivateBignum,
	OSslEccPublicCompressedBlob,
	OSslEccPublicUncompressedBlob,
	OSslEccPrivateCompressedBlob
} END_ENUM_CLASS(CngKeyBlobFormat);

class CngKey {
public:
	void *m_pimpl;

	CngKey()
		:	m_pimpl(0)
	{}

	CngKey(const CngKey& key);
	~CngKey();
	CngKey& operator=(const CngKey& key);
	Blob Export(CngKeyBlobFormat format) const;
	static CngKey AFXAPI Import(const ConstBuf& mb, CngKeyBlobFormat format);
protected:
	CngKey(void *pimpl)
		:	m_pimpl(pimpl)
	{}

	friend class Dsa;
	friend class ECDsa;
};

class Dsa : public Object {
public:
	CngKey Key;

	virtual Blob SignHash(const ConstBuf& hash) =0;
	virtual bool VerifyHash(const ConstBuf& hash, const ConstBuf& signature) =0;
protected:
	Dsa(void *keyImpl)
		:	Key(keyImpl)
	{}

	Dsa(const CngKey& key)
		:	Key(key)
	{}
};

class ECDsa : public Dsa {
	typedef Dsa base;
public:
	ECDsa(int keySize = 521);

	ECDsa(const CngKey& key)
		:	base(key)
	{
	}

	Blob SignHash(const ConstBuf& hash) override;
	bool VerifyHash(const ConstBuf& hash, const ConstBuf& signature) override;
};

ptr<Dsa> CreateECDsa();


}} // Ext::Crypto::


