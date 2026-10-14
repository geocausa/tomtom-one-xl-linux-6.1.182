# TomTom ONE XL Bluetooth shell packaging — 2026-10-14

## Hardware result

Modern14 A2DP was exercised on real TomTom ONE XL hardware with two phones, including a Galaxy S6. The previously obvious cadence/stutter and ~3–4 s A/V delay were not reproduced. Remaining A/V offset was small/negligible for this device. Phone-side volume control works when `bluealsa-aplay` uses `--volume=software`.

Modern14 queue telemetry also showed no multi-second host-side reservoir in the good run: Bluetooth socket `rmem/backlog/drops` stayed non-problematic and downstream ALSA/PCM buffering was in the sub-second range.

## Important live-image discovery

Do not trust `/sdcard/boot_dmesg.txt` as proof of the currently running kernel. On 2026-10-14 that file still described build #71, while `/proc/version` on the live device reported:

```
Linux version 6.1.182 ... #174 Fri Oct 9 12:24:19 BST 2026
```

The FAT `ttsystem` before the final shell packaging had SHA-256:

```
2b75b07b083fbb4530500f95d174cfab1b51ca0bfd7b9d3e099f1bb84080fc2c
```

Its second TTBL section is the zImage used by the live system. Replacing only the first TTBL section with another cpio did **not** replace the live rootfs. For this build path, the authoritative rootfs is the initramfs embedded inside the zImage (`CONFIG_INITRAMFS_SOURCE`).

Therefore future shell/rootfs packaging must rebuild the zImage with the desired embedded initramfs. Do not assume the first TTBL section overlays/replaces the embedded rootfs.

## Kernel lineage

The local `k6` source tree contained no non-generated kernel source changes after the live #174 build time. The later #175 local build differed by rootfs additions (`usr/bin/aplay` and `etc/asound.conf`) rather than driver source changes.

For the final shell image, the ARM config was restored from the pre-existing `.config.old`, `CONFIG_INITRAMFS_SOURCE` was pointed at the production rootfs staging tree, and the kernel was rebuilt with:

```
ARCH=arm
CROSS_COMPILE=arm-linux-gnueabi-
```

The resulting zImage identifies as build #176. No TomTom driver logic was intentionally changed for this packaging step.

## Production shell layout

The home screen remains four tiles:

- CALIBRATE
- TOOLS
  - Terminal
  - USB Network & SSH
- EXPLORER
- BLUETOOTH

The Bluetooth screen exposes START AUDIO, SCAN PHONES and STOP AUDIO. Phone volume control is used; the launcher no longer forces the codec to a loud fixed level when entering the Bluetooth screen.

## Production Bluetooth startup

`userspace/bluetooth/ttbt_modern_init-production.sh` is idempotent: it refuses to spawn a duplicate `bluealsa-aplay` worker and reuses an already-attached HCI controller.

The production path uses the external `/sdcard/ttbt-modern` runtime, the hardware-proven 42968 Hz ALSA bridge, SBC max bitpool 35 runtime, BlueALSA RT priority, `bluealsa-aplay --volume=software`, and temporary logs under `/tmp` to avoid continuous SD-card writes.

The current Oct-9 `csrinit` binary (SHA-256 `50665bb1...`) failed CSR BCSP sync during the 2026-10-14 hardware session. The preserved modern10 `csrinit` (SHA-256 `8e8b3870a922a8dfa91836d3736acf9d1a53507d792baa286c5284db0694beb7`) succeeded immediately after the CSR reset helper and is the one packaged for production.

## Final image built/deployed in this session

Candidate SHA-256:

```
c2f0865d1ca39fa8972e21fb825ae918fbb01b402b6af48506b778a6b2c6eff0
```

The candidate was written as `/sdcard/ttsystem`, read back over SSH, and hash-verified before reboot testing.

A RAM-only launcher swap was also used before reboot to verify the UI. The framebuffer showed the intended CALIBRATE / TOOLS / EXPLORER / BLUETOOTH four-tile layout. That RAM swap is not a persistence mechanism; the rebuilt embedded-initramfs image is the persistent fix.
