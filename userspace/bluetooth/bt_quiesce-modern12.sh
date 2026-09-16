#!/bin/sh
LOG=/sdcard/BT-QUIESCE.LOG

# Stop every process whose /proc/PID/comm begins with PREFIX. Linux truncates
# comm to 15 bytes, so prefix matching is intentional for FAT .exe names.
stop_prefix() {
    prefix="$1"
    label="$2"

    # TERM all matches first.
    for d in /proc/[0-9]*; do
        pid=${d#/proc/}
        comm="$(/bin/cat "$d/comm" 2>/dev/null)"
        case "$comm" in
            "$prefix"*)
                echo "TERM $label pid=$pid comm=$comm"
                /bin/kill -TERM "$pid" 2>/dev/null || true
                ;;
        esac
    done

    # Give normal teardown up to 3 seconds. This BusyBox ash was built without
    # arithmetic expansion support, so use a fixed-count loop.
    for _wait in 1 2 3; do
        alive=0
        for d in /proc/[0-9]*; do
            pid=${d#/proc/}
            comm="$(/bin/cat "$d/comm" 2>/dev/null)"
            case "$comm" in
                "$prefix"*) alive=1 ;;
            esac
        done
        /bin/test "$alive" -eq 0 && return 0
        /bin/sleep 1
    done

    # Do not let a wedged userspace daemon block hardware restart forever.
    for d in /proc/[0-9]*; do
        pid=${d#/proc/}
        comm="$(/bin/cat "$d/comm" 2>/dev/null)"
        case "$comm" in
            "$prefix"*)
                echo "KILL $label pid=$pid comm=$comm"
                /bin/kill -KILL "$pid" 2>/dev/null || true
                ;;
        esac
    done
    /bin/sleep 1
}

{
    echo "=== modern BT/audio quiesce begin ==="
    /bin/cat /proc/meminfo 2>/dev/null | /bin/grep -E 'MemFree|MemAvailable' || true

    stop_prefix "ttcpumon" "ttcpumon"
    if /bin/test -f /run/TTCPU-MODERN12.LOG; then
        /bin/cp /run/TTCPU-MODERN12.LOG /sdcard/TTCPU-MODERN12.LOG 2>/dev/null || true
    fi

    stop_prefix "bluealsa-aplay" "bluealsa-aplay"
    stop_prefix "bluealsa" "bluealsa"
    stop_prefix "hci_event_log" "hci_event_logger"
    stop_prefix "ttbluez-agent" "ttbluez-agent"
    stop_prefix "bluetoothd" "bluetoothd"

    if /bin/test -d /var/lib/bluetooth; then
        /bin/rm -rf /sdcard/bluez-store
        /bin/mkdir -p /sdcard/bluez-store
        /bin/cp -R /var/lib/bluetooth/* /sdcard/bluez-store/ 2>/dev/null || true
    fi

    echo "hci0 down"
    /sbin/hciconfig hci0 down 2>/dev/null || true
    stop_prefix "hciattach" "hciattach"
    stop_prefix "dbus-daemon" "dbus-daemon"

    echo "=== remaining relevant processes ==="
    for d in /proc/[0-9]*; do
        pid=${d#/proc/}
        comm="$(/bin/cat "$d/comm" 2>/dev/null)"
        case "$comm" in
            bluealsa*|ttbluez-agent*|bluetoothd*|dbus-daemon*|hciattach*|hci_event_log*)
                echo "LEFT pid=$pid comm=$comm"
                ;;
        esac
    done

    /bin/sync
    echo "=== modern BT/audio quiesce complete ==="
} >> "$LOG" 2>&1
exit 0
