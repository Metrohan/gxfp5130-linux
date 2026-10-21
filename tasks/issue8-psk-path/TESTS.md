# Evidence
- Baseline: cmake configured with MbedTLS_DIR=/usr/lib/mbedtls3/cmake/MbedTLS; userspace built; CTest gxfp_fdt_retry_policy 1/1 passed.
- RED: python3 -m unittest discover -s tests -v: initial 15 cases, 11 expected failures against old code. Actual driver selection/read statements compiled with GLib and production file reader; absolute paths redirected into temporary fixtures. Hardware tool replaced at shell boundary only.
- GREEN: 18 driver/provision tests passed, including fresh installation and explicit legacy replacement using fake sensor responses.
- Doctor RED: 3 new cases failed (new path, legacy warning, explicit path); total 21 cases.
- GREEN: all 21 cases passed.
- Full libfprint build/tests and independent review: pending.
- No real PSK read, hardware provisioning, service changes, package installation or Fedora enforcing tests performed.
- Raw local logs: /tmp/gxfp-issue8-build/. Test harness is not an end-to-end FpDevice test; full build and Fedora checks are separate.

## Final local results (2026-10-21)
- Review regression RED: 23 cases, 2 failures reproduced doctor incorrectly selecting the legacy key when the new parent was inaccessible or not a directory.
- Review regression GREEN: `python3 -B -m unittest discover -s tests -v`: 23/23 passed after ancestor checks. Tests do not expose key bytes in output.
- `cmake --build /tmp/gxfp-issue8-build/userspace -j2` succeeded; `ctest --test-dir /tmp/gxfp-issue8-build/userspace --output-on-failure`: 1/1 passed.
- `PKG_CONFIG_PATH=/usr/lib/mbedtls3/pkgconfig meson setup /tmp/gxfp-issue8-build/libfprint libfprint -Ddrivers=gxfp -Ddoc=false -Dintrospection=false -Dgtk-examples=false -Dudev_rules=disabled -Dudev_hwdb=disabled` succeeded.
- `meson compile -C /tmp/gxfp-issue8-build/libfprint -j2` succeeded. Existing NBIS compiler warnings were emitted (e.g. unused variable in remove.c).
- First `meson test -C /tmp/gxfp-issue8-build/libfprint --print-errorlogs`: 1 passed, 5 failed, 19 skipped. All five failures were loader errors (`libgusb.so.2: undefined symbol: libusb_set_option`) with inherited `/opt/MVS` LD_LIBRARY_PATH.
- `env -u LD_LIBRARY_PATH meson test -C /tmp/gxfp-issue8-build/libfprint --print-errorlogs`: 5 passed, 0 failed, 20 skipped. Only this child process environment was changed. Introspection/other drivers/hwdb are disabled in this configuration; skips are not passes.
- `sh -n scripts/provision-psk.sh scripts/doctor.sh`, `git diff --check` and `cmp userspace/PSK.md libfprint/libfprint/drivers/gxfpmoc/PSK.md` passed.
- Independent reviewer ran the then-current 21 tests successfully; found one MAJOR doctor issue and one documentation issue. Both addressed; see REVIEW.md. No independent Fedora acceptance claim.
