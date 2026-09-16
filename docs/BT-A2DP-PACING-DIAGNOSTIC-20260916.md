# TomTom ONE XL A2DP pacing diagnostic — 2026-09-16

## Preserved hardware truths

- `modern10` remains the reboot-good baseline. Active `/ttsystem` was reverified as
  `7efb5549432299f9e08d0b66c852bddd87b0833944aae071bf84ae44061be42f` before and after telemetry deployment.
- `modern11` remains the lower-audio-path oracle: local 44.1 kHz PCM through `ttbt` to the ~42.968 kHz hardware path is steady with 0 XRUNs and ~100 ms final drain.
- The 3–4 second A2DP delay/cadence therefore remains above the proven ALSA/DMA/I2S/codec path.

## Findings

1. BlueALSA PCM data uses `pipe2()`; its `SOCK_SEQPACKET` pair is the control channel. The bounded 32768-byte PCM-pipe patch therefore targets the correct data FIFO.
2. `bluealsa-aplay` drains one period from the BlueALSA PCM pipe and then performs a potentially blocking `snd_pcm_writei()` in the same worker. The local ALSA oracle measured healthy writes as long as ~41.7 ms.
3. The historical 749-overrun log is not a clean controlled sample: it begins with an `org.bluez.Error.AlreadyExists` registration failure, consistent with a stale/duplicate partial stack.
4. `modern12` scheduler telemetry was invalid as an audio test: `bluealsa-aplay` raced BlueALSA D-Bus registration and exited before streaming.
5. `CONFIG_CPU_FREQ` and `CONFIG_CPU_IDLE` are disabled in the current kernel, so awake charger-vs-battery behavior is not explained by a Linux cpufreq/cpuidle governor transition. The power clue remains relevant to radio/power integrity or scheduling/load.
6. The A2DP transport receive buffer had not been bounded. Existing telemetry records `SO_RCVBUF`, `SO_SNDBUF`, and MTUs; RX timing will distinguish upstream compressed-audio backlog from consumer-side stalls before any receive-buffer policy is changed.

## modern13 measurement-only telemetry

No multi-second buffer is added and no playback policy is changed.

- `0005-tomtom-aplay-consumer-timing.patch`
  - measures each successful ALSA write duration;
  - samples unread BlueALSA PCM-pipe bytes with `FIONREAD`;
  - emits one `TomTom APLAYSTAT` aggregate per 128 writes.
- `0006-tomtom-aggregate-pcm-overrun-log.patch`
  - preserves BlueALSA's drop-on-full semantics;
  - reports first and every 32nd overrun instead of writing one warning per dropped block, reducing FAT-log perturbation.

Built candidate hashes:

- `bluealsa.exe`: `04f6eb5205582cada9b69aff5b97a59732f45c7d3de9df139c0eaeece8e3d00b`
- `bluealsa-aplay.exe`: `64ac9f116b4d635c748a6a6860bcfa69ef43f37f7d1fdc6befeda7d32d06f015`

Local hardware checkpoint:
`/home/macbuntu/Documents/ttwork/checkpoints/20260916-modern13-rx-aplay-telemetry`

## Controlled run interpretation

Use one fresh boot and one START/INIT only. Play ~30 seconds on battery, then connect charger without pausing and continue ~30 seconds, then pause once and reboot.

Read together:

- `TomTom A2DP socket buffers`: actual transport receive/send queue sizing.
- `TomTom A2DP RXSTAT`: RTP audio time vs wall time, missing frames, decode cost, and maximum receive/scheduling gap.
- `TomTom APLAYSTAT`: ALSA-write cost and maximum unread PCM FIFO backlog.
- `TomTom PCM overrun total`: downstream producer/consumer pressure without warning-log flood.

Do not shrink A2DP receive buffering or add further PCM buffering until this controlled run identifies where the seconds-long backlog resides.
