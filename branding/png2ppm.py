#!/usr/bin/env python3
"""Convert an 8-bit RGB/RGBA PNG to the binary PPM (P6) the PBA draws.

Transparent pixels are blended onto the console background colour, so the
logo sits on the same navy as the text screen. Standard library only.

    png2ppm.py logo.png logo.ppm [--bg 070c2b]
"""
import argparse
import struct
import sys
import zlib


def read_png(path):
    with open(path, "rb") as f:
        data = f.read()
    if data[:8] != b"\x89PNG\r\n\x1a\n":
        sys.exit(f"{path}: not a PNG")
    pos, idat, header = 8, b"", None
    while pos < len(data):
        length, kind = struct.unpack(">I4s", data[pos:pos + 8])
        body = data[pos + 8:pos + 8 + length]
        pos += 12 + length
        if kind == b"IHDR":
            header = struct.unpack(">IIBBBBB", body)
        elif kind == b"IDAT":
            idat += body
        elif kind == b"IEND":
            break
    width, height, depth, colour, _, _, interlace = header
    if depth != 8 or colour not in (2, 6) or interlace:
        sys.exit(f"{path}: need 8-bit RGB or RGBA, non-interlaced")
    bpp = 4 if colour == 6 else 3
    raw = zlib.decompress(idat)
    stride = width * bpp
    rows, prev = [], bytearray(stride)
    for y in range(height):
        ftype = raw[y * (stride + 1)]
        line = bytearray(raw[y * (stride + 1) + 1:(y + 1) * (stride + 1)])
        for i in range(stride):
            a = line[i - bpp] if i >= bpp else 0
            b = prev[i]
            c = prev[i - bpp] if i >= bpp else 0
            if ftype == 1:
                line[i] = (line[i] + a) & 0xFF
            elif ftype == 2:
                line[i] = (line[i] + b) & 0xFF
            elif ftype == 3:
                line[i] = (line[i] + (a + b) // 2) & 0xFF
            elif ftype == 4:
                p = a + b - c
                pa, pb, pc = abs(p - a), abs(p - b), abs(p - c)
                pred = a if pa <= pb and pa <= pc else (b if pb <= pc else c)
                line[i] = (line[i] + pred) & 0xFF
        rows.append(bytes(line))
        prev = line
    return width, height, bpp, rows


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("png")
    ap.add_argument("ppm")
    ap.add_argument("--bg", default="070c2b", help="background colour, hex RRGGBB")
    args = ap.parse_args()
    bg = bytes.fromhex(args.bg)
    width, height, bpp, rows = read_png(args.png)
    out = bytearray()
    for row in rows:
        for x in range(width):
            px = row[x * bpp:(x + 1) * bpp]
            alpha = px[3] if bpp == 4 else 255
            out += bytes((px[i] * alpha + bg[i] * (255 - alpha) + 127) // 255 for i in range(3))
    with open(args.ppm, "wb") as f:
        f.write(b"P6\n%d %d\n255\n" % (width, height))
        f.write(out)


if __name__ == "__main__":
    main()
