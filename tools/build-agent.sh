#!/bin/sh
set -eu

ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
WORKBENCH=${TTWORK:-$(dirname "$ROOT")}
BR_OUT=${BUILDROOT_OUT:-"$WORKBENCH/buildroot-bt/out"}
HOST="$BR_OUT/host"
CC="$HOST/bin/arm-buildroot-linux-uclibcgnueabi-gcc"
PKG="$HOST/bin/pkg-config"
DEST=${1:-"$ROOT/out/userspace"}
OUT="$DEST/ttbluez-agent.exe"

mkdir -p "$DEST"
test -x "$CC"
test -x "$PKG"

"$CC" -Os -s -o "$OUT" \
    "$ROOT/userspace/bluetooth/ttbluez-agent.c" \
    $("$PKG" --cflags --libs dbus-1)

echo "built $OUT"
sha256sum "$OUT"
