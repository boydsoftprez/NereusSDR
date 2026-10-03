#!/usr/bin/env python3
"""Run each installer's actual healthy Bash hook against a temporary filesystem."""
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

HERE = Path(__file__).resolve().parent
CURRENT = 'cccccccc'
PREVIOUS = 'bbbbbbbb'
BACKUP = 'rollback-bbbbbbbb-before-cccccccc'
TRIO = ('bin/nereusd', 'lib/libNereusCore.so', 'lib/librade.so.0.1')


class PiRetention(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory(prefix='nereus-pi-retention-')
        self.addCleanup(self.tmp.cleanup)
        self.root = Path(self.tmp.name).resolve()
        self.store = self.root / 'store'
        self.uploads = self.root / 'uploads'
        self.installed = self.root / 'installed'
        for root in (self.store, self.uploads):
            for name in ('aaaaaaaa', PREVIOUS, CURRENT):
                (root / name).mkdir(parents=True)
        self.backup = self.store / BACKUP
        (self.backup / 'daemon-config').mkdir(parents=True)
        (self.backup / 'files.tar').write_bytes(b'old executable and libraries')
        (self.backup / 'assets.tar').write_bytes(b'old assets')
        self.old_backup = self.store / 'rollback-aaaaaaaa-before-bbbbbbbb'
        self.old_backup.mkdir()
        (self.old_backup / 'files.tar').write_bytes(b'obsolete recovery')
        self.keep = self.store / CURRENT
        self.unpack = self.keep / 'unpack.fixture'
        self.unpack.mkdir()
        lines = []
        for name in TRIO:
            target = self.installed / name
            target.parent.mkdir(parents=True, exist_ok=True)
            target.write_bytes(b'new installed ' + name.encode())
            lines.append(hashlib.sha256(target.read_bytes()).hexdigest() + '  usr/local/' + name + '\n')
        (self.keep / f'stage-{CURRENT}-pi4.sha256').write_text(''.join(lines))
        self.helper = self.root / 'prune-core-artifacts.py'
        shutil.copyfile(HERE.parent / 'radxa/prune-core-artifacts.py', self.helper)

    def hook(self, installer='upgrade-core-pi4.sh', snapshot='1'):
        script = (HERE / installer).read_text()
        marker = '# BEGIN healthy retention hook\n'
        hook = script.split(marker, 1)[1].split('# END healthy retention hook', 1)[0] if marker in script else ''
        for source, placeholder in (('/home/nereus/nereus-stage', '__UPLOADS__'),
                                    ('/var/lib/nereus-build/pi4', '__STORE__'),
                                    ('/usr/local', '__INSTALLED__')):
            hook = hook.replace(source, placeholder)
        hook = hook.replace('$unpack__INSTALLED__', '$unpack/usr/local')
        for placeholder, target in (('__UPLOADS__', self.uploads), ('__STORE__', self.store),
                                     ('__INSTALLED__', self.installed)):
            hook = hook.replace(placeholder, str(target))
        prefix = '''set -euo pipefail
new=$1
cp=$1
previous=$2
backup=$3
keep=$4
unpack=$5
retention=$6
state_snapshot_ready=$7
'''
        first_install = installer == 'install-core-pi4.sh'
        return subprocess.run(['bash', '-s', '--', CURRENT, CURRENT if first_install else PREVIOUS, '' if first_install else str(self.backup),
                               str(self.keep), str(self.unpack), str(self.helper), snapshot],
                              input=prefix + hook, text=True, capture_output=True, timeout=10)

    def test_healthy_upgrade_prunes_store_uploads_and_old_backup(self):
        result = self.hook()
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertFalse((self.store / 'aaaaaaaa').exists(), result.stdout + result.stderr)
        self.assertFalse((self.uploads / 'aaaaaaaa').exists())
        self.assertFalse(self.old_backup.exists())
        self.assertFalse(self.unpack.exists())
        proof = json.loads((self.store / 'current-install.json').read_text())
        self.assertEqual(proof, json.loads((self.keep / 'successful-install.json').read_text()))
        self.assertEqual(proof['checkpoint'], CURRENT)
        self.assertTrue(proof['post_stop_snapshot_complete'])
        self.assertIn('files.tar', proof['recovery_sha256'])
        self.assertTrue((self.backup / 'daemon-config').is_dir())

    def test_upgrade_retains_installed_hash_evidence(self):
        result = self.hook()
        self.assertEqual(result.returncode, 0, result.stderr)
        evidence = self.keep / 'installed.sha256'
        self.assertTrue(evidence.is_file(), 'installed hash evidence must survive maintenance')
        self.assertIn(hashlib.sha256((self.installed / TRIO[0]).read_bytes()).hexdigest(), evidence.read_text())

    def test_verified_payload_helper_is_installed_when_canonical_helper_was_missing(self):
        payload = self.unpack / 'usr/local/libexec/nereusd/prune-core-artifacts.py'
        payload.parent.mkdir(parents=True)
        payload.write_bytes(self.helper.read_bytes())
        manifest = self.keep / f'stage-{CURRENT}-pi4.sha256'
        with manifest.open('a') as output:
            output.write(hashlib.sha256(payload.read_bytes()).hexdigest() + '  ./usr/local/libexec/nereusd/prune-core-artifacts.py\n')
        result = self.hook()
        self.assertEqual(result.returncode, 0, result.stderr)
        target = self.installed / 'libexec/nereusd/prune-core-artifacts.py'
        self.assertTrue(target.is_file(), result.stderr)
        self.assertEqual(target.read_bytes(), self.helper.read_bytes())
        self.assertFalse((self.store / 'aaaaaaaa').exists(), result.stderr)

    def test_native_pi_manifest_dot_prefix_is_accepted(self):
        manifest = self.keep / f'stage-{CURRENT}-pi4.sha256'
        manifest.write_text(manifest.read_text().replace('  usr/local/', '  ./usr/local/'))
        result = self.hook()
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertFalse((self.store / 'aaaaaaaa').exists(), result.stderr)
        self.assertTrue((self.store / 'current-install.json').is_file())

    def test_cleanup_failure_warns_and_preserves_healthy_core_and_recovery(self):
        (self.store / 'aaaaaaaa' / '.created-by-deploy').touch()
        result = self.hook()
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn('WARNING', result.stderr)
        self.assertTrue(self.old_backup.exists())
        self.assertTrue((self.uploads / 'aaaaaaaa').exists())
        self.assertTrue((self.backup / 'files.tar').is_file())
        self.assertEqual((self.installed / TRIO[0]).read_bytes(), b'new installed bin/nereusd')

    def test_installed_manifest_mismatch_never_prunes_or_publishes(self):
        (self.installed / TRIO[0]).write_bytes(b'unexpected installed binary')
        result = self.hook()
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn('WARNING', result.stderr)
        self.assertFalse((self.store / 'current-install.json').exists())
        self.assertTrue(self.old_backup.exists())
        self.assertTrue((self.uploads / 'aaaaaaaa').exists())

    def test_incomplete_poststop_snapshot_never_prunes(self):
        result = self.hook(snapshot='0')
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn('WARNING', result.stderr)
        self.assertFalse((self.store / 'current-install.json').exists())
        self.assertTrue(self.old_backup.exists())

    def test_first_install_stage_gate_rejects_missing_rnnoise_models(self):
        deep = self.unpack / 'usr/local/share/NereusSDR/models/dfnet3/DeepFilterNet3_onnx.tar.gz'
        deep.parent.mkdir(parents=True)
        deep.write_bytes(b'deep model')
        licenses = self.unpack / 'usr/local/share/doc/nereussdr/deepfilter'
        licenses.mkdir(parents=True)
        for name in ('LICENSE', 'LICENSE-APACHE', 'LICENSE-MIT', 'COMMIT'):
            (licenses / name).write_bytes(b'license')
        script = (HERE / 'install-core-pi4.sh').read_text()
        gate = script[script.index('test -s "$unpack/usr/local/share/NereusSDR/models/dfnet3/'):script.index("echo 'DFNR model and licences present")]
        result = subprocess.run(['bash', '-s', '--', str(self.unpack)],
                                input='set -euo pipefail\nunpack=$1\n' + gate,
                                text=True, capture_output=True, timeout=5)
        self.assertNotEqual(result.returncode, 0, 'both RNNoise models must be present before installation')

    def test_failed_health_gate_never_runs_retention(self):
        script = (HERE / 'upgrade-core-pi4.sh').read_text()
        tail = script[script.index('systemctl is-active --quiet nereusd'):].split('# END healthy retention hook', 1)[0]
        result = subprocess.run(['bash', '-s'], input='set -euo pipefail\nsystemctl() { return 1; }\n' + tail,
                                text=True, capture_output=True, timeout=5)
        self.assertNotEqual(result.returncode, 0)
        self.assertTrue((self.store / 'aaaaaaaa').exists())
        self.assertTrue((self.uploads / 'aaaaaaaa').exists())
        self.assertFalse((self.store / 'current-install.json').exists())

    def test_receipt_publication_failure_keeps_healthy_core_and_all_recovery(self):
        (self.keep / 'installed.sha256').mkdir()
        result = self.hook()
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn('WARNING', result.stderr)
        self.assertFalse((self.store / 'current-install.json').exists())
        self.assertTrue(self.old_backup.exists())
        self.assertTrue((self.uploads / 'aaaaaaaa').exists())
        self.assertTrue((self.backup / 'files.tar').is_file())

    def test_first_install_singleton_does_not_delete_current(self):
        shutil.rmtree(self.old_backup)
        shutil.rmtree(self.backup)
        for root in (self.store, self.uploads):
            for name in ('aaaaaaaa', PREVIOUS):
                shutil.rmtree(root / name)
        result = self.hook('install-core-pi4.sh')
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertTrue((self.store / CURRENT / 'successful-install.json').is_file())
        self.assertTrue((self.uploads / CURRENT).is_dir())

    def test_first_install_retains_only_current_without_backup(self):
        shutil.rmtree(self.old_backup)
        shutil.rmtree(self.backup)
        result = self.hook('install-core-pi4.sh')
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(sorted(p.name for p in self.uploads.iterdir()), [CURRENT])
        stages = sorted(p.name for p in self.store.iterdir() if p.is_dir())
        self.assertEqual(stages, [CURRENT])
        proof = json.loads((self.store / 'current-install.json').read_text())
        self.assertIsNone(proof['backup'])
        self.assertFalse(proof['post_stop_snapshot_complete'])


if __name__ == '__main__':
    unittest.main()
