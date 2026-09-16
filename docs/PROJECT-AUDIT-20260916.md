# TomTom ONE XL Linux 6.1 Project Audit — 2026-09-16

## Executive summary

The project is technically strong but not yet production-clean or reproducibly packaged. The hard hardware work is mostly successful: Linux 6.1 boots, framebuffer/touch/UI/storage work, local ALSA/I2S/WM8711 playback is proven, CSR Bluetooth works, BlueZ 5.86 + BlueALSA 4.3.1 run, and the iPhone cadence problem was almost eliminated by capping inbound SBC bitpool to 35.

Highest-priority issues:

1. The live device is ahead of canonical source/build state.
2. There is no single top-level reproducible build/deploy recipe.
3. The 14 GiB workspace mixes source, generated output, experiments and immutable evidence.
4. Production startup still carries bring-up/debug instrumentation.
5. BlueALSA still reports many PCM overruns despite nearly perfect audible output.
6. Several kernel bring-up hacks need scoping/removal before upstreaming.
7. Pairing policy is intentionally permissive and should be hardened later.

The project should now transition from exploration to consolidation, not continue accumulating numbered rootfs/image variants at the top level.

## Current hardware/runtime truth

Current intended path:

    TomTom bootloader
      -> Linux 6.1.182 / S3C2412 port
      -> small initramfs / custom PID1
      -> FAT /sdcard runtime
      -> original CSR BCSP bring-up
      -> BlueZ 5.86
      -> BlueALSA 4.3.1
      -> bluealsa-aplay
      -> ALSA plug/rate
      -> 42968-Hz hardware-facing PCM
      -> S3C2412 I2S -> WM8711 -> amplifier/speaker

Proven milestones include boot, framebuffer/touch/UI, hardware-proven SFTRST restart with INFORM0=0x9, audible local ALSA playback, CSR BCSP, BlueZ/BlueALSA connectivity, good Galaxy S6 audio and nearly-perfect iPhone audio after max_bitpool=35.

Current unresolved items:
- modern8 reboot quiesce needs final hardware re-validation;
- tiny residual frequency-specific click;
- successful iPhone session logged 749 BlueALSA PCM-overrun drops and zero underruns, so the residual click is not yet proven analog-only.

## Strong areas

### Checkpointing and hardware evidence

Checkpoint discipline is excellent. Failures were preserved rather than overwritten, including reboot failures, DMA Oopses, silent PCM, oversized-initramfs panics and loader/runtime failures. Preserve checkpoints as immutable engineering evidence.

### Modern Bluetooth architecture

Replacing the custom A2DP prototype with BlueZ + BlueALSA + ALSA was the correct production direction. The prototype served its diagnostic purpose.

### 32 MiB RAM packaging

The current small-initramfs + FAT-runtime split is the correct architecture for this device. Larger initramfs attempts produced real memory-deadlock panics and established the hardware ceiling.

### Audio hardware work

The original 2.6 TomTom source was used correctly as the hardware oracle for WM8711 registers, I2S topology, amp/DAC power and restart semantics.

### Compiler hygiene

Modified kernel objects pass W=1 except for two missing-prototype warnings for tomtom_audio_set_amp() and tomtom_audio_set_dac_power(). The D-Bus pairing agent passes a strict warning syntax pass cleanly.

## Critical reproducibility gap

The active device runtime is ahead of the canonical local runtime bundle.

Known drift:
- device bluealsa.exe is the bitpool35/logging build; local ttbt-modern5-sd is older;
- device ttbluez-agent.exe is retry-hardened; local bundle is older;
- device bluetoothd.exe is a small debug wrapper around bluetoothd.real.exe;
- device retains rollback copies of those hotfixes.

The successful bitpool35 source edit currently lives in generated Buildroot output at buildroot-bt/out/build/bluez-alsa-4.3.1/src/a2dp-sbc.c, while the clean checkout still has SBC_MAX_BITPOOL.

DO NOT clean buildroot-bt/out/build until that change is captured as a canonical source patch.

Recommended patch location:

    third_party/patches/bluez-alsa/0001-tomtom-cap-a2dp-sink-bitpool-35.patch

## Build/reproducibility

There is no top-level Makefile/build.sh/deploy.sh that regenerates the active system from pristine source.

Recommended entry points:

    ./tools/build.sh dev
    ./tools/build.sh prod
    ./tools/deploy.sh /dev/sdX

Build should verify toolchains, extract pristine Linux, apply ordered patches, build custom userspace and BlueZ/BlueALSA, materialize the VFAT runtime, build initramfs/zImage/ttsystem, and emit manifests/size checks.

