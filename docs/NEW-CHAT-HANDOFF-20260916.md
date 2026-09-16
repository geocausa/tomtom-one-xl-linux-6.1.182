# TomTom ONE XL Linux 6.1 — NEW CHAT HANDOFF — 2026-09-16

## Scope
Use **macbuntu only**.

Workbench:
`/home/macbuntu/Documents/ttwork`

Canonical clean tree:
`/home/macbuntu/Documents/ttwork/tomtom-one-xl-linux6`

## Proven platform state
Linux 6.1.182 boots on TomTom ONE XL with framebuffer, touch, MMC/FAT, launcher, ALSA, S3C2412 I2S, WM8711, amp/DAC GPIOs, CSR Bluetooth over BCSP, BlueZ 5.86 and BlueALSA 4.3.1.

Reboot hardware truth:
- INFORM0 = 0x9
- SFTRST
- `s3c2412_cpu_reboot()`
- never use `soft_restart(0)`; it froze.

Modern10 proved reboot works after active Bluetooth playback. Quiesce order is now:
1. bluealsa-aplay
2. bluealsa
3. HCI logger
4. pairing agent
5. bluetoothd
6. persist BlueZ state
7. hci0 down
8. hciattach
9. dbus-daemon
10. sync
11. SFTRST

Important shell constraint:
**BusyBox ash has arithmetic expansion disabled. Never use `$((...))`.**

## Local audio oracle
Modern11 SPEAKER MODE was temporarily repurposed for a Bluetooth-free local ALSA test:

44.1 kHz local PCM
-> ALSA `ttbt` plug/rate
-> 42.968 kHz hardware-facing PCM
-> S3C2412 DMA/I2S
-> WM8711
-> speaker

Hardware result:
- tone steady
- no cadence
- no glitching
- startup felt delayed
- 30.000 s nominal audio fed in ~29.957 s
- 0 XRUNs
- 0 short writes
- slowest ALSA write ~41.7 ms
- final `snd_pcm_drain()` ~99.6 ms

This is decisive:
**multi-second Bluetooth delay/cadence is NOT caused by ALSA resampling, DMA, I2S, WM8711, or speaker hardware.**

## Audio clock bridge
TomTom PCLK gives actual 44.1-family hardware rate around 42968.75 Hz.

Current design:
- app/Bluetooth PCM = 44100 Hz
- ALSA plug converts to 42968 Hz
- CPU DAI uses real 42968
- WM8711 maps 42968 to nominal 44100 coefficient recipe

`/etc/asound.conf` uses `pcm.ttbt` with `rate 42968`.

## Bluetooth transport / bitpool
CSR BCSP UART:
- `/dev/ttySAC2`
- 460800 baud
- 26 MHz reference

A2DP sink max SBC bitpool was capped to 35.
Known iPhone negotiation:
- 44.1 kHz
- joint stereo
- min bitpool 2
- max bitpool 35

Bitpool35 helped transport headroom but is **not the complete root fix**.

Samsung phones show in legacy TomTom-side scan and connect.
Two iPhones did not show in TomTom-side legacy scan; do not treat that alone as a fault because iPhone generally initiates pairing to discoverable accessories rather than behaving like a legacy inquiry target.

## Current audio blocker
Both Galaxy S6 and iPhone have shown:
- cadence/jitter/stutter
- several-second audio latency / A-V offset (~3–4 s)
- multi-second tail after pause

User also observed:
- playback is somewhat better while charger connected
- worse on battery

Power/CPU/radio margin may matter, but local ALSA proves the lower playback path can sustain real-time audio cleanly.

Focus now on:
- BlueALSA decode
- BlueALSA PCM handoff
- bluealsa-aplay scheduling
- Bluetooth receive/scheduling
- charger-vs-battery power state

Do NOT solve by adding multi-second buffers.

