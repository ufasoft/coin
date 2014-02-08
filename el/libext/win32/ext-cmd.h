/*######     Copyright (c) 1997-2013 Ufasoft  http://ufasoft.com  mailto:support@ufasoft.com,  Sergey Pavlov  mailto:dev@ufasoft.com #######################################
#                                                                                                                                                                          #
# This program is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation;  #
# either version 3, or (at your option) any later version. This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the      #
# implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details. You should have received a copy of the GNU #
# General Public License along with this program; If not, see <http://www.gnu.org/licenses/>                                                                               #
##########################################################################################################################################################################*/

#pragma once

namespace Ext {

class CCmdTarget;
class AFX_MODULE_STATE;

/*!!!OLD
#define DECLARE_MESSAGE_MAP() \
private: \
static const AFX_MSGMAP_ENTRY _messageEntries[]; \
protected: \
static AFX_DATA const AFX_MSGMAP messageMap; \
static const AFX_MSGMAP *_GetBaseMessageMap(); \
virtual const AFX_MSGMAP *GetMessageMap() const; \

#define BEGIN_MESSAGE_MAP(theClass, baseClass) \
const AFX_MSGMAP* theClass::_GetBaseMessageMap() \
{ return &baseClass::messageMap; } \
const AFX_MSGMAP* theClass::GetMessageMap() const \
{ return &theClass::messageMap; } \
AFX_COMDAT AFX_DATADEF const AFX_MSGMAP theClass::messageMap = \
{ &theClass::_GetBaseMessageMap, &theClass::_messageEntries[0] }; \
AFX_COMDAT const AFX_MSGMAP_ENTRY theClass::_messageEntries[] = \
{ \
#define END_MESSAGE_MAP() \
{0, 0, 0, 0, AfxSig_end, (AFX_PMSG)0 } \
}; \
*/

//#define PTM_WARNING_DISABLE
//#define PTM_WARNING_RESTORE

#define DECLARE_MESSAGE_MAP() \
protected: \
	static const AFX_MSGMAP* PASCAL GetThisMessageMap(); \
	virtual const AFX_MSGMAP* GetMessageMap() const; \


#define BEGIN_MESSAGE_MAP(theClass, baseClass) \
	PTM_WARNING_DISABLE \
	const AFX_MSGMAP* theClass::GetMessageMap() const \
{ return GetThisMessageMap(); } \
	const AFX_MSGMAP* PASCAL theClass::GetThisMessageMap() \
{ \
	typedef theClass ThisClass;						   \
	typedef baseClass TheBaseClass;					   \
	static const AFX_MSGMAP_ENTRY _messageEntries[] =  \
{

#define END_MESSAGE_MAP() \
{0, 0, 0, 0, AfxSig_end, (AFX_PMSG)0 } \
}; \
	static const AFX_MSGMAP messageMap = \
{ &TheBaseClass::GetThisMessageMap, &_messageEntries[0] }; \
	return &messageMap; \
}								  \
	PTM_WARNING_RESTORE

#define AFX_MSG_CALL

typedef void (AFX_MSG_CALL CCmdTarget::*AFX_PMSG)();

#define _AFX_PACKING    4   // default packs structs at 4 bytes
#pragma pack(push, _AFX_PACKING)


struct AFX_MSGMAP_ENTRY
{
	UINT nMessage;   // windows message
	UINT nCode;      // control code or WM_NOTIFY code
	UINT nID;        // control ID (or 0 for windows messages)
	UINT nLastID;    // used for entries specifying a range of control id's
	UINT_PTR nSig;       // signature type (action) or pointer to message #
	AFX_PMSG pfn;    // routine to call (or special value)
};

struct AFX_MSGMAP
{
	const AFX_MSGMAP *(PASCAL* pfnGetBaseMap)();
	const AFX_MSGMAP_ENTRY *lpEntries;
};


enum AFX_DISPMAP_FLAGS
{
	afxDispCustom = 0,
	afxDispStock = 1
};

struct AFX_DISPMAP_ENTRY
{
	LPCTSTR lpszName;       // member/property name
	long lDispID;           // DISPID (may be DISPID_UNKNOWN)
	LPCSTR lpszParams;      // member parameter description
	AFX_PMSG pfn;           // normal member On<membercall> or, OnGet<property>
	AFX_PMSG pfnSet;        // special member for OnSet<property>
	size_t nPropOffset;     // property offset
	AFX_DISPMAP_FLAGS flags;// flags (e.g. stock/custom)
	WORD vt;                // return value type / or type of property
};

struct AFX_EVENTSINKMAP_ENTRY
{
	AFX_DISPMAP_ENTRY dispEntry;
	UINT nCtrlIDFirst;
	UINT nCtrlIDLast;
};


struct AFX_EVENTSINKMAP
{
	const AFX_EVENTSINKMAP* (* pfnGetBaseMap)();
	const AFX_EVENTSINKMAP_ENTRY* lpEntries;
	UINT* lpEntryCount;
};

#define DECLARE_EVENTSINK_MAP() \
private: \
	static const AFX_EVENTSINKMAP_ENTRY _eventsinkEntries[]; \
	static UINT _eventsinkEntryCount; \
protected: \
	static AFX_DATA const AFX_EVENTSINKMAP eventsinkMap; \
	static const AFX_EVENTSINKMAP* _GetBaseEventSinkMap(); \
	virtual const AFX_EVENTSINKMAP* GetEventSinkMap() const; \

#define BEGIN_EVENTSINK_MAP(theClass, baseClass) \
	const AFX_EVENTSINKMAP* theClass::_GetBaseEventSinkMap() \
{ return &baseClass::eventsinkMap; } \
	const AFX_EVENTSINKMAP* theClass::GetEventSinkMap() const \
{ return &theClass::eventsinkMap; } \
	const AFX_EVENTSINKMAP theClass::eventsinkMap = \
{ &theClass::_GetBaseEventSinkMap, &theClass::_eventsinkEntries[0], \
	&theClass::_eventsinkEntryCount }; \
	UINT theClass::_eventsinkEntryCount = (UINT)-1; \
	const AFX_EVENTSINKMAP_ENTRY theClass::_eventsinkEntries[] = \
{ \

#define END_EVENTSINK_MAP() \
{ VTS_NONE, DISPID_UNKNOWN, VTS_NONE, VT_VOID, \
	(AFX_PMSG)NULL, (AFX_PMSG)NULL, (size_t)-1, afxDispCustom, \
	(UINT)-1, 0 } }; \

struct AFX_INTERFACEMAP_ENTRY
{
	const void* piid;       // the interface id (IID) (NULL for aggregate)
	size_t nOffset;         // offset of the interface vtable from m_unknown
};

struct AFX_INTERFACEMAP
{
	const AFX_INTERFACEMAP* (* pfnGetBaseMap)(); // NULL is root class
	const AFX_INTERFACEMAP_ENTRY* pEntry; // map for this class
};

#define DECLARE_INTERFACE_MAP() \
private: \
	static const AFX_INTERFACEMAP_ENTRY _interfaceEntries[]; \
protected: \
	static AFX_DATA const AFX_INTERFACEMAP interfaceMap; \
	static const AFX_INTERFACEMAP* _GetBaseInterfaceMap(); \
	virtual const AFX_INTERFACEMAP* GetInterfaceMap() const; \

/*!!!
#define DECLARE_DISPATCH_MAP() \
private: \
static const AFX_DISPMAP_ENTRY _dispatchEntries[]; \
static UINT _dispatchEntryCount; \
static DWORD _dwStockPropMask; \
protected: \
static AFX_DATA const AFX_DISPMAP dispatchMap; \
static const AFX_DISPMAP* _GetBaseDispatchMap(); \
virtual const AFX_DISPMAP* GetDispatchMap() const; \
*/

struct AFX_CONNECTIONMAP_ENTRY
{
	const void* piid;   // the interface id (IID)
	size_t nOffset;         // offset of the interface vtable from m_unknown
};

struct AFX_CONNECTIONMAP
{
	const AFX_CONNECTIONMAP* (* pfnGetBaseMap)(); // NULL is root class
	const AFX_CONNECTIONMAP_ENTRY* pEntry; // map for this class
};

#define DECLARE_CONNECTION_MAP() \
private: \
	static const AFX_CONNECTIONMAP_ENTRY _connectionEntries[]; \
protected: \
	static AFX_DATA const AFX_CONNECTIONMAP connectionMap; \
	static const AFX_CONNECTIONMAP* _GetBaseConnectionMap(); \
	virtual const AFX_CONNECTIONMAP* GetConnectionMap() const; \


#undef AFX_DATA
#define AFX_DATA AFX_CORE_DATA

struct AFX_CMDHANDLERINFO
{
	CCmdTarget* pTarget;
	void (AFX_MSG_CALL CCmdTarget::*pmf)(void);
};

class CCmdTarget : public Object {
	DECLARE_DYNAMIC(CCmdTarget)
public:
	AFX_MODULE_STATE *m_pModuleState;

