/*######     Copyright (c) 1997-2013 Ufasoft  http://ufasoft.com  mailto:support@ufasoft.com,  Sergey Pavlov  mailto:dev@ufasoft.com #######################################
#                                                                                                                                                                          #
# This program is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation;  #
# either version 3, or (at your option) any later version. This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the      #
# implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details. You should have received a copy of the GNU #
# General Public License along with this program; If not, see <http://www.gnu.org/licenses/>                                                                               #
##########################################################################################################################################################################*/

#pragma once

#ifdef _WIN32
#	include <amd-app/cal.h>
#	include <amd-app/cal_ext.h>
#	include <amd-app/calcl.h>
#else
#	include <CAL/cal.h>
#	include <CAL/cal_ext.h>
#	include <CAL/calcl.h>
#endif

namespace Ext { 

namespace Gpu {

const int
	CAL_TARGET_TAHITI 		= 20,
	CAL_TARGET_PITCAIRN 	= 21,
	CAL_TARGET_CAPE_VERDE 	= 22,
	CAL_TARGET_TRINITY 		= 23,
	CAL_TARGET_BONAIRE 		= 26;


void CalCheck(CALresultEnum v);
void CalclCheck(CALresultEnum v);

}

template<> struct HandleTraits<CALdevice> : HandleTraitsBase<CALdevice> {
	static void ResourceRelease(const handle_type& h) {
		CALresultEnum rc = ::calDeviceClose(h);
		if (rc != CAL_RESULT_BUSY)				//!!! normal, when used with OpenCL
			Gpu::CalCheck(rc);
	}
};

namespace Gpu {

using namespace Ext;

class CalException : public Exc {
	typedef Exc base;
public:
	CalException(HRESULT hr, RCString s)
		:	base(hr, s)
	{}
};

class CalEngine;
class CalMem;
class CalContext;

class CalDeviceObj : public Object, public ResourceWrapper<CALdevice> {
public:
	CalEngine& Engine;

	CalDeviceObj(CalEngine& engine, CALdevice h)
		:	Engine(engine)
	{
		m_h = h;
	}

	~CalDeviceObj();
protected:

	friend class CalDevice;
};

class CalDevice {
	typedef CalDevice class_type;
public:
	int Index;

	operator CALdevice() { return m_pimpl->m_h; }

	CALdevicestatus get_Status();
	DEFPROP_GET(CALdevicestatus, Status);
private:
	ptr<CalDeviceObj> m_pimpl;
	friend class CalEngine;
};

class CalEngine {
public:	

	CalEngine();
	~CalEngine();

	static UInt64 AFXAPI GetVersion();
	int DeviceCount();
	CALdeviceattribs GetDeviceAttribs(int idx);
	CalDevice GetDevice(int idx);

	CALdeviceinfo GetDeviceInfo(int idx) {
		CALdeviceinfoRec r;
		CalCheck(::calDeviceGetInfo(&r, idx));
		return r;
	}
protected:
	std::vector<CalDeviceObj*> m_devices;	

	friend class CalDeviceObj;
};

template <typename T>
struct CalType {
	enum {
		CAL_FORMAT = 0
	};
};

//template <> struct CalType<UInt32> { enum { CAL_FORMAT = CAL_FORMAT_UNSIGNED_INT32_4 }; };
template <> struct CalType<UInt32> { enum { CAL_FORMAT = CAL_FORMAT_FLOAT_4 }; };

class CalResourceObj : public Object {
public:
	CALresource m_h;

	~CalResourceObj() {
		CalCheck(::calResFree(m_h));
	}
};

template <typename T>
class CalResource {
public:
	CalResource() {
	}

	CalResource(CalDevice& dev, int width) {
		CALresource res;
		CALdevice calDev = dev;
		CalCheck(::calResAllocRemote1D(&res, &calDev, 1, width, CALformat(CalType<T>::CAL_FORMAT), 0));
		m_pimpl = new CalResourceObj;
		m_pimpl->m_h = res;
	}

	operator CALresource() {
		return m_pimpl->m_h;
	}
protected:
	ptr<CalResourceObj> m_pimpl;
};

template <typename T>
class CalGlobalResource : public CalResource<T> {
	typedef CalResource<T> base;
public:
	CalGlobalResource(bool)
	{}

	CalGlobalResource(CalDevice& dev, int width, int height) {
		Init(dev, width, height);
	}

	void Init(CalDevice& dev, int width, int height) {
		CALresource res;
		CalCheck(::calResAllocLocal2D(&res, dev, width, height, CALformat(CalType<T>::CAL_FORMAT), CAL_RESALLOC_GLOBAL_BUFFER));
		base::m_pimpl = new CalResourceObj;
		base::m_pimpl->m_h = res;
	}
};

template <typename T>
class CalResMap {
public:
	CalResource<T>& m_res;
	T *m_p;
	CALuint Pitch;

