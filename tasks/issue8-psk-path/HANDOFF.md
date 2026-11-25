# Handoff — issue #8

Updated 2026-10-21. Isolated worktree `/tmp/gxfp-issue8`, branch `fix/issue8-psk-path`, base `e702d34`. Main checkout's unrelated changes are untouched. Vault memory is read-only.

## Implemented
- Driver uses explicit FP_GXFP_PSK, otherwise new /var/lib/fprint/gxfp/psk_raw32.bin, with legacy fallback only on absence; permission errors, malformed keys and direct dangling symlinks do not select another key.
- Provisioning protects existing keys at either location, uses new path for fresh installs, keeps legacy target for explicit legacy-only replacement, refuses two existing paths and symlink replacement. Runs restorecon when available.
- Doctor reports path and legacy use, checks ancestor accessibility, warns that terminal environment differs from fprintd service environment.
- English/Turkish README and both PSK guides updated. Fedora test request prepared, not sent.

## Evidence and limits
23 regression tests, userspace CTest 1/1, libfprint build succeeded. Meson tests 5 passed/20 skipped with LD_LIBRARY_PATH removed only from child process; inherited /opt/MVS caused five loader errors in first run. Independent review found doctor EACCES issue and stale backup documentation; both fixed with red-green evidence for doctor. See TESTS.md and REVIEW.md.
No installed package, service, sensor key, kernel, PAM, or SELinux policy was changed. Fedora enforcing behavior needs Tarek's actual machine. Published with user authorization on 2026-10-21: implementation commit `bc12719`, draft PR https://github.com/Metrohan/gxfp5130-linux/pull/22, Tarek notification https://github.com/Metrohan/gxfp5130-linux/issues/8#issuecomment-6316503141. Issue #8 remains open. Worktree is under /tmp: preserve it until reviewed/committed.

## Rollback
Local change can be left on this isolated branch without affecting the installed driver. A published/installed candidate can be rolled back to prior libfprint with the original legacy key if no sensor key was rotated. Explicitly replacing a sensor key requires its own secure backup/recovery procedure; local tests never do this.

Next exact action: Review TarekELz's Fedora enforcing results on issue #8 before proposing PR readiness/merge.
