#!/usr/bin/env python3
"""Exercise the installer's group drop-in block in a temporary directory.

The Core runs as a systemd DynamicUser account with no groups of its own, so
it can open a sound card only when a drop-in grants the audio group. Only the
drop-in directory is redirected; no service commands run.
"""
import os
from pathlib import Path
import subprocess
import tempfile
import unittest


HERE = Path(__file__).resolve().parent
INSTALLER = Path(os.environ.get("RETENTION_TEST_INSTALLER", HERE / "install-core-selector.sh"))
COMMON = HERE.parents[1] / "packaging/station-image/common"
DROPIN_DIR = "/etc/systemd/system/nereusd.service.d"
BEGIN = "# BEGIN group drop-ins\n"
END = "# END group drop-ins\n"


class GroupDropins(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory(prefix="nereus-dropins-")
        self.addCleanup(self.tmp.cleanup)
        self.dir = Path(self.tmp.name).resolve() / "nereusd.service.d"
        self.text = INSTALLER.read_text()

    def block(self):
        self.assertIn(BEGIN, self.text, "the installer must carry the group drop-in block")
        block = self.text.split(BEGIN, 1)[1].split(END, 1)[0]
        self.assertIn(DROPIN_DIR, block)
        return block.replace(DROPIN_DIR, str(self.dir))

    def run_block(self, commands):
        return subprocess.run(["bash", "-s"], input="set -euo pipefail\n" + self.block() + commands,
                              capture_output=True, text=True, timeout=10)

    def names(self):
        return sorted(p.name for p in self.dir.iterdir()) if self.dir.exists() else []

    def test_core_without_grants_gets_the_audio_and_serial_dropins(self):
        result = self.run_block("add_group_dropins\n"
                                "echo flags=$audio_dropin_added$serial_dropin_added\n")
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn("flags=11", result.stdout)
        self.assertEqual(self.names(), ["audio.conf", "serial.conf"])
        audio = (self.dir / "audio.conf").read_text()
        self.assertIn("[Service]\nSupplementaryGroups=audio\n", audio,
                      "without the audio group the Core cannot open a sound card")
        # One content for every route: the station images' own files.
        self.assertEqual(audio, (COMMON / "nereusd-audio.conf").read_text())
        self.assertEqual((self.dir / "serial.conf").read_text(),
                         (COMMON / "nereusd-serial.conf").read_text())
        for name in ("audio.conf", "serial.conf"):
            self.assertEqual((self.dir / name).stat().st_mode & 0o777, 0o644)

    def test_rock_names_already_present_are_kept_and_never_duplicated(self):
        # The Rock was first set up by hand with these two names.
        self.dir.mkdir(parents=True)
        (self.dir / "nereusd-audio.conf").write_text("[Service]\nSupplementaryGroups=audio\n# mine\n")
        (self.dir / "nereusd-serial.conf").write_text("[Service]\nSupplementaryGroups=dialout\n# mine\n")
        result = self.run_block("add_group_dropins\n"
                                "echo flags=$audio_dropin_added$serial_dropin_added\n"
                                "remove_added_group_dropins\n")
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn("flags=00", result.stdout)
        self.assertEqual(self.names(), ["nereusd-audio.conf", "nereusd-serial.conf"])
        self.assertIn("# mine", (self.dir / "nereusd-audio.conf").read_text())
        self.assertIn("# mine", (self.dir / "nereusd-serial.conf").read_text())

    def test_existing_dropin_under_the_shared_name_is_left_as_found(self):
        self.dir.mkdir(parents=True)
        (self.dir / "audio.conf").write_text("[Service]\nSupplementaryGroups=audio video\n")
        result = self.run_block("add_group_dropins\n"
                                "echo flags=$audio_dropin_added$serial_dropin_added\n")
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn("flags=01", result.stdout)
        self.assertEqual(self.names(), ["audio.conf", "serial.conf"])
        self.assertEqual((self.dir / "audio.conf").read_text(),
                         "[Service]\nSupplementaryGroups=audio video\n")

    def test_rollback_removes_only_what_this_install_added(self):
        self.dir.mkdir(parents=True)
        (self.dir / "nereusd-serial.conf").write_text("[Service]\nSupplementaryGroups=dialout\n")
        (self.dir / "override.conf").write_text("[Service]\nNice=0\n")
        result = self.run_block("add_group_dropins\n"
                                "test -f '" + str(self.dir / "audio.conf") + "'\n"
                                "remove_added_group_dropins\n")
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(self.names(), ["nereusd-serial.conf", "override.conf"])

    def test_install_adds_the_grants_before_start_and_rollback_takes_them_back(self):
        install = self.text.split("trap rollback EXIT", 1)[1].split("trap - EXIT", 1)[0]
        self.assertIn("\nadd_group_dropins\n", install)
        self.assertLess(install.index("\nadd_group_dropins\n"),
                        install.index("systemctl daemon-reload\nsystemctl start nereusd\n"))
        rollback = self.text.split("rollback() {", 1)[1].split("\n}\n", 1)[0]
        self.assertIn("remove_added_group_dropins || restored=0", rollback)
        self.assertLess(rollback.index("remove_added_group_dropins"),
                        rollback.index("systemctl daemon-reload"))
        # The functions exist before the trap that may call them.
        self.assertLess(self.text.index(END), self.text.index("trap rollback EXIT"))


if __name__ == "__main__":
    unittest.main()
