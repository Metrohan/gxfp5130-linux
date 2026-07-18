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
/var/lib/fprintd/gxfp/psk_raw32.bin
```

The path can be overridden with the `FP_GXFP_PSK` environment variable.

Install the PSK to the default location with proper permissions.