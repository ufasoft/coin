/*######     Copyright (c) 1997-2013 Ufasoft  http://ufasoft.com  mailto:support@ufasoft.com,  Sergey Pavlov  mailto:dev@ufasoft.com #######################################
#                                                                                                                                                                          #
# This program is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation;  #
# either version 3, or (at your option) any later version. This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the      #
# implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details. You should have received a copy of the GNU #
# General Public License along with this program; If not, see <http://www.gnu.org/licenses/>                                                                               #
##########################################################################################################################################################################*/

#pragma once

namespace Ext {

class CCommandLineInfo : public Object { //!!!
	void ParseParamFlag(RCString pszParam);
	void ParseParamNotFlag(RCString pszParam);
	void ParseLast(bool bLast);
public:
	bool m_bShowSplash;
	bool m_bRunEmbedded;
	bool m_bRunAutomated;
	enum { FileNew, FileOpen, FilePrint, FilePrintTo, FileDDE, AppRegister,
		AppUnregister, FileNothing = -1 } m_nShellCommand;

	// not valid for FileNew
	String m_strFileName;

	// valid only for FilePrintTo
	String m_strPrinterName;
	String m_strDriverName;
	String m_strPortName;

	CCommandLineInfo();
	virtual ~CCommandLineInfo() {}
	virtual void ParseParam(RCString pszParam, bool bFlag, bool bLast);
};

#if UCFG_ARGV_UNICODE
typedef wchar_t argv_char_t;
#else
typedef char argv_char_t;
#endif

class CAppBase
#if UCFG_COMPLEX_WINAPP	
	:	public CWinThread //!!! must be Thread
#endif
{
	typedef CAppBase class_type;
public:
	EXT_DATA static bool s_bSigBreak;

	std::vector<String> m_argv;
	int Argc;
	CPointer<argv_char_t *> Argv;
	std::vector<argv_char_t*> m_argvp;

	bool m_bPrintLogo;
	String FileDescription;
	String SVersion;
	String LegalCopyright;
	String Url;
	String m_appName;
#ifdef _WIN32
	ptr<ThreadPool> m_pThreadPool;
#endif

	String GetInternalName();

	String m_appDataDir;

	String get_AppDataDir();
	DEFPROP_GET(String, AppDataDir);


#if UCFG_COMPLEX_WINAPP
	HINSTANCE m_hInstance;

	String m_exeName;
	String m_cmdLine;
	int m_nCmdShow;

	RegistryKey m_keyLM,
		m_keyCU;

	CAppBase();
	void SetCurrentHandles();
	String GetCompanyName();
	String GetRegPath();

	RegistryKey& get_KeyLM();
	DEFPROP_GET(RegistryKey&, KeyLM);

	RegistryKey& get_KeyCU();
	DEFPROP_GET(RegistryKey&, KeyCU);
#else
	static CAppBase *I;

	CAppBase()
		:	m_bPrintLogo(true)
	{
		I = this;
	}

	~CAppBase() {
		I = nullptr;
	}

#endif
#if !UCFG_EXTENDED || !UCFG_COMPLEX_WINAPP
	virtual int InitInstance() { return 1; }
	virtual int ExitInstance() {
		return 1;
	}
#endif

	void AFXAPI ParseCommandLine(CCommandLineInfo& rCmdInfo);
protected:
	void ProcessArgv(int argc, argv_char_t *argv[]);
#if !UCFG_ARGV_UNICODE
	void ProcessArgv(int argc, String::Char *argv[]);
#endif

	String m_internalName;
};

AFX_API CAppBase * AFXAPI AfxGetCApp();

class CConApp : public CAppBase, public CCommandLineInfo {
	static CConApp *s_pApp;

	static void __cdecl OnSigInt(int sig);
public:
	CConApp();

	EXT_API int Main(int argc, argv_char_t *argv[]);
	virtual void Usage() {}
	virtual void Execute() {}	
	virtual bool OnSignal(int sig);

	static void AFXAPI SetSignals();
};

#ifdef WIN32

#	if UCFG_ARGV_UNICODE
#		define ext_main wmain
#	else
#		define ext_main main
#	endif
#	define EXT_DEFINE_MAIN(app)	extern "C" int __cdecl ext_main(int argc, argv_char_t *argv[]) { return app.Main(argc, argv); }

#else

#	define EXT_DEFINE_MAIN(app)	extern "C" int __cdecl main(int argc, char *argv[]) {	\
	app.FileDescription = VER_FILEDESCRIPTION_STR;									\
	app.SVersion			= VER_PRODUCTVERSION_STR;									\
	app.LegalCopyright = VER_LEGALCOPYRIGHT_STR;										\
	app.Url = VER_EXT_URL;															\
	return app.Main(argc, argv); }

#endif

#ifdef WIN32

class CDocTemplate;
class CDocManager;
class CCmdUI;
class CDocument;

#if !UCFG_WCE

#define AFX_ABBREV_FILENAME_LEN 30
#define _AFX_MRU_COUNT   4      // default support for 4 entries in file MRU
#define _AFX_MRU_MAX_COUNT 16   // currently allocated id range supports 16


class AFX_CLASS CRecentFileList {
public:
	int m_nSize;                // contents of the MRU list
	int m_nMaxDisplayLength;
	UINT m_nStart;
	String m_strSectionName,
		m_strEntryFormat;
	String m_strOriginal;      // original menu item contents
	std::vector<String> m_arrNames;

