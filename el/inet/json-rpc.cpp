/*######     Copyright (c) 1997-2013 Ufasoft  http://ufasoft.com  mailto:support@ufasoft.com,  Sergey Pavlov  mailto:dev@ufasoft.com #######################################
#                                                                                                                                                                          #
# This program is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation;  #
# either version 3, or (at your option) any later version. This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the      #
# implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details. You should have received a copy of the GNU #
# General Public License along with this program; If not, see <http://www.gnu.org/licenses/>                                                                               #
##########################################################################################################################################################################*/

#include <el/ext.h>

#include "json-rpc.h"

namespace Ext { namespace Inet { 

String JsonRpcRequest::ToString() const {
	VarValue req;
	req.Set("id", Id);
	req.Set("method", Method);
	req.Set("params", Params);
	ostringstream os;
	MarkupParser::CreateJsonParser()->Print(os, req);
	return os.str();
}

bool JsonRpc::TryAsRequest(const VarValue& v, JsonRpcRequest& req) {
	if (!v.HasKey("method"))
		return false;
	req.Method = v["method"].ToString();
	req.Id = v["id"];
	req.Params = v["params"];
	return true;
}

void JsonRpc::PrepareRequest(JsonRpcRequest *req) {
	int id = m_nextId++;
	req->Id = VarValue(id);
	m_requests[id] = req;
}

String JsonRpc::Request(RCString method, const vector<VarValue>& params, ptr<JsonRpcRequest> req) {
	if (!req)
		req = new JsonRpcRequest;
	req->Method = method;
	PrepareRequest(req);
	req->Params.SetType(VarType::Array);
	for (int i=0; i<params.size(); ++i)
		req->Params.Set(i, params[i]);
	return req->ToString();
}

String JsonRpc::Request(RCString method, const CJsonNamedParams& params, ptr<JsonRpcRequest> req) {
	if (!req)
		req = new JsonRpcRequest;
	req->Method = method;
	PrepareRequest(req);
	req->Params.SetType(VarType::Map);
	for (CJsonNamedParams::const_iterator it(params.begin()), e(params.end()); it!=e; ++it)
		req->Params.Set(it->first, it->second);
	return req->ToString();
}

JsonResponse JsonRpc::Response(const VarValue& v) {
	JsonResponse r;
	r.Id = v["id"];
	if (r.Id.type() == VarType::Int) {
		CRequests::iterator it = m_requests.find((int)r.Id.ToInt64());
		if (it != m_requests.end()) {
			r.Request = it->second;
			m_requests.erase(it);
		}
	}

	if (r.Success = (!v.HasKey("error") || v["error"]==VarValue(nullptr))) {
		r.Result = v["result"];
	} else {
		VarValue er = v["error"];
		r.ErrorVal = er;
		if (er.HasKey("code"))
			r.Code = int(er["code"].ToInt64());
		if (er.HasKey("message"))
			r.JsonMessage = er["message"].ToString();
		if (er.HasKey("data"))
			r.Data = er["data"];
	}
	return r;
}




}} // Ext::Inet::


