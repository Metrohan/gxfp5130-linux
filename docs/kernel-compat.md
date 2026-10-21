# Kernel compatibility

Only what has been observed is listed. "Reporter" means the result comes from a
GitHub issue and has not been reproduced by the maintainer.

## Kernels seen in the wild

| Kernel | Distro | Source | Reported result |
| --- | --- | --- | --- |
| 7.2.9-arch1-1 | Arch Linux | maintainer, MateBook D16 2024 | module builds and loads; enroll and verify work |
| 7.2.3-arch1-3 | Arch Linux | reporter, #4 | works |
| 7.2.4-arch1-2 | Arch Linux | reporter, #7 | partial: edge/partial touches do not match |
| 7.2.5-3-omarchy | Omarchy (Arch) | reporter, #15 | works |
| 7.2.7 / 7.2.10-zen1 | Arch (zen) | reporter, #17 | FDT never reports finger down; sensor/firmware suspected, not the kernel |
| 7.0.0-38-generic | Ubuntu | reporter, #20 | enroll/verify unreliable; Windows works |

Results other than the first row depend on the laptop model and firmware as much
as on the kernel; see the hardware table in the README.

## What is not known

- **Minimum supported kernel.** The module has no `LINUX_VERSION_CODE` guards.
  No kernel older than 7.0 has been tried.
- **Newer kernels than the ones above.** Each release can break an out-of-tree
  module; the table is only updated after someone builds against it.
- **CI.** `.github/workflows/ci.yml` builds the module on the current Arch and
  Fedora kernel headers and on the Ubuntu runner via DKMS. Those results are
  not copied here; check the latest run.

## Adding a row

Open a compatibility report with `uname -r`, distro, `dkms status` and the
result of `fprintd-enroll` / `fprintd-verify`. A row is added only for a
kernel that someone actually ran.