Deploy should verify the target, preserve current active image, stage and hash-check files, switch ttsystem only after validation, sync, and unmount.

## Source control

- k6 is not Git even though it contains the real kernel port. Move the port into Git as ordered patches/commits based on the pristine tar.
- linux-6.1.182/ is not fully pristine: head.S, misc.c and mach-tomtomgo.c differ from the original tar. Treat linux-6.1.182.tar.xz as the real baseline.
- opentom has a legitimate one-line string.h fix plus untracked built binaries. Commit the source fix and ignore/build outputs outside the tree.
- third_party_src/bluez-alsa and third_party_src/buildroot are clean. Keep them clean and apply local patches externally.

## Kernel audit

Good standalone patch candidates:
- I2S/DMA clkdev aliases;
- s3c24xx_dma_tx_status teardown NULL guard;
- DMA slave capability/residue reporting;
- WM8711 stale line-bypass removal.

Cleanup/scoping needed:
1. mach-tomtomgo.c is overburdened (~940 lines) with board data, early framebuffer console, panic renderer, power-button polling, audio helpers, reboot, hwstatus ABI and bring-up logging.
2. Bring-up logs (TT step markers, initcall alive markers, GPFDAT transitions) should be debug-gated or removed from production.
3. Generic clk-s3c2412.c still contains the old TomTom INFORM0=0x1 experiment. Remove it; TomTom already has the proven priority-255 INFORM0=0x9 handler.
4. SND_SOC_SAMSUNG_TOMTOM_WM8711 incorrectly depends on I2C although this board uses GPIO SPI. Fix Kconfig dependency.
5. gpio-cfg-helpers.h globally treats missing set_pull as success; scope/fix instead of silently succeeding globally.
6. IRQ parent-ack/mask-all changes need per-change validation and S3C2412 scoping if appropriate.
7. Broadened S3C2412 ID matching needs documented silicon evidence.
8. Generic decompressor code unconditionally forces TomTom machine ID/ATAG/framebuffer behavior. Gate under CONFIG_MACH_TOMTOMGO or use a TomTom-specific hook.

Audio driver notes:
- add a shared prototype/header for amp/DAC helpers;
- eventually reduce direct codec-register clobbering from the DAPM speaker callback;
- document/test the 42968-Hz KNOT bridge as an explicit platform quirk.

## Bluetooth/A2DP audit

The bitpool35 result is a strong hardware finding. 460800-baud BCSP has little headroom for high-quality SBC near bitpool 53 after framing/protocol overhead. Limiting sink max bitpool to 35 materially restores transport headroom and should become a documented platform constraint.

Remaining issue: BlueALSA logged 749 PCM-overrun frame drops in a successful session. Investigate in this order:
1. remove full BlueZ/HCI debug logging from normal operation;
2. re-test and count overruns;
3. measure ALSA plug/rate CPU and pacing cost;
4. tune bluealsa-aplay buffer/period timing without adding seconds of latency;
5. only then classify the residual click as analog/speaker resonance.

## PID1 / init

c_init.c is appropriately small and understandable. Main issue: /sdcard is mounted in a forked child while launcher starts ~100 ms later. Modern runtime now depends on /sdcard, so add explicit storage readiness or mount synchronously.

Manual mknod after devtmpfs is largely redundant/hardcoded. Prefer devtmpfs and create only deliberate compatibility aliases/nodes.

## Launcher

ttlauncher.c is monolithic (~1575 lines) but coherent for a tiny framebuffer appliance. Static BSS buffers are a good choice.

Improvements:
- quiesce_bt_audio() waits indefinitely on its child; add an outer deadline even though modern8's script is bounded;
- long Bluetooth actions are blocking; show progress/state if desired;
- root diagnostic shell should be dev-profile-only in a final appliance.

## Shell/control scripts

All audited rootfs scripts pass /bin/sh -n.

Production debt:
- ttbt_modern_init.sh still contains meminfo/process diagnostics and full BlueZ debug;
- one path uses /bin/tail even though there is no tail symlink; use /bin/busybox tail consistently;
- bt_tool.sh still uses legacy hcitool scan.

Create quiet production startup and explicit debug startup modes.

Modern8 quiesce is substantially better: audio consumer first, then transport, BlueZ before controller down, D-Bus last, with bounded TERM and KILL fallback. After hardware validation this should become canonical.

## Pairing/security

Current settings are lab-friendly, not final-appliance policy: NoInputNoOutput, PIN 0000 fallback, auto-authorization, always pairable and infinitely discoverable.

Final behavior should open a bounded pairing window from UI, then become non-discoverable, persist trusted bonds, and authorize only required services/trusted devices. Remove stale hcid.conf/PIN helper/PIN file if no legacy requirement remains.

