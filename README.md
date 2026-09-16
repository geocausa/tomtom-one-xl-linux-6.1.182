# TomTom ONE XL — Linux 6.1 Port

Clean consolidation tree for the TomTom ONE XL (Limerick / Samsung S3C2412).

This directory is the new canonical source/build layer. The older
`/home/macbuntu/Documents/ttwork` workbench remains intact as historical evidence
until the clean build is fully validated on hardware.

## Current proven hardware state

- Linux 6.1.182 boots.
- framebuffer, touch, storage and UI work.
- TomTom SFTRST reboot using INFORM0=0x9 is hardware-proven.
- ALSA / S3C2412 I2S / WM8711 local playback is audible.
- CSR BlueCore BCSP at 460800 baud works.
- BlueZ 5.86 + BlueALSA 4.3.1 A2DP sink works.
- iPhone transport cadence is nearly fixed by capping SBC sink max bitpool to 35.

See `docs/PROJECT-AUDIT-20260916.md` and `reference/PROVENANCE.md`.

## Important preservation rule

Do not remove the legacy Buildroot output or old workbench yet.

The exact live runtime and image state were captured at:

    ../checkpoints/20260916-consolidation-p0-live-authority

## Layout

- `kernel/patches/` — kernel source preservation/cleanup patches.
- `userspace/pid1/` — tiny custom PID1.
- `userspace/launcher/` — framebuffer launcher/UI.
- `userspace/diagnostics/` — no-libc tone, volume and HCI tools.
- `userspace/bluetooth/` — pairing agent, startup, quiesce and configs.
- `third_party/patches/bluez-alsa/` — TomTom platform patches.
- `configs/` — current reference configs; clean dev/prod defconfigs follow.
- `tools/` — reproducible build/materialization helpers.
- `out/` — generated output only; safe-to-delete goal.
- `reference/` — manifests and provenance, not generated binaries.

## Reproduce the retry-hardened pairing agent

    ./tools/build-agent.sh

Expected SHA256:

    57d8fc79e301fc8ebc1df0ff5e1efa040907c2cc12eebd58fc2b9c5e33155ebf

## Rebuild patched BlueALSA 4.3.1

    ./tools/rebuild-bluealsa.sh

Expected stripped BlueALSA SHA256:

    417b1cea148c2cf703c6e195c10e5635938bd33a4509d98431cda0514c3d1c9c

## Generate the FAT runtime

After BlueALSA has been rebuilt:

    ./tools/build-runtime.sh

This copies the exact Buildroot target binaries/libraries, strips them, and
materializes SONAME aliases as real files because VFAT cannot preserve Unix
symlinks.

## Production vs debug Bluetooth startup

- `userspace/bluetooth/ttbt_modern_init.sh`: prepared quiet production startup.
  It waits explicitly for /sdcard, avoids normal verbose BlueZ/HCI logging and is
  **not deployed yet**.
- `userspace/bluetooth/ttbt_modern_init-debug-current.sh`: current verbose
  hardware-debug startup preserved from modern8.

The debug path stays authoritative until reboot/overrun validation is complete.

## Kernel source preservation

`kernel/patches/0000-current-k6-preservation.patch` is intentionally unsplit and
not upstream-ready. It exists so pristine Linux 6.1.182 can reproduce the exact
current k6 source state before cleanup begins.

Next work is to split and simplify that patch while continuously checking against
the preservation patch and hardware.
