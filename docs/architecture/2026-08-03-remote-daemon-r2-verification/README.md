# Remote-daemon R2 verification

Design: `docs/architecture/2026-07-28-remote-daemon-architecture-design.md`
Addendum: `docs/architecture/2026-08-03-remote-daemon-r2-r3-design-addendum.md`
Plan: `docs/architecture/2026-08-03-remote-daemon-r2-plan.md`

**Status: matrix populated by Task 16 (2026-08-06).** The gate's automated
half (Step 1 and Step 2, below) ran and is **GREEN**: 623/623 tests passed
with zero failures. The live bench row (Step 3, on the ANAN-G2E) is written
out as a numbered, runnable procedure with its status left **OPEN**; the
maintainer is running it directly and it is not part of what Task 16 itself
executed. See "Task 16: local direct mode regression gate" below for the
full record. The paragraph immediately following this one is Task 5's own
seeded note and is unchanged.

---

## Task 5: the spot-collector gate is a safety measure, not an ownership decision

Task 5 gates `RadioModel::restoreSpotClientAutoStartState()`
(`src/models/RadioModel.cpp`, declared `RadioModel.h:1118`) on
`Role::Local`. Today `nereusd` never calls this method at all (see
`docs/architecture/2026-08-03-remote-daemon-r2-open-issues.md` issue draft
1), so the gate has no observable effect yet. It exists now, ahead of that
defect being fixed, to stop duplicate cluster logins and duplicate PSK
Reporter uploads under one callsign once a daemon-side caller exists:
**which side owns each collector is an open question R2 does not settle.**
Read as an ownership decision, gating all seven sources to the client puts
WSJT-X on the wrong machine.

The measured starting position, derived 2026-08-03:

- `WsjtxClient` binds and **listens** on UDP 2237
  (`src/core/WsjtxClient.h:103` the socket member, `:107` the default port
  field `m_port{2237}`), so its decoder belongs wherever the operator is,
  which is the **client**.
- `PskReporterClient`, `FreeDVReporterClient` and
  `FreeDVRadeReporterBridge` **upload under the operator's callsign** a
  claim about one physical receiver on a frequency only the station knows,
  so they belong to the **station**.
- `DxClusterClient` (also the RBN instance), `PotaClient` and
  `SpotCollectorClient` are read-only downloads whose only consumer is a
  GUI panel and whose login, filters and callsign are per-operator, so they
  are **arguably client**.

R2 turns all seven off on the client (via this gate) because it ships no
spot delivery over the link, so nothing is lost by doing so. **Do not
extend this gate into a delivery path** -- that is a design decision for
whichever task eventually resolves the ownership question above, informed
by a real daemon-side caller existing (open issue 1) and by an operator
actually running both a client and a station and finding out which
placement is wrong in practice.

Cross-reference: this paragraph is the client-side half of design addendum
risk 9 (section 11, item 9); the daemon-side half is open issue draft 1 in
`docs/architecture/2026-08-03-remote-daemon-r2-open-issues.md`.

### Automated coverage