## Kernel config / optimization

bt-modern8.config is still a development kernel: DEBUG_KERNEL, DEBUG_MUTEXES, DEBUG_INFO, FTRACE, FB debug, verbose ALSA procfs, modules and broad USB/network/filesystem/input support are enabled.

Create separate dev and prod defconfigs. Do not randomly strip while debugging. Trim only after a hardware regression matrix passes.

Current approximate sizes: Image 8.6 MiB, zImage 4.6 MiB, initramfs cpio 2.7 MiB.

## Folder hygiene

Workspace is ~14 GiB.

Largest areas:
- buildroot-bt ~7.9 GiB (out/build ~7.4 GiB, out/host ~494 MiB)
- k6 ~2.1 GiB
- linux-6.1.182 ~2.1 GiB
- third_party_src ~469 MiB
- gpl ~362 MiB
- checkpoints ~291 MiB
- rootfs snapshots ~96 MiB
- 60 top-level ttsystem* images ~267 MiB

33 of 60 top-level ttsystem images are byte-identical to checkpoint copies (~157 MiB). 27 are not checkpoint-covered and must not be deleted blindly.

The problem is namespace/ownership more than raw capacity: canonical source, generated output, experiments and evidence share one top-level directory.

## Recommended layout

    ttwork/
      README.md
      PROJECT-AUDIT-20260916.md
      configs/
      kernel/upstream/
      kernel/patches/
      userspace/pid1/
      userspace/launcher/
      userspace/diagnostics/
      userspace/bluetooth/
      third_party/
      third_party/patches/bluez-alsa/
      rootfs/base/
      rootfs/overlay/
      tools/
      out/kernel/
      out/rootfs/
      out/runtime/
      out/images/
      out/manifests/
      experiments/
      checkpoints/
      oracle/

Rule: if deleting out/ would lose engineering knowledge, the project is not yet reproducible enough.

## Safe consolidation sequence

P0 — preserve current truth before cleanup
1. Hardware-test modern8 reboot during active streaming.
2. Copy actual active FAT runtime back into canonical staging.
3. Save exact runtime/image manifests.
4. Canonicalize bitpool35, retry-agent and required wrapper changes.
5. Verify a clean rebuild reproduces behavior.

P1 — source of truth / one-command build
1. Create project Git history.
2. Recreate pristine kernel from original tar.
3. Import kernel changes as ordered commits.
4. Create canonical rootfs overlay.
5. Add Buildroot defconfig + BlueALSA patch.
6. Add build/deploy/verify scripts.
7. Put all generated output under out/.

P2 — productionize runtime
1. Remove debug wrapper from normal boot.
2. Make verbose logging explicit debug mode.
3. Remove stale legacy Bluetooth files.
4. Replace hcitool scan with modern pairing workflow.
5. Add storage-ready handling.
6. Add launcher-side total quiesce deadline.
7. Regression-test idle/connected/playing/repeated-connect reboot paths.

P3 — finish audio pacing
1. Retest bitpool35 without debug logging.
2. Count PCM overruns.
3. Measure resampler/consumer pacing.
4. Tune buffer/period without adding large latency.
5. Then classify the residual click.

P4 — kernel cleanup/upstream prep
1. Remove generic INFORM0=0x1 experiment.
2. Fix audio Kconfig dependency.
3. Add helper prototypes.
4. Gate/remove bring-up logging.
5. Scope GPIO/IRQ changes.
6. Gate TomTom decompressor behavior.
7. Split diagnostics/compat helpers where useful.
8. Maintain dev/prod defconfigs and run W=1/static checks.

P5 — folder cleanup
Only after P0/P1:
- archive/remove top-level images already checkpointed;
- move uncheckpointed legacy artifacts under artifacts/legacy;
- delete/regenerate Buildroot output;
- use out-of-tree kernel builds;
- retire redundant rootfs snapshots after manifests are verified;
- keep checkpoints immutable.

## Overall assessment

Hardware/kernel bring-up: strong.
Checkpointing/forensics: excellent.
Runtime architecture: strong and recently stabilized.
Audio quality: close, but PCM overruns remain.
Shell/control logic: functional with bring-up debt.
Kernel upstreamability: medium until diagnostics/generic hacks are scoped.
Reproducibility: weak today but straightforward to fix.
Folder hygiene: weak; it is an engineering workbench, not a clean source tree.

## Bottom line

Do not rewrite the project.

The right next move is consolidation: freeze successful hardware facts, make the live device reproducible from source, separate production from diagnostics, eliminate PCM overruns, turn kernel work into a reviewable patch series, and only then clean generated/history clutter around that canonical core.
