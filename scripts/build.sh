#!/usr/bin/env bash
# Build the CakesCats PBA image with Buildroot. Needs a Linux x86_64 host
# (or the container from the Dockerfile).
#
#   scripts/build.sh [--profile default|hardened] [--logo logo.png] [--jobs N]
#
# Downloads and caches go to $CAKES_WORK (default ~/.cache/cakes-pba),
# finished images to dist/.
set -euo pipefail

BUILDROOT_VERSION=2026.02.3
BUILDROOT_SHA256=5a59e7501b0b4ec52c41f4bfa79412320e0b37eae5f719605a258e8d0c6fc7fb

TOP=$(cd "$(dirname "$0")/.." && pwd)
PROFILE=default
JOBS=$(nproc)
while [ $# -gt 0 ]; do
	case "$1" in
	--profile) PROFILE=$2; shift 2 ;;
	--logo) export CAKES_PBA_LOGO=$(realpath "$2"); shift 2 ;;
	--jobs) JOBS=$2; shift 2 ;;
	-h|--help) sed -n '2,9p' "$0"; exit 0 ;;
	*) echo "unknown option $1" >&2; exit 1 ;;
	esac
done
case "$PROFILE" in default|hardened) ;; *) echo "profile must be default or hardened" >&2; exit 1 ;; esac

WORK=${CAKES_WORK:-$HOME/.cache/cakes-pba}
BR=$WORK/buildroot-$BUILDROOT_VERSION
OUT=$WORK/output-$PROFILE
DIST=$TOP/dist
mkdir -p "$WORK" "$DIST"

if [ ! -d "$BR" ]; then
	tarball=$WORK/buildroot-$BUILDROOT_VERSION.tar.xz
	[ -f "$tarball" ] || wget -q -O "$tarball" "https://buildroot.org/downloads/buildroot-$BUILDROOT_VERSION.tar.xz"
	echo "$BUILDROOT_SHA256  $tarball" | sha256sum -c -
	tar -C "$WORK" -xf "$tarball"
fi

# version shown in the PBA and used in the image name
if desc=$(git -C "$TOP" describe --tags --match 'v*' --dirty 2>/dev/null); then
	CAKES_PBA_VERSION=${desc#v}
else
	CAKES_PBA_VERSION=$(cat "$TOP/VERSION")
	hash=$(git -C "$TOP" rev-parse --short HEAD 2>/dev/null || true)
	[ -n "$hash" ] && CAKES_PBA_VERSION="$CAKES_PBA_VERSION+g$hash"
fi
export CAKES_PBA_VERSION
export BR2_DL_DIR=$WORK/dl

defconfig=$OUT/cakes_pba_${PROFILE}_defconfig
mkdir -p "$OUT"
cat "$TOP/pba/configs/cakes_pba_defconfig" > "$defconfig"
[ "$PROFILE" = hardened ] && cat "$TOP/pba/configs/hardened.fragment" >> "$defconfig"

make -C "$BR" O="$OUT" BR2_EXTERNAL="$TOP/pba" BR2_DEFCONFIG="$defconfig" defconfig
# the sedutil sources are rsynced from this checkout: always rebuild them
make -C "$BR" O="$OUT" cakes-sedutil-dirclean
make -C "$BR" O="$OUT" -j"$JOBS"

cp "$OUT"/images/cakes-pba-*-"$PROFILE".img* "$DIST/"
ls -l "$DIST"
