#!/bin/sh
set -eu

ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
WORKBENCH=${TTWORK:-$(dirname "$ROOT")}
BR_SRC=${BUILDROOT_SRC:-"$WORKBENCH/third_party_src/buildroot"}
BR_OUT=${BUILDROOT_OUT:-"$WORKBENCH/buildroot-bt/out"}
PATCH="$ROOT/third_party/patches/bluez-alsa/0001-tomtom-cap-sbc-bitpool35-and-log.patch"
BUILD="$BR_OUT/build/bluez-alsa-4.3.1"

test -d "$BR_SRC"
test -d "$BR_OUT"
test -f "$PATCH"
test -x /usr/bin/gnuinstall

SHIM=$(mktemp -d)
TMP=
cleanup() {
    rm -rf "$SHIM"
    [ -z "$TMP" ] || rm -f "$TMP"
}
trap cleanup EXIT INT TERM
ln -s /usr/bin/gnuinstall "$SHIM/install"

brmake() {
    PATH="$SHIM:$PATH" make -C "$BR_SRC" O="$BR_OUT" "$@"
}

brmake bluez-alsa-dirclean
brmake bluez-alsa-patch
patch -d "$BUILD" -p1 --batch < "$PATCH"
brmake bluez-alsa-rebuild -j"${JOBS:-4}"

STRIP="$BR_OUT/host/bin/arm-buildroot-linux-uclibcgnueabi-strip"
TMP=$(mktemp)
cp "$BR_OUT/target/usr/bin/bluealsa" "$TMP"
"$STRIP" --strip-unneeded "$TMP"

echo "rebuilt stripped BlueALSA:"
sha256sum "$TMP"
