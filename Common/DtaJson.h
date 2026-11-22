/* C:B**************************************************************************
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
/* Machine-readable output for sedutil-cli -j.
 *
 * With -j the program writes exactly one JSON object to stdout:
 *   {"ok":true,"code":0,"status":"SUCCESS", <payload>}
 *   {"ok":false,"code":1,"status":"NOT_AUTHORIZED","errors":["..."]}
 * Everything else (log lines, progress) goes to stderr. docs/json.md
 * describes the format. */
#pragma once
#include <cstdint>
#include <string>
#include <vector>

class DtaDev;

namespace DtaJson {

/** A JSON string literal, quotes included. Bytes that are not printable
 *  ASCII are escaped as \u00XX, so the result is always valid JSON. */
std::string quote(const std::string &s);
/** A string reported by the drive (model, serial): trailing spaces and NULs cut */
std::string trimmed(const char *s, size_t max);

/** Builds one JSON object. Values passed to raw() must already be JSON. */
class Obj {
public:
	Obj &raw(const char *key, const std::string &json);
	Obj &str(const char *key, const std::string &value) { return raw(key, quote(value)); }
	Obj &b(const char *key, bool value) { return raw(key, value ? "true" : "false"); }
	Obj &u(const char *key, uint64_t value) { return raw(key, std::to_string(value)); }
	std::string done() const { return "{" + body + "}"; }
private:
	std::string body;
};

/** Builds one JSON array. Values must already be JSON. */
class Arr {
public:
	Arr &add(const std::string &json);
	Arr &str(const std::string &value) { return add(quote(value)); }
	std::string done() const { return "[" + body + "]"; }
private:
	std::string body;
};

/** True after -j: commands put their data into the result instead of printing */
bool enabled();
void enable();
/** Add a field to the result object main() prints at the end */
void set(const char *key, const std::string &json);
/** Error lines logged so far (LOG(E)), oldest first */
std::vector<std::string> &errors();
/** The whole result for the exit code rc */
std::string result(int rc);

/** Name for an exit code: TCG method status, DTAERROR_* or UNKNOWN */
const char *statusName(int rc);

/** Discovery 0 and identify data of an opened device */
std::string deviceInfo(DtaDev *d, const char *path);
/** --scan: one deviceInfo per path, {"path":..,"present":false} if it did not open */
int scan(const std::vector<std::string> &paths);

}
