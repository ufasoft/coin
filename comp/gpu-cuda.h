/*######     Copyright (c) 1997-2013 Ufasoft  http://ufasoft.com  mailto:support@ufasoft.com,  Sergey Pavlov  mailto:dev@ufasoft.com #######################################
#                                                                                                                                                                          #
# This program is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation;  #
# either version 3, or (at your option) any later version. This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the      #
# implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details. You should have received a copy of the GNU #
# General Public License along with this program; If not, see <http://www.gnu.org/licenses/>                                                                               #
##########################################################################################################################################################################*/

#pragma once

#include <nvidia/cuda.h>

namespace Ext { 

namespace Gpu {

void CuCheck(CUresult rc);

}

template<> struct HandleTraits<CUcontext> : HandleTraitsBase<CUcontext> {
	static void ResourceRelease(const handle_type& h) {
		Gpu::CuCheck(::cuCtxDestroy(h));
	}
};

template<> struct HandleTraits<CUmodule> : HandleTraitsBase<CUmodule> {
	static void ResourceRelease(const handle_type& h) {
		Gpu::CuCheck(::cuModuleUnload(h));
	}

	static CUmodule Retain(const CUmodule& h) { Throw(E_NOTIMPL); }
};

namespace Gpu {

class CuException : public Exc {
	typedef Exc base;
public:
	CuException(HRESULT hr, RCString s)
		:	base(hr, s)
	{}
};

class CuContextObj : public Object, public ResourceWrapper<CUcontext> {
public:
};

class CuContext {
public:
	operator CUcontext() { return m_pimpl->m_h; }

	static void Synchronize() {
		CuCheck(::cuCtxSynchronize());
	}
private:
	ptr<CuContextObj> m_pimpl;

	friend class CuDevice;
};

class CuKernel {
public:
	CUfunction m_h;

	void ParamSetv(int offset, void *p, size_t size);

	template <typename T>
	void ParamSetv(int offset, T& v) {
		ParamSetv(offset, &v, sizeof v);
	}

	void ParamSetSize(size_t size);
	void SetBlockShape(int x, int y = 1, int z = 1);
	void Launch();
	void Launch(int width, int height);
};

class CuModuleObj : public Object, public ResourceWrapper<CUmodule> {
public:
};

class CuModule {
public:
	operator CUmodule() { return m_pimpl->m_h; }
	static CuModule FromFile(RCString filename);
	static CuModule FromData(const void *image);
	CuKernel GetFunction(RCString name);
private:
	ptr<CuModuleObj> m_pimpl;
};


typedef std::pair<int, int> CuVersion;

class CuDevice {
	typedef CuDevice class_type;
public:
	CUdevice m_dev;
	
	String get_Name();
	DEFPROP_GET(String, Name);

	CuVersion get_ComputeCapability();
	DEFPROP_GET(CuVersion, ComputeCapability);

	int GetAttribute(CUdevice_attribute attrib);

#define DEF_CU_DEVPROP(name, attr) int get_##name() { return GetAttribute(attr); } DEFPROP_GET(int, name);

	DEF_CU_DEVPROP(MultiprocessorCount, CU_DEVICE_ATTRIBUTE_MULTIPROCESSOR_COUNT);
	DEF_CU_DEVPROP(MaxThreadsPerBlock, CU_DEVICE_ATTRIBUTE_MAX_THREADS_PER_BLOCK);
	DEF_CU_DEVPROP(MaxGridDimX, CU_DEVICE_ATTRIBUTE_MAX_GRID_DIM_X);

	CuContext CreateContext(UInt32 flags = 0);
};

class CuEngine {
	typedef CuEngine class_type;
public:
	CuEngine(bool bInit = true) {
		if (bInit)
			Init();
	}

	void Init();

	int DeviceCount();
	CuDevice GetDevice(int idx);

	int get_Version();
	DEFPROP_GET(int, Version);
};

class CuBuffer {
public:
	CUdeviceptr m_h;
	
	CuBuffer()
		:	m_h(0)
	{}

	void Alloc(size_t size) {
		if (m_h)
			Throw(E_FAIL);
		CuCheck(::cuMemAlloc(&m_h, size));
	}

	~CuBuffer() {
		if (m_h)
			CuCheck(::cuMemFree(m_h));
	}

	void FromHostMem(const void *p, size_t size) {
		CuCheck(::cuMemcpyHtoD(m_h, p, size));
	}

	void ToHostMem(void *p, size_t size) {
		CuCheck(::cuMemcpyDtoH(p, m_h, size));
	}
};

}} // Ext::Gpu::

