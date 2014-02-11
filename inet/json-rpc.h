/*######     Copyright (c) 1997-2013 Ufasoft  http://ufasoft.com  mailto:support@ufasoft.com,  Sergey Pavlov  mailto:dev@ufasoft.com #######################################
#                                                                                                                                                                          #
# This program is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation;  #
# either version 3, or (at your option) any later version. This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the      #
# implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details. You should have received a copy of the GNU #
# General Public License along with this program; If not, see <http://www.gnu.org/licenses/>                                                                               #
##########################################################################################################################################################################*/

#pragma once

namespace Ext { namespace Inet {
using namespace Ext;

typedef unordered_map<String, VarValue> CJsonNamedParams;

class JsonRpcExc : public Exception {
	typedef Exception base;
public:
	int Code;
	VarValue Data;

	JsonRpcExc(int code)
		:	base(MAKE_HRESULT(SEVERITY_ERROR, FACILITY_JSON_RPC, UInt16(code)))
		,	Code(code)
	{}
};

class JsonRpcRequest : public Object {
public:
	typedef Interlocked interlocked_policy;

	VarValue Id;
	String Method;
	VarValue Params;

	String ToString() const;
};

class JsonResponse {
public:
	ptr<JsonRpcRequest> Request;

	VarValue Id;
	VarValue Result;
	VarValue ErrorVal;
	VarValue Data;

	String JsonMessage;
	int Code;
	bool Success;

	JsonResponse()
		:	Success(true)
		,	Code(0)
	{}

	String ToString() const;
};

class JsonRpc {
public:
	JsonRpc()
		:	m_nextId(1)
	{}

	static bool TryAsRequest(const VarValue& v, JsonRpcRequest& req);
	String Request(RCString method, const vector<VarValue>& params = vector<VarValue>(), ptr<JsonRpcRequest> req = nullptr);
	String Request(RCString method, const CJsonNamedParams& params, ptr<JsonRpcRequest> req = nullptr);
	String Notification(RCString method, const vector<VarValue>& params = vector<VarValue>(), ptr<JsonRpcRequest> req = nullptr);
	JsonResponse Response(const VarValue& v);
private:
	std::mutex MtxReqs;

	typedef LruMap<int, ptr<JsonRpcRequest>> CRequests;
	CRequests m_reqs;

	volatile Int32 m_nextId;

	void PrepareRequest(JsonRpcRequest *req);
};






}} // Ext::Inet::

