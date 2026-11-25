# Issue #8 implementation plan

Goal: compatible new PSK directory without changing existing sensor keys.
Spec: TASK.md. C driver, POSIX shell scripts, Python unittest and C regression harness.

1. Write and run regression tests against existing selection/provision behavior (RED).
2. Add driver-local PSK read helper: explicit path, otherwise new path then legacy only on ENOENT. Reuse gxfp_read_file_all. Driver retains its existing validation/session behavior.
3. Provision new installs into new path; protect both existing paths, retain legacy target for explicit replacement, reject two existing paths. Restore SELinux contexts when restorecon is available. Doctor follows path priority; document that shell FP_GXFP_PSK is not automatically inherited by fprintd.
4. Update both README languages and PSK guide copies; add Fedora test handoff.
5. Run regression suite, CMake/CTest and configured Meson tests. Independent reviewer checks uncommitted diff against TASK.md. Commit/push/install require separate authorization.

Review focus: explicit nonexistent paths, EACCES, invalid 32-byte key, dangling symlinks, legacy-only and dual-key provisioning, SELinux context restoration.
