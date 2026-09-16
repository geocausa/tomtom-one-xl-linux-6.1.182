#!/bin/sh
case "$1" in
    init)
        /usr/bin/ttbt_modern_init.sh
        ;;
    scan)
        echo "Pair from the phone: TomTom ONE XL is discoverable."
        echo "Legacy inquiry scan:"
        /bin/hcitool scan --flush --length=4 2>&1 || true
        ;;
    speaker)
        echo "LOCAL ALSA ISOLATION TEST"
        echo "Bluetooth is NOT used for this test."
        echo "Path: 44.1k -> ALSA ttbt plug -> 42.968k -> I2S/WM8711"
        echo "30 second steady 440 Hz triangle tone."
        echo

        # Ensure Bluetooth playback cannot own the PCM during this discriminator.
        /usr/bin/bt_quiesce.sh >/dev/null 2>&1 || true

        if /bin/test ! -e /dev/snd/pcmC0D0p; then
            echo "ERROR: /dev/snd/pcmC0D0p not present"
            exit 30
        fi
        if /bin/test ! -x /sdcard/ttdiag/ttalsa-stream.exe; then
            echo "ERROR: /sdcard/ttdiag/ttalsa-stream.exe missing"
            exit 31
        fi

        : > /sdcard/TTALSA-STREAM.LOG
        LD_LIBRARY_PATH=/sdcard/ttbt-modern/lib             /sdcard/ttdiag/ttalsa-stream.exe             >> /sdcard/TTALSA-STREAM.LOG 2>&1
        rc=$?

        /bin/cat /sdcard/TTALSA-STREAM.LOG 2>/dev/null || true
        echo "local stream rc=$rc"
        /bin/dmesg > /sdcard/TTALSA-STREAM-DMESG.LOG 2>&1 || true
        exit "$rc"
        ;;
    *)
        echo "usage: $0 {init|scan|speaker}"
        exit 2
        ;;
esac
