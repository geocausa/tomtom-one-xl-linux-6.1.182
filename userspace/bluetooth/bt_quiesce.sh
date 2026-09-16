#!/bin/sh
LOG=/sdcard/BT-QUIESCE.LOG

# Stop every process whose /proc/PID/comm begins with PREFIX.  Linux truncates
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

    # Give normal teardown up to 3 seconds.
    n=0
    while /bin/test "$n" -lt 3; do
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
        n=$((n + 1))
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

    # 1. Close ALSA first.  This is the critical ordering: it releases the
    #    WM8711/I2S/DMA stream before Bluetooth or the controller disappear.
    stop_prefix "bluealsa-aplay" "bluealsa-aplay"

    # 2. Stop codec/transport workers after the PCM consumer is gone.
    stop_prefix "bluealsa" "bluealsa"

    # 3. Passive HCI logging must release its socket before controller down.
    stop_prefix "hci_event_log" "hci_event_logger"

    # 4. Pairing/UI policy agent, then BlueZ.  Match both bluetoothd.exe
    #    wrapper and bluetoothd.real.exe by common prefix.
    stop_prefix "ttbluez-agent" "ttbluez-agent"
    stop_prefix "bluetoothd" "bluetoothd"

    # BlueZ has now had a chance to flush its bond database. Persist it.
    if /bin/test -d /var/lib/bluetooth; then
        /bin/rm -rf /sdcard/bluez-store
        /bin/mkdir -p /sdcard/bluez-store
        /bin/cp -R /var/lib/bluetooth/* /sdcard/bluez-store/ 2>/dev/null || true
    fi

    # 5. Shut down the controller only after userspace has released it.
    echo "hci0 down"
    /sbin/hciconfig hci0 down 2>/dev/null || true
    stop_prefix "hciattach" "hciattach"

    # 6. System bus is last because BlueZ/BlueALSA use it during teardown.
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