	CalResMap(CalResource<T>& res)
		:	m_res(res)
	{
		void *p;
		CalCheck(::calResMap(&p, &Pitch, m_res, 0));
		m_p = (T*)p;
	}

	~CalResMap() {
		CalCheck(::calResUnmap(m_res));
	}

	operator T*() { return m_p; }
};

class CalMemObj : public Object {
public:
	CALmem m_h;

	CalMemObj(CalContext& ctx, CALmem h)
		:	m_ctx(ctx)
		,	m_h(h)
	{
	}

	~CalMemObj();
private:
	CalContext& m_ctx;
};

class CalMem {
public:
	ptr<CalMemObj> m_pimpl;

	operator CALmem() const { return m_pimpl->m_h; }
};

class CalContext {
public:
	CalContext(CalDevice& dev) {
		CalCheck(::calCtxCreate(&m_h, dev));
	}

	~CalContext() {
		CalCheck(::calCtxDestroy(m_h));
	}

	operator CALcontext() {
		return m_h;
	}

	template <typename T>
	CalMem GetMem(CalResource<T>& res) {
		CALmem h;
		CalCheck(::calCtxGetMem(&h, m_h, res));
		CalMem r;
		r.m_pimpl = new CalMemObj(_self, h);
		return r;
	}
private:
	CALcontext m_h;
};


class CalObject {
public:
	CalObject()
		:	m_h(0)
	{}

	~CalObject() {
		if (m_h)
			CalCheck(::calclFreeObject(m_h));
	}

	void Compile(const CALchar* source, CALtarget target, CALlanguage language = CAL_LANGUAGE_IL);
	void Assemble(const CALchar* source, CALtarget target, CALCLprogramType typ = CAL_PROGRAM_TYPE_CS);

	operator CALobject() {
		if (!m_h)
			Throw(E_FAIL);
		return m_h;
	}
private:
	CALobject m_h;
};

class CalImage {
	typedef CalImage class_type;
public:
	CalImage()
		:	m_h(0)
	{}

	~CalImage() {
		if (m_h)
			CalCheck(::calclFreeImage(m_h));
	}

	operator CALimage() {
		if (!m_h)
			Throw(E_FAIL);
		return m_h;
	}

	void Add(CalObject& obj) {
		m_objs.push_back(obj);
	}

	void Link() {
		if (m_h)
			Throw(E_FAIL);
		CalCheck(::calclLink(&m_h, &m_objs[0], m_objs.size()));
	}

	size_t get_Size() {
		CALuint r;
		CalclCheck(::calclImageGetSize(&r, m_h));
		return r;
	}
	DEFPROP_GET(size_t, Size);

	void Read(const ConstBuf& mb) {
		if (m_h)
			Throw(E_FAIL);
		CalCheck(::calImageRead(&m_h, mb.P, mb.Size));
	}

	void Write(const Buf& mb) {
		CalCheck(::calclImageWrite(mb.P, mb.Size, m_h));
	}

	String Disassemble();
private:
	vector<CALobject> m_objs;
	CALimage m_h;
};

class CalModule {
public:
	unordered_map<String, CalMem> m_args;
	CalContext& m_ctx;

	CalModule(CalContext& ctx)
		:	m_ctx(ctx)
		,	m_h(0)
	{}

	CalModule(CalContext& ctx, CalImage& image)
		:	m_ctx(ctx)
	{
		Init(image);		
	}

	void Init(CalImage& image) {
		if (m_h)
			Throw(E_FAIL);
		CalCheck(::calModuleLoad(&m_h, m_ctx, image));
	}

	~CalModule() {
		if (m_h)
			CalCheck(::calModuleUnload(m_ctx, m_h));
	}

	void Set(RCString name, const CalMem& mem);
	void Reset(RCString name);
	CALfuncInfo GetFuncInfo(RCString name = "main");
	CALevent Run(CALdomain& domain);
	CALevent RunGrid(CALprogramGrid& pg);
	void Wait(CALevent e, int msPoll = 5);
private:
	CALmodule m_h;
};

template <class T>
struct ResourceBinder {
	CalModule& m_m;
	String m_name;
	CBool m_bBinded;
	T& m_res;

	ResourceBinder(CalModule& m, RCString name, T& res, bool)
		:	m_m(m)
		,	m_name(name)
		,	m_res(res)
	{
	}

	~ResourceBinder() {
		Unbind();
	}

	void Bind() {
		m_m.Set(m_name, m_m.m_ctx.GetMem(m_res));
		m_bBinded = true;
	}

	void Unbind() {
		if (m_bBinded) {
			m_bBinded = false;
			m_m.Reset(m_name);
		}
	}
};

}} // Ext::Gpu::

