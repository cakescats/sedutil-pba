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
#include <string>
#include <vector>
#include <stdint.h>

/* Colours. PbaScreen::init() maps the console palette to the CakesCats
 * palette, so these name the brand colours rather than the VGA ones. */
#define PBA_C_RESET  "\033[0m"
#define PBA_C_TEXT   "\033[0;37m"  /* ink-2  #d6d9ee */
#define PBA_C_BRIGHT "\033[1;37m"  /* ink    #f3f4fb */
#define PBA_C_MUTED  "\033[1;30m"  /* muted  #a3a9cf */
#define PBA_C_BRAND  "\033[1;35m"  /* violet #aa81ff */
#define PBA_C_OK     "\033[1;36m"  /* cyan   #57f3fe */
#define PBA_C_WARN   "\033[1;33m"  /* amber  #ffd166 */
#define PBA_C_ERROR  "\033[1;31m"  /* pink   #ff6b8b */

/** The PBA console: brand palette, logo on the framebuffer and text below it.
 * Works without a framebuffer (serial console, no efifb) by skipping the logo.
 */
class PbaScreen {
public:
	PbaScreen();
	~PbaScreen();
	/** Set up palette and load the logo, a binary PPM (P6) file */
	void init(bool logo, const std::string &logoFile);
	/** Clear the screen, draw the logo and put the cursor under it */
	void clear();
	/** Console width in characters */
	int cols() const { return ncols; }
	/** Print text centred on the current line */
	void centered(const std::string &text, const char *color = PBA_C_TEXT);
	/** Restore the default palette and cursor, e.g. before the service shell */
	void restore();
private:
	bool loadPPM(const std::string &file);
	void drawLogo();
	void unmapFramebuffer();

	int ncols, nrows, charHeight;
	int fbfd;
	uint8_t *fb;
	size_t fbsize;
	uint32_t xres, yres, bpp, stride;
	uint32_t roff, rlen, goff, glen, boff, blen;
	int logoW, logoH, logoScale, logoRows;
	std::vector<uint8_t> logoRGB;
};
