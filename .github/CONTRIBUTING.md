# Contributing

## Reporting a bug

Use the [bug report template](https://github.com/Metrohan/gxfp5130-linux/issues/new?template=bug_report.yml).
Always include:

- `uname -r` output
- `journalctl -k -b -g gxfp` output
- Which step failed (build / install / PSK provision / capture / enrollment)

## Reporting a compatible device

If the driver works on a MateBook model not listed in the README, open a
[compatibility report](https://github.com/Metrohan/gxfp5130-linux/issues/new?template=compatibility_report.yml).
Include the laptop model, ACPI ID from `/sys/bus/acpi/devices/GXFP5130*/`, and the
firmware string from `journalctl -k -b -g 'INIT: FW'`.

## Submitting a patch

1. Fork the repo and create a branch from `main`.
2. Keep changes focused — one logical change per PR.
3. Test on real hardware before submitting. Include `dmesg` and `fprintd-verify` output.
4. Open a pull request with a clear description of what changed and why.

### Kernel module patches

The kernel module lives in `kernel/`. Follow existing code style (Linux kernel
coding style). Run `make` and load the module before submitting.

### libfprint patches

The libfprint fork is in `libfprint/`. Build with `./scripts/build.sh` and test
enrollment and verification end-to-end.

### Script/packaging patches

Scripts are in `scripts/`. Keep them POSIX-compatible where possible. Test on a
clean installation.

## What we are not looking for

- Unrelated refactors
- Changes that have not been tested on real hardware
- Patches for devices that have native libfprint support upstream

## Questions

Open a [Discussion](https://github.com/Metrohan/gxfp5130-linux/discussions) for
questions that are not bug reports.
