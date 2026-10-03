#!/usr/bin/env python3
"""Dry-run by default; prune only allowlisted top-level Core artifacts.

The caller MUST hold the common deployment flock throughout planning/applying.
It validates that --backup is a successful recovery; this helper never infers
success from recency. Age cleanup additionally preserves the newest rollback.
Linux socket leaves are checked against readonly UNIX_DIAG_VFS kernel identities,
so renamed endpoints and abstract addresses need no pathname interpretation.
Unreadable proof or a different live network namespace fails closed.
Directory descriptors and O_NOFOLLOW prevent traversal through directory links.
No service, radio, network, or global-lock actions are performed here.
"""
import argparse
from dataclasses import dataclass
import json
import os
import pathlib
import re
import socket
import stat
import struct
import sys
import time

HEX = re.compile(r"[0-9a-f]{8,40}\Z")
ROLLBACK = re.compile(r"rollback-[0-9a-f]{8,40}-before-[0-9a-f]{8,40}"
                      r"(?:-initial-stale-build|-attempt2)?\Z")
DIRECTORY_FLAGS = os.O_RDONLY | os.O_DIRECTORY | os.O_NOFOLLOW
MAX_NODES = 100000
MAX_DEPTH = 128


class RetentionError(RuntimeError):
    def __init__(self, message, deleted=(), partial=None):
        super().__init__(message)
        self.deleted = list(deleted)
        self.partial = partial


def fingerprint(value):
    # atime changes from inspection; ctime catches mutation even if a writer
    # restores size/mtime. A metadata snapshot does not assert file contents.
    return (value.st_dev, value.st_ino, value.st_mode, value.st_size,
            value.st_mtime_ns, value.st_ctime_ns)


def identity(value):
    return (value.st_dev, value.st_ino, value.st_mode)


def open_root(path):
    path = pathlib.Path(path)
    if not path.is_absolute() or ".." in path.parts:
        raise RetentionError("roots must be absolute paths without traversal")
    descriptor = os.open("/", DIRECTORY_FLAGS)
    try:
        for component in path.parts[1:]:
            next_descriptor = os.open(component, DIRECTORY_FLAGS, dir_fd=descriptor)
            os.close(descriptor)
            descriptor = next_descriptor
        return descriptor
    except OSError as error:
        os.close(descriptor)
        raise RetentionError("root must be an existing real nonsymlink directory: " + str(path)) from error


def linux_mounts():
    if not sys.platform.startswith("linux"):
        return set()
    try:
        rows = pathlib.Path("/proc/self/mountinfo").read_text().splitlines()
    except OSError as error:
        raise RetentionError("cannot inspect Linux mount topology") from error
    return {re.sub(r"\\([0-7]{3})", lambda match: chr(int(match[1], 8)), row.split()[4])
            for row in rows}


def check_mount(path, root_device, value, mounts):
    if value.st_dev != root_device or str(path) in mounts or os.path.ismount(path):
        raise RetentionError("artifact contains a mount boundary: " + str(path))


def snapshot_tree(descriptor, path, root_device, mounts, budget, depth=0):
    if depth > MAX_DEPTH:
        raise RetentionError("artifact exceeds bounded directory depth")
    before = os.fstat(descriptor)
    check_mount(path, root_device, before, mounts)
    result = {"": fingerprint(before)}
    for name in sorted(os.listdir(descriptor)):
        budget[0] -= 1
        if budget[0] < 0:
            raise RetentionError("inventory exceeds bounded node count")
        value = os.stat(name, dir_fd=descriptor, follow_symlinks=False)
        item = fingerprint(value)
        if stat.S_ISDIR(value.st_mode):
            child = os.open(name, DIRECTORY_FLAGS, dir_fd=descriptor)
            try:
                if fingerprint(os.fstat(child)) != item:
                    raise RetentionError("directory changed while taking snapshot")
                nested = snapshot_tree(child, path / name, root_device, mounts, budget, depth + 1)
            finally:
                os.close(child)
            for relative, proof in nested.items():
                result[name + ("/" + relative if relative else "")] = proof
        elif stat.S_ISLNK(value.st_mode):
            result[name] = item + (os.readlink(name, dir_fd=descriptor),)
        elif stat.S_ISREG(value.st_mode) or stat.S_ISSOCK(value.st_mode):
            # Cloned configurations can contain a closed Unix socket pathname.
            # Snapshot it as a leaf; removal unlinks it without opening it.
            result[name] = item
        else:
            raise RetentionError("artifact contains a special file: " + str(path / name))
        if fingerprint(os.stat(name, dir_fd=descriptor, follow_symlinks=False)) != item:
            raise RetentionError("entry changed while taking snapshot")
    if fingerprint(os.fstat(descriptor)) != fingerprint(before):
        raise RetentionError("directory changed while taking snapshot")
    return result


