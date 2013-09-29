/*######     Copyright (c) 1997-2013 Ufasoft  http://ufasoft.com  mailto:support@ufasoft.com,  Sergey Pavlov  mailto:dev@ufasoft.com #######################################
#                                                                                                                                                                          #
# This program is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation;  #
# either version 3, or (at your option) any later version. This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the      #
# implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details. You should have received a copy of the GNU #
# General Public License along with this program; If not, see <http://www.gnu.org/licenses/>                                                                               #
##########################################################################################################################################################################*/

#include <el/ext.h>

#include "ext-http.h"


namespace Ext {

int AFXAPI InetCheck(int i) {
	if (!i) {
		DWORD dw = GetLastError();
		if (dw == ERROR_INTERNET_EXTENDED_ERROR) {
			TCHAR buf[256];
			DWORD code,
				len = _countof(buf);
			Win32Check(::InternetGetLastResponseInfo(&code, buf, &len));
			code = code;
			//!!!! todo
		}
		Throw(dw ? HRESULT_FROM_WIN32(dw) : E_EXT_UnknownWin32Error);
	}
	return i;
}

#if !UCFG_USE_LIBCURL

void CInternetFile::Attach(HINTERNET hInternet) {
	m_hInternet = hInternet;
}

CInternetFile::~CInternetFile() {
}

UInt32 CInternetFile::Read(void *lpBuf, UInt32 nCount) {
	DWORD dw;
	Win32Check(InternetReadFile(m_hInternet, lpBuf, nCount, &dw));
	return dw;
}

const int INTERNET_BUF_SIZE = 4096;

String CInternetFile::ReadString() {
	char buf[INTERNET_BUF_SIZE];
	DWORD dw;
	Win32Check(::InternetReadFile(m_hInternet, buf, sizeof(buf), &dw));
	return buf;
}

DWORD CInternetFile::SetFilePointer(LONG offset, DWORD method) {
	return InternetSetFilePointer(m_hInternet, offset, 0, method, 0);
}

CInternetSession::CInternetSession(RCString agent, DWORD dwContext, DWORD dwAccessType, RCString pstrProxyName, RCString pstrProxyBypass, DWORD dwFlags) {
	Init(agent, dwContext, dwAccessType, pstrProxyName, pstrProxyBypass, dwFlags);
}

void CInternetSession::Init(RCString agent, DWORD dwContext, DWORD dwAccessType, RCString pstrProxyName, RCString pstrProxyBypass, DWORD dwFlags) {
	Attach(::InternetOpen(agent, dwAccessType, pstrProxyName, pstrProxyBypass, dwFlags));
//!!!	Win32Check(m_hSession != 0);
}

CInternetSession::~CInternetSession() {
	try {
		Close();
	} catch (RCExc) {
	}
}

/*!!!
void CInternetSession::Close() {
	if (m_hSession)
		Win32Check(::InternetCloseHandle(exchangeZero(m_hSession)));
}*/

void CInternetSession::ReleaseHandle(HANDLE h) const {
	Win32Check(::InternetCloseHandle(h));
}

void CInternetSession::OpenUrl(CInternetConnection& conn, RCString url, LPCTSTR headers, DWORD headersLength, DWORD dwFlags, DWORD_PTR ctx) {
	conn.Attach(::InternetOpenUrl(BlockingHandleAccess(_self), url, headers, headersLength, dwFlags, ctx));
}

void CInternetSession::Connect(CInternetConnection& conn, RCString serverName, INTERNET_PORT port, RCString userName, RCString password, DWORD dwService, DWORD dwFlags, DWORD_PTR ctx) {
	conn.Attach(::InternetConnect(BlockingHandleAccess(_self), serverName, port, userName, password, dwService, dwFlags, ctx));
}

#endif // !UCFG_USE_LIBCURL


/*!!!R
int CInetStreambuf::overflow(int c) {
	return EOF;
}

int CInetStreambuf::uflow() {
	char c;
	int r = m_file.Read(&c, 1);
	return r > 0 ? c : EOF;
}

int CInetStreambuf::underflow() {
	int c = uflow();
	if (c != EOF)
		m_file.SetFilePointer(-1, FILE_CURRENT);
	return c;
}

void CInetIStream::open(CInternetSession& sess, RCString url) {
	sess.OpenURL(url, &((CInetStreambuf*)rdbuf())->m_file);
}*/

#if UCFG_WIN32
bool AFXAPI ConnectedToInternet() {
	DWORD dwState = 0,
		dwSize = sizeof(DWORD);
	Win32Check(::InternetQueryOption(0, INTERNET_OPTION_CONNECTED_STATE, &dwState, &dwSize));
	return !(dwState & INTERNET_STATE_DISCONNECTED_BY_USER);
}
#endif //UCFG_WIN32



} // Ext::

