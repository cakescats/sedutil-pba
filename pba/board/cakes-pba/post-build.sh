#!/bin/sh
# $1 = target dir, $2 = profile (default or hardened)
set -e
TARGET_DIR=$1
PROFILE=${2:-default}
TOP=$(cd "$(dirname "$0")/../../.." && pwd)
VERSION=${CAKES_PBA_VERSION:-$(cat "$TOP/VERSION")}

# logo: branding/logo.ppm, or a PNG given in CAKES_PBA_LOGO
mkdir -p "$TARGET_DIR/usr/share/cakes-pba"
if [ -n "$CAKES_PBA_LOGO" ]; then
	python3 "$TOP/branding/png2ppm.py" "$CAKES_PBA_LOGO" "$TARGET_DIR/usr/share/cakes-pba/logo.ppm"
else
	cp "$TOP/branding/logo.ppm" "$TARGET_DIR/usr/share/cakes-pba/logo.ppm"
fi

echo "CakesCats PBA $VERSION ($PROFILE)" > "$TARGET_DIR/etc/cakes-pba-release"

# our inittab runs nothing from /etc/init.d, drop the scripts
rm -f "$TARGET_DIR"/etc/init.d/S*
