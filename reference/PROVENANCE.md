# Provenance and hardware authority

This clean source tree was created from the engineering workbench at:

    /home/macbuntu/Documents/ttwork

The legacy workbench remains available as evidence and as a rescue path.

Hardware-authoritative P0 checkpoint:

    ../checkpoints/20260916-consolidation-p0-live-authority

At capture time the active device image and local modern8 image were byte-identical:

    e9236602117ce69221be77da82abddaba595dcecafbd6efed4811eb5dcbcf3f5

The FAT runtime was captured byte-for-byte before cleanup. The live runtime differs
from the older ttbt-modern5-sd staging bundle in three intentional hotfixes:

1. BlueALSA 4.3.1 SBC sink max bitpool = 35 plus negotiation logging.
2. Retry-hardened ttbluez-agent.
3. Temporary bluetoothd debug wrapper around bluetoothd.real.exe.

Canonical source now exists for (1) and (2). The wrapper is debug instrumentation,
not part of the intended production runtime.

The retry agent is reproducible byte-for-byte with the Buildroot 15.3 toolchain
using:

    -Os -s

Live/rebuilt agent SHA256:

    57d8fc79e301fc8ebc1df0ff5e1efa040907c2cc12eebd58fc2b9c5e33155ebf

The BlueALSA patch is based on the exact Buildroot 4.3.1 archive
(SHA256 933fe898dfac21fdfeb5f4ffa685c2aa2db9c064d639170ac2652f156e956a2a).
Rebuilding and stripping produces the exact live binary SHA256:

    417b1cea148c2cf703c6e195c10e5635938bd33a4509d98431cda0514c3d1c9c

Kernel source authority:

- pristine baseline: ../linux-6.1.182.tar.xz
- active engineering tree: ../k6
- exact current 30-file source delta:
  kernel/patches/0000-current-k6-preservation.patch

The preservation patch was applied to pristine 6.1.182 and every affected file
was byte-compared back to k6 successfully.
