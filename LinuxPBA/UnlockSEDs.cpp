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

#include "os.h"
#include "UnlockSEDs.h"
#include "DtaDevGeneric.h"
#include "DtaDevOpal1.h"
#include "DtaDevOpal2.h"
#include "DtaLexicon.h"

#include <dirent.h>
#include <fnmatch.h>
#include <algorithm>

using namespace std;

static string fixedString(const char *s, size_t len)
{
	string r(s, strnlen(s, len));
	size_t e = r.find_last_not_of(' ');
	r.erase(string::npos == e ? 0 : e + 1);
	size_t b = r.find_first_not_of(' ');
	return string::npos == b ? "" : r.substr(b);
}

static DtaDev *openOpal(const string &dev, bool opal2)
{
	DtaDev *d;
	if (opal2)
		d = new DtaDevOpal2(dev.c_str());
	else
		d = new DtaDevOpal1(dev.c_str());
	d->no_hash_passwords = false;
	return d;
}

/** Drive needs a password: data range locked or shadow MBR still shown */
static bool needsUnlock(DtaDev *d)
{
	return d->Locked() || (d->MBREnabled() && !d->MBRDone());
}

static bool isOpal2(const string &dev)
{
	DtaDevGeneric probe(dev.c_str());
	return probe.isOpal2();
}

vector<PbaDrive> ScanSEDs()
{
	vector<string> devices;
	DIR *dir = opendir("/dev");
	if (dir) {
		struct dirent *dirent;
		while ((dirent = readdir(dir)) != NULL) {
			if (!fnmatch("sd[a-z]", dirent->d_name, 0) ||
				!fnmatch("sd[a-z][a-z]", dirent->d_name, 0) ||
				!fnmatch("nvme[0-9]", dirent->d_name, 0) ||
				!fnmatch("nvme[0-9][0-9]", dirent->d_name, 0))
				devices.push_back(string("/dev/") + dirent->d_name);
		}
		closedir(dir);
	}
	sort(devices.begin(), devices.end());

	vector<PbaDrive> drives;
	for (const string &dev : devices) {
		LOG(D1) << "Probing " << dev;
		DtaDevGeneric probe(dev.c_str());
		if (!probe.isPresent()) continue;
		PbaDrive drive;
		drive.dev = dev;
		drive.model = fixedString(probe.getModelNum(), 40);
		drive.serial = fixedString(probe.getSerialNum(), 20);
		if (!probe.isOpal1() && !probe.isOpal2()) {
			drive.state = DRIVE_NOT_OPAL;
		}
		else {
			DtaDev *d = openOpal(dev, probe.isOpal2());
			drive.state = needsUnlock(d) ? DRIVE_LOCKED : DRIVE_NOT_LOCKED;
			delete d;
		}
		drives.push_back(drive);
	}
	return drives;
}

/** Unlock one drive. Returns 0 on success, otherwise the OPAL status of
 *  the first failing call. */
static uint8_t unlockDrive(const string &dev, char *password, bool &authorized)
{
	DtaDev *d = openOpal(dev, isOpal2(dev));
	uint8_t rc = 0;
	authorized = false;
	if (d->Locked()) {
		rc = d->setLockingRange(0, OPAL_LOCKINGSTATE::READWRITE, password);
		if (rc) {
			delete d;
			return rc;
		}
		authorized = true;
	}
	if (d->MBREnabled() && !d->MBRDone()) {
		rc = d->setMBRDone(1, password);
		if (!rc) authorized = true;
	}
	delete d;
	return rc;
}

int UnlockSEDs(vector<PbaDrive> &drives, const char *password, const vector<PbaHashSpec> &hashes)
{
	int unlocked = 0;
	vector<char> pw(password, password + strlen(password) + 1);
	DtaHashAlg savedAlg;
	unsigned int savedIter;
	DtaGetHashConfig(&savedAlg, &savedIter);
	LOG(D1) << "Enter UnlockSEDs";
	for (PbaDrive &drive : drives) {
		if (DRIVE_LOCKED != drive.state) continue;
		for (const PbaHashSpec &spec : hashes) {
			bool authorized;
			DtaSetHashConfig(spec.alg, spec.iter);
			uint8_t rc = unlockDrive(drive.dev, pw.data(), authorized);
			if (!rc) {
				drive.state = DRIVE_UNLOCKED;
				drive.hash = spec.name();
				unlocked++;
				break;
			}
			LOG(D1) << drive.dev << " " << spec.name() << " rc " << (uint16_t)rc;
			if (authorized) {
				// password was right but the drive refused the change
				drive.state = DRIVE_FAILED;
				drive.hash = spec.name();
				break;
			}
			if (OPALSTATUSCODE::AUTHORITY_LOCKED_OUT == rc) {
				// too many tries since power on; more attempts only fail
				drive.state = DRIVE_FAILED;
				drive.hash = "locked out, power cycle";
				break;
			}
		}
	}
	DtaSetHashConfig(savedAlg, savedIter);
	memset(pw.data(), 0, pw.size());
	return unlocked;
}

int CountSEDs(const vector<PbaDrive> &drives, PbaDriveState state)
{
	return (int)count_if(drives.begin(), drives.end(),
		[state](const PbaDrive &d) { return d.state == state; });
}
