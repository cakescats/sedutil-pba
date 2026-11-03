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
#include "os.h"
#include "PbaScreen.h"
#include <stdio.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <linux/fb.h>
#include <fstream>
#include "qrcodegen/qrcodegen.hpp"

using namespace std;

/* Console palette, index 0..15, taken from the cakescats.com design tokens */
static const char *palette[16] = {
	"070c2b", /* 0  background (--bg) */
	"ff6b8b", /* 1  error */
	"57f3fe", /* 2  ok (--ok) */
	"ffd166", /* 3  warning */
	"8f6bff", /* 4  brand (--brand) */
	"8f6bff", /* 5  brand */
	"57f3fe", /* 6  cyan (--cyan) */
	"d6d9ee", /* 7  text (--ink-2) */
	"a3a9cf", /* 8  muted (--muted) */
	"ff6b8b", /* 9  error */
	"57f3fe", /* 10 ok */
	"ffd166", /* 11 warning */
	"aa81ff", /* 12 violet (--violet) */
	"aa81ff", /* 13 violet */
	"57f3fe", /* 14 cyan */
	"f3f4fb", /* 15 bright text (--ink) */
};

PbaScreen::PbaScreen() :
	ncols(80), nrows(25), charHeight(16), fbfd(-1), fb(NULL), fbsize(0),
	xres(0), yres(0), bpp(0), stride(0),
	roff(0), rlen(0), goff(0), glen(0), boff(0), blen(0),
	logoW(0), logoH(0), logoScale(1), logoRows(0)
{
}

PbaScreen::~PbaScreen()
{
	unmapFramebuffer();
}

void PbaScreen::unmapFramebuffer()
{
	if (fb) munmap(fb, fbsize);
	if (fbfd >= 0) close(fbfd);
	fb = NULL;
	fbfd = -1;
}

bool PbaScreen::loadPPM(const string &file)
{
	ifstream f(file, ios::binary);
	if (!f) return false;
	string magic;
	f >> magic;
	if (magic != "P6") return false;
	int vals[3], n = 0;
	while ((n < 3) && f) {
		f >> ws;
		if ('#' == f.peek()) {
			string comment;
			getline(f, comment);
			continue;
		}
		f >> vals[n++];
	}
	if ((n < 3) || (vals[2] != 255) || (vals[0] <= 0) || (vals[1] <= 0) ||
		(vals[0] > 4096) || (vals[1] > 4096))
		return false;
	f.get(); // single whitespace before the raster
	logoW = vals[0];
	logoH = vals[1];
	logoRGB.resize((size_t)logoW * logoH * 3);
	f.read((char *)logoRGB.data(), logoRGB.size());
	return (size_t)f.gcount() == logoRGB.size();
}

void PbaScreen::init(bool logo, const string &logoFile)
{
	struct winsize ws;
	if ((0 == ioctl(1, TIOCGWINSZ, &ws)) && ws.ws_col && ws.ws_row) {
		ncols = ws.ws_col;
		nrows = ws.ws_row;
	}
	// brand palette, no console blanking or powerdown, hidden cursor
	for (int i = 0; i < 16; i++) printf("\033]P%x%s", i, palette[i]);
	printf("\033[9;0]\033[14;0]\033[?25l" PBA_C_TEXT);
	fflush(stdout);

	fbfd = open("/dev/fb0", O_RDWR);
	if (fbfd < 0) return;
	struct fb_var_screeninfo vinfo;
	struct fb_fix_screeninfo finfo;
	if (ioctl(fbfd, FBIOGET_VSCREENINFO, &vinfo) || ioctl(fbfd, FBIOGET_FSCREENINFO, &finfo) ||
		((vinfo.bits_per_pixel != 16) && (vinfo.bits_per_pixel != 24) && (vinfo.bits_per_pixel != 32))) {
		unmapFramebuffer();
		return;
	}
	xres = vinfo.xres;
	yres = vinfo.yres;
	bpp = vinfo.bits_per_pixel / 8;
	stride = finfo.line_length;
	roff = vinfo.red.offset; rlen = vinfo.red.length;
	goff = vinfo.green.offset; glen = vinfo.green.length;
	boff = vinfo.blue.offset; blen = vinfo.blue.length;
	fbsize = (size_t)stride * yres;
	void *p = mmap(NULL, fbsize, PROT_READ | PROT_WRITE, MAP_SHARED, fbfd, 0);
	if (MAP_FAILED == p) {
		fb = NULL;
		unmapFramebuffer();
		return;
	}
	fb = (uint8_t *)p;
	charHeight = (int)(yres / nrows);
	if (charHeight < 8) charHeight = 16;

	if (!logo || !loadPPM(logoFile)) return;
	// 96px logo: 2x for 8x16 fonts, 4x for the 16x32 font used on 4K panels
	logoScale = 2 * charHeight / 16;
	if (logoScale < 1) logoScale = 1;
	while ((logoScale > 1) && ((uint32_t)(logoH * logoScale) > yres / 3)) logoScale--;
	logoRows = (charHeight + logoH * logoScale + charHeight - 1) / charHeight + 1;
	// keep enough rows for the drive list and prompt
	if ((logoRows > nrows - 14) || ((uint32_t)(logoW * logoScale) > xres)) {
		logoRows = 0;
		logoW = 0;
	}
}