def inventory(descriptor, root, kind, mounts, budget):
    entries, trees = {}, {}
    root_device = os.fstat(descriptor).st_dev
    for name in sorted(os.listdir(descriptor)):
        budget[0] -= 1
        if budget[0] < 0:
            raise RetentionError("inventory exceeds bounded node count")
        if name.startswith(".deploy"):
            raise RetentionError("concurrent deployment marker: " + str(root / name))
        value = os.stat(name, dir_fd=descriptor, follow_symlinks=False)
        if stat.S_ISLNK(value.st_mode):
            raise RetentionError("top-level child is a symlink: " + str(root / name))
        entries[name] = fingerprint(value)
        allowed = HEX.fullmatch(name) if kind == "stage" else ROLLBACK.fullmatch(name)
        if allowed and stat.S_ISDIR(value.st_mode):
            child = os.open(name, DIRECTORY_FLAGS, dir_fd=descriptor)
            try:
                if fingerprint(os.fstat(child)) != fingerprint(value):
                    raise RetentionError("artifact replaced during inventory")
                trees[name] = snapshot_tree(child, root / name, root_device, mounts, budget)
            finally:
                os.close(child)
    return entries, trees


@dataclass
class Plan:
    roots: dict
    root_identities: dict
    entries: dict
    trees: dict
    candidates: list
    protected: set

    def path(self, key):
        kind, name = key
        return str(self.roots[kind] / name)

    def report(self, deleted=()):
        return {"list": [self.path(key) for key in self.candidates],
                "deleted": list(deleted),
                "protected": sorted(self.path(key) for key in self.protected)}


def check_inflight(plan, deleted_keys=()):
    for kind, name in plan.candidates:
        if (kind, name) in deleted_keys:
            continue
        direct_children = {relative for relative in plan.trees[kind][name]
                           if relative and "/" not in relative}
        if ".created-by-deploy" in direct_children or any(
                child.startswith(".deploy") for child in direct_children):
            raise RetentionError("candidate has an inflight deployment marker: " + plan.path((kind, name)))


def plan_retention(stage_root, backup_root, current, previous, backup=None, before_epoch=None,
                   keep_stage=()):
    for name in (current, previous, *keep_stage):
        if not isinstance(name, str) or HEX.fullmatch(name) is None:
            raise RetentionError("stage arguments must be lowercase 8..40 digit hex basenames")
    if backup is not None and ROLLBACK.fullmatch(backup) is None:
        raise RetentionError("backup must be an allowlisted rollback basename")
    if before_epoch is not None and (not isinstance(before_epoch, int) or before_epoch < 0):
        raise RetentionError("before epoch must be a nonnegative integer")
    roots = {"stage": pathlib.Path(stage_root), "backup": pathlib.Path(backup_root)}
    stage_path, backup_path = roots.values()
    if stage_path in backup_path.parents or backup_path in stage_path.parents:
        raise RetentionError("stage and backup roots must not be nested")
    entries, trees, root_identities = {}, {}, {}
    mounts, budget = linux_mounts(), [MAX_NODES]
    for kind, root in roots.items():
        descriptor = open_root(root)
        try:
            root_identities[kind] = identity(os.fstat(descriptor))
            entries[kind], trees[kind] = inventory(descriptor, root, kind, mounts, budget)
        finally:
            os.close(descriptor)
    protected = {("stage", name) for name in (current, previous) if name in trees["stage"]}
    if before_epoch is None and current not in trees["stage"]:
        raise RetentionError("protected current stage is missing")
    for name in keep_stage:
        if name not in trees["stage"]:
            raise RetentionError("explicit keep stage is missing: " + name)
        protected.add(("stage", name))
    if backup is not None:
        if backup not in trees["backup"]:
            raise RetentionError("protected backup is missing")
        protected.add(("backup", backup))
    elif before_epoch is None and trees["backup"]:
        raise RetentionError("normal rollback pruning requires an explicit successful backup")
    if before_epoch is not None:
        if trees["backup"]:
            newest = max(trees["backup"], key=lambda name: (entries["backup"][name][4], name))
            protected.add(("backup", newest))
        if trees["stage"] and not any(kind == "stage" for kind, name in protected):
            newest = max(trees["stage"], key=lambda name: (entries["stage"][name][4], name))
            protected.add(("stage", newest))
    candidates = [(kind, name) for kind in roots for name in trees[kind]
                  if (kind, name) not in protected
                  and (before_epoch is None or entries[kind][name][4] < before_epoch * 1000000000)]
    candidates.sort(key=lambda key: (entries[key[0]][key[1]][4], str(roots[key[0]] / key[1])))
    plan = Plan(roots, root_identities, entries, trees, candidates, protected)
    check_inflight(plan)
    return plan


