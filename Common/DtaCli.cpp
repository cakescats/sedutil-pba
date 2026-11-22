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
#include <algorithm>
#include <cstring>
#include "DtaCli.h"
#ifdef _WIN32
#include <io.h>
#else
#include <signal.h>
#include <termios.h>
#include <unistd.h>
#endif

/* Long enough for any passphrase; a longer line is an error, not truncated */
static const size_t MAX_SECRET = 4096;

bool DtaReadLine(FILE *in, std::string &line)
{
	line.clear();
	int c;
	bool any = false;
	while ((c = fgetc(in)) != EOF) {
		any = true;
		if (c == '\n') break;
		line += (char)c;
	}
	if (!line.empty() && line.back() == '\r') line.pop_back();
	return any;
}

/* The two password slots; the strings live until DtaWipeStdinPasswords */
static std::string secrets[2];

void DtaWipeStdinPasswords()
{
	for (auto &s : secrets) {
		volatile char *p = s.empty() ? NULL : &s[0];
		for (size_t i = 0; p && i < s.size(); i++) p[i] = 0;
		s.clear();
	}
}

#ifdef _WIN32
static bool stdinIsTerminal()
{
	DWORD mode;
	return _isatty(_fileno(stdin)) && GetConsoleMode(GetStdHandle(STD_INPUT_HANDLE), &mode);
}

/* The console delivers UTF-16; convert to UTF-8 as the PBA would see it */
static bool readTerminal(const char *prompt, std::string &line)
{
	HANDLE h = GetStdHandle(STD_INPUT_HANDLE);
	DWORD mode;
	GetConsoleMode(h, &mode);
	fprintf(stderr, "%s: ", prompt);
	fflush(stderr);
	SetConsoleMode(h, (mode | ENABLE_LINE_INPUT) & ~ENABLE_ECHO_INPUT);
	std::wstring w;
	wchar_t buf[256];
	DWORD got = 0;
	bool ok = true;
	while (w.find(L'\n') == std::wstring::npos && w.find(L'\r') == std::wstring::npos) {
		if (!ReadConsoleW(h, buf, 256, &got, NULL) || got == 0) { ok = false; break; }
		w.append(buf, got);
	}
	SetConsoleMode(h, mode);
	fprintf(stderr, "\n");
	size_t end = w.find_first_of(L"\r\n");
	if (end != std::wstring::npos) w.resize(end);
	line.clear();
	if (!w.empty()) {
		int n = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), (int)w.size(), NULL, 0, NULL, NULL);
		line.resize(n);
		WideCharToMultiByte(CP_UTF8, 0, w.c_str(), (int)w.size(), &line[0], n, NULL, NULL);
	}
	SecureZeroMemory(&w[0], w.size() * sizeof(wchar_t));
	SecureZeroMemory(buf, sizeof(buf));
	return ok;
}

FILE *DtaDetachStdout()
{
	fflush(stdout);
	int fd = _dup(_fileno(stdout));
	if (fd < 0) return stdout;
	if (_dup2(_fileno(stderr), _fileno(stdout)) < 0) {
		_close(fd);
		return stdout;
	}
	FILE *f = _fdopen(fd, "w");
	return f ? f : stdout;
}
#else
static bool stdinIsTerminal()
{
	return isatty(STDIN_FILENO);
}

static struct termios saved;
static volatile sig_atomic_t echoOff = 0;

static void restoreEcho(int sig)
{
	if (echoOff) tcsetattr(STDIN_FILENO, TCSAFLUSH, &saved);
	signal(sig, SIG_DFL);
	raise(sig);
}

static bool readTerminal(const char *prompt, std::string &line)
{
	fprintf(stderr, "%s: ", prompt);
	fflush(stderr);
	bool restore = (tcgetattr(STDIN_FILENO, &saved) == 0);
	if (restore) {
		struct termios quiet = saved;
		quiet.c_lflag &= ~(tcflag_t)ECHO;
		signal(SIGINT, restoreEcho);
		signal(SIGTERM, restoreEcho);
		echoOff = 1;
		tcsetattr(STDIN_FILENO, TCSAFLUSH, &quiet);
	}
	bool ok = DtaReadLine(stdin, line);
	if (restore) {
		tcsetattr(STDIN_FILENO, TCSAFLUSH, &saved);
		echoOff = 0;
		signal(SIGINT, SIG_DFL);
		signal(SIGTERM, SIG_DFL);
	}
	fprintf(stderr, "\n");
	return ok;
}

FILE *DtaDetachStdout()
{
	fflush(stdout);
	int fd = dup(STDOUT_FILENO);
	if (fd < 0) return stdout;
	if (dup2(STDERR_FILENO, STDOUT_FILENO) < 0) {
		close(fd);
		return stdout;
	}
	FILE *f = fdopen(fd, "w");
	return f ? f : stdout;
}
#endif

static const char *promptFor(uint8_t action, bool newSlot)
{
	switch (action) {
	case sedutiloption::PSIDrevert:
	case sedutiloption::PSIDrevertAdminSP:
	case sedutiloption::yesIreallywanttoERASEALLmydatausingthePSID:
		return "PSID";
	case sedutiloption::initialSetup:
		return "New SID and Admin1 password";
	case sedutiloption::setRangeUser:
	case sedutiloption::setupPartitionRanges:
		return newSlot ? "New range password" : "Admin1 password";
	case sedutiloption::setLockingRangeUser:
		return "Range password";
	default:
		return newSlot ? "New password" : "Password";
	}
}

uint8_t DtaReadStdinPasswords(char *argv[], DTA_OPTIONS *opts)
{
	struct Slot { uint8_t arg; bool isNew; };
	Slot slots[2];
	int n = 0;
	if (opts->password && !strcmp(argv[opts->password], "-"))
		slots[n++] = { opts->password, opts->action == sedutiloption::initialSetup };
	if (opts->newpassword && !strcmp(argv[opts->newpassword], "-"))
		slots[n++] = { opts->newpassword, true };
	if (n == 0) {
		LOG(E) << "-S given but no password argument is \"-\"";
		return DTAERROR_INVALID_PARAMETER;
	}
	std::sort(slots, slots + n, [](const Slot &a, const Slot &b) { return a.arg < b.arg; });
	const bool tty = stdinIsTerminal();
	for (int i = 0; i < n; i++) {
		std::string &s = secrets[i];
		bool ok;
		if (tty) {
			const char *prompt = promptFor(opts->action, slots[i].arg == opts->newpassword);
			ok = readTerminal(prompt, s);
			if (ok && slots[i].isNew) {
				std::string again;
				std::string label = std::string(prompt) + " again";
				ok = readTerminal(label.c_str(), again);
				const bool same = (again == s);
				volatile char *p = again.empty() ? NULL : &again[0];
				for (size_t k = 0; p && k < again.size(); k++) p[k] = 0;
				if (ok && !same) {
					LOG(E) << "The two entries differ";
					DtaWipeStdinPasswords();
					return DTAERROR_INVALID_PARAMETER;
				}
			}
		}
		else ok = DtaReadLine(stdin, s);
		if (!ok) {
			LOG(E) << "stdin ended before password " << (i + 1) << " of " << n;
			DtaWipeStdinPasswords();
			return DTAERROR_INVALID_PARAMETER;
		}
		if (s.size() > MAX_SECRET) {
			LOG(E) << "Password " << (i + 1) << " is longer than " << MAX_SECRET << " bytes";
			DtaWipeStdinPasswords();
			return DTAERROR_INVALID_PARAMETER;
		}
		argv[slots[i].arg] = &s[0];
	}
	return 0;
}
