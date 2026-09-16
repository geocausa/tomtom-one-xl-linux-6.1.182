#!/bin/sh
MOD=/sdcard/ttbt-modern

if /bin/test ! -f "$MOD/bin/dbus-daemon.exe"; then
    echo "ERROR: modern Bluetooth runtime missing: $MOD"
    exit 10
fi

/bin/mkdir -p /var/lib/bluetooth /run/dbus
if /bin/test -d /sdcard/bluez-store; then
    /bin/cp -R /sdcard/bluez-store/* /var/lib/bluetooth/ 2>/dev/null || true
fi

# Proven legacy CSR hardware bring-up: original glibc environment only.
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

# Start D-Bus synchronously enough to know whether it actually initialized.
# --fork returns only after successful setup; --nosyslog keeps diagnostics in FAT.
DBLOG=/sdcard/BT-MODERN-DBUS.LOG
: > "$DBLOG"
echo "=== pre-start meminfo ===" >> "$DBLOG"
/bin/cat /proc/meminfo >> "$DBLOG" 2>&1
LD_LIBRARY_PATH="$MOD/lib" "$MOD/bin/dbus-daemon.exe" \
    --config-file=/etc/ttbt/dbus-system.conf --fork --nopidfile --nosyslog \
    --print-address=1 --print-pid=1 >> "$DBLOG" 2>&1
DBRC=$?
echo "dbus_start_rc=$DBRC" >> "$DBLOG"
echo "=== post-start state ===" >> "$DBLOG"
/bin/ps >> "$DBLOG" 2>&1 || true
/bin/ls -ld /run /run/dbus /run/dbus/system_bus_socket >> "$DBLOG" 2>&1 || true
/bin/cat /proc/meminfo >> "$DBLOG" 2>&1
/bin/dmesg | /bin/tail -n 80 >> "$DBLOG" 2>&1 || true
if /bin/test "$DBRC" -ne 0 || /bin/test ! -S /run/dbus/system_bus_socket; then
    echo "modern D-Bus failed rc=$DBRC"
    exit 20
fi

BZLOG=/sdcard/BT-MODERN-BLUEZ.LOG
: > "$BZLOG"
echo "=== pre-bluez meminfo ===" >> "$BZLOG"
/bin/cat /proc/meminfo >> "$BZLOG" 2>&1
LD_LIBRARY_PATH="$MOD/lib" "$MOD/bin/bluetoothd.exe" -n -d -f /etc/ttbt/main.conf \
    >> "$BZLOG" 2>&1 &
BZPID=$!
echo "bluetoothd_pid=$BZPID" >> "$BZLOG"
/bin/sleep 2
echo "=== bluez state after 2s ===" >> "$BZLOG"
if /bin/kill -0 "$BZPID" 2>/dev/null; then
    echo "bluetoothd_alive=YES" >> "$BZLOG"
    /bin/cat "/proc/$BZPID/status" >> "$BZLOG" 2>&1 || true
    /bin/cat /proc/meminfo >> "$BZLOG" 2>&1
else
    echo "bluetoothd_alive=NO" >> "$BZLOG"
    set +e
    wait "$BZPID"
    BZRC=$?
    set -e
    echo "bluetoothd_exit_rc=$BZRC" >> "$BZLOG"
    /bin/cat /proc/meminfo >> "$BZLOG" 2>&1
    echo "=== dmesg tail ===" >> "$BZLOG"
    /bin/dmesg | /bin/busybox tail -n 120 >> "$BZLOG" 2>&1 || true
    echo "bluetoothd failed rc=$BZRC"
    exit 21
fi

: > /sdcard/BT-MODERN-AGENT.LOG
LD_LIBRARY_PATH="$MOD/lib" "$MOD/bin/ttbluez-agent.exe" \
    >> /sdcard/BT-MODERN-AGENT.LOG 2>&1 &
/bin/sleep 1
/bin/ps | /bin/grep '[t]tbluez-agent' >/dev/null 2>&1 || { echo "pairing agent failed"; exit 22; }

: > /sdcard/BT-MODERN-BLUEALSA.LOG
LD_LIBRARY_PATH="$MOD/lib" "$MOD/bin/bluealsa.exe" -p a2dp-sink -c SBC \
    --a2dp-force-audio-cd --initial-volume=100 --keep-alive=0 \
    >> /sdcard/BT-MODERN-BLUEALSA.LOG 2>&1 &
/bin/sleep 1
/bin/ps | /bin/grep '[b]luealsa' >/dev/null 2>&1 || { echo "bluealsa failed"; exit 23; }

: > /sdcard/BT-MODERN-APLAY.LOG
LD_LIBRARY_PATH="$MOD/lib" "$MOD/bin/bluealsa-aplay.exe" -D ttbt \
    --pcm-buffer-time=100000 --pcm-period-time=20000 \
    --volume=none --single-audio 00:00:00:00:00:00 \
    >> /sdcard/BT-MODERN-APLAY.LOG 2>&1 &
/bin/sleep 1
/bin/ps | /bin/grep '[b]luealsa-aplay' >/dev/null 2>&1 || { echo "bluealsa-aplay failed"; exit 24; }

if /bin/test -x /usr/bin/hci_event_logger; then
    : > /sdcard/BT-HCI-EVENTS.LOG
    /usr/bin/hci_event_logger >> /sdcard/BT-HCI-EVENTS.LOG 2>&1 &
fi

echo "Modern Bluetooth ready: BlueZ 5.86 + BlueALSA 4.3.1"
echo "Runtime: direct FAT ELF execution (low-RAM)"
echo "A2DP: SBC 44.1k -> ALSA resample -> 42.968k hardware"
echo "Pairing: discoverable/always pairable; legacy PIN fallback 0000"
