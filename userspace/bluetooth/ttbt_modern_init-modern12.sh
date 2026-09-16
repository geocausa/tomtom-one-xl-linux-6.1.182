#!/bin/sh
# Production-like startup with one deliberate metric: retain BlueALSA warnings
# so PCM-overrun count can be measured without full BlueZ/HCI debug overhead.
MOD=/sdcard/ttbt-modern

# This BusyBox ash is built without arithmetic expansion support.
RUNTIME_READY=0
for _wait in 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15; do
    if /bin/test -f "$MOD/bin/dbus-daemon.exe"; then
        RUNTIME_READY=1
        break
    fi
    /bin/sleep 1
done
if /bin/test "$RUNTIME_READY" != 1; then
    echo "ERROR: modern Bluetooth runtime missing: $MOD"
    exit 10
fi

/bin/mkdir -p /var/lib/bluetooth /run/dbus
if /bin/test -d /sdcard/bluez-store; then
    /bin/cp -R /sdcard/bluez-store/* /var/lib/bluetooth/ 2>/dev/null || true
fi

unset LD_LIBRARY_PATH
if /sbin/hciconfig hci0 >/dev/null 2>&1; then
    echo "CSR already attached; reusing hci0"
else
    echo "CSR init: BCSP /dev/ttySAC2 460800 26MHz"
    /bin/csrinit bcsp /dev/ttySAC2 460800 26000000 || exit 11
    /sbin/hciattach /dev/ttySAC2 bcsp 460800 || exit 12
fi
/sbin/hciconfig hci0 up || exit 13

/bin/rm -f /run/dbus/system_bus_socket /run/dbus-daemon.pid
LD_LIBRARY_PATH="$MOD/lib" "$MOD/bin/dbus-daemon.exe" \
    --config-file=/etc/ttbt/dbus-system.conf --fork --nopidfile --nosyslog \
    >/dev/null 2>&1 || exit 20
/bin/test -S /run/dbus/system_bus_socket || exit 20

BZ="$MOD/bin/bluetoothd.real.exe"
/bin/test -x "$BZ" || BZ="$MOD/bin/bluetoothd.exe"
: > /sdcard/BT-MODERN12-BLUEZ.LOG
LD_LIBRARY_PATH="$MOD/lib" "$BZ" -n -f /etc/ttbt/main.conf \
    >> /sdcard/BT-MODERN12-BLUEZ.LOG 2>&1 &
BZPID=$!
/bin/sleep 1
/bin/kill -0 "$BZPID" 2>/dev/null || { echo "bluetoothd failed"; exit 21; }

: > /sdcard/BT-MODERN12-AGENT.LOG
LD_LIBRARY_PATH="$MOD/lib" "$MOD/bin/ttbluez-agent.exe" \
    >> /sdcard/BT-MODERN12-AGENT.LOG 2>&1 &
/bin/sleep 1
/bin/ps | /bin/grep '[t]tbluez-agent' >/dev/null 2>&1 || {
    echo "pairing agent failed"; exit 22;
}

: > /sdcard/BT-MODERN12-BLUEALSA.LOG
LD_LIBRARY_PATH="$MOD/lib" "$MOD/bin/bluealsa.exe" -p a2dp-sink -c SBC \
    --a2dp-force-audio-cd --initial-volume=100 --keep-alive=0 \
    --io-rt-priority=5 \
    >> /sdcard/BT-MODERN12-BLUEALSA.LOG 2>&1 &
/bin/sleep 1
/bin/ps | /bin/grep '[b]luealsa' >/dev/null 2>&1 || {
    echo "bluealsa failed"; exit 23;
}

: > /sdcard/BT-MODERN12-APLAY.LOG
RTEXEC=/sdcard/ttdiag/tt-rt-exec.exe
if /bin/test -x "$RTEXEC"; then
    LD_LIBRARY_PATH="$MOD/lib" "$RTEXEC" 6 "$MOD/bin/bluealsa-aplay.exe" -D ttbt \
        --pcm-buffer-time=100000 --pcm-period-time=20000 \
        --volume=none --single-audio 00:00:00:00:00:00 \
        >> /sdcard/BT-MODERN12-APLAY.LOG 2>&1 &
else
    echo "tt-rt-exec missing; starting aplay with normal scheduler" \
        >> /sdcard/BT-MODERN12-APLAY.LOG
    LD_LIBRARY_PATH="$MOD/lib" "$MOD/bin/bluealsa-aplay.exe" -D ttbt \
        --pcm-buffer-time=100000 --pcm-period-time=20000 \
        --volume=none --single-audio 00:00:00:00:00:00 \
        >> /sdcard/BT-MODERN12-APLAY.LOG 2>&1 &
fi
/bin/sleep 1
/bin/ps | /bin/grep '[b]luealsa-aplay' >/dev/null 2>&1 || {
    echo "bluealsa-aplay failed"; exit 24;
}

if /bin/test -x /sdcard/ttdiag/ttcpumon.exe; then
    /bin/rm -f /run/TTCPU-MODERN12.LOG
    LD_LIBRARY_PATH="$MOD/lib" /sdcard/ttdiag/ttcpumon.exe \
        /run/TTCPU-MODERN12.LOG >/dev/null 2>&1 &
fi

echo "Modern12 scheduler metric mode ready"
echo "A2DP: SBC max bitpool 35; BlueALSA RT=5; aplay RT=6"
