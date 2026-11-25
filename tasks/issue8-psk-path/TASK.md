# Issue #8: compatible PSK path

Problem: Fedora enforcing policy covers /var/lib/fprint, but the driver and scripts default to /var/lib/fprintd. The device policy and udev trigger already shipped.
Outcome: fresh installs use /var/lib/fprint/gxfp/psk_raw32.bin; existing installations keep their key.
Acceptance: explicit FP_GXFP_PSK wins (including errors); new default wins; fallback only on ENOENT; no fallback on unreadable/invalid keys; provisioning refuses implicit replacement at either location; explicit replacement on a legacy-only installation stays at the legacy path; ambiguous two-key replacement is refused. Doctor follows the same priority and identifies legacy use. Fedora labels restored where available. Existing tests and new regression tests pass; Fedora enforcing acceptance remains separate.
Scope: driver PSK selection, provisioning/doctor, regression tests, English/Turkish docs and mirrored PSK instructions.
Out of scope: sensor protocol, matching, PSK format, automatic migration/rotation, SELinux policy redesign, host installation, publication.
Risks: fallback must not hide access errors; legacy fallback still requires documented SELinux labels; live provisioning is never used in local tests.
