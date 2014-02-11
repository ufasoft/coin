/*######     Copyright (c) 1997-2013 Ufasoft  http://ufasoft.com  mailto:support@ufasoft.com,  Sergey Pavlov  mailto:dev@ufasoft.com #######################################
#                                                                                                                                                                          #
# This program is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation;  #
# either version 3, or (at your option) any later version. This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the      #
# implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details. You should have received a copy of the GNU #
# General Public License along with this program; If not, see <http://www.gnu.org/licenses/>                                                                               #
##########################################################################################################################################################################*/

#pragma once

namespace Ext {

class CVariant : public PROPVARIANT, public CPersistent {
  	void Check(VARTYPE t) const;
public:
	typedef CVariant class_type;

  	CVariant();
  	CVariant(const PROPVARIANT& v);
  	CVariant(DWORD dw);
  	CVariant(const Blob& b);
  	~CVariant();
  	void Clear();
  	CVariant& operator=(const PROPVARIANT& v);

  	DWORD get_UI4() const;
  	void put_UI4(DWORD dw);
  	DEFPROP_CONST(DWORD, UI4);

  	Blob get_Blob() const;
  	void put_Blob(const Blob& blob);
  	DEFPROP_CONST(Blob, Blob);
};

class CProperty : public CPersistent {
public:
	typedef CProperty class_type;

  	PROPID m_id;
  	String m_name;
  	DWORD m_dwType;
  	Blob m_blob;

  	~CProperty();
  	void Read(const BinaryReader& rd) override;
  	void Write(BinaryWriter& wr) const override;

  	CVariant get_Value();
  	void put_Value(const CVariant& v);
  	DEFPROP(CVariant, Value);
};

class CPropertySection : public CPersistent {
public:
	CLSID m_clsid;
	std::vector<CProperty> m_ar;

	CProperty *GetProperty(PROPID propid);
  	void Read(const BinaryReader& rd) override;
  	void Write(BinaryWriter& wr) const override;;
	CVariant TryGet(PROPID propid, const CVariant& v);
};

class CPropertySet : public CPersistent {
	CComPtr<IPropertySetStorage> m_iPSS;
	CComPtr<IPropertyStorage> m_iPS;
public:
	std::vector<CPropertySection> m_sections;

	CPropertySection *AddSection(const CLSID& clsid);
	CPropertySection *GetSection(const CLSID& clsid);
	void Create(IStorage *stg);
	void Create(IStream *stm);
	void Set(PROPID propid, const VARIANT& v);
  	void Read(const BinaryReader& rd) override;
  	void Write(BinaryWriter& wr) const override;
};

class CProp : public CPersistent, public Object {
public:
	String m_name;

	virtual ~CProp();

	virtual CVariant GetValue();
	virtual void SetValue(const CVariant& v);
	__declspec(property(get=GetValue, put=SetValue)) CVariant Value;

	virtual COleVariant GetOleVariant() {
		return COleVariant();
	}

	virtual void SetOleVariant(const VARIANT& v)
	{}

	void Read(const BinaryReader& rd) override {}
	void Write(BinaryWriter& wr) const override {}
};

class CColorProp : public CProp {
public:
	OLE_COLOR m_clr;

	CColorProp()
		:	m_clr(0)
	{}

	CVariant GetValue() {
		return m_clr;
	}

	void SetValue(const CVariant& v) {
		m_clr = v.UI4;
	}

	COleVariant GetOleVariant() {
		return COleVariant(long(m_clr));
	}

	void SetOleVariant(const VARIANT& v);
};

class CFontHolder {
public:
	CComPtr<IFont> m_iFont;

 	HFONT GetFontHandle();
};

class CFontProp : public CProp {
public:
	CFontHolder m_font;

	CVariant GetValue();
	void SetValue(const CVariant& v);

	COleVariant GetOleVariant() {
		return (IUnknown*)m_font.m_iFont;
	}

