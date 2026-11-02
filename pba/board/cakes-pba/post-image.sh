#!/bin/sh
# $1 = images dir, $2 = profile (default or hardened)
# Lays out the EFI system partition and builds cakes-pba-<version>-<profile>.img
set -e
BINARIES_DIR=$1
PROFILE=${2:-default}
BOARD_DIR=$(cd "$(dirname "$0")" && pwd)
VERSION=${CAKES_PBA_VERSION:-$(cat "$BOARD_DIR/../../../VERSION")}

ESP="$BINARIES_DIR/efi-part"
mkdir -p "$ESP/cakes"
cp "$BINARIES_DIR/bzImage" "$BINARIES_DIR/rootfs.cpio.xz" "$ESP/cakes/"
sed -e "s/@VERSION@/$VERSION/g" "$BOARD_DIR/grub.cfg" > "$ESP/EFI/BOOT/grub.cfg"
if [ "$PROFILE" = hardened ]; then
	sed -i -e '/#@service-begin/,/#@service-end/d' "$ESP/EFI/BOOT/grub.cfg"
fi

support/scripts/genimage.sh -c "$BOARD_DIR/genimage.cfg"

IMG="cakes-pba-$VERSION-$PROFILE.img"
mv "$BINARIES_DIR/cakes-pba.img" "$BINARIES_DIR/$IMG"
gzip -9 -n -k -f "$BINARIES_DIR/$IMG"
(cd "$BINARIES_DIR" && sha256sum "$IMG" "$IMG.gz" > "$IMG.sha256")
echo "PBA image: $BINARIES_DIR/$IMG"
