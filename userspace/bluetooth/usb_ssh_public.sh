#!/bin/sh
# Public-safe USB networking + SSH for TomTom ONE XL.
# Host keys are generated on first use and persisted on the SD card.
# User login keys are read from /sdcard/authorized_keys.
/sbin/ifconfig usb0 192.168.7.2 netmask 255.255.255.0 up
echo "usb0: 192.168.7.2 up"
mkdir -p /var/run /etc/dropbear /root/.ssh

# Restore a previously generated host identity from persistent storage.
if /bin/test -d /sdcard/.dropbear; then
    /bin/cp /sdcard/.dropbear/dropbear_rsa_host_key /etc/dropbear/ 2>/dev/null || true
    /bin/cp /sdcard/.dropbear/dropbear_ed25519_host_key /etc/dropbear/ 2>/dev/null || true
fi

# Provision the user's own login key from the SD card, if present.
if /bin/test -f /sdcard/authorized_keys; then
    /bin/cp /sdcard/authorized_keys /root/.ssh/authorized_keys
fi

if ps | grep -q "[d]ropbear"; then
    echo "SSH server: RUNNING"
else
    # -R generates missing host keys; -s disables password authentication.
    /sbin/dropbear -R -s 2>/dev/null
    echo "SSH server: STARTED"
fi

# Persist generated host keys so the device fingerprint remains stable.
mkdir -p /sdcard/.dropbear 2>/dev/null || true
/bin/cp /etc/dropbear/dropbear_rsa_host_key /sdcard/.dropbear/ 2>/dev/null || true
/bin/cp /etc/dropbear/dropbear_ed25519_host_key /sdcard/.dropbear/ 2>/dev/null || true

echo "link: $(cat /sys/class/net/usb0/operstate 2>/dev/null) carrier $(cat /sys/class/net/usb0/carrier 2>/dev/null)"
if /bin/test -f /root/.ssh/authorized_keys; then
    echo "Login key: /sdcard/authorized_keys loaded"
    echo "Connect: ssh root@192.168.7.2"
else
    echo "SSH locked: copy your public key to /authorized_keys on the TomTom"
fi
