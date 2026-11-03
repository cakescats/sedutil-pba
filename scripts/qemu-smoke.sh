#!/usr/bin/env bash
# Boot a PBA image in QEMU with UEFI firmware (OVMF) and check it comes up.
#
#   scripts/qemu-smoke.sh dist/cakes-pba-<version>-default.img [outdir]
#
# 1. kernel + initramfs from the image, kernel log on the serial port:
#    waits for the PBA's "cakes-pba: scan:" line. An emulated NVMe drive
#    (no OPAL) must show up as one drive, none locked.
# 2. the whole image through GRUB: saves a screenshot of the PBA screen and
#    checks most of it is the PBA background colour.
#
# With SSO_KEY set (the image's sso_key), also presses F2, checks the QR
# code's token the way the unlock server does, and saves sso.ppm.
#
# QEMU cannot emulate an OPAL drive, so unlocking itself is tested on hardware.
set -euo pipefail

IMG=$(realpath "$1")
OUT=${2:-$(pwd)/qemu-smoke}
TOP=$(cd "$(dirname "$0")/.." && pwd)
mkdir -p "$OUT"
WORK=$(mktemp -d)
trap 'kill $(jobs -p) 2>/dev/null || true; rm -rf "$WORK"' EXIT

for f in /usr/share/OVMF/OVMF_CODE_4M.fd /usr/share/OVMF/OVMF_CODE.fd /usr/share/ovmf/OVMF.fd \
	/usr/share/qemu/edk2-x86_64-code.fd /opt/homebrew/share/qemu/edk2-x86_64-code.fd \
	/usr/local/share/qemu/edk2-x86_64-code.fd; do
	[ -f "$f" ] && OVMF=$f && break
done
[ -n "${OVMF:-}" ] || { echo "OVMF firmware not found" >&2; exit 1; }

ACCEL="-accel tcg"
[ -w /dev/kvm ] && ACCEL="-accel kvm"
QEMU="qemu-system-x86_64 -machine q35 $ACCEL -m 1024 -smp 2 -nodefaults -vga std
	-drive if=pflash,format=raw,readonly=on,file=$OVMF
	-drive file=$WORK/nvme.raw,format=raw,if=none,id=nv0 -device nvme,drive=nv0,serial=CAKESTEST01"
dd if=/dev/zero of="$WORK/nvme.raw" bs=1048576 count=0 seek=64 2>/dev/null

wait_for() { # file pattern seconds
	for _ in $(seq "$3"); do
		grep -q "$2" "$1" 2>/dev/null && return 0
		sleep 1
	done
	return 1
}

echo "== 1: kernel and initramfs, serial log"
python3 -I "$TOP/scripts/esp-extract.py" "$IMG" "$WORK/esp" >/dev/null
# GRUB loads the overlay a builder image has after the initramfs; the kernel
# unpacks concatenated archives in order, so one file does the same here
cat "$WORK/esp/cakes/rootfs.cpio.xz" > "$WORK/initrd"
[ -f "$WORK/esp/cakes/overlay.cpio" ] && cat "$WORK/esp/cakes/overlay.cpio" >> "$WORK/initrd"
$QEMU -display none -serial "file:$OUT/serial.log" -monitor "unix:$WORK/mon1,server,nowait" \
	-kernel "$WORK/esp/cakes/bzImage" -initrd "$WORK/initrd" \
	-append "console=ttyS0,115200 loglevel=7 libata.allow_tpm=1 reboot=warm" &
QPID=$!
if ! wait_for "$OUT/serial.log" "cakes-pba: scan:" 300; then
	kill $QPID; tail -n 40 "$OUT/serial.log"
	echo "FAIL: PBA did not start" >&2; exit 1
fi
if [ -n "${SSO_KEY:-}" ]; then
	# F2 on the PBA screen must show a QR code whose token the server accepts
	sleep 8 # past the rescans of an empty drive list
	monitor() { python3 -I - "$WORK/mon1" "$1" <<'PY'
import socket, sys, time
s = socket.socket(socket.AF_UNIX)
s.connect(sys.argv[1])
time.sleep(0.5)
s.recv(4096)
s.sendall(sys.argv[2].encode() + b"\n")
time.sleep(2)
PY
	}
	monitor "sendkey f2"
	if ! wait_for "$OUT/serial.log" "cakes-pba: sso:" 30; then
		kill $QPID; echo "FAIL: F2 did not show the QR code" >&2; exit 1
	fi
	monitor "screendump $OUT/sso.ppm"
	url=$(grep -o 'cakes-pba: sso: [^[:space:]]*' "$OUT/serial.log" | tail -n 1 | cut -d' ' -f3)
	python3 -I - "$url" "$SSO_KEY" <<'PY' || { kill $QPID; exit 1; }
# the token format of LinuxPBA/PbaSso.cpp, as the unlock server checks it
import base64, hashlib, hmac, sys, urllib.parse
url = urllib.parse.urlsplit(sys.argv[1])
machine = urllib.parse.unquote(url.path.rsplit("/", 1)[1])
token = urllib.parse.parse_qs(url.query)["c"][0]
raw = base64.b32decode(token)
msg = b"cakes-pba-sso-v1\0" + machine.encode() + b"\0" + raw[:4]
ok = hmac.compare_digest(hmac.new(bytes.fromhex(sys.argv[2]), msg, hashlib.sha512).digest()[:6], raw[4:])
print(f"sso: {url.scheme}://{url.netloc}, machine {machine}, token {'valid' if ok else 'INVALID'}")
sys.exit(0 if ok else 1)
PY
fi
kill $QPID; wait $QPID 2>/dev/null || true
grep "cakes-pba:" "$OUT/serial.log"
grep -q "cakes-pba: scan: 1 drive(s), 0 locked" "$OUT/serial.log" ||
	{ echo "FAIL: expected the NVMe test drive, unlocked" >&2; exit 1; }
if grep -q "cakes-pba: config:" "$OUT/serial.log"; then
	echo "FAIL: config warnings" >&2; exit 1
fi

echo "== 2: full image through GRUB, screenshot"
cp "$IMG" "$WORK/pba.img"
$QEMU -display none -monitor "unix:$WORK/mon,server,nowait" \
	-drive file="$WORK/pba.img",format=raw,if=none,id=usb0 \
	-device qemu-xhci -device usb-storage,drive=usb0,bootindex=0 &
QPID=$!
SHOT_WAIT=${SHOT_WAIT:-90}
sleep "$SHOT_WAIT"
python3 -I - "$WORK/mon" "$OUT/screen.ppm" <<'PY'
import socket, sys, time
s = socket.socket(socket.AF_UNIX)
s.connect(sys.argv[1])
time.sleep(0.5)
s.recv(4096)
s.sendall(f"screendump {sys.argv[2]}\n".encode())
time.sleep(2)
PY
kill $QPID; wait $QPID 2>/dev/null || true
python3 -I - "$OUT/screen.ppm" <<'PY'
import sys
data = open(sys.argv[1], "rb").read()
parts = data.split(b"\n", 3)
w, h = map(int, parts[1].split())
px = parts[3]
bg = sum(1 for i in range(0, w * h * 3, 3) if px[i:i + 3] == b"\x07\x0c\x2b")
share = bg / (w * h)
print(f"screen {w}x{h}, {share:.0%} PBA background")
sys.exit(0 if share > 0.5 else 1)
PY
echo "PASS, screenshot: $OUT/screen.ppm"
