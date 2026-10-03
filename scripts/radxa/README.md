# Radxa deployment retention

These are the maintained deployment helpers. Copy `install-core-selector.sh`
and `prune-core-artifacts.py` into `/var/lib/nereus-build/` on Radxa. Copy
`deploy-stage-to-rock.sh` into the local Rock Docker kit, beside its existing
`out/` directory. The stage script checks and publishes artifacts; it never
installs them or restarts the Core.

All three deployment operations use `/run/lock/nereus-core-deploy.lock`.
The pruning helper expects its caller to hold this lock.

After a healthy installation, the installer publishes a durable receipt in
the new rollback directory and `/var/lib/nereus-build/current-install.json`.
It verifies the installed binaries against the new stage before pruning.
It retains:

- The running stage and its previous stage.
- The complete rollback for that successful installation.

Publishing a verified new candidate also retains that candidate, replacing
older unused candidates. Uploads, builds, incomplete recovery, symlinks, and
unknown directory names are never silently removed. Failed installation does
not invoke pruning. Maintenance failure after successful installation warns
and preserves the healthy running Core; it cannot trigger rollback.

One-time age cleanup can preserve newer artifacts while removing only older
ones. Preview first, then use the same command with `--apply`, while holding
the shared deployment lock. Example for October 1, 2026 at midnight Chicago:

```sh
flock /run/lock/nereus-core-deploy.lock python3 \
  /var/lib/nereus-build/prune-core-artifacts.py \
  --stage-root /home/yonder/nereus-core --backup-root /var/lib/nereus-build \
  --current bc8b65dc --previous 6b340562 \
  --backup rollback-6b340562-before-bc8b65dc \
  --before-epoch 1790830800
```

The age test is strictly earlier than the cutoff. Protection takes precedence
over age. The helper defaults to a JSON preview and only deletes with
`--apply`.

Closed Unix sockets copied into configuration snapshots can be removed.
On Linux the helper checks kernel socket filesystem identities, open files,
and mapped libraries before deleting candidates. It fails safely if it cannot
inspect all live processes or their network namespaces.

Run its temporary-filesystem tests without a radio or device:

```sh
python3 scripts/radxa/test_prune_core_artifacts.py
```

The daemon stage now carries the shared maintenance helper, which the installer
publishes automatically. Native Core logging also rotates while the Core runs:
five files per profile, each at most 32 MiB, with the newest diagnostics retained.
The background log writer performs rotation; logging producers never do disk I/O.
Existing oversized closed logs are reduced to recent text at startup. External
`copytruncate` is neither required nor configured.
