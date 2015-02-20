#include <el/ext.h>

#include <el/win/idle-monitor.cpp>

#include <el/win/service.h>
using namespace Ext::ServiceProcess;

#include "miner-service.h"

namespace Coin {

String MINER_SERVICE_NAME = VER_INTERNALNAME_STR;

#ifdef _DEBUG
	TimeSpan IdleTimeBeforeStart = TimeSpan::FromSeconds(59);
#else
	TimeSpan IdleTimeBeforeStart = TimeSpan::FromMinutes(5);
#endif

void InstallMinerService() {
	vector<String> ss = Environment::GetCommandLineArgs();
	ss.erase(ss.begin());
	for (int i=0; i<ss.size(); ++i) {
		if (ss[i] == "-S") {
			ss.erase(ss.begin()+i);
			break;
		}
	}
	CSCManager sm;
	sm.CreateService(MINER_SERVICE_NAME, MINER_SERVICE_NAME, MAXIMUM_ALLOWED, SERVICE_WIN32_OWN_PROCESS, SERVICE_AUTO_START, SERVICE_ERROR_NORMAL, "\""+System.ExeFilePath+"\" " + String::Join(" ", ss));
	cerr << "Service " << MINER_SERVICE_NAME << " installed" << endl;
}

void UninstallMinerService() {
	ServiceController sc(MINER_SERVICE_NAME);
	try {
		sc.Stop();
	} catch (RCExc) {
	}
#ifdef _DEBUG//!!!D
	auto status = sc.Status;
#endif
	sc.Delete();
	cerr << "Service " << MINER_SERVICE_NAME << " uninstalled" << endl;
}

class MinerService : public ServiceBase {
	typedef ServiceBase base;
public:
	BitcoinMiner& Miner;
	CBool m_bPaused;

	MinerService(BitcoinMiner& miner)
		:	Miner(miner)
	{
		ServiceName = MINER_SERVICE_NAME;		
		CanPauseAndContinue = CanHandlePowerEvent = true;
		CanHandleSessionChangeEvent = true;
	}

protected:
	void OnSessionChange(const SessionChangeDescription& scd) override {
		base::OnSessionChange(scd);

		switch (scd.Reason) {
		case SessionChangeReason::RemoteConnect:
		case SessionChangeReason::SessionLogon:
		case SessionChangeReason::SessionUnlock:
			Miner.Stop();
			break;
		}
	}

	void OnStart() override;
	
	void OnStop() override {
		CloseMiner();
		base::OnStop();
	}
	
	void OnShutdown() override {
		CloseMiner();
		base::OnShutdown();
	}

	void OnPause() override {
		m_bPaused = true;
		Miner.Stop();
		base::OnPause();
	}

	void OnContinue() override {
		m_bPaused = false;
		base::OnContinue();
	}

	void OnPowerEvent(PowerBroadcastStatus powerStatus) override {
		switch (powerStatus) {			
		case PowerBroadcastStatus::Suspend:
		case PowerBroadcastStatus::BatteryLow:
			Miner.Stop();
			m_bPaused = true;
			break;
		case PowerBroadcastStatus::ResumeSuspend:
			m_bPaused = false;
			break;
		}
		base::OnPowerEvent(powerStatus);
	}
private:
	ptr<Thread> m_timerThread;

	void CloseMiner();
};

class TimerThread : public Thread {
public:
	MinerService& Service;

	TimerThread(MinerService& service)
		:	Service(service)
	{
	}

	void Execute() override;
private:
//!!!R	TimeSpan m_spanToPowerSave;
};


void MinerService::OnStart() {
	(m_timerThread = new TimerThread(_self))->Start();
	base::OnStart();
}

void MinerService::CloseMiner() {
	m_timerThread->interrupt();
	m_timerThread->Join();
	m_timerThread = nullptr;

	Miner.Stop();
}

void RunMinerAsService(BitcoinMiner& miner) {
#if UCFG_TRC
	void *posPrev = CTrace::s_pOstream;
	int nLevelPrev = CTrace::s_nLevel;
	static ofstream s_log("/var/log/miner-service.log");
	CTrace::s_pOstream = &s_log;
	CTrace::s_nLevel = 31;
#endif
#ifdef X_DEBUG//!!!D
	while (true) {
		miner.Start();
		Sleep(30000);
		miner.Stop();
		Sleep(30000);
	}
	return;
#endif
	MinerService svc(miner);
	try {
		svc.Run();
	} catch (RCExc) {
#if UCFG_TRC
		CTrace::s_pOstream = posPrev;
		CTrace::s_nLevel = nLevelPrev;
#endif
		throw;
	}
}

void TimerThread::Execute() {
	Name = "TimerThread";

	IdleMonitor idleMonitor;

	while (!m_bStop) {
		try {
			seconds idleTime = duration_cast<seconds>(idleMonitor.GetIdleTime());
			TRC(4, "Idle Time: " << idleTime.count());

			if (idleTime > IdleTimeBeforeStart && !Service.m_bPaused)
				Service.Miner.Start();
			else
				Service.Miner.Stop();
		} catch (RCExc DBG_PARAM(ex)) {
			TRC(1, ex.what());
		}
		Sleep(8000);		// IdleTime has granularity 15 s, but we want to detect changing ASAP
	}
}

} // Coin::
