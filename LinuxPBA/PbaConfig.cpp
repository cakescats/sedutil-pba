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
#include "PbaConfig.h"
#include <fstream>
#include <sstream>
#include <cstdlib>
#include <cstring>

using namespace std;

string PbaHashSpec::name() const
{
	return string(DTA_HASH_SHA512 == alg ? "sha512:" : "sha1:") + to_string(iter);
}

static string trim(const string &s)
{
	size_t b = s.find_first_not_of(" \t\r\n");
	if (string::npos == b) return "";
	size_t e = s.find_last_not_of(" \t\r\n");
	string r = s.substr(b, e - b + 1);
	if ((r.size() >= 2) && ('"' == r[0]) && ('"' == r[r.size() - 1]))
		r = r.substr(1, r.size() - 2);
	return r;
}

static bool parseBool(const string &v, bool &out)
{
	if ((v == "1") || (v == "on") || (v == "yes") || (v == "true")) { out = true; return true; }
	if ((v == "0") || (v == "off") || (v == "no") || (v == "false")) { out = false; return true; }
	return false;
}

static bool parseInt(const string &v, int lo, int hi, int &out)
{
	char *end;
	long n = strtol(v.c_str(), &end, 10);
	if (v.empty() || *end || (n < lo) || (n > hi)) return false;
	out = (int)n;
	return true;
}

int PbaParseHashList(const string &value, vector<PbaHashSpec> &out)
{
	vector<PbaHashSpec> list;
	stringstream ss(value);
	string item;
	while (getline(ss, item, ',')) {
		item = trim(item);
		if (item.empty()) continue;
		PbaHashSpec spec;
		if (DtaParseHashSpec(item.c_str(), &spec.alg, &spec.iter)) return 1;
		list.push_back(spec);
	}
	if (list.empty()) return 1;
	out = list;
	return 0;
}

/** Apply one key. fromFile is false for kernel command line values,
 *  which may not touch the service shell settings. */
static void setKey(PbaConfig &cfg, const string &key, const string &value, bool fromFile)
{
	bool ok = true;
	if (key == "timeout") ok = parseInt(value, -1, 3600, cfg.timeout);
	else if (key == "boot_delay") ok = parseInt(value, 0, 60, cfg.boot_delay);
	else if (key == "hash") ok = !PbaParseHashList(value, cfg.hashes);
	else if (key == "reboot") {
		ok = (value == "warm") || (value == "cold") || (value == "keep");
		if (ok) cfg.reboot = value;
	}
	else if (key == "logo") ok = parseBool(value, cfg.logo);
	else if (key == "logo_file") cfg.logo_file = value;
	else if (key == "title") cfg.title = value;
	else if (key == "mode") {
		ok = (value == "normal") || (value == "service");
		if (ok) cfg.service_boot = (value == "service");
	}
	else if (!fromFile && ((key == "service") || (key == "service_word") || (key == "service_hash"))) {
		cfg.warnings.push_back("cakes." + key + " is ignored on the kernel command line");
		return;
	}
	else if (key == "service") {
		if (value == "password") cfg.service = PBA_SERVICE_PASSWORD;
		else {
			bool on;
			ok = parseBool(value, on);
			if (ok) cfg.service = on ? PBA_SERVICE_ON : PBA_SERVICE_OFF;
		}
	}
	else if (key == "service_word") cfg.service_word = value;
	else if (key == "service_hash") cfg.service_hash = value;
	else {
		cfg.warnings.push_back("unknown setting " + key);
		return;
	}
	if (!ok) cfg.warnings.push_back("bad value for " + key + ": " + value);
}

/** Split the kernel command line, keeping "quoted values" together */
static vector<string> splitCmdline(const string &line)
{
	vector<string> out;
	string cur;
	bool quoted = false;
	for (char c : line) {
		if ('"' == c) quoted = !quoted;
		else if (!quoted && ((' ' == c) || ('\n' == c) || ('\t' == c))) {
			if (!cur.empty()) out.push_back(cur);
			cur.clear();
		}
		else cur += c;
	}
	if (!cur.empty()) out.push_back(cur);
	return out;
}

void PbaLoadConfig(PbaConfig &cfg, const char *file, const char *cmdline)
{
	ifstream f(file);
	string line;
	unsigned int lineno = 0;
	while (f && getline(f, line)) {
		lineno++;
		size_t hash = line.find('#');
		if (string::npos != hash) line.erase(hash);
		line = trim(line);
		if (line.empty()) continue;
		size_t eq = line.find('=');
		if (string::npos == eq) {
			cfg.warnings.push_back(string(file) + ":" + to_string(lineno) + ": expected key = value");
			continue;
		}
		setKey(cfg, trim(line.substr(0, eq)), trim(line.substr(eq + 1)), true);
	}

	ifstream c(cmdline);
	if (c && getline(c, line)) {
		for (const string &tok : splitCmdline(line)) {
			if (tok.compare(0, 6, "cakes.")) continue;
			size_t eq = tok.find('=');
			if (string::npos == eq) continue;
			setKey(cfg, tok.substr(6, eq - 6), tok.substr(eq + 1), false);
		}
	}
	if ((PBA_SERVICE_PASSWORD == cfg.service) && cfg.service_hash.empty()) {
		cfg.warnings.push_back("service = password needs service_hash, service shell disabled");
		cfg.service = PBA_SERVICE_OFF;
	}
	if (PBA_SERVICE_OFF == cfg.service) cfg.service_boot = false;
}

static int hexval(char c)
{
	if ((c >= '0') && (c <= '9')) return c - '0';
	if ((c >= 'a') && (c <= 'f')) return c - 'a' + 10;
	if ((c >= 'A') && (c <= 'F')) return c - 'A' + 10;
	return -1;
}

static bool fromHex(const string &hex, vector<uint8_t> &out)
{
	out.clear();
	if (hex.size() % 2) return false;
	for (size_t i = 0; i < hex.size(); i += 2) {
		int hi = hexval(hex[i]), lo = hexval(hex[i + 1]);
		if ((hi < 0) || (lo < 0)) return false;
		out.push_back((uint8_t)(hi * 16 + lo));
	}
	return true;
}

/** Check password against service_hash = sha512:<iter>:<salt hex>:<hash hex> */
bool PbaCheckServicePassword(const PbaConfig &cfg, const string &password)
{
	vector<string> parts;
	size_t start = 0, colon;
	while ((colon = cfg.service_hash.find(':', start)) != string::npos) {
		parts.push_back(cfg.service_hash.substr(start, colon - start));
		start = colon + 1;
	}
	parts.push_back(cfg.service_hash.substr(start));
	vector<uint8_t> salt, expected, hash;
	if ((parts.size() != 4) || (parts[0] != "sha512") || !fromHex(parts[2], salt) ||
		!fromHex(parts[3], expected) || expected.empty() || (expected.size() > 253))
		return false;
	unsigned long iter = strtoul(parts[1].c_str(), NULL, 10);
	if (!iter) return false;
	if (password.empty()) return false;
	DtaHashPassword(hash, (char *)password.c_str(), salt, (unsigned int)iter,
		(uint8_t)expected.size(), DTA_HASH_SHA512);
	if (hash.size() != expected.size() + 2) return false;
	uint8_t diff = 0;
	for (size_t i = 0; i < expected.size(); i++) diff |= hash[i + 2] ^ expected[i];
	return 0 == diff;
}
