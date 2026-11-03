#!/usr/bin/env python3
"""Make a custom CakesCats PBA image from a released one, in seconds.

    cakes-builder.py build -i cakes-pba-<version>-default.img.gz -o my-pba.img \\
        [--config my.conf] [--set key=value ...] [--logo logo.png] \\
        [--file src:/dest/path ...] [--gzip]
    cakes-builder.py show my-pba.img

The released image is not rebuilt. The builder adds a small overlay archive,
cakes/overlay.cpio, next to the image's initramfs, and GRUB loads both:
the kernel unpacks them in order, so files in the overlay replace the ones
in the image. The overlay holds /etc/cakes-pba.conf and the logo, and any
--file you add.

Settings start from the image's own /etc/cakes-pba.conf (or from an overlay
the image already has, or from --config), and --set lines go on top. Nothing
in the image is secret: the shadow MBR can be read without a password, so
don't put passwords here.

Standard library only: runs on Linux, macOS and Windows without root.
"""
import argparse
import gzip
import hashlib
import lzma
import os
import re
import struct
import sys
import zlib

SECTOR = 512
OVERLAY = "cakes/overlay.cpio"
ROOTFS = "cakes/rootfs.cpio.xz"
GRUB_CFG = "EFI/BOOT/grub.cfg"
CONF_PATH = "etc/cakes-pba.conf"
LOGO_PATH = "usr/share/cakes-pba/logo.ppm"
# keys LinuxPBA/PbaConfig.cpp understands
KNOWN_KEYS = {"timeout", "boot_delay", "hash", "reboot", "logo", "logo_file", "title",
              "mode", "service", "service_word", "service_hash", "ranges"}
# fixed time stamps keep the output reproducible
FIXED_MTIME = 1767225600  # 2026-01-01
FAT_DATE = ((2026 - 1980) << 9) | (1 << 5) | 1
MAX_LOGO = 192


def fail(msg):
    sys.exit(f"cakes-builder: {msg}")


# ---------------------------------------------------------------- GPT

def esp_partition(img):
    """Byte offset and size of the first GPT partition."""
    hdr = img[SECTOR:2 * SECTOR]
    if hdr[:8] != b"EFI PART":
        fail("not a GPT disk image")
    lba, count, size = struct.unpack("<QII", hdr[72:88])
    for i in range(count):
        entry = img[lba * SECTOR + i * size:lba * SECTOR + (i + 1) * size]
        if entry[:16] != b"\0" * 16:
            first, last = struct.unpack("<QQ", entry[32:48])
            return first * SECTOR, (last - first + 1) * SECTOR
    fail("no partition in the image")


# ---------------------------------------------------------------- FAT read

