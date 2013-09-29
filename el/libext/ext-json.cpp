/*######     Copyright (c) 1997-2013 Ufasoft  http://ufasoft.com  mailto:support@ufasoft.com,  Sergey Pavlov  mailto:dev@ufasoft.com #######################################
#                                                                                                                                                                          #
# This program is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation;  #
# either version 3, or (at your option) any later version. This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the      #
# implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details. You should have received a copy of the GNU #
# General Public License along with this program; If not, see <http://www.gnu.org/licenses/>                                                                               #
##########################################################################################################################################################################*/

#include <el/ext.h>

#if UCFG_JSON == UCFG_JSON_JANSSON
#	include <jansson.h>
#elif UCFG_JSON == UCFG_JSON_JSONCPP
#	include <json/reader.h>
#endif

namespace Ext {
using namespace std;

#if UCFG_JSON == UCFG_JSON_JANSSON

class JsonHandle {
public:
	JsonHandle(json_t *h = 0)
		:	m_h(h)
	{}

	~JsonHandle() {
		if (m_h)
			::json_decref(m_h);
	}

	JsonHandle& operator=(json_t *h) {
		if (m_h)
			::json_decref(m_h);
		m_h = h;
		return _self;
	}

	operator json_t *() { return m_h; }
	operator const json_t *() const { return m_h; }
	const json_t * get() const { return m_h; }
	json_t *Detach() { return exchange(m_h, nullptr); }
private:
	json_t *m_h;
};

class JsonVarValueObj : public VarValueObj {
public:
	JsonVarValueObj(json_t *json)
		:	m_json(json)
	{}

	bool HasKey(RCString key) const override {
		return ::json_object_get(m_json, key);
	}

	VarType type() const override {
		switch (json_typeof(m_json.get())) {
		case JSON_NULL:		return VarType::Null;
		case JSON_INTEGER:	return VarType::Int;
		case JSON_REAL:		return VarType::Float;
		case JSON_TRUE:
		case JSON_FALSE:	return VarType::Bool;
		case JSON_STRING:	return VarType::String;
		case JSON_ARRAY:	return VarType::Array;
		case JSON_OBJECT:	return VarType::Map;
		default:
			Throw(E_NOTIMPL);
		}
	}

	size_t size() const override {
		switch (type()) {
		case VarType::Array: return ::json_array_size(m_json);
		case VarType::Map: return ::json_object_size(m_json);
		default:
			Throw(E_NOTIMPL);
		}
	}

	VarValue operator[](int idx) const override {
		return FromJsonT(::json_incref(::json_array_get(m_json, idx)));
	}

	VarValue operator[](RCString key) const override {
		return FromJsonT(::json_incref(::json_object_get(m_json, key)));
	}

	String ToString() const override {
		const char *s = ::json_string_value(m_json);
		return Encoding::UTF8.GetChars(ConstBuf(s, strlen(s)));
	}

	Int64 ToInt64() const override {
		return ::json_integer_value(m_json);
	}

	bool ToBool() const override {
		if (type() != VarType::Bool)
			Throw(E_INVALIDARG);
		return json_is_true(m_json.get());
	}

	double ToDouble() const override {
		if (type() == VarType::Int)
			return double(::json_integer_value(m_json));
		else
			return ::json_real_value(m_json);
	}

	static VarValue FromJsonT(json_t *json) {
		VarValue r;
		if (json)
			r.m_pimpl = new JsonVarValueObj(json);
		return r;
	}

	void Set(int idx, const VarValue& v) override {
		Throw(E_NOTIMPL);
	}

	void Set(RCString key, const VarValue& v) override {
		Throw(E_NOTIMPL);
	}

	std::vector<String> Keys() const override {
		if (type() != VarType::Map)
			Throw(E_INVALIDARG);
		std::vector<String> r;
		for (void *it=::json_object_iter((json_t*)m_json.get()); it; it=::json_object_iter_next((json_t*)m_json.get(), it))
			r.push_back(::json_object_iter_key(it));
		return r;
	}
private:
	JsonHandle m_json;
};


class JsonExc : public Exc {
	typedef Exc base;
public:
	json_error_t m_err;

	JsonExc(const json_error_t& err)
		:	base(E_EXT_JSON_Parse)
		,	m_err(err)
	{}

