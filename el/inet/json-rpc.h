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

class JsonRpcExc : public Exc {
	typedef Exc base;
public:
	int Code;
	String JsonMessage;
	VarValue Data;

	JsonRpcExc(int code)
		:	base(MAKE_HRESULT(SEVERITY_ERROR, FACILITY_JSON_RPC, UInt16(code)))
		,	Code(code)
	{}

	String get_Message() const override { return JsonMessage; }
};

class JsonRpcRequest : public Object {
public:
	VarValue Id;
	String Method;
	VarValue Params;

	String ToString() const;
};

class JsonResponse {
public:
	VarValue Id;
	bool Success;
	VarValue Result;
	VarValue ErrorVal;

	int Code;
	String JsonMessage;
	VarValue Data;

	ptr<JsonRpcRequest> Request;

	JsonResponse()
		:	Success(true)
		,	Code(0)
	{}
};

class JsonRpc {
	typedef unordered_map<int, ptr<JsonRpcRequest>> CRequests;
	CRequests m_requests;
public:
	int m_nextId;

	JsonRpc()
		:	m_nextId(1)
	{}

	static bool TryAsRequest(const VarValue& v, JsonRpcRequest& req);
	String Request(RCString method, const vector<VarValue>& params = vector<VarValue>(), ptr<JsonRpcRequest> req = nullptr);
	String Request(RCString method, const CJsonNamedParams& params, ptr<JsonRpcRequest> req = nullptr);
	JsonResponse Response(const VarValue& v);
private:
	void PrepareRequest(JsonRpcRequest *req);
};






}} // Ext::Inet::

