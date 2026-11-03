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
#include <stdint.h>
#include <string>
#include <vector>
#include "PbaConfig.h"

/* Unlock with a company account (docs/sso.md).
 *
 * The PBA shows a QR code with <sso_url>/m/<machine id>?c=<token>. The token
 * is a fresh random challenge plus HMAC-SHA512(machine key, challenge),
 * so the unlock server can tell that the request comes from a screen of
 * this machine's PBA, and refuse a token it has seen before. The user signs
 * in with the company SSO on a phone, the server checks that this user may
 * unlock this machine, and shows the password to type into the PBA.
 * The PBA itself needs no network and no clock.
 */

/** Bytes of challenge and of MAC in a token */
#define PBA_SSO_CHALLENGE 4
#define PBA_SSO_MAC 6

/** Token for a given challenge: 16 base32 characters, no separators */
std::string PbaSsoToken(const std::vector<uint8_t> &key, const std::string &machineId,
	const uint8_t challenge[PBA_SSO_CHALLENGE]);

/** Is unlocking with a company account configured */
bool PbaSsoEnabled(const PbaConfig &cfg);

/** New random token; url is what the QR code holds, code the token in
 *  groups of four for typing by hand. false if there is no randomness. */
bool PbaSsoMake(const PbaConfig &cfg, std::string &url, std::string &code);
