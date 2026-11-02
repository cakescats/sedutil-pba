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
#include <string>
using namespace std;

/** Result of reading a pass-phrase */
typedef enum _PassPhraseResult {
	PASSPHRASE_ENTERED, /**< Enter was pressed */
	PASSPHRASE_ESCAPE,  /**< Esc was pressed, nothing returned */
} PassPhraseResult;

/** Read a pass-phrase from the console without echo.
 * Ctrl-U and Ctrl-C clear the line, function and arrow keys are ignored.
 */
PassPhraseResult GetPassPhrase(const char *prompt, string &password, bool show_asterisk = true);
string GetPassPhrase(const char *prompt, bool show_asterisk = true);

/** Wait for a key press.
 * @param seconds how long to wait, -1 waits forever
 * @param tick called once a second with the seconds left, may be NULL
 * @return the key pressed (27 for Esc and function keys) or -1 on timeout
 */
int WaitKey(int seconds, void (*tick)(int left, void *ctx) = NULL, void *ctx = NULL);