	CRecentFileList(UINT nStart, LPCTSTR lpszSection, LPCTSTR lpszEntryFormat, int nSize, int nMaxDispLen = AFX_ABBREV_FILENAME_LEN);
	virtual ~CRecentFileList() {}
	bool GetDisplayName(String& strName, int nIndex, LPCTSTR lpszCurDir, int nCurDir, bool bAtLeastName = true) const;
	virtual void UpdateMenu(CCmdUI *pCmdUI);
	virtual void ReadList();    // reads from registry or ini file
	virtual void WriteList();   // writes to registry or ini file
	virtual void Add(LPCTSTR pathName);
	virtual void Remove(int nIndex);
};

#endif // !UCFG_WCE

class EXTAPI CWinApp : public CAppBase {
#if UCFG_WIN_MSG
	DECLARE_MESSAGE_MAP()
#endif
public:
	CWinApp(RCString lpszAppName = "");
	virtual ~CWinApp();
	virtual int InitInstance();
protected:
	int m_nWaitCursorCount;
	HCURSOR m_hcurWaitCursorRestore;

#if UCFG_COMPLEX_WINAPP

protected:
#if UCFG_EXTENDED && !UCFG_WCE	

	std::unique_ptr<CRecentFileList> m_pRecentFileList;

	void LoadStdProfileSettings(UINT nMaxMRU = _AFX_MRU_COUNT); // load MRU file list and last preview state
	void SaveStdProfileSettings();  // save options to .INI file
#endif
	void SetRegistryKey(RCString registryKey);
	afx_msg void OnFileNew();
	afx_msg void OnFileOpen();
	afx_msg void OnHelp();
	afx_msg void OnAppExit();
public:
	String m_registryKey;
	String m_profileName;
	std::unique_ptr<Object> m_pDocManager;
	ptr<CCommandLineInfo> m_pCmdInfo;


	bool ProcessShellCommand(CCommandLineInfo& rCmdInfo);
#if UCFG_WND
	virtual int DoMessageBox(RCString lpszPrompt, UINT nType, UINT nIDPrompt);
#endif
#if UCFG_GUI
	void AddDocTemplate(CDocTemplate* pTemplate);
	virtual void DoWaitCursor(int nCode);
	HICON LoadIcon(const CResID& resID) const;
#	if !UCFG_WCE
	virtual void AddToRecentFileList(RCString pathName);  // add to MRU
#	endif
#	if UCFG_WIN32_FULL
	virtual CDocument* OpenDocumentFile(RCString lpszFileName);
#	endif
	virtual bool Register();
	virtual bool Unregister();
#endif	// UCFG_GUI
	void RegisterShellFileTypes(BOOL bCompat);
	void UnregisterShellFileTypes();
	virtual void OnAbort() {}
	BOOL SaveAllModified();
	void HideApplication();
	void CloseAllDocuments(BOOL bEndSession);
	bool DoPromptFileName(String& fileName, UINT nIDSTitle, DWORD lFlags, bool bOpenFileDialog, CDocTemplate* pTemplate);

	CDocManager *get_DocManager();
	DEFPROP_GET(CDocManager*, DocManager);

	LRESULT ProcessWndProcException(RCExc e, const MSG *pMsg);
	String GetProfileString(LPCTSTR lpszSection, LPCTSTR lpszEntry, LPCTSTR lpszDefault = 0);
	void WriteProfileString(LPCTSTR lpszSection, LPCTSTR lpszEntry, LPCTSTR lpszValue);
	void Enable3dControls();
	void GetAppRegistryKey(RegistryKey& key);

#endif
};

#define WM_USER                         0x0400

const int UWM_NOTIFYICON = WM_USER+1;
const int UWM_INVALID_REGCODE = WM_USER+10;


//!!!FILETIME AFXAPI StringToFileTime(RCString s);

//!!!#	ifndef _EXT 



class CConsoleStreamHook {
	FILE *m_file;
public:
	int m_descPrev;
	int m_arDesc[2];

	CConsoleStreamHook(FILE *file = stdout, size_t size = 0);
	virtual ~CConsoleStreamHook();
	void CreateFromFile(FILE *file, size_t size = 0);
	void CloseInput();
	Blob ReadToEnd();
};

class CStreamHookThread : public Thread, public CConsoleStreamHook {
	void Stop() {
		CloseInput();
		Thread::Stop();
	}

	void Execute();
public:
	CStreamHookThread(thread_group *tr, FILE *file)
		:	Thread(tr)
		,	CConsoleStreamHook(file)
	{}
};

class CConsoleStreamsCodepageConverter {
	thread_group m_tr;
public:
	CConsoleStreamsCodepageConverter();
	~CConsoleStreamsCodepageConverter();
};

class CDllServer {
public:
	static CDllServer *I;

	CDllServer() {
		I = this;
	}

	virtual ~CDllServer() {
		I = 0;
	}

	virtual HRESULT OnRegister();
	virtual HRESULT OnUnregister();
};



//!!!#	endif // _EXT

#endif // WIN32

EXT_API String AFXAPI RtfToText(RCString rtf);
EXT_API String AFXAPI HtmlToText(RCString html);


} // Ext::