`tests/tst_remote_role_inert.cpp::remoteRestoreSpotClientAutoStartStateStaysSilent`
seeds `DxClusterAutoConnect` / `RbnAutoConnect` / `WsjtxAutoStart` True
against loopback hosts / a local UDP port (never a real remote host --
POTA's real-network `startPolling()` call is deliberately not exercised;
see that test's header comment) and asserts the Local arm still attempts
each start while the Remote arm produces silence on all three. This is
regression coverage for the gate, not a substitute for a live two-process
bench run; Task 16 owns writing and running that row.

---

## Task 16: local direct mode regression gate

This is the gate. Fifteen tasks landed before this one; three of them
(Tasks 3, 12, 13) changed local direct mode's observable behaviour on
purpose. This section's job is proving nothing else did. A red result here
stops the phase before the OpenSSL dependency lands in Task 17.

Verified at HEAD `e40a8545` (branch `claude/remote-daemon-r2`), 2026-08-06.
The gate-regex component table below was originally measured by the
controller at Task 13's own commit, `10d1d35c`; HEAD is nine commits past
that point, including Tasks 14 and 15, which is why `settings_scope` and
`settings_proxy` now match where they legitimately matched nothing before
those two tasks landed.

| Step | What | Result |
| --- | --- | --- |
| 1 | Targeted regression set (`-L models` + 11-component regex) | GREEN: 259/259, then 16/16 |
| 2 | Full suite | GREEN: 623/623 |
| 3 | Bench, local direct mode, ANAN-G2E | OPEN (not run by this task, see below) |
| 4 | Record + commit | this section |

### Why a naive run would not be trustworthy

1. **A dead regex component is invisible.** `--no-tests=error` only fires
   when the *whole* selector matches nothing, not when one component inside
   an otherwise-working selector matches nothing. The gate regex originally
   named in the plan included `radio_store`, which matches zero tests in
   this tree, so the saved-radio coverage the gate exists to provide would
   have been silently absent while the run still reported green. Fixed in
   commit `c8147477`. The corrected regex is used below, and its `ctest -N`
   selection output is reproduced verbatim so the gate's non-vacuousness is
   an artifact, not a claim.
2. **Stale binaries.** `ctest -R`/`-L` executes whatever binary already sits
   on disk; it does not rebuild anything. A prior task in this branch found
   15 of 56 relevant binaries had not been relinked since their source last
   changed, so a sweep against them would report a pass for code that was
   no longer current. Both sweeps below ran only after
   `cmake --build build --target all_tests` completed and a second,
   identical build invocation reported `ninja: no work to do` -- proof the
   whole tree, not just one named target, was fresh before any test ran.

### The three tasks that changed local direct mode on purpose

If a bench or test observation matches one of these three, it is the
intended effect of a landed task, not a regression.

**Task 3** -- commit `8900b2aa` ("feat(r2): make RadioModel::isConnected()
storage-backed"), single commit, no fix round. `isConnected()` changed from
deriving off the connection pointer (`m_connection && m_connection->
isConnected()`) to reading stored state (`m_connectionState ==
ConnectionState::Connected`), because a `Role::Remote` model that
deliberately owns no `RadioConnection` used to report disconnected forever.
That derivation change makes ordering matter inside `teardownConnection()`:
`setConnectionState(Disconnected)` is now hoisted ahead of
`teardownWorkerThreadedConnection()` (maintainer-approved local-direct-mode
ordering change, design addendum section 5), closing a window where
`m_connection` used to go null 24 lines before the model's own state caught
up. **How to recognise it, not mistake it for a regression:** the disconnect
sequence still ends in the same place and should look the same from the UI;
only the internal ordering of when `Disconnected` becomes visible relative
to teardown moved earlier. Pinned by `tst_connected_state_equivalence` and
the `tst_p2_ddc_*` pair (both gate the P2 DDC wire push on `isConnected()`).

**Task 12** -- commit `cae70eb6` ("feat(remote-daemon): R2 Task 12 -
per-slice S-meter gets a model home"), fix round `66066111`. Moved
per-slice S-meter polling out of the GUI-only `MeterPoller` into a new
core-side `SliceMeterPump`, so a headless `nereusd` has a producer for the
value at all. While doing that, it deleted a real duplicate write: before
this task, the active slice's VFO flag was updated by two independent
connects every tick (a generic one and a Slice-A-specific one), with the
Slice-A-specific one always winning by call order, and every other slice
was hardcoded to the "signal average" source regardless of what the analog
S-Meter was displaying. **How to recognise it:** the VFO flag's mini-bar now
visibly follows whichever source (Peak / Average / Max Bin) the operator
has the analog S-Meter's right-click menu set to, for every slice, not just
Slice A. Seeing the flag change which source it tracks when the analog
meter's mode changes is the intended effect (task 12 step 7), not a
divergence. The full deleted-lines inventory lives in
`.superpowers/sdd/2026-08-03-remote-daemon-r2-plan/task-12-report.md` ("GUI
lines/blocks deleted" section); this file does not repeat it. Pinned by
`tst_slice_meter_pump` and `tst_meter_poller_tx_bindings`.

**Task 13** -- commit `10d1d35c` ("feat(remote-daemon): R2 Task 13 -
AppSettings mutator funnel"), fix round `c57d8a64`. Routed every
`AppSettings` mutator and reader through canonical accessors. Zero
caller-visible behaviour change was intended anywhere except the
`radios/*` saved-radio helpers, which were rewritten as part of the funnel.
**How to recognise it:** there should be nothing to see; saved radios
round-trip across a relaunch exactly as before. If they do not, that is a
real regression, not this task's intended effect. Pinned by
`tst_connection_panel_saved_radios` and the `tst_app_settings_*` family.

### Step 1: targeted regression set

**Build**, literal target name from the brief, run after `all_tests` (see
Step 2) had already built everything once:

```
$ cmake --build build --target tests_models
[0/2] Re-checking globbed directories...
ninja: no work to do.
```

`ninja: no work to do` is the expected, correct result here: `all_tests`
had already built this subset moments earlier, so re-running the build for
the `tests_models` target by name found nothing left to do. This is the
idempotency confirmation the brief asks for, applied to this target
specifically; Step 2 below shows the same confirmation for `all_tests`
itself.

**`ctest -L models --no-tests=error`: 259/259 passed, 0 failed.**

```
100% tests passed, 0 tests failed out of 259

Label Time Summary:
core      = 267.01 sec*proc (209 tests)
gui       = 107.11 sec*proc (88 tests)
models    = 308.85 sec*proc (259 tests)

Total Test time (real) = 308.98 sec
```

**Targeted regex.** Dry-run selection first, exactly as instructed, because
this output is the artifact proving the gate is not vacuous:

```
$ ctest --test-dir build -N -R 'settings_mutator_funnel|app_settings|connection_panel_saved_radios|settings_hygiene|spot_settings|connected_state_equivalence|p2_ddc|slice_meter_pump|meter_poller|settings_scope|settings_proxy'

Test project /Users/j.j.boyd/NereusSDR/.worktrees/remote-daemon-r2/build
  Test  #66: tst_connection_panel_saved_radios
  Test #138: tst_app_settings_vax_migration
  Test #139: tst_app_settings_profile
  Test #215: tst_settings_hygiene
  Test #268: tst_meter_poller_tx_bindings
  Test #305: tst_app_settings_migration
  Test #306: tst_app_settings_arbitrary_key_persistence
  Test #309: tst_app_settings_corruption
  Test #519: tst_spot_settings_round_trip
  Test #568: tst_p2_ddc_mask_ownership
  Test #569: tst_p2_ddc_assignment_marshalling
  Test #607: tst_connected_state_equivalence
  Test #616: tst_slice_meter_pump
  Test #617: tst_settings_mutator_funnel
  Test #618: tst_settings_scope
  Test #619: tst_settings_proxy

Total Tests: 16
```

Every one of the 11 regex components contributed at least one test:

| Component | Tests matched |
| --- | --- |
| `settings_mutator_funnel` | 1 |
| `app_settings` | 5 |
| `connection_panel_saved_radios` | 1 |
| `settings_hygiene` | 1 |
| `spot_settings` | 1 |
| `connected_state_equivalence` | 1 |
| `p2_ddc` | 2 |
| `slice_meter_pump` | 1 |
| `meter_poller` | 1 |
| `settings_scope` | 1 |
| `settings_proxy` | 1 |

16 total, matching the controller's pre-dispatch count exactly, now with
`settings_scope` and `settings_proxy` both present (Tasks 14 and 15 had not
landed when that count was first taken at `10d1d35c`).

Execution: **16/16 passed, 0 failed.**

```
100% tests passed, 0 tests failed out of 16
Total Test time (real) =  19.13 sec
```

**Step 1 verdict: GREEN.** 259/259 and 16/16, 0 failures in either sweep
(the two sweeps overlap on several tests, so this is not a claim of 275
distinct tests; every test that ran in Step 1, across both sweeps, passed).

### Step 2: full suite

**Build**, the target that actually covers this whole gate in one pass:

```
$ time cmake --build build --target all_tests
[... 1551 build steps ...]
cmake --build build --target all_tests  1265.52s user 526.45s system 1069% cpu 2:47.53 total
```

Exit code 0. Grepped the full build log for "error"/"FAILED": 2 matches,
both test binary names containing the word ("`tst_tci_silent_error_
invariant`"), not real errors.

Idempotency confirmation, the artifact proving the whole tree (not just one
target) was fresh before any sweep ran:

```
$ cmake --build build --target all_tests
[0/2] Re-checking globbed directories...
ninja: no work to do.
```

A note on the 2:47 build time, per this project's own timing-measurement
caution: this is far below fast-test-loop.md's "~32 min cold" estimate
because the tree was not cold. 623 test binaries already existed from
earlier tasks' work and ccache (33 GB local cache) had a substantial hit
rate banked already. Load average went from 4.88/4.23/3.98 (before) to
58.69/30.70/15.06 (immediately after) on an 18-core host, i.e. very high
contention from parallel linking; this number should not be quoted as a
general "how long does all_tests take" figure without that context.

**`ctest --no-tests=error --output-on-failure`: 623/623 passed, 0 failed.**

```
100% tests passed, 0 tests failed out of 623

Label Time Summary:
core            = 412.63 sec*proc (514 tests)
gui             = 126.27 sec*proc (175 tests)
models          = 159.78 sec*proc (259 tests)
unclassified    =   3.57 sec*proc (5 tests)

Total Test time (real) = 470.42 sec
```

Wall clock per `time`: 7:50.40 total (124.63s user + 25.16s system, 31%
CPU -- consistent with mostly-serial test execution, not the parallel
build). Load average settled back to 4.61/6.08/8.34 by the end.

**Known-acceptable ccache trio, checked explicitly: not hit.**
`tst_cty_dat_parser` (#500), `tst_adif_parser` (#501) and
`tst_dxcc_color_provider` (#503) all **passed** in this run (confirmed
individually before the full sweep finished, and again in the final
summary). `task_68cac297` describes a real but conditional failure mode
(`ccache base_dir` rewriting a `__FILE__`-derived path); this run's ccache
configuration did not trigger it, so there is nothing to explain away here.
The gate is clean, not "clean because we forgave the usual three."

**Step 2 verdict: GREEN.** 623/623, 0 failures, nothing to attribute to the
known ccache issue because it did not occur.

### Step 3: bench, local direct mode on the ANAN-G2E (STATUS: OPEN)

**Not run by Task 16.** The maintainer authorised bench access to the
ANAN-G2E (HermesC10) only, and explicitly excluded the ANAN-G2, a different
radio on the same LAN that an earlier smoke run in this branch reached by
accident (commit `d8f0ad15`, "docs(architecture): bench on the G2E only,
never the G2"). Task 16 did not run discovery, did not connect to anything,
and did not run the real `nereusd` or `NereusSDR` binaries. The maintainer
is running this row directly with the operator.

Numbered procedure, to be executed against the live ANAN-G2E:

1. Launch `NereusSDR` in local direct mode (no daemon involved).
2. Open the connect dialog and run discovery. List every responder together
   with its MAC address and board-type identification byte. Do not connect
   yet.
3. Identify the ANAN-G2E by its board-type byte: discovery byte `0x14`
   decodes to `HPSDRHW::HermesC10`. If a different radio (including an
   ANAN-G2) responds on the same LAN, do not select it.
4. Pin the candidate MAC address and confirm it with the maintainer before
   connecting. If the only responder present does not identify as the G2E,
   stop -- do not fall back to a different radio.
5. Connect to the pinned G2E MAC. Record the pinned MAC address here once
   run.
6. Per-slice S-meter check (Task 12 regression surface):
   a. Confirm each active slice's VFO flag S-meter bar moves with signal.
   b. Confirm the active slice's flag is written once per tick, not twice
      (no double-update or flicker between two competing writers).
   c. Right-click the analog S-Meter, set it to "S-Meter Peak", and confirm
      the VFO flag's mini-bar agrees with the analog needle. Repeat with
      "Max Bin". (Task 12 step 7's flag-vs-needle agreement check; see
      `task-12-report.md`, "Bench-checkable, per the three named local-mode
      risks".)
7. Open Setup -> Multimeter and move the Delay slider. Confirm both the
   composite meter widget's cadence and the VFO flags' cadence visibly
   change together.
8. Disconnect, then reconnect. Confirm the sequence completes cleanly with
   no visible glitch, hang, or stuck "Connected" indicator. This exercises
   Task 3's `setConnectionState`/teardown reordering; the reordering is
   expected to be invisible from the UI.
9. Quit and relaunch NereusSDR. Reopen the connect dialog and confirm the
   G2E's saved-radio entry is still present (Task 13 regression surface).
10. Record PASS/FAIL for steps 6 through 9 here, with the pinned MAC
    address and the date the bench was run.

**Result: OPEN. Not yet run.** A red result in Step 1 or Step 2 above
stops the phase regardless of this row's eventual outcome; those steps
already ran and are GREEN.

### Task 16 overall verdict

**GREEN** for the automated half (Steps 1 and 2: 259/259, 16/16, and
623/623, zero failures across all three sweeps). Step 3 (bench) remains
OPEN, owned by the maintainer, and is recorded above as a runnable
procedure rather than executed by this task.

### Controller correction to the Task 16 result, 2026-08-07

The task's own full-suite run reported 623/623 with zero failures. **An
independent controller re-run of the same binaries reported
`tst_tx_mic_source` FAILED.** Both runs are accurate; the suite contains a
load-dependent flake, so a single green full-suite run is not by itself
sufficient evidence.

Measured on macOS, 18 cores, at `22825c02`:

- Serially: 10 consecutive runs of `tst_tx_mic_source`, all exit 0.
- 18 concurrent instances: 15 failed.
- 12 concurrent instances: 10 failed.

The failure is real, not a teardown artifact:

```
FAIL!  : TestTxMicSource::concurrent_producerConsumer_noDataCorruption()
         Compared doubles are not the same (fuzzy compare)
   Actual   (drained[i])   : 512
   Expected (produced[i])  : 0
   Loc: [../tests/tst_tx_mic_source.cpp(299)]
```

The producer pushes one block then sleeps 50 microseconds; the ring is 512
frames. Under CPU contention the consumer is descheduled long enough for the
producer to lap the ring, so frames are overwritten before `drainBlock` reads
them, and the test asserts strict sample-order preservation.

**It is not a regression from this branch.**
`git log 3349ccb8..HEAD -- tests/tst_tx_mic_source.cpp src/core/audio/TxMicSource.{h,cpp}`
returns nothing, so neither the test nor the class was modified here. Chipped as
a follow-up (`task_107045fb`) with the reproduction and a diagnosis.

**The gate's verdict stands as GREEN for its stated purpose**, which is proving
that the fifteen R2 tasks did not regress local direct mode. Every targeted test
and every models-label test passed in both runs, and the single failure is a
pre-existing timing-sensitive test in an unrelated subsystem. The 623/623 figure
should be read as "623/623 on an unloaded machine", not as an unqualified pass.

---

## Task 20: remote-mode GUI gating, `--station`, and the acceptance run

Task 20 splits into a code half and a bench half. The code half (steps 1a,
1b, 2, 3 and 4) landed with the task and is verified by
`tests/tst_remote_gui_gating.cpp`. **Steps 5 and 7 are bench procedures and
are OPEN.** They are written out below as numbered runnable procedures so
whoever runs them does not have to reconstruct them.

### What shipped, and what the gate actually is

| Piece | Where |
| --- | --- |
| Capability authority | `RadioModel::ownsLocalDsp()` |
| Reach-through audit | `RadioModel::localDspHandOutCount()` / `localDspHandOutNames()` |
| Setup page gate | `SetupDialog::realizePage()`, one self-classifying branch |
| Local-hardware UI gate | `MainWindow::applyRemoteRoleGating()` + `showConnectionPanel()` + `openNetworkDiagnostics()` |
| MOX refusal | the existing `MoxController` `MoxCheckFn`, installed at construction for `Role::Remote` |
| Station selection | `--station` / `--token` / `--station-fingerprint` / `--station-allow-unpinned`, plus Setup > CAT & Network > Remote Station |

Five Setup pages are disabled on a remote model, all of them under Audio:
**Devices, TX Input, VAX, TCI, Advanced**. That is not a hand-written list,
it is what the gate classified, and it will change on its own if the pages
do. Expect them greyed out on the bench and do not file it.

### Known limitations to read BEFORE the bench, so they are not filed as bugs

1. **The daemon's listener is off by default.** `remote_port` defaults to 0
   and `remote_bind` to loopback, deliberately. The acceptance run needs a
   config file; step 5.1 below writes one.
2. **All 13 `TunerModel` properties arrive and cannot be applied**, so
   `StationClient::unappliedProperties()` reports 13 entries and the log
   carries 13 one-time warnings per handshake. That is honest reporting of
   a read-only-property limitation, not a regression; 10 of the 13 were
   already unapplied before R2.
3. **No spectrum, no waterfall, no audio, no MOX.** Expected, and step 5.9
   records it as a pass condition rather than a defect.
4. **The client token is stored in plain text** in this machine's settings
   file when entered through Setup. `--token` avoids writing it. Closing
   this belongs with R5's identity and pairing work.
5. **`FaultLog::reload()` is wired but unasserted.** Confirming it needs
   PGXL or TGXL hardware, which no row here has.
6. **Qt's generic TLS key backend is unverified.** The development machine
   uses the OpenSSL backend, so nothing has exercised `QSslKey` against
   OpenSSL 3's PKCS#8 output on a Schannel or SecureTransport build.
7. **Three Windows and Intel-macOS CI routes have never run on real
   hardware.** First proof is the PR, not this matrix.
8. **`options/autoAtt/rx1AdaptiveFloor` is Station-scoped and ungated.** It
   reaches the step attenuator through the adaptive decay floor, which is
   then persisted back into the bounds-checked keys. It is the one live
   path by which an in-range gated field can be driven outside its union by
   an ungated sibling. Step 6a is where that gets probed.
9. **The gate has one known blind spot: `RadioModel::rxChannelForSlice()`.**
   It is not counted by the hand-out audit, so a Setup page reaching DSP
   through it stays **enabled** on a remote client while doing nothing.
   **Setup > DSP > Options is already in that state**, and so is
   Setup > DSP > MNF's minimum-notch-width readout. Both were left enabled
   deliberately: each page exists mainly to edit Station-scoped settings
   that must round-trip, and each reaches for a channel for one incidental
   local binding, so disabling them would cost two working settings pages
   to silence one dead widget on each. Expect those two pages to be usable
   but to have one control that does not respond. Full reasoning is on
   `RadioModel::localDspHandOutCount()`. The blind spot does **not** widen
   unnoticed: check 2 of `scripts/verify-no-gui-dsp-access.py` inventories
   every `rxChannelForSlice()` call under `src/gui/` per file and fails on
   a call in an uninventoried file or a count that moved, in CI and in the
   pre-commit hook both. A third page joining these two has to pass that
   check first.
10. **`tst_settings_scope.cpp`'s completeness sweep has no
    over-classification arm, and cannot usefully be given one.** Both its
    assertions test for under-classification (a core/models key that is
    not Station; an overlap key that is not Station), which is why the ten
    over-classified `FreeDvReporter/*` window-presentation keys survived to
    a whole-branch review. The obvious inverse arm ("a key touched only
    from `src/gui` outside `src/gui/setup` must be OperatorLocal") flags
    25 keys against the tree as it stands after those ten were fixed, and
    every one of the 25 is correctly Station: the whole `DxCluster*` /
    `Rbn*` / `Wsjtx*` / `Pota*` / `PskReporter*` / `SpotCollector*` family
    is written only from `SpotHubDialog.cpp` while being read by
    `src/models/RadioModel.cpp` through a key this scan's regex cannot
    see. An exemption list of 25 that grows with every new GUI-written
    station key is the list doing the work, not the assertion, so the arm
    was declined and the ten keys are pinned by explicit
    `knownExamples_data()` rows instead.
11. **Three further `FreeDvReporter/*` keys are written only from
    `src/gui` and were deliberately left Station**: `SavedMessages`,
    `ReportToPsk` and `IdleTimeoutMinutes`. Each is a judgement rather
    than a presentation fact (the first two have Station-side
    counterparts; the third is the only expiry `FreeDVStationModel` has
    anywhere), none was in the review's scope, and all three are recorded
    in `SettingsScope.cpp`'s exception block so a later reader sees they
    were checked. If a bench finds a remote client's daemon store growing
    saved status-message lists, this is the entry to reopen.
12. **The `PanadapterModel` arm of the mirror is unreachable in R2.**
    `RadioModel::addPanadapter()` has no production caller (pan ids are
    minted GUI-side by `PanadapterStack`), so the `pan:N` loops in both
    `StationServer::buildMirror` and `StationClient::handleCapabilities`
    never execute and four `MirrorPolicy` rows read as live surface while
    being dormant. Left alone deliberately: the two ends are **symmetric**,
    so this is dead code rather than a divergence, and the rows are the
    default-deny table's whole point (a property with no entry mirrors
    read-only, so deleting them to "clean up" would silently change the
    answer the day 3F wires panadapters up). Expect
    `mirroredObjectKeys()` to carry no `pan:` entry on the bench and do
    not file it.
13. **A remote client's panadapters come up looking live and paint
    nothing.** `MainWindow::pushConnectionStateToPans()` computes
    `live = isConnected()`, which is true on a remote client after task 3,
    so the pans do not show the disconnect overlay. There is no trace to
    draw because R2 ships no spectrum at all (limitation 3). Arguably the
    correct answer already, since the radio really is connected, and R3's
    spectrum path settles it either way, so nothing was changed here
    rather than adding an R2-only overlay state that R3 deletes. This is
    step 5.9's expected appearance, not a defect.
14. **Once a station is saved in Setup, every launch is remote** until the
    field is cleared. There is no `--local` escape hatch. A launch against
    a dead station therefore shows a greyed Connect menu with no obvious
    cause. **Recovery: Setup > CAT & Network > Remote Station, clear the
    Station address field, relaunch.** The Remote Station page is never
    disabled by the gate, which is asserted by
    `theRemoteStationPageStaysUsableOnARemoteModel`, so this recovery is
    always reachable.

---

### Step 5: the acceptance run (STATUS: OPEN; row 5.13 was BLOCKED, now UNBLOCKED)

> **Row 5.13 was blocked, and is not any more. Read the row description
> below before running it: what it checks has changed shape.**
>
> The whole-branch review found that `StationClient::invokeCommand()` had
> **zero callers**. The GUI's slice controls called `RadioModel` directly, so
> on a remote client an active-slice click flipped `active` locally, the
> mirror correctly refused to send a daemon-authoritative property outbound,
> the daemon never learned, and with nothing changed on the daemon there was
> no corrective delta coming back: a silent, permanent divergence. Add-slice
> minted a client-local slice the station had never heard of. Task 4 created
> `IStationLink` as the attach point with a header note that a later task
> would grow it once it had a real call to make; no later task was assigned
> that work.
>
> **What now ships.** `IStationLink` carries the five verbs
> `SessionCommandDispatcher` already accepts (`addSlice`, `addSliceOnPan`,
> `removeSlice`, `setActiveSliceById`, `requestSliceSampleRate`).
> `StationClient` implements it and attaches itself to the `RadioModel` in its
> constructor. `RadioModel`'s five slice-mutating entry points route to the
> link when `role() == Role::Remote` **instead of** mutating locally.
> `tests/tst_remote_slice_commands.cpp` pins the whole path end to end,
> against a real `StationServer` over the in-process transport, asserting on
> the DAEMON's own `RadioModel`.
>
> **Three things the bench must watch for, because they are what a test on
> one host cannot show:**
>
> 1. **Latency is now visible in the UI.** A remote click is asynchronous by
>    construction: nothing moves locally until the daemon has acted and the
>    delta has come back. On a LAN that is one `StationServer::
>    kDefaultDeltaFlushMs` tick (50 ms) plus the round trip. Over a real
>    internet path in R5 it will be longer, and there is **no pending-state
>    UI** for the gap. A flag that takes a beat to highlight is expected
>    behaviour, not a defect. File it if it feels bad; do not file it as
>    "the click did nothing" without watching for the deferred update.
> 2. **Refusals arrive as toasts.** The station's own reason is relayed
>    verbatim onto `sliceAddRejected` (4 s toast) or, for rate changes,
>    `sliceRetuneRejected` (6 s toast). If a refusal toast shows this
>    client's wording rather than the daemon's, that is a defect.
> 3. **`addSliceOnPan` carries a `panId` the daemon interprets on its own
>    terms.** It is honest, not faked: the daemon stamps it as `panKey` and
>    derives its stream placement from whether that pan already holds slices,
>    and `panKey` mirrors back. But the client's pan LAYOUT is client-owned
>    and `RadioModel::addPanadapter()` still has no production caller, so a
>    slice added onto a second pan is placed by the daemon against the daemon's
>    view of that pan id. Check where a second pan's slice actually lands.
>
> Rows 5.10 through 5.12 still cover property mirroring only. 5.13 is the one
> row that covers command verbs.

**Bench hardware: the ANAN-G2E, never the ANAN-G2.** The G2 is a different
radio reachable on the same LAN and is explicitly excluded. Confirm the
board by its discovery board-type byte before starting, not by asking for a
MAC, and reuse the identification pinned at task 16 step 3.

Two processes, one host. `nereusd` holds the radio; the GUI holds nothing.

| Row | What | Needs a radio? | Result |
| --- | --- | --- | --- |
| 5.1 | Daemon config written, `nereusd` starts, listener bound | no | OPEN |
| 5.2 | GUI launches with `--station`, `wss` handshake completes | no | OPEN |
| 5.3 | Token round-trip succeeds | no | OPEN |
| 5.4 | A WRONG token is refused, and refused again while rate-limited | no | OPEN |
| 5.4a | Setup is REFUSED with a toast while the station is unreachable | no | OPEN |
| 5.5 | Settings snapshot arrives; `setupDialogAllowed()` opens the gate | no | OPEN |
| 5.6 | Setup opens; Remote Station page is usable; the 5 Audio pages are greyed | no | OPEN |
| 5.7 | Connect / Disconnect / Manage Radios / Protocol Info are all greyed; Network diagnostics refuses | no | OPEN |
| 5.8 | MOX is refused with a toast naming R4; the radio stays in RX | **yes** | OPEN |
| 5.9 | Panadapter blank, waterfall blank, speakers silent, spot collectors inert | **yes** | OPEN |
| 5.10 | VFO, band, mode, filter round-trip and the radio follows | **yes** | OPEN |
| 5.11 | AGC, NR, NB, SNB, APF, squelch round-trip | **yes** | OPEN |
| 5.12 | RIT, XIT, antenna round-trip | **yes** | OPEN |
| 5.13 | Add slice, remove slice, set active slice, per-slice sample rate: the command verbs reach the daemon, the daemon acts, and the answer comes back through the mirror. A refusal shows the STATION's wording. Nothing moves on the client before the daemon has spoken. See the note above. | **yes** | OPEN (was BLOCKED) |
| 5.14 | Per-slice S-meter needles are live | **yes** | OPEN |
| 5.15 | Setup pages round-trip station settings both ways | **yes** | OPEN |
| 5.16 | `unappliedProperties()` reports exactly the 13 TunerModel entries | no | OPEN |

Rows 5.1 through 5.7 (including 5.4a) and 5.16 do **not** need a radio and
can be run first on any machine. Record which rows ran and which are still
OPEN; do not merge the two into one verdict.

#### Radio-free subset: RUN 2026-08-09, results below

Run on macOS after the whole-branch review fixes landed, with the daemon
under `--profile r2accept` and the client under `--profile r2client` so
neither touched the developer's real settings directory.

**The ANAN-G2E was not on the network.** A two-packet discovery probe (P1
and P2) across both subnets got exactly one answer: the ANAN-G2 at
`192.168.109.45`, which is the radio the maintainer excluded. So
`radio_mac` was pinned to the G2E's MAC deliberately rather than left
unset, because unset means "first radio discovered" and that would have
grabbed the excluded board. The daemon saw the G2, logged it, and
continued disconnected. **Rows 5.8 through 5.15 stay PENDING on G2E
availability.**

| Row | Result | Evidence |
| --- | --- | --- |
| 5.1 | **PASS** | `Station listening on wss:// 127.0.0.1 : 50055`. The pairing banner printed on stdout with the full 32-byte fingerprint intact, and the log confirms neither secret reached it. |
| 5.2 | **PASS** | `Station handshake complete: wss://127.0.0.1:50055` |
| 5.3 | **PASS** | Daemon: `Session established`. The pin was satisfied, or the run would not have reached auth. |
| 5.4 | **PASS** | Client: `Station refused authentication`. Daemon: `Authentication refused ... Peer detached`. |
| 5.4a | OPEN | Needs UI interaction. |
| 5.5 | **PARTIAL** | The snapshot arrived but carried `0 station settings`, because the daemon profile was fresh. That exercises the seed-marker fallback rather than the non-empty-snapshot branch. Re-run against a daemon with real station settings. |
| 5.6 | OPEN | Needs UI interaction. |
| 5.7 | OPEN | Needs UI interaction. |
| 5.16 | **PASS** | Exactly 13 `TunerModel` entries warned, matching limitation 1 verbatim: `relayC1`, `relayL`, `relayC2`, `isOperate`, `isBypass`, `isTuning`, `antennaA`, `hasAntennaSwitch`, `isPresent`, `hasDirectConnection`, `tgxlIp`, `fwdPower`, `swr`. |

Two things this run did **not** prove, stated plainly so nobody reads more
into it than it earned:

1. **It did not exercise the remote-handshake crash fix.** That crash
   needs `caps.radioConnected == true`, which needs the daemon to hold a
   radio. With no radio the branch is never entered. The fix is real and
   the null check is there, but **this run is not its proof**; the G2E
   bench is.
2. Clean shutdown was observed (`Peer detached ... peer closed the link`),
   which is the easy half of link loss. The silent-death half is what the
   heartbeat exists for and is covered by test, not by this run.

#### 5.1 Start the daemon

```
mkdir -p ~/.config/nereusd
cat > /tmp/nereusd-bench.conf <<'CONF'
remote_port = 50100
remote_bind = 127.0.0.1
CONF
cmake --build build --target nereusd
./build/nereusd --profile daemon --config /tmp/nereusd-bench.conf
```

The first run prints the generated token and the certificate fingerprint.
Copy both. The token file lives beside the daemon profile with
owner-only permissions and is deliberately NOT in AppSettings.

Confirm the listener is actually bound before going further:

```
lsof -nP -iTCP:50100 -sTCP:LISTEN
```

An empty result means `remote_port` did not reach the daemon, which is
limitation 1 above, not a defect.

#### 5.2 through 5.3 Launch the GUI against it

```
./build/NereusSDR --profile client \
  --station wss://127.0.0.1:50100 \
  --token <TOKEN-FROM-5.1> \
  --station-fingerprint <SHA256-FROM-5.1>
```

Pass: the toast reads `Connected to station wss://127.0.0.1:50100`, and the
daemon log shows one authenticated peer.

`--profile client` matters. Without it both binaries resolve to the same
settings file on one host and the whole SettingsProxy layer is bypassed
without anything appearing wrong. See design addendum section 2.1.

#### 5.4 The wrong token

Relaunch with `--token deliberately-wrong`. Pass: the session ends with an
auth failure and the GUI does not reach Connected. Repeat five times, then
try the CORRECT token immediately. Pass: it is still refused while the
lockout is in force (`TokenStore` returns `RateLimited`, which is distinct
from `Rejected` on purpose). Wait out `kDefaultLockoutMs` and retry to
confirm recovery.

#### 5.4a The gate seen SHUT

Run this before 5.5, because 5.5 only proves the gate opens and an
always-open gate would pass it. Stop `nereusd` (or leave it stopped after
5.4) and launch the GUI with the same `--station`. The handshake cannot
complete, so `SettingsProxy::ready()` stays false.

Now try to open Setup, by any route: **File > Settings** (Ctrl+,), a
right-click "Setup" item on an applet, or the VFO flag's AGC / NB / NR
right-click hops. All twelve routes go through
`MainWindow::createSetupDialog()`.

Pass: **no dialog appears**, and a status-bar toast says Setup is not ready
yet and that this window is still waiting for the station's settings. Fail:
the dialog opens. That is the failure that matters most on this whole
matrix, because a Setup dialog opened here shows this machine's ship
defaults, and the first control the operator touches writes one of them
into the STATION store as if it had been chosen deliberately.

Then start `nereusd` and wait for the reconnect toast. Pass: the same Setup
route now opens the dialog, with no relaunch needed. What changed is the
snapshot arriving, nothing the operator did.

#### 5.5 through 5.6 The Setup gate and the page gate

Open Setup. Pass: it opens at all (`SettingsProxy::setupDialogAllowed()` is
`ready()` **and** (non-empty snapshot **or** the seed marker
`AppSettings::kDaemonProfileSeededKey`)). It is asked through
`setupDialogAllowedForCurrentBackend()`, from
`MainWindow::createSetupDialog()`, which is the only place in `src/gui`
that constructs the dialog. Setup cannot be opened before the handshake
completes; 5.4a is that half of the row.

Then, on the Audio category: Devices, TX Input, VAX, TCI and Advanced must
all be greyed out. On CAT & Network: **Remote Station** must be usable, and
must show the station address that was passed on the command line only if
it was also saved; the command line does not write it back.

Two pages are expected to be **enabled but partially dead**, which is
limitation 9 above and not a defect to file: Setup > DSP > Options (its
high-resolution filter-characteristics toggle binds a local WDSP channel
that does not exist here) and Setup > DSP > MNF (its minimum-notch-width
readout reads the same channel). Everything else on both pages is
Station-scoped and should round-trip normally.

Audio > TCI is greyed for a different reason from its four neighbours and
this is worth knowing before judging it: the page itself touches no local
DSP at all. It is wrapped by `SetupDialog::wrapWithAudioBackendStrip`,
which constructs an `AudioBackendStrip` against the local `AudioEngine`,
so every wrapped leaf classifies as reaching regardless of its own
content. Left as-is pending a maintainer decision, because the only
non-invasive fix (skip the strip in remote mode) would also un-grey
Audio > TX Input, whose sole reach is a VU-meter timer tick. That is a
change to which pages the gate outputs, and it belongs with the "a
disabled page says nothing" question rather than with this task.

#### 5.7 The local-hardware surfaces

Radio menu: Connect, Disconnect, Manage Radios and Protocol Info all
greyed, each with a tooltip saying why. Click the status-bar RTT block and
the audio pip: neither may open Network Diagnostics. Right-click the
connection segment: the Network diagnostics entry must do nothing. Nothing
anywhere may open the Connection Panel.

#### 5.8 MOX (needs the G2E, and a dummy load or antenna)

Press MOX. Pass: a warning toast naming R4 appears, the MOX state does not
advance, and **the radio stays in receive**. Confirm the last part at the
radio, not only on screen: the screen is the thing under test.

#### 5.9 The blank surfaces, recorded as EXPECTED

Panadapter blank, waterfall blank, speakers silent, spot collectors inert.
All four are R2 scope decisions, not defects. Record them as passes.

#### 5.10 through 5.15 The control round-trips

For each control: change it in the GUI, confirm the radio followed, then
change it at the daemon end (or via a second client) and confirm the GUI
followed. A control that moves the radio but does not come back is a
one-directional mirror bug and is worth more than a note.

#### 5.16 Unapplied properties

Read `StationClient::unappliedProperties()` from the log at handshake.
Pass: exactly 13 entries, all `TunerModel.*`. More than 13, or any entry
outside `TunerModel`, is a real finding.

---

### Step 6: proxied-read scan (STATUS: OPEN)

Scan the `proxied-read ... resolved-locally` log lines produced during step
5 for keys that should have classified Station and did not. The log line
comes from task 15 step 9 and is on the `nereus.settingsproxy` category:

```
QT_LOGGING_RULES="nereus.settingsproxy.debug=true" ./build/NereusSDR --profile client --station ...
```

### Step 6a: one hostile value (STATUS: OPEN, EXPECTED TO DOCUMENT A GAP)

Inbound Station writes reach the daemon's store **unvalidated**.
`SettingsHygiene` runs once on connect, only reports rather than clamps,
and reports to two GUI diagnostics pages `nereusd` does not link. Task 15
added a bounds check for the step attenuator specifically, because
`StepAttenuatorController::loadForMac` demonstrably bypasses the clamp that
`setAttenuation()` applies. The general gap is deliberately left open for a
later phase.

Procedure: from the client, write one out-of-range Station value (the step
attenuator keys are the ones with a bound to violate) and record what the
daemon does with it. **This row is expected to document a gap, not to
pass.** Also probe `options/autoAtt/rx1AdaptiveFloor`, which is
Station-scoped and ungated and reaches the attenuator through the adaptive
decay floor.

---

### Step 7: re-run the task 16 gate (STATUS: OPEN for its bench half)

Tasks 17 and 20 both landed after the task 16 gate ran, and both have large
local blast radius, so the gate is re-run here rather than trusted.

**7.1 Targeted regression set.** Re-run task 16 step 1 exactly as recorded
in the Task 16 section above, including the eleven-component regex. Confirm
with `ctest -N` that every component contributed at least one test: a regex
component matching nothing is invisible to `--no-tests=error`.

**7.2 Full suite.**

```
cmake --build build --target all_tests
ctest --test-dir build --no-tests=error
```

Read the result against the known-flaky sets recorded in the Task 16 section
and in "A second load-dependent flake" below: `cty_dat_parser`,
`adif_parser` and `dxcc_color_provider` can fail under ccache, and
`tst_tx_mic_source` and `tst_p1_loopback_connection` are load-dependent.
Anything else is a real finding. Do not report an unqualified pass from a
single run on a loaded machine.

**7.3 Bench, local direct mode on the ANAN-G2E.** Re-run task 16 step 3 on
the same pinned G2E, with **no** `--station` argument, and confirm local
direct mode is unchanged: discovery finds the radio, connect succeeds, the
panadapter and waterfall paint, audio comes out of the speakers, MOX keys
into a dummy load, and every Setup page is enabled. This is the row that
proves task 20 did not gate anything it should not have.

**7.4 Release artifacts.** Confirmed on the PR, not locally. `release.yml`
is the only workflow that runs `cmake --install`, and it triggers only on
`v*` tags, so the `nereusd` install path still has no PR-CI coverage.

---

### Task 20 automated coverage (GREEN)

```
$ cmake --build build --target tst_remote_gui_gating
$ ctest --test-dir build -R remote_gui_gating --no-tests=error --output-on-failure
100% tests passed, 0 tests failed out of 1
```

21 cases. Non-vacuity was established by sabotage-and-revert on four
separate mechanisms; the results are recorded in
`.superpowers/sdd/2026-08-03-remote-daemon-r2-plan/task-20-report.md`,
including one sabotage that did **not** fail and the claim that was
corrected as a result.

### A second load-dependent flake, found by Task 20's full-suite run

Task 20's full suite reported **623/627 with four failures**: the three
known ccache-sensitive parsers (`tst_cty_dat_parser`, `tst_adif_parser`,
`tst_dxcc_color_provider`) and one that was not on any known list,
`tst_p1_loopback_connection`.

Diagnosed rather than waved through. Reproduced by running the binary 40
ways concurrently: 39 of 40 exit 0, one fails.

```
QWARN  : P1: Connect watchdog fired -- no ep6 frame within 2000 ms;
         tearing down and emitting connectFailed(Timeout)
FAIL!  : TestP1LoopbackConnection::firstConnectLogFiresExactlyOncePerBatch()
         Compared values are not the same
   Actual   (conn.state())              : 0
   Expected (ConnectionState::Connected): 3
   Loc: [../tests/tst_p1_loopback_connection.cpp(189)]
```

The failing run took 10615 ms against a normal 1021 ms. The test burst-sends
100 ep6 frames and then waits for `Connected`; under enough CPU contention
the event loop does not run for two seconds, so `P1RadioConnection`'s own
2000 ms connect watchdog fires first, tears the connection down, and the
state reads Disconnected. That is the product behaving correctly against a
starved event loop, not a product defect.

**Not a regression from this branch.**
`git log 3349ccb8..HEAD -- tests/tst_p1_loopback_connection.cpp
src/core/P1RadioConnection.{h,cpp} tests/fakes/P1FakeRadio.cpp` returns
nothing, so neither the test nor the class it exercises was touched here.
Serially and at 16-way concurrency it passes every time; it took 40-way
concurrency to reproduce.

One procedural note worth recording, because it cost time: **running
`ctest -R <name>` to investigate a failure overwrites
`build/Testing/Temporary/LastTest.log`**, so the failing run's output is
gone by the time you go looking for it. Read the log first, re-run second.
`LastTestsFailed.log` survives and is the reliable list of which tests
failed.

---

## Whole-branch review, 2026-08-09

Run after all 20 tasks landed, as four scoped reviewers (session and mirror,
settings plane, security and daemon and build, GUI and role and local-mode
non-regression). Every task had already had its own review and fix rounds.

**It found five Criticals. Every one of them is a gap BETWEEN tasks, which is
exactly why twenty per-task reviews saw none of them.** Recorded here rather
than only in the ledger, because the pattern is the reusable part.

| # | What | Why no per-task review could see it | Status |
| --- | --- | --- | --- |
| 1 | The GUI never issues the command verbs. `StationClient::invokeCommand()` has zero callers, so an active-slice click or an add-slice never leaves the client. | Task 11 built the verbs, task 18 built the session, task 20 gated the GUI. **No task was assigned to connect them.** Task 4's `IStationLink` seam was still an empty interface. | Fixed. `IStationLink` grew the five verbs, `StationClient` implements it and attaches itself, and `RadioModel`'s five slice-mutating entry points route to the link in `Role::Remote` instead of mutating locally. Pinned end to end by `tests/tst_remote_slice_commands.cpp`. Row 5.13 unblocked. |
| 2 | The remote GUI crashed on handshake: `connection()->radioInfo()` guarded only by `isConnected()`. | Task 3 changed `isConnected()` from a pointer test to a stored state, so the guard stopped implying a non-null pointer. The caller is in a different file and was written months earlier. | Fixed, `29744d76` |
| 3 | The first-run pairing banner printed a fingerprint with 25 of 32 bytes destroyed. | The redactor's MAC pattern and the banner were written by different tasks. A colon-separated hex fingerprint is exactly the shape of a MAC. | Fixed, `fa95d6de` |
| 4 | No incoming message cap on the station listener: about 2 GiB per message per socket, entirely pre-authentication. | The precedent lives in `TciServer.cpp`, a file the session work had no reason to open. | Fixed, `edac6eaa` |
| 5 | The certificate pin was consulted only from inside the `sslErrors` handler, so a clean handshake skipped it and `ws://` sent the token in cleartext. | The pin check landed in the handler that looked like its natural home. Nothing in one task's scope asks "what if this handler never fires?" | Fixed, `0360ec0f` |

Two of the five are the same species: **a guard that used to imply something and
quietly stopped**. That is the question worth asking on any future branch that
changes a widely-read predicate: not "who calls this?" but "what did callers
believe this proved, and does it still?"

Beyond the Criticals: TUNE was completely ungated remotely and, because
`TransmitModel::tune` is Bidirectional, a remote TUNE press **wrote onto the
daemon** and switched the station's drive-power source. Fixed in `fc4a8000`.
Twelve further Importants were fixed across five rounds, `5aff2905` through
`9b91fd4e`.

### Two things recorded and deliberately not fixed

- `TwoToneController::activate` writes the mirrored `TransmitModel::power`
  before its own `setMox(true)`. It is a dead door today **only** because
  `m_txChannel` is assigned inside `connectToRadio`, which a remote model never
  enters. **It opens the moment R4 gives a remote model a TxChannel.**
- The rate limiter is global rather than per-peer, so anyone who can reach the
  port can hold the station rate-limited by guessing every 60 s. Making it
  per-peer would key unbounded state on attacker-controlled input and buys
  nothing against address rotation. The mitigations that actually apply are
  that `remote_port` defaults to 0 and `remote_bind` to loopback. A durable
  answer belongs with R5.

### One methodology note

The single highest-value fact this review produced was not a fix, it was an
explanation: **acceptance rows 5.2 through 5.16 could never have passed**,
because the client died at the handshake. The bench run that would have found
it was scheduled after every task was already marked complete. On the next
phase, run the first end-to-end bench earlier, even against a stub, rather
than treating it as the final gate.
