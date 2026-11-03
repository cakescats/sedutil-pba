/* C:B**************************************************************************
This software is Copyright 2026 CakesCats

This file is part of sedutil.

sedutil is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

sedutil is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with sedutil.  If not, see <http://www.gnu.org/licenses/>.

 * C:E********************************************************************** */
#include "PbaSso.h"
#include <fstream>
#include <cctype>
#include <cstring>

extern "C" {
#include "hmac.h"
#include "sha2.h"
}

using namespace std;

/* Bump together with the unlock server (cakescats/sedutil-opal-enterprise,
 * server/unlock.py) when the token format changes */
static const char SSO_CONTEXT[] = "cakes-pba-sso-v1";

static string base32(const uint8_t *data, size_t len)
{
	static const char alphabet[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ234567";
	string out;
	unsigned int buffer = 0, bits = 0;
	for (size_t i = 0; i < len; i++) {
		buffer = (buffer << 8) | data[i];
		bits += 8;
		while (bits >= 5) {
			out += alphabet[(buffer >> (bits - 5)) & 31];
			bits -= 5;
		}
	}
	if (bits) out += alphabet[(buffer << (5 - bits)) & 31];
	return out;
}

string PbaSsoToken(const vector<uint8_t> &key, const string &machineId,
	const uint8_t challenge[PBA_SSO_CHALLENGE])
{
	// context, machine id and challenge, separated by NUL bytes
	vector<uint8_t> msg(SSO_CONTEXT, SSO_CONTEXT + sizeof(SSO_CONTEXT));
	msg.insert(msg.end(), machineId.begin(), machineId.end());
	msg.push_back(0);
	msg.insert(msg.end(), challenge, challenge + PBA_SSO_CHALLENGE);
	uint8_t mac[CF_SHA512_HASHSZ];
	cf_hmac(key.data(), key.size(), msg.data(), msg.size(), mac, &cf_sha512);
	uint8_t token[PBA_SSO_CHALLENGE + PBA_SSO_MAC];
	memcpy(token, challenge, PBA_SSO_CHALLENGE);
	memcpy(token + PBA_SSO_CHALLENGE, mac, PBA_SSO_MAC);
	return base32(token, sizeof(token));
}

bool PbaSsoEnabled(const PbaConfig &cfg)
{
	return !cfg.sso_url.empty() && !cfg.sso_machine_id.empty() && !cfg.sso_key.empty();
}

static string urlEncode(const string &s)
{
	static const char hex[] = "0123456789ABCDEF";
	string out;
	for (unsigned char c : s) {
		if (isalnum(c) || (c == '-') || (c == '_') || (c == '.') || (c == '~')) out += (char)c;
		else {
			out += '%';
			out += hex[c >> 4];
			out += hex[c & 15];
		}
	}
	return out;
}

bool PbaSsoMake(const PbaConfig &cfg, string &url, string &code)
{
	uint8_t challenge[PBA_SSO_CHALLENGE];
	ifstream random("/dev/urandom", ios::binary);
	if (!random.read((char *)challenge, sizeof(challenge))) return false;
	string token = PbaSsoToken(cfg.sso_key, cfg.sso_machine_id, challenge);
	string base = cfg.sso_url;
	while (!base.empty() && (base[base.size() - 1] == '/')) base.erase(base.size() - 1);
	url = base + "/m/" + urlEncode(cfg.sso_machine_id) + "?c=" + token;
	code.clear();
	for (size_t i = 0; i < token.size(); i += 4) {
		if (i) code += '-';
		code += token.substr(i, 4);
	}
	return true;
}
