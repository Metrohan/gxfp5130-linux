# fix: use Fedora-compatible PSK path with legacy fallback

New installations currently store their PSK outside Fedora's fprintd SELinux file-context rule. Default to `/var/lib/fprint/gxfp/psk_raw32.bin`, retaining `/var/lib/fprintd/gxfp/psk_raw32.bin` only when the new default is absent. Explicit `FP_GXFP_PSK` paths and access errors never silently select another key.

Provisioning protects existing files at both locations, keeps explicit replacement on legacy-only installations at the old path, rejects ambiguous two-key replacement, and restores SELinux labels when available. Doctor and both README languages explain selection, migration and legacy labels. TarekELz's existing GPL-2.0 device policy is unchanged.

Validation: 23 offline regressions; userspace CTest 1/1; full configured libfprint build; Meson 5 passed/20 skipped. Meson required clearing inherited LD_LIBRARY_PATH in the test process because /opt/MVS caused a libusb loader conflict. Independent review findings were corrected. Actual Fedora enforcing enroll/verify/sudo acceptance is pending; this must not automatically close #8.

Refs #8.
