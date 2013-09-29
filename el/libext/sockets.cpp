/*######     Copyright (c) 1997-2013 Ufasoft  http://ufasoft.com  mailto:support@ufasoft.com,  Sergey Pavlov  mailto:dev@ufasoft.com #######################################
#                                                                                                                                                                          #
# This program is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation;  #
# either version 3, or (at your option) any later version. This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the      #
# implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details. You should have received a copy of the GNU #
# General Public License along with this program; If not, see <http://www.gnu.org/licenses/>                                                                               #
##########################################################################################################################################################################*/

#include <el/ext.h>


#include <el/libext/ext-net.h>

#if UCFG_WCE
#	pragma comment(lib, "ws2")
#endif

using namespace std;

#if UCFG_WIN32
#	define WSA(e) WSA##e
#else
#	define WSA(e) e

int WSAGetLastError() {
	return errno;
}
#endif


namespace Ext {


Socket::COSSupportsIPver::operator bool() {
	if (-1 == m_supports) {
		try {
			Socket sock;
			sock.Create(m_af, SocketType::Stream, ProtocolType::Tcp);
			m_supports = 1;
		} catch (RCExc) {
			m_supports = 0;
		}
	}
	return m_supports;
}

Socket::COSSupportsIPver
	Socket::OSSupportsIPv4(AddressFamily::InterNetwork),
	Socket::OSSupportsIPv6(AddressFamily::InterNetworkV6);

Socket::~Socket() {
	Close(true);
}

void Socket::Create(UInt16 nPort, int nSocketType, UInt32 host) {
	Open(nSocketType, 0, AF_INET);
	Bind(IPEndPoint(htonl(host), nPort));
}

void Socket::Create(AddressFamily af, SocketType socktyp, ProtocolType prottyp) {
	Open((int)socktyp, (int)prottyp, (int)af);
}

void Socket::ReleaseHandle(HANDLE h) const {
#if UCFG_WIN32
	SocketCheck(::closesocket(SOCKET(h)));
#else
	SocketCheck(::close(static_cast<SOCKET>(reinterpret_cast<intptr_t>(h))));
#endif
}

void Socket::Open(int nSocketType, int nProtocolType, int nAddressFormat) {
	if (Valid())
		Throw(E_EXT_AlreadyOpened);
	SOCKET h = ::socket(nAddressFormat, nSocketType, nProtocolType);
	if (h == INVALID_SOCKET)
		ThrowWSALastError();
	Attach(h);
}

void Socket::Bind(const IPEndPoint& ep) {
	SocketCheck(::bind(HandleAccess(_self), ep.c_sockaddr(), ep.sockaddr_len()));
}

bool Socket::ConnectHelper(const IPEndPoint& ep) {
	IPEndPoint nep(ep.Normalize());
	if (::connect(BlockingHandleAccess(_self), nep.c_sockaddr(), nep.sockaddr_len()) != SOCKET_ERROR) {
		return true;
	}
	if (WSAGetLastError() != WSA(EWOULDBLOCK))
		ThrowWSALastError();
	return false;
}

bool Socket::Connect(DWORD host, WORD hostPort) {
	return ConnectHelper(IPEndPoint(htonl(host), hostPort));
}

bool Socket::Connect(RCString hostAddress, WORD hostPort) {
	return ConnectHelper(IPEndPoint(hostAddress, hostPort));
}

void Socket::SendTo(const ConstBuf& cbuf, const IPEndPoint& ep) {	
	SocketCheck(::sendto(BlockingHandleAccess(_self), (const char*)cbuf.P, cbuf.Size, 0, ep.c_sockaddr(), ep.sockaddr_len()));
}

bool Socket::Accept(Socket& socket, IPEndPoint& epRemote) {
	byte sa[100];
	socklen_t addrlen = sizeof(sa);
	SOCKET s;
	s = ::accept(BlockingHandleAccess(_self), (sockaddr*)sa, &addrlen);
	if (s == INVALID_SOCKET)
		if (WSAGetLastError() == WSA(EWOULDBLOCK))
			return false;
		else
			ThrowWSALastError();
	socket.Attach(s);
	epRemote = IPEndPoint(*(const sockaddr*)sa);
	return true;
}

void Socket::Listen(int backLog) {
	SocketCheck(::listen(HandleAccess(_self), backLog));
}

#ifndef WDM_DRIVER
void Socket::ReleaseFromAPC() {
	if (SafeHandle::HandleAccess *ha = (SafeHandle::HandleAccess*)(void*)SafeHandle::t_pCurrentHandle)
		ha->Release();
}
#endif

#if UCFG_WIN32

void Socket::EventSelect(HANDLE hEvent, long lEvents) {
	SocketCheck(::WSAEventSelect(HandleAccess(_self), hEvent, lEvents));
}

WSANETWORKEVENTS Socket::EnumNetworkEvents(HANDLE hEvent) {
	WSANETWORKEVENTS r;
	SocketCheck(::WSAEnumNetworkEvents(HandleAccess(_self), hEvent, &r));
	return r;
}

int Socket::Ioctl(DWORD ioCode, const void *pIn, DWORD cbIn, void *pOut, DWORD cbOut, WSAOVERLAPPED *pov, LPWSAOVERLAPPED_COMPLETION_ROUTINE pfCR) {
	DWORD r;
	if (::WSAIoctl(BlockingHandleAccess(_self), ioCode, (void*)pIn, cbIn, pOut, cbOut, &r, pov, pfCR) == SOCKET_ERROR) {
		if (WSAGetLastError() != WSAEWOULDBLOCK)
			ThrowWSALastError();
		return SOCKET_ERROR;
	}
	return r;
}

#	if !UCFG_WCE
WSAPROTOCOL_INFO Socket::Duplicate(DWORD dwProcessid) {
	WSAPROTOCOL_INFO pi;
	SocketCheck(::WSADuplicateSocket(HandleAccess(_self), dwProcessid, &pi));
	return pi;
}
#	endif

void Socket::Create(int af, int type, int protocol, LPWSAPROTOCOL_INFO lpProtocolInfo, DWORD dwFlag) {
	SOCKET h = ::WSASocket(af, type, protocol, lpProtocolInfo, 0, dwFlag);
	if (h == INVALID_SOCKET)
		ThrowWSALastError();
	Attach(h);
}

int Socket::get_Available() {
	int r;
	IOControl(FIONREAD, Buf(&r, sizeof r));
	return r;
}

CWSAEvent::CWSAEvent()
	:	m_hEvent(WSACreateEvent())
{
	if (m_hEvent == WSA_INVALID_EVENT)
		ThrowWSALastError();
}

CWSAEvent::~CWSAEvent() {
	SocketCheck(WSACloseEvent(m_hEvent));
}


#endif // UCFG_WIN32

void Socket::Shutdown(int how) {
	TRC(3, (int)HandleAccess(_self) << "\t" << how);

	SocketCheck(::shutdown(HandleAccess(_self), how));
}

int Socket::Receive(void *buf, int len, int flags) {
	int r = recv(BlockingHandleAccess(_self), (char*)buf, len, flags);
	if (SOCKET_ERROR == r && WSAGetLastError() != WSA(EWOULDBLOCK))
		ThrowWSALastError();
	return r;
}

int Socket::Send(const void *buf, int len, int flags) {
	int r = send(BlockingHandleAccess(_self), (char*)buf, len, flags);
	if (SOCKET_ERROR == r && WSAGetLastError() != WSA(EWOULDBLOCK))
		ThrowWSALastError();
	return r;
}

int Socket::ReceiveFrom(void *buf, int len, IPEndPoint& ep) {
	byte bufSockaddr[100];
	ZeroStruct(bufSockaddr);
	sockaddr& sa =  *(sockaddr*)bufSockaddr;
	sa.sa_family = AF_INET;
	socklen_t addrLen = sizeof(bufSockaddr);
	int r = recvfrom(BlockingHandleAccess(_self), (char*)buf, len, 0, &sa, &addrLen);
	if (r == SOCKET_ERROR && WSAGetLastError() != WSA(EWOULDBLOCK))
		ThrowWSALastError();
	ep = IPEndPoint(sa);
	return r;
}

void Socket::Attach(SOCKET s) {
	Close();
	SafeHandle::Attach(HANDLE(s));
	//!!!  m_hSocket = s;
}

SOCKET Socket::Detach() {
	return static_cast<SOCKET>(reinterpret_cast<intptr_t>(SafeHandle::Detach()));

	//!!!return exchange(m_hSocket, INVALID_SOCKET);
}

void Socket::GetSocketOption(int optionLevel, int optionName, void *pVal, socklen_t& len) {
	SocketCheck(getsockopt(HandleAccess(_self), optionLevel, optionName, (char *)pVal, &len));
}

void Socket::SetSocketOption(int optionLevel, int optionName, const ConstBuf& mb) {
	SocketCheck(::setsockopt(HandleAccess(_self), optionLevel, optionName, (const char*)mb.P, mb.Size));
}

void Socket::IOControl(int code, const Buf& mb) {
#if UCFG_WIN32
	SocketCheck(::ioctlsocket(HandleAccess(_self), code, (u_long*)mb.P));
#else
	SocketCheck(::ioctl(HandleAccess(_self), code, (u_long*)mb.P));
#endif
}

IPEndPoint Socket::get_LocalEndPoint() {
	IPEndPoint ep;
	socklen_t len = sizeof(sockaddr_in6);
	SocketCheck(::getsockname(HandleAccess(_self), &ep.Address.m_sockaddr, &len));
	return ep;
}

IPEndPoint Socket::get_RemoteEndPoint() {
	IPEndPoint ep;
	socklen_t len = sizeof(sockaddr_in6);
	SocketCheck(::getpeername(HandleAccess(_self), &ep.Address.m_sockaddr, &len));
	return ep;
}

void NetworkStream::ReadBuffer(void *buf, size_t count) const {
	while (count) {
		if (int n = m_sock.Receive(buf, (int)count)) {    
			count -= n;
			(byte*&)buf += n;
		} else {
			m_bEof = true;
			Throw(E_EXT_EndOfStream);
		}
	}
}

void NetworkStream::WriteBuffer(const void *buf, size_t count) {
	while (count) {
		if (int n = m_sock.Send(buf, (int)count)) {
			if (n < 0)
				Throw(E_FAIL);
			count -= n;
			(const byte*&)buf += n;
		} else if (count)
			Throw(E_FAIL);
	}
}

int NetworkStream::ReadByte() const {
	DBG_LOCAL_IGNORE_NAME(E_EXT_EndOfStream, E_EXT_EndOfStream);
	try {
		byte a;
		ReadBuffer(&a, 1);
		return a;
	} catch (RCExc e) {
		if (e.HResult == E_EXT_EndOfStream)
			return -1;
		throw;
	}
}

bool NetworkStream::Eof() const {
	return m_bEof;
}

void NetworkStream::Close() const {
	m_sock.Close();
}

void CSocketLooper::Send(Socket& sock, const ConstBuf& mb) {
	NetworkStream stm(sock);
	stm.WriteBuffer(mb.P, (int)mb.Size);
}

void CSocketLooper::Loop(Socket& sockS, Socket& sockD) {
	FUN_TRACE_2

	DBG_LOCAL_IGNORE_NAME(HRESULT_FROM_WIN32(WSA(ECONNRESET)), ignReset);

	class CLoopKeeper {
	public:
		bool m_bLive, m_bAccepts,
			m_bIncoming;
		CSocketLooper& m_socketLooper;
		Socket &m_sock, &m_sockOther;
		Socket::BlockingHandleAccess m_hp;

		CLoopKeeper(CSocketLooper& socketLooper, Socket& sock, Socket& sockOther, bool bIncoming = false)
			:	m_socketLooper(socketLooper)
			,	m_sock(sock)
			,	m_hp(m_sock)
			,	m_sockOther(sockOther)
			,	m_bLive(true)
			,	m_bAccepts(true)
			,	m_bIncoming(bIncoming)
		{}

		bool Process(fd_set *fdset) {
			if (FD_ISSET(m_hp, fdset)) {
				BYTE buf[BUF_SIZE];
				int r = m_sock.Receive(buf, sizeof buf);
				if (m_bLive = r) {
					bool bDisconnectAfterData = false;
					Blob blob(buf, r);
					if (m_bIncoming)
						m_socketLooper.ProcessDest(blob, bDisconnectAfterData);
					else
						m_socketLooper.ProcessSrc(blob, bDisconnectAfterData);
					try {
						m_socketLooper.Send(m_sockOther, blob);
					} catch (RCExc) {
						m_bAccepts = false;
						if (m_socketLooper.m_bDoShutdown)
							m_sock.Shutdown(SHUT_RD);
					}
					if (bDisconnectAfterData)
						return false;
				} else {
					if (m_socketLooper.m_bDoShutdown)
						m_sockOther.Shutdown(SHUT_WR);
				}

			}
			return true;
		}
	};

	CLoopKeeper loopS(_self, sockS, sockD, true),
				loopD(_self, sockD, sockS);
	while (loopS.m_bLive && loopS.m_bAccepts ||
			loopD.m_bLive && loopD.m_bAccepts)
	{
		fd_set readfds;
		FD_ZERO(&readfds);
		int nfds = 0;
		if (loopS.m_bLive && loopS.m_bAccepts) {
			FD_SET((SOCKET)loopS.m_hp, &readfds);
#if UCFG_USE_POSIX
			nfds = std::max(nfds, 1+(SOCKET)loopS.m_hp);
#endif
		}
		if (loopD.m_bLive && loopD.m_bAccepts) {
			FD_SET((SOCKET)loopD.m_hp, &readfds);
#if UCFG_USE_POSIX
			nfds = std::max(nfds, 1+(SOCKET)loopD.m_hp);
#endif
		}
		TimeSpan span = GetTimeout();
		timeval timeout,
			*pTimeout = 0;
		if (span.Ticks != -1) {
			pTimeout = &timeout;
			span.ToTimeval(timeout);
		}
		if (NeedToCancel())
			break;
		if (!SocketCheck(::select(nfds, &readfds, 0, 0, pTimeout))) {
			TRC(1, "Session timed-out!");
			break;
		}
		if (!loopS.Process(&readfds) || !loopD.Process(&readfds))
			break;
	}
}



} // Ext::

