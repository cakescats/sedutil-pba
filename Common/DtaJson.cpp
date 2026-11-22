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
#include "os.h"
#include <cstdio>
#include <utility>
#include "DtaJson.h"
#include "DtaDev.h"
#include "DtaDevGeneric.h"
#include "DtaLexicon.h"
#include "DtaStructures.h"

namespace DtaJson {

std::string quote(const std::string &s)
{
	std::string out = "\"";
	for (unsigned char c : s) {
		switch (c) {
		case '"': out += "\\\""; break;
		case '\\': out += "\\\\"; break;
		case '\n': out += "\\n"; break;
		case '\r': out += "\\r"; break;
		case '\t': out += "\\t"; break;
		default:
			if (c < 0x20 || c > 0x7e) {
				// drives report raw bytes; keep the output ASCII and valid
				char b[8];
				snprintf(b, sizeof(b), "\\u%04x", c);
				out += b;
			}
			else out += (char)c;
		}
	}
	return out + "\"";
}

std::string trimmed(const char *s, size_t max)
{
	size_t n = 0;
	while (n < max && s[n]) n++;
	while (n && (s[n - 1] == ' ' || s[n - 1] == '\0')) n--;
	size_t start = 0;
	while (start < n && s[start] == ' ') start++;
	return std::string(s + start, n - start);
}

Obj &Obj::raw(const char *key, const std::string &json)
{
	if (!body.empty()) body += ",";
	body += quote(key) + ":" + json;
	return *this;
}

Arr &Arr::add(const std::string &json)
{
	if (!body.empty()) body += ",";
	body += json;
	return *this;
}

static bool on = false;
static std::vector<std::pair<std::string, std::string> > &fields()
{
	static std::vector<std::pair<std::string, std::string> > f;
	return f;
}

bool enabled() { return on; }
void enable() { on = true; }

void set(const char *key, const std::string &json)
{
	for (auto &f : fields()) {
		if (f.first == key) { f.second = json; return; }
	}
	fields().push_back(std::make_pair(std::string(key), json));
}

std::vector<std::string> &errors() { return DtaLoggedErrors(); }

std::string result(int rc)
{
	Obj o;
	o.b("ok", rc == 0).u("code", (uint64_t)rc).str("status", statusName(rc));
	for (auto &f : fields()) o.raw(f.first.c_str(), f.second);
	if (!errors().empty()) {
		Arr a;
		for (auto &e : errors()) a.str(e);
		o.raw("errors", a.done());
	}
	return o.done();
}

const char *statusName(int rc)
{
	switch (rc) {
	case OPALSTATUSCODE::SUCCESS: return "SUCCESS";
	case OPALSTATUSCODE::NOT_AUTHORIZED: return "NOT_AUTHORIZED";
	case OPALSTATUSCODE::SP_BUSY: return "SP_BUSY";
	case OPALSTATUSCODE::SP_FAILED: return "SP_FAILED";
	case OPALSTATUSCODE::SP_DISABLED: return "SP_DISABLED";
	case OPALSTATUSCODE::SP_FROZEN: return "SP_FROZEN";
	case OPALSTATUSCODE::NO_SESSIONS_AVAILABLE: return "NO_SESSIONS_AVAILABLE";
	case OPALSTATUSCODE::UNIQUENESS_CONFLICT: return "UNIQUENESS_CONFLICT";
	case OPALSTATUSCODE::INSUFFICIENT_SPACE: return "INSUFFICIENT_SPACE";
	case OPALSTATUSCODE::INSUFFICIENT_ROWS: return "INSUFFICIENT_ROWS";
	case OPALSTATUSCODE::INVALID_FUNCTION: return "INVALID_FUNCTION";
	case OPALSTATUSCODE::INVALID_PARAMETER: return "INVALID_PARAMETER";
	case OPALSTATUSCODE::INVALID_REFERENCE: return "INVALID_REFERENCE";
	case OPALSTATUSCODE::TPER_MALFUNCTION: return "TPER_MALFUNCTION";
	case OPALSTATUSCODE::TRANSACTION_FAILURE: return "TRANSACTION_FAILURE";
	case OPALSTATUSCODE::RESPONSE_OVERFLOW: return "RESPONSE_OVERFLOW";
	case OPALSTATUSCODE::AUTHORITY_LOCKED_OUT: return "AUTHORITY_LOCKED_OUT";
	case OPALSTATUSCODE::FAIL: return "FAIL";
	case DTAERROR_UNSUPORTED_LOCKING_RANGE: return "UNSUPPORTED_LOCKING_RANGE";
	case DTAERROR_OBJECT_CREATE_FAILED: return "OBJECT_CREATE_FAILED";
	case DTAERROR_INVALID_PARAMETER: return "BAD_ARGUMENT";
	case DTAERROR_OPEN_ERR: return "OPEN_FAILED";
	case DTAERROR_INVALID_LIFECYCLE: return "INVALID_LIFECYCLE";
	case DTAERROR_INVALID_COMMAND: return "INVALID_COMMAND";
	case DTAERROR_AUTH_FAILED: return "AUTH_FAILED";
	case DTAERROR_COMMAND_ERROR: return "COMMAND_ERROR";
	case DTAERROR_NO_METHOD_STATUS: return "NO_METHOD_STATUS";
	case DTAERROR_NO_LOCKING_INFO: return "NO_LOCKING_INFO";
	case DTAERROR_IO: return "IO_ERROR";
	case 0xff: return "FAILED"; // a catch-all in older code: bad argument, not supported
	default: return "UNKNOWN";
	}
}

static const char *interfaceName(DTA_DEVICE_TYPE t)
{
	switch (t) {
	case DEVICE_TYPE_ATA: return "ATA";
	case DEVICE_TYPE_SAS: return "SAS";
	case DEVICE_TYPE_NVME: return "NVMe";
	case DEVICE_TYPE_USB: return "USB";
	default: return "other";
	}
}

std::string deviceInfo(DtaDev *d, const char *path)
{
	const OPAL_DiskInfo &di = d->diskInfo();
	Obj o;
	o.str("path", path).b("present", true);
	o.str("interface", interfaceName(di.devType));
	o.str("model", trimmed((const char *)di.modelNum, sizeof(di.modelNum)));
	o.str("firmware", trimmed((const char *)di.firmwareRev, sizeof(di.firmwareRev)));
	o.str("serial", trimmed((const char *)di.serialNum, sizeof(di.serialNum)));
	Arr ssc;
	if (di.OPAL20) ssc.str("opal2");
	if (di.OPAL10) ssc.str("opal1");
	if (di.Enterprise) ssc.str("enterprise");
	o.b("sed", di.ANY_OPAL_SSC != 0).raw("ssc", ssc.done());
	if (di.Locking) {
		o.raw("locking", Obj()
			.b("supported", di.Locking_lockingSupported != 0)
			.b("enabled", di.Locking_lockingEnabled != 0)
			.b("locked", di.Locking_locked != 0)
			.b("mbrEnabled", di.Locking_MBREnabled != 0)
			.b("mbrDone", di.Locking_MBRDone != 0)
			.b("mediaEncryption", di.Locking_mediaEncrypt != 0)
			.done());
	}
	else o.raw("locking", "null");
	if (di.BlockSIDAuthentication) {
		// SID Value State 1 means C_PIN_SID differs from C_PIN_MSID
		o.raw("blockSid", Obj()
			.b("sidIsMsid", di.BlockSIDAuthentication_SIDValueState == 0)
			.b("sidBlocked", di.BlockSIDAuthentication_SIDBlockedState != 0)
			.b("hardwareResetClears", di.BlockSIDAuthentication_HardwareReset != 0)
			.done());
	}
	if (di.TPer) {
		o.raw("tper", Obj()
			.b("sync", di.TPer_sync != 0).b("async", di.TPer_async != 0)
			.b("ackNak", di.TPer_ACKNACK != 0).b("bufferManagement", di.TPer_bufferMgt != 0)
			.b("streaming", di.TPer_streaming != 0).b("comIdManagement", di.TPer_comIDMgt != 0)
			.done());
	}
	if (di.Geometry) {
		o.raw("geometry", Obj()
			.b("align", di.Geometry_align != 0)
			.u("logicalBlockSize", di.Geometry_logicalBlockSize)
			.u("alignmentGranularity", di.Geometry_alignmentGranularity)
			.u("lowestAlignedLba", di.Geometry_lowestAlignedLBA)
			.done());
	}
	if (di.OPAL20) {
		o.raw("opal2", Obj()
			.u("baseComId", di.OPAL20_basecomID).u("comIds", di.OPAL20_numcomIDs)
			.u("initialPin", di.OPAL20_initialPIN).u("revertedPin", di.OPAL20_revertedPIN)
			.u("lockingAdmins", di.OPAL20_numAdmins).u("lockingUsers", di.OPAL20_numUsers)
			.b("rangeCrossing", di.OPAL20_rangeCrossing != 0)
			.done());
	}
	if (di.OPAL10) {
		o.raw("opal1", Obj()
			.u("baseComId", di.OPAL10_basecomID).u("comIds", di.OPAL10_numcomIDs)
			.done());
	}
	if (di.Enterprise) {
		o.raw("enterprise", Obj()
			.u("baseComId", di.Enterprise_basecomID).u("comIds", di.Enterprise_numcomID)
			.b("rangeCrossing", di.Enterprise_rangeCrossing != 0)
			.done());
	}
	if (di.SingleUser) {
		o.raw("singleUser", Obj()
			.b("any", di.SingleUser_any != 0).b("all", di.SingleUser_all != 0)
			.b("policy", di.SingleUser_policy != 0)
			.u("lockingObjects", di.SingleUser_lockingObjects)
			.done());
	}
	if (di.DataStore) {
		o.raw("dataStore", Obj()
			.u("maxTables", di.DataStore_maxTables).u("maxTableSize", di.DataStore_maxTableSize)
			.u("alignment", di.DataStore_alignment)
			.done());
	}
	return o.done();
}

int scan(const std::vector<std::string> &paths)
{
	Arr devices;
	for (auto &p : paths) {
		DtaDev *d = new DtaDevGeneric(p.c_str());
		if (d->isPresent()) devices.add(deviceInfo(d, p.c_str()));
		else devices.add(Obj().str("path", p).b("present", false).done());
		delete d;
	}
	set("devices", devices.done());
	return 0;
}

}