void PbaScreen::fillRect(uint32_t x0, uint32_t y0, uint32_t w, uint32_t h, const uint8_t rgb[3])
{
	uint32_t v = ((uint32_t)(rgb[0] >> (8 - rlen)) << roff) |
		((uint32_t)(rgb[1] >> (8 - glen)) << goff) |
		((uint32_t)(rgb[2] >> (8 - blen)) << boff);
	for (uint32_t y = y0; (y < y0 + h) && (y < yres); y++) {
		uint8_t *row = fb + (size_t)y * stride;
		for (uint32_t x = x0; (x < x0 + w) && (x < xres); x++)
			memcpy(row + (size_t)x * bpp, &v, bpp); // little endian framebuffer
	}
}

void PbaScreen::drawLogo()
{
	if (!fb || !logoW) return;
	uint32_t x0 = (xres - logoW * logoScale) / 2;
	uint32_t y0 = charHeight;
	for (int y = 0; y < logoH; y++) {
		const uint8_t *src = logoRGB.data() + (size_t)y * logoW * 3;
		for (int x = 0; x < logoW; x++)
			fillRect(x0 + x * logoScale, y0 + y * logoScale, logoScale, logoScale, src + x * 3);
	}
}

int PbaScreen::clear(bool logo)
{
	printf(PBA_C_RESET "\033[2J\033[H" PBA_C_TEXT);
	fflush(stdout);
	// let fbcon finish clearing before drawing over it
	usleep(20000);
	int row = 2;
	if (logo && logoRows) {
		drawLogo();
		row = logoRows + 1;
	}
	printf("\033[%d;1H", row);
	fflush(stdout);
	return row;
}

int PbaScreen::drawQR(const string &text, int row, int maxRows)
{
	if (!fb || (maxRows < 4)) return 0;
	qrcodegen::QrCode qr = qrcodegen::QrCode::encodeText("", qrcodegen::QrCode::Ecc::LOW);
	try {
		qr = qrcodegen::QrCode::encodeText(text.c_str(), qrcodegen::QrCode::Ecc::MEDIUM);
	}
	catch (const std::exception &) {
		return 0; // longer than a QR code holds
	}
	// four modules of white quiet zone around it, as scanners expect
	int modules = qr.getSize() + 8;
	int px = (maxRows * charHeight) / modules;
	if ((uint32_t)(px * modules) > xres) px = xres / modules;
	if (px > 12) px = 12;
	if (px < 2) return 0;
	static const uint8_t white[3] = { 255, 255, 255 }, black[3] = { 0, 0, 0 };
	uint32_t side = px * modules;
	uint32_t x0 = (xres - side) / 2;
	uint32_t y0 = (row - 1) * charHeight;
	fillRect(x0, y0, side, side, white);
	for (int y = 0; y < qr.getSize(); y++)
		for (int x = 0; x < qr.getSize(); x++)
			if (qr.getModule(x, y))
				fillRect(x0 + (x + 4) * px, y0 + (y + 4) * px, px, px, black);
	return (side + charHeight - 1) / charHeight;
}

void PbaScreen::centered(const string &text, const char *color)
{
	int pad = (ncols - (int)text.size()) / 2;
	if (pad < 0) pad = 0;
	printf("%*s%s%s" PBA_C_TEXT "\n", pad, "", color, text.c_str());
}

void PbaScreen::restore()
{
	unmapFramebuffer();
	printf(PBA_C_RESET "\033]R\033[?25h\033[2J\033[H");
	fflush(stdout);
}
