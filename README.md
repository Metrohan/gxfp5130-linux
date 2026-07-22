readm

# Goodix GXFP5130 Linux Fingerprint Driver

English | **[Türkçe](README.tr.md)**

[![License: GPL-2.0](https://img.shields.io/badge/License-GPL--2.0-blue.svg)](LICENSE)
[![Kernel Patch: Under Review](<https://img.shields.io/badge/Kernel%20Patch-Under%20Review-yellow.svg>)](https://lore.kernel.org/linux-kernel/20260718080917.21893-1-metehangnen@gmail.com/)

Full Linux support for the **Goodix GXFP5130** fingerprint sensor found in
Huawei MateBook laptops — kernel module, userspace tools, libfprint integration,
and PAM setup, all in one place.

> Works on Huawei MateBook D16 2024 (MCLF-XX). If it works on your machine,
> please [open a compatibility report](https://github.com/Metrohan/gxfp5130-linux/issues/new?template=compatibility_report.yml).

> **Credits:** The original kernel driver, `gxfpmoc` userspace library, and
> the libfprint SIGFM fork were created by [**Void755**](https://github.com/Void755).
> This repository packages that work for distribution (Arch package, PAM
> integration, mainline kernel submission) — see [Upstream provenance](#upstream-provenance)
> for exact source snapshots.

---

## Upstream status

The kernel driver has been submitted to the Linux kernel mailing list
for mainline inclusion:

**[[PATCH 0/4] drivers/misc: add Goodix GXFP5130 eSPI fingerprint sensor driver](https://lore.kernel.org/linux-kernel/20260718080917.21893-1-metehangnen@gmail.com/)**

Once accepted, the module will ship with the mainline kernel and no DKMS
installation will be needed on supported distributions.

---

## What this does

The GXFP5130 is not a USB or PCIe device — it lives on the Embedded Controller's
internal SPI bus and is unreachable through any standard Linux driver path. Out of
the box, `fprintd-enroll` prints `No devices available` and nothing works.

This package fixes that end-to-end:

- **Kernel module** — eSPI mailbox transport + GPIO handshake; creates `/dev/gxfp`
- **Userspace tools** — TLS-PSK provisioning, raw capture, diagnostics
- **libfprint fork** — GXFP driver + SIGFM matching algorithm
- **PAM integration** — `sudo` and the login screen accept a fingerprint

## Supported hardware

| Laptop                                     | ACPI ID         | Firmware            | Status                       |
| ------------------------------------------ | --------------- | ------------------- | ---------------------------- |
| Huawei MateBook D16 2024 (MCLF-XX / M1010) | `GXFP5130:00` | `GF_GCC_EC_20067` | ✅ Verified                  |
| Other MateBook models with`GXFP5130:00`  | `GXFP5130:00` | unknown             | ❓ Untested — please report |

Check whether your sensor is present: `find /sys/bus/acpi/devices -name 'GXFP5130*'`

## Prerequisites

**Arch Linux:**

```sh
sudo pacman -S --needed base-devel linux-headers dkms cmake meson ninja \
  mbedtls glib2 libgusb gusb pixman nss libgudev cairo opencv doctest fprintd
```

Use `linux-lts-headers` if you are running the LTS kernel. Other distributions
need the equivalent packages: C/C++ compiler, CMake ≥ 3.16, Meson, Ninja,
Mbed TLS, GLib 2, GUsb, pixman, NSS, Cairo, OpenCV ≥ 4.5, fprintd.

## Quick start

```sh
# 1. Build (no root needed)
./scripts/doctor.sh
./scripts/build.sh

# 2. Install transport + DKMS module
sudo ./scripts/install.sh
sudo modprobe gxfp
./scripts/verify.sh

# 3. Provision the TLS PSK and confirm capture
sudo ./scripts/provision-psk.sh
sudo gxfp_capture --psk-raw32 /var/lib/fprintd/gxfp/psk_raw32.bin
```

If you dual-boot Windows and the sensor was already provisioned there, extract
the existing key instead of replacing it: see [userspace/PSK.md](userspace/PSK.md).

## Install libfprint integration

Installing this fork replaces the stock `libfprint` package. On Arch, the build
script creates a pacman-managed package so removal is tracked:

```sh
./scripts/build-arch-package.sh
sudo pacman -U config/arch/libfprint-gxfp-*.pkg.tar.zst
sudo systemctl restart fprintd
fprintd-enroll
fprintd-verify
```

To return to the distribution package later: `sudo pacman -S libfprint`.

For other distributions (local install only):

```sh
sudo meson install -C build/libfprint
sudo ldconfig
sudo systemctl restart fprintd
fprintd-enroll
fprintd-verify
```

Debug a session with verbose logging:

```sh
sudo systemctl stop fprintd
sudo env FP_GXFP_LOG=1 /usr/lib/fprintd
```

## PAM integration (sudo + login screen)

Add `pam_fprintd.so` as a `sufficient` rule above the password line in
`/etc/pam.d/system-auth`:

```
auth    required    pam_faillock.so   preauth
auth    sufficient  pam_fprintd.so timeout=10   ← add this line
auth    [success=2 default=ignore]  pam_systemd_home.so
auth    [success=1 default=bad]     pam_unix.so  try_first_pass nullok
```

`timeout=10` is important — without it, typing a password causes a ~120 s
wait while the fingerprint module times out before falling through to
password auth.

The same change in `/etc/pam.d/system-login` covers the display manager.

## Architecture

```
Browser / sudo / login screen
        │
        ▼
   pam_fprintd.so
        │
        ▼
    fprintd  ←─── libfprint (GXFP driver + SIGFM matcher)
        │
        ▼
   /dev/gxfp  (character device, DKMS kernel module)
        │
        ▼
  eSPI mailbox @ 0xFE800000
  GPIO 816 pulse → EC → GXFP5130 chip (EC-internal SPI)
  GPIO 813 pulse ← EC  (response ready)
```

The CPU cannot talk to the chip directly. Every command goes through the
Embedded Controller via the eSPI mailbox. The kernel module handles the
transport; userspace sees a simple read/write character device.

## Troubleshooting

| Symptom                                        | Likely cause                            | Fix                                                           |
| ---------------------------------------------- | --------------------------------------- | ------------------------------------------------------------- |
| No`GXFP5130:00` in `/sys/bus/acpi/devices` | Fingerprint reader disabled in firmware | Enable in BIOS/UEFI                                           |
| No`/dev/gxfp` after `modprobe gxfp`        | Kernel/module version mismatch          | Check`modinfo gxfp` vermagic vs `uname -r`                |
| `Operation not permitted` from fprintd       | Missing`DeviceAllow` drop-in          | Re-run`sudo ./scripts/install.sh`, restart fprintd          |
| TLS MAC verification failure                   | Host PSK ≠ sensor PSK                  | Re-provision with`provision-psk.sh` or extract from Windows |
| Capture works, enrollment fails                | libfprint/SIGFM layer issue             | Collect`FP_GXFP_LOG=1` output and open an issue             |
| Finger-up occasionally missed                  | Known firmware`GF_GCC_EC_20067` quirk | Driver retries automatically; no action needed                |
| Module missing after kernel upgrade            | DKMS rebuild needed                     | `sudo dkms autoinstall -k "$(uname -r)"`                    |

## Removal

```sh
sudo ./scripts/uninstall.sh
```

Enrolled fingerprints and the PSK are not deleted.

## Contributing

See [CONTRIBUTING.md](.github/CONTRIBUTING.md).

If the driver works on a MateBook model not listed above, please open a
[compatibility report](https://github.com/Metrohan/gxfp5130-linux/issues/new?template=compatibility_report.yml)
— it takes two minutes and helps everyone with the same hardware.

## Upstream provenance

- Kernel transport: [`Void755/gxfp_linux_driver`](https://github.com/Void755/gxfp_linux_driver), snapshot `594c372`
- Userspace library/tools: [`Void755/gxfpmoc`](https://github.com/Void755/gxfpmoc), snapshot `4b489a7`
- libfprint fork: [`Void755/libfprint`](https://github.com/Void755/libfprint), snapshot `1f7941e` + OpenCV 5 fallback

The stack is experimental and is not affiliated with Goodix or Huawei.

## License

Kernel module: [GPL-2.0-only](LICENSE)
libfprint fork: [LGPL-2.1+](libfprint/COPYING)
Userspace tools: GPL-2.0-only
