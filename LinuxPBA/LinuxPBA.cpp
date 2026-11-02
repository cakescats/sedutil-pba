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

#include <unistd.h>
#include <sys/reboot.h>
#include <iostream>
#include <fstream>
#include <algorithm>
#include "log.h"
#include "Version.h"
#include "DtaOptions.h"
#include "DtaHashPwd.h"
#include "GetPassPhrase.h"
#include "UnlockSEDs.h"
#include "PbaConfig.h"
#include "PbaScreen.h"

using namespace std;

/* Default to output that includes timestamps and goes to stderr*/
sedutiloutput outputFormat = sedutilNormal;

/** linuxpba exit code that tells cakes-pba-launch to open the service shell */
#define PBA_EXIT_SERVICE 2

static PbaScreen screen;

static void wipe(string &s)
{
	fill(s.begin(), s.end(), '\0');
	s.clear();
}

static string fit(const string &s, size_t width)
{
	if (s.size() <= width) return s + string(width - s.size(), ' ');
	return s.substr(0, width - 1) + "~";
}

static void draw(const PbaConfig &cfg, const vector<PbaDrive> &drives,
	const string &message, const char *messageColor)
{
	screen.clear();
	screen.centered(cfg.title, PBA_C_BRAND);
	screen.centered(string("sedutil ") + GIT_VERSION, PBA_C_MUTED);
	printf("\n");
	int cols = screen.cols();
	size_t modelWidth = cols >= 100 ? 40 : (cols >= 80 ? 28 : 16);
	size_t serialWidth = cols >= 80 ? 20 : 10;
	if (drives.empty())
		printf("  " PBA_C_MUTED "No SATA or NVMe drives found" PBA_C_TEXT "\n");
	for (const PbaDrive &d : drives) {
		const char *color = PBA_C_MUTED;
		string state;
		switch (d.state) {
		case DRIVE_NOT_OPAL: state = "no OPAL"; break;
		case DRIVE_NOT_LOCKED: state = "not locked"; break;
		case DRIVE_LOCKED: state = "locked"; color = PBA_C_WARN; break;
		case DRIVE_UNLOCKED: state = "unlocked"; color = PBA_C_OK; break;
		case DRIVE_FAILED: state = "failed (" + d.hash + ")"; color = PBA_C_ERROR; break;
		}
		string dev = d.dev.substr(d.dev.rfind('/') + 1);
		printf("  " PBA_C_BRIGHT "%s" PBA_C_TEXT " %s " PBA_C_MUTED "%s" PBA_C_TEXT " %s%s" PBA_C_TEXT "\n",
			fit(dev, 7).c_str(), fit(d.model, modelWidth).c_str(),
			fit(d.serial, serialWidth).c_str(), color, state.c_str());
		for (const PbaRange &r : d.ranges) {
			const char *rcolor = PBA_C_MUTED;
			string rstate = "not locked";
			if (DRIVE_LOCKED == r.state) {
				rstate = "locked";
				if (r.tries_left > 0) rstate += ", " + to_string(r.tries_left) + " tr" + (r.tries_left == 1 ? "y" : "ies") + " left";
				rcolor = PBA_C_WARN;
			}
			if (DRIVE_UNLOCKED == r.state) { rstate = "unlocked"; rcolor = PBA_C_OK; }
			if (DRIVE_FAILED == r.state) { rstate = "failed (locked out, power cycle)"; rcolor = PBA_C_ERROR; }
			char size[32] = "";
			if (r.size) snprintf(size, sizeof(size), "%.1f GiB", r.size / 2097152.0);
			printf("  %s " PBA_C_TEXT "%s " PBA_C_MUTED "%s" PBA_C_TEXT " %s%s" PBA_C_TEXT "\n",
				fit("", 7).c_str(), fit("  range " + to_string(r.num), modelWidth).c_str(),
				fit(size, serialWidth).c_str(), rcolor, rstate.c_str());
		}
	}
	printf("\n");
	for (const string &w : cfg.warnings)
		printf("  " PBA_C_WARN "config: %s" PBA_C_TEXT "\n", w.c_str());
	if (!message.empty())
		printf("  %s%s" PBA_C_TEXT "\n", messageColor, message.c_str());
	printf("\n");
	fflush(stdout);
}

/** Ask for the service password if needed.
 *  Returns true when the service shell may be opened. */
static bool serviceAllowed(const PbaConfig &cfg, string &message)
{
	if (PBA_SERVICE_OFF == cfg.service) {
		message = "Service shell is disabled";
		return false;
	}
	if (PBA_SERVICE_ON == cfg.service) return true;
	string pw;
	GetPassPhrase("  Service password: ", pw);
	bool ok = PbaCheckServicePassword(cfg, pw);
	wipe(pw);
	if (!ok) {
		sleep(2);
		message = "Wrong service password";
	}
	return ok;
}

static void enterService()
{
	ConsoleQuiet(false);
	screen.restore();
	exit(PBA_EXIT_SERVICE);
}

/** One line to the kernel log, for serial consoles and automated tests.
 *  Not shown on screen with loglevel=0. */
static void kmsg(const string &line)
{
	ofstream f("/dev/kmsg");
	if (f) f << "<6>cakes-pba: " << line << endl;
}

static void setRebootMode(const string &mode)
{
	if (mode == "keep") return;
	ofstream f("/sys/kernel/reboot/mode");
	if (f) f << mode;
}