def live_unix_vfs_identities():
    # Linux UAPI: unix_diag.h (req24/msg16/VFS attr1, inode/dev U32),
    # sock_diag.h (SOCK_DIAG_BY_FAMILY20), netlink.h (REQUEST|DUMP0x301).
    # This AF_NETLINK query reads kernel state; it sends no IP/radio traffic.
    sequence = 1
    request = struct.pack("=BBHIIIII", socket.AF_UNIX, 0, 0, 0xffffffff,
                          0, 2, 0xffffffff, 0xffffffff)
    live, received = set(), 0
    deadline = time.monotonic() + 2
    try:
        with socket.socket(socket.AF_NETLINK, socket.SOCK_RAW, 4) as diagnostic:
            diagnostic.bind((0, 0))
            header = struct.pack("=IHHII", 16 + len(request), 20, 0x301,
                                 sequence, diagnostic.getsockname()[0])
            diagnostic.sendto(header + request, (0, 0))
            while True:
                remaining = deadline - time.monotonic()
                if remaining <= 0:
                    raise RetentionError("Unix kernel identity query exceeded two seconds")
                diagnostic.settimeout(remaining)
                packet, _, flags, sender = diagnostic.recvmsg(1024 * 1024)
                received += len(packet)
                if sender[0] != 0 or flags & socket.MSG_TRUNC or received > 8 * 1024 * 1024:
                    raise RetentionError("incomplete or excessive Unix kernel identity dump")
                position = 0
                while position < len(packet):
                    if len(packet) - position < 16:
                        raise RetentionError("truncated Unix diagnostic message")
                    length, kind, message_flags, reply_sequence, _ = struct.unpack_from("=IHHII", packet, position)
                    if length < 16 or position + length > len(packet) or reply_sequence != sequence:
                        raise RetentionError("invalid Unix diagnostic message")
                    if message_flags & 0x10:  # NLM_F_DUMP_INTR: never trust a partial dump.
                        raise RetentionError("Unix kernel identity dump was interrupted")
                    body = packet[position + 16:position + length]
                    position += (length + 3) & ~3
                    if kind in (2, 3):  # NLMSG_ERROR / NLMSG_DONE
                        if kind == 2 and len(body) < 4:
                            raise RetentionError("truncated Unix diagnostic error")
                        if body:
                            if len(body) < 4:
                                raise RetentionError("truncated Unix diagnostic error")
                            kernel_error = struct.unpack_from("=i", body)[0]
                            if kernel_error != 0:
                                if kernel_error > 0:
                                    raise RetentionError("invalid Unix diagnostic errno")
                                cause = OSError(-kernel_error, os.strerror(-kernel_error))
                                raise RetentionError("Unix kernel identity query returned an error: " + str(cause)) from cause
                        if kind == 3:
                            return live
                        continue
                    if kind != 20 or len(body) < 16 or body[0] != socket.AF_UNIX:
                        raise RetentionError("unexpected Unix diagnostic response")
                    attribute = 16
                    while attribute < len(body):
                        if len(body) - attribute < 4:
                            raise RetentionError("truncated Unix diagnostic attribute")
                        attribute_length, attribute_kind = struct.unpack_from("=HH", body, attribute)
                        if attribute_length < 4 or attribute + attribute_length > len(body):
                            raise RetentionError("invalid Unix diagnostic attribute")
                        if attribute_kind & 0x3fff == 1:  # UNIX_DIAG_VFS
                            if attribute_length != 12:
                                raise RetentionError("invalid Unix filesystem identity")
                            inode, device = struct.unpack_from("=II", body, attribute + 4)
                            live.add((device, inode))
                        attribute += (attribute_length + 3) & ~3
    except OSError as error:
        raise RetentionError("cannot obtain complete Unix kernel filesystem identities") from error


