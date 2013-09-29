/*######     Copyright (c) 1997-2013 Ufasoft  http://ufasoft.com  mailto:support@ufasoft.com,  Sergey Pavlov  mailto:dev@ufasoft.com #######################################
#                                                                                                                                                                          #
# This program is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation;  #
# either version 3, or (at your option) any later version. This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the      #
# implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details. You should have received a copy of the GNU #
# General Public License along with this program; If not, see <http://www.gnu.org/licenses/>                                                                               #
##########################################################################################################################################################################*/

#include <el/ext.h>

#include "ext-opencl.h"

#pragma comment(lib, "opencl")

namespace Ext { namespace cl {

const char * const s_errorTable[] = {
	0,						"DEVICE_NOT_FOUND",			"DEVICE_NOT_AVAILABLE",		"COMPILER_NOT_AVAILABLE",		"MEM_OBJECT_ALLOCATION_FAILURE",	"OUT_OF_RESOURCES",				"OUT_OF_HOST_MEMORY",						"PROFILING_INFO_NOT_AVAILABLE",
	"MEM_COPY_OVERLAP",		"IMAGE_FORMAT_MISMATCH",	"IMAGE_FORMAT_NOT_SUPPORTED",	"BUILD_PROGRAM_FAILURE",	"MAP_FAILURE",						"MISALIGNED_SUB_BUFFER_OFFSET",	"EXEC_STATUS_ERROR_FOR_EVENTS_IN_WAIT_LIST","COMPILE_PROGRAM_FAILURE",
	"LINKER_NOT_AVAILABLE",	"LINK_PROGRAM_FAILURE",		"DEVICE_PARTITION_FAILED",	"KERNEL_ARG_INFO_NOT_AVAILABLE",0,									0,								0,											0,
	0,						0,							0,							0,								0,									0,								"VALUE",									"DEVICE_TYPE",
	"PLATFORM",				"DEVICE",					"CONTEXT",					"QUEUE_PROPERTIES",				"COMMAND_QUEUE",					"HOST_PTR",						"MEM_OBJECT",								"IMAGE_FORMAT_DESCRIPTOR",
	"IMAGE_SIZE",			"SAMPLER",					"BINARY",					"BUILD_OPTIONS",				"PROGRAM",							"PROGRAM_EXECUTABLE",			"KERNEL_NAME",								"KERNEL_DEFINITION",
	"KERNEL",				"ARG_INDEX",				"ARG_VALUE",				"ARG_SIZE",						"KERNEL_ARGS",						"WORK_DIMENSION",				"WORK_GROUP_SIZE",							"WORK_ITEM_SIZE",
	"GLOBAL_OFFSET",		"EVENT_WAIT_LIST",			"EVENT",					"OPERATION",					"GL_OBJECT",						"BUFFER_SIZE",					"MIP_LEVEL",								"GLOBAL_WORK_SIZE",
	"PROPERTY",				"IMAGE_DESCRIPTOR",			"COMPILER_OPTIONS",			"LINKER_OPTIONS",				"DEVICE_PARTITION_COUNT"
};

String OpenclException::get_Message() const {
	UInt32 code = HRESULT_CODE(HResult);
	if (code < _countof(s_errorTable) && s_errorTable[code])
		return EXT_STR("OpenCL CL_" << (code<30 ? "" : "INVALID_") << s_errorTable[code]);
	return base::get_Message();	
}

void ClCheck(cl_int rc) {
	if (rc != CL_SUCCESS)
		throw OpenclException(MAKE_HRESULT(SEVERITY_ERROR, FACILITY_OPENCL, -rc));
}

String CommandTypeToString(cl_command_type ct) {
#if !UCFG_TRC
	return EXT_STR("CL_COMMAND " << hex << ct);
#else
	String s = "Unknown";
	switch (ct) {
	case CL_COMMAND_NDRANGE_KERNEL:			s = "NDRANGE_KERNEL";			break;
	case CL_COMMAND_TASK:					s = "TASK";						break;
	case CL_COMMAND_NATIVE_KERNEL:			s = "NATIVE_KERNEL";			break;
	case CL_COMMAND_READ_BUFFER:			s = "READ_BUFFER";				break;
	case CL_COMMAND_WRITE_BUFFER:           s = "WRITE_BUFFER";				break;
	case CL_COMMAND_COPY_BUFFER:			s = "COPY_BUFFER";				break;
	case CL_COMMAND_READ_IMAGE:             s = "READ_IMAGE";				break;
	case CL_COMMAND_WRITE_IMAGE:            s = "WRITE_IMAGE";				break;
	case CL_COMMAND_COPY_IMAGE:             s = "COPY_IMAGE";				break;
	case CL_COMMAND_COPY_IMAGE_TO_BUFFER:	s = "COPY_IMAGE_TO_BUFFER";		break;
	case CL_COMMAND_COPY_BUFFER_TO_IMAGE:   s = "COPY_BUFFER_TO_IMAGE";		break;
	case CL_COMMAND_MAP_BUFFER:    			s = "MAP_BUFFER";				break;
	case CL_COMMAND_MAP_IMAGE:              s = "MAP_IMAGE";				break;
	case CL_COMMAND_UNMAP_MEM_OBJECT:		s = "UNMAP_MEM_OBJECT";			break;
	case CL_COMMAND_MARKER:        			s = "MARKER";					break;
	case CL_COMMAND_ACQUIRE_GL_OBJECTS:		s = "ACQUIRE_GL_OBJECTS";		break;
	case CL_COMMAND_RELEASE_GL_OBJECTS:     s = "RELEASE_GL_OBJECTS";		break;
	case CL_COMMAND_READ_BUFFER_RECT:      	s = "READ_BUFFER_RECT";			break;
	case CL_COMMAND_WRITE_BUFFER_RECT:      s = "WRITE_BUFFER_RECT";		break;
	case CL_COMMAND_COPY_BUFFER_RECT:		s = "COPY_BUFFER_RECT";			break;
	case CL_COMMAND_USER:        			s = "USER";						break;
	case CL_COMMAND_BARRIER:				s = "BARRIER";					break;
	case CL_COMMAND_MIGRATE_MEM_OBJECTS:	s = "MIGRATE_MEM_OBJECTS";		break;
	case CL_COMMAND_FILL_BUFFER:     		s = "FILL_BUFFER";				break;
	case CL_COMMAND_FILL_IMAGE:				s = "FILL_IMAGE";				break;
	}
	return "CL_COMMAND_" + s;
#endif
}

static cl_uint GetNumberOfClPlatforms() {
	cl_uint n;
	int rc;
#if UCFG_WIN32
	__try {
#endif
		rc = ::clGetPlatformIDs(0, 0, &n);			//!!! sometime Access Violation under Windows
#if UCFG_WIN32
	} __except (EXCEPTION_EXECUTE_HANDLER) {
		return 0;
	}
#endif
	if (rc == CL_PLATFORM_NOT_FOUND_KHR)		// OpenCL not installed
		return 0;
	ClCheck(rc);
	return n;
}

vector<Platform> Platform::GetAll() {
	cl_uint n = GetNumberOfClPlatforms();
	if (!n)
		return vector<Platform>();
	cl_platform_id *p = (cl_platform_id*)alloca(n*sizeof(cl_platform_id));
	ClCheck(::clGetPlatformIDs(n, p, 0));
	return vector<Platform>(p, p+n);
}

String Platform::GetStringInfo(cl_platform_info param_name) const {
	size_t size;
	ClCheck(::clGetPlatformInfo(Id, param_name, 0, 0, &size));
	char *p  = (char*)alloca(size);
	ClCheck(::clGetPlatformInfo(Id, param_name, size, p, 0));
	return String(p);
}

bool Platform::get_HasRetainDeviceFunction() const {
	String sver = Version;
	return !(sver.Find("OpenCL 1.0 ") >= 0 || sver.Find("OpenCL 1.1 ") >= 0);
}

vector<Device> Platform::GetDevices(cl_device_type typ) const {
	cl_uint n;
	ClCheck(::clGetDeviceIDs(Id, typ, 0, 0, &n));
	cl_device_id *p = (cl_device_id*)alloca(n*sizeof(cl_device_id));
	ClCheck(::clGetDeviceIDs(Id, typ, n, p, 0));
	vector<Device> r(n);
	for (int i=0; i<n; ++i)
		r[i] = Device(*p, HasRetainDeviceFunction);
	return r;
}

Device::Device(cl_device_id id)
	:	m_h(id)
{
	m_bHasRetain = get_Platform().HasRetainDeviceFunction;
}


Blob Device::GetInfo(cl_device_info pname) const {
	size_t size;
	ClCheck(::clGetDeviceInfo(Handle(), pname, 0, 0, &size));
	Blob r(0, size);
	ClCheck(::clGetDeviceInfo(Handle(), pname, r.Size, r.data(), 0));
	return r;
}

ExecutionStatus Event::get_Status() {
	cl_int r;
	ClCheck(::clGetEventInfo(Handle(), CL_EVENT_COMMAND_EXECUTION_STATUS, sizeof r, &r, 0));
	return ExecutionStatus(r);
}

int Event::get_RefCount() {
	cl_uint r;
	ClCheck(::clGetEventInfo(Handle(), CL_EVENT_REFERENCE_COUNT, sizeof r, &r, 0));
	return r;
}

cl_command_type Event::get_CommandType() {
	cl_command_type r;
	ClCheck(::clGetEventInfo(Handle(), CL_EVENT_COMMAND_TYPE, sizeof r, &r, 0));
	return r;
}

UInt64 Event::GetProfilingInfo(cl_event_info name) {
	cl_ulong r;
	ClCheck(::clGetEventProfilingInfo(Handle(), name, sizeof r, &r, 0));
	return r;
}

void Event::setCallback(void (CL_CALLBACK *pfn)(cl_event, cl_int, void *), void *userData) {
	ClCheck(::clSetEventCallback(Handle(), CL_COMPLETE, pfn, userData));
}

void Event::wait() {
	ClCheck(::clWaitForEvents(1, &m_h));
}

static cl_uint NumEvents(const vector<Event> *events) {
	return events ? events->size() : 0;
}

static const cl_event *ToEventWaitList(const vector<Event> *events) {
	return events && !events->empty() ? &events->front().m_h : 0;
}

CommandQueue::CommandQueue(const Context& ctx, const Device& dev, cl_command_queue_properties prop) {
	cl_int rc;
	OutRef() = ::clCreateCommandQueue(ctx(), dev(), prop, &rc);
	ClCheck(rc);
}

Event CommandQueue::enqueueTask(const Kernel& kernel, const vector<Event> *events) {
	Event ev;
	ClCheck(::clEnqueueTask(Handle(), kernel(), NumEvents(events), ToEventWaitList(events), &ev.m_h));
	return ev;
}

Event CommandQueue::enqueueNDRangeKernel(const Kernel& kernel, const NDRange& offset, const NDRange& global, const NDRange& local, const vector<Event> *events) {
	Event ev;
	ClCheck(::clEnqueueNDRangeKernel(Handle(), kernel(), global.dimensions(), offset, global, local, NumEvents(events), ToEventWaitList(events), &ev.m_h));
	return ev;
}

Event CommandQueue::enqueueNativeKernel(void (CL_CALLBACK *pfn)(void *), pair<void*, size_t> args, const vector<Memory> *mems, const vector<const void*> *memLocs, const vector<Event> *events) {
	Event ev;
	ClCheck(::clEnqueueNativeKernel(Handle(), pfn, args.first, args.second, (mems ? mems->size() : 0), (mems ? &(*mems)[0].m_h : 0), (memLocs ? (const void **)&(*memLocs)[0]: 0), NumEvents(events), ToEventWaitList(events), &ev.m_h));
	return ev;
}

Event CommandQueue::enqueueReadBuffer(const Buffer& buffer, bool bBlocking, size_t offset, size_t cb, void *p, const vector<Event> *events) {
	Event ev;
	ClCheck(::clEnqueueReadBuffer(Handle(), buffer(), bBlocking, offset, cb, p, NumEvents(events), ToEventWaitList(events), &ev.m_h));
	return ev;
}

Event CommandQueue::enqueueWriteBuffer(const Buffer& buffer, bool bBlocking, size_t offset, size_t cb, const void *p, const vector<Event> *events) {
	Event ev;
	ClCheck(::clEnqueueWriteBuffer(Handle(), buffer(), bBlocking, offset, cb, p, NumEvents(events), ToEventWaitList(events), &ev.m_h));
	return ev;
}

Event CommandQueue::enqueueMarkerWithWaitList(const vector<Event> *events) {
	Event ev;
	ClCheck(::clEnqueueMarkerWithWaitList(Handle(), NumEvents(events), ToEventWaitList(events), &ev.m_h));
	return ev;
}

Event CommandQueue::enqueueBarrierWithWaitList(const vector<Event> *events) {
	Event ev;
	ClCheck(::clEnqueueBarrierWithWaitList(Handle(), NumEvents(events), ToEventWaitList(events), &ev.m_h));
	return ev;
}

Context::~Context() {
}

Context::Context(const Device& dev, const CContextProps *props) {
	cl_context_properties *p = props ? (cl_context_properties*)alloca((props->size()*2+1)*sizeof(cl_context_properties)) : 0;
	int i = 0;
	if (props) {
		for (auto it=props->begin(); it!=props->end(); ++it, i+=2) {
			p[i] = it->first;
			p[i+1] = it->second;
		}
		p[i] = 0;
	}
	cl_int rc;
	cl_device_id id = dev();
	OutRef() = ::clCreateContext(p, 1, &id, 0, 0, &rc);
	if (!m_h)
		ClCheck(rc);
}

void Context::CreateFromType(const CContextProps& props, cl_device_type typ) {
	cl_context_properties *p = (cl_context_properties*)alloca((props.size()*2+1)*sizeof(cl_context_properties));
	int i = 0;
	for (auto it=props.begin(); it!=props.end(); ++it, i+=2) {
		p[i] = it->first;
		p[i+1] = it->second;
	}
	p[i] = 0;
	cl_int rc;
	OutRef() = ::clCreateContextFromType(p, typ, 0, 0, &rc);
	if (!m_h)
		ClCheck(rc);
}

vector<Device> Context::get_Devices() const {
	cl_uint n;
	ClCheck(::clGetContextInfo(Handle(), CL_CONTEXT_NUM_DEVICES, sizeof n, &n, 0));
	cl_device_id *p = (cl_device_id*)alloca(n*sizeof(cl_device_id));
	ClCheck(::clGetContextInfo(Handle(), CL_CONTEXT_DEVICES, n*sizeof(cl_device_id), p, 0));
	return vector<Device>(p, p+n);
}

Buffer::Buffer(const Context& ctx, cl_mem_flags flags, size_t size, void *host_ptr) {
	cl_int rc;
	OutRef() = ::clCreateBuffer(ctx(), flags, size, host_ptr, &rc);
	ClCheck(rc);
}

Kernel::Kernel(const Program& prog, RCString name) {
	cl_int rc;
	m_h = ::clCreateKernel(prog(), name, &rc);
	ClCheck(rc);
}

Program::Program(const Context& ctx, RCString s, bool bBuild) {
//!!!R	Ctx = &ctx;
	vector<const char*> pp(1, s);
	cl_int rc;
	OutRef() = ::clCreateProgramWithSource(ctx(), 1, &pp[0], 0, &rc);
	ClCheck(rc);
	if (bBuild)
		build();
}

Program Program::FromBinary(Context& ctx, const Device& dev, const ConstBuf& mb) {
	cl_int st, rc;
	Program r;
	const byte *p = mb.P;
	cl_device_id id = dev();
	r.OutRef() = ::clCreateProgramWithBinary(ctx(), 1, &id, &mb.Size, &p, &st, &rc);
	ClCheck(rc);
	return r;
}

vector<Device> Program::get_Devices() {
	cl_uint n;
	ClCheck(::clGetProgramInfo(Handle(), CL_PROGRAM_NUM_DEVICES, sizeof(n), &n, 0));
	vector<Device> devs(n);
	if (n) {
		cl_device_id *devids = (cl_device_id*)alloca(n * sizeof(cl_device_id));
		ClCheck(::clGetProgramInfo(Handle(), CL_PROGRAM_DEVICES, sizeof(cl_device_id)*n, devids, 0));
		for (int i=0; i<n; ++i)
			devs[i] = Device(devids[i]);
	}
	return devs;
}

vector<ProgramBinary> Program::get_Binaries() {
	vector<ProgramBinary> r;

	vector<Device> devs = Devices;
	if (cl_uint n = devs.size()) {
		vector<size_t> vSize(n);
		ClCheck(::clGetProgramInfo(Handle(), CL_PROGRAM_BINARY_SIZES, sizeof(size_t)*n, &vSize[0], 0));
		r.resize(n);
		vector<void*> vp(n);
		for (int i=0; i<n; ++i) {
			r[i].Device = devs[i];
			r[i].Binary.Size = vSize[i];
			vp[i] = r[i].Binary.data();
		}
		ClCheck(::clGetProgramInfo(Handle(), CL_PROGRAM_BINARIES,	sizeof(void*)*n, &vp[0], 0));
	}
	return r;
}

void Program::build() { 
	String sOptions = nullptr;
	if (!Options.empty()) {
		ostringstream os;
		for (COptions::iterator it=Options.begin(), e=Options.end(); it!=e; ++it) {
			os << " -D " << it->first;
			if (it->second != nullptr)
				os << "=" << it->second;
		}
		sOptions = os.str();
	}
	cl_int rc = ::clBuildProgram(Handle(), 0, 0, sOptions, 0, 0);
	if (rc == CL_SUCCESS) // && || != CL_BUILD_PROGRAM_FAILURE)
		ClCheck(rc);
	else {
		vector<Device> devs = Devices;
		cl_build_status st;
		const Device& dev = devs[0];				//!!!TODO
		ClCheck(::clGetProgramBuildInfo(Handle(), dev(), CL_PROGRAM_BUILD_STATUS, sizeof st, &st, 0));
		if (st == CL_SUCCESS)
			Throw(E_FAIL); //!!!
		size_t size;
		ClCheck(::clGetProgramBuildInfo(Handle(), dev(), CL_PROGRAM_BUILD_LOG, 0, 0, &size));
		vector<char> buf(size);
		ClCheck(::clGetProgramBuildInfo(Handle(), dev(), CL_PROGRAM_BUILD_LOG, size, &buf[0], 0));
		throw BuildException(MAKE_HRESULT(SEVERITY_ERROR, FACILITY_OPENCL, -rc), &buf[0]);
	}
}

void Program::build(const vector<Device>& devs) {
	String sOptions = nullptr;
	if (!Options.empty()) {
		ostringstream os;
		for (COptions::iterator it=Options.begin(), e=Options.end(); it!=e; ++it) {
			os << " -D " << it->first;
			if (it->second != nullptr)
				os << "=" << it->second;
		}
		sOptions = os.str();
	}
	cl_device_id *devids = !devs.empty() ? (cl_device_id*)alloca(devs.size() * sizeof(cl_device_id)) : 0;
	for (int i=0; i<devs.size(); ++i)
		devids[i] = devs[i]();
	cl_int rc = ::clBuildProgram(Handle(), devs.size(), devids, sOptions, 0, 0);
	if (rc == CL_SUCCESS) // && || != CL_BUILD_PROGRAM_FAILURE)
		ClCheck(rc);
	else {
		cl_build_status st;
		const Device& dev = devs[0];				//!!!TODO
		ClCheck(::clGetProgramBuildInfo(Handle(), dev(), CL_PROGRAM_BUILD_STATUS, sizeof st, &st, 0));
		if (st == CL_SUCCESS)
			Throw(E_FAIL); //!!!
		size_t size;
		ClCheck(::clGetProgramBuildInfo(Handle(), dev(), CL_PROGRAM_BUILD_LOG, 0, 0, &size));
		vector<char> buf(size);
		ClCheck(::clGetProgramBuildInfo(Handle(), dev(), CL_PROGRAM_BUILD_LOG, size, &buf[0], 0));
		throw BuildException(MAKE_HRESULT(SEVERITY_ERROR, FACILITY_OPENCL, -rc), &buf[0]);
	}
}


}} // Ext::cl::

