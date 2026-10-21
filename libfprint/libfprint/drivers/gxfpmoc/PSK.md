# PSK Key Management

The GXFP5130 sensor uses TLS-PSK (Pre-Shared Key) for session encryption. The key is 32 bytes. The sensor's MCU stores the PSK in a provisioning blob (`BB010002`). Both the sensor and the host must hold the same PSK to complete the TLS handshake.

Typical error with mismatched PSK:

```
SSL - Verification of the message MAC failed
```

## Obtaining the PSK

### Method 1: Dual-Boot with Windows

When the Windows driver detects a PSK mismatch, it generates and uploads a new PSK, then caches it using Windows DPAPI in `Goodix_Cache.bin`.

Extract it with `tools/unseal_bb010002.py`:

```bash
pip install -r tools/requirements.txt
python tools/unseal_bb010002.py
```

This outputs the raw PSK file `psk_raw32.bin`.

If auto-discovery fails, specify the cache path manually:

```bash
python tools/unseal_bb010002.py --cache /path/to/Goodix_Cache.bin
```

### Method 2: Linux-Only

Generate a new provisioning blob and upload it to the sensor:

```bash
# 1. Build the provisioning blob
gxfp_psk_tool --build-bb010002 blob.bin --out-psk-raw32 psk_raw32.bin

# 2. Upload the blob to the sensor
gxfp_psk_tool --upload-bb010002 blob.bin
```

## Installing the PSK

The PSK must be readable by libfprint. The default path is:

```
/var/lib/fprint/gxfp/psk_raw32.bin
```

Selection order is a nonempty `FP_GXFP_PSK`, then the new default, then
`/var/lib/fprintd/gxfp/psk_raw32.bin` only when the new default is absent.
An explicit path never falls back. Access errors, malformed keys and broken
symlinks at the new path must be fixed instead of selecting a different key.
For fprintd, set overrides in its service environment; exporting a variable in
a terminal does not change a running system service.

New provisioning uses the new default. Existing keys at either location are
protected without `--replace`. Explicit replacement on a legacy-only install
stays at that path. If both paths exist, provisioning refuses to choose which
key to overwrite. `gxfp_capture` still requires an explicit `--psk-raw32` path;
`gxfp_recovery` does not select a PSK file.

To migrate, back up the existing key securely, stop fprintd, and copy the same
32 bytes into the new directory (root ownership, directory 0700, file 0600).
Restore SELinux labels and test verification before retiring the old copy.
Do not run provisioning just to move a file: it changes the sensor key.
Rollback is possible using the old path or an explicit service override as
long as the sensor key has not been replaced. Never post PSK contents.

Install the PSK to the default location with proper permissions.