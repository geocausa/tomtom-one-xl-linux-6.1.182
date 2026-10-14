#!/bin/sh
# TomTom ONE XL production A2DP sink startup.
# External modern runtime stays on /sdcard to keep the initramfs small.
# BusyBox ash on this device has no arithmetic expansion support.

MOD=/sdcard/ttbt-modern
BTUTIL=/usr/bin/bt_utils

# Idempotent: do not create a second playback worker.
if /bin/ps | /bin/grep '[b]luealsa-aplay.exe' >/dev/null 2>&1; then
    echo "Bluetooth audio already running"
    /sbin/hciconfig hci0 2>/dev/null || true
    exit 0
fi

RUNTIME_READY=0
for _wait in 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15; do
    if /bin/test -f "$MOD/bin/dbus-daemon.exe"; then
        RUNTIME_READY=1
        break
    fi
    /bin/sleep 1
done
if /bin/test "$RUNTIME_READY" != 1; then
    echo "ERROR: Bluetooth runtime missing: $MOD"
    exit 10
fi

if /bin/test ! -x "$BTUTIL" && /bin/test -x /sdcard/modern14/bt_utils.exe; then
    BTUTIL=/sdcard/modern14/bt_utils.exe
fi

/bin/mkdir -p /var/lib/bluetooth /run/dbus
if /bin/test -d /sdcard/bluez-store; then
    /bin/cp -R /sdcard/bluez-store/* /var/lib/bluetooth/ 2>/dev/null || true
fi

unset LD_LIBRARY_PATH
if /sbin/hciconfig hci0 >/dev/null 2>&1; then
    echo "CSR already attached; reusing hci0"
else
    if /bin/test ! -x "$BTUTIL"; then
        echo "ERROR: missing CSR reset helper"
        exit 11
    fi
    echo "Resetting CSR BlueCore"
    "$BTUTIL" reset || exit 11
    /bin/sleep 1

    echo "CSR init: BCSP /dev/ttySAC2 460800 26MHz"
    /bin/csrinit bcsp /dev/ttySAC2 460800 26000000 || exit 12
    /bin/sleep 2
    /sbin/hciattach /dev/ttySAC2 bcsp 460800 || exit 13
fi
/sbin/hciconfig hci0 up || exit 14

# Remove partial userspace state from a failed previous start. Do not kill
# hciattach: keeping the controller attached makes a restart cheap and safe.
for d in /proc/[0-9]*; do
    /bin/test -r "$d/cmdline" || continue
    CMD=$(/bin/cat "$d/cmdline" 2>/dev/null)
    case "$CMD" in
        *bluealsa-aplay.exe*|*bluealsa.exe*|*ttbluez-agent.exe*|*bluetoothd.real.exe*|*bluetoothd.exe*|*dbus-daemon.exe*)
            /bin/kill "${d#/proc/}" 2>/dev/null || true
            ;;
    esac
done
/bin/sleep 1
/bin/rm -f /run/dbus/system_bus_socket /run/dbus-daemon.pid

LD_LIBRARY_PATH="$MOD/lib" "$MOD/bin/dbus-daemon.exe" \
    --config-file=/etc/ttbt/dbus-system.conf --fork --nopidfile --nosyslog \
    >/dev/null 2>&1 || exit 20
/bin/test -S /run/dbus/system_bus_socket || exit 20

BZ="$MOD/bin/bluetoothd.real.exe"
/bin/test -x "$BZ" || BZ="$MOD/bin/bluetoothd.exe"
: > /tmp/BT-A2DP-BLUEZ.LOG
LD_LIBRARY_PATH="$MOD/lib" "$BZ" -n -f /etc/ttbt/main.conf \
    >> /tmp/BT-A2DP-BLUEZ.LOG 2>&1 &
BZPID=$!
/bin/sleep 1
/bin/kill -0 "$BZPID" 2>/dev/null || { echo "bluetoothd failed"; exit 21; }

: > /tmp/BT-A2DP-AGENT.LOG
LD_LIBRARY_PATH="$MOD/lib" "$MOD/bin/ttbluez-agent.exe" \
    >> /tmp/BT-A2DP-AGENT.LOG 2>&1 &
/bin/sleep 1
/bin/ps | /bin/grep '[t]tbluez-agent' >/dev/null 2>&1 || {
    echo "pairing agent failed"; exit 22;
}

: > /tmp/BT-A2DP-BLUEALSA.LOG
LD_LIBRARY_PATH="$MOD/lib" "$MOD/bin/bluealsa.exe" -p a2dp-sink -c SBC \
    --a2dp-force-audio-cd --initial-volume=100 --keep-alive=0 \
    --io-rt-priority=5 \
    >> /tmp/BT-A2DP-BLUEALSA.LOG 2>&1 &
/bin/sleep 2
/bin/ps | /bin/grep '[b]luealsa.exe' >/dev/null 2>&1 || {
    echo "bluealsa failed"; exit 23;
}

: > /tmp/BT-A2DP-APLAY.LOG
RTEXEC=/sdcard/ttdiag/tt-rt-exec.exe
if /bin/test -x "$RTEXEC"; then
    LD_LIBRARY_PATH="$MOD/lib" "$RTEXEC" 6 "$MOD/bin/bluealsa-aplay.exe" -D ttbt \
        --pcm-buffer-time=100000 --pcm-period-time=20000 \
        --volume=software --single-audio 00:00:00:00:00:00 \
        >> /tmp/BT-A2DP-APLAY.LOG 2>&1 &
else
    LD_LIBRARY_PATH="$MOD/lib" "$MOD/bin/bluealsa-aplay.exe" -D ttbt \
        --pcm-buffer-time=100000 --pcm-period-time=20000 \
        --volume=software --single-audio 00:00:00:00:00:00 \
        >> /tmp/BT-A2DP-APLAY.LOG 2>&1 &
fi
/bin/sleep 2
/bin/ps | /bin/grep '[b]luealsa-aplay.exe' >/dev/null 2>&1 || {
    echo "bluealsa-aplay failed"
    /bin/cat /tmp/BT-A2DP-APLAY.LOG 2>/dev/null || true
    exit 24
}

# Phone volume is handled by BlueALSA software volume.

/sbin/hciconfig hci0 piscan >/dev/null 2>&1 || true

echo "Bluetooth audio READY"
echo "Name: TomTom ONE XL"
echo "PIN: 0000"
echo "A2DP: SBC bitpool <=35, 44.1k -> ttbt -> 42968 Hz"
