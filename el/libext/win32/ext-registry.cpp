/*######     Copyright (c) 1997-2013 Ufasoft  http://ufasoft.com  mailto:support@ufasoft.com,  Sergey Pavlov  mailto:dev@ufasoft.com #######################################
#                                                                                                                                                                          #
# This program is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation;  #
# either version 3, or (at your option) any later version. This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the      #
# implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details. You should have received a copy of the GNU #
# General Public License along with this program; If not, see <http://www.gnu.org/licenses/>                                                                               #
##########################################################################################################################################################################*/

#include <el/ext.h>

#if UCFG_WIN32_FULL
#	pragma comment(lib, "advapi32")
#endif

#if UCFG_WIN32
#	include <winreg.h>

#	include <el/libext/win32/ext-win.h>
#endif

namespace Ext {
using namespace std;

CRegistryValue::CRegistryValue(DWORD typ, BYTE *p, int len)
	:	m_type(typ)
	,	m_blob(p, len)
{
}

CRegistryValue::CRegistryValue(int v)
	:	m_type(REG_DWORD)
	,	m_blob(&v, sizeof(v))
{
}

CRegistryValue::CRegistryValue(UInt32 v)
	:	m_type(REG_DWORD)
	,	m_blob(&v, sizeof(v))
{
}

#if	UCFG_SEPARATE_LONG_TYPE
CRegistryValue::CRegistryValue(unsigned long v)
	:	m_type(REG_DWORD)
	,	m_blob(&v, sizeof(v))
{
}
#endif

CRegistryValue::CRegistryValue(UInt64 v)
	:	m_type(REG_QWORD)
	,	m_blob(&v, sizeof(v))
{
}

/*!!!
CRegistryValue::CRegistryValue(LPCSTR s)
:	m_type(REG_SZ)
{
if (!s)
s = "";
m_blob = Blob(s, strlen(s)+1); //!!!
}*/

CRegistryValue::CRegistryValue(RCString s, bool bExpand)
	:	m_type(bExpand ? REG_EXPAND_SZ : REG_SZ)
	,	m_blob((const TCHAR*)s, s != nullptr ? (s.Length+1)*sizeof(TCHAR) : 0)
{
}

CRegistryValue::CRegistryValue(const ConstBuf& mb)
	:	m_type(REG_BINARY)
	,	m_blob(mb)
{
}

void CRegistryValue::Init(const CStringVector& ar) {
	m_type = REG_MULTI_SZ;
	size_t size = 1,
		i;
	for (i=0; i<ar.size(); i++)
		size += ar[i].Length+1;
	m_blob.Size = size*sizeof(TCHAR);
	TCHAR *p = (TCHAR*)m_blob.data();
	for (i=0; i<ar.size(); i++) {
		_tcscpy(p, ar[i]);
		p += _tcslen(p)+1;
	}
	*p = 0;
}

String CRegistryValue::get_Name() const {
	return m_name;
}

CRegistryValue::operator UInt32() const {
	if (m_type == REG_SZ)
		return Convert::ToUInt32(operator String());
	if (m_type != REG_DWORD)
		Throw(E_EXT_REGISTRY);
	return *(DWORD*)m_blob.constData();
}

CRegistryValue::operator UInt64() const {
	if (m_type == REG_SZ)
		return Convert::ToUInt32(operator String());
	if (m_type == REG_DWORD)
		return *(DWORD*)m_blob.constData();
	if (m_type == REG_QWORD)
		return *(UInt64*)m_blob.constData();
	Throw(E_EXT_REGISTRY);
}

CRegistryValue::operator String() const {
	if (m_type != REG_SZ)
		Throw(E_EXT_REGISTRY);
	return (const TCHAR*)m_blob.constData();
}

CRegistryValue::operator Blob() const {
	if (m_type != REG_BINARY)
		Throw(E_EXT_REGISTRY);
	return m_blob;
}

CRegistryValue::operator CStringVector() const {
	if (m_type != REG_MULTI_SZ)
		Throw(E_EXT_REGISTRY);
	vector<String> vec;
	TCHAR *p = (TCHAR*)m_blob.constData();
	while (*p) {
		String s = p;
		vec.push_back(s);
		p += s.GetLength()+1;
	}
	return vec;
}

#if UCFG_WIN32

void AFXAPI RegCheck(LONG v, LONG allowableError) {
	if (v != ERROR_SUCCESS && v != allowableError)
		Throw(HRESULT_FROM_WIN32(v));
}

void RegistryKey::ReleaseHandle(HANDLE h) const {
	RegCheck(::RegCloseKey((HKEY)h));
}

CRegistryValue CRegistryValues::operator[](int i) {
	DWORD nameLen = m_key.GetMaxValueNameLen()+1,
		valueLen = m_key.GetMaxValueLen(),
		typ;
	TCHAR *name = (TCHAR*)alloca(sizeof(TCHAR)*nameLen);
	BYTE *value = (BYTE*)alloca(valueLen);
	RegCheck(::RegEnumValue(m_key, i, name, &nameLen, 0, &typ, value, &valueLen));
	CRegistryValue r = CRegistryValue(typ, value, valueLen);
	r.m_name = name;
	return r;
}

int CRegistryValues::get_Count() {
	return m_key.GetRegKeyInfo().Values;
}

void CRegistryValuesIterator::operator++(int) {
	m_i++;
}

bool CRegistryValuesIterator::operator!=(CRegistryValuesIterator& i) {
	return m_i != i.m_i;
}

CRegistryValue CRegistryValuesIterator::operator*() {
	return m_values[m_i];
}

void RegistryKey::DeleteSubKey(RCString subkey) {
	RegCheck(::RegDeleteKey(_self, subkey));
}

void RegistryKey::DeleteSubKeyTree(RCString subkey) {
	{
		RegistryKey key(_self, subkey);
		vector<String> subkeys = key.GetSubKeyNames();
		for (int i=0; i<subkeys.size(); i++)
			key.DeleteSubKeyTree(subkeys[i]);
	}
	DeleteSubKey(subkey);
}

bool RegistryKey::KeyExists(RCString subKey) {
	HKEY hkey;
	bool r = RegOpenKeyEx(_self, subKey, 0, AccessRights, &hkey) == ERROR_SUCCESS;
	if (r)
		RegCheck(::RegCloseKey(hkey));
	return r;
}

#if !UCFG_WCE
void RegistryKey::Load(RCString subKey, RCString fileName) {
	RegCheck(::RegLoadKey(_self, subKey, fileName));
}

void RegistryKey::Save(RCString fileName) {
	RegCheck(::RegSaveKey(_self, fileName, 0));
}

void RegistryKey::UnLoad(RCString subKey) {
	RegCheck(::RegUnLoadKey(_self, subKey));
}
#endif


RegistryKey Registry::ClassesRoot(HKEY_CLASSES_ROOT),
Registry::CurrentUser(HKEY_CURRENT_USER),
Registry::LocalMachine(HKEY_LOCAL_MACHINE);


void RegistryKey::SetValue(RCString name, DWORD value) {
	SetValue(name, CRegistryValue(value));
}

void RegistryKey::SetValue(RCString name, RCString value) {
	SetValue(name, CRegistryValue(value));
}

void RegistryKey::SetValue(RCString name, const CRegistryValue& rv) {
	RegCheck(RegSetValueEx(_self, String(name), 0, rv.m_type, rv.m_blob.constData(), (DWORD)rv.m_blob.Size));
}

void RegistryKey::GetSubKey(int idx, RegistryKey& sk) {
	TCHAR subKey[255];
	DWORD subKeyLen = _countof(subKey);
	RegCheck(::RegEnumKeyEx(_self, idx, subKey, &subKeyLen, 0, 0, 0, 0));
	sk.Open(_self, subKey);
}

CStringVector RegistryKey::GetSubKeyNames() {
	vector<String> ar;
	for (int i=0;; i++) {
		TCHAR subKey[256];
		DWORD subKeyLen = _countof(subKey);
		LONG rr = ::RegEnumKeyEx(_self, i, subKey, &subKeyLen, 0, 0, 0, 0);
		switch (rr)
		{
		case ERROR_NO_MORE_ITEMS:
			return ar;
		default:
			RegCheck(rr);
		}
		ar.push_back(subKey);
	}
}

bool RegistryKey::ValueExists(RCString name) {
	int r = ::RegQueryValueEx(_self, name, 0, 0, 0, 0);
	if (r == ERROR_FILE_NOT_FOUND)
		return false;
	RegCheck(r);
	return true;
}


#endif

RegistryKey::RegistryKey(HKEY key, RCString subKey, bool create)
	:	m_parent(key)
	,	m_subKey(subKey)
	,	m_create(create)
	,	AccessRights(MAXIMUM_ALLOWED)			//!!! KEY_READ|KEY_WRITE|KEY_CREATE_SUB_KEY
{
}

RegistryKey::~RegistryKey() {
	Close();
}

void RegistryKey::Create() const {
	if (!Valid()) {
		HKEY hKey;
#if UCFG_WIN32
		DWORD dw;
		RegCheck(::RegCreateKeyEx(m_parent, m_subKey, 0, 0, REG_OPTION_NON_VOLATILE, AccessRights, 0, (HKEY *)&hKey, &dw));
#else
		OBJECT_ATTRIBUTES oa;
		InitializeObjectAttributes(&oa, m_subKey, OBJ_KERNEL_HANDLE, m_parent, nullptr);
		ULONG disposition;
		NtCheck(::ZwCreateKey(&hKey, AccessRights, &oa, 0, 0, REG_OPTION_NON_VOLATILE, &disposition));
#endif
		const_cast<RegistryKey*>(this)->Attach(hKey);
	}
}

void RegistryKey::Open(bool create) {
	if (create || m_create)
		Create();
	if (!Valid()) {
		HKEY hKey;
#if UCFG_WIN32
		RegCheck(RegOpenKeyEx(m_parent, m_subKey, 0, AccessRights/*!!!KEY_READ|KEY_WRITE|KEY_CREATE_SUB_KEY */, &hKey));
#else
		OBJECT_ATTRIBUTES oa;
		InitializeObjectAttributes(&oa, m_subKey, OBJ_KERNEL_HANDLE, m_parent, nullptr);
		NtCheck(::ZwOpenKey(&hKey, AccessRights, &oa));
#endif
		Attach(hKey);
	}
}

void RegistryKey::Open(HKEY key, RCString subKey, bool create) {
	m_create = create;
	Close();
	m_parent = key;
	m_subKey = subKey;
	Open();
}

void RegistryKey::Flush() {
	if (Valid()) {
#if UCFG_WIN32
		RegCheck(::RegFlushKey(_self));
#else
		NtCheck(::ZwFlushKey(_self));
#endif
	}
}

void RegistryKey::DeleteValue(RCString name, bool throwOnMissingValue) {
#if UCFG_WIN32
	RegCheck(::RegDeleteValue(_self, name), throwOnMissingValue ? ERROR_FILE_NOT_FOUND : ERROR_SUCCESS);
#else
	NtCheck(::ZwDeleteValueKey(_self, name), throwOnMissingValue ? STATUS_OBJECT_NAME_NOT_FOUND  : STATUS_SUCCESS);
#endif
}

RegistryKey::CRegKeyInfo RegistryKey::GetRegKeyInfo() {
	CRegKeyInfo rki;

#if UCFG_WIN32
	FILETIME ft;
	RegCheck(::RegQueryInfoKey(_self, 0, 0, 0,
		&rki.SubKeys,
		&rki.MaxSubKeyLen,
		&rki.MaxClassLen,
		&rki.Values,
		&rki.MaxValueNameLen,
		&rki.MaxValueLen,
		&rki.SecurityDescriptor,
		&ft));
	rki.LastWriteTime = ft;
#else
	KEY_FULL_INFORMATION kfi;
	ULONG rLen;
	NtCheck(::ZwQueryKey(_self, KeyFullInformation, &kfi, sizeof kfi, &rLen));
	rki.SubKeys			= kfi.SubKeys;
	rki.MaxSubKeyLen	= kfi.MaxNameLen;
	rki.MaxClassLen		= kfi.MaxClassLen;
	rki.Values			= kfi.Values;
	rki.MaxValueNameLen = kfi.MaxValueNameLen;
	rki.MaxValueLen		 = kfi.MaxValueDataLen;
	rki.LastWriteTime	= DateTime(kfi.LastWriteTime.QuadPart);
#endif
	return rki;
}

int RegistryKey::GetMaxValueNameLen() {
	return GetRegKeyInfo().MaxValueNameLen;
}

int RegistryKey::GetMaxValueLen() {
	return GetRegKeyInfo().MaxValueLen;
}

CRegistryValue RegistryKey::QueryValue(RCString name) {
	Open();
	DWORD valueLen = GetMaxValueLen();
#if !UCFG_WIN32
	valueLen += sizeof(KEY_VALUE_PARTIAL_INFORMATION);
#endif
	BYTE *p = (BYTE*)alloca(valueLen);
#if UCFG_WIN32
	DWORD typ;
	RegCheck(::RegQueryValueEx(_self, name, 0, &typ, p, &valueLen));
	CRegistryValue r(typ, p, valueLen);
#else
	ULONG rLen;
	NtCheck(::ZwQueryValueKey(_self, name,  KeyValuePartialInformation, p, valueLen, &rLen));
	KEY_VALUE_PARTIAL_INFORMATION *pi = (KEY_VALUE_PARTIAL_INFORMATION*)p;
	CRegistryValue r(pi->Type, pi->Data, pi->DataLength);
#endif
	r.m_name = name;
	return r;
}

CRegistryValue RegistryKey::TryQueryValue(RCString name, const CRegistryValue& def) {
	Open();
	DWORD valueLen = GetMaxValueLen();
#if !UCFG_WIN32
	valueLen += sizeof(KEY_VALUE_PARTIAL_INFORMATION);
#endif
	BYTE *p = (BYTE*)alloca(valueLen);
#if UCFG_WIN32
	DWORD typ;
	LONG rc = ::RegQueryValueEx(_self, name, 0, &typ, p, &valueLen);
	if (rc == ERROR_SUCCESS) {
		CRegistryValue r(typ, p, valueLen);
		r.m_name = name;
		return r;
	} else {
		if (rc != ERROR_FILE_NOT_FOUND)
			Throw(HRESULT_FROM_WIN32(rc));
	}
#else
	ULONG rLen;
	if (NT_SUCCESS(::ZwQueryValueKey(_self, name,  KeyValuePartialInformation, p, valueLen, &rLen))) {
		KEY_VALUE_PARTIAL_INFORMATION *pi = (KEY_VALUE_PARTIAL_INFORMATION*)p;
		CRegistryValue r(pi->Type, pi->Data, pi->DataLength);
		r.m_name = name;
		return r;
	}
#endif
	return def;
}

CRegistryValue RegistryKey::TryQueryValue(RCString name, DWORD def) {
	return TryQueryValue(name, CRegistryValue(def));
}

CRegistryValue RegistryKey::TryQueryValue(RCString name, RCString def) {
	return TryQueryValue(name, CRegistryValue(def));
}

String RegistryKey::get_Name() {
	return m_subKey;
}

RegistryKey::operator HKEY() const {
	Create();
	return (HKEY)DangerousGetHandleEx();
}

#if UCFG_WIN32

typedef LONG (APIENTRY *PFN_RegDisableReflectionKey)(HKEY hBase);
typedef LONG (APIENTRY *PFN_RegEnableReflectionKey)(HKEY hBase);
typedef LONG (APIENTRY *PFN_RegQueryReflectionKey)(HKEY hBase, BOOL *bIsReflectionDisabled);

bool RegistryKey::get_Reflected() {
	BOOL bIsDisabled = TRUE;
	DlProcWrap<PFN_RegQueryReflectionKey> pfn("KERNEL32.DLL", "RegQueryReflectionKey");
	if (pfn)
		RegCheck(pfn(_self, &bIsDisabled));
	return !bIsDisabled;
}

void RegistryKey::put_Reflected(bool v) {
	DlProcWrap<PFN_RegEnableReflectionKey> pfn("KERNEL32.DLL", v ? "RegEnableReflectionKey" : "RegDisableReflectionKey");
	if (pfn)
		RegCheck(pfn(_self));
}
#endif

} // Ext::

