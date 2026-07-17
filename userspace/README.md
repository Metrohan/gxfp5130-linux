# GXFP5130 Linux Fingerprint Sensor Driver

An experimental Linux driver stack for the GXFP5130 fingerprint sensor connected via eSPI.

## Overview

The project consists of three components:

| Repository | Description |
|---|---|
| [gxfp_linux_driver](https://github.com/Void755/gxfp_linux_driver) | Kernel driver implementing the eSPI transport layer, exposing `/dev/gxfp` |
| gxfpmoc (this repo) | Userspace driver library managing TLS sessions, sensor configuration, and image capture. Includes diagnostic and provisioning tools |
| [libfprint](https://github.com/Void755/libfprint) | Fork of [libfprint](https://github.com/goodix-fp-linux-dev/libfprint) with SIGFM matching algorithm |

## Installation (NixOS)

### Flake Configuration

Add to your `flake.nix`:

```nix
{
  inputs = {
    fprintd.url = "git+https://github.com/Void755/libfprint?ref=sigfm&submodules=1";
    gxfp-linux-driver.url = "github:Void755/gxfp_linux_driver";
  };

  outputs = { self, nixpkgs, fprintd, gxfp-linux-driver, ... }: {
    nixosConfigurations.your-host = nixpkgs.lib.nixosSystem {
      modules = [
        fprintd.nixosModules.default
        gxfp-linux-driver.nixosModules.default
        {
          hardware.gxfp.enable = true;
        }
        ({ pkgs, ... }: {
          nixpkgs.overlays = [
            fprintd.overlays.default
          ];
          environment.systemPackages = with pkgs; [ gxfp-tools ];
        })
      ];
    };
  };
}
```

The `hardware.gxfp` module:

- Loads the `gxfp` kernel module and adds a udev rule
- Enables `fprintd` with the SIGFM-enabled `libfprint`

## PSK Key Management

The sensor uses TLS-PSK for session encryption. See [PSK.md](PSK.md) for details on obtaining and configuring the PSK.

## Device Compatibility

The sensor is identified via ACPI as `GXFP5130` and connects over eSPI (not the usual SPI or USB).

| Device | Sensor ID | Sensor Size | MCU Firmware | Status |
|---|---|---|---|---|
| MateBook GT 14 | ACPI/GXFP5130 | 176×54 | GF_GCC_EC_20061 | 🟢 |
| MateBook 14 2024 | ACPI/GXFP5130 | 64×80 | GF_GCC_EC_20068 | 🟡 ¹ |
| MateBook X Pro 2024 | ACPI/GXFP5130 | - | GF_GCC_EC_20068 | 🟡 ² |
| MateBook X Pro 2024 | ACPI/SIL6250 | - | - | 🔴 ² |

¹ Image capture works. SIGFM matching unverified

² See [gxfp_linux_driver#1](https://github.com/Void755/gxfp_linux_driver/issues/1)

## Building

- CMake >= 3.16
- C11 compiler
- [Mbed TLS](https://github.com/Mbed-TLS/mbedtls)

Nix development shell:

```bash
nix develop -f shell.nix
```

## Tools

### gxfp_psk_tool

Manages PSK provisioning data on the sensor MCU. See [PSK.md](PSK.md) for usage.

### gxfp_capture

Diagnostic tool that captures a raw fingerprint image and saves it as a 16-bit PGM file.

```bash
gxfp_capture --psk-raw32 /path/to/psk_raw32.bin
```

## Disclaimer

This project is experimental and not affiliated with or endorsed by Goodix or Huawei.