def encoded_socket_identity(device, inode):
    # Linux v6.1 net/unix/diag.c exports raw dentry->d_sb->s_dev (major<<20
    # |minor), not userspace st_dev/new_encode_dev. Inode is truncated to U32.
    # A collision conservatively rejects pruning rather than ignoring an endpoint.
    major, minor = os.major(device), os.minor(device)
    encoded = (major << 20) | minor
    return encoded & 0xffffffff, inode & 0xffffffff


def assert_not_in_use(paths, objects, sockets):
    if not sys.platform.startswith("linux") or not paths:
        return
    try:
        processes = os.listdir("/proc")
    except OSError as error:
        raise RetentionError("cannot inspect Linux processes") from error
    if sockets:
        try:
            value = os.stat("/proc/self/ns/net")
            current_network = (value.st_dev, value.st_ino)
        except OSError as error:
            raise RetentionError("cannot inspect current network namespace") from error
    for process in processes:
        if not process.isdigit():
            continue
        process_root = "/proc/" + process
        if sockets:
            try:
                value = os.stat(process_root + "/ns/net")
            except FileNotFoundError:
                continue  # exited process or kernel thread without a namespace
            except OSError as error:
                if not os.path.exists(process_root):
                    continue
                raise RetentionError("cannot inspect live network namespace") from error
            if (value.st_dev, value.st_ino) != current_network:
                raise RetentionError("cannot prove Unix socket closure across network namespaces")
        for leaf in ("cwd", "exe"):
            try:
                target = os.readlink(process_root + "/" + leaf)
                value = os.stat(process_root + "/" + leaf)
            except FileNotFoundError:
                continue  # exited process or kernel thread without an exe/cwd
            except OSError as error:
                if not os.path.exists(process_root):
                    break
                raise RetentionError("cannot prove process is outside candidate artifacts") from error
            if target.endswith(" (deleted)"):
                target = target[:-10]
            if (value.st_dev, value.st_ino) in objects or any(
                    target == path or target.startswith(path + "/") for path in paths):
                raise RetentionError("artifact in use by process " + process + " " + leaf + ": " + target)
        try:
            descriptors = os.listdir(process_root + "/fd")
        except FileNotFoundError:
            continue
        except OSError as error:
            if not os.path.exists(process_root):
                continue
            raise RetentionError("cannot inspect live process file descriptors") from error
        for descriptor in descriptors:
            try:
                # Stat the proc reference, not its textual target: a bind alias
                # retains the candidate's underlying device/inode identity.
                value = os.stat(process_root + "/fd/" + descriptor)
            except FileNotFoundError:
                continue  # descriptor closed after enumeration
            except OSError as error:
                if not os.path.exists(process_root):
                    break
                raise RetentionError("cannot inspect live process file descriptor") from error
            if (value.st_dev, value.st_ino) in objects:
                raise RetentionError("artifact held open by process " + process + " fd " + descriptor)
        try:
            mappings = pathlib.Path(process_root, "maps").read_text().splitlines()
        except FileNotFoundError:
            continue
        except OSError as error:
            if not os.path.exists(process_root):
                continue
            raise RetentionError("cannot inspect live process memory mappings") from error
        for mapping in mappings:
            try:
                fields = mapping.split(None, 5)
                major, minor = fields[3].split(":")
                object_id = (os.makedev(int(major, 16), int(minor, 16)), int(fields[4]))
            except (IndexError, ValueError, OverflowError) as error:
                raise RetentionError("cannot interpret live process memory mapping") from error
            if object_id in objects:
                raise RetentionError("artifact mapped by process " + process)
    if sockets:
        candidates = {encoded_socket_identity(device, inode) for device, inode in sockets}
        if candidates & live_unix_vfs_identities():
            raise RetentionError("candidate Unix socket is live (kernel filesystem identity)")


