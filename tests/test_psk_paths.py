"""Offline regression tests: real driver selection block and shell control flow.

Only absolute filesystem roots and hardware/privileged command boundaries are
replaced. No sensor, real PSK, root privilege or SELinux host change is used.
"""
import os
from pathlib import Path
import re
import shlex
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]


class Paths(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory(prefix='gxfp-psk-test-')
        self.addCleanup(self.tmp.cleanup)
        self.root = Path(self.tmp.name)
        self.new = self.root / 'var/lib/fprint/gxfp/psk_raw32.bin'
        self.old = self.root / 'var/lib/fprintd/gxfp/psk_raw32.bin'
        self.env = dict(os.environ)
        self.env.pop('FP_GXFP_PSK', None)

    def key(self, path, data=b'N' * 32):
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(data)

    def driver(self, override=None):
        # Compile the actual path-selection/read statements, without needing
        # a live FpDevice. Keep the production file reader and GLib behavior.
        source = (ROOT / 'libfprint/libfprint/drivers/gxfp.c').read_text()
        start = source.index('  psk_env = g_getenv ("FP_GXFP_PSK");')
        end = source.index('  if (r < 0)', start)
        block = source[start:end]
        defines = '\n'.join(re.findall(r'^#define GXFP_\w*PSK_PATH.*$', source, re.M))
        defines = defines.replace('"/var/lib/', '"' + str(self.root) + '/var/lib/')
        program = '''#include <sys/stat.h>
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <glib.h>
#include "gxfp/algo/common.h"
''' + defines + '''
int main(void) {
  struct { char *psk_path; } storage = {0}, *self = &storage;
  const char *psk_env;
  uint8_t *psk_raw = NULL;
  size_t psk_raw_len = 0;
  int r;
''' + block + '''
  printf("%d\\n%s\\n%zu\\n", r, self->psk_path, psk_raw_len);
  free(psk_raw); g_free(self->psk_path); return 0;
}
'''
        cfile = self.root / 'selection.c'
        cfile.write_text(program)
        binary = self.root / 'selection'
        flags = shlex.split(subprocess.check_output(['pkg-config', '--cflags', '--libs', 'glib-2.0'], text=True))
        subprocess.run(['cc', '-Wall', '-Wextra', '-Werror', '-I' + str(ROOT / 'userspace/include'), str(cfile), str(ROOT / 'userspace/src/algo/common.c'), '-o', str(binary), *flags], check=True, capture_output=True)
        if override is not None:
            self.env['FP_GXFP_PSK'] = str(override)
        output = subprocess.check_output([binary], env=self.env, text=True).splitlines()
        return int(output[0]), output[1], int(output[2])

    def test_fresh_install_reads_new_path(self):
        self.key(self.new)
        self.assertEqual(self.driver(), (0, str(self.new), 32))

    def test_legacy_only_falls_back(self):
        self.key(self.old)
        self.assertEqual(self.driver(), (0, str(self.old), 32))

    def test_new_wins_when_both_exist(self):
        self.key(self.new)
        self.key(self.old, b'O' * 32)
        self.assertEqual(self.driver(), (0, str(self.new), 32))

    def test_explicit_missing_path_does_not_fall_back(self):
        self.key(self.new)
        self.key(self.old)
        custom = self.root / 'missing'
        self.assertEqual(self.driver(custom)[:2], (-2, str(custom)))

    def test_explicit_path_wins(self):
        self.key(self.new)
        self.key(self.old)
        custom = self.root / 'custom'
        self.key(custom)
        self.assertEqual(self.driver(custom), (0, str(custom), 32))

    def test_empty_override_uses_new_default(self):
        self.key(self.new)
        self.assertEqual(self.driver(''), (0, str(self.new), 32))

    def test_access_denied_does_not_fall_back(self):
        self.key(self.new)
        self.key(self.old)
        self.new.chmod(0)
        self.assertNotEqual(os.geteuid(), 0, 'Run these tests unprivileged')
        self.assertEqual(self.driver()[:2], (-13, str(self.new)))

    def test_invalid_key_does_not_fall_back(self):
        self.key(self.new, b'bad')
        self.key(self.old)
        # Selection must pass the short key onward, not hide it with another key.
        self.assertEqual(self.driver(), (0, str(self.new), 3))

    def test_dangling_new_link_does_not_fall_back(self):
        self.key(self.old)
        self.new.parent.mkdir(parents=True)
        self.new.symlink_to(self.root / 'missing')
        self.assertEqual(self.driver()[:2], (-2, str(self.new)))

    def test_no_keys_reports_new_default(self):
        self.assertEqual(self.driver()[:2], (-2, str(self.new)))

    def doctor(self, override=None):
        source = (ROOT / 'scripts/doctor.sh').read_text()
        block = source[source.index('psk='):source.index("printf '\\nSummary:")]
        block = block.replace('/var/lib/', str(self.root) + '/var/lib/')
        if override is not None:
            self.env['FP_GXFP_PSK'] = str(override)
        script = "ok() { echo OK \"$1\"; }; warn() { echo WARN \"$1\"; }; fail() { echo FAIL \"$1\"; };\n" + block
        return subprocess.run(['sh', '-c', script], env=self.env, capture_output=True, text=True)

    def test_doctor_checks_new_install(self):
        self.key(self.new)
        result = self.doctor()
        self.assertIn('PSK is exactly 32 bytes', result.stdout)

    def test_doctor_identifies_legacy_path(self):
        self.key(self.old)
        self.assertIn('legacy', self.doctor().stdout.lower())

    def test_doctor_does_not_hide_inaccessible_new_parent(self):
        self.key(self.new)
        self.key(self.old)
        self.new.parent.chmod(0)
        self.addCleanup(self.new.parent.chmod, 0o700)
        result = self.doctor()
        self.assertIn('FAIL', result.stdout)
        self.assertNotIn('Using legacy', result.stdout)
        self.assertNotIn('PSK is exactly 32 bytes', result.stdout)

    def test_doctor_does_not_hide_non_directory_parent(self):
        self.new.parent.parent.mkdir(parents=True)
        self.new.parent.write_text('not a directory')
        self.key(self.old)
        result = self.doctor()
        self.assertIn('FAIL', result.stdout)
        self.assertNotIn('Using legacy', result.stdout)

    def test_doctor_respects_explicit_path(self):
        custom = self.root / 'custom'
        self.key(custom)
        self.assertIn('PSK is exactly 32 bytes', self.doctor(custom).stdout)

    def provision(self, *args, success=False):
        bindir = self.root / 'bin'
        bindir.mkdir(exist_ok=True)
        tool = bindir / 'gxfp_psk_tool'
        tool.write_text('#!/bin/sh\necho called >> "$CALL_LOG"\nexit 99\n')
        if success:
            tool.write_text("#!/bin/sh\necho \"$1\" >> \"$CALL_LOG\"\ncase \"$1\" in\n--build-bb010002) printf blob > \"$2\"; printf '01234567890123456789012345678901' > \"$4\" ;;\n--upload-bb010002) test -f \"$2\" ;;\n*) exit 98 ;;\nesac\n")
        tool.chmod(0o755)
        installer = bindir / 'install'
        installer.write_text("#!/usr/bin/env python3\nimport os, sys\na=sys.argv[1:]\nfor flag in ('-o', '-g'):\n if flag in a:\n  i=a.index(flag); del a[i:i+2]\nos.execv('/usr/bin/install', ['install', *a])\n")
        installer.chmod(0o755)
        label = bindir / 'restorecon'
        label.write_text('#!/bin/sh\nprintf "%s\\n" "$@" > "$LABEL_LOG"\n')
        label.chmod(0o755)
        ident = bindir / 'id'
        ident.write_text('#!/bin/sh\necho 0\n')
        ident.chmod(0o755)
        source = (ROOT / 'scripts/provision-psk.sh').read_text()
        source = source.replace('/var/lib/', str(self.root) + '/var/lib/').replace('/dev/gxfp', '/dev/null')
        script = self.root / 'provision.sh'
        script.write_text(source)
        env = dict(self.env, PATH=str(bindir) + ':' + os.environ['PATH'], CALL_LOG=str(self.root / 'calls'), LABEL_LOG=str(self.root / 'labels'))
        return subprocess.run(['sh', script, *args], env=env, capture_output=True, text=True)

    def assert_no_tool(self, result):
        self.assertEqual(result.returncode, 2, result.stdout + result.stderr)
        self.assertFalse((self.root / 'calls').exists(), 'Sensor tool must not be called')

    def test_provision_protects_new_key(self):
        self.key(self.new)
        self.assert_no_tool(self.provision())
        self.assertEqual(self.new.read_bytes(), b'N' * 32)

    def test_provision_protects_legacy_key(self):
        self.key(self.old)
        self.assert_no_tool(self.provision())

    def test_provision_protects_dangling_legacy_link(self):
        self.old.parent.mkdir(parents=True)
        self.old.symlink_to(self.root / 'missing')
        self.assert_no_tool(self.provision())

    def test_provision_refuses_ambiguous_replace(self):
        self.key(self.new)
        self.key(self.old, b'O' * 32)
        self.assert_no_tool(self.provision('--replace'))

    def test_fresh_provision_installs_new_key_and_relabels(self):
        result = self.provision(success=True)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(self.new.read_bytes(), b'01234567890123456789012345678901')
        self.assertFalse(self.old.exists())
        self.assertEqual(self.new.stat().st_mode & 0o777, 0o600)
        self.assertEqual(self.new.parent.stat().st_mode & 0o777, 0o700)
        self.assertIn(str(self.new.parent), (self.root / 'labels').read_text())

    def test_explicit_legacy_replace_stays_at_legacy_path(self):
        self.key(self.old)
        result = self.provision('--replace', success=True)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertFalse(self.new.exists())
        self.assertEqual(self.old.read_bytes(), b'01234567890123456789012345678901')

    def test_explicit_replace_rejects_symlink(self):
        self.new.parent.mkdir(parents=True)
        self.new.symlink_to(self.root / 'missing')
        self.assert_no_tool(self.provision('--replace'))

    def test_provision_rejects_unknown_option(self):
        self.assert_no_tool(self.provision('--replcae'))


if __name__ == '__main__':
    unittest.main()