## BlueALSA producer/consumer finding
BlueALSA hands decoded PCM to bluealsa-aplay through a non-blocking Unix `SOCK_SEQPACKET` socketpair.

BlueALSA behavior:
- decoder produces PCM continuously
- socket write is non-blocking
- when PCM client queue is full, BlueALSA logs:
  `Dropping PCM frames: PCM overrun`

Earlier session:
- 749 PCM overruns
- 0 underruns

bluealsa-aplay reads from that socket and performs potentially blocking ALSA writes in the same worker path.
Local ALSA oracle proved a healthy ALSA write can block ~42 ms.
During that block, BlueALSA cannot be drained, so the handoff queue can fill.

This is currently the strongest software hypothesis.

## Pipe32k experiment
A bounded PCM handoff patch was formalized:

`tomtom-one-xl-linux6/third_party/patches/bluez-alsa/0002-tomtom-bounded-pcm-pipe-headroom.patch`

Target:
- 32768 bytes
- ~186 ms stereo S16/44.1
- enough to absorb short scheduling stalls
- deliberately not seconds of buffering

Current candidate BlueALSA SHA256:
`31ea0d1fd2de64510e476503d7c86484c5a56f1c0b82ca892dd5c3141ddc55f5`

Previous hardware-tested bitpool35 BlueALSA rollback:
`417b1cea148c2cf703c6e195c10e5635938bd33a4509d98431cda0514c3d1c9c`

Rollback file on FAT:
`/ttbt-modern/bin/bluealsa.pre-pipe32k.exe`

At handoff, pipe32k BlueALSA is believed active on FAT. Latest user report still has multi-second delay, so pipe headroom alone did not solve it.

New chat should pull current logs and verify:
- actual pipe capacity before/after
- resize success/failure
- PCM overrun count
- whether delay grows progressively

## Power clue
User reports playback is somewhat better on charger than battery.

Check:
1. cpufreq presence/state
2. CPU clock on battery vs charger
3. cpuidle / PM config
4. HCI/BCSP retry/error evidence
5. battery/power GPIO/regulator behavior from original TomTom 2.6 source

Do not blame speaker; local ALSA path is clean.

## Modern lineage
### modern9
Purpose: quieter runtime, no BlueZ -d flood, no HCI logger.
Failed START/INIT because shell used `$((...))`.
Preserved:
`ttsystem.bt-modern9-HW-BUSYBOX-ARITH-FAIL`

### modern10
Fixed all arithmetic expansion in startup/quiesce.
SHA256:
`7efb5549432299f9e08d0b66c852bddd87b0833944aae071bf84ae44061be42f`

Hardware:
- Galaxy S6 still cadence/buffering
- reboot works
- quiesce log proves all modern processes stop cleanly

Checkpoint:
`checkpoints/20260916-bt-modern10-HW-S6-CADENCE-REBOOT-GOOD`

### modern11
Local ALSA discriminator image.
SHA256:
`b06e039f75e54fc13c9c1010045514dccd67e58de5a6659f6fdc1bb4ccdcda18`

Local ALSA v2 hardware result is clean/steady.
Checkpoint:
`checkpoints/20260916-modern11-local-alsa-v2`

## Canonical project / Git
Path:
`/home/macbuntu/Documents/ttwork/tomtom-one-xl-linux6`

Important files:
- `README.md`
- `docs/PROJECT-AUDIT-20260916.md`
- `reference/PROVENANCE.md`
- `userspace/bluetooth/`
- `userspace/diagnostics/ttalsa_stream.c`
- `third_party/patches/bluez-alsa/`
- `kernel/patches/0000-current-k6-preservation.patch`
- `tools/build-agent.sh`
- `tools/rebuild-bluealsa.sh`
- `tools/build-runtime.sh`

