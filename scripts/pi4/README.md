# Pi 4 Core installation and retention

These scripts are the source-owned copies of the Pi 4 installers. Run them on
the Pi as root after publishing a verified Pi stage under
`/home/nereus/nereus-stage/<checkpoint>/`. `install-core-pi4.sh <checkpoint>`
refuses an existing Core; `upgrade-core-pi4.sh <new> <installed>` preserves the
operator configuration and makes a complete daemon-settings snapshot after
stopping the old Core. The stage must include the DFNR model/licences and both
RNNoise models. Checkpoints are eight lowercase hexadecimal characters.

Both installers require `python3` and `flock` and hold
`/run/lock/nereus-core-deploy.lock` before mutating deployment files. All other
publishers and maintenance callers must hold that same lock. Publish uploads
atomically; while preparing a stage, use a top-level `.deploy…` marker or an
artifact-local `.created-by-deploy` marker. Never publish new files into an
existing stage during pruning. A changed inventory stops deletion.

A healthy upgrade retains these groups:

- The current and previous upload directories in `/home/nereus/nereus-stage`.
- The current and previous verified archive directories in
  `/var/lib/nereus-build/pi4`.
- The successful upgrade's recovery directory
  `rollback-<previous>-before-<current>`, including `files.tar`, any saved
  `assets.tar`, and the complete post-stop `daemon-config` snapshot.

A first install retains its current stage and requires no rollback directory.
Pruning removes obsolete allowlisted groups oldest first. It never deletes the
current stage, previous stage, or named recovery. Unknown names are retained;
symlinks, inflight markers, content changes, mount boundaries, open files, live
process use, and live Unix sockets make cleanup fail closed. Closed socket
leaves require the Linux `CONFIG_UNIX_DIAG` interface to prove closure. If that
interface is unavailable, any candidate containing a socket is rejected before
deletion; ordinary artifacts containing no sockets can still be pruned. No
pathname or connection-attempt substitute is used. Positive diagnostic tests
skip only when the kernel explicitly reports an unsupported capability;
permission, malformed-response and other errors remain test failures. Failed installs never run pruning and retain
inflight evidence for deliberate operator review.

After Core passes its health checks, the upgrade rollback trap is cleared and
the unpacked payload is removed. Installed daemon/Core/RADE hashes must match
the already verified stage manifest before maintenance publishes durable
`installed.sha256`, `<current>/successful-install.json`, and
`current-install.json`. Receipts identify current/previous stages, complete
post-stop recovery, and archive hashes. Receipt or cleanup errors emit a warning
and preserve the healthy Core; they never invoke rollback. The verified store
is pruned first, then uploads, under the same deployment lock.

The stage's manifest-verified
`usr/local/libexec/nereusd/prune-core-artifacts.py` is installed to
`/usr/local/libexec/nereusd/prune-core-artifacts.py` after healthy startup. For
older payloads, an already installed helper or a sibling helper is accepted.
`NEREUS_RETENTION_HELPER` selects an explicit helper for controlled fixtures or
maintenance. No dependency or helper is downloaded automatically. The common
Core binary's log rotation bounds daemon log files; these scripts bound
successful deployment archives and saved startup evidence by stage retention.

Local checks (temporary files only, no Pi/service/network access):

```sh
python3 -B -m unittest discover -s scripts/pi4 -v
python3 -B -m unittest discover -s scripts/radxa -p test_prune_core_artifacts.py -v
bash -n scripts/pi4/install-core-pi4.sh scripts/pi4/upgrade-core-pi4.sh
```