class FatImage:
    """Reads a FAT12/16/32 file system into {path: bytes} plus its identity."""

    def __init__(self, data):
        b = data
        if b[510:512] != b"\x55\xaa":
            fail("ESP has no FAT boot sector")
        self.bps, self.spc, rsv, nfats, rootent, tot16, _, fatsz16 = struct.unpack("<HBHBHHBH", b[11:24])
        tot32, fatsz32 = struct.unpack("<II", b[32:40])
        fatsz = fatsz16 or fatsz32
        rootsecs = (rootent * 32 + self.bps - 1) // self.bps
        data_start = rsv + nfats * fatsz + rootsecs
        clusters = ((tot16 or tot32) - data_start) // self.spc
        self.width = 12 if clusters < 4085 else 16 if clusters < 65525 else 32
        ext = 64 if self.width == 32 else 36
        self.volume_id = struct.unpack("<I", b[ext + 3:ext + 7])[0]
        self.label = b[ext + 7:ext + 18]
        fat = b[rsv * self.bps:(rsv + fatsz) * self.bps]
        end = {12: 0xFF8, 16: 0xFFF8, 32: 0x0FFFFFF8}[self.width]
        csize = self.spc * self.bps

        def next_cluster(c):
            if self.width == 32:
                return struct.unpack("<I", fat[c * 4:c * 4 + 4])[0] & 0x0FFFFFFF
            if self.width == 16:
                return struct.unpack("<H", fat[c * 2:c * 2 + 2])[0]
            v = struct.unpack("<H", fat[c * 3 // 2:c * 3 // 2 + 2])[0]
            return v >> 4 if c & 1 else v & 0xFFF

        def chain(c):
            parts = []
            while 2 <= c < end:
                off = (data_start + (c - 2) * self.spc) * self.bps
                parts.append(b[off:off + csize])
                c = next_cluster(c)
            return b"".join(parts)

        self.files = {}
        self.dirs = []

        def walk(raw, prefix):
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
                short = e[:8].decode("latin-1").rstrip()
                ext_ = e[8:11].decode("latin-1").rstrip()
                if e[12] & 0x08:
                    short = short.lower()
                if e[12] & 0x10:
                    ext_ = ext_.lower()
                name = lfn or (short + ("." + ext_ if ext_ else ""))
                lfn = ""
                attr = e[11]
                if name in (".", "..") or attr & 0x08:
                    continue
                cluster = (struct.unpack("<H", e[20:22])[0] << 16) | struct.unpack("<H", e[26:28])[0]
                size = struct.unpack("<I", e[28:32])[0]
                path = prefix + name
                if attr & 0x10:
                    self.dirs.append(path)
                    walk(chain(cluster), path + "/")
                else:
                    self.files[path] = chain(cluster)[:size]

        if self.width == 32:
            walk(chain(struct.unpack("<I", b[44:48])[0]), "")
        else:
            off = (rsv + nfats * fatsz) * self.bps
            walk(b[off:off + rootsecs * self.bps], "")

    def find(self, path):
        """Case-insensitive lookup, FAT names are case-insensitive."""
        for name in self.files:
            if name.lower() == path.lower():
                return name
        return None


# ---------------------------------------------------------------- FAT write

def short_name(name, taken):
    """8.3 name, NT case flags and whether a long name entry is needed."""
    base, dot, ext = name.rpartition(".")
    if not dot:
        base, ext = name, ""
    legal = re.compile(r"^[A-Za-z0-9_\-~!#$%&'(){}^@`]+$")
    fits = (1 <= len(base) <= 8 and len(ext) <= 3 and legal.match(base)
            and (not ext or legal.match(ext)) and name.count(".") <= 1)
    if fits:
        flags = 0
        if base == base.lower() and base != base.upper():
            flags |= 0x08
        elif base != base.upper():
            fits = False
        if ext and ext == ext.lower() and ext != ext.upper():
            flags |= 0x10
        elif ext != ext.upper():
            fits = False
        if fits:
            sn = base.upper().ljust(8) + ext.upper().ljust(3)
            if sn not in taken:
                return sn.encode("ascii"), flags, False
    clean = lambda s: re.sub(r"[^A-Z0-9_]", "", s.upper())
    b, e = clean(base)[:6] or "FILE", clean(ext)[:3]
    for n in range(1, 1000):
        tail = f"~{n}"
        sn = (b[:8 - len(tail)] + tail).ljust(8) + e.ljust(3)
        if sn not in taken:
            return sn.encode("ascii"), 0, True
    fail(f"too many similar names for {name}")


def lfn_entries(name, short):
    checksum = 0
    for c in short:
        checksum = (((checksum & 1) << 7) + (checksum >> 1) + c) & 0xFF
    units = name.encode("utf-16le")
    units += b"\0\0" if len(units) % 26 else b""
    while len(units) % 26:
        units += b"\xff\xff"
    chunks = [units[i:i + 26] for i in range(0, len(units), 26)]
    out = []
    for seq, chunk in enumerate(chunks, 1):
        order = seq | (0x40 if seq == len(chunks) else 0)
        out.append(bytes([order]) + chunk[0:10] + bytes([0x0F, 0, checksum]) + chunk[10:22]
                   + b"\0\0" + chunk[22:26])
    return list(reversed(out))


def dir_entry(short, attr, flags, cluster, size):
    return (short + bytes([attr, flags, 0]) + struct.pack("<HHH", 0, FAT_DATE, FAT_DATE)
            + struct.pack("<H", cluster >> 16) + struct.pack("<HH", 0, FAT_DATE)
            + struct.pack("<H", cluster & 0xFFFF) + struct.pack("<I", size))


def make_fat16(size, files, label, volume_id, hidden_sectors):
    """A FAT16 file system of exactly size bytes holding files {path: bytes}."""
    total = size // SECTOR
    rsv, nfats, rootent = 4, 2, 512
    rootsecs = rootent * 32 // SECTOR
    for spc in (1, 2, 4, 8, 16, 32, 64):
        # FAT size and cluster count depend on each other, two passes settle it
        fatsz = 1
        for _ in range(3):
            clusters = (total - rsv - nfats * fatsz - rootsecs) // spc
            fatsz = ((clusters + 2) * 2 + SECTOR - 1) // SECTOR
        if 4085 <= clusters < 65525:
            break
    else:
        fail("ESP size does not fit FAT16")
    # the last FAT size may leave fewer sectors for data than the pass assumed
    clusters = (total - rsv - nfats * fatsz - rootsecs) // spc
    if (clusters + 2) * 2 > fatsz * SECTOR:
        fail("FAT16 layout does not add up")
    csize = spc * SECTOR
    data_start = rsv + nfats * fatsz + rootsecs
    fat = [0] * (clusters + 2)
    fat[0], fat[1] = 0xFFF8, 0xFFFF
    data = bytearray(clusters * csize)
    next_free = [2]

    def alloc(payload):
        if not payload:
            return 0
        n = (len(payload) + csize - 1) // csize
        first = next_free[0]
        if first + n > clusters + 2:
            fail("files do not fit into the ESP")
        for i in range(n):
            fat[first + i] = first + i + 1 if i < n - 1 else 0xFFFF
        off = (first - 2) * csize
        data[off:off + len(payload)] = payload
        next_free[0] += n
        return first

    # tree of directories: {name: subtree or bytes}
    tree = {}
    for path, content in files.items():
        node = tree
        parts = path.split("/")
        for part in parts[:-1]:
            node = node.setdefault(part, {})
        node[parts[-1]] = content

    def build_dir(node, own, parent):
        """Directory entries for node; files and subdirectories get clusters."""
        entries = []
        if own is not None:
            entries.append(dir_entry(b".          ", 0x10, 0, own, 0))
            entries.append(dir_entry(b"..         ", 0x10, 0, parent, 0))
        taken = set()
        for name in sorted(node):
            content = node[name]
            short, flags, needs_lfn = short_name(name, taken)
            taken.add(short.decode("ascii"))
            if needs_lfn:
                entries.extend(lfn_entries(name, short))
            if isinstance(content, dict):
                # reserve the directory's clusters first so "." can point at them
                body_len = (len(content) * 4 + 3) * 32
                n = (body_len + csize - 1) // csize
                first = alloc(b"\0" * (n * csize))
                body = build_dir(content, first, own or 0)
                if len(body) > n * csize:
                    fail(f"directory {name} too large")
                off = (first - 2) * csize
                data[off:off + len(body)] = body
                entries.append(dir_entry(short, 0x10, flags, first, 0))
            else:
                entries.append(dir_entry(short, 0x20, flags, alloc(content), len(content)))
        return b"".join(entries)

    root = bytearray(dir_entry(label, 0x08, 0, 0, 0))
    root += build_dir(tree, None, 0)
    if len(root) > rootsecs * SECTOR:
        fail("too many files in the ESP root")
    root = bytes(root).ljust(rootsecs * SECTOR, b"\0")

    boot = bytearray(SECTOR)
    boot[0:3] = b"\xeb\x3c\x90"
    boot[3:11] = b"CAKESPBA"
    tot16, tot32 = (total, 0) if total < 0x10000 else (0, total)
    boot[11:36] = struct.pack("<HBHBHHBHHHII", SECTOR, spc, rsv, nfats, rootent, tot16, 0xF8,
                              fatsz, 32, 64, hidden_sectors, tot32)
    boot[36:62] = struct.pack("<BBBI", 0x80, 0, 0x29, volume_id) + label + b"FAT16   "
    boot[510:512] = b"\x55\xaa"

    fat_bytes = b"".join(struct.pack("<H", v) for v in fat).ljust(fatsz * SECTOR, b"\0")
    out = bytearray(size)
    out[0:SECTOR] = boot
    for i in range(nfats):
        off = (rsv + i * fatsz) * SECTOR
        out[off:off + len(fat_bytes)] = fat_bytes
    off = (rsv + nfats * fatsz) * SECTOR
    out[off:off + len(root)] = root
    out[data_start * SECTOR:data_start * SECTOR + len(data)] = data
    return bytes(out)


# ---------------------------------------------------------------- cpio

def cpio_read(data):
    """{path: bytes} for regular files of a newc archive (possibly xz)."""
    if data[:6] == b"\xfd7zXZ\0":
        data = lzma.decompress(data)
    files, off = {}, 0
    while off + 110 <= len(data):
        h = data[off:off + 110]
        if h[:6] != b"070701":
            break
        f = [int(h[6 + i * 8:14 + i * 8], 16) for i in range(13)]
        mode, fsize, nsize = f[1], f[6], f[11]
        name = data[off + 110:off + 110 + nsize - 1].decode()
        off = (off + 110 + nsize + 3) & ~3
        body = data[off:off + fsize]
        off = (off + fsize + 3) & ~3
        if name == "TRAILER!!!":
            break
        if mode & 0o170000 == 0o100000:
            files[name[2:] if name.startswith("./") else name.lstrip("/")] = body
    return files


def cpio_write(files):
    """newc archive with files {path: bytes}, parent directories included."""
    out = bytearray()
    ino = [1]

    def add(name, mode, body=b""):
        nb = name.encode() + b"\0"
        hdr = "070701" + "".join(f"{v:08X}" for v in (
            ino[0], mode, 0, 0, 2 if mode & 0o040000 else 1, FIXED_MTIME, len(body),
            0, 0, 0, 0, len(nb), 0))
        ino[0] += 1
        out.extend(hdr.encode() + nb)
        out.extend(b"\0" * (-len(out) % 4))
        out.extend(body)
        out.extend(b"\0" * (-len(out) % 4))

    dirs = sorted({"/".join(p.split("/")[:i]) for p in files for i in range(1, p.count("/") + 1)})
    for d in dirs:
        add(d, 0o040755)
    for path in sorted(files):
        add(path, 0o100644, files[path])
    add("TRAILER!!!", 0)
    return bytes(out)


# ---------------------------------------------------------------- settings

def parse_conf(text):
    """(key, value) pairs in order, as the PBA reads them."""
    pairs = []
    for line in text.splitlines():
        line = line.split("#", 1)[0].strip()
        if not line:
            continue
        if "=" not in line:
            fail(f"config line without '=': {line}")
        key, value = (s.strip() for s in line.split("=", 1))
        pairs.append((key, value))
    return pairs


def check_conf(text):
    for key, value in parse_conf(text):
        if key not in KNOWN_KEYS:
            fail(f"unknown setting '{key}' (known: {', '.join(sorted(KNOWN_KEYS))})")
        if key == "service_hash" and value and not re.match(r"^sha512:\d+:[0-9a-fA-F]+:[0-9a-fA-F]+$", value):
            fail("service_hash must look like sha512:<iterations>:<salt hex>:<hash hex>")


def effective(text):
    settings = {}
    for key, value in parse_conf(text):
        settings[key] = value
    return settings


# ---------------------------------------------------------------- logo

def read_png(path):
    """Width, height, bytes per pixel and unfiltered rows of an 8-bit RGB(A) PNG."""
    with open(path, "rb") as f:
        data = f.read()
    if data[:8] != b"\x89PNG\r\n\x1a\n":
        fail(f"{path}: not a PNG")
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
        fail(f"{path}: need an 8-bit RGB or RGBA PNG, not interlaced")
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


def logo_ppm(png_path, bg="070c2b"):
    """PNG to PPM on the PBA background, scaled down to at most MAX_LOGO px."""
    width, height, bpp, rows = read_png(png_path)
    bgc = bytes.fromhex(bg)
    rgb = []
    for row in rows:
        line = []
        for x in range(width):
            px = row[x * bpp:(x + 1) * bpp]
            a = px[3] if bpp == 4 else 255
            line.append(tuple((px[i] * a + bgc[i] * (255 - a) + 127) // 255 for i in range(3)))
        rgb.append(line)
    factor = max(1, -(-max(width, height) // MAX_LOGO))
    if factor > 1:
        # box filter: average factor x factor blocks
        nw, nh = width // factor, height // factor
        small = []
        for y in range(nh):
            line = []
            for x in range(nw):
                block = [rgb[y * factor + j][x * factor + i] for j in range(factor) for i in range(factor)]
                line.append(tuple(sum(c[k] for c in block) // len(block) for k in range(3)))
            small.append(line)
        rgb, width, height = small, nw, nh
    body = b"".join(bytes(c) for line in rgb for c in line)
    return b"P6\n%d %d\n255\n" % (width, height) + body, (width, height)


# ---------------------------------------------------------------- commands

def read_image(path):
    with open(path, "rb") as f:
        data = f.read()
    if data[:2] == b"\x1f\x8b":
        data = gzip.decompress(data)
    return bytearray(data)


def grub_with_overlay(cfg):
    """Make every 'initrd /cakes/rootfs.cpio.xz' also load the overlay."""
    lines, changed = [], 0
    for line in cfg.splitlines(keepends=True):
        m = re.match(r"^(\s*initrd\s+/cakes/rootfs\.cpio\.xz)(\s*)$", line.rstrip("\n"))
        if m:
            line = m.group(1) + " /" + OVERLAY + "\n"
            changed += 1
        elif re.match(r"^\s*initrd\s+/cakes/rootfs\.cpio\.xz\s+/" + re.escape(OVERLAY), line):
            changed += 1
        lines.append(line)
    if not changed:
        fail("no 'initrd /cakes/rootfs.cpio.xz' line in grub.cfg, is this a CakesCats PBA image?")
    return "".join(lines)


def cmd_build(args):
    img = read_image(args.image)
    offset, size = esp_partition(img)
    fs = FatImage(bytes(img[offset:offset + size]))
    files = dict(fs.files)

    rootfs_name = fs.find(ROOTFS)
    grub_name = fs.find(GRUB_CFG)
    if not rootfs_name or not grub_name:
        fail("the image has no cakes/rootfs.cpio.xz or EFI/BOOT/grub.cfg")

    overlay_name = fs.find(OVERLAY)
    overlay = cpio_read(files[overlay_name]) if overlay_name else {}
    if args.config:
        with open(args.config, encoding="utf-8") as f:
            conf = f.read()
    elif CONF_PATH in overlay:
        conf = overlay[CONF_PATH].decode("utf-8")
    else:
        base = cpio_read(files[rootfs_name])
        if CONF_PATH not in base:
            fail("the image's initramfs has no /etc/cakes-pba.conf")
        conf = base[CONF_PATH].decode("utf-8")
    if args.set:
        conf = conf.rstrip("\n") + "\n\n# cakes-builder\n"
        for item in args.set:
            if "=" not in item:
                fail(f"--set needs key=value, got {item}")
            key, value = (s.strip() for s in item.split("=", 1))
            conf += f"{key} = {value}\n"
    check_conf(conf)
    overlay[CONF_PATH] = conf.encode("utf-8")

    if args.logo:
        ppm, dims = logo_ppm(args.logo)
        overlay[LOGO_PATH] = ppm
        print(f"logo: {args.logo}, {dims[0]}x{dims[1]}")
    for item in args.file or []:
        src, sep, dest = item.rpartition(":")
        if not sep or not dest.startswith("/"):
            fail(f"--file needs src:/absolute/dest, got {item}")
        with open(src, "rb") as f:
            overlay[dest.lstrip("/")] = f.read()

    files[overlay_name or OVERLAY] = cpio_write(overlay)
    files[grub_name] = grub_with_overlay(files[grub_name].decode("utf-8")).encode("utf-8")

    img[offset:offset + size] = make_fat16(size, files, fs.label, fs.volume_id, offset // SECTOR)
    out = args.output
    with open(out, "wb") as f:
        f.write(img)
    digests = [(hashlib.sha256(img).hexdigest(), os.path.basename(out))]
    if args.gzip:
        gz = gzip.compress(bytes(img), 9, mtime=0)
        with open(out + ".gz", "wb") as f:
            f.write(gz)
        digests.append((hashlib.sha256(gz).hexdigest(), os.path.basename(out) + ".gz"))
    with open(out + ".sha256", "w") as f:
        f.writelines(f"{d}  {n}\n" for d, n in digests)
    print(f"image: {out}")
    for key, value in sorted(effective(conf).items()):
        print(f"  {key} = {value}")


def cmd_show(args):
    img = read_image(args.image)
    offset, size = esp_partition(img)
    fs = FatImage(bytes(img[offset:offset + size]))
    print(f"ESP: FAT{fs.width}, {size // 1048576} MiB, label {fs.label.decode().strip()}, "
          f"id {fs.volume_id:08X}")
    for name in sorted(fs.files):
        print(f"  {len(fs.files[name]):>10}  {name}")
    overlay_name = fs.find(OVERLAY)
    if overlay_name:
        overlay = cpio_read(fs.files[overlay_name])
        print("overlay:")
        for name in sorted(overlay):
            print(f"  {len(overlay[name]):>10}  /{name}")
        conf = overlay.get(CONF_PATH)
    else:
        print("overlay: none")
        conf = cpio_read(fs.files[fs.find(ROOTFS)]).get(CONF_PATH)
    if conf:
        print("settings:")
        for key, value in sorted(effective(conf.decode("utf-8")).items()):
            print(f"  {key} = {value}")


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    sub = ap.add_subparsers(dest="cmd", required=True)
    b = sub.add_parser("build", help="make a custom image")
    b.add_argument("-i", "--image", required=True, help="released image, .img or .img.gz")
    b.add_argument("-o", "--output", required=True, help="new .img")
    b.add_argument("--config", help="cakes-pba.conf to use instead of the image's")
    b.add_argument("--set", action="append", metavar="KEY=VALUE", help="setting on top, repeatable")
    b.add_argument("--logo", help="PNG logo, scaled down to %d px if larger" % MAX_LOGO)
    b.add_argument("--file", action="append", metavar="SRC:/DEST", help="extra file for the overlay")
    b.add_argument("--gzip", action="store_true", help="also write .img.gz")
    b.set_defaults(func=cmd_build)
    s = sub.add_parser("show", help="list files and settings of an image")
    s.add_argument("image")
    s.set_defaults(func=cmd_show)
    args = ap.parse_args()
    args.func(args)


if __name__ == "__main__":
    main()