static void countdownTick(int left, void *ctx)
{
	int stillLocked = *(int *)ctx;
	printf("\r  " PBA_C_TEXT "%d drive%s or range%s still locked. Type the next password, "
		"starting the OS in " PBA_C_BRIGHT "%d" PBA_C_TEXT " s ",
		stillLocked, stillLocked == 1 ? "" : "s", stillLocked == 1 ? " is" : "s are", left);
	fflush(stdout);
}

static void bootOS(const PbaConfig &cfg, vector<PbaDrive> &drives)
{
	string message;
	if (cfg.service_boot) {
		draw(cfg, drives, "Service mode: opening the shell", PBA_C_BRAND);
		if (serviceAllowed(cfg, message)) enterService();
	}
	kmsg("boot: " + to_string(CountSEDs(drives, DRIVE_UNLOCKED)) + " drive(s) unlocked, " +
		to_string(CountSEDs(drives, DRIVE_LOCKED)) + " still locked");
	draw(cfg, drives, "Starting OS", PBA_C_OK);
	sleep(cfg.boot_delay);
	sync();
	setRebootMode(cfg.reboot);
	reboot(RB_AUTOBOOT);
	// reboot() only returns on error
	LOG(E) << "reboot failed";
	sleep(30);
	exit(1);
}

int main(int argc, char** argv) {
	CLog::Level() = CLog::FromInt(2);
	RCLog::Level() = RCLog::FromInt(2);
	LOG(D4) << "Legacy PBA start" << endl;

	PbaConfig cfg;
	PbaLoadConfig(cfg);
	ConsoleQuiet(true);
	screen.init(cfg.logo, cfg.logo_file);
	draw(cfg, vector<PbaDrive>(), "Scanning drives", PBA_C_MUTED);

	// NVMe and AHCI probe asynchronously, give slow controllers a few seconds
	vector<PbaDrive> drives = ScanSEDs();
	for (int i = 0; (i < 5) && !CountSEDs(drives, DRIVE_LOCKED); i++) {
		sleep(1);
		drives = ScanSEDs();
	}

	kmsg("scan: " + to_string(drives.size()) + " drive(s), " +
		to_string(CountSEDs(drives, DRIVE_LOCKED)) + " locked");
	for (const string &w : cfg.warnings) kmsg("config: " + w);
	kmsg("state: " + DescribeSEDs(drives));

	string message;
	const char *messageColor = PBA_C_TEXT;
	string password;
	// a key that stopped the countdown and starts the next password
	string typed;
	for (;;) {
		int locked = CountSEDs(drives, DRIVE_LOCKED);
		int unlocked = CountSEDs(drives, DRIVE_UNLOCKED);
		if (!locked) {
			if (unlocked) bootOS(cfg, drives);
			if (message.empty())
				message = CountSEDs(drives, DRIVE_FAILED) ? "Unlock failed" : "No locked OPAL drives found";
			draw(cfg, drives, message, PBA_C_WARN);
			message.clear();
			printf("  " PBA_C_MUTED "R: rescan   Enter: reboot%s" PBA_C_TEXT "\n",
				PBA_SERVICE_OFF == cfg.service ? "" : "   S: service shell");
			int key = WaitKey(-1);
			if (('r' == key) || ('R' == key)) drives = ScanSEDs();
			else if ((('s' == key) || ('S' == key)) && (PBA_SERVICE_OFF != cfg.service)) {
				if (serviceAllowed(cfg, message)) enterService();
			}
			else if (('\n' == key) || ('\r' == key)) {
				sync();
				setRebootMode(cfg.reboot);
				reboot(RB_AUTOBOOT);
			}
			continue;
		}

		draw(cfg, drives, message, messageColor);
		message.clear();
		if (unlocked)
			printf("  " PBA_C_MUTED "Enter on an empty line or Esc starts the OS with the remaining drives locked"
				PBA_C_TEXT "\n\n");
		PassPhraseResult res = GetPassPhrase(unlocked ? "  Password for the remaining drives: " : "  Password: ",
			password, true, typed);
		wipe(typed);
		if ((PASSPHRASE_ESCAPE == res) || password.empty()) {
			if (unlocked) bootOS(cfg, drives);
			continue;
		}
		if ((PBA_SERVICE_OFF != cfg.service) && (password == cfg.service_word)) {
			wipe(password);
			if (serviceAllowed(cfg, message)) enterService();
			messageColor = PBA_C_ERROR;
			continue;
		}

		printf("  " PBA_C_MUTED "Checking..." PBA_C_TEXT "\n");
		fflush(stdout);
		int now = UnlockSEDs(drives, password.c_str(), cfg);
		wipe(password);
		kmsg("state: " + DescribeSEDs(drives));
		if (!now) {
			message = "No locked drive accepted this password";
			messageColor = PBA_C_ERROR;
			continue;
		}

		locked = CountSEDs(drives, DRIVE_LOCKED);
		if (!locked || !cfg.timeout) bootOS(cfg, drives);
		if (cfg.timeout < 0) continue;
		draw(cfg, drives, "", PBA_C_TEXT);
		// keys typed while the last password was checked belong to the next one
		int key = WaitKey(cfg.timeout, countdownTick, &locked, false);
		if (key < 0) bootOS(cfg, drives);
		if ((key >= 32) && (key < 127)) typed = string(1, (char)key);
	}
	return 0;
}
