# Changelog

Release notes, copied verbatim from the published GitHub releases. Newest first.
Source: https://github.com/Metrohan/gxfp5130-linux/releases

## Unreleased

- README: Omarchy troubleshooting note (`omarchy-setup-security-fingerprint` detects USB readers only), reported by @choirib in #15.
- README: troubleshooting rows now include the literal error strings `No devices available` and `SSL - Verification of the message MAC failed`.
- Issue templates: warning not to paste key material; optional `doctor.sh` output and firmware fields.

## v0.2.0 (2026-10-10)

Second release. 40 commits since v0.1.0, most of them from community contributors.

### Re-enrollment may be required

Matching changed in #11 (ratio test 0.75 → 0.85, plus a CLAHE contrast step).
Fingerprints enrolled with v0.1.0 may match differently or not at all.
If verify stops matching after the upgrade, delete and re-enroll:

    fprintd-delete $USER
    fprintd-enroll

### What changed

**Build and install**
- Modern Arch builds: external/DKMS module build, the Mbed TLS 3 compatibility
  package, and no more high-order RX queue allocation (#3, @nawka12)
- `doctor.sh` now checks the actual pkg-config module names and `glib-mkenums`;
  README Arch package list fixed (#5, reported by @shern2)
- `install.sh` runs `udevadm trigger --subsystem-match=misc`

**Matching (SIGFM)**
- Geometric matching fix (#3, @nawka12)
- Ratio 0.85 + CLAHE so matches survive day-to-day skin changes (#11, @nawka12)
- Unit tests for both halves of that change, run under `meson test` (#13, @nawka12)
- Fix a leak and a critical warning when saving SIGFM prints (#12, @nawka12)

**Firmware**
- Fix WAIT_UP on firmware `GF_GCC_EC_20069` and widen the re-arm budget
  from 3 to 20 (#14, @fstronin)

**SELinux (Fedora)**
- `gxfp_local.te` policy module shipped in the repo (#9, @TarekELz);
  README section for Fedora/SELinux enforcing

**Hardware reports added to the README**
- MateBook MCLG-XX, FW `GF_GCC_EC_20055`: verified (@shern2, #4)
- MateBook 14 2022 KLVF-XX, FW `GF_GCC_EC_20040`: partial, placement-sensitive (@JonasKenke, #7)
- MateBook D MDG-XX, FW `GF_GCC_EC_20055`: verified (@choirib, #15)
- Troubleshooting: kernel-side trace via debugfs, MAC-failure diagnosis

### Verification

On a clean checkout of this release (`git archive`), Arch Linux, kernel
7.2.9-arch1-1, Mbed TLS 3 compatibility package, OpenCV 5:

- `scripts/doctor.sh`, `scripts/build.sh`: passed
- libfprint `meson test`: 5 passed, 20 skipped (driver tests need introspection,
  which `build.sh` disables)
- userspace and vendored `ctest`: passed

Not tested by the maintainer for this release: loading the module and
`fprintd-enroll` / `fprintd-verify` on the v0.2.0 build, firmware
`GF_GCC_EC_20069` (verified by its reporter only), and next-day matching
across several days.

### Known issues and notes

- The DKMS package version stays `0.1.0` on purpose. `install.sh` removes and
  re-adds that version, so upgrading in place works. `dkms status` will still
  show `gxfp/0.1.0`.
- Fedora SELinux (#8) is still open: moving the default PSK path to
  `/var/lib/fprint/gxfp/` is not done.
- Dual-boot: the sensor holds one PSK. Windows may replace the one Linux
  provisioned, which shows up as `SSL - Verification of the message MAC failed`
  (and `pam_fprintd` then silently falls back to the password). Documented in
  `userspace/PSK.md`, not verified end to end here.
- Updating SIGFM templates after a match (#10) is not part of this release.

### Credits

@nawka12, @TarekELz, @fstronin, @shern2, @JonasKenke, @choirib.
Upstream: Void755 (`gxfp_linux_driver`, `gxfpmoc`, `libfprint` fork), see README.

## v0.1.0 — Initial release (2026-07-17)

## First working release

Full Linux support for the Goodix GXFP5130 fingerprint sensor on Huawei MateBook D16 2024 (MCLF-XX).

### What works

- Kernel module (DKMS): eSPI mailbox transport + GPIO handshake, creates `/dev/gxfp`
- TLS-PSK provisioning via `gxfp_psk_tool`
- Raw fingerprint capture: 80×64 images, FDT detection, finger-up recovery
- libfprint integration with SIGFM matching (Void755 fork + OpenCV 5 fallback)
- 16-stage `fprintd-enroll` → `fprintd-verify` returns `verify-match`
- PAM integration: `sudo` and the login screen accept fingerprint

### Verified on

| Item | Value |
|---|---|
| Laptop | Huawei MateBook D16 2024, MCLF-XX / M1010 |
| Sensor | `GXFP5130:00` |
| Firmware | `GF_GCC_EC_20067` |
| Kernel | Arch Linux `7.1.3-arch1-3` |

### Quick start

```sh
./scripts/doctor.sh && ./scripts/build.sh
sudo ./scripts/install.sh && sudo modprobe gxfp
sudo ./scripts/provision-psk.sh
./scripts/build-arch-package.sh
sudo pacman -U config/arch/libfprint-gxfp-*.pkg.tar.zst
fprintd-enroll && fprintd-verify
```

See [README](https://github.com/Metrohan/gxfp5130-linux/blob/main/README.md) for full instructions.

### Upstream provenance

- Kernel transport: [Void755/gxfp_linux_driver](https://github.com/Void755/gxfp_linux_driver) @ `594c372`
- Userspace tools: [Void755/gxfpmoc](https://github.com/Void755/gxfpmoc) @ `4b489a7`
- libfprint fork: [Void755/libfprint](https://github.com/Void755/libfprint) @ `1f7941e`