Important commits:
- `b950e2f` Consolidate hardware-proven TomTom Linux 6.1 state
- `faa02e2` Make Bluetooth scripts compatible with minimal BusyBox ash
- `3cf11f9` Add local ALSA stream discriminator
- `8581b4c` Match local ALSA diagnostic to BlueALSA setup
- inspect current git log for later BlueALSA pipe-patch cleanup commit

## P0 preservation already done
Live device checkpoint:
`checkpoints/20260916-consolidation-p0-live-authority`

At capture, active device `/ttsystem` exactly matched local modern8:
`e9236602117ce69221be77da82abddaba595dcecafbd6efed4811eb5dcbcf3f5`

Retry pairing-agent rebuild exact SHA256:
`57d8fc79e301fc8ebc1df0ff5e1efa040907c2cc12eebd58fc2b9c5e33155ebf`

Bitpool35 BlueALSA rebuild exact SHA256:
`417b1cea148c2cf703c6e195c10e5635938bd33a4509d98431cda0514c3d1c9c`

Generated runtime builder reproduces the core working FAT runtime binaries/libraries exactly.

## Kernel source preservation
Pristine baseline:
`/home/macbuntu/Documents/ttwork/linux-6.1.182.tar.xz`

Active engineering tree:
`/home/macbuntu/Documents/ttwork/k6`

Exact current source delta:
`tomtom-one-xl-linux6/kernel/patches/0000-current-k6-preservation.patch`

Patch covers 30 source/build-description files and was verified by applying to pristine 6.1.182 and byte-comparing every affected file back to k6.

## Kernel cleanup still pending
Do after current Bluetooth pacing bug unless directly relevant:
1. remove stale generic S3C2412 INFORM0=0x1 experiment
2. fix TomTom WM8711 Kconfig dependency (GPIO SPI, not stale I2C)
3. add prototypes/header for amp/DAC helpers
4. gate/remove bring-up logs
5. review broad GPIO pull fallback
6. review IRQ parent-ack / mask-all changes
7. gate TomTom decompressor machine-ID/ATAG/framebuffer changes
8. create dev/prod defconfigs
9. move to out-of-tree kernel builds

## Immediate new-chat steps
1. Use macbuntu only.
2. Read this handoff.
3. Check git log/status in canonical tree.
4. Check current device hashes if USB present.
5. Pull latest BlueALSA/BlueZ/aplay logs from pipe32k run.
6. Verify pipe capacity + overrun count.
7. Compare charger vs battery CPU/power state.
8. Determine whether bluealsa-aplay consumes slower than real time.
9. Prefer dropping stale PCM to stay synchronized to “now” over accumulating delay.
10. Keep local ALSA discriminator as lower-path oracle.

## User observations not to lose
- Galaxy S6 was good in an early prototype, but later S6 and iPhone both show cadence/buffering.
- Two Samsungs appear in TomTom-side legacy scan and connect.
- Two iPhones do not appear in TomTom-side legacy scan.
- Charger makes playback a bit better than battery.
- Current Bluetooth A/V delay can be 3–4 seconds.
- Local ALSA 30-second tone is steady, glitch-free, 0 XRUN.
- Reboot works after modern10 quiesce fix.
- User relies on assistant to drive engineering autonomously.

## Starter prompt for new chat
Continue the TomTom ONE XL Linux 6.1 port from:
`/home/macbuntu/Documents/ttwork/NEW-CHAT-HANDOFF-20260916.md`
and:
`/home/macbuntu/Documents/ttwork/tomtom-one-xl-linux6`

Use macbuntu only. First verify Git/device state and latest logs. Preserve modern10 reboot-good behavior and modern11 local-ALSA-good evidence. Current blocker is multi-second Bluetooth A2DP delay/cadence above ALSA. The local 44.1k -> ttbt -> 42.968k ALSA path is proven steady with 0 XRUNs and ~100 ms drain, so focus on BlueALSA/bluealsa-aplay/BT scheduling and the charger-vs-battery power clue. Do not hide the issue with multi-second buffering. Preserve all rollback images/checkpoints.
