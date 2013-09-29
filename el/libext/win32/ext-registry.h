/*######     Copyright (c) 1997-2013 Ufasoft  http://ufasoft.com  mailto:support@ufasoft.com,  Sergey Pavlov  mailto:dev@ufasoft.com #######################################
#                                                                                                                                                                          #
# This program is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation;  #
# either version 3, or (at your option) any later version. This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the      #
# implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details. You should have received a copy of the GNU #
# General Public License along with this program; If not, see <http://www.gnu.org/licenses/>                                                                               #
##########################################################################################################################################################################*/

#pragma once

namespace Ext {

#if !UCFG_WIN32
	typedef HANDLE HKEY;
#endif

class CRegistryValuesIterator;

class AFX_CLASS CRegistryValue {
public:
	typedef CRegistryValue class_type;

	Blob m_blob;
	String m_name;
	DWORD m_type;

	CRegistryValue(DWORD typ, BYTE *p, int len);
	CRegistryValue(int v);
	CRegistryValue(UInt32 v);
	CRegistryValue(UInt64 v);
	//!!!	CRegistryValue(LPCSTR s);
	CRegistryValue(RCString s, bool bExpand = false);
	CRegistryValue(const ConstBuf& mb);
	CRegistryValue(const CStringVector& ar) { Init(ar); }

	String get_Name() const;
	DEFPROP_GET(String, Name);

	operator UInt32() const;

#if	UCFG_SEPARATE_LONG_TYPE
	CRegistryValue(unsigned long v);
	operator unsigned long() const { return operator UInt32(); }
#endif

	operator UInt64() const;
	operator String() const;
	operator Blob() const;
	operator bool() const { return operator UInt64(); }
	EXT_API operator CStringVector() const;

	CStringVector ToStrings() const { return operator CStringVector(); }
	//!!!  VARIANT GetAsSafeArray();
private:
	EXT_API void Init(const CStringVector& ar);

	friend class RegistryKey;
	friend class CRegistryValues;
};

class AFX_CLASS CRegistryValues {
	RegistryKey& m_key;
public:
	typedef CRegistryValues class_type;
	CRegistryValues(RegistryKey& key)
		:	m_key(key)
	{}
	CRegistryValuesIterator begin();
	CRegistryValuesIterator end();
	CRegistryValue operator[](int i);
	CRegistryValues operator[](RCString name);

	int get_Count();
	DEFPROP_GET(int, Count);
};

class AFX_CLASS CRegistryValuesIterator {
	int m_i;
	CRegistryValues& m_values;
public:
	CRegistryValuesIterator(CRegistryValues& values)
		:	m_i(0)
		,	m_values(values)
	{}

	CRegistryValuesIterator(const CRegistryValuesIterator& i)
		:	m_i(i.m_i)
		,	m_values(i.m_values)
	{}

	void operator++(int);
	bool operator!=(CRegistryValuesIterator& i);
	CRegistryValue operator*();
};

class AFX_CLASS RegistryKey : public SafeHandle {
	typedef RegistryKey class_type;
public:
	struct CRegKeyInfo {
		DWORD SubKeys,
			MaxSubKeyLen,
			MaxClassLen,
			Values,
			MaxValueNameLen,
			MaxValueLen,
			SecurityDescriptor;
		DateTime LastWriteTime;

		CRegKeyInfo() {
			ZeroStruct(*this);
		}
	};

	String m_subKey;
	ACCESS_MASK AccessRights;

	RegistryKey()
		:	AccessRights(MAXIMUM_ALLOWED)
	{
	}

	RegistryKey(HKEY key)
		:	SafeHandle(key)
		,	AccessRights(MAXIMUM_ALLOWED)
	{
	}

	RegistryKey(HKEY key, RCString subKey, bool create = true);
	~RegistryKey();
	void Create() const;
	void Open(bool create = false);
	void Open(HKEY key, RCString subKey, bool create = true);
	void Load(RCString subKey, RCString fileName);
	void Save(RCString fileName);
	void UnLoad(RCString subKey);
	void Flush();
	void DeleteSubKey(RCString subkey);
	void DeleteSubKeyTree(RCString subkey);
	void DeleteValue(RCString name, bool throwOnMissingValue = true);
	bool ValueExists(RCString name);
	CRegistryValue QueryValue(RCString name = nullptr);
	CRegistryValue TryQueryValue(RCString name, DWORD def);
	CRegistryValue TryQueryValue(RCString name, RCString def);
	CRegistryValue TryQueryValue(RCString name, const CRegistryValue& def);
	CRegKeyInfo GetRegKeyInfo();
	void SetValue(RCString name, DWORD value);
	void SetValue(RCString name, RCString value);
	void SetValue(RCString name, const CRegistryValue& rv);
	int GetMaxValueNameLen();
	int GetMaxValueLen();
	void GetSubKey(int idx, RegistryKey& sk);
	EXT_API CStringVector GetSubKeyNames();

	String get_Name();
	DEFPROP_GET(String, Name);

	bool get_Reflected();
	void put_Reflected(bool v);
	DEFPROP(bool, Reflected);

	operator HKEY() const;
	bool KeyExists(RCString subKey);
protected:
#if UCFG_WIN32
	void ReleaseHandle(HANDLE h) const;
#endif
private:
	HKEY m_parent;
	bool m_create;
};

class Registry {
public:
	EXT_DATA static RegistryKey ClassesRoot,
		CurrentUser,
		LocalMachine;
};

class Wow64RegistryReflectionKeeper {
public:
	RegistryKey& m_key;
	CBool m_bChanged, m_bPrev;

	Wow64RegistryReflectionKeeper(RegistryKey& key)
		:	m_key(key)
	{
	}

	~Wow64RegistryReflectionKeeper() {
		if (m_bChanged)
			m_key.Reflected = m_bPrev;
	}

	void Change(bool v) {
		m_bChanged = true;
		m_bPrev = m_key.Reflected;
		m_key.Reflected = v;
	}
};


}	// Ext::
