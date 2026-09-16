#!/bin/sh
case "$1" in
    init)
        if /bin/ps | /bin/grep '[b]luealsa-aplay' >/dev/null 2>&1; then
            echo "Modern Bluetooth stack already running."
            echo "BlueZ 5.86 + BlueALSA 4.3.1"
            exit 0
        fi
        /usr/bin/ttbt_modern_init.sh
        ;;
    scan)
        echo "Pair from the phone: TomTom ONE XL is discoverable."
        echo "Legacy inquiry scan:"
        /bin/hcitool scan --flush --length=4 2>&1 || true
        ;;
    speaker)
        echo "TomTom speaker mode - hardware golden multitone"
        echo "=== ALSA card ==="
        /bin/cat /proc/asound/cards 2>&1 || true
        echo "=== PCM ==="
        /bin/cat /proc/asound/pcm 2>&1 || true
        if /bin/test -e /dev/snd/pcmC0D0p; then
            /bin/printf "Codec volume: "
            /usr/bin/ttvolume 2>&1 || true
            echo "Playing strong 500 Hz -> 1 kHz -> 2 kHz test..."
            /usr/bin/tttone-native 2>&1
        else
            echo "ERROR: /dev/snd/pcmC0D0p not present"
        fi
        /bin/dmesg > /sdcard/AUDIO-TONE8-DMESG.LOG 2>&1 || true
        ;;
    *) echo "usage: $0 {init|scan|speaker}"; exit 2;;
esac
