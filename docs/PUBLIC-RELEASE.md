# Public V3 release

The public release is derived from the hardware-accepted Linux 6.1.182 #176 + V3 shell state.

## Security difference from the private preservation image

The private preservation image contained the exact Dropbear host keys and the lab login public key used during hardware development. Those are intentionally **not** redistributed.

The public image instead:

- contains no Dropbear host private key;
- contains no baked-in `authorized_keys`;
- starts Dropbear with `-R -s`;
- generates missing host keys on first use;
- persists generated host keys to `/sdcard/.dropbear/`;
- loads the user's public key from `/sdcard/authorized_keys`;
- disables password authentication.

The launcher binary is byte-identical to the accepted V3 launcher.

## Public artifact checksums

```text
2061081239680f17eb476cf8e299e5e5bbe5806e9c5ea55cdab7998f41ada4db  zImage
0033810e10b5a79955b6398c60028413706ed0068893a9b2cfcbc2430ce1cb21  ttsystem
50ceebbf92dfd8c9444a8f0aae6dce39ea2ead543c451ae5ade75af2b59f7cac  ttlauncher.v3.final
```

TTBL validation confirms that section 0 is the preserved 138-byte TomTom boot payload and section 1 is byte-for-byte the public-safe zImage.
