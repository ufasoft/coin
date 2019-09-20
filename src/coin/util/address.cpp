/*######   Copyright (c) 2013-2019 Ufasoft  http://ufasoft.com  mailto:support@ufasoft.com,  Sergey Pavlov  mailto:dev@ufasoft.com ####
#                                                                                                                                     #
# 		See LICENSE for licensing information                                                                                         #
#####################################################################################################################################*/

#include <el/ext.h>

#include <el/bignum.h>
#include <el/crypto/hash.h>
using namespace Crypto;

#include "util.h"
#include "opcode.h"

namespace Coin {

AddressObj::AddressObj(HasherEng& hasher)
	: Hasher(hasher)
	, Type(AddressType::P2SH)
{
}

AddressObj::AddressObj(HasherEng& hasher, AddressType typ, RCSpan data, uint8_t witnessVer, RCString comment)
	: Hasher(hasher)
	, Type(typ)
	, WitnessVer(witnessVer)
	, Comment(comment)
{
	if (data.size() > Data.max_size())
		Throw(E_FAIL);
	Data.resize(data.size());
	memcpy(Data.data(), data.data(), data.size());
}

AddressObj::AddressObj(HasherEng& hasher, RCString s, RCString comment)
	: Hasher(hasher)
	, Type(AddressType::Legacy)
	, Comment(comment)
{
	try {
		if (s.StartsWith(hasher.Hrp))
			DecodeBech32(hasher.Hrp, s);
		else {
			Blob blob = ConvertFromBase58(s.Trim());
			if (blob.size() < 21)
				Throw(CoinErr::InvalidAddress);
			uint8_t ver = blob.constData()[0];
			if (ver == Hasher.AddressVersion)
				Type = AddressType::P2PKH;
			else if (ver == Hasher.ScriptAddressVersion)
				Type = AddressType::P2SH;
			else
				Throw(CoinErr::InvalidAddress);
			Data.resize(20);
			memcpy(Data.data(), blob.constData() + 1, 20);
		}
	} catch (RCExc) {
		Throw(CoinErr::InvalidAddress);
	}
	CheckVer(hasher);
}

Address& Address::operator=(const Address& a) {
	if (&m_pimpl->Hasher != &a->Hasher)
		Throw(errc::invalid_argument);
	m_pimpl = new AddressObj(*a.m_pimpl);
 	return *this;
 }


void AddressObj::CheckVer(HasherEng& hasher) const {
	if (&Hasher != &hasher)
		Throw(CoinErr::InvalidAddress);
}

Address::operator HashValue160() const {
	if (m_pimpl->Data.size() != 20)
		Throw(E_FAIL);
	return HashValue160(Span(m_pimpl->Data.data(), 20));
}

Address::operator HashValue() const {
	if (m_pimpl->Data.size() != 32)
		Throw(E_FAIL);
	return HashValue(Span(m_pimpl->Data.data(), 32));
}

static const char s_base32ToChar[33] = "qpzry9x8gf2tvdw0s3jn54khce6mua7l";

static uint8_t s_base32FromChar[256];

static bool InitBase32FromChar() {
	for (uint8_t i = 0; i < 32; ++i) {
		char ch = s_base32ToChar[i];
		s_base32FromChar[ch] = s_base32FromChar[toupper(ch)] = i;
	}
	return true;
}

static bool s_initBase32FromChar = InitBase32FromChar();

static int Bech32Polymod(RCSpan cb) {
	static const int GEN[5] = {0x3b6a57b2, 0x26508e6d, 0x1ea119fa, 0x3d4233dd, 0x2a1462b3};
	int chk = 1;
	for (int i = 0; i < cb.size(); ++i) {
		int b = chk >> 25;
		chk = ((chk & 0x1ffffff) << 5) ^ cb[i];
		for (int j = 0; j < 5; ++j)
			chk ^= b & (1 << j) ? GEN[j] : 0;
	}
	return chk;
}

static void FillBech32Checksum(uint8_t buf[], size_t len) {
	buf[len - 1] = buf[len - 2] = buf[len - 3] = buf[len - 4] = buf[len - 5] = buf[len - 6] = 0;
	int chk = Bech32Polymod(ConstBuf(buf, len)) ^ 1;
	for (int i = 6; i--;)
		buf[len - 6 + i] = uint8_t((chk >> 5 * (5 - i)) & 31);
}

size_t ExpandHrp(uint8_t buf[], RCString hrp) {
	auto hrpLen = hrp.length();
	for (auto i = hrpLen; i--;) {
		auto ch = (uint8_t)hrp[i];
		buf[i] = ch >> 5;
		buf[hrpLen + 1 + i] = ch & 31;
	}
	buf[hrpLen] = 0;
	return hrpLen;
}

Blob AddressObj::ToScriptPubKey() const {
	switch (Type) {
	case AddressType::P2PKH: {
		uint8_t ar[25] = { (uint8_t)Opcode::OP_DUP, (uint8_t)Opcode::OP_HASH160, 20, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, (uint8_t)Opcode::OP_EQUALVERIFY, (uint8_t)Opcode::OP_CHECKSIG };
		memcpy(ar + 3, Data.data(), 20);
		return Blob(ar, sizeof ar);
	}
	case AddressType::P2SH: {
		uint8_t ar[23] = { (uint8_t)Opcode::OP_HASH160, 20, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, (uint8_t)Opcode::OP_EQUAL };
		memcpy(ar + 2, Data.data(), 20);
		return Blob(ar, sizeof ar);
	}
	case AddressType::Bech32: {
		uint8_t ar[42] = { uint8_t(WitnessVer ? (uint8_t(Opcode::OP_1) + WitnessVer - 1) : (int)Opcode::OP_0), uint8_t(Data.size()) };
		memcpy(ar + 2, Data.data(), Data.size());
		return Blob(ar, 2 + Data.size());
	}
	default:
		Throw(E_NOTIMPL);
	}
}

String AddressObj::ToString() const {
	CHasherEngThreadKeeper hasherKeeper(&Hasher);

	array<uint8_t, 21> v;
	switch (Type) {
	case AddressType::P2PKH:
		v[0] = Hasher.AddressVersion;
		memcpy(&v[1], Data.data(), 20);
		return ConvertToBase58(Span(v));
	case AddressType::P2SH:
		v[0] = Hasher.ScriptAddressVersion;
		memcpy(&v[1], Data.data(), 20);
		return ConvertToBase58(Span(v));
	case AddressType::WitnessV0KeyHash:
	case AddressType::WitnessV0ScriptHash:
	case AddressType::Bech32: {
		uint8_t buf[Address::MAX_LEN * 3]; // enough for max Hrp & Data
		auto hrpLen = ExpandHrp(buf, Hasher.Hrp);
		buf[hrpLen * 2 + 1] = WitnessVer;
		String sdata = Convert::ToBase32String(Span(Data.data(), Data.size()), s_base32ToChar);
		for (size_t i = sdata.length(); i--;)
			buf[hrpLen * 2 + 2 + i] = s_base32FromChar[sdata[i]];
		FillBech32Checksum(buf, hrpLen * 2 + 2 + sdata.length() + 6);
		char sbuf[100] = { Hasher.HrpSeparator };
		for (size_t i = 1 + sdata.length() + 6; i--;)
			sbuf[i + 1] = s_base32ToChar[buf[hrpLen * 2 + 1 + i]];
		return Hasher.Hrp + String(sbuf, 2 + sdata.length() + 6);
	}
	default:
		Throw(E_NOTIMPL);
	}
}

static regex
	s_reBech32AdrressOne(  "^([-!\"#$%&\'()*+,./:;<=>?@\\[\\]\\\\^_`{|}~a-z0-1]{1,83})1([02-9ac-hj-np-z]{6,})$"),
	s_reBech32AdrressColon("^([-!\"#$%&\'()*+,./:;<=>?@\\[\\]\\\\^_`{|}~a-z0-1]{1,83}):([02-9ac-hj-np-z]{6,})$");

// BIP173
void AddressObj::DecodeBech32(RCString hrp, RCString s) {
	if (s.length() > Address::MAX_LEN)
		Throw(CoinErr::InvalidAddress);
	char lower[Address::MAX_LEN + 1];
	bool bHasLower = false, bHasUpper = false;
	for (int i = s.length(); i--;) {
		wchar_t ch = s[i];
		if (ch < 33 || ch > 126)
			Throw(CoinErr::InvalidAddress);
		bHasLower |= (bool)islower(ch);
		bHasUpper |= (bool)isupper(ch);
		lower[i] = (char)tolower(ch);
	}
	if (bHasLower && bHasUpper)
		Throw(CoinErr::InvalidAddress);
	lower[s.length()] = 0;
	regex* pre = Hasher.HrpSeparator == '1' ? &s_reBech32AdrressOne
		: Hasher.HrpSeparator == ':' ? &s_reBech32AdrressColon
		: 0;
	if (!pre)
		Throw(E_NOTIMPL);
	cmatch m;
	if (!regex_match(lower, m, *pre) || String(m[1]) != hrp)
		Throw(CoinErr::InvalidAddress);
	String data = String(m[2]);
	uint8_t buf[Address::MAX_LEN * 3]; // enough for max Hrp & Data
	auto hrpLen = ExpandHrp(buf, hrp);
	for (int i = data.length(); i--;)
		buf[hrpLen * 2 + 1 + i] = s_base32FromChar[data[i]];
	if (1 != Bech32Polymod(ConstBuf(buf, hrpLen * 2 + 1 + data.length())))
		Throw(CoinErr::InvalidAddress);
	if (data.length() < 11)
		Throw(CoinErr::InvalidAddress);
	if ((WitnessVer = buf[hrpLen * 2 + 1]) > 16)
		Throw(CoinErr::InvalidAddress);
	Blob witnessProgram = Convert::FromBase32String(data.substr(1, data.length() - 7), s_base32FromChar);
	if (witnessProgram.size() > MAX_WITNESS_PROGRAM)
		Throw(CoinErr::InvalidAddress);
	if (WitnessVer == 0 && witnessProgram.size() != 20 && witnessProgram.size() != 32)
		Throw(CoinErr::InvalidAddress);
	Data.resize(witnessProgram.size());
	memcpy(Data.data(), witnessProgram.constData(), Data.size());
	Type = AddressType::Bech32;
}

} // namespace Coin