def verify_plan(plan, deleted_keys):
    descriptors = {}
    mounts, budget = linux_mounts(), [MAX_NODES]
    try:
        for kind, root in plan.roots.items():
            descriptor = open_root(root)
            descriptors[kind] = descriptor
            if identity(os.fstat(descriptor)) != plan.root_identities[kind]:
                raise RetentionError("root replaced since planning")
            entries, trees = inventory(descriptor, root, kind, mounts, budget)
            # Pi stores disjoint hex-stage and rollback namespaces in one
            # physical directory. Each inventory sees both kinds, so a known
            # deletion must disappear from both top-level snapshots.
            removed_names = {name for deleted_kind, name in deleted_keys
                             if plan.root_identities[deleted_kind] == plan.root_identities[kind]}
            expected_entries = {name: proof for name, proof in plan.entries[kind].items()
                                if name not in removed_names}
            expected_trees = {name: proof for name, proof in plan.trees[kind].items()
                              if (kind, name) not in deleted_keys}
            if entries != expected_entries or trees != expected_trees:
                raise RetentionError("inventory or content snapshot changed since planning")
        check_inflight(plan, deleted_keys)
        remaining = [key for key in plan.candidates if key not in deleted_keys]
        objects = {(proof[0], proof[1]) for kind, name in remaining
                   for proof in plan.trees[kind][name].values()
                   if not stat.S_ISLNK(proof[2])}
        sockets = {(proof[0], proof[1]) for kind, name in remaining
                   for proof in plan.trees[kind][name].values()
                   if stat.S_ISSOCK(proof[2])}
        assert_not_in_use([plan.path(key) for key in remaining], objects, sockets)
        return descriptors
    except Exception:
        for descriptor in descriptors.values():
            os.close(descriptor)
        raise


def remove_tree(parent, name, tree, relative=""):
    descriptor = os.open(name, DIRECTORY_FLAGS, dir_fd=parent)
    try:
        expected = tree[relative]
        if fingerprint(os.fstat(descriptor)) != expected:
            raise RetentionError("directory changed immediately before removal")
        prefix = relative + "/" if relative else ""
        children = {path[len(prefix):] for path in tree if path.startswith(prefix)
                    and path != relative and "/" not in path[len(prefix):]}
        if set(os.listdir(descriptor)) != children:
            raise RetentionError("directory membership changed immediately before removal")
        for child in sorted(children):
            child_path = prefix + child
            value = os.stat(child, dir_fd=descriptor, follow_symlinks=False)
            proof = fingerprint(value)
            if stat.S_ISLNK(value.st_mode):
                proof += (os.readlink(child, dir_fd=descriptor),)
            if proof != tree[child_path]:
                raise RetentionError("entry changed immediately before removal")
            if stat.S_ISDIR(value.st_mode):
                remove_tree(descriptor, child, tree, child_path)
            else:
                os.unlink(child, dir_fd=descriptor)  # never follows leaf symlinks
        if identity(os.stat(name, dir_fd=parent, follow_symlinks=False)) != expected[:3]:
            raise RetentionError("directory name replaced during removal")
        if os.listdir(descriptor):
            raise RetentionError("directory gained content during removal")
        os.rmdir(name, dir_fd=parent)
    finally:
        os.close(descriptor)


def apply_plan(plan):
    deleted_keys, deleted = set(), []
    partial = None
    try:
        # Validate ALL candidates and protected recovery before first deletion.
        descriptors = verify_plan(plan, deleted_keys)
        for descriptor in descriptors.values():
            os.close(descriptor)
        for kind, name in plan.candidates:
            descriptors = verify_plan(plan, deleted_keys)
            try:
                partial = plan.path((kind, name))
                remove_tree(descriptors[kind], name, plan.trees[kind][name])
                deleted.append(partial)
                deleted_keys.add((kind, name))
                partial = None
            finally:
                for descriptor in descriptors.values():
                    os.close(descriptor)
        return plan.report(deleted)
    except (OSError, RetentionError) as error:
        raise RetentionError(str(error), deleted=deleted, partial=partial) from error


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--stage-root", required=True)
    parser.add_argument("--backup-root", required=True)
    parser.add_argument("--current", required=True)
    parser.add_argument("--previous", required=True)
    parser.add_argument("--backup")
    parser.add_argument("--keep-stage", action="append", default=[])
    parser.add_argument("--before-epoch", type=int)
    parser.add_argument("--apply", action="store_true")
    args = parser.parse_args()
    plan = None
    try:
        plan = plan_retention(args.stage_root, args.backup_root, args.current,
                              args.previous, args.backup, args.before_epoch, args.keep_stage)
        result = apply_plan(plan) if args.apply else plan.report()
        print(json.dumps(result, sort_keys=True))
        return 0
    except (OSError, RetentionError) as error:
        result = plan.report() if plan is not None else {"list": [], "deleted": [], "protected": []}
        result.update(error=str(error), deleted=getattr(error, "deleted", []))
        if getattr(error, "partial", None) is not None:
            result["partial"] = error.partial
        print(json.dumps(result, sort_keys=True))
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
