#!/usr/bin/env python3
"""Real temporary-filesystem retention contracts; never touches a Radxa.

Run with python3 -B -m unittest discover -s scripts/radxa
    -p test_prune_core_artifacts.py -v
"""
import importlib.util
import json
import os
import pathlib
import shutil
import socket
import subprocess
import sys
import tempfile
import unittest

sys.dont_write_bytecode = True
SCRIPT = pathlib.Path(__file__).with_name("prune-core-artifacts.py")
spec = importlib.util.spec_from_file_location("prune_core_artifacts", SCRIPT)
retention = importlib.util.module_from_spec(spec)
sys.modules[spec.name] = retention
spec.loader.exec_module(retention)

A = "aaaaaaaa"
B = "bbbbbbbb"
C = "cccccccc"
D = "dddddddd"
E = "eeeeeeee"
AB = "rollback-aaaaaaaa-before-bbbbbbbb"
BC = "rollback-bbbbbbbb-before-cccccccc"
CD = "rollback-cccccccc-before-dddddddd"


class RetentionContracts(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory(prefix="nereus-retention-test-")
        self.addCleanup(self.temporary.cleanup)
        # macOS /var is a symlink; ordinary fixtures use the real path, while
        # the explicit root/ancestor-symlink cases exercise rejection below.
        self.root = pathlib.Path(self.temporary.name).resolve()
        self.stages = self.root / "stages"
        self.backups = self.root / "backups"
        self.stages.mkdir()
        self.backups.mkdir()

    def artifact(self, root, name, epoch):
        path = root / name
        path.mkdir()
        (path / "payload").write_text("recovery " + name)
        os.utime(path, (epoch, epoch))
        return path

    def fixture(self):
        for name, epoch in ((A, 10), (B, 20), (C, 30)):
            self.artifact(self.stages, name, epoch)
        for name, epoch in ((AB, 5), (BC, 15)):
            self.artifact(self.backups, name, epoch)

    def closed_socket(self, path):
        # Relative binding avoids macOS's short sockaddr_un pathname limit.
        # Restore cwd before invoking pruning so this test owns no live cwd/FD.
        previous = os.open(".", os.O_RDONLY | os.O_DIRECTORY)
        try:
            os.chdir(path.parent)
            with socket.socket(socket.AF_UNIX, socket.SOCK_STREAM) as endpoint:
                endpoint.bind(path.name)
        finally:
            os.fchdir(previous)
            os.close(previous)

    def call(self, *, current=C, previous=B, backup=BC, before=None, apply=False,
             stage_root=None, backup_root=None, keep_stage=()):
        command = [sys.executable, "-B", str(SCRIPT), "--stage-root",
                   str(stage_root or self.stages), "--backup-root",
                   str(backup_root or self.backups), "--current", current,
                   "--previous", previous]
        if backup is not None:
            command += ["--backup", backup]
        if before is not None:
            command += ["--before-epoch", str(before)]
        for stage in keep_stage:
            command += ["--keep-stage", stage]
        if apply:
            command += ["--apply"]
        result = subprocess.run(command, capture_output=True, text=True, timeout=5)
        value = json.loads(result.stdout) if result.stdout.strip() else None
        return result.returncode, value

    def assert_rejected(self, **kwargs):
        code, value = self.call(apply=True, **kwargs)
        self.assertNotEqual(code, 0, "unsafe request must be rejected before deletion")
        if value is not None:
            self.assertEqual(value["deleted"], [])

    def test_dry_run_lists_oldest_first_without_removing_anything(self):
        self.fixture()
        code, value = self.call()
        self.assertEqual(code, 0)
        self.assertEqual(value["list"], [str(self.backups / AB), str(self.stages / A)])
        self.assertEqual(value["deleted"], [])
        self.assertTrue((self.backups / AB / "payload").is_file())
        self.assertTrue((self.stages / A / "payload").is_file())

    def test_normal_apply_keeps_current_previous_and_authoritative_backup(self):
        self.fixture()
        code, value = self.call(apply=True)
        self.assertEqual(code, 0)
        self.assertEqual(value["deleted"], [str(self.backups / AB), str(self.stages / A)])
        self.assertEqual(sorted(p.name for p in self.stages.iterdir()), [B, C])
        self.assertEqual([p.name for p in self.backups.iterdir()], [BC])

    def test_new_deployment_evicts_old_pair_and_old_backup(self):
        self.fixture()
        self.artifact(self.stages, D, 40)
        self.artifact(self.backups, CD, 35)
        code, value = self.call(current=D, previous=C, backup=CD, apply=True)
        self.assertEqual(code, 0)
        self.assertEqual(value["deleted"], [str(self.backups / AB), str(self.stages / A),
                                           str(self.backups / BC), str(self.stages / B)])
        self.assertEqual(sorted(p.name for p in self.stages.iterdir()), [C, D])
        self.assertEqual([p.name for p in self.backups.iterdir()], [CD])

    def test_newer_unverified_backup_does_not_replace_known_recovery(self):
        self.fixture()
        self.artifact(self.backups, CD, 100)
        code, value = self.call(apply=True)
        self.assertEqual(code, 0)
        self.assertEqual(value["deleted"], [str(self.backups / AB), str(self.stages / A),
                                           str(self.backups / CD)])
        self.assertEqual([p.name for p in self.backups.iterdir()], [BC])

    def test_new_published_candidate_evicts_unused_stage_but_keeps_running_pair(self):
        self.fixture()
        self.artifact(self.stages, D, 40)
        self.artifact(self.stages, E, 50)
        code, value = self.call(keep_stage=[E], apply=True)
        self.assertEqual(code, 0)
        self.assertEqual(value["deleted"], [str(self.backups / AB), str(self.stages / A),
                                           str(self.stages / D)])
        self.assertEqual(sorted(p.name for p in self.stages.iterdir()), [B, C, E])
        self.assertEqual([p.name for p in self.backups.iterdir()], [BC])

    def test_explicit_published_stage_is_protected_even_before_age_cutoff(self):
        self.fixture()
        self.artifact(self.stages, D, 1)
        code, value = self.call(keep_stage=[D], before=50, apply=True)
        self.assertEqual(code, 0)
        self.assertEqual(value["deleted"], [str(self.backups / AB), str(self.stages / A)])
        self.assertTrue((self.stages / D).is_dir())

    def test_missing_or_invalid_explicit_keep_stage_is_rejected(self):
        self.fixture()
        for keep in ([D], ["../cccccccc"]):
            with self.subTest(keep=keep):
                self.assert_rejected(keep_stage=keep)
        self.assertTrue((self.stages / A).is_dir())

    def test_age_cutoff_keeps_boundary_newer_and_two_backup_protections(self):
        self.fixture()
        self.artifact(self.stages, D, 50)
        self.artifact(self.stages, E, 60)
        self.artifact(self.backups, CD, 45)
        code, value = self.call(before=50, apply=True)
        self.assertEqual(code, 0)
        self.assertEqual(value["deleted"], [str(self.backups / AB), str(self.stages / A)])
        self.assertEqual(sorted(p.name for p in self.stages.iterdir()), [B, C, D, E])
        self.assertEqual(sorted(p.name for p in self.backups.iterdir()), [BC, CD])

    def test_age_without_known_backup_keeps_newest_recovery(self):
        self.fixture()
        code, value = self.call(before=50, backup=None, apply=True)
        self.assertEqual(code, 0)
        self.assertEqual(value["deleted"], [str(self.backups / AB), str(self.stages / A)])
        self.assertTrue((self.backups / BC).is_dir())

    def test_age_without_present_stage_guards_keeps_newest_stage(self):
        self.artifact(self.stages, A, 10)
        self.artifact(self.stages, B, 20)
        code, value = self.call(current=C, previous=D, backup=None, before=50, apply=True)
        self.assertEqual(code, 0)
        self.assertEqual(value["deleted"], [str(self.stages / A)])
        self.assertTrue((self.stages / B).is_dir())

    def test_singleton_stage_and_rollback_survive_age_cleanup(self):
        self.artifact(self.stages, C, 1)
        self.artifact(self.backups, BC, 1)
        code, value = self.call(before=50, apply=True)
        self.assertEqual(code, 0)
        self.assertEqual(value["list"], [])
        self.assertEqual(value["deleted"], [])
        self.assertTrue((self.stages / C).is_dir())
        self.assertTrue((self.backups / BC).is_dir())

    def test_first_install_with_empty_backups_and_missing_previous_is_valid(self):
        self.artifact(self.stages, C, 1)
        code, value = self.call(backup=None, apply=True)
        self.assertEqual(code, 0)
        self.assertEqual(value["list"], [])
        self.assertTrue((self.stages / C).is_dir())

    def test_missing_current_in_normal_mode_refuses_every_deletion(self):
        self.fixture()
        self.assert_rejected(current=D)
        self.assertTrue((self.stages / A / "payload").is_file())

    def test_missing_named_backup_refuses_every_deletion(self):
        self.fixture()
        self.assert_rejected(backup=CD)
        self.assertTrue((self.stages / A / "payload").is_file())

    def test_missing_backup_argument_with_rollback_inventory_is_rejected(self):
        self.fixture()
        self.assert_rejected(backup=None)
        self.assertTrue((self.backups / AB).is_dir())

    def test_unknown_directory_names_are_untouched(self):
        self.fixture()
        unknown_stages = ["DEADBEEF", "1234567", "f" * 41, "notes", "abc12345.extra"]
        unknown_backups = ["rollback-short-before-cccccccc", BC + "-mystery", "notes"]
        for name in unknown_stages:
            self.artifact(self.stages, name, 1)
        for name in unknown_backups:
            self.artifact(self.backups, name, 1)
        code, value = self.call(apply=True)
        self.assertEqual(code, 0)
        self.assertEqual(value["deleted"], [str(self.backups / AB), str(self.stages / A)])
        for name in unknown_stages:
            self.assertTrue((self.stages / name / "payload").is_file())
        for name in unknown_backups:
            self.assertTrue((self.backups / name / "payload").is_file())

    def test_legacy_backup_suffixes_and_full_hex_stages_are_allowlisted(self):
        self.fixture()
        legacy1 = AB + "-initial-stale-build"
        legacy2 = AB + "-attempt2"
        full_stage = "1" * 40
        self.artifact(self.backups, legacy1, 1)
        self.artifact(self.backups, legacy2, 2)
        self.artifact(self.stages, full_stage, 3)
        code, value = self.call(apply=True)
        self.assertEqual(code, 0)
        self.assertEqual(value["deleted"], [str(self.backups / legacy1), str(self.backups / legacy2),
                                           str(self.stages / full_stage), str(self.backups / AB),
                                           str(self.stages / A)])

    def test_argument_path_traversal_and_invalid_hex_are_rejected(self):
        self.fixture()
        for arguments in ({"current": "../cccccccc"}, {"previous": "DEADBEEF"},
                          {"backup": "../" + BC}, {"backup": BC + "/payload"},
                          {"backup": BC + "-unknown"}, {"before": -1}):
            with self.subTest(arguments=arguments):
                self.assert_rejected(**arguments)
        self.assertTrue((self.stages / A).is_dir())

    def test_symlink_root_is_rejected(self):
        self.fixture()
        linked = self.root / "linked-stages"
        linked.symlink_to(self.stages, target_is_directory=True)
        self.assert_rejected(stage_root=linked)
        self.assertTrue((self.stages / A).is_dir())

    def test_symlink_ancestor_is_rejected(self):
        self.fixture()
        linked = self.root / "linked-parent"
        linked.symlink_to(self.root, target_is_directory=True)
        self.assert_rejected(stage_root=linked / "stages")
        self.assertTrue((self.stages / A).is_dir())

    def test_direct_child_symlink_cannot_escape_inventory(self):
        self.fixture()
        outside = self.artifact(self.root, "outside", 1)
        (self.stages / D).symlink_to(outside, target_is_directory=True)
        self.assert_rejected()
        self.assertTrue((outside / "payload").is_file())
        self.assertTrue((self.stages / A).is_dir())

    def test_nested_payload_symlinks_are_unlinked_without_following(self):
        self.fixture()
        outside = self.artifact(self.root, "outside", 1)
        (self.stages / A / "external").symlink_to(outside, target_is_directory=True)
        os.utime(self.stages / A, (10, 10))
        code, value = self.call(apply=True)
        self.assertEqual(code, 0)
        self.assertIn(str(self.stages / A), value["deleted"])
        self.assertFalse((self.stages / A).exists())
        self.assertTrue((outside / "payload").is_file())

    def test_closed_unix_socket_in_old_stage_is_unlinked_as_leaf(self):
        self.fixture()
        self.closed_socket(self.stages / A / "ipc")
        self.closed_socket(self.root / "outside-ipc")
        outside = os.stat(self.root / "outside-ipc")
        os.utime(self.stages / A, (10, 10))
        code, value = self.call(apply=True)
        self.assertEqual(code, 0)
        self.assertIn(str(self.stages / A), value["deleted"])
        self.assertFalse((self.stages / A).exists())
        self.assertEqual((self.root / "outside-ipc").stat().st_ino, outside.st_ino)
        self.assertTrue((self.stages / C / "payload").is_file())

    def test_closed_unix_socket_in_protected_backup_does_not_block_pruning(self):
        self.fixture()
        self.closed_socket(self.backups / BC / "ipc")
        protected = os.stat(self.backups / BC / "ipc")
        os.utime(self.backups / BC, (15, 15))
        code, value = self.call(apply=True)
        self.assertEqual(code, 0)
        self.assertEqual(value["deleted"], [str(self.backups / AB), str(self.stages / A)])
        self.assertEqual((self.backups / BC / "ipc").stat().st_ino, protected.st_ino)

    def test_concurrent_deploy_marker_aborts_before_any_deletion(self):
        self.fixture()
        (self.stages / ".deploy-cccccccc-123").mkdir()
        self.assert_rejected()
        self.assertTrue((self.backups / AB).is_dir())
        self.assertTrue((self.stages / A).is_dir())

    def test_candidate_created_by_deploy_marker_aborts_entire_plan(self):
        self.fixture()
        (self.stages / A / ".created-by-deploy").write_text("inflight upload")
        os.utime(self.stages / A, (10, 10))
        self.assert_rejected()
        self.assertTrue((self.backups / AB).is_dir())
        self.assertTrue((self.stages / A / "payload").is_file())

    def test_candidate_direct_deploy_child_aborts_entire_plan(self):
        self.fixture()
        (self.stages / A / ".deploy.upload").mkdir()
        os.utime(self.stages / A, (10, 10))
        self.assert_rejected()
        self.assertTrue((self.backups / AB).is_dir())
        self.assertTrue((self.stages / A / "payload").is_file())

    def test_candidate_replacement_after_plan_is_rejected(self):
        self.fixture()
        plan = retention.plan_retention(self.stages, self.backups, C, B, BC)
        (self.stages / A).rename(self.stages / "saved-old")
        replacement = self.artifact(self.stages, A, 10)
        with self.assertRaises(retention.RetentionError):
            retention.apply_plan(plan)
        self.assertTrue((replacement / "payload").is_file())
        self.assertTrue((self.backups / AB).is_dir())

    def test_candidate_content_change_after_plan_is_rejected(self):
        self.fixture()
        plan = retention.plan_retention(self.stages, self.backups, C, B, BC)
        (self.stages / A / "payload").write_text("changed after inventory snapshot")
        with self.assertRaises(retention.RetentionError):
            retention.apply_plan(plan)
        self.assertTrue((self.stages / A).is_dir())
        self.assertTrue((self.backups / AB).is_dir())

    def test_protected_backup_replacement_after_plan_is_rejected(self):
        self.fixture()
        plan = retention.plan_retention(self.stages, self.backups, C, B, BC)
        (self.backups / BC).rename(self.backups / "saved-protected")
        self.artifact(self.backups, BC, 15)
        with self.assertRaises(retention.RetentionError):
            retention.apply_plan(plan)
        self.assertTrue((self.backups / AB).is_dir())

    def test_root_rename_and_replacement_after_plan_is_rejected(self):
        self.fixture()
        plan = retention.plan_retention(self.stages, self.backups, C, B, BC)
        self.stages.rename(self.root / "saved-stages")
        self.stages.mkdir()
        replacement = self.artifact(self.stages, A, 10)
        with self.assertRaises(retention.RetentionError):
            retention.apply_plan(plan)
        self.assertTrue((replacement / "payload").is_file())
        self.assertTrue((self.backups / AB).is_dir())

    @unittest.skipUnless(sys.platform.startswith("linux") and pathlib.Path("/proc").is_dir(),
                         "Linux /proc guard")
    def test_live_process_cwd_in_candidate_refuses_deletion(self):
        self.fixture()
        process = subprocess.Popen([sys.executable, "-B", "-c",
                                    'import time; print("ready", flush=True); time.sleep(30)'],
                                   cwd=self.stages / A, stdout=subprocess.PIPE, text=True)
        try:
            self.assertEqual(process.stdout.readline().strip(), "ready")
            self.assert_rejected()
            self.assertTrue((self.stages / A / "payload").is_file())
            self.assertTrue((self.backups / AB).is_dir())
        finally:
            process.terminate()
            process.wait(timeout=3)
            process.stdout.close()

    @unittest.skipUnless(sys.platform.startswith("linux") and pathlib.Path("/proc").is_dir(),
                         "Linux /proc guard")
    def test_live_executable_in_candidate_refuses_deletion(self):
        self.fixture()
        executable = self.stages / A / "sleep"
        shutil.copy2("/bin/sleep", executable)
        os.utime(self.stages / A, (10, 10))
        process = subprocess.Popen([str(executable), "30"])
        try:
            self.assert_rejected()
            self.assertTrue(executable.is_file())
            self.assertTrue((self.backups / AB).is_dir())
        finally:
            process.terminate()
            process.wait(timeout=3)

    @unittest.skipUnless(sys.platform.startswith("linux") and pathlib.Path("/proc").is_dir(),
                         "Linux /proc guard")
    def test_live_open_payload_fd_with_outside_cwd_refuses_deletion(self):
        self.fixture()
        process = subprocess.Popen([sys.executable, "-B", "-c",
                                    'import sys,time; held=open(sys.argv[1],"rb"); '
                                    'print("ready",flush=True); time.sleep(30)',
                                    str(self.stages / A / "payload")],
                                   cwd=self.root, stdout=subprocess.PIPE, text=True)
        try:
            self.assertEqual(process.stdout.readline().strip(), "ready")
            self.assert_rejected()
            self.assertTrue((self.stages / A / "payload").is_file())
            self.assertTrue((self.backups / AB).is_dir())
        finally:
            process.terminate()
            process.wait(timeout=3)
            process.stdout.close()

    @unittest.skipUnless(sys.platform.startswith("linux") and pathlib.Path("/proc").is_dir(),
                         "Linux /proc guard")
    def test_live_absolute_unix_socket_with_outside_cwd_refuses_deletion(self):
        self.fixture()
        process = subprocess.Popen([sys.executable, "-B", "-c",
                                    'import socket,sys,time; '
                                    'held=socket.socket(socket.AF_UNIX,socket.SOCK_STREAM); '
                                    'held.bind(sys.argv[1]); print("ready",flush=True); '
                                    'time.sleep(30)', str(self.stages / A / "ipc")],
                                   cwd=self.root, stdout=subprocess.PIPE, text=True)
        try:
            self.assertEqual(process.stdout.readline().strip(), "ready")
            self.assert_rejected()
            self.assertTrue((self.stages / A / "payload").is_file())
            self.assertTrue((self.backups / AB).is_dir())
        finally:
            process.terminate()
            process.wait(timeout=3)
            process.stdout.close()

    @unittest.skipUnless(sys.platform.startswith("linux") and pathlib.Path("/proc").is_dir(),
                         "Linux Unix VFS identity guard")
    def test_renamed_external_live_socket_does_not_block_closed_candidate(self):
        self.fixture()
        self.closed_socket(self.stages / A / "ipc")
        process = subprocess.Popen([sys.executable, "-B", "-c",
                                    'import os,socket,sys,time; '
                                    'held=socket.socket(socket.AF_UNIX,socket.SOCK_STREAM); '
                                    'held.bind(sys.argv[1]); os.rename(sys.argv[1],sys.argv[2]); '
                                    'print("ready",flush=True); time.sleep(30)',
                                    str(self.root / "outside-original"),
                                    str(self.root / "outside-live")],
                                   cwd=self.root, stdout=subprocess.PIPE, text=True)
        try:
            self.assertEqual(process.stdout.readline().strip(), "ready")
            code, value = self.call(apply=True)
            self.assertEqual(code, 0, "renamed external IPC must not invalidate closed artifact proof")
            self.assertIn(str(self.stages / A), value["deleted"])
            self.assertTrue((self.root / "outside-live").exists())
            self.assertTrue((self.stages / C / "payload").is_file())
        finally:
            process.terminate()
            process.wait(timeout=3)
            process.stdout.close()

    @unittest.skipUnless(sys.platform.startswith("linux") and pathlib.Path("/proc").is_dir(),
                         "Linux Unix VFS identity guard")
    def test_renamed_candidate_live_socket_is_identified_before_deletion(self):
        self.fixture()
        process = subprocess.Popen([sys.executable, "-B", "-c",
                                    'import os,socket,sys,time; '
                                    'held=socket.socket(socket.AF_UNIX,socket.SOCK_STREAM); '
                                    'held.bind(sys.argv[1]); os.rename(sys.argv[1],sys.argv[2]); '
                                    'print("ready",flush=True); time.sleep(30)',
                                    str(self.root / "outside-original"),
                                    str(self.stages / A / "ipc")],
                                   cwd=self.root, stdout=subprocess.PIPE, text=True)
        try:
            self.assertEqual(process.stdout.readline().strip(), "ready")
            code, value = self.call(apply=True)
            self.assertNotEqual(code, 0)
            self.assertIn("candidate Unix socket is live", value["error"],
                          "kernel identity must identify the live socket despite rename")
            self.assertTrue((self.stages / A / "ipc").exists())
            self.assertTrue((self.backups / AB).is_dir())
        finally:
            process.terminate()
            process.wait(timeout=3)
            process.stdout.close()

    @unittest.skipUnless(sys.platform.startswith("linux") and pathlib.Path("/proc").is_dir(),
                         "Linux /proc guard")
    def test_live_file_mapping_without_open_fd_and_outside_cwd_refuses_deletion(self):
        self.fixture()
        # libc mmap leaves no Python mmap-owned duplicate descriptor, so only
        # /proc/maps can reveal this recovery payload after os.close(fd).
        code = ('import ctypes,os,sys,time; '
                'libc=ctypes.CDLL(None,use_errno=True); libc.mmap.restype=ctypes.c_void_p; '
                'libc.mmap.argtypes=[ctypes.c_void_p,ctypes.c_size_t,ctypes.c_int,'
                'ctypes.c_int,ctypes.c_int,ctypes.c_long]; '
                'fd=os.open(sys.argv[1],os.O_RDONLY); '
                'address=libc.mmap(None,os.fstat(fd).st_size,1,2,fd,0); os.close(fd); '
                'assert address not in (None,ctypes.c_void_p(-1).value); '
                'print("ready",flush=True); time.sleep(30)')
        process = subprocess.Popen([sys.executable, "-B", "-c", code,
                                    str(self.stages / A / "payload")],
                                   cwd=self.root, stdout=subprocess.PIPE, text=True)
        try:
            self.assertEqual(process.stdout.readline().strip(), "ready")
            candidate = os.stat(self.stages / A / "payload")
            for descriptor in pathlib.Path("/proc", str(process.pid), "fd").iterdir():
                try:
                    value = descriptor.stat()
                except FileNotFoundError:
                    continue
                self.assertNotEqual((value.st_dev, value.st_ino),
                                    (candidate.st_dev, candidate.st_ino),
                                    "mapping fixture must have closed every payload FD")
            self.assert_rejected()
            self.assertTrue((self.stages / A / "payload").is_file())
            self.assertTrue((self.backups / AB).is_dir())
        finally:
            process.terminate()
            process.wait(timeout=3)
            process.stdout.close()


if __name__ == "__main__":
    unittest.main()
