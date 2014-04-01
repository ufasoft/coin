#pragma once

#include "p2p-peers.h"


#ifndef UCFG_P2P_SEND_THREAD
#	define UCFG_P2P_SEND_THREAD 0
#endif

namespace Ext { namespace Inet { namespace P2P {
class Net;
class Link;
}}}

namespace Ext {
	template <> struct ptr_traits<Ext::Inet::P2P::Link> {
		typedef Interlocked interlocked_policy;
	};
}

namespace Ext { namespace Inet { namespace P2P {

const int P2P_CONNECT_TIMEOUT = 5000;
const int PERIODIC_SECONDS = 10;
const int PERIODIC_SEND_SECONDS = 10;

class Message : public Object, public CPersistent {
public:
	typedef Interlocked interlocked_policy;

	DateTime Timestamp;
	ptr<P2P::Link> Link;

	virtual void Process(P2P::Link& link) {}
};

#if UCFG_P2P_SEND_THREAD

class LinkSendThread : public SocketThread {
	typedef SocketThread base;
public:
	P2P::Link& Link;
	AutoResetEvent m_ev;	

	LinkSendThread(P2P::Link& link);

	void Stop() override {
		base::Stop();
		m_ev.Set();
	}
protected:
#if UCFG_WIN32
	void OnAPC() override {
		base::OnAPC();
		Socket::ReleaseFromAPC();
	}
#endif

	void Execute() override;
};

#endif // UCFG_P2P_SEND_THREAD

class Link : public LinkBase {
	typedef LinkBase base;
public:
	typedef Interlocked interlocked_policy;

	CPointer<P2P::Net> Net;
#if UCFG_P2P_SEND_THREAD
	ptr<LinkSendThread> SendThread;
#endif
	
	vector<ptr<Message>> OutQueue;
	TcpClient Tcp;
	DateTime m_dtCheckLastRecv,
			m_dtLastRecv,
			m_dtLastSend;
	Blob DataToSend;
	TimeSpan PingTimeout;
	int FirstByte;
	int PeerVersion;
	CBool UseMagic;
	CBool LineBased;	//!!!TODO

	Link(P2P::NetManager *netManager, thread_group *tr)
		:	base(netManager, tr)
		,	PeerVersion(0)
		,	UseMagic(true)
		,	FirstByte(-1)
		,	PingTimeout(TimeSpan::FromMinutes(20))
	{
	}

	virtual void SendBinary(const ConstBuf& buf);
	virtual void Send(ptr<P2P::Message> msg);				// ptr<> to prevent Memory Leak in Send(new Message) construction
	virtual size_t GetMessageHeaderSize();
	virtual size_t GetMessagePayloadSize(const ConstBuf& buf);
	virtual ptr<Message> RecvMessage(const BinaryReader& rd);
	virtual void OnMessageReceived(Message *m);
	virtual void ReceiveAndProcessMessage(const BinaryReader& rd);
	virtual void ReceiveAndProcessLineMessage(const ConstBuf& bufLine);
	virtual void OnCloseLink();
	virtual void OnPingTimeout();
	
	void OnSelfLink();

	void Stop() override;
protected:
#if UCFG_WIN32
	void OnAPC() override {
		base::OnAPC();
		Socket::ReleaseFromAPC();
	}
#endif

	void BeforeStart() override;
	void Execute() override;	
	virtual void OnPeriodic() {}

	virtual bool OnStartConnection() { return true; }

	friend class LinkSendThread;
};

class ListeningThread : public SocketThread {
	typedef SocketThread base;
public:
	P2P::NetManager& NetManager;

	ListeningThread(P2P::NetManager& netManager, thread_group& tr, AddressFamily af = AddressFamily::InterNetwork);
protected:
	void BeforeStart() override;
	void Execute() override;
private:
	AddressFamily m_af;
	Socket m_sock;	
};

class Net : public PeerManager {
	typedef PeerManager base;
public:
	thread_group m_tr;
	CBool Runned;
	UInt32 ProtocolMagic;
	bool Listen;

	Net(P2P::NetManager& netManager)
		:	base(netManager)
		,	ProtocolMagic(0)
		,	Listen(true)
	{}

	virtual void Start() {
		PeerManager::m_owner = &m_tr;
//!!!		(MsgThread = new MsgLoopThread(_self, tr))->Start();

		Runned = true;
	}

	bool IsRandomlyTrickled() {
		return EXT_LOCKED(MtxPeers, Ext::Random().Next(Links.size()) == 0);
	}

//	ptr<Peer> GetPeer(const IPEndPoint& ep);
//	virtual void Send(Link& link, Message& msg) =0;

	
protected:
	virtual size_t GetMessageHeaderSize() =0;
	virtual size_t GetMessagePayloadSize(const ConstBuf& buf) =0;
	virtual ptr<Message> RecvMessage(Link& link, const BinaryReader& rd) =0;
	virtual void OnPingTimeout(Link& link) {}
	virtual void OnInitLink(Link& link) {}

	virtual void OnInitMsgLoop() {}
	virtual void OnCloseMsgLoop() {}
	virtual void OnMessageReceived(Message *m) {}

	virtual void OnPeriodicMsgLoop() {
		PeerManager::OnPeriodic();
	}

	friend class Link;
	friend class MsgLoopThread;
};



}}} // Ext::Inet::P2P::

