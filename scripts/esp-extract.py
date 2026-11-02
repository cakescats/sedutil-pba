#!/usr/bin/env python3
"""Copy the files out of the EFI system partition of a PBA image.

    scripts/esp-extract.py cakes-pba.img outdir

Reads the GPT and the FAT12/16/32 file system directly, so it works on any
OS without mounting or mtools.
"""
import os
import struct
import sys


def esp_offset(img):
    hdr = img[512:1024]
    if hdr[:8] != b"EFI PART":
        sys.exit("no GPT header")
    lba, count, size = struct.unpack("<QII", hdr[72:88])
    for i in range(count):
        entry = img[lba * 512 + i * size:lba * 512 + (i + 1) * size]
        if entry[:16] != b"\0" * 16:
            return struct.unpack("<Q", entry[32:40])[0] * 512
    sys.exit("no partition")


def extract(img, base, out):
    b = img[base:]
    bps, spc, rsv, nfats, rootent, tot16, _, fatsz16 = struct.unpack("<HBHBHHBH", b[11:24])
    tot32, fatsz32 = struct.unpack("<II", b[32:40])
    fatsz = fatsz16 or fatsz32
    rootsecs = (rootent * 32 + bps - 1) // bps
    data = rsv + nfats * fatsz + rootsecs
    clusters = ((tot16 or tot32) - data) // spc
    width = 12 if clusters < 4085 else 16 if clusters < 65525 else 32
    fat = b[rsv * bps:(rsv + fatsz) * bps]

    def next_cluster(c):
        if width == 32:
            return struct.unpack("<I", fat[c * 4:c * 4 + 4])[0] & 0x0FFFFFFF
        if width == 16:
            return struct.unpack("<H", fat[c * 2:c * 2 + 2])[0]
        v = struct.unpack("<H", fat[c * 3 // 2:c * 3 // 2 + 2])[0]
        return v >> 4 if c & 1 else v & 0xFFF

    end = {12: 0xFF8, 16: 0xFFF8, 32: 0x0FFFFFF8}[width]

    def chain(c):
        parts = []
        while 2 <= c < end:
            off = (data + (c - 2) * spc) * bps
            parts.append(b[off:off + spc * bps])
            c = next_cluster(c)
        return b"".join(parts)

    def walk(raw, path):
        lfn = ""
        for i in range(0, len(raw), 32):
            e = raw[i:i + 32]
            if e[0] == 0:
                break
            if e[0] == 0xE5:
                lfn = ""
                continue
            if e[11] == 0x0F:
                part = (e[1:11] + e[14:26] + e[28:32]).decode("utf-16le")
                lfn = part.split("\0")[0].replace("￿", "") + lfn
                continue
            short = e[:8].decode("latin-1").strip()
            ext = e[8:11].decode("latin-1").strip()
            name = lfn or (short + ("." + ext if ext else ""))
            lfn = ""
            attr = e[11]
            if name in (".", "..") or attr & 0x08:
                continue
            cluster = (struct.unpack("<H", e[20:22])[0] << 16) | struct.unpack("<H", e[26:28])[0]
            size = struct.unpack("<I", e[28:32])[0]
            target = os.path.join(path, name)
            if attr & 0x10:
                os.makedirs(target, exist_ok=True)
                walk(chain(cluster), target)
            else:
                with open(target, "wb") as f:
                    f.write(chain(cluster)[:size])
                print(target)

    if width == 32:
        root = chain(struct.unpack("<I", b[44:48])[0])
    else:
        off = (rsv + nfats * fatsz) * bps
        root = b[off:off + rootsecs * bps]
    os.makedirs(out, exist_ok=True)
    walk(root, out)


def main():
    if len(sys.argv) != 3:
        sys.exit(__doc__)
    with open(sys.argv[1], "rb") as f:
        img = f.read()
    extract(img, esp_offset(img), sys.argv[2])


if __name__ == "__main__":
    main()
