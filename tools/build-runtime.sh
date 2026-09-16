#!/bin/sh
set -eu

ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
WORKBENCH=${TTWORK:-$(dirname "$ROOT")}
BR_OUT=${BUILDROOT_OUT:-"$WORKBENCH/buildroot-bt/out"}
TARGET="$BR_OUT/target"
STRIP="$BR_OUT/host/bin/arm-buildroot-linux-uclibcgnueabi-strip"
FINAL="$ROOT/out/runtime/ttbt-modern"
TMP="$ROOT/out/runtime/.ttbt-modern.tmp.$$"

rm -rf "$TMP"
mkdir -p "$TMP/bin" "$TMP/lib"

copy_strip() {
    src=$1
    dst=$2
    real=$(readlink -f "$src")
    cp "$real" "$dst"
    "$STRIP" --strip-unneeded "$dst"
}

copy_strip "$TARGET/usr/bin/dbus-daemon" "$TMP/bin/dbus-daemon.exe"
copy_strip "$TARGET/usr/libexec/bluetooth/bluetoothd" "$TMP/bin/bluetoothd.exe"
copy_strip "$TARGET/usr/bin/bluealsa" "$TMP/bin/bluealsa.exe"
copy_strip "$TARGET/usr/bin/bluealsa-aplay" "$TMP/bin/bluealsa-aplay.exe"

"$ROOT/tools/build-agent.sh" "$ROOT/out/userspace" >/dev/null
cp "$ROOT/out/userspace/ttbluez-agent.exe" "$TMP/bin/ttbluez-agent.exe"

while IFS='|' read -r dst src; do
    [ -n "$dst" ] || continue
    copy_strip "$src" "$TMP/lib/$dst"
done <<EOF
ld-uClibc-1.0.59.so|$TARGET/lib/ld-uClibc-1.0.59.so
ld-uClibc.so.0|$TARGET/lib/ld-uClibc-1.0.59.so
ld-uClibc.so.1|$TARGET/lib/ld-uClibc-1.0.59.so
libuClibc-1.0.59.so|$TARGET/lib/libuClibc-1.0.59.so
libc.so.0|$TARGET/lib/libuClibc-1.0.59.so
libgcc_s.so.1|$TARGET/lib/libgcc_s.so.1
libasound.so.2|$TARGET/usr/lib/libasound.so.2.0.0
libasound.so.2.0.0|$TARGET/usr/lib/libasound.so.2.0.0
libbluetooth.so.3|$TARGET/usr/lib/libbluetooth.so.3.19.16
libbluetooth.so.3.19.16|$TARGET/usr/lib/libbluetooth.so.3.19.16
libdbus-1.so.3|$TARGET/usr/lib/libdbus-1.so.3.38.3
libdbus-1.so.3.38.3|$TARGET/usr/lib/libdbus-1.so.3.38.3
libexpat.so.1|$TARGET/usr/lib/libexpat.so.1.12.4
libexpat.so.1.12.4|$TARGET/usr/lib/libexpat.so.1.12.4
libffi.so.8|$TARGET/usr/lib/libffi.so.8.5.0
libffi.so.8.5.0|$TARGET/usr/lib/libffi.so.8.5.0
libgio-2.0.so.0|$TARGET/usr/lib/libgio-2.0.so.0.8800.3
libgio-2.0.so.0.8800.3|$TARGET/usr/lib/libgio-2.0.so.0.8800.3
libglib-2.0.so.0|$TARGET/usr/lib/libglib-2.0.so.0.8800.3
libglib-2.0.so.0.8800.3|$TARGET/usr/lib/libglib-2.0.so.0.8800.3
libgmodule-2.0.so.0|$TARGET/usr/lib/libgmodule-2.0.so.0.8800.3
libgmodule-2.0.so.0.8800.3|$TARGET/usr/lib/libgmodule-2.0.so.0.8800.3
libgobject-2.0.so.0|$TARGET/usr/lib/libgobject-2.0.so.0.8800.3
libgobject-2.0.so.0.8800.3|$TARGET/usr/lib/libgobject-2.0.so.0.8800.3
libiconv.so.2|$TARGET/usr/lib/libiconv.so.2.7.1
libiconv.so.2.7.1|$TARGET/usr/lib/libiconv.so.2.7.1
libpcre2-8.so.0|$TARGET/usr/lib/libpcre2-8.so.0.16.0
libpcre2-8.so.0.16.0|$TARGET/usr/lib/libpcre2-8.so.0.16.0
libsbc.so.1|$TARGET/usr/lib/libsbc.so.1.3.1
libsbc.so.1.3.1|$TARGET/usr/lib/libsbc.so.1.3.1
libz.so.1|$TARGET/usr/lib/libz.so.1.3.2
libz.so.1.3.2|$TARGET/usr/lib/libz.so.1.3.2
EOF

(
    cd "$TMP"
    find . -type f ! -name SHA256SUMS -print0 | sort -z | xargs -0 sha256sum > SHA256SUMS
)

mkdir -p "$(dirname "$FINAL")"
rm -rf "$FINAL"
mv "$TMP" "$FINAL"

echo "runtime built: $FINAL"
du -sh "$FINAL"
sha256sum "$FINAL/bin/"*
