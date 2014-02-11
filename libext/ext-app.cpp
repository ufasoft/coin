/*######     Copyright (c) 1997-2013 Ufasoft  http://ufasoft.com  mailto:support@ufasoft.com,  Sergey Pavlov  mailto:dev@ufasoft.com #######################################
#                                                                                                                                                                          #
# This program is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation;  #
# either version 3, or (at your option) any later version. This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the      #
# implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details. You should have received a copy of the GNU #
# General Public License along with this program; If not, see <http://www.gnu.org/licenses/>                                                                               #
##########################################################################################################################################################################*/

#include <el/ext.h>

#include "ext-fw.h"

#if UCFG_WIN32
#	include <el/libext/win32/ext-win.h>
#endif

namespace Ext {
using namespace std;
	
CCommandLineInfo::CCommandLineInfo() {
	m_bShowSplash = true;
	m_bRunEmbedded = false;
	m_bRunAutomated = false;
	m_nShellCommand = FileNew;
}

void CCommandLineInfo::ParseParamFlag(RCString pszParam) {
	String p = pszParam;
	p.MakeUpper();
	if (p == "PT")
		m_nShellCommand = FilePrintTo;
	else if (p == "P")
		m_nShellCommand = FilePrint;
	else if (p == "REGISTER" || p == "REGSERVER")
		m_nShellCommand = AppRegister;
	else if (p == "UNREGISTER" || p == "UNREGSERVER")
		m_nShellCommand = AppUnregister;
}

void CCommandLineInfo::ParseParamNotFlag(RCString pszParam) {
	if (m_strFileName.IsEmpty())
		m_strFileName = pszParam;
	else if (m_nShellCommand == FilePrintTo && m_strPrinterName.IsEmpty())
		m_strPrinterName = pszParam;
	else if (m_nShellCommand == FilePrintTo && m_strDriverName.IsEmpty())
		m_strDriverName = pszParam;
	else if (m_nShellCommand == FilePrintTo && m_strPortName.IsEmpty())
		m_strPortName = pszParam;
}

void CCommandLineInfo::ParseLast(bool bLast) {
	if (bLast) {
		if ((m_nShellCommand == FileNew || m_nShellCommand == FileNothing)&& !m_strFileName.IsEmpty())
			m_nShellCommand = FileOpen;
		m_bShowSplash = !m_bRunEmbedded && !m_bRunAutomated;
	}
}

void CCommandLineInfo::ParseParam(RCString pszParam, bool bFlag, bool bLast) {
	if (bFlag)
		ParseParamFlag(pszParam);
	else
		ParseParamNotFlag(pszParam);
	ParseLast(bLast);
}




#if !UCFG_COMPLEX_WINAPP
CAppBase *CAppBase::I;
#endif

bool CAppBase::s_bSigBreak;

#if UCFG_COMPLEX_WINAPP

CAppBase::CAppBase()
	:	m_bPrintLogo(true)
{
	Argc = __argc;
#if UCFG_ARGV_UNICODE
	ProcessArgv(__argc, __wargv);
#else
	ProcessArgv(__argc, __argv);
#endif
	if (!Argv && __wargv)
		ProcessArgv(__argc, __wargv);
	AFX_MODULE_STATE* pModuleState = _AFX_CMDTARGET_GETSTATE();
	pModuleState->m_pCurrentCApp = this;

#if UCFG_USE_RESOURCES
	{
		FileVersionInfo vi;
		if (vi.m_blob.Size != 0) {
			FileDescription = vi.FileDescription;
			SVersion = vi.GetProductVersionN().ToString(2);
			LegalCopyright = vi.LegalCopyright;
			Url = TryGetVersionString(vi, "URL");
		} else
			m_bPrintLogo = false;
	}
#endif
	AttachSelf();
}

String CAppBase::GetCompanyName() {
	String companyName = MANUFACTURER;
	DWORD dw;
	try {
		if (GetFileVersionInfoSize((_TCHAR*)(const _TCHAR*)AfxGetModuleState()->FileName, &dw)) //!!!
			companyName = FileVersionInfo().CompanyName;
	} catch (RCExc){
	}
	return companyName;
}

String CAppBase::GetRegPath() {
	return "Software\\"+GetCompanyName()+"\\"+GetInternalName();
}

RegistryKey& CAppBase::get_KeyLM() {
	if (!m_keyLM.Valid())
		m_keyLM.Open(HKEY_LOCAL_MACHINE, GetRegPath(), true);
	return m_keyLM;
}

RegistryKey& CAppBase::get_KeyCU() {
	if (!m_keyCU.Valid())
		m_keyCU.Open(HKEY_CURRENT_USER, GetRegPath(), true);
	return m_keyCU;
}

#endif // UCFG_COMPLEX_WINAPP

#if UCFG_WIN32_FULL

String COperatingSystem::get_WindowsDirectory() {
	TCHAR szPath[_MAX_PATH];
	Win32Check(::GetWindowsDirectory(szPath , _countof(szPath)));
	return szPath;
}

#endif // UCFG_WIN32_FULL


#if UCFG_WIN32

String AFXAPI GetAppDataManufacturerFolder() {
	String r;
#	if !UCFG_WCE
	try {
		r = Environment::GetEnvironmentVariable("APPDATA");
		if (!r)
			r = Environment::GetFolderPath(SpecialFolder::ApplicationData);
	} catch (RCExc) {
		r = Path::Combine(System.WindowsDirectory, "Application Data");
	}
#	endif
#if UCFG_COMPLEX_WINAPP
	r = Path::Combine(r, AfxGetApp()->GetCompanyName());
#else
	r = Path::Combine(r, MANUFACTURER);
#endif
	Directory::CreateDirectory(r);
	return r;
}
#endif


String CAppBase::GetInternalName() {
	if (!m_internalName.IsEmpty())
		return m_internalName;
#if UCFG_WIN32
	DWORD dw;
	if (GetFileVersionInfoSize((_TCHAR*)(const _TCHAR*)AfxGetModuleState()->FileName, &dw))
		return FileVersionInfo().InternalName;
#endif
#ifdef VER_INTERNALNAME_STR
	return VER_INTERNALNAME_STR;
#else
	return "InternalName";
#endif
}

String CAppBase::get_AppDataDir() {
	if (m_appDataDir.IsEmpty()) {
#if UCFG_WIN32
		String dir = Path::Combine(GetAppDataManufacturerFolder(), GetInternalName());
#elif UCFG_USE_POSIX
		String dir = Path::Combine(Environment::GetFolderPath(SpecialFolder::ApplicationData), "."+GetInternalName());
#endif
		Directory::CreateDirectory(AddDirSeparator(m_appDataDir = dir));
	}
	return m_appDataDir;
}


void CAppBase::ProcessArgv(int argc, argv_char_t *argv[]) {
	Argc = argc;
#if UCFG_WCE
	if (argv) {
		for (int i=0; i<=argc; ++i) {
			String arg = argv[i];
			if (!arg.IsEmpty() && arg.Length>=2 && arg[0]=='\"' && arg[arg.Length-1]=='\"')
				arg = arg.Substring(1, arg.Length-2);
			m_argv.push_back(arg);
			m_argvp.push_back((argv_char_t*)(const argv_char_t*)m_argv[i]);
		}
		Argv = &m_argvp[0];
	}
#else
	Argv = argv;
#endif
}

#if !UCFG_ARGV_UNICODE
void CAppBase::ProcessArgv(int argc, String::Char *argv[]) {
	Argc = argc;
	if (argv) {
		for (int i=0; i<=argc; ++i) {
			String arg = argv[i];
	#if UCFG_WCE
			if (arg.Length>=2 && arg[0]=='\"' && arg[arg.Length-1]=='\"')
				arg = arg.Substring(1, arg.Length-2);
	#endif
			m_argv.push_back(arg);
			m_argvp.push_back((argv_char_t*)(const argv_char_t*)m_argv[i]);
		}
		Argv = &m_argvp[0];
	}
}
#endif // !UCFG_ARGV_UNICODE

void CAppBase::ParseCommandLine(CCommandLineInfo& rCmdInfo) {
	for (int i = 1; i < Argc; i++)
	{
		bool bFlag = false;
		bool bLast = ((i + 1) == Argc);
#ifdef WIN32
		if (__wargv) {
			LPCWSTR pszParam = __wargv[i];
			if (pszParam[0] == '-' || pszParam[0] == '/')
			{
				// remove flag specifier
				bFlag = TRUE;
				++pszParam;
			}
			rCmdInfo.ParseParam(pszParam, bFlag, bLast);
		} else 
#endif
		{
			const argv_char_t *pszParam = Argv[i];
			if (pszParam[0] == '-' || pszParam[0] == '/') {
				// remove flag specifier
				bFlag = true;
				++pszParam;
				if (pszParam[0] == '-')
					++pszParam;
			}
			rCmdInfo.ParseParam(pszParam, bFlag, bLast);
		}
	}
}

CAppBase * AFXAPI AfxGetCApp() {
#if UCFG_COMPLEX_WINAPP
	return AfxGetModuleState()->m_pCurrentCApp;
#else
	return CAppBase::I;
#endif
}


/*!!!R
#if UCFG_WIN32_FULL

class COemStreambuf : public filebuf {
	int overflow(int c) override {
#if UCFG_WIN32_FULL
		if (c != EOF)
			CharToOemA((LPCSTR)&c, (LPSTR)&c);
#endif
		if (c == 7) //!!! BELL
			return c; 
		return filebuf::overflow(c);
	}
public:
	COemStreambuf(FILE *file)
		:	filebuf(file)
	{}
};


class CConsoleFilter {
public:
	CConsoleFilter()
		:	m_sbufStdout(stdout)
		,	m_sbufStderr(stderr)
	{
		if (_isatty(_fileno(stdout)))
			cout.rdbuf(&m_sbufStdout);
		if (_isatty(_fileno(stderr)))
			cerr.rdbuf(&m_sbufStderr);
	}
private:
	COemStreambuf m_sbufStdout,
				m_sbufStderr;

}; //!!! g_consoleFilter;

#endif
*/

CConApp::CConApp() {
	s_pApp = this;
	setlocale(LC_CTYPE, "");
#if UCFG_WIN32
	_wsetlocale(LC_CTYPE, L"");
#endif

	/*!!!
#if UCFG_WIN32_FULL
	locale locConsole(locale(), new CodepageCvt(::GetConsoleOutputCP()));
	if (_isatty(_fileno(stdout)))
		wcout.imbue(locConsole);
	if (_isatty(_fileno(stderr)))
		wcerr.imbue(locConsole);
	

//!!!	new CConsoleFilter;
#endif 
*/
}



CConApp *CConApp::s_pApp;

int CConApp::Main(int argc, argv_char_t *argv[]) {
#if UCFG_USE_POSIX
	cerr << " \b";		// mix of std::cerr && std::wcerr requires this hack
#endif
	ProcessArgv(argc, argv);
	try {
#ifdef WIN32
		AfxWinInit(::GetModuleHandle(0), NULL, ::GetCommandLine(), 0);
#endif
#if UCFG_COMPLEX_WINAPP || !UCFG_EXTENDED
		InitInstance();
#endif
#if !UCFG_WCE
		SetSignals();
#endif
		ParseCommandLine(_self);
		if (m_bPrintLogo) {
			wcerr << FileDescription << ' ' << SVersion << "  " << LegalCopyright;
			if (!Url.IsEmpty())
				wcerr << "  " << Url;
			wcerr << endl;
		}
		Execute();
	} catch (const Exception& ex) {
		switch (ex.HResult) {
		case 1:
			Usage();
		case 0:
		case E_EXT_NormalExit:
			break;
		case E_EXT_SignalBreak:
			Environment::ExitCode = 1;
			break;
		case 3:
			Environment::ExitCode = 3;  // Compilation error
			break;
		default:
			wcerr << ex << endl;
			Environment::ExitCode = 2;
		}
	} catch (const exception& ex) {
		cerr << ex.what() << endl;
		Environment::ExitCode = 2;
	}
#if UCFG_COMPLEX_WINAPP || !UCFG_EXTENDED
	ExitInstance();
#endif
	return Environment::ExitCode;
}

bool CConApp::OnSignal(int sig) {
	s_bSigBreak = true;
	return false;
}

#if !UCFG_WCE

void CConApp::SetSignals() {
	signal(SIGINT, OnSigInt);
#ifdef WIN32
	signal(SIGBREAK, OnSigInt);
#endif
}

void __cdecl CConApp::OnSigInt(int sig) {
	TRC(2, sig);
	if (s_pApp->OnSignal(sig))
		signal(sig, OnSigInt);
	else {
		raise(sig);
#ifdef WIN32
		AfxEndThread(0, true); //!!!
#endif
	}
}
#endif // !UCFG_WCE

} // Ext::


