# Remote-daemon R2 open issues (drafted, not yet filed)

Drafted by Task 5 ("Neutralise the local authorities") per its step 6, which
covers design addendum risk 9
(`docs/architecture/2026-08-03-remote-daemon-r2-r3-design-addendum.md`
section 11, item 9): "Deferring record streams leaves two daemon defects
live: `nereusd` builds seven spot collectors and starts none, and
`FreeDVStationModel` grows without bound headless. File both as issues now."

**Status: NOT YET FILED.** These are drafts for the maintainer (J.J. Boyd /
KG4VCF) to review and file on GitHub at their discretion. Nothing in this
file has been posted anywhere. Task 5 also gates
`RadioModel::restoreSpotClientAutoStartState()` on `Role::Local` (step 5),
which is the client-side half of issue 1's ownership question; see that
gate's comment in `src/models/RadioModel.cpp` and step 5a's paragraph in
`docs/architecture/2026-08-03-remote-daemon-r2-verification/README.md` for
the cross-reference.

---

## Issue draft 1: `nereusd` builds seven spot collectors and starts none

**Defect.** `RadioModel`'s constructor unconditionally constructs all seven
spot-ingest clients regardless of role: `DxClusterClient` (the DX-cluster
instance), a second `DxClusterClient` instance for RBN, `WsjtxClient`,
`SpotCollectorClient`, `PotaClient`, `FreeDVReporterClient` and
`PskReporterClient`. The only thing that ever starts any of them is
`RadioModel::restoreSpotClientAutoStartState()`, and that method has exactly
one production caller: `MainWindow.cpp:771`. `nereusd` (`src/core/daemon/`)
never calls it. A daemon can therefore never surface DX cluster spots, RBN
spots, WSJT-X decodes, SpotCollector spots, POTA activations, FreeDV
Reporter station data or PSK Reporter uploads, even when an operator has
enabled auto-start for every one of them in `NereusSDR.settings` on the
machine running `nereusd`.

**Evidence.**
- `RadioModel::restoreSpotClientAutoStartState()` is declared at
  `src/models/RadioModel.h:1118` and defined at
  `src/models/RadioModel.cpp:2358` (post Task 5; the two-line role gate
  Task 5 adds sits at the top of that definition).
- `grep -rn "restoreSpotClientAutoStartState" src/` (this session's tree)
  shows exactly one call site outside the declaration/definition/tests:
  `src/gui/MainWindow.cpp:771`.
- `src/core/daemon/DaemonApp.cpp` and `src/core/daemon/DaemonConfig.{h,cpp}`
  contain no reference to `restoreSpotClientAutoStartState`, any of the
  seven client accessors, or any per-source `AutoConnect` / `AutoStart`
  AppSettings key.
- The seven clients are constructed in `RadioModel`'s constructor
  unconditionally (not gated on role or on a GUI being present), so a
  `nereusd` process already owns a live, idle instance of each one; only the
  start call is missing.

**Blast radius.** Every NereusSDR feature that depends on a spot source
being reachable from wherever the *radio* physically is (RBN skimming near
the antenna, POTA/DX cluster context for the operating location, a WSJT-X
instance colocated with the radio for a remote-shack digital setup) is
unreachable in a daemon-hosted deployment today. This is functionally inert
rather than actively wrong -- nothing crashes, nothing corrupts state -- but
it silently drops a whole feature area for any R2/R3 remote-station
operator who expects spot ingest to work the way it does in local direct
mode.

**Relationship to Task 5's gate.** Task 5 gates
`restoreSpotClientAutoStartState()` on `Role::Local` specifically so that
*when* this daemon-side defect is fixed, the client and the daemon do not
both call it and both connect to the same DX cluster / both upload to PSK
Reporter under one callsign (the other half of design addendum risk 9,
section 4.1 "Fourth, the spot collectors"). Fixing this issue without also
keeping that gate in place would reopen exactly the double-connect problem
Task 5 closed.

**Not settled by this issue or by Task 5.** Which physical machine (client
or station) *should* call `restoreSpotClientAutoStartState()` for each of
the seven sources, once nereusd is allowed to call it at all, is an open
architectural question -- see the per-source ownership breakdown in step
5a's paragraph
(`docs/architecture/2026-08-03-remote-daemon-r2-verification/README.md`).
This issue is scoped to "the daemon-side call is missing", not to resolving
that question.

---

## Issue draft 2: `FreeDVStationModel` has no expiry in core

**Defect.** `FreeDVStationModel` (`src/models/FreeDVStationModel.h`), the
model backing the FreeDV Reporter live station map, holds its stations in a
bare `QHash<QString /*sid*/, FreeDVStation> m_stations` with no `QTimer`, no
last-seen expiry check, and no size cap anywhere in the class. The only code
in this tree that ever removes an idle entry is
`FreeDVReporterDialog::onIdleSweepTick()`
(`src/gui/FreeDVReporterDialog.cpp:2044`), wired to a periodic timer inside
the dialog's constructor (`FreeDVReporterDialog.cpp:1080`) and also invoked
once from the dialog's own init path (`:1985`). `FreeDVReporterDialog` is a
GUI class (`src/gui/`); a headless `nereusd` process never constructs it and
therefore never sweeps.

**Evidence.**
- `grep -n "QTimer\|expire\|Expire\|prune\|Prune\|sweep\|Sweep\|lastSeen\|LastSeen" src/models/FreeDVStationModel.h`
  (this session's tree) returns nothing: the model has no expiry mechanism
  of its own.
- `grep -rn "onIdleSweepTick"` shows its only definition
  (`FreeDVReporterDialog.cpp:2044`), its only `connect()`
  (`FreeDVReporterDialog.cpp:1080`), and one direct call from inside the
  same file (`:1985`) -- all three in a `src/gui/` class.
- Per this repository's own architectural rule (enforced by
  `tst_core_has_no_gui_includes`), nothing under `src/core/` or
  `src/models/` may include a GUI header, so `FreeDVStationModel` cannot
  reach into `FreeDVReporterDialog` even if it wanted to borrow its sweep
  logic; the sweep has to move, not be called cross-boundary.

**Blast radius.** Once a daemon has any path that feeds
`FreeDVStationModel` (today it does not -- see issue draft 1 -- but a
future task, or a maintainer fix to issue 1, could wire one), a `nereusd`
process left running for days accumulates one hash entry per FreeDV station
ever reported, forever, with no bound. This is a slow, unbounded memory
leak in a process explicitly designed to run unattended and indefinitely
(`nereusd` is a systemd-managed daemon per the remote-daemon R1 design).
It is not a defect today only because issue draft 1 means nothing feeds the
model headless yet; fixing issue 1 without also fixing this one would turn
a dormant leak into a live one.

**Not settled by this issue.** Where the sweep timer should live once
extracted (a new small core class parallel to task 12's `SliceMeterPump`
pattern, a method on `FreeDVStationModel` itself, or something else) is a
design decision for whoever picks this issue up, not decided here.
