#!/bin/sh
INIT=/usr/bin/ttbt_modern_init.sh

case "$1" in
    start|init|speaker)
        /bin/sh "$INIT"
        ;;
    stop)
        echo "Stopping Bluetooth audio userspace..."
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
        echo "Bluetooth audio stopped; controller remains attached"
        ;;
    scan)
        /sbin/hciconfig hci0 up >/dev/null 2>&1 || {
            echo "Bluetooth is not started. Tap START AUDIO first."
            exit 1
        }
        /sbin/hciconfig hci0 piscan >/dev/null 2>&1 || true
        /bin/hcitool scan --flush --length=4 2>&1
        ;;
    status)
        /sbin/hciconfig hci0 2>&1 || true
        echo "--- Bluetooth userspace ---"
        /bin/ps | /bin/grep -E 'bluealsa|bluetoothd|ttbluez-agent|dbus-daemon' 2>/dev/null || true
        ;;
    *)
        echo "usage: $0 start|stop|scan|status"
        exit 2
        ;;
esac
