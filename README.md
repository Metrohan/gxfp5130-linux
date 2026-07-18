# GXFP5130 Linux stack

This directory is a self-contained Linux stack for the Goodix GXFP5130 ACPI
fingerprint sensor used in recent Huawei MateBook systems.

> Status on this machine (Huawei MCLF-XX): all components build on Linux 7.1.3.
> The signed DKMS module loads, `/dev/gxfp` is created, and firmware
> `GF_GCC_EC_20067` responds. Image capture and finger-up recovery were verified
> for 14 consecutive captures; two missed adaptive thresholds were recovered by
> the last-known-good WAIT_UP retry. A 16-stage right-index enrollment completed
> through fprintd and `fprintd-verify` returned `verify-match` on this machine.

### Verified host

| Item | Verified value |
|---|---|
| Laptop | Huawei MateBook D16 2024, MCLF-XX / M1010 |
| Sensor | ACPI `GXFP5130:00` |
| Firmware | `GF_GCC_EC_20067` |
| Kernel | Arch Linux `7.1.3-arch1-3` |
| Raw capture | 80x64, 14 consecutive captures |
| Finger-up fallback | Triggered twice and recovered both times |
| Desktop integration | `fprintd-enroll` completed; `fprintd-verify` matched |

## Architecture

The sensor is not USB and is not directly attached to a normal host SPI bus.
The stack is split into:

1. `kernel/`: eSPI mailbox transport; creates `/dev/gxfp`.
2. `userspace/`: Goodix protocol, TLS-PSK session, recovery/provisioning and
   capture tools.
3. `libfprint/`: libfprint fork containing the GXFP driver and SIGFM matcher.

The old direct-SPI/MMIO experiments are intentionally not part of this package.

## Prerequisites

Arch Linux:

```sh
sudo pacman -S --needed base-devel linux-headers dkms cmake meson ninja \
  mbedtls glib2 libgusb gusb pixman nss libgudev cairo opencv doctest fprintd
```

Use the matching headers when running another kernel (for example
`linux-lts-headers`). Other distributions need the equivalent development
packages plus a C/C++ compiler, CMake >= 3.16, Meson, Ninja and Mbed TLS.

## Build

No root privileges are needed:

```sh
./scripts/doctor.sh
./scripts/build.sh
```

This builds the kernel module, the three diagnostic tools, and the GXFP-enabled
libfprint in `build/`.

To produce a clean source archive without build artifacts:

```sh
./scripts/make-release.sh
```

## Install the transport and tools

```sh
sudo ./scripts/install.sh
sudo modprobe gxfp
./scripts/verify.sh
```

The installer uses DKMS so the module is rebuilt after kernel upgrades. It also
installs the diagnostic tools and a udev rule granting the logged-in desktop
user access to `/dev/gxfp`. A systemd drop-in grants the sandboxed fprintd
service access to the character device. Log out and back in if the udev ACL is
not applied.

## Provision the PSK and test capture

The sensor and host must share the same 32-byte TLS PSK. If Windows already
provisioned the device, follow [userspace/PSK.md](userspace/PSK.md) to extract
the key. For a Linux-only installation:

```sh
sudo ./scripts/provision-psk.sh
```

Provisioning replaces the key stored by the sensor. Keep a secure backup; a
Windows driver may reprovision it later. Confirm raw capture before installing
the libfprint fork:

```sh
sudo gxfp_capture --psk-raw32 /var/lib/fprintd/gxfp/psk_raw32.bin
file finger.pgm
```

## Install libfprint integration

Installing a libfprint fork replaces a core distribution package. On Arch,
build a pacman-managed package so installation and removal remain tracked:

```sh
./scripts/build-arch-package.sh
sudo pacman -U config/arch/libfprint-gxfp-*.pkg.tar.zst
sudo systemctl restart fprintd
fprintd-enroll
fprintd-verify
```

Pacman will ask to replace the conflicting stock `libfprint` package. To return
to the distribution version later, run `sudo pacman -S libfprint`.

For a local test installation on another distribution:

```sh
sudo meson install -C build/libfprint
sudo ldconfig
sudo systemctl restart fprintd
fprintd-enroll
fprintd-verify
```

To debug a session:

```sh
sudo systemctl stop fprintd
sudo env FP_GXFP_LOG=1 /usr/lib/fprintd
```

## Troubleshooting

- No `GXFP5130:00`: enable the fingerprint reader in firmware and check
  `find /sys/bus/acpi/devices -maxdepth 1 -name 'GXFP5130*'`.
- No `/dev/gxfp`: inspect `journalctl -k -b -g gxfp`; check that the running
  kernel and `modinfo gxfp` vermagic match.
- `open(/dev/gxfp) failed: Operation not permitted` from fprintd: reinstall the
  transport with `sudo ./scripts/install.sh` so the systemd `DeviceAllow`
  drop-in is installed, then restart `fprintd.service`.
- TLS MAC verification failure: the host PSK does not match the sensor PSK.
- Capture works but enrollment fails: the transport/TLS layers are healthy;
  collect `FP_GXFP_LOG=1` output and test the SIGFM/libfprint layer.
- Finger-up is occasionally missed on firmware `GF_GCC_EC_20067`: this package
  retries WAIT_UP with the last successful threshold table. After three failed
  retries it returns a timeout and runs recovery instead of hanging forever.
- After a kernel upgrade: run `dkms status` and rebuild with
  `sudo dkms autoinstall -k "$(uname -r)"`.

## Removal

```sh
sudo ./scripts/uninstall.sh
```

The uninstall script does not delete the PSK or enrolled fingerprints.

## Upstream provenance

- Kernel transport: `Void755/gxfp_linux_driver`, snapshot `594c372`.
- Userspace library/tools: `Void755/gxfpmoc`, snapshot `4b489a7`.
- libfprint fork: `Void755/libfprint`, snapshot `1f7941e` plus the local OpenCV
  5 fallback already present on this machine.

The stack is experimental and is not affiliated with Goodix or Huawei.

## Acknowledgements

This integration was built with [Claude Code](https://claude.ai/code) (Anthropic).
