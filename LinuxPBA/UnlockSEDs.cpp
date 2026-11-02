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

/** Read the range table with the Admin1 password and open the ranges
 *  that have no password of their own; keeps the ranges a range password
 *  opened earlier marked as unlocked */
static void readRanges(PbaDrive &drive, char *password)
{
	DtaDevOpal *d = static_cast<DtaDevOpal *>(openOpal(drive.dev, isOpal2(drive.dev)));
	vector<DtaDevOpal::lrStatus_t> table;
	if (!d->getLockingRanges(password, table)) {
		vector<PbaRange> ranges;
		for (const DtaDevOpal::lrStatus_t &lr : table) {
			if (!lr.lockingrange_num || !lr.size || !(lr.RLKEna || lr.WLKEna)) continue;
			PbaRange r;
			r.num = lr.lockingrange_num;
			r.size = lr.size;
			r.own_password = lr.userEnabled;
			r.tries_left = lr.tryLimit ? (int)lr.tryLimit - (int)lr.tries : -1;
			r.state = ((lr.RLKEna && lr.RLocked) || (lr.WLKEna && lr.WLocked)) ? DRIVE_LOCKED : DRIVE_NOT_LOCKED;
			for (const PbaRange &old : drive.ranges)
				if ((old.num == r.num) && (DRIVE_UNLOCKED == old.state) && (DRIVE_NOT_LOCKED == r.state))
					r.state = DRIVE_UNLOCKED;
			if ((DRIVE_LOCKED == r.state) && r.own_password && !r.tries_left)
				r.state = DRIVE_FAILED;
			if ((DRIVE_LOCKED == r.state) && !r.own_password)
				r.state = d->setLockingRange((uint8_t)r.num, OPAL_LOCKINGSTATE::READWRITE, password) ?
					DRIVE_FAILED : DRIVE_UNLOCKED;
			ranges.push_back(r);
		}
		drive.ranges = ranges;
		drive.ranges_known = true;
	}
	delete d;
}

static PbaRange *findRange(PbaDrive &drive, int num)
{
	for (PbaRange &r : drive.ranges)
		if (r.num == num) return &r;
	return NULL;
}

static uint8_t unlockRange(const string &dev, int num, char *password)
{
	DtaDev *d = openOpal(dev, isOpal2(dev));
	uint8_t rc = d->setLockingRangeUser((uint8_t)num, OPAL_LOCKINGSTATE::READWRITE, password);
	delete d;
	return rc;
}

/** Drive still locked anywhere, as level 0 discovery sees it */
static bool stillLocked(const string &dev)
{
	DtaDev *d = openOpal(dev, isOpal2(dev));
	bool locked = needsUnlock(d);
	delete d;
	return locked;
}

int UnlockSEDs(vector<PbaDrive> &drives, const char *password, const PbaConfig &cfg)
{
	int unlocked = 0;
	vector<char> pw(password, password + strlen(password) + 1);
	DtaHashAlg savedAlg;
	unsigned int savedIter;
	DtaGetHashConfig(&savedAlg, &savedIter);
	LOG(D1) << "Enter UnlockSEDs";
	for (PbaDrive &drive : drives) {
		if ((DRIVE_NOT_OPAL == drive.state) || (DRIVE_FAILED == drive.state)) continue;
		bool tryAdmin = (DRIVE_LOCKED == drive.state);
		vector<int> candidates;
		if (drive.ranges_known) {
			for (const PbaRange &r : drive.ranges)
				if ((DRIVE_LOCKED == r.state) && r.own_password) candidates.push_back(r.num);
		}
		else if (tryAdmin) {
			for (int num : cfg.ranges)
				if (!findRange(drive, num) || (DRIVE_LOCKED == findRange(drive, num)->state))
					candidates.push_back(num);
		}
		if (!tryAdmin && candidates.empty()) continue;
		int before = unlocked;
		for (const PbaHashSpec &spec : cfg.hashes) {
			DtaSetHashConfig(spec.alg, spec.iter);
			if (tryAdmin) {
				bool authorized;
				uint8_t rc = unlockDrive(drive.dev, pw.data(), authorized);
				if (!rc) {
					drive.state = DRIVE_UNLOCKED;
					drive.hash = spec.name();
					unlocked++;
					tryAdmin = false;
					readRanges(drive, pw.data());
					// the Admin1 password is not tried on the ranges: a failed
					// try would count against their TryLimit
					candidates.clear();
				}
				else if (authorized) {
					// password was right but the drive refused the change
					drive.state = DRIVE_FAILED;
					drive.hash = spec.name();
					tryAdmin = false;
				}
				else if (OPALSTATUSCODE::AUTHORITY_LOCKED_OUT == rc) {
					// too many tries since power on; more attempts only fail
					drive.state = DRIVE_FAILED;
					drive.hash = "locked out, power cycle";
					tryAdmin = false;
				}
				else LOG(D1) << drive.dev << " Admin1 " << spec.name() << " rc " << (uint16_t)rc;
			}
			for (auto it = candidates.begin(); it != candidates.end();) {
				uint8_t rc = unlockRange(drive.dev, *it, pw.data());
				if (!rc || (OPALSTATUSCODE::AUTHORITY_LOCKED_OUT == rc)) {
					PbaRange *r = findRange(drive, *it);
					if (!r) {
						drive.ranges.push_back(PbaRange{ *it, 0, DRIVE_LOCKED, true, -1 });
						r = &drive.ranges.back();
					}
					r->state = rc ? DRIVE_FAILED : DRIVE_UNLOCKED;
					if (!rc) unlocked++;
					it = candidates.erase(it);
				}
				else {
					PbaRange *r = findRange(drive, *it);
					if (r && (r->tries_left > 0) && !--r->tries_left) {
						r->state = DRIVE_FAILED;
						it = candidates.erase(it);
					}
					else ++it;
				}
			}
		}
		// a range password may have opened the last locked part
		if ((unlocked > before) && (DRIVE_LOCKED == drive.state) && !stillLocked(drive.dev))
			drive.state = DRIVE_NOT_LOCKED;
	}
	DtaSetHashConfig(savedAlg, savedIter);
	memset(pw.data(), 0, pw.size());
	return unlocked;
}

static const char *stateName(PbaDriveState state)
{
	switch (state) {
	case DRIVE_NOT_OPAL: return "no-opal";
	case DRIVE_NOT_LOCKED: return "not-locked";
	case DRIVE_LOCKED: return "locked";
	case DRIVE_UNLOCKED: return "unlocked";
	case DRIVE_FAILED: return "failed";
	}
	return "?";
}

string DescribeSEDs(const vector<PbaDrive> &drives)
{
	string out;
	for (const PbaDrive &d : drives) {
		if (!out.empty()) out += " ";
		out += d.dev.substr(d.dev.rfind('/') + 1) + "=" + stateName(d.state);
		if (d.ranges.empty()) continue;
		out += "[";
		for (size_t i = 0; i < d.ranges.size(); i++)
			out += (i ? "," : "") + to_string(d.ranges[i].num) + "=" + stateName(d.ranges[i].state);
		out += "]";
	}
	return out;
}

int CountSEDs(const vector<PbaDrive> &drives, PbaDriveState state)
{
	int n = 0;
	for (const PbaDrive &d : drives) {
		if (d.state == state) n++;
		for (const PbaRange &r : d.ranges)
			if (r.state == state) n++;
	}
	return n;
}
