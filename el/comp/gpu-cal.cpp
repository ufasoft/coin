/*######     Copyright (c) 1997-2013 Ufasoft  http://ufasoft.com  mailto:support@ufasoft.com,  Sergey Pavlov  mailto:dev@ufasoft.com #######################################
#                                                                                                                                                                          #
# This program is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation;  #
# either version 3, or (at your option) any later version. This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the      #
# implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details. You should have received a copy of the GNU #
# General Public License along with this program; If not, see <http://www.gnu.org/licenses/>                                                                               #
##########################################################################################################################################################################*/

#include <el/ext.h>

#include "gpu-cal.h"

#ifdef _WIN64
#	pragma comment(lib, "aticalrt64")
#	pragma comment(lib, "aticalcl64")
#else
#	pragma comment(lib, "aticalrt")
#	pragma comment(lib, "aticalcl")
#endif

namespace Ext { namespace Gpu {

void CalCheck(CALresultEnum v) {
	if (v == CAL_RESULT_OK)
		return;
	throw CalException(MAKE_HRESULT(SEVERITY_ERROR, FACILITY_GPU_CAL, v), String("CAL: ") + ::calGetErrorString());
}

void CalclCheck(CALresultEnum v) {
	if (v == CAL_RESULT_OK)
		return;
	throw CalException(MAKE_HRESULT(SEVERITY_ERROR, FACILITY_GPU_CAL, v), String("CAL compiler: ") + ::calclGetErrorString());
}

CalDeviceObj::~CalDeviceObj() {
//	CalCheck(::calDeviceClose(m_h));
	Remove(Engine.m_devices, this);
}

CALdevicestatus CalDevice::get_Status() {
	CALdevicestatus r = { sizeof r };
	CalCheck(::calDeviceGetStatus(&r, _self));
	return r;
}

CalEngine::CalEngine() {
	CalCheck(::calInit());
}

CalEngine::~CalEngine() {
	CalCheck(::calShutdown());
}

UInt64 CalEngine::GetVersion() {
	CALuint mj, mn, imp;
	CalCheck(::calGetVersion(&mj, &mn, &imp));
	return (UInt64(UInt16(mj)) << 48) | (UInt64(UInt16(mn))<<32) | imp;
}

int CalEngine::DeviceCount() {
	CALuint count;
	CalCheck(::calDeviceGetCount(&count));
	return count;
}

CALdeviceattribs CalEngine::GetDeviceAttribs(int idx) {
	CALdeviceattribs r = { sizeof r };
	CalCheck(::calDeviceGetAttribs(&r, idx));
	return r;
}

CalDevice CalEngine::GetDevice(int idx) {
	if (idx >= m_devices.size() || !m_devices[idx]) {
		if (idx >= m_devices.size())
			m_devices.resize(idx+1);
		CALdevice h;
		CalCheck(::calDeviceOpen(&h, idx));
		m_devices[idx] = new CalDeviceObj(_self, h);
	}
	CalDevice r;
	r.m_pimpl = m_devices[idx];
	r.Index = idx;
	return r;
}

CalMemObj::~CalMemObj() {
	CalCheck(::calCtxReleaseMem(m_ctx, m_h));
}

void CalObject::Compile(const CALchar* source, CALtarget target, CALlanguage language) {
	if (m_h)
		Throw(E_FAIL);
	CalclCheck(::calclCompile(&m_h, language, source, target));
}

void CalObject::Assemble(const CALchar* source, CALtarget target, CALCLprogramType typ) {
	if (m_h)
		Throw(E_FAIL);
	CalclCheck(::calclAssembleObject(&m_h, typ, source, target));
}

void CalModule::Set(RCString name, const CalMem& mem) {
	CALname n;
	CalCheck(::calModuleGetName(&n, m_ctx, m_h, name));
	CalCheck(::calCtxSetMem(m_ctx, n, mem));
	m_args[name] = mem;
}

void CalModule::Reset(RCString name) {
	CALname n;
	CalCheck(::calModuleGetName(&n, m_ctx, m_h, name));
	CalCheck(::calCtxSetMem(m_ctx, n, 0));
	m_args.erase(name);
}

CALfuncInfo CalModule::GetFuncInfo(RCString name) {
	CALfunc func;
	CalCheck(::calModuleGetEntry(&func, m_ctx, m_h, name));
	CALfuncInfo r;
	CalCheck(::calModuleGetFuncInfo(&r, m_ctx, m_h, func));
	return r;
}

CALevent CalModule::Run(CALdomain& domain) {
	CALfunc func;
	CalCheck(::calModuleGetEntry(&func, m_ctx, m_h, "main"));
	CALevent e;
	CalCheck(::calCtxRunProgram(&e, m_ctx, func, &domain));
	return e;
}

CALevent CalModule::RunGrid(CALprogramGrid& pg) {
	CALfunc func;
	CalCheck(::calModuleGetEntry(&func, m_ctx, m_h, "main"));
	CALevent e;
	pg.func = func;

    static PFNCALCTXRUNPROGRAMGRID pfncalCtxRunProgramGrid = 0;
    if (!pfncalCtxRunProgramGrid)
        CalCheck(::calExtGetProc((CALextproc*)&pfncalCtxRunProgramGrid, CAL_EXT_COMPUTE_SHADER, "calCtxRunProgramGrid"));


	CalCheck(pfncalCtxRunProgramGrid(&e, m_ctx, &pg));
	return e;
}

void CalModule::Wait(CALevent e, int msPoll) {
	while (true) {
		CALresult rc = calCtxIsEventDone(m_ctx, e);
		if (rc == 0)
			break;
		if (rc != CAL_RESULT_PENDING)
			CalCheck(rc);
		Thread::Sleep(msPoll);
	}
}

static String s_result;

static void __cdecl DisasmLogFunction(const char* msg) {
	s_result += msg;
}

String CalImage::Disassemble() {
	s_result = "";
	::calclDisassembleImage(_self, &DisasmLogFunction);
	return exchange(s_result, String());
}

}} // Ext::Gpu::


