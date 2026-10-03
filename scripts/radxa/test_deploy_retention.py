#!/usr/bin/env python3
"""Exercise the installer's actual post-success hook in temporary directories.

Only literal filesystem roots are redirected; no service commands run.
"""
import json
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest


HERE = Path(__file__).resolve().parent
INSTALLER = Path(os.environ.get("RETENTION_TEST_INSTALLER", HERE / "install-core-selector.sh"))


class DeploymentRetention(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory(prefix="nereus-deploy-hook-")
        self.addCleanup(self.tmp.cleanup)
        self.root = Path(self.tmp.name).resolve()
        self.stages = self.root / "stages"
        self.backups = self.root / "backups"
        self.installed = self.root / "installed"
        self.current = "cccccccc"
        self.previous = "bbbbbbbb"
        self.backup = self.backups / "rollback-bbbbbbbb-before-cccccccc"
        for name in ("aaaaaaaa", self.previous, self.current):
            (self.stages / name).mkdir(parents=True)
        self.old_backup = self.backups / "rollback-aaaaaaaa-before-bbbbbbbb"
        self.old_backup.mkdir(parents=True)
        (self.old_backup / "payload").write_bytes(b"old recovery")
        (self.backup / "daemon-config").mkdir(parents=True)
        for name in ("files.tar", "libNereusCore.so", "assets.tar"):
            (self.backup / name).write_bytes(b"verified recovery " + name.encode())
        for name in ("bin/nereusd", "lib/libNereusCore.so", "lib/librade.so.0.1"):
            stage = self.stages / self.current / "stage/usr/local" / name
            installed = self.installed / name
            stage.parent.mkdir(parents=True, exist_ok=True)
            installed.parent.mkdir(parents=True, exist_ok=True)
            stage.write_bytes(b"new binary " + name.encode())
            installed.write_bytes(stage.read_bytes())
        self.helper = self.backups / "prune-core-artifacts.py"
        shutil.copyfile(HERE / "prune-core-artifacts.py", self.helper)

    def hook(self):
        # The baseline hook is empty, preserving the old accumulation behavior.
        hook = INSTALLER.read_text().rsplit("trap - EXIT", 1)[1]
        # Substitute all source roots before inserting fixture paths. On Radxa
        # TMPDIR itself can contain a production root; never rewrite it twice.
        hook = hook.replace("/home/yonder/nereus-core", "__TEST_STAGES__")
        hook = hook.replace("/var/lib/nereus-build", "__TEST_BACKUPS__")
        hook = hook.replace("/usr/local", "__TEST_INSTALLED__")
        # The stage-relative suffix must remain stage/usr/local after root mapping.
        hook = hook.replace("stage__TEST_INSTALLED__", "stage/usr/local")
        hook = hook.replace("__TEST_STAGES__", str(self.stages))
        hook = hook.replace("__TEST_BACKUPS__", str(self.backups))
        hook = hook.replace("__TEST_INSTALLED__", str(self.installed))
        prefix = 'set -euo pipefail\nbackup=$1\ncheckpoint=$2\nprevious=$3\nretention=$4\n'
        return subprocess.run(["bash", "-s", "--", str(self.backup), self.current,
                               self.previous, str(self.helper)], input=prefix + hook,
                              capture_output=True, text=True, timeout=10)

    def test_healthy_success_publishes_receipt_and_replaces_old_artifacts(self):
        result = self.hook()
        self.assertEqual(result.returncode, 0, result.stderr)
        receipt = self.backup / "successful-install.json"
        self.assertTrue(receipt.is_file(), "healthy install must publish its recovery receipt")
        proof = json.loads(receipt.read_text())
        self.assertEqual(proof, json.loads((self.backups / "current-install.json").read_text()))
        self.assertEqual(proof["checkpoint"], self.current)
        self.assertEqual(sorted(p.name for p in self.stages.iterdir()), [self.previous, self.current])
        self.assertFalse(self.old_backup.exists())
        self.assertTrue((self.backup / "files.tar").is_file())
        self.assertTrue((self.installed / "bin/nereusd").is_file())

    def test_cleanup_failure_preserves_healthy_install_and_all_recovery(self):
        (self.stages / "aaaaaaaa" / ".created-by-deploy").touch()
        result = self.hook()
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn("WARNING", result.stderr)
        self.assertTrue(self.old_backup.is_dir())
        self.assertTrue((self.backup / "files.tar").is_file())
        self.assertEqual((self.installed / "bin/nereusd").read_bytes(), b"new binary bin/nereusd")

    def test_live_binary_mismatch_never_publishes_success_or_prunes(self):
        (self.installed / "bin/nereusd").write_bytes(b"unexpected binary")
        result = self.hook()
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn("WARNING", result.stderr)
        self.assertFalse((self.backups / "current-install.json").exists())
        self.assertTrue(self.old_backup.is_dir())
        self.assertTrue((self.stages / "aaaaaaaa").is_dir())


if __name__ == "__main__":
    unittest.main()
