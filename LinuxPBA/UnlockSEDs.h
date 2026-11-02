/* C:B**************************************************************************
This software is Copyright 2014-2017 Bright Plaza Inc. <drivetrust@drivetrust.com>
Copyright 2026 CakesCats

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
#include <stdint.h>
#include <string>
#include <vector>
#include "PbaConfig.h"

typedef enum _PbaDriveState {
	DRIVE_NOT_OPAL,   /**< no OPAL 1/2 locking SP */
	DRIVE_NOT_LOCKED, /**< OPAL, nothing to unlock */
	DRIVE_LOCKED,     /**< waiting for a password */
	DRIVE_UNLOCKED,   /**< unlocked in this session */
	DRIVE_FAILED,     /**< password accepted but unlock or MBRDone failed */
} PbaDriveState;

/** An OPAL locking range with its own password (User n+1, see
 *  sedutil-cli --setRangeUser) */
struct PbaRange {
	int num;
	/** logical blocks, 0 until Admin1 showed the range table */
	uint64_t size;
	PbaDriveState state;
	/** has its own password; otherwise Admin1 opens it with the drive */
	bool own_password;
	/** failed tries its password has left until the next power cycle, -1 unknown or no limit */
	int tries_left;
};

struct PbaDrive {
	std::string dev;
	std::string model;
	std::string serial;
	/** state of the part Admin1 opens: global range and shadow MBR */
	PbaDriveState state;
	/** hash scheme that unlocked the drive */
	std::string hash;
	/** ranges with their own password, from the range table once Admin1
	 *  worked, before that only the ones a range password opened */
	std::vector<PbaRange> ranges;
	bool ranges_known = false;
};

/** Find SATA and NVMe drives and read their OPAL state */
std::vector<PbaDrive> ScanSEDs();

/** Try password on every locked drive with each hash scheme in turn:
 * as Admin1 (global range, shadow MBR and the ranges without a password
 * of their own), otherwise as the user of each locked range that has one
 * (cfg.ranges until Admin1 showed which ranges exist). Every failed try
 * counts against that authority's TryLimit until the next power cycle,
 * so a password is only tried where it can fit.
 * @return number of drives and ranges unlocked by this password
 */
int UnlockSEDs(std::vector<PbaDrive> &drives, const char *password, const PbaConfig &cfg);

/** One line per drive for the kernel log, e.g. "nvme0=unlocked[1=unlocked,2=locked]" */
std::string DescribeSEDs(const std::vector<PbaDrive> &drives);

/** Count drives and ranges in a given state */
int CountSEDs(const std::vector<PbaDrive> &drives, PbaDriveState state);
