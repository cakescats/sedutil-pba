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
#include "GetPassPhrase.h"
#include <string>
#include <termios.h>
#include <stdio.h>
#include <poll.h>
#include <time.h>
using namespace std;

/** Puts the console in no-echo, no-signal, byte at a time mode while in scope */
class RawTerminal {
public:
	RawTerminal() {
		ok = (0 == tcgetattr(0, &saved));
		if (!ok) return;
		struct termios raw = saved;
		raw.c_lflag &= ~(ICANON | ECHO | ISIG | IEXTEN);
		raw.c_iflag |= ICRNL;
		raw.c_cc[VMIN] = 1;
		raw.c_cc[VTIME] = 0;
		tcsetattr(0, TCSANOW, &raw);
	}
	~RawTerminal() {
		if (ok) tcsetattr(0, TCSANOW, &saved);
	}
private:
	struct termios saved;
	bool ok;
};

/** Wait up to ms milliseconds for input on stdin */
static bool inputReady(int ms)
{
	struct pollfd p = { 0, POLLIN, 0 };
	return (1 == poll(&p, 1, ms)) && (p.revents & POLLIN);
}

static int readByte()
{
	unsigned char ch;
	if (1 != read(0, &ch, 1)) return -1;
	return ch;
}

/** Read one key. Escape sequences (arrows, F-keys) come back as 27. */
static int readKey()
{
	int ch = readByte();
	if (27 == ch) {
		// swallow the rest of an escape sequence
		while (inputReady(30)) readByte();
	}
	return ch;
}

static void drainInput()
{
	while (inputReady(0)) readByte();
}

PassPhraseResult GetPassPhrase(const char *prompt, string &password, bool show_asterisk)
{
	const int BACKSPACE = 127;
	const int CTRL_H = 8;
	const int CTRL_C = 3;
	const int CTRL_U = 21;
	const size_t MAXLEN = 256;
	RawTerminal raw;
	password.clear();
	LOG(D4) << "Enter GetPassPhrase";
	printf("%s", prompt);
	fflush(stdout);
	drainInput();
	for (;;) {
		int ch = readKey();
		if ((ch < 0) || ('\n' == ch) || ('\r' == ch))
			break;
		if (27 == ch) {
			password.clear();
			printf("\n");
			return PASSPHRASE_ESCAPE;
		}
		if ((BACKSPACE == ch) || (CTRL_H == ch)) {
			if (password.length() != 0) {
				if (show_asterisk) printf("\b \b");
				password.resize(password.length() - 1);
			}
		}
		else if ((CTRL_U == ch) || (CTRL_C == ch)) {
			if (show_asterisk)
				for (size_t i = 0; i < password.length(); i++) printf("\b \b");
			password.clear();
		}
		else if ((ch >= 32) && (password.length() < MAXLEN)) {
			password += (char)ch;
			if (show_asterisk) printf("*");
		}
		fflush(stdout);
	}
	printf("\n");
	return PASSPHRASE_ENTERED;
}

string GetPassPhrase(const char *prompt, bool show_asterisk)
{
	string password;
	GetPassPhrase(prompt, password, show_asterisk);
	return password;
}

static long nowMs()
{
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	return ts.tv_sec * 1000L + ts.tv_nsec / 1000000L;
}

int WaitKey(int seconds, void (*tick)(int left, void *ctx), void *ctx)
{
	RawTerminal raw;
	drainInput();
	if (seconds < 0) return readKey();
	long deadline = nowMs() + seconds * 1000L;
	for (int left = seconds; left > 0; left--) {
		if (tick) tick(left, ctx);
		long wait = deadline - (left - 1) * 1000L - nowMs();
		if ((wait > 0) && inputReady((int)wait)) return readKey();
	}
	return -1;
}
