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
#pragma once
#include <string>
#include <vector>
#include "DtaHashPwd.h"

/** One password hashing scheme to try when unlocking */
struct PbaHashSpec {
	DtaHashAlg alg;
	unsigned int iter;
	std::string name() const;
};

typedef enum _PbaServiceMode {
	PBA_SERVICE_OFF,      /**< no service shell */
	PBA_SERVICE_ON,       /**< service shell without extra password */
	PBA_SERVICE_PASSWORD, /**< service shell behind service_hash */
} PbaServiceMode;

/** PBA settings.
 * Read from /etc/cakes-pba.conf (baked into the image) and then from
 * cakes.<key>=<value> on the kernel command line (editable in grub.cfg).
 * Service shell settings are only read from the file.
 * Nothing in here is secret: the shadow MBR can be read without a password.
 */
struct PbaConfig {
	/** seconds to wait for a key when some drives are still locked:
	 *  0 boots at once, -1 keeps asking until every drive is unlocked */
	int timeout = 3;
	/** seconds the result stays on screen before the reboot */
	int boot_delay = 1;
	std::vector<PbaHashSpec> hashes = { { DTA_HASH_SHA1, 75000 } };
	/** locking ranges to try a password on (as User n+1) before Admin1
	 *  has shown which ranges exist: none, auto = 1..8, or a list.
	 *  Each listed range costs a failed try for every other password. */
	std::vector<int> ranges;
	PbaServiceMode service = PBA_SERVICE_ON;
	/** typing this instead of a password opens the service shell */
	std::string service_word = "debug";
	/** sha512:<iterations>:<salt hex>:<hash hex>, see tools/mkservicehash.py */
	std::string service_hash;
	/** warm, cold or keep (leave the kernel reboot= setting alone) */
	std::string reboot = "warm";
	bool logo = true;
	std::string logo_file = "/usr/share/cakes-pba/logo.ppm";
	std::string title = "CakesCats Pre-Boot Authorization";
	/** cakes.mode=service: unlock, then open the shell instead of rebooting */
	bool service_boot = false;
	/** problems found while parsing, shown on screen */
	std::vector<std::string> warnings;
};

void PbaLoadConfig(PbaConfig &cfg, const char *file = "/etc/cakes-pba.conf",
	const char *cmdline = "/proc/cmdline");
/** Parse a comma separated list of hash specs, e.g. "sha1:75000,sha512" */
int PbaParseHashList(const std::string &value, std::vector<PbaHashSpec> &out);
/** Parse the ranges setting: "auto", "none" or e.g. "1,2" */
int PbaParseRangeList(const std::string &value, std::vector<int> &out);
/** Check password against cfg.service_hash */
bool PbaCheckServicePassword(const PbaConfig &cfg, const std::string &password);