	String get_Message() const override {
		return EXT_STR(m_err.text << " at line " << m_err.line);
	}
};

void JanssonCheck(bool r, const json_error_t& err) {
	if (!r)
		throw JsonExc(err);
}

VarValue AFXAPI ParseJson(RCString s) {
	json_error_t err;
	json_t *json = ::json_loads(s, 0, &err);
	JanssonCheck(json, err);
	return JsonVarValueObj::FromJsonT(json);
}

class JsonParser : public MarkupParser {
public:
	static json_t *CopyToJsonT(const VarValue& v);

	static VarValue CopyToJsonValue(const VarValue& v) {
		return JsonVarValueObj::FromJsonT(CopyToJsonT(v));
	}
protected:
	VarValue Parse(istream& is, Encoding *enc) override {
		String full;
		for (string s; getline(is, s);)
			full += s;
		return ParseJson(full);
	}

	void Print(ostream& os, const VarValue& v) override {
		JsonHandle jh(CopyToJsonT(v));
		char *s = json_dumps(jh, 0);
		os << s;
		free(s);
	}
};

json_t *JsonParser::CopyToJsonT(const VarValue& v) {
	switch (v.type()) {
	case VarType::Null:
		return ::json_null();
	case VarType::Int:
		return ::json_integer(v.ToInt64());
	case VarType::Float:
		return ::json_real(v.ToDouble());
	case VarType::String:
		return ::json_string(v.ToString());
	case VarType::Array:
		{
			JsonHandle json(::json_array());
//!!!R			VarValue r = JsonVarValueObj::FromJsonT(json);
			for (int i=0; i<v.size(); ++i)
				CCheck(::json_array_append_new(json, CopyToJsonT(v[i])));
			return json.Detach();
		}
	case VarType::Map:
		{
			JsonHandle json(::json_object());
//!!!R			VarValue r = JsonVarValueObj::FromJsonT(json);
			vector<String> keys = v.Keys();
			EXT_FOR (const String& key, keys) {
				json_t *j = CopyToJsonT(v[key]);
				VarValue vj(j);
				CCheck(::json_object_set(json, key, j));
			}
			return json.Detach();
		}
	default:
		Throw(E_NOTIMPL);

	}
}

#elif UCFG_JSON == UCFG_JSON_JSONCPP

class JsonVarValueObj : public VarValueObj {
public:
	JsonVarValueObj(Encoding& enc, const Json::Value& val)
		:	m_enc(enc)
		,	m_val(val)
	{}

	bool HasKey(RCString key) override {
		return !!m_val[(const char*)key];
	}

	VarType type() const override {
		switch (m_val.type()) {
		case Json::nullValue:		return VarType::Null;
		case Json::intValue:		return VarType::Int;
		case Json::realValue:		return VarType::Float;
		case Json::booleanValue:	return VarType::Bool;
		case Json::stringValue:		return VarType::String;
		case Json::arrayValue:		return VarType::Array;
		case Json::objectValue:		return VarType::Map;
		default:
			Throw(E_NOTIMPL);
		}
	}

	size_t size() const override {
		return m_val.size();
	}

	VarValue operator[](int idx) override {
		const Json::Value& v = m_val[idx];
		VarValue r;
		if (!v.isNull())
			r.m_pimpl = new JsonVarValueObj(m_enc, v);
		return r;
	}

	VarValue operator[](RCString key) override {
		const Json::Value& v = m_val[(const char*)key];
		VarValue r;
		if (!v.isNull())
			r.m_pimpl = new JsonVarValueObj(m_enc, v);
		return r;
	}

	String ToString() const override {
		string s = m_val.asString();
		return m_enc.GetChars(ConstBuf(s.data(), s.length()));
	}

	Int64 ToInt64() const override {
		return m_val.asInt();
	}
private:
	Encoding& m_enc;
	Json::Value m_val;
};


class JsonParser : public MarkupParser {
	VarValue Parse(istream& is, Encoding *enc) {
		Json::Value v;
		try {
			is >> v;
		} catch (const std::exception&) {
			Throw(E_FAIL);
		}
		VarValue r;
		r.m_pimpl = new JsonVarValueObj(*enc, v);
		return r;
	}
};

VarValue AFXAPI ParseJson(RCString s) {	
	string ss(s.c_str());
	istringstream is(ss);
	return MarkupParser::CreateJsonParser()->Parse(is);		
}

#endif // UCFG_JSON == UCFG_JSON_JSONCPP

ptr<MarkupParser> MarkupParser::CreateJsonParser() {
	return new JsonParser;
}


} // Ext::
