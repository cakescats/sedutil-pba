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

struct PbaDrive {
	std::string dev;
	std::string model;
	std::string serial;
	PbaDriveState state;
	/** hash scheme that unlocked the drive */
	std::string hash;
};

/** Find SATA and NVMe drives and read their OPAL state */
std::vector<PbaDrive> ScanSEDs();

/** Try password on every locked drive with each hash scheme in turn.
 * @return number of drives unlocked by this password
 */
int UnlockSEDs(std::vector<PbaDrive> &drives, const char *password,
	const std::vector<PbaHashSpec> &hashes);

/** Count drives in a given state */
int CountSEDs(const std::vector<PbaDrive> &drives, PbaDriveState state);
