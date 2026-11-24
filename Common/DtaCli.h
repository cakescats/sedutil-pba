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
/* Input and output plumbing of sedutil-cli that a GUI or script relies on:
 * passwords from stdin (-S) and a stdout that carries only JSON (-j). */
#pragma once
#include <cstdio>
#include <string>
#include "DtaOptions.h"

/** -S: every password argument that is exactly "-" is replaced by one line of
 *  stdin, in the order of the arguments. On a terminal the line is read with
 *  echo off after a prompt on stderr, and a new password is asked twice.
 *  @return 0, or DTAERROR_INVALID_PARAMETER when stdin ends early or the
 *  two entries of a new password differ */
uint8_t DtaReadStdinPasswords(char *argv[], DTA_OPTIONS *opts);

/** One line of in without its \n or \r\n. False at end of input when
 *  nothing was read, so an empty line is a valid (empty) password. */
bool DtaReadLine(FILE *in, std::string &line);

/** Overwrite the passwords DtaReadStdinPasswords read */
void DtaWipeStdinPasswords();

/** -j: point stdout (and std::cout) at stderr and return a stream on the
 *  original stdout, so nothing but the JSON result can end up there */
FILE *DtaDetachStdout();