	void SetOleVariant(const VARIANT& v) {
		m_font.m_iFont = AsUnknown(v);
	}
};


#define IMPLEMENT_STOCKPROP(t, n) \
  STDMETHOD(get_##n)(t *p) \
  { \
    return COleControl::get_##n(p);  \
  } \
  STDMETHOD(put_##n)(t v) \
  { \
    return COleControl::put_##n(v); \
  }

#define IMPLEMENT_STOCKPROPREF(t, n) \
  STDMETHOD(get_##n)(t *p) \
  { \
    return COleControl::get_##n(p);  \
  } \
  STDMETHOD(putref_##n)(t v) \
  { \
    return COleControl::putref_##n(v); \
  }

/*!!!
#define IMPLEMENT_STOCKPROP_FORECOLOR \
  STDMETHOD(get_ForeColor)(OLE_COLOR *pclr) \
  { \
    return COleControl::get_ForeColor(pclr);  \
  } \
  STDMETHOD(put_ForeColor)(OLE_COLOR clr) \
  { \
    return COleControl::put_ForeColor(clr); \
  }

#define IMPLEMENT_STOCKPROP_BACKCOLOR \
  STDMETHOD(get_BackColor)(OLE_COLOR *pclr) \
  { \
    return COleControl::get_BackColor(pclr);  \
  } \
  STDMETHOD(put_BackColor)(OLE_COLOR clr) \
  { \
    return COleControl::put_BackColor(clr); \
  }

#define IMPLEMENT_STOCKPROP_FONT \
  STDMETHOD(get_Font)(IFont **pfont) \
  { \
    return COleControl::get_Font(pfont);  \
  } \
  STDMETHOD(putref_Font)(IFont *font) \
  { \
    return COleControl::putref_Font(font);  \
  }*/

/////////////////////////////////////////////////////////////////////////////
// Codes for COleControl::SendAdvise
//......Code.........................Method called
#define OBJECTCODE_SAVED          0  //IOleAdviseHolder::SendOnSave
#define OBJECTCODE_CLOSED         1  //IOleAdviseHolder::SendOnClose
#define OBJECTCODE_RENAMED        2  //IOleAdviseHolder::SendOnRename
#define OBJECTCODE_SAVEOBJECT     3  //IOleClientSite::SaveObject
#define OBJECTCODE_DATACHANGED    4  //IDataAdviseHolder::SendOnDataChange
#define OBJECTCODE_SHOWWINDOW     5  //IOleClientSite::OnShowWindow(TRUE)
#define OBJECTCODE_HIDEWINDOW     6  //IOleClientSite::OnShowWindow(FALSE)
#define OBJECTCODE_SHOWOBJECT     7  //IOleClientSite::ShowObject
#define OBJECTCODE_VIEWCHANGED    8  //IOleAdviseHolder::SendOnViewChange

class COleControl : public CComDispatchImpl,
                    public IOleObject,
                    public IOleControl,
                    public IOleInPlaceObject,
                    public IOleInPlaceActiveObject,
                    public IPersistPropertyBag,
                    public IPersistStorage,
                    public IPersistStreamInit,
                    public IViewObjectEx,
                    public IProvideClassInfo
{
	LONG m_cxExtent,
	   m_cyExtent;
	Rectangle m_rcPos;
	CComPtr<IOleClientSite> m_iClientSite;
	CComPtr<IOleControlSite> m_iControlSite;
	CComPtr<IOleInPlaceSite> m_iInPlaceSite;
	CComPtr<IOleInPlaceFrame> m_iInPlaceFrame;
	CComPtr<IOleInPlaceUIWindow> m_iInPlaceDoc;
	CComPtr<IOleAdviseHolder> m_iAdviseHolder;
	bool m_bOpen,
	   m_bUIActive,
	   m_bModified;

	STDMETHOD(SetClientSite)(LPOLECLIENTSITE);
	STDMETHOD(GetClientSite)(LPOLECLIENTSITE*);
	STDMETHOD(SetHostNames)(LPCOLESTR, LPCOLESTR);
	STDMETHOD(Close)(DWORD);
	STDMETHOD(SetMoniker)(DWORD, LPMONIKER);
	STDMETHOD(GetMoniker)(DWORD, DWORD, LPMONIKER*);
	STDMETHOD(InitFromData)(LPDATAOBJECT, BOOL, DWORD);
	STDMETHOD(GetClipboardData)(DWORD, LPDATAOBJECT*);
	STDMETHOD(DoVerb)(LONG, LPMSG, LPOLECLIENTSITE, LONG, HWND, LPCRECT);
	STDMETHOD(EnumVerbs)(IEnumOLEVERB**);
	STDMETHOD(Update)();
	STDMETHOD(IsUpToDate)();
	STDMETHOD(GetUserClassID)(CLSID*);
	STDMETHOD(GetUserType)(DWORD, LPOLESTR*);
	STDMETHOD(SetExtent)(DWORD, LPSIZEL);
	STDMETHOD(GetExtent)(DWORD, LPSIZEL);
	STDMETHOD(Advise)(LPADVISESINK, LPDWORD);
	STDMETHOD(Unadvise)(DWORD);
	STDMETHOD(EnumAdvise)(LPENUMSTATDATA*);
	STDMETHOD(GetMiscStatus)(DWORD, LPDWORD);
	STDMETHOD(SetColorScheme)(LPLOGPALETTE);

	STDMETHOD(GetControlInfo)(LPCONTROLINFO pCI);
	STDMETHOD(OnMnemonic)(LPMSG pMsg);
	STDMETHOD(OnAmbientPropertyChange)(DISPID dispid);
	STDMETHOD(FreezeEvents)(BOOL bFreeze);

	STDMETHOD(GetWindow)(HWND*);
	STDMETHOD(ContextSensitiveHelp)(BOOL);
	STDMETHOD(InPlaceDeactivate)();
	STDMETHOD(UIDeactivate)();
	STDMETHOD(SetObjectRects)(LPCRECT, LPCRECT);
	STDMETHOD(ReactivateAndUndo)();

	STDMETHOD(TranslateAccelerator)(LPMSG);
	STDMETHOD(OnFrameWindowActivate)(BOOL);
	STDMETHOD(OnDocWindowActivate)(BOOL);
	STDMETHOD(ResizeBorder)(LPCRECT, LPOLEINPLACEUIWINDOW, BOOL);
	STDMETHOD(EnableModeless)(BOOL);

	STDMETHOD(GetClassID)(LPCLSID);
	STDMETHOD(IsDirty)();
	STDMETHOD(InitNew)(LPSTORAGE);
	STDMETHOD(Load)(LPSTORAGE);
	STDMETHOD(Save)(LPSTORAGE, BOOL);
	STDMETHOD(SaveCompleted)(LPSTORAGE);
	STDMETHOD(HandsOffStorage)();

	STDMETHOD(Load)(LPPROPERTYBAG pPropBag, LPERRORLOG pErrorLog);
	STDMETHOD(Save)(LPPROPERTYBAG pPropBag, BOOL fClearDirty, BOOL fSaveAllProperties);

	STDMETHOD(Load)(LPSTREAM);
	STDMETHOD(Save)(LPSTREAM, BOOL);
	STDMETHOD(GetSizeMax)(ULARGE_INTEGER *);
	STDMETHOD(InitNew)();

	STDMETHOD(Draw)(DWORD, LONG, void*, DVTARGETDEVICE*, HDC, HDC, LPCRECTL, LPCRECTL, BOOL (CALLBACK*)(ULONG_PTR), ULONG_PTR);
	STDMETHOD(GetColorSet)(DWORD, LONG, void*, DVTARGETDEVICE*, HDC, LPLOGPALETTE*);
	STDMETHOD(Freeze)(DWORD, LONG, void*, DWORD*);
	STDMETHOD(Unfreeze)(DWORD);
	STDMETHOD(SetAdvise)(DWORD, DWORD, LPADVISESINK);
	STDMETHOD(GetAdvise)(DWORD*, DWORD*, LPADVISESINK*);
	STDMETHOD(GetExtent) (DWORD, LONG, DVTARGETDEVICE*, LPSIZEL);
	STDMETHOD(GetRect)(DWORD, LPRECTL);
	STDMETHOD(GetViewStatus)(DWORD*);
	STDMETHOD(QueryHitPoint)(DWORD, LPCRECT, POINT, LONG, DWORD*);
	STDMETHOD(QueryHitRect)(DWORD, LPCRECT, LPCRECT, LONG, DWORD*);
	STDMETHOD(GetNaturalExtent)(DWORD, LONG, DVTARGETDEVICE*, HDC, DVEXTENTINFO*, LPSIZEL);

	STDMETHOD(GetClassInfo)(LPTYPEINFO* ppTypeInfo);

	void OnActivateInPlace(bool bUIActivate);
	void OnHide();
	void CreateControlWindow(HWND hwndParent, const Rectangle& rcPos);
	String PropIDtoStr(PROPID propid);
	protected:
	COleDispatchDriver m_ambientDispDriver;
	std::auto_ptr<CWnd> m_pWnd;

	typedef std::map<PROPID, ptr<CProp>> CMapProps;
	CMapProps m_mapProps;

	void SendAdvise(UINT uCode);
	void InvalidateControl(LPCRECT lpRect = 0, bool bErase = true);
	void SaveState(Stream& stm);
	void LoadState(Stream& stm);
	void SetPropsetData(LPFORMATETC lpFormatEtc, LPSTGMEDIUM lpStgMedium, REFCLSID fmtid);
	void AddStock_ForeColor();
	void AddStock_BackColor();
	void AddStock_Font();

	HRESULT get_ForeColor(OLE_COLOR *pclr);
	HRESULT put_ForeColor(OLE_COLOR clr);
	HRESULT get_BackColor(OLE_COLOR *pclr);
	HRESULT put_BackColor(OLE_COLOR clr);
	HRESULT get_Font(IFont **pfont);
	HRESULT putref_Font(IFont *font);
public:
	UINT m_userTypeNameID;
	CLSID m_clsid;
	unsigned m_bMsgReflect : 1;

	_EXT_BEGIN_COM_MAP(COleControl)
		_EXT_COM_INTERFACE_ENTRY(IOleObject)
		_EXT_COM_INTERFACE_ENTRY(IOleControl)
		_EXT_COM_INTERFACE_ENTRY(IOleInPlaceObject)
		_EXT_COM_INTERFACE_ENTRY(IOleInPlaceActiveObject)
		_EXT_COM_INTERFACE_ENTRY(IPersistPropertyBag)
		_EXT_COM_INTERFACE_ENTRY(IPersistStorage)
		_EXT_COM_INTERFACE_ENTRY(IPersistStreamInit)
		_EXT_COM_INTERFACE_ENTRY(IViewObject2)
		_EXT_COM_INTERFACE_ENTRY(IProvideClassInfo)
	_EXT_END_COM_MAP()

	COleControl();
	~COleControl();
	COleDispatchDriver *COleControl::GetAmbientDispatchDriver();
	bool GetAmbientProperty(DISPID dwDispID, VARTYPE vtProp, void* pvProp);
	OLE_COLOR AmbientForeColor();
	CFontHolder& InternalGetFont();
	virtual BOOL IsSubclassedControl();
};

class CControlClass : public CComGeneralClass {
public:
	int m_idBM;
	DWORD m_misc;
	String m_sVersion;

	CControlClass(const CLSID& clsid, EXT_ATL_CREATORFUNC *pfn, int descID, LPCTSTR progID,
				LPCTSTR indProgID = 0, DWORD flags = THREADFLAGS_APARTMENT)
		:	CComGeneralClass(clsid, pfn, descID, progID, indProgID, flags)
	{}    

	void Register();
};




} // Ext::
