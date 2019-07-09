#include <el/ext.h>

#ifdef HAVE_LIBUDEV
#	include <el/comp/device-manager.h>
#endif

#if UCFG_USE_POSIX
#	include <termios.h>

class CTermiosKeeper {
public:
	int m_fd;
	termios m_orig;

	CTermiosKeeper(int fd)
		:	m_fd(fd)
	{
		CCheck(::tcgetattr(m_fd, &m_orig));
	}

	~CTermiosKeeper() {
		::tcsetattr(m_fd, 0, &m_orig);
	}
};

#endif // UCFG_USE_POSIX

#if UCFG_WIN32
#	include <windows.h>
#endif

#include "serial-device.h"

namespace Coin {

void SerialDeviceThread::Write(RCSpan cbuf) {
#if UCFG_USE_POSIX
	CCheck(fwrite(cbuf.P, cbuf.Size, 1, m_file));
	CCheck(fflush(m_file));
	fseek(m_file, 0, SEEK_CUR);				// don't check retval, switch after reading, bug in the MSC CRT fflush() for non-buffered streams
#else
	int rc = CCheck(_write(fileno(m_file), cbuf.data(), cbuf.size()));
	if (rc != cbuf.size()) {
		*Miner.m_pTraceStream << "write() not complete" << endl;
		throw StopException();
	}
#endif
}

Blob SerialDeviceThread::Command(const ConstBuf cmd, size_t replySize) {
	Write(cmd);

	Blob buf(0, (replySize==size_t(-1) || replySize==0) ? 256 : replySize);
	int i = 0;
	while (true) {
#if UCFG_USE_POSIX
		size_t repsize = replySize==size_t(-1) ? buf.Size : replySize;
		if (0 == repsize)
			repsize = 1;
		int rc = fread(buf.data(), 1, repsize, m_file);
		CCheck(rc);
		if (replySize == 0 && rc>0)
			continue;
		if (replySize==size_t(-1))
			buf.Size = rc;
		else if (rc != replySize) {
			Throw(E_FAIL);
		}
		fseek(m_file, 0, SEEK_CUR);					// dont check retval, switch after reading
#else
		int fd = fileno(m_file);
		if (replySize == 0) {
			int rc = _read(fd, buf.data(), 1);
			if (rc < 1) {
				CCheck(rc, EINVAL);
				break;
			}
			if (rc == 1)
				continue;
			break;
		} else if (replySize == size_t(-1)) {
			int rc = _read(fd, buf.data(), buf.size());
			if (rc < 1)
				Throw(E_FAIL);
			if (replySize == size_t(-1))
				buf.resize(rc);
			else if (rc != replySize) {
				Throw(E_FAIL);
			}
		} else {
			for (int i=0; i<replySize; ++i) {
				int rc = _read(fd, buf.data()+i, 1);
				if (rc != 1)
					Throw(E_FAIL);
			}
		}
#endif
		break;
	}
	return buf;
}

void SerialDeviceThread::Execute() {
	Name = "SerialDevice_Thread";

	if (m_file = fopen(m_devpath, "r+b")) {
		try {			
			struct FileKeeper {
				SerialDeviceThread *m_t;

				~FileKeeper() {
					m_t->CloseFile();

					EXT_LOCK (m_t->Miner.MtxDevices) {
						m_t->Miner.TestedDevices.erase(m_t->m_devpath);
						m_t->Miner.WorkingDevices.erase(m_t->m_devpath);
						if (m_t->Device)
							Remove(m_t->Miner.Devices, m_t->Device);
					}	
				}
			} fileKeeper = { this };			
			CCheck(setvbuf(m_file, 0, _IONBF, 0));
#if UCFG_USE_POSIX
			CTermiosKeeper termKeeper(fileno(m_file));
			termios tio = termKeeper.m_orig;
			::cfmakeraw(&tio);
			tio.c_cc[VMIN] = 0;
			tio.c_cc[VTIME] = MsTimeout/100;
			CCheck(::tcsetattr(termKeeper.m_fd, 0, &tio));
#else
			COMMTIMEOUTS cto = { MsTimeout, 0, MsTimeout, 0, MsTimeout };
			::SetCommTimeouts((HANDLE)_get_osfhandle(fileno(m_file)), &cto);
#endif
			CommunicateDevice();
		} catch (StopException&) {
		} catch (Exception& ex) {
			if (ex.code() != ExtErr::InvalidUTF8String) {
				*Miner.m_pTraceStream << ex.what() << endl;
			}
		} catch (...) {
			*Miner.m_pTraceStream << "Unknown exception detected" << endl;
		}
		if (Detected && !m_bStop) {
			*Miner.m_pTraceStream << "device detached" << endl;
		}
	}
	m_evDetected.Set();
}

void SerialDeviceThread::Stop() {
	base::Stop();
	m_evStartMining.Set();
#if UCFG_WIN32_FULL
	typedef BOOL (WINAPI *PFN_CancelIoEx)(HANDLE hFile, LPOVERLAPPED lpOverlapped);
	DlProcWrap<PFN_CancelIoEx> pfn("kernel32.dll", "CancelIoEx");
	if (pfn && m_file)
		pfn((HANDLE)_get_osfhandle(fileno(m_file)), 0);
#endif
}

bool SerialDeviceArchitecture::TimeToDetect() {
	DateTime now = Clock::now();
	if (now < m_dtNextDetect)
		return false;
	m_dtNextDetect = now+TimeSpan::FromSeconds(10);
	return true;		
}

void SerialDeviceArchitecture::DetectNewDevices(BitcoinMiner& miner) {
	if (!TimeToDetect())
		return;
}

} // Coin::