	CCmdTarget();
	~CCmdTarget();
	void BeginWaitCursor();
	void EndWaitCursor();
	void RestoreWaitCursor();
	virtual int InitInstance();
	virtual int ExitInstance() { return TRUE; } //!!!
#if UCFG_WIN_MSG
	virtual BOOL OnCmdMsg(UINT nID, int nCode, void *pExtra, AFX_CMDHANDLERINFO *pHandlerInfo);
#endif
	//!!!	virtual BOOL GetDispatchIID(IID* pIID);

	//!!!friend class COleConnPtContainer;
protected:
	/*!!! 	void GetStandardProp(const AFX_DISPMAP_ENTRY* pEntry, VARIANT* pvarResult, UINT* puArgErr);
	HRESULT SetStandardProp(const AFX_DISPMAP_ENTRY* pEntry, DISPPARAMS* pDispParams, UINT* puArgErr);
	HRESULT CallMemberFunc(const AFX_DISPMAP_ENTRY* pEntry, WORD wFlags, VARIANT* pvarResult, DISPPARAMS* pDispParams, UINT* puArgErr);
	static UINT GetEntryCount(const AFX_DISPMAP *pDispMap);
	const AFX_DISPMAP_ENTRY *GetDispEntry(LONG memid);
	static MEMBERID MemberIDFromName(const AFX_DISPMAP* pDispMap, LPCTSTR lpszName);
	static UINT GetStackSize(const BYTE* pbParams, VARTYPE vtResult);
	HRESULT PushStackArgs(BYTE* pStack, const BYTE* pbParams, void* pResult, VARTYPE vtResult, DISPPARAMS* pDispParams,
	UINT* puArgErr, VARIANT* rgTempVars);*/

	DECLARE_MESSAGE_MAP()
};

#pragma pack(pop)

HRESULT AFXAPI GetLastHResult();
void AFXAPI SetLastHResult(HRESULT hr);

AFX_API int AFXAPI Win32Check(LRESULT i);


typedef HRESULT (WINAPI _ATL_CREATORARGFUNC)(void* pv, REFIID riid, LPVOID* ppv, DWORD_PTR dw);

#define _EXT_SIMPLEMAPENTRY ((_ATL_CREATORARGFUNC*)1)

struct _ATL_INTMAP_ENTRY {
	const IID* piid;       // the interface id (IID)
	DWORD_PTR dw;
	_ATL_CREATORARGFUNC* pFunc; //NULL:end, 1:offset, n:ptr
};


} // Ext::
