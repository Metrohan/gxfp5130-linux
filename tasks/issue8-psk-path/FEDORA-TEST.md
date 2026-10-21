# Fedora acceptance request (draft; not sent)

@TarekELz, the PSK path change is ready for review on a local branch (not yet published).
No new sensor protocol or matching changes are included. Your GPL-2.0 device policy is unchanged.

Please keep SELinux enforcing and keep a secure backup of your current PSK.
Do not post key contents or raw fingerprint images.

1. Existing installation: keep the old path and existing fcontext rule. Install the candidate libfprint and run doctor as root, verify, then sudo authentication. Legacy fallback must work with no provisioning.
2. New path with the same key: stop fprintd, copy the existing key to /var/lib/fprint/gxfp/psk_raw32.bin (root:root, dir 0700/file 0600), restorecon -Rv /var/lib/fprint/gxfp, restart fprintd. Confirm ls -Z shows fprintd_var_lib_t and /dev/gxfp has gxfp_device_t. Verify must work using the existing enrollment.
3. Confirm provision-psk.sh without --replace refuses existing keys. With both files present it must refuse even --replace. These refusal checks must not change the sensor key.
4. Fresh provisioning only on a deliberately disposable/fresh installation whose sensor-key replacement you accept: run the updated script, check mode and SELinux labels, enroll, verify and sudo. Do not delete a working key just to simulate a fresh install.
5. Send command outcomes, versions and redacted AVC denials (ausearch -m avc -ts recent), never secrets. Report each case separately.

Rollback: old libfprint with the original old-path key works as long as the sensor key was not changed. The new driver can also use FP_GXFP_PSK configured explicitly in the fprintd service environment. Restore only task-specific changes to the service and key location; retain the existing device policy.
