# Slice control and shared listening: implementation plan

Status: plan for crew, in progress. The six Core policies are approved and
final (JJ, September 28, 2026; addendum G-118). JJ ruled the full-window UI
(flags, applets, multiple pans, placement of unseen slices, floating focus,
same-pan flag stacking, local-audio presentation, layout changes and the TX
applet letters) the same day as U1 to U8, recorded verbatim below; Tasks 13
to 16 are no longer blocked and carry acceptance cases written from those
rulings. Task 1 is complete on its lane (signed `efb398303` on
`codex/slice-access`, not yet merged to the trunk); the crew ledger is
`.crew/2026-09-28-slice-control-and-listening-plan/progress.md` in the trunk
worktree.

Inputs read in full:

- `docs/architecture/2026-09-28-slice-control-and-listening-design.md` (the
  approved design; "the design" below)
- `docs/architecture/2026-09-27-core-gui-plan-addendum.md` G-118, G-124,
  G-125, G-126
- `core-gui-slice-experience-deep-scout.md`,
  `core-gui-multi-pan-slice-flow-scout.md`,
  `core-gui-multi-pan-reference-report.md` (in `~/.config/nereus/work/`)
- `docs/architecture/2026-09-23-station-link-v1.md` sections 4, 4.1, 6.2,
  6.3, 7.1, 7.7, 17, 18; `docs/architecture/2026-09-20-remote-media-control-v1.md`
  "Receiver audio"

Every file:line below was read at `/Users/j.j.boyd/.codex/worktrees/nereus-r2-integration/NereusSDR`
HEAD `26e89c507` (source identical to `a67633ddb` for every cited file; the
only commit between them is a Setup doc). An implementer re-reads each cited
range before editing, because the lane moves.

Dependency spine:

```
1 incarnation + control revision
  -> 2 three predicates at every write, media and TX path
    -> 3 per-device membership + per-device active RX
      -> 4 negotiation, SliceAccess mirror, listen/stop/take/release verbs
        -> 5 remote desktop client plumbing (no new UI)
        -> 6 per-listener audio fan-out (needs ruling Q1)
        -> 7 zero-slice Core safety
          -> 8 grace expiry + explicit leave with absence-generation fencing
            -> 9 capacity refusals offer existing slices; victims kept
              -> 10 hosting desktop through the same validated operations
                -> 11 TX surfaces stay on the transmit slice
                  -> 12 phone contract note
                    -> 13..16 UI (ruled by JJ, U1 to U8, 2026-09-28)
                      -> 17 delivery verification
```

### Execution order (JJ, 2026-09-28 night)

JJ's ruling: "same plan, same scope, new order". The dependency spine above
still says what each task needs. This is the order the crew runs them in:

1. Tasks 1 to 5 (done), their fix wave, and the layout-restore regression fix
   (done).
2. Task 7: zero-slice Core safety.
3. Task 8 with Amendment 8a: grace expiry, explicit leave and generation
   fencing, and slices held for away devices never count in the preselector
   (BYPASS) decision or in several-devices confirmations.
4. Task 8b: device identity clarity. It is a pairing-security change and gets
   a scoped review before merge.
5. Task 13: the bottom RX chooser, built to the approved mockup.
6. Task 14a: joined flags, their menus (Release, Take control, Stop
   listening), labels and disabled tuning, without the "Your volume" level.
7. Task 6: per-listener audio fan-out.
8. Task 14b: the "Your volume" level on a listened slice.
9. Tasks 9, 10, 11, 15, 16, 12 and 17, in that order.

Consequences of the new order:

- Task 13 now runs before Task 10. Its hosting-desktop rows use the hosting
  path as it stands at that point; Task 10 moves them onto the same validated
  operations afterwards, without changing what the chooser shows.
- Task 14a runs before Task 6. Until Task 6 lands, a listened slice's flag
  shows no level control; the merge gate on Task 6 (Important 3 of the Tasks
  1-4 review) still applies to the branch as a whole.
- Rulings U1 to U8 are made (see the ledger). The BLOCKED marks on Tasks 13
  to 16 below record when each was planned; each task's brief carries the
  ruling that applies to it.

## Global Constraints

- **Source first where Thetis applies.** Shared listening, control handoff,
  membership and claims are NereusSDR-original (Thetis has one operator and
  no device sessions); new files carry `// no-port-check: NereusSDR-original.`
  and a Modification history block. Anything touching a Thetis-derived path
  (the audio mixer's Thetis `cmaster.c` / `audio.cs` cites in
  `AudioEngine.cpp`, WDSP panel gain, TX band derivation) keeps its existing
  cites and inline tags verbatim, and any new logic taken from Thetis is read
  first at `/Users/j.j.boyd/Thetis` (`v2.10.3.15`, `3759d096`) and cited
  `// From Thetis <file>:<line> [v2.10.3.15]`. If a task needs Thetis logic
  it cannot find: stop and ask.
- **AppSettings, never QSettings.** Booleans as `"True"` / `"False"`.
- **No mutex in the audio callback.** DSP-thread code (`rxBlockReady`,
  `MasterMixer` drains, taps) reads only atomics or immutable snapshots
  published by the main thread; it never walks a listener list, a QHash, a
  QObject or `RadioModel`'s slice list (the R-R3-49 rule, `AudioEngine.h:320-338`).
- **Model thread only** for every control or membership change
  (`SliceOwnership.h:52`: "Single thread: RadioModel's").
- **Wire changes ship with their documents** (link section 17): the link
  document, `tests/data/link/v1/`, `surface.json` (regenerate with
  `tst_link_surface_manifest_regen`, then `python3 scripts/render-link-tables.py`)
  in the same commit. New capability entries are appended, never inserted
  (G-52, G-68). Wire kinds are the link's section 4.1 set: `bool`, `i64`,
  `f64`, `utf8`, `enum`. A media operation change also updates
  `2026-09-20-remote-media-control-v1.md`.
- **Operator wording.** Every reason the Core sends passes
  `OperatorWording::isPlain` and is scanned by `tst_station_reason_wording`;
  plain operator words, American spelling in app wording, no protocol terms
  (no "incarnation", "revision", "claim", "predicate", "capability"). No
  source cites inside user-visible strings.
- **No em dash** anywhere: code comments, commit messages, docs, strings.
- **Commits:** GPG-signed (`git commit -S`), hooks installed
  (`scripts/install-hooks.sh`), run with `NEREUS_THETIS_DIR=/Users/j.j.boyd/Thetis`
  so `verify-inline-tag-preservation.py` sees the corpus. No `Co-Authored-By`
  trailer. Never `--no-verify`, never `--no-gpg-sign`.
- **Tests run offscreen:** every ctest and test binary is prefixed
  `QT_QPA_PLATFORM=offscreen`.
- **Never key a radio.** No task keys real hardware. TX cases use the fake
  radio / fake `TransmitHolder` hooks. No live radio, service restart, app
  launch or preview without JJ's explicit go-ahead.
- **Build and test at the real load:** `nice -n 10 cmake --build <build> -j2 --target <tst>`
  then `QT_QPA_PLATFORM=offscreen nice -n 10 ctest --test-dir <build> -R '^<tst>$' --output-on-failure`.
  Never wait for the load to drop and never raise a deadline to make a test
  pass. Record load average and ninja step count with any timing. A test that
  fails only under load is a finding: report the test, the assertion, the
  load, the cause and a fix; never retry it into a pass.
- **No stubs.** No placeholder logic, no "return 0 to unblock". A task that
  cannot be finished as written stops and reports.
- **Older peers see today's wire.** Nothing new reaches a peer that did not
  declare the feature; everything a sliceAccess peer gets beyond today is
  behind the two-key gate (link 6.2).
- **UI decisions are not the implementer's.** Tasks 13 to 16 build only what
  JJ has approved; any open presentation choice goes back to him.

## Open decisions (each a question with a recommendation)

Core questions. A task that needs one names it; it does not start until the
controller has JJ's answer (or JJ accepts the recommendation).

- **Q1. Listener fan-out mechanism (blocks Task 6).** The design says to
  verify the per-receiver tap's zero-gain behavior and stream capacity
  before using it. The tap is one `SliceAudioTap` per (device, slice) with
  `kMaxSliceAudioTaps = 8` Core-wide (`AudioEngine.h:405-420`, installed per
  stream at `DaemonAudioSource.cpp:437-467`) and at most
  `kMaxReceiverAudioStreams = 4` per device (`IMediaTransport.h:243`); a
  listened receiver stream would have its own RTP timeline that the device
  must mix and clock against its main program. **Recommendation:** mix each
  listened slice into the listener's existing owner mix at the Core
  (`AudioEngine.h:572-601`, `MasterMixer.h:299-330`), taking the same
  AF-undone, pre-mute block the tap takes, at the device's own level and
  mute held in atomics. Why: one program stream and one audio clock per
  device (no new client mixer, phone gets it free), bounded by
  `kMaxOwnerMixes = 4` times 32 slice bits with no new stream admission to
  fail, TX monitor and headphones handling unchanged. Caveat: it adds a
  per-owner, per-slice ramp to the mixer's owner drain.
- **Q2. Controller AF at zero silences listeners.** AF is applied inside
  WDSP (`SetRXAPanelGain1`), and the undo returns 1.0 at or below 0.001
  (`AudioEngine.cpp:2040-2053`), so a controller who turns AF to zero
  silences every listener. **Recommendation:** accept it for this plan and
  let the listener's screen say why from the mirrored AF value (no DSP
  change); a pre-PanelGain1 WDSP tap is a separate DSP change.
  **Superseded 2026-09-28 by U5** (lead ruling, "settles Q2 vs U5"): U5
  wins; a listener's audio never depends on the controller's AF, including
  AF at zero. See Task 6.
- **Q3. Listener pan and route.** A slice's pan (`audioPan`) and
  speakers/headphones route (`outputRoute`) are shared `SliceModel`
  properties. **Recommendation:** a listener hears the slice centered in its
  own speakers sum; the controller's pan and route are not applied, so a
  controller never moves audio on another device.
- **Q4. Starting listen level.** **Recommendation:** a new listener, and the
  former controller after a handoff, start at the slice's current
  AF-equivalent linear level, so nobody hears a jump; after that each
  device's level is independent.
- **Q5. Stop listening from the controller.** The controller is always a
  listener. **Recommendation:** not offered; the Core refuses
  `slice.stopListening` from the controller with a reason pointing to
  Release.
- **Q6. Existing close paths on a shared slice.** Flag close and
  `removeSlice` close a slice outright today
  (`SessionCommandDispatcher.cpp:1990-2031`, `MainWindow.cpp:3114-3128`).
  **Recommendation:** from its controller, a close acts as Release: kept for
  remaining listeners, closed when nobody else is joined. An older window's
  `removeSlice` on its slice follows the same rule.
- **Q7. Taking control from an older window.** An older window (no
  sliceAccess) cannot stay on as a listener, which the approved handoff
  requires. **Recommendation:** refuse `slice.takeControl` on a slice whose
  controller lacks the feature, with a plain reason; the existing capacity
  Take stays available.
- **Q8. What "clear TX selection" does under the one-bound-slice invariant**
  (`TxSliceArbiter.h:40-45`, `syncToSliceList` at `TxSliceArbiter.h:134-153`).
  **Recommendation:** (a) follow ruling 8.12's close path
  (`RadioModel.cpp:11964-11987`, `StationTransmitTake.cpp:529-557`): if the
  former controller holds transmit, the flag moves to another of its
  slices, else transmit is released through a transfer to nobody; (b) drop
  the slice from the former controller's remembered TX choice
  (`m_chosenTxSlice`, `StationServer.cpp:2677-2689`); (c) the new
  controller's first key does not bind the taken slice through
  `bindForHolder`'s active-slice fallback (`TxSliceArbiter.cpp:64-69`); it
  must pick it with `tx.setTxSlice`. (c) is the part that needs JJ's yes.
- **Q9. Auto-adoption of a released slice.** A lone device adopts every
  slice nobody owns (`StationServer.cpp:7649-7659`, admission step 3 at
  `:7723-7725`, `sliceAdded` hook `:2652-2661`, hosting at
  `StationHost.cpp:163-170`). A released slice with listeners also has no
  controller. **Recommendation:** never adopt a slice that has listeners;
  control comes only from Take control.
- **Q10. Zero slices at radio connect and restart.** Radio connect creates
  Slice A when there is none (`RadioModel.cpp:14159-14163`); Protocol 2 needs
  a first DDC frame to finish connecting (`P2RadioConnection.cpp:768-772`);
  a saved layout of zero slices is refused (`ReceiveLayoutStore.cpp:140-144`).
  **Recommendation:** zero slices is supported while connected; a radio
  (re)connect or Core restart with none still creates one unowned Slice A so
  the radio can stream, and Q9's rule does not stop a lone arriving device
  adopting it (it has no listeners).
- **Q11. Listener membership across a Core restart.** Owners persist in the
  restart manifest (ruling 5.3, `RadioModel.cpp:1192-1195`).
  **Recommendation:** listeners are not persisted; a restored slice keeps its
  controller as today, and devices listen in again.
- **Q12. Existing held marks.** Today a device's slices pass to the station
  "held for" it when nobody else is on the Core (`StationServer.cpp:7846-7858`).
  **Recommendation:** no new holds are created under the approved expiry
  rule; holds restored from an older manifest still return on admission
  (`SliceOwnership.cpp:188-195`) and follow the new rule at that device's
  next expiry.
- **Q13. Display and raw I/Q for listeners.** **Recommendation:** display
  subscriptions follow the see predicate (a listener can show its joined
  slice's pan); raw I/Q (`remoteIqVersion`) and TCI I/Q stay controller-only.
- **Q14. `connectedDevices.listeningOn`** lists controlled slices today
  (`StationServer.cpp:7619-7640`). **Recommendation:** keep its content for
  older clients; listening is carried by the new SliceAccess object; how the
  devices list shows it is a UI decision (Task 15/16).
- **Q15. TX applet band source.** `TxApplet::txBand` follows the window's
  receive slice (`TxApplet.cpp:2145-2162`; hosting lambda
  `MainWindow.cpp:1745-1753`). **Recommendation:** follow the transmit-bound
  slice, after reading how Thetis derives TX band from the TX frequency
  (source first); a listened slice never drives TX surfaces either way.
- **Q16. Per-device fairness at full cap** (deep scout P1). **Recommendation:**
  no per-device quota in this plan; the capacity chooser names every
  affected slice and listener.
- **Q17. Hosting desktop capacity questions.** **Recommendation:** the host
  answers Core questions in the same chooser dialog a remote desktop uses
  (`MultiDeviceController`, `MainWindow.cpp:1990-2035`), not a new dialog.

UI questions and JJ's rulings (2026-09-28). Each question keeps the
recommendation it was asked with; JJ's words are quoted exactly as the crew
ledger records them, and "Ruled" is the lead's recorded reading.

- **U1. Where an unseen slice shows.** Recommendation (design): focus its
  existing pane if visible (floating included); else prefer an empty pane;
  else offer a named destination or another pane. Never create a physical
  receiver or change shared tuning to show it.
  JJ: "1 your recommendation but maybe not a floating pan but a layout that
  fits in the single window". Ruled: an unseen slice goes into the main
  window: an existing main-window pan showing it comes forward, else an
  empty main-window pan, else the main window grows to a pan layout that
  fits in the single window, or asks for a named destination; placing a
  slice never opens a new floating pan. Lead confirmation (2026-09-28): ask
  only when no larger single-window layout fits; growing the layout to place
  a slice adds no new slice to any other empty pan.
- **U2. Bottom RX selection and a floating pan.** Recommendation: selecting
  a row whose slice sits in a floating pan raises that floater and updates
  main-window RX; focusing a floater's flag updates the main bottom bar.
  JJ: "1 your recommendation". Ruled: a slice already showing in a floating
  pan brings that floater to the front and switches the main window's RX to
  it; nothing moves, no second copy.
- **U3. Pan background click.** Recommendation (scout): keep today's
  display-only meaning (`PanadapterApplet.cpp:217-234`); only a flag, a
  joined RX tab or explicit Select RX changes window RX.
  JJ: "1 your recommendation". Ruled: a pan background click keeps today's
  display-only meaning (keyboard and scroll focus); slices are chosen by
  flag or tab.
- **U4. Same-pan stacking** of controlled and listened flags. Recommendation:
  keep today's per-pan selected-flag-forward behavior
  (`PanadapterApplet.cpp:118-134,197-213`) for joined flags; foreign
  markers stay dashed.
  JJ: "1 your recommendation". Ruled: same-pan stacking keeps today's rule,
  the selected slice's flag on top; listened slices keep their color with
  "Listening · controlled by ..." text.
- **U5. Local audio presentation.** Recommendation: a listener's flag and RX
  applet show a local volume and mute bound to the device's listen level
  (never `SliceModel::afGain` / `muted`); the controller keeps today's AF
  and mute controls.
  JJ: "1 your recommendation". Ruled: the existing AF slider and mute on any
  slice (controlled or listened) change only what this device hears;
  labeled "Your volume" on a listened slice; nobody's volume or mute changes
  anyone else's audio, the controller's included. Consequence carried into
  Task 6: the controller's AF must become per-device too, not only
  listeners'. The RX applet has no AF slider or mute today
  (`RxApplet.cpp:1190-1191`), so the existing surfaces are the flag's
  (`VfoWidget`) AF slider and mute; no new volume control is added.
- **U6. Listener flag wording and actions.** Recommendation (design): letter
  and Aether color unchanged; text "You control" or "Listening" plus the
  controller's name; Take control and Stop listening reachable from the
  flag and the chooser.
  JJ: "1 your wording sounds fine". Ruled: flag text "You control" (menu:
  Release); "Listening · controlled by <device>" (menu: Take control, Stop
  listening; tuning disabled with "<device> controls this slice"); "TX" as
  today, red on air.
- **U7. Layout change behavior** (G-126). Recommendation: a layout change
  never moves or recreates a listened slice or spends a DDC; empty panes
  offer Choose a slice and New slice instead of adding automatically.
  JJ: "stop listening to slices, its confusinf to hear slices you have no
  visual referance to". Ruled: a listened slice that loses its pan in a
  layout change stops being listened to on this device (with a plain
  notice); principle: a device hears only slices it can see. Controlled
  slices keep today's rehoming into a remaining pan so every heard slice
  stays visible. This supersedes the design's earlier "replacing or hiding
  a view retains listening until explicit leave" and the RX applet tabs for
  hidden joined slices.
- **U8. TX applet showing every letter** (JJ's tentative idea in G-118).
  Recommendation: not in this plan; TX selection stays in today's TX
  controls.
  JJ: "2 but for only slices tgat are activatyed show in the applet".
  Ruled: the TX applet shows a row of slice letter buttons, only for slices
  active on this device (read: slices this device controls, the only ones it
  may transmit on); pressing one selects it for transmit through the
  existing `tx.setTxSlice` behavior, the same for every client (ruling 8.10:
  while keyed it unkeys, then moves). Correction (lead, 2026-09-28): an
  earlier record added "idle only; refused on air"; that was the lead's
  addition, not JJ's ruling, and is withdrawn. Built in Task 11.

## Controller rulings on the open decisions (2026-09-28)

The lead (controller) settled Q1-Q17 as recommended, each consistent with JJ's six approved policies:
Q1 mix listened audio into the device's existing Core-side mix from the per-receiver feed block (before mute, AF undone);
Q2 accept listener silence when the controller's AF is at zero (a pre-AF WDSP tap is a separate change; superseded
later the same day by the lead's ruling that U5 wins, see Task 6);
Q3 listeners hear the slice centered on their own route; Q4 a listener's level starts at the current AF level (new
listeners and the former controller); Q5 the controller cannot Stop listening, the Core refuses and points to Release;
Q6 existing close paths (flag close, removeSlice) act as Release on a slice others listen to; Q7 Take control from a
window that cannot stay on as a listener is refused, the capacity Take stays available; Q8 clearing an idle TX
selection reuses ruling 8.12's close path, drops the former controller's remembered choice, and the new controller's
first key never picks up the taken slice automatically (JJ's policy 5: the new controller selects transmit explicitly);
Q9 no auto-adopt of a released slice; Q10 at radio connect or Core restart with zero slices the Core still creates one
unowned Slice A (Protocol 2 needs a streaming DDC; zero slices remains a valid idle state after release); Q11 listeners
do not survive a Core restart; Q12 no new held-for marks; Q13 listeners get the display, raw I/Q and TCI I/Q stay
controller-only; Q14 connectedDevices.listeningOn keeps its current content; Q15 the TX applet band follows the
transmit slice, after reading how Thetis does it; Q16 no per-device quota in this plan; Q17 the hosting desktop uses the
remote desktop's chooser dialog. U1-U8 went to JJ one at a time and he ruled all eight on 2026-09-28 (above);
Tasks 13-16 are unblocked. U5 adds a consequence to Task 6 and U8 adds the TX applet letter row to Task 11.

## Task 1: Slice incarnation and control revision

Implements: design "Core boundaries and invariants" (incarnation distinct
from the reusable letter; control changes have a revision; stale commands
and confirmations never act on a reused letter or a newer assignment);
deep scout P1 "takeSlice currently checks subject, not a unique object
generation".
Depends on: nothing. Model tier: opus.

Status (2026-09-28): complete on lane `codex/slice-access` in signed
`efb398303` ("feat(slice): give each slice an incarnation and a control
revision"), not yet merged to the trunk. Ledger results: `tst_slice_ownership`
19/19 (4 new), `tst_confirm_step` 80/80 (2 new, red on the trunk before the
change), `tst_station_multi_session` 100/100. The task added
`ConfirmStep::Question::shownRefs`, needed to catch reused ids. The lead
accepted one implementer ruling: a held-for mark moving between devices keeps
the revision, because the owner stays the station device. Finding carried
into Task 4: `incarnation()` reads 0 during removal; if the removal message
needs it, Task 4 adds an accessor.

Today:
- Slice identity on the wire is only the reusable id: `sliceIndex` is the
  mirror's identity (`SliceModel.h:253`, `MirrorPolicy.h:44-52`); ids are
  reused lowest-free (`RadioModel.h:837-848`).
- In-process identity checks use `QPointer<SliceModel>`
  (`SessionCommandDispatcher.cpp:2008-2013`, `:2202-2245`), which a remote
  peer cannot carry.
- Confirmations check the requester still owns each named slice
  (`StationReceivers.cpp:1411-1419`) and `proceedTakeSlice` checks only the
  subject (`StationReceivers.cpp:1652-1654`), so a closed-and-reused letter
  with the same subject passes.
- `SliceOwnership` has no per-slice counter (`SliceOwnership.h:203-214`).

Change:
- `SliceOwnership` assigns each slice an incarnation in `noteSliceAdded`
  (`SliceOwnership.h:106`) and keeps it until `endRemove`
  (`SliceOwnership.cpp:68-77`). Value: a 20-bit random boot nonce shifted
  left 32 plus a 32-bit counter, so it stays below 2^53 (exact as a JSON
  number, link 4.1) and never repeats across Core restarts within practical
  use.
- A per-slice control revision starts at 1 and rises on every change of
  `mark().owner` (in `setMark`, `SliceOwnership.cpp:115-147`), including
  adoption, hold, return, take and release.
- `ConfirmStep::Question` records `{sliceId, incarnation}` for every named
  slice and every choice target; `answerConfirm` and `proceedTakeSlice`,
  `proceedTakeReceiver`, `proceedPanMove`, `proceedTakeBack` refuse with the
  existing changed-since-asked words when an incarnation differs.
- No wire change in this task (Task 4 publishes both values).

Files: `src/core/SliceOwnership.{h,cpp}`, `src/core/session/ConfirmStep.{h,cpp}`,
`src/core/session/StationReceivers.cpp`, `tests/tst_slice_ownership.cpp`,
`tests/tst_confirm_step.cpp`.

Interfaces:
- `struct SliceOwnership::SliceRef { int sliceId = -1; quint64 incarnation = 0; };`
- `quint64 SliceOwnership::incarnation(int sliceId) const;` (0 when not live)
- `quint64 SliceOwnership::controlRevision(int sliceId) const;` (0 when not live)
- `bool SliceOwnership::matches(const SliceRef& ref) const;` (live and same incarnation)
- `SliceRef SliceOwnership::refOf(int sliceId) const;`
- signal `void controlRevisionChanged(int sliceId, quint64 revision);`
- `ConfirmStep::Question::namedRefs` (`QList<SliceOwnership::SliceRef>`) and
  `choiceRefs` alongside the existing `namedSlices` / `choiceTargets`.

Acceptance:
- Close A, add a new A for the same device: incarnations differ; a
  `takeSlice` confirmation asked about the old A is refused as changed and
  closes nothing (fails today at `StationReceivers.cpp:1652-1654`).
- Two slices created in one run never share an incarnation; a Core object
  rebuilt with a new nonce does not reuse the prior run's first value
  (injected nonce in test).
- `setMark` with an unchanged owner does not raise the revision; each owner
  change raises it by exactly 1; `hold` then `returnHeld` raises it twice.
- Every existing `tst_confirm_step` case still passes (78 cases per G-125).

Verification: `tst_slice_ownership`, `tst_confirm_step`,
`tst_station_multi_session` at real load, offscreen.

## Task 2: See, hear and change predicates at every write, media and transmit path

Implements: design "Keep three predicates separate" (extending `ownsSlice`
globally would grant listeners write or TX access); verification bullet 2
(forged writes, verbs and TX attempts refused).
Depends on: Task 1. Model tier: opus.

Today every check is one owner test, written in many places:
- Session property writes: `StationServer::handlePropertyWrite`
  (`StationServer.cpp:5966-5997`) via `sliceRefusal` (`:7605-7617`).
- Settings writes and removals of `Slice<N>/...` keys:
  `sliceSettingsRefusal` (`StationServer.cpp:6502-6528`, called at `:6453`
  and `:6676`).
- Verbs naming a slice: `refusedForAnotherDevice` with `kSliceVerbs`
  (`SessionCommandDispatcher.cpp:1608-1636`), the deferred recheck
  (`:2202-2215`), `tx.setTxSlice` (`:1578-1584`), all through
  `setSliceAccess` (`StationServer.cpp:2548-2550`).
- Receiver commands: `StationReceivers.cpp:394`, `:439`, `:513`, `:689`;
  shared settings `StationSharedSettings.cpp:1270`.
- Mirror delivery: `ownershipAllows` (`StationServer.cpp:7155-7194`).
- Media: `DaemonMediaController::ownsSlice` (`DaemonMediaController.cpp:1373-1376`)
  used at `:653`, `:1387`, `:1398`, `:2571`, `:3027`, `:3078`, `:3147`,
  `:3289`, `:3626`, backed by `StationServer::mediaSessionOwnsSlice`
  (`StationServer.cpp:8270-8279`).
- Transmit: `TxSliceArbiter::requestHandoff(sliceId, requester)` and
  `bindForHolder` owner lookups (`TxSliceArbiter.cpp:55-74`, set at
  `RadioModel.cpp:1857-1859`, which reads `subject()`, so a held slice counts for its absent device); TX marks (`StationTransmitTake.cpp:489-503`).
- Station-side writers: station TCI write gate allows any slice with an
  empty owner (`StationTciController.cpp:57-69`); `TciServer::desktopSliceForReceiver`
  (`TciServer.cpp:1591-1597`); `ContainerButtonDispatcher::sliceFor`
  (`ContainerButtonDispatcher.cpp:105-118`); `MainWindow::desktopSliceAllowed`
  (`MainWindow.cpp:1509-1515`).

Change:
- New `SliceAccessPolicy` (pure, model thread) with three predicates over
  `SliceOwnership` plus a listener set added to `SliceOwnership` here (Task 3
  adds its lifecycle): see = controller or listener; hear = controller or
  listener; change = controller only. The empty-owner exception the station
  TCI gate and hosting desktop rely on stays only for a slice with no
  controller **and no listeners** (boot slices before any device); a
  released slice with listeners is changeable by nobody until Take control.
- Every site above calls the policy by name; no call site compares
  `mark().owner` itself for access (the owner comparisons that decide
  anchors, saving and notices stay).
- Media: split `ownsSlice` into `controlsSlice` (owner mix membership until
  Task 6, raw I/Q, headphone mix of own slices), `hearsSlice` and
  `seesSlice` (display subscriptions, per Q13), each backed by a
  `StationServer` query; behavior for today's peers is unchanged because
  nobody is a listener yet.
- A listener's refusal words (used from Task 4): "Slice %1 is controlled by
  %2. Take control to change it." (name set aside for the wording check).
  Older peers keep `ownedElsewhereReason` (`StationServer.cpp:7196-7210`).

Files: `src/core/session/SliceAccessPolicy.{h,cpp}` (new),
`src/core/SliceOwnership.{h,cpp}`, `src/core/session/StationServer.{h,cpp}`,
`StationReceivers.cpp`, `StationSharedSettings.cpp`,
`SessionCommandDispatcher.{h,cpp}`, `media/DaemonMediaController.{h,cpp}`,
`src/core/StationTciController.cpp`, `src/core/TciServer.cpp`,
`src/gui/containers/ContainerButtonDispatcher.cpp`, `src/gui/MainWindow.cpp`,
`src/core/TxSliceArbiter.cpp`, `src/models/RadioModel.cpp`, CMake test
registration, `tests/tst_slice_access_policy.cpp` (new).

Interfaces:
- `class SliceAccessPolicy { public: static bool maySee(const SliceOwnership&, const QByteArray& device, int sliceId); static bool mayHear(const SliceOwnership&, const QByteArray& device, int sliceId); static bool mayChange(const SliceOwnership&, const QByteArray& device, int sliceId); static bool stationMayChangeUnclaimed(const SliceOwnership&, int sliceId); };`
- `bool SliceOwnership::isListening(const QByteArray& device, int sliceId) const;`
  `QList<QByteArray> SliceOwnership::listenersOf(int sliceId) const;`
  (controller included; populated by Task 3/4)
- `bool StationServer::mediaSessionControlsSlice(quint64 epoch, int sliceId) const;`
  `bool StationServer::mediaSessionHearsSlice(quint64 epoch, int sliceId) const;`
  `bool StationServer::mediaSessionSeesSlice(quint64 epoch, int sliceId) const;`
  (replacing `mediaSessionOwnsSlice`)
- `QString StationServer::changeRefusal(const QByteArray& requester, int sliceId) const;`
  (replacing `sliceRefusal`, same empty-when-allowed contract)

Acceptance:
- A source-scan test lists every function above and fails when a new
  `mark(...).owner ==` / `!=` access comparison appears outside
  `SliceOwnership`, `SliceAccessPolicy`, the anchor code and
  `ReceiveLayoutStore` (the scan is the invariant, not a count).
- With a listener injected into `SliceOwnership` for device B on A's slice:
  B's property write, settings write, settings remove, each `kSliceVerbs`
  verb, `requestStreamCentre`, `requestStreamCtunPinned`, `slice.selectBand`,
  `notch.addAtSlice` and `tx.setTxSlice` are refused and leave every
  property, setting and binding unchanged; A's same writes succeed.
- The station TCI gate refuses a write to a slice with no controller and one
  listener; it still allows a boot slice with no controller and no listeners.
- `bindForHolder(B, ...)` never binds a slice B only listens to, even when
  that slice is B's active receive slice.
- All existing `tst_station_multi_session` foreign-write cases
  (`tests/tst_station_multi_session.cpp:1999-2205`) pass unchanged.

Verification: `tst_slice_access_policy`, `tst_slice_ownership`,
`tst_station_multi_session`, `tst_tx_slice_arbiter`, `tst_daemon_media_controller`,
`tst_station_reason_wording`, offscreen, real load.

## Task 3: Per-device membership and per-device active receive slice

Implements: design "Listeners need ... an authoritative per-device active RX
selection; an owner's global active property cannot represent every
listener's independent choice"; "Selecting another slice on this device
changes the active receive focus ... does not release previously joined
slices or change the device's transmit selection".
Depends on: Task 2. Model tier: opus.

Today:
- One active slice per owner among its own (`SliceOwnership.cpp:216-239`),
  published as `SliceModel::active` (`SliceModel.h:235`); a slice's `active`
  means "its owner's active slice" (`SliceOwnership.h:35-44`).
- Station-level active slice follows the transmit holder's active slice, else
  the most recent choice by any owner (`SliceOwnership.cpp:241-250`), and
  drives once-per-radio duties such as Alex band routing
  (`RadioModel.cpp:1530-1533`) and the FreeDV report (`:13100-13122`).
- `setActiveSliceByIdFor` refuses anything but the caller's own slice
  (`RadioModel.cpp:13154-13160`).

Change:
- Listener lifecycle in `SliceOwnership`: join order kept; the controller is
  always a listener (a new owner joins in `setMark`); leaving never happens
  implicitly on an owner change (the former controller stays, per the
  approved handoff rule), only by explicit stop listening, release, close or
  claims removal.
- Per-device active receive slice over joined slices, separate from
  `activeFor`: `activeRxFor(device)` returns the device's last choice while
  still joined, else its controller-active slice, else its first joined
  slice, else -1.
- `activeFor`, `isActive`, `SliceModel::active` and `stationActiveSlice`
  keep today's controller-only meaning, so a listener's choice never moves
  station-level duties (Alex band, FreeDV report, TX binding).
- Claims removal for one device returns what it touched, for Task 7/8.
- Nothing persisted (Q11).

Files: `src/core/SliceOwnership.{h,cpp}`, `src/models/RadioModel.{h,cpp}`,
`tests/tst_slice_ownership.cpp`.

Interfaces:
- `bool SliceOwnership::join(const QByteArray& device, int sliceId);`
- `bool SliceOwnership::leave(const QByteArray& device, int sliceId);`
- `QList<int> SliceOwnership::joinedBy(const QByteArray& device) const;` (creation order)
- `int SliceOwnership::activeRxFor(const QByteArray& device) const;`
- `bool SliceOwnership::setActiveRx(const QByteArray& device, int sliceId);` (false unless joined)
- `struct SliceOwnership::ClaimsRemoved { QList<int> releasedControl; QList<int> leftListening; };`
  `ClaimsRemoved SliceOwnership::removeClaims(const QByteArray& device);`
- `QList<int> SliceOwnership::unclaimed() const;` (live, no controller, no listeners)
- signals `void listenersChanged(int sliceId);` and `void activeRxChanged(const QByteArray& device);`
- `bool RadioModel::setActiveRxFor(const QByteArray& device, int sliceId);`
  (calls `setActive` too when the device controls it, then `applyActiveSlices`)

Acceptance:
- A controls A0; B joins A0: listeners `[A, B]`; B's `activeRxFor` is A0;
  A's `activeFor` is A0; `stationActiveSlice` unchanged by B's choices.
- B controls B1 and listens to A0, selects A0: `activeRxFor(B) == A0`,
  `activeFor(B) == B1`, `SliceModel` `active` of B1 still true; Alex band
  (fake) still follows the station-level slice, never A0 because of B.
- Owner change A to B on A0 keeps A in listeners and B added; revision +1.
- `removeClaims(B)` on a slice B controls with listener C: controller empty,
  listeners `[C]`, `unclaimed()` excludes it; on a slice only B joined:
  `unclaimed()` includes it.
- `setActiveRx` for a slice not joined returns false and changes nothing.

Verification: `tst_slice_ownership`, `tst_radio_model_3m1b_ownership`,
`tst_pan_active_slice_sync`, offscreen, real load.

## Task 4: Negotiation, SliceAccess mirror and the listen, stop listening, take control and release verbs

Implements: design "Approved behavior" rows Listen in, Take control, Release,
Handoff while TX idle, Handoff while transmitting; "Negotiate the new
slice-access feature explicitly"; "A successful handoff publishes the new
authority without an intermediate destroyed or unowned slice. Do not rebuild
its DSP channel or remove/recreate its audio source"; G-118 rulings.
Depends on: Task 3; Q5, Q6, Q7, Q8 answered. Model tier: opus.

Carried from Task 1 (ledger finding, 2026-09-28): `SliceOwnership::incarnation()`
reads 0 while a slice is being removed. If the removal message this task
publishes needs the removed slice's incarnation, add an accessor that keeps
it readable through removal rather than sending 0.

Today:
- Negotiation: hello features (`StationClient.cpp:2831-2882`, server
  `peerDeclares` at `StationServer.cpp:2971`); capabilities appended per
  feature, `radioAntennaRowsVersion` last before `coreBuildInfo`
  (`StationCapabilities.cpp:343-353`; `surface.json` tail).
- Verb table `SessionCommandDispatcher::verbSpecs` (`SessionCommandDispatcher.cpp:486-825`).
- A device receives `slice:` only for its own slices and `marker:` for the
  rest (`StationServer.cpp:7155-7194`); an owner change swaps forms by
  destroy/create (`:7863-7920`).
- The only "take" is destructive: close then replay (`StationReceivers.cpp:1150-1188`,
  `:1553-1694`); no intact transfer exists (deep scout P1).
- `removeSlice` refuses only the last physical slice
  (`SessionCommandDispatcher.cpp:2014-2023`).
- The media owner mix drops a slice on any owner change
  (`DaemonMediaController.cpp:643-661`, `:1378-1407`) and retires its
  displays.

Change:
- Hello feature `sliceAccess` = 1 (client) and capability
  `sliceAccessVersion` = 1 (Core), sent only to a peer at agreed minor 11
  that declared both `sessionHolder` and `sliceAccess` in hello.
- New mirrored class `SliceAccess`, one object per live slice, key
  `access:<sliceId>`, created on `sliceAdded`, destroyed on `sliceRemoved`,
  sent only to sliceAccess peers (schema included), maintained by a new
  `SliceAccessSet` modeled on `SliceMarkerSet` (`SliceMarker.h:150-168`).
- Delivery: a sliceAccess peer receives `slice:<id>` for every slice it has
  joined (see predicate) and `marker:<id>` for every other; joining or
  leaving swaps the form with the existing destroy/create pattern. Older
  peers are unchanged. Writes from a listener are refused by Task 2.
- `slice.listen`: no allocation of any kind (slice count, stream count and
  DDC mask unchanged); works at full capacity; already joined is an accepted
  no-op.
- `slice.stopListening`: refused from the controller (Q5); leaves; the
  device's active receive slice moves to its next joined slice; a slice left
  with no controller and no listeners closes (the last physical slice keeps
  today's guard until Task 7 lifts it).
- `slice.takeControl`: checks incarnation and expected control revision;
  refused while the slice is the transmit slice of a holder on the air or the
  station freeze holds it (`onAirHolder`, `stationFrozenSlice`,
  `StationServer.cpp:7523-7550`), checked at the moment of mutation; refused
  from/against an older window (Q7); then one `setMark` on the model thread:
  former controller stays a listener, taker joins, TX selection cleared per
  Q8, questions naming the slice dropped (existing `dropQuestionsNaming`),
  TX marks refreshed, a `controlTaken` notice to the former controller. No
  `sliceRemoved` / `sliceAdded`, no WDSP channel change, no
  `DaemonAudioSource` or owner-mix slot change; the media controller's
  `markChanged` hook retires displays only when the device can no longer see
  the slice.
- `slice.release`: controller only; checks incarnation and revision;
  controller cleared, releaser leaves; kept for remaining listeners
  (available to Take control, not adopted per Q9); closed when nobody is
  left (interim last-slice guard until Task 7). TX per Q8. Existing close
  paths from a controller act as release (Q6).
- `setActiveSliceById` from a sliceAccess peer accepts any joined slice
  (`RadioModel::setActiveRxFor`); from an older peer, today's rule.
- One `SliceAccessController` holds the validation and mutation for these
  verbs so Task 10's hosting desktop calls the same code.

Files: `src/core/session/SliceAccessController.{h,cpp}` (new),
`src/core/session/SliceAccessSet.{h,cpp}` (new), `StationServer.{h,cpp}`,
`SessionCommandDispatcher.{h,cpp}`, `StationCapabilities.{h,cpp}`,
`MirrorPolicy.cpp`, `MirrorSchema.cpp` if the class list lives there,
`SessionMessages.cpp` (notice kind), `StationClient.cpp` (hello feature
only; the rest is Task 5), `media/DaemonMediaController.cpp`,
`docs/architecture/2026-09-23-station-link-v1.md` (sections 6.3, 7.1, 7.5,
9.1, notices), `tests/data/link/v1/*`, `surface.json`,
`tests/tst_slice_access_verbs.cpp` (new), `tests/tst_station_multi_session.cpp`,
`tests/tst_link_surface_manifest.cpp`, `tests/tst_mirror_schema.cpp`.

Interfaces (wire):
- Hello `features.sliceAccess`: number, 1.
- Capability `sliceAccessVersion` (`i64`, value 1), appended after
  `radioAntennaRowsVersion` and before `coreBuildInfo`; 0 or absent means no
  feature. Two-key gate: agreed minor >= 11 (`kRadioIdentitySessionProtocolMinor`)
  and `sliceAccessVersion >= 1`.
- Class `SliceAccess`, key `access:<sliceId>`, all outbound:
  0 `sliceId` `i64` constantSnapshot; 1 `incarnation` `i64` constantSnapshot;
  2 `controllerDeviceId` `utf8` (`""` none; the id `connectedDevices` uses);
  3 `controlRevision` `i64`; 4 `listenerDeviceIds` `utf8` (JSON array of
  device ids, join order, controller included); 5 `activeRxDeviceIds` `utf8`
  (JSON array of devices whose active receive slice this is);
  6 `txSelected` `bool` (same rule as the marker's `txSlice`, ruling 5.4a);
  7 `onAir` `bool`.
- Verbs (`command.invoke`, gate `sliceAccessVersion` 1, minor 11):
  - `slice.listen`: `sliceId` `i64`, `incarnation` `i64`; result values
    `controlRevision` `i64`.
  - `slice.stopListening`: `sliceId` `i64`, `incarnation` `i64`.
  - `slice.takeControl`: `sliceId` `i64`, `incarnation` `i64`,
    `controlRevision` `i64`; result values `controlRevision` `i64`.
  - `slice.release`: `sliceId` `i64`, `incarnation` `i64`,
    `controlRevision` `i64`.
- Notice kind `controlTaken` (existing `notice` keys: `byDeviceId`,
  `byName`, `byShortName`, `byKind`, `slices` [{`sliceId`, `letter`,
  `frequencyHz`, `mode`, `band`}], `takeBack` false). Words: "%1 took control
  of slice %2. You are still listening."
- Refusal words (examples, all scanned): "That slice has closed. Choose it
  again from the list."; "Someone else changed who controls slice %1. Look
  again and try once more."; "Slice %1 is transmitting. Take control once it
  stops."; "You control slice %1. Use Release to leave it."
Interfaces (C++):
- `class SliceAccessController : public QObject` with
  `struct Result { bool accepted = false; QString reason; quint64 controlRevision = 0; QList<QByteArray> affected; };`
  `Result listen(const QByteArray& device, SliceOwnership::SliceRef ref);`
  `Result stopListening(const QByteArray& device, SliceOwnership::SliceRef ref);`
  `Result takeControl(const QByteArray& device, SliceOwnership::SliceRef ref, quint64 expectedRevision);`
  `Result release(const QByteArray& device, SliceOwnership::SliceRef ref, quint64 expectedRevision);`
  `Result selectRx(const QByteArray& device, int sliceId);`
  signal `void controlTaken(int sliceId, const QByteArray& fromDevice, const QByteArray& byDevice);`
- `bool StationServer::peerHasSliceAccess(SessionTransport* transport) const;`
- `void StationServer::onSliceAccessChanged(int sliceId);` (form swap per view)

Acceptance (fake radio, three sessions A, B, C with the feature, D older):
- B `slice.listen` on A's A0 at full slice cap and full DDC use: accepted;
  `RadioModel::slices().size()`, active streams and DDC mask unchanged; B's
  view gets `object.destroy marker:0` then `object.create slice:0`;
  `access:0.listenerDeviceIds == [A,B]`.
- B's property write to `slice:0.frequency` is refused with the listener
  words; A's succeeds and B receives the delta.
- B `slice.takeControl` with the current revision: accepted; A0's letter,
  frequency, mode, filter unchanged; no `sliceRemoved`/`sliceAdded`; the
  `RxChannel*` for id 0 and every `DaemonAudioSource` pointer identical before
  and after; A keeps `slice:0` and gets `controlTaken`; A's next frequency
  write is refused.
- Double take: B and C both send `takeControl` with revision r; exactly one
  is accepted, the other refused with the changed words; controller is the
  first; nothing else changes.
- Stale incarnation: A0 closed and a new A0 made; a `takeControl` or
  `listen` carrying the old incarnation is refused and the new A0 untouched.
- Idle TX: A holds transmit unkeyed, A0 is TX-bound, A also has A1: after B
  takes A0, the flag is on A1, `m_chosenTxSlice[A] != 0`, B holds nothing,
  no key occurred (fake MOX never set). A with no other slice: transmit is
  released to nobody.
- On air: A keyed on A0: B's `takeControl` refused with the transmitting
  words; controller, listeners and binding unchanged; after unkey the same
  command succeeds.
- Release with listener: A releases A0 while B listens: controller empty,
  listeners `[B]`, slice live, B's audio membership unchanged, B not given
  control (Q9) even when alone on the Core.
- Release with nobody: A releases A0 and another slice exists: A0 closes and
  its receiver is freed.
- D (older) receives no `SliceAccess` schema or object, no new verb is
  accepted from it ("Update this app ..." style refusal from the gate), and
  its `slice:`/`marker:` traffic is byte-identical to today's fixture.
- `tst_link_surface_manifest` passes after regeneration; the capability is
  last before `coreBuildInfo`.

Verification: `tst_slice_access_verbs`, `tst_station_multi_session`,
`tst_confirm_step`, `tst_link_surface_manifest`, `tst_mirror_schema`,
`tst_station_reason_wording`, `tst_transmit_holder`, `tst_tx_slice_arbiter`,
`tst_daemon_media_controller`, offscreen, real load.

## Task 5: Remote desktop client: access mirror, verbs and read-only listened slices

Implements: design "Hosting desktop, remote desktop and phone have visible
pending/refusal feedback and equivalent action semantics" (the non-visual
half); "Controller-only edits are disabled for a listener".
Depends on: Task 4. Model tier: opus.

Today:
- The remote window treats every `slice:` it receives as its own and
  writable; `RadioModel::removeSlice` on Remote sends the verb
  (`RadioModel.cpp:11890-11916`); client verbs live on `IStationLink`
  (`IStationLink.h:201-225`); feature gates are `remote*Available()`
  (`StationClient.h:865-1310`).
- Foreign slices arrive as markers via `MultiDeviceController`
  (`MainWindow.cpp:1990-2035`).

Change:
- `StationClient` declares `sliceAccess`, reads `sliceAccessVersion`,
  mirrors `SliceAccess` objects into a small model, and exposes the four
  verbs plus the extended select.
- A listened `SliceModel` on the remote side is marked read-only: local
  setters that would send a write are held back with the Core's listener
  words (the Core still refuses; this avoids optimistic UI drift the deep
  scout saw at `BandSlicesModel.swift:166-190`).
- `controlTaken` notice and every refusal reach `MultiDeviceController`'s
  existing `refusal` path (`MainWindow.cpp:1996-1998`) as toasts; no new
  visual design.

Files: `src/core/session/StationClient.{h,cpp}`, `IStationLink.h`,
`src/core/session/SliceAccessMirror.{h,cpp}` (new),
`src/models/RadioModel.{h,cpp}`, `src/gui/MultiDeviceController.{h,cpp}`,
`tests/tst_remote_window_harness.cpp`, `tests/tst_slice_access_client.cpp` (new).

Interfaces:
- `bool StationClient::remoteSliceAccessAvailable() const;` (agreed minor >= 11
  and `sliceAccessVersion >= 1`)
- `IStationLink::CommandOutcome requestListen(int sliceId, quint64 incarnation);`
  `requestStopListening(int sliceId, quint64 incarnation);`
  `requestTakeControl(int sliceId, quint64 incarnation, quint64 controlRevision);`
  `requestRelease(int sliceId, quint64 incarnation, quint64 controlRevision);`
  (default implementations return not-sent with the update words, as
  `requestSelectBand` does at `IStationLink.h:225`)
- `class SliceAccessMirror : public QObject` with
  `struct Entry { int sliceId; quint64 incarnation; QString controllerDeviceId; quint64 controlRevision; QStringList listeners; QStringList activeRx; bool txSelected; bool onAir; };`
  `std::optional<Entry> entry(int sliceId) const; bool controlledHere(int sliceId) const; bool listeningHere(int sliceId) const;`
  signal `void changed(int sliceId);`
- `bool SliceModel::isReadOnlyListener() const;` set only by the remote
  mirror (not mirrored, not a Q_PROPERTY on the wire).

Acceptance:
- Against a loopback Core: listen, take, release and stop listening round-trip
  and the mirror reflects each within one delta.
- A listened slice's frequency setter sends nothing and reports the listener
  words; after Take control it sends.
- A window without the feature (Core without `sliceAccessVersion`) never
  sends a new verb.
- `controlTaken` produces one refusal/notice toast on the former controller.

Verification: `tst_slice_access_client`, `tst_remote_window_harness`,
`tst_multi_device_screens`, offscreen, real load.

## Task 6: Per-listener audio fan-out with independent local level and mute

Implements: design "Per-listener gain/mute cannot use the shared SliceModel's
AF or muted properties"; "verify its zero-gain behavior and stream capacity";
"Preserve bounded audio resources, clear admission failures,
reconnect/session fencing, and remote audio clock behavior"; "DSP callback
code must never traverse mutable listener collections"; "Host speakers and
remote playback must obey the same per-device listening policy"; verification
bullet 1 (continuous audio after handoff, independent volume and mute).
Depends on: Task 4; ruling Q1 (mechanism), Q3, Q4; JJ's ruling U5 and the
lead's ruling that U5 supersedes Q2.
Model tier: opus.

U5 consequence (JJ 2026-09-28, "1 your recommendation"; ledger finding):
the existing AF slider and mute on any slice change only what this device
hears, "the controller's included". So the controller's AF and mute become
per-device too, not only a listener's level:
- The controller's slider and mute set the controller's own hearing only;
  they never change a listener's sum or the local output of another device.
- A handoff carries no device's level to another: the former controller
  keeps its level as a listener (Q4 seeds it from what it heard), and the
  new controller keeps the level it had as a listener instead of inheriting
  the former controller's AF.
- Ruling (lead, 2026-09-28, settles Q2 vs U5): U5 wins. A listener's audio
  must never depend on the controller's AF, including AF at zero. The
  listener feed is taken before the controller's AF: before WDSP's panel
  gain (`SetRXAPanelGain1`, where AF is applied today), or an equivalent
  that never divides by the AF. The AF-undo route (which returns 1.0 at or
  below 0.001, `AudioEngine.cpp:2040-2053`) does not meet this. The
  controller's own path stays as it is today. Read the WDSP source and
  Thetis's callsite before choosing where the feed is taken, change no DSP
  parameter or constant, and name the chosen point in the task report.

Today:
- Owner mixes sum only a device's own slices, each at the slice's shared
  gain, pan, mute and route (`MasterMixer.h:299-330`), masks set from
  `ownsSlice` (`DaemonMediaController.cpp:1378-1407`).
- The local output plays the station device's slices
  (`RadioModel.cpp:1196-1214`, `AudioEngine.h:564-571`).
- The receiver tap is after the MOX gate, before mute/pan, AF undone
  (`AudioEngine.h:234-247`, `AudioEngine.cpp:2055-2080`), one tap per
  (device, slice), eight Core-wide.
- Blocks are read through the per-slice view word (`AudioEngine.cpp:2159-2163`)
  and the mix (`:2226`) and taps (`:2264`).

Change (as recommended in Q1; if JJ picks the tap route instead, this task is
re-planned before it starts):
- Step 1, evidence first: a test measures today's tap and owner-mix output
  at AF 0, AF 1 and AF max, and counts tap slots under four devices with
  receiver streams, recording both results in the task report.
- Listen sums: each owner mix slot gets a listen mask and per-slice level and
  mute held in atomics; the local output gets the same for the station
  device. In the drain, a listened slice contributes its AF-undone,
  pre-mute block times the device's level, centered, into that device's
  speakers sum (Q3), with its own mute ramp; the shared `SliceModel`
  `muted`, `afGain`, `audioPan` and `outputRoute` are never read for it. The
  MOX gate still silences a TX-bound slice for everyone.
- The controller keeps today's path. On handoff the former controller moves
  from its controlled sum to its listen sum in the same drain period with
  its level seeded per Q4, so audio is continuous; the new controller moves
  the other way.
- Levels are set per (device, slice incarnation); a level command for a
  stale incarnation is refused. A remote device sets its level with a verb;
  the hosting desktop calls the same controller in-process (Task 10).
- Membership changes publish new masks from the main thread only
  (`listenersChanged`, `markChanged`); a device's session ending clears its
  slot as today (`DaemonMediaController.cpp:1360-1371`).

Files: `src/core/AudioEngine.{h,cpp}`, `src/core/audio/MasterMixer.{h,cpp}`,
`src/core/session/media/DaemonMediaController.{h,cpp}`,
`src/core/session/SliceAccessController.{h,cpp}`, `SessionCommandDispatcher.{h,cpp}`,
`src/models/RadioModel.cpp`, link and media documents,
`tests/tst_audio_engine_owner_mix.cpp`, `tests/tst_slice_listen_audio.cpp` (new),
`tests/tst_daemon_media_controller.cpp`, `tests/tst_slice_audio_view_race.cpp`.

Interfaces:
- `void AudioEngine::setOwnerMixListen(int slot, int sliceId, float level, bool muted);`
  `void AudioEngine::clearOwnerMixListen(int slot, int sliceId);`
  `void AudioEngine::setLocalListen(int sliceId, float level, bool muted);`
  `void AudioEngine::clearLocalListen(int sliceId);` (main thread; atomics)
- `MasterMixer::OwnerOutput` gains `std::uint32_t listenMask{0};` and
  `const float* listenLevels{nullptr};` (32 entries, level 0 when muted,
  ramped by the mixer) and the mixer keeps per-owner, per-slice ramp state
  owned by the audio thread.
- Verb `slice.setListenLevel` (gate `sliceAccessVersion` 1, minor 11):
  `sliceId` `i64`, `incarnation` `i64`, `level` `f64` (0.0 to 1.0),
  `muted` `bool`. Refused for a slice the device does not hear.
- `SliceAccessController::Result setListenLevel(const QByteArray& device, SliceOwnership::SliceRef ref, double level, bool muted);`

Acceptance:
- A controls A0, B listens: the controller muting A0 (`SliceModel::muted`)
  or lowering AF leaves B's sum unchanged (to 1e-6); B muting leaves A's
  sum and the local output unchanged.
- U5 over Q2: with the controller's AF at 0 and at maximum, B's level is
  unchanged (to 1e-6) from its level at the starting AF; the controller's
  own sum follows its AF as today.
- Handoff under a running drain: B takes A0; neither A's nor B's sum has a
  gap longer than one period or a level step larger than the seeded level
  difference; no owner-mix slot is released or acquired; no receiver stream
  context changes.
- Four devices each listening to all five slices: no new stream, no
  admission refusal; `ownerMixCount()` unchanged.
- ThreadSanitizer (`build-selector-asan` or a TSan build) run of the race
  test: join/leave/level churn on the main thread while blocks flow reports
  nothing, and the DSP path takes no lock (source scan of the drain).
- A level for a stale incarnation is refused and changes nothing.
- Hosting desktop listening to a device's slice plays it locally at the
  host's level; the device's own mute does not silence the host.
- U5: the controller moving its AF slider or muting changes only its own
  sum; every listener's sum and the host's local output are unchanged (to
  1e-6).
- U5: B takes A0 from A. Neither device's level changes at the handoff: A
  hears A0 at the level it had, B at the level it had as a listener; the
  slider each device shows reads its own level.

Verification: `tst_slice_listen_audio`, `tst_audio_engine_owner_mix`,
`tst_audio_engine_slice_tap`, `tst_slice_audio_view_race` (TSan),
`tst_daemon_media_controller`, `tst_link_surface_manifest`, offscreen,
real load.

## Task 7: Zero-slice Core safety audit and fixes

Implements: design "Zero physical slices is a supported idle Core state.
Audit all last-slice guards and first-slice indexing, active RX/TX
fallbacks, saved layout, display demand, DDC retirement, audio taps, RADE and
external diversity. Closing the last slice must leave valid empty state and
a working New slice path"; approved Release rule including the last physical
Core slice; verification bullet 4.
Depends on: Task 4; Q10. Model tier: opus.

Today (each item is audited and fixed or proven safe with a test):
- Last-slice guards: `RadioModel::removeSliceImpl` (`RadioModel.cpp:11927-11935`),
  `SessionCommandDispatcher::handleRemoveSlice` (`:2014-2023`),
  `StationServer::closeSliceFor` (`StationServer.cpp:7744-7748`).
- TX fallback indexes position 1 when the victim is at 0
  (`RadioModel.cpp:11964-11967`): out of range with one slice.
- `TxSliceArbiter::syncToSliceList` with an empty list, and keying with no
  transmit slice (must refuse, never key).
- Active fallbacks: `applyActiveSlices` (`RadioModel.cpp:13100-13122`),
  `activeSlice()` readers (40 uses of `m_activeSlice` in `RadioModel.cpp`,
  including Alex band `:1530-1533`), `MainWindow::activeSliceForWindow`
  (`MainWindow.cpp:1517-1529`), `TxApplet::activeSliceForControls`
  (`TxApplet.cpp:2158-2162`).
- First-slice indexing: `RadioModel.cpp:7684`, `:9218-9230`
  (`installReceiveFallbackSlice`, called at `:9267` and `:9416`), `:14304`;
  `TciServer.cpp:3418`; `SpectrumOverlayPanel.cpp:975`;
  `DspOptionsPage.cpp:387`; `server_main.cpp:664` (test-only).
- Radio connect creates A when empty (`RadioModel.cpp:14159-14163`);
  Protocol 2 connect watchdog needs a DDC frame (`P2RadioConnection.cpp:768-772`);
  after connect any accepted datagram keeps the link
  (`P2RadioConnection.cpp:2977-3006`); Protocol 1 always streams one
  receiver (`P1RadioConnection.cpp:3055-3075` watchdog).
- Saved layout refuses zero slices (`ReceiveLayoutStore.cpp:140-144`).
- DDC retirement: `retireStream`, `syncReceiverToStream`,
  `requestDdcAssignment` (`RadioModel.cpp:12033-12048`); display demand and
  wideband demands (`:11988-11994`); media displays
  (`retireSliceDisplays`, `DaemonMediaController.cpp:648-655`).
- Audio: `publishSliceAudioView` with no slices (`RadioModel.cpp:11008-11027`),
  owner masks 0, `drainMixes` with no barrier member.
- RADE target (`RadioModel.cpp:11958-11964`) and external diversity on
  Slice A (`:11939-11944`, `:18433-18458`, `:26472`, `:26551`).
- Admission step 4 creates a slice for a device with none
  (`StationServer.cpp:7727-7734`); lone-device adoption (Q9).

Change:
- The last-slice guards give way to the claims rule: a slice closes when it
  has no controller and no listeners, whatever the count; the remote side's
  existing exception (`m_stationMayCloseLastSlice`) covers the window.
- Each unsafe item above is fixed at its cause; each safe item gets a
  zero-slice case in the new test.
- Q10's connect and restart rule implemented as ruled.
- New slice from zero: `addSlice`/`addSliceOnPan` produce a working slice
  with a stream, WDSP channel active, audio view present, TX binding
  re-established by `syncToSliceList`.

Files: `src/models/RadioModel.{h,cpp}`, `src/core/TxSliceArbiter.cpp`,
`src/core/session/StationServer.cpp`, `SessionCommandDispatcher.cpp`,
`src/core/ReceiveLayoutStore.cpp`, `src/core/TciServer.cpp`,
`src/gui/SpectrumOverlayPanel.cpp`, `src/gui/setup/DspOptionsPage.cpp`,
`src/gui/MainWindow.cpp` (null paths only), `tests/tst_zero_slice_core.cpp` (new).

Interfaces:
- `bool RadioModel::closeUnclaimedSlice(int sliceId);` (Local role; closes
  only when `SliceOwnership` reports it unclaimed; saves its settings as
  `removeSliceImpl` does)
- `bool RadioModel::hasTransmitSlice() const;` used by the keying gate
  refusal "There is no slice to transmit on. Add a slice first."

Acceptance:
- One device releases the only slice with nobody listening: slice count 0;
  no crash (ASan build); DDC mask 0; audio views all absent; owner masks 0;
  `activeSlice() == nullptr`; `TxSliceArbiter::txBoundSliceId() == -1`;
  a key request is refused with the plain words and the fake MOX never set.
- From zero, `addSlice` gives a slice that streams (fake radio frames reach
  its channel), has an audio view, gets the TX binding, and a remote view
  receives `object.create`.
- Protocol 2 fake connection at zero slices mid-session stays Connected past
  the established-silence window with status datagrams only; Protocol 1
  keeps streaming its first receiver.
- Radio reconnect and Core restart at zero slices follow Q10 exactly as
  ruled.
- External diversity running on A stops cleanly when A closes at zero and
  is not restarted on a later new slice unless asked; RADE target cleared.
- Every first-slice site listed returns a safe value at zero (one case each).

Verification: `tst_zero_slice_core` (normal and ASan builds),
`tst_slice_access_verbs`, `tst_p2_ddc_mask_ownership`, `tst_tx_slice_arbiter`,
`tst_rade_rx_multislice_routing`, `tst_suspended_stream_has_no_receiver`,
offscreen, real load.

## Task 8: Reconnect grace expiry and explicit leave with absence-generation fencing

Implements: approved rule "Missing device: preserve the existing three-minute
reconnect grace. At expiry, remove the absent device's control/listening
claims. Keep slices for remaining listeners; close those with nobody left";
"Expiry acts on the matching absence generation, so an old timer cannot
release a replacement session's work. Explicit leave releases immediately";
"A subsequent reconnect restores only claims the device still has; it cannot
steal back transferred control"; verification bullet 4.
Depends on: Task 7; Q12. Model tier: opus.

Today:
- Grace is `kGraceMs = 180000` (`DeviceSessionRegistry.h:97-98`); drop marks
  away (`DeviceSessionRegistry.cpp:131-154`); `expireAway` frees the place
  and emits `graceEnded(deviceId)` synchronously (`:208-230`), also from
  inside `admit` (`:71-76`); the timer re-arms on `changed`
  (`StationServer.cpp:2716-2723`, `:3238-3249`).
- `graceEnded` runs `saveTakenSlicesFor` then `releaseDeviceSlices`
  (`StationServer.cpp:2008-2016`) and releases transmit (`:2017-2020`);
  explicit leave and token windows release at once (`:3634-3668`).
- `releaseDeviceSlices` closes owned slices when another device holds a
  place, else holds them for the device (`StationServer.cpp:7828-7861`).
- The signal carries no generation, so the release acts on whatever the
  device owns at that moment.

Change:
- Each away period gets an absence generation (a registry counter stamped at
  `sessionEnded(Dropped)`); `graceEnded` carries it; the release handler
  proceeds only if the device has no entry again, or its entry's current
  away generation still equals it (a same-device return clears it).
- `releaseDeviceClaims(deviceId, generation)` replaces `releaseDeviceSlices`
  for expiry, leave, token end, revoke and fifth-device replacement:
  `SliceOwnership::removeClaims`; each slice the device controlled keeps
  running with no controller if listeners remain, else closes (saved for a
  paired device's layout as today, without a control claim); no new holds
  (Q12); C-Tune pins cleared as today (`clearStreamCtunPinsAnchoredBy`).
- Take control of an away device's idle slice is allowed during grace
  (Task 4 rules; its holder is unkeyed by the fence); on return the device
  has only what it still holds (listener of that slice).
- Restoring a returning device's saved layout (`placeSlicesForAdmission`,
  `StationServer.cpp:7670-7736`) never displaces a current controller or
  listener and never recreates a control claim on a slice that still exists.
- Lead ruling (2026-09-29), a deviation from "token end" above: a token
  window alone on the Core keeps today's rule. Its claims go, and its slices
  pass to nobody and stay, so the window signing in again (with the token,
  or with the key it enrolled) adopts them. Closing them would lose the
  operator's slice at first pairing. With another device on the Core the
  claims rule applies (close with nobody left, nothing saved).
- Lead ruling (2026-09-29), Amendment 8a below: an away device's slices stay
  in the take-a-receiver choices (the take closes them); only the list of
  slices offered to close leaves them out.

Amendment 8a (JJ approved, 2026-09-28 night). Slices held for away devices
never count in:
- the preselector decision. `RadioModel::republishAlexAdcSlices`
  (`RadioModel.cpp:18936`) feeds every slice on an ADC into the per-chain
  filter choice. A held slice on another band then forces the chain to
  BYPASS for the connected device's pan (JJ's bench: a re-paired computer's
  pan went to BYPASS because its old key's slices sat on another band).
  The choice uses only slices that are not held for an away device.
- several-devices confirmations. `ReceiverPlanner` builds the disturbed list
  (`ReceiverPlanner.cpp:59`, `planWindowMove`) and the choice lists
  (`:101`, `:174`) that a confirmation names. A slice held for an away device
  is neither named as disturbed nor offered as a choice to close, and a
  change that would disturb only held slices applies without asking (JJ's
  bench: every attenuator step asked for confirmation naming away devices).
  What happens to the held slice itself is unchanged: it is released at
  grace expiry by this task's main change.

Amendment 8a acceptance:
- A holds A0 on 20 m and drops; B is connected with B0 on 40 m on the same
  ADC; during grace, B's chain is not bypassed on account of A0, and
  `RadioModel::panBypassState` for B's pan reports not bypassed.
- The same set-up without the drop (A connected): the filter choice is as
  today (A0 counts).
- B changes the attenuator or moves its pan while A is away: no
  confirmation names A, and the change applies at once.
- The same change while A is connected and disturbed: the confirmation names
  A as today.
- A returns within grace: A0 counts again in both decisions from the moment
  A's hold returns to A.

Files: `src/core/session/DeviceSessionRegistry.{h,cpp}`,
`src/core/session/StationServer.{h,cpp}`, `StationReceivers.cpp` (notices),
`tests/tst_device_session_registry.cpp`, `tests/tst_station_multi_session.cpp`,
`tests/tst_slice_claims_expiry.cpp` (new).

Interfaces:
- `quint64 DeviceSessionRegistry::Entry::awayGeneration` (0 while listening)
- signal `void DeviceSessionRegistry::graceEnded(const QByteArray& deviceId, quint64 awayGeneration);`
- `bool DeviceSessionRegistry::isCurrentAbsence(const QByteArray& deviceId, quint64 awayGeneration) const;`
- `void StationServer::releaseDeviceClaims(const QByteArray& deviceId, std::optional<quint64> awayGeneration);`
  (`nullopt` for leave, token end, revoke and replacement)

Acceptance (injected clock):
- A controls A0 and B listens; A drops; at 180 s A0 keeps running, no
  controller, listeners `[B]`, B's audio uninterrupted, `graceEnded` also
  releases transmit as today.
- A controls A0 alone and drops; at 180 s A0 closes; if it was the only
  slice the Core reaches zero slices (Task 7 state) and A's layout is saved.
- A drops at t0, returns at t0+170 s, drops again at t0+175 s: the first
  absence's timer firing at t0+180 s does nothing; the second expires at
  t0+355 s.
- A drops; B takes A0 at 60 s; A returns at 120 s: A is a listener of A0,
  B controls it, A's attempt to write is refused.
- Explicit `session.leave` applies the same rule immediately.
- A reconnect inside `admit` after the deadline (timer not yet fired)
  expires first, then admits fresh, and the handler acts once.

Amendment 8a adds `src/models/RadioModel.cpp` (the preselector feed),
`src/core/session/ReceiverPlanner.{h,cpp}` (disturbed and choice lists) and
their tests (`tests/tst_confirm_step.cpp`, the preselector cases in
`tests/tst_station_multi_session.cpp`) to the files above.

Verification: `tst_slice_claims_expiry`, `tst_device_session_registry`,
`tst_station_multi_session` (grace cases at `:740-805`, `:2619-2632`),
`tst_transmit_holder`, and for Amendment 8a `tst_confirm_step` and the
preselector cases, offscreen, real load.

## Task 8b: Device identity clarity

Implements: JJ's ruling (2026-09-28 night) "the Core's device list names each
desktop profile; This Core can remove a token-enrolled device". Root cause
from JJ's bench: a default-profile window and a `--profile` window on the
same computer are two devices with two keys and the same name
("MacBook-Pro"), so neither the device list nor a refusal told them apart,
and the older key's held slices stayed with no way to remove it.
Depends on: Task 8 (its claims release is what removal relies on).
Model tier: opus. Pairing-security change: scoped review before merge.

Today:
- A desktop sends `ClientDeviceIdentity::machineName()` and
  `machineShortName()` (`src/core/security/ClientDeviceIdentity.cpp:50-68`,
  from `QSysInfo::machineHostName()`; callers `MainWindow.cpp:2250-2251`,
  `GuiConnectionController.cpp:784`). The profile is not part of the name,
  though each profile has its own key
  (`ClientDeviceIdentity::forThisProfile`, from
  `AppSettings::resolveConfigDir(AppSettings::profileOverride())`).
- `StationDevicesFacade::revoke` refuses a token-enrolled device while the
  pairing token is active ("Stop accepting the pairing token first, then
  remove this computer.", `StationDevicesFacade.cpp:237-240`).
  `station.retireToken` exists (`StationDevicesFacade.cpp:278-298`,
  `SessionCommandDispatcher.cpp:826`, `StationControlCommands.cpp:345`) but
  nothing on This Core offers it next to that refusal.

Change:
- A desktop running with a profile other than the default sends a name that
  carries the profile in plain words (for example "MacBook-Pro (radxa)").
  The default profile's name is unchanged. The Core's device list, the
  refusal wording and the chooser show that name.
- On This Core's device list, removing a token-enrolled device while the
  token is active offers, in plain words, to stop accepting the pairing token
  and then remove the device, as one confirmed action. It uses
  `station.retireToken` then `devices.revoke`, with every existing guard kept
  (never lock the owner out: `retireToken` still refuses with no other
  paired device; `revoke` still refuses the last way in).
- Removing a device releases its claims at once through Task 8's
  `releaseDeviceClaims` (as revoke does).

Files: `src/core/security/ClientDeviceIdentity.{h,cpp}`,
`src/gui/setup/RemoteStationPage.{h,cpp}` and/or
`src/gui/setup/ThisCorePage.{h,cpp}` (whichever hosts the device list),
`src/gui/GuiDesktopStationRuntime.cpp`,
`src/core/session/StationDevicesFacade.{h,cpp}`,
`tests/tst_client_device_identity.cpp` (new), `tests/tst_station_devices.cpp`,
`tests/tst_multi_device_screens.cpp`.

Interfaces:
- `static QString ClientDeviceIdentity::machineName(const QString& profile);`
  and `machineShortName(const QString& profile)` (an empty profile gives
  today's name).
- `DeviceAdminResult StationDevicesFacade::retireTokenAndRevoke(const QString& deviceId);`
  (the safe path: both steps under the existing guards, nothing changed when
  either refuses).

Acceptance:
- Two profiles on one computer appear as two plainly different names in the
  Core's device list and in a refusal naming the other device.
- The default profile's name is byte-for-byte today's.
- A token-enrolled device is removed from This Core in one confirmed action
  while the token was active: the token no longer works, the device is gone,
  its slices are released or closed per Task 8, and another paired device
  still signs in.
- With no other paired device, the action is refused with the existing plain
  words and nothing changes (token still active, device still paired).
- No user-visible string names a token, key or profile setting in developer
  terms.

Verification: `tst_client_device_identity`, `tst_station_devices`,
`tst_station_multi_session` (removal releases claims), offscreen captures of
the device list in `tst_multi_device_screens`; the scoped pairing-security
review before merge.

## Task 9: Capacity refusals offer existing slices; close-then-apply paths keep their victims

Implements: design "When the last available slice or hardware receiver is in
use, the chooser still permits Listen in and intact Take control ... New
slice reports the actual resource constraint and offers existing slices to
use. Moving an entire receiver or closing others to create a different
slice remains a distinct capacity action: the confirmation must name every
affected slice and listener, recheck the current set, and leave everything
intact on refusal"; verification bullet 5; G-125 remainder ("direct
callbacks during closing and non-capacity restoration failures can still
leave a partial result").
Depends on: Task 8; Q16. Model tier: opus.

Today:
- Add at capacity: the Core refuses and, for a feature peer, asks
  `takeSlice` / `takeReceiver` (`StationReceivers.cpp:617-672`); the words
  name the resource (`sliceCapReason`, receiver-full reason).
- Bounded preflight for Add, PanMove and Take-back landed (G-125,
  `6848a1d44`, `c54147ae9`): `planAddAfterClosing`,
  `panMoveFitsAfterClosing`, `planRestoreAfterClosing`
  (`ReceiverPlanner.h:100-117`).
- `closeForTake` (`StationReceivers.cpp:1150-1165`) closes before
  `applyHeld`; notices go to each closed slice's subject only
  (`tellTaken`, `:1166-1188`); listeners do not exist in the question.

Change:
- A refused `addSlice` / `addSliceOnPan` from a sliceAccess peer carries the
  live slices it could use instead as result values; the reason names the
  resource.
- Take questions to a sliceAccess peer list, per affected slice, its
  listeners; proceed rechecks the listener set as well as owner and
  incarnation, and asks again if it grew.
- A capacity close notifies every listener of a closed slice, not only its
  controller.
- The remaining partial paths (callbacks during `closeForTake`,
  restoration failures that are not capacity) get deterministic failure
  injection; the fix restores or never closes the victims. General rollback
  beyond these paths is not claimed.

Files: `StationReceivers.cpp`, `ReceiverPlanner.{h,cpp}`,
`ConfirmStep.{h,cpp}`, `StationServer.cpp`, `SessionCommandDispatcher.cpp`,
link document sections 7.3 and 9.1, fixtures, `tests/tst_confirm_step.cpp`,
`tests/tst_station_multi_session.cpp`.

Interfaces:
- `command.result` values for a capacity-refused add to a sliceAccess peer:
  `usableSlices` `utf8` (JSON array of {`sliceId`, `incarnation`, `letter`,
  `controllerDeviceId`}).
- `confirm.request` `affected` entries gain `listenerDeviceIds` (array of
  strings) for sliceAccess peers only.
- `ReceiverPlanner::Disturbed::listeners` (`QList<QByteArray>`).

Acceptance:
- Full HL2 fake pool (two user DDCs, five slices): B's Add is refused naming
  receivers and lists every live slice; B's `slice.listen` on one of them
  then succeeds without any close.
- A take question shows A0 with listeners `[A, C]`; C leaves before proceed:
  proceed still valid; D joins before proceed: asked again, nothing closed.
- A closed-by-take slice's listener C gets a notice naming it.
- Injected failure inside `closeForTake`'s callbacks and in a non-capacity
  restore: every victim still live with its incarnation, no notice claims a
  take, the refusal is honest.
- The G-125 fake-radio regression (A four slices on DDC 0, C one on DDC 1)
  still passes.

Verification: `tst_confirm_step`, `tst_station_multi_session`,
`tst_new_pan_is_its_own_receiver`, `tst_station_reason_wording`,
`tst_link_surface_manifest`, offscreen, real load.

## Task 10: Hosting desktop through the same validated operations

Implements: design "The hosting desktop must call the same validated Core
operations as remote clients. Its direct RadioModel shortcut must not bypass
capacity questions, TX protection, generation checks or listener cleanup";
deep scout P1 (host has no take path); verification bullet 6 (core half).
Depends on: Task 9; Q6, Q17. Model tier: opus.

Today (direct `RadioModel` calls from the hosting window):
- Flag close and menu remove: `m_radioModel->removeSlice(idx)`
  (`MainWindow.cpp:3114-3128`).
- Flag activation and other selections: `setActiveSliceByIdFor(stationDevice, id)`
  (`MainWindow.cpp:3140-3150`, `:3341-3371`, `:4461`, `:7626`).
- `+RX` and layout growth: `addSliceOnPan` (`MainWindow.cpp:4417-4422`,
  `:4692`, `:8580`, `populatePanSlices` `:13949-13958`); capacity refusal is a
  toast with no chooser (`MainWindow.cpp:6706+`, `RadioModel.cpp:12081-12142`).
- The host is the station device in the registry
  (`StationHost.cpp:145-175`) with no transport; the confirm step and
  refusals assume a `SessionTransport` (`StationReceivers.cpp:1379-1385`).

Change:
- An in-process entry on `StationServer` runs a `command.invoke` as the
  station device through the same dispatcher, checks, confirm step and
  `SliceAccessController`, answering and asking through callbacks instead of
  a transport.
- The host's close, select, add and layout-add paths use it; its capacity
  questions reach the existing chooser dialog (Q17); its refusals reach the
  existing toast. Q6 governs close.
- Host listen, stop listening, take, release and level entries exist for
  Task 13 to call; no new widget in this task.
- Without a `StationServer` (plain local desktop, no hosting), today's direct
  path stays; the result is the same single-operator behavior.

Files: `src/core/session/StationServer.{h,cpp}`,
`src/core/session/SessionCommandDispatcher.cpp`, `src/gui/MainWindow.{h,cpp}`,
`src/gui/DesktopStationController.{h,cpp}`, `src/gui/MultiDeviceController.{h,cpp}`,
`tests/tst_hosting_slice_operations.cpp` (new), `tests/tst_multi_device_screens.cpp`.

Interfaces:
- `using StationAnswer = std::function<void(const SessionMessage&)>;`
  `void StationServer::invokeAsStationDevice(const SessionMessage& invoke, StationAnswer answer, StationAnswer question);`
- `class HostingSliceActions : public QObject` (in `src/gui/`) with
  `void listen(int sliceId); void stopListening(int sliceId); void takeControl(int sliceId); void release(int sliceId); void select(int sliceId); void addOnPan(const QString& panId); void setListenLevel(int sliceId, double level, bool muted);`
  and signals `refused(QString)`, `question(SessionMessage)`, `pending(int sliceId, bool)`.

Acceptance:
- Host `+RX` at full cap produces the same `takeSlice`/`takeReceiver`
  question a remote peer gets (compared field by field) and nothing closes
  before proceed.
- Host take of a remote device's on-air slice is refused with the same words
  as a remote take.
- Host close of its slice with a remote listener keeps the slice for the
  listener (Q6); with nobody, closes it, reaching zero if it was the last.
- A stale host selection or take (reused letter) is refused.
- The plain local desktop (no hosting) keeps today's behavior in
  `tst_pan_active_slice_sync` and `tst_slice_rehome_on_layout_shrink`.

Verification: `tst_hosting_slice_operations`, `tst_multi_device_screens`,
`tst_pan_active_slice_sync`, `tst_slice_rehome_on_layout_shrink`,
`tst_station_multi_session`, offscreen, real load.

## Task 11: Transmit surfaces stay on the transmit slice

Implements: design "TX remains explicitly bound to the selected transmit
slice even when a different slice is selected for receive. The current
active-RX-dependent TX applet bindings need a safety audit"; "RX control
never implicitly keys or grants transmit" (verification bullet 3); JJ's
ruling U8 (2026-09-28, "2 but for only slices tgat are activatyed show in
the applet"): the TX applet letter row.
Depends on: Task 10; Q15; U8. Model tier: opus.

Today:
- `TxApplet::txBand()` and `activeSliceForControls()` follow the window's
  receive slice (`TxApplet.cpp:2145-2162`); the hosting handler returns
  `activeSliceForWindow` (`MainWindow.cpp:1745-1753`); `setCurrentBand` from
  the active slice (`:1754-1756`).
- Flags show TX only for the host-held TX slice (`MainWindow.cpp:1719-1729`,
  `:3423-3432`); per-pan TX pill uses the pan's slice and transmit authority
  (the scout cited `MainWindow.cpp:4613-4635` at `6848a1d44`; that range has moved, so locate the pill wiring again before editing).
- Station-level duties follow controllers only after Task 3.

Change:
- Read Thetis's TX band derivation first (source first) and cite it.
- TX band, per-band TX power, TX antenna and every TX-applet control resolve
  the transmit-bound slice, never a listened slice; per Q15 whether a
  controlled non-TX receive slice still drives them.
- A test drives every TX-applet binding with window RX on a listened slice
  and TX on a controlled one.
- U8 letter row: the TX applet shows one letter button per slice this
  device controls, in its Aether color, and none for a listened or foreign
  slice. The row follows take, release and handoff. Pressing a letter
  selects that slice for transmit through the existing `tx.setTxSlice`
  behavior, the same for every client (ruling 8.10): unkeyed it moves at
  once; while keyed the transmitter unkeys through the unkey gate and the
  flag moves once that is confirmed (`SessionCommandDispatcher.cpp:1585-1590`).
  A remote window sends `tx.setTxSlice`; the hosting desktop calls the same
  validated operation (Task 10). The verb's behavior does not change.
- Correction (lead, 2026-09-28): an earlier version of this task added "idle
  only; refused on air" to U8. That was the lead's addition, not JJ's
  ruling, and is withdrawn; the row is not disabled on air.

Files: `src/gui/applets/TxApplet.{h,cpp}`, `src/gui/MainWindow.cpp`,
`tests/tst_tx_applet_binding.cpp` (new or the existing TxApplet test).

Interfaces:
- `void TxApplet::setTransmitSliceResolver(std::function<SliceModel*()> resolver);`
  (replacing the fifth `setDesktopKeyHandlers` argument)
- U8 row: the applet is given the slices this device controls and the
  transmit slice, and emits a request naming a slice id; the implementer
  names these in the report (they are consumed only inside this task and
  Task 17's captures).

Acceptance:
- Window RX on listened B0 (20 m), TX bound on controlled A1 (40 m): TX band
  reads 40 m; power slider loads the 40 m value; no MOX, no holder change.
- Selecting a listened slice for receive never changes `txBoundSliceId`,
  `m_chosenTxSlice` or the holder.
- U8: device X controls A0 and C2 and listens to B1; the row shows A and C
  only. Pressing C while unkeyed and holding transmit binds C2
  (`txBoundSliceId` 2) with no key. Pressing C while on air on A0 unkeys A0
  through the unkey gate, then binds C2 once the unkey is confirmed; nothing
  keys C2. After Y takes A0, A leaves X's row; if A0 was X's transmit
  selection it clears by the Q8 path.
- U8: a device that does not hold transmit gets today's refusal words
  (`StationServer.cpp:2576-2582`) and nothing changes.

Verification: `tst_tx_applet_binding`, `tst_tx_slice_arbiter`,
`tst_multi_device_screens` (including a capture of the U8 row unkeyed and on
air), offscreen, real load.

## Task 12: Phone contract note

Implements: design "the matching iPhone UI belongs in the phone's dependent
PR"; "The phone uses the same actions and states through its receive-slice
UI, with its actual layout to be reviewed by the phone crew".
Depends on: Tasks 4, 6, 8, 9 (the wire is final). Model tier: sonnet.

Today: the phone reaches Take only after a refused command
(`ConfirmationLayer.swift:35-59`, `TakeReceiverSheet.swift:8-73`); close and
select refusals are log-only (`BandSlicesModel.swift:302-349`); foreign notes
say only the owner may tune (`ForeignSliceLabel.swift:8-14`) (deep scout).

Change: a short note for the phone crew, no phone code: the hello feature,
capability and gate; the `SliceAccess` class; the five verbs and the
extended select; `controlTaken`; capacity result values and question fields;
local level semantics (the phone's volume never writes shared AF or mute);
refusal words to show as sent; the approved behaviors and journeys; the
phone parity gaps the deep scout found (log-only refusals, foreign note
wording) as items for that PR; JJ's desktop window rulings U1 to U8
(2026-09-28), quoted, as the behavior the phone's receive-slice UI should
match where it applies (per-device volume and mute, the listened wording,
hearing only slices it can see, the transmit letter choice among controlled
slices), with
the phone's actual layout left to the phone crew's review.

Files: `docs/architecture/2026-09-28-slice-access-phone-contract.md` (new).

Acceptance: every wire name in the note matches `surface.json` after Task 9;
no em dash; American spelling; no phone layout stated as settled beyond
JJ's rulings.

Verification: `scripts/render-link-tables.py` shows no drift; a grep of the
note's names against `tests/data/link/v1/surface.json` finds each one.

## Task 13: Bottom RX chooser

Status: the bottom area is approved by JJ (design status line; G-118 "JJ
said the bottom area seems good"). Build it to
`nereus-slice-chooser-review.html`, adjusted by JJ's rulings of 2026-09-28:
row wording per U6, no volume control in a row (U5 keeps volume on the
existing AF slider and mute), and Select RX placing or raising the slice's
pan per U1 and U2 (built in Task 16; until Task 16 lands, Select RX focuses
only what it focuses today).
Implements: design "What the operator should be able to do" and the chooser
paragraph; "An empty window still offers Choose a slice and New slice";
deep scout P2 (misleading empty-state wording); U5, U6.
Depends on: Tasks 5 and 10. Model tier: opus.

Today:
- The bottom RX dashboard shows this window's active slice
  (`MainWindow.cpp:1531-1540`; G-124 null-binding fix in `9925ed9a`).
- No inventory, no listen or take entry; the remote empty-pan Take button
  uses `m_hadSliceThisSession` and can wrongly blame another device after a
  deliberate close (`MainWindow.cpp:2013-2022`, `:2090-2095`).

Change: the dashboard letter opens an all-slice chooser (letter and Aether
color, frequency, mode and filter, controller with this window / the Core's
desktop / away distinguished by id not name, this device's listening state,
TX state); inspecting a row does nothing; actions Listen in, Take control,
Release, Stop listening, New slice, with pending and refusal feedback; empty
state offers Choose a slice and New slice with honest wording. Hosting uses
`HostingSliceActions`; remote uses the Task 5 verbs.

Files: `src/gui/SliceChooser.{h,cpp}` (new), `src/gui/RxDashboard.{h,cpp}`,
`src/gui/MainWindow.cpp`, `tests/tst_slice_chooser.cpp` (new),
`tests/tst_multi_device_screens.cpp`.

Interfaces: `class SliceChooser : public QWidget` with
`void setInventory(const QList<SliceChooser::Row>& rows);` and signals
`listenRequested(int sliceId)`, `takeControlRequested(int sliceId)`,
`releaseRequested(int sliceId)`, `stopListeningRequested(int sliceId)`,
`newSliceRequested()`, `selectRequested(int sliceId)`.

Acceptance:
- Offscreen capture at the reference geometry
  (`core-gui-multi-pan-reference-report.md`, 1440 by 900) matches the
  reviewed mockup's content as adjusted by U5 and U6; narrow width does not
  clip.
- U6 wording: a row this device controls reads "You control" and offers
  Release; a row it listens to reads "Listening · controlled by <device>"
  and offers Take control and Stop listening; a row on air shows "TX" in
  red; an idle transmit selection shows "TX" as today.
- Inspecting a row (selection or hover) sends nothing on the wire and
  changes no window RX, TX, pan or slice state (signal spy on the model and
  the session).
- No row has a volume or mute control (U5).
- Every action shows pending, then the Core's result or refusal words as
  sent; a refused action changes nothing.
- Two devices with the same display name are told apart (by id, not
  name); this window and the Core's desktop are marked as such.
- A zero-slice window offers Choose a slice and New slice, and the empty
  state never blames another device after this device's own close
  (deep scout P2).
- `selectRequested` is emitted for Select RX; its pan effect is Task 16's
  acceptance.

Verification: `tst_slice_chooser`, `tst_multi_device_screens` with
`NEREUS_TASK78_SHOTS` captures reviewed by the controller before any launch.

## Task 14a: Joined flags, menus, labels and disabled tuning

Implements: design full-window proposal bullet 2; G-119 palette; rulings U4
and U6. Split from the former Task 14 by JJ's new order: this part runs
before Task 6 and carries no level control.
Depends on: Task 13. Model tier: opus.

Today: a hosting flag not station-owned is hidden (`MainWindow.cpp:1708-1729`;
`src/gui/widgets/VfoWidget.cpp:2489-2495`); foreign slices are dashed markers with owner,
away and TX cues (`MainWindow::refreshForeignMarkers`, `MainWindow.cpp:2034+`); the flag writes shared
`muted`, pan and AF (`MainWindow.cpp:3166-3168`, `:3312-3317`; `src/gui/widgets/VfoWidget.cpp:1245-1271`,
`:1369-1390`).

Change (U4, U6): a joined listened flag keeps its letter and color; its text
reads "Listening · controlled by <device>", its menu offers Take control and
Stop listening, and its shared tuning controls are disabled with "<device>
controls this slice". A controlled flag reads "You control" with Release in
its menu. "TX" stays as today, red on the air. Same-pan stacking keeps
today's rule: the selected slice's flag on top. A listened flag shows no
level control until Task 14b.

Acceptance:
- A listener's flag never writes `SliceModel` tuning, AF or mute (signal
  spy).
- Each menu action sends its verb and shows pending, then the Core's result.
- Tuning controls on a listened flag are disabled and name the controller.
- Captures show each state: controlled, listened, listened while on the air,
  and two flags stacked on one pan.

Verification: `tst_multi_device_screens`, `tst_remote_window_harness`
captures.

## Task 14b: "Your volume" on a listened slice

Implements: ruling U5 (the existing AF slider and mute on any slice change
only what this device hears; labeled "Your volume" on a listened slice).
Depends on: Task 6 (per-listener audio fan-out) and Task 14a.
Model tier: opus.

Change: the listened flag's AF slider and mute return, bound to this
device's own listen level and mute from Task 6, labeled "Your volume". On a
controlled slice the slider and mute also change only what this device hears
(U5; the controller's own path per the U5-over-Q2 ruling).

Acceptance:
- Moving "Your volume" or mute on a listened slice changes only this
  device's audio; the controller's and every other listener's audio and the
  shared `SliceModel` AF and mute are unchanged (signal spy and media test).
- The controller's AF at zero does not silence a listener.
- A capture shows the labeled control on a listened flag.

Verification: `tst_multi_device_screens`, the Task 6 media tests,
`tst_remote_window_harness` captures.

## Task 15: RX applet tabs and listener gating

Implements: design full-window bullet 3; JJ's rulings U5, U6 and U7
(2026-09-28). U7 supersedes the earlier plan for tabs of hidden joined
slices: a device hears only slices it can see, so every joined slice is
visible and there are no hidden-slice tabs. U5 keeps volume and mute on the
existing surfaces; the RX applet gains no volume or mute.
Depends on: Task 14. Model tier: opus.

Today: RX applet tabs list only station-owned slices on a hosting window, via `RxApplet::updateSliceButtons` (`RxApplet.cpp:1411`)
(`MainWindow.cpp:1711-1740`); its controls write `SliceModel` directly
(for example the mode combo at `RxApplet.cpp:614-619`); its mute button was removed, VfoWidget is the mute surface (`RxApplet.cpp:1190`, `:1605`).

Change (as ruled): tabs for every slice this device controls or listens
to; shared tuning and DSP disabled for a listened slice with "<device>
controls this slice" (U6); Take control reachable from a listened tab.

Acceptance:
- Tabs list exactly the slices this device controls or listens to; a
  slice not joined here has no tab.
- After a layout change stops listening to a slice (U7, Task 16), its tab
  goes away; no tab ever exists for a slice this device cannot see.
- On a listened tab every shared tuning and DSP control is disabled and
  says "<device> controls this slice"; a signal spy shows no `SliceModel`
  write and no wire message from any of them; Take control is reachable.
- The RX applet has no volume or mute control (U5; `RxApplet.cpp:1190-1191`).
- Selecting a listened tab changes window RX (bottom bar, flag focus) and
  never changes the transmit selection or the holder.

Verification: `tst_rx_applet_*`, `tst_multi_device_screens` captures.

## Task 16: Multiple pans, unseen slices, floating focus, background click and layout changes

Implements: design full-window bullets 1 and 5; G-126; JJ's rulings U1,
U2, U3 and U7 (2026-09-28).
Depends on: Task 15. Model tier: opus.

Today: `applyPanLayout` rehomes and spreads slices and `populatePanSlices`
adds a slice to every empty pane (`MainWindow.cpp:13853-13958`); the layout
dialog caps by receiver count (`:13967+`); pan background selects display
focus only (`PanadapterApplet.cpp:217-234`); floating reparents without
changing identity (`PanadapterStack::floatPanadapter` `:481`, `dockPanadapter` `:534`); spectrum actions target
the emitting pan's slice through `sliceForPan` (`MainWindow.cpp:3936-3980`, `:4179`, `:4222-4261`).

Change (as ruled): an unseen slice this window starts listening to or
takes control of goes into the main window: a main-window pan that shows it
comes forward, else an empty main-window pan, else the window grows to the
next layout that fits in the single window, else the operator picks a pan.
Growing adds no slice to any other empty pan and never opens a floating pan.
A listened slice is placed by this window only; its pan key stays its
controller's. Selecting a slice shown in a floating pan brings that floater
forward and makes it this window's RX; nothing moves and no second flag
appears (U2). A pan background click stays display-only (U3). A layout
change moves only the slices this window controls; a listened slice whose
pan the change removes stops being listened to here, with the notice
"Stopped listening to Slice B: it is no longer shown in this window." (U7).
A layout change never moves, recreates or spends a DDC for a slice. Every
control keeps an explicit target slice.

Acceptance:
- U1, placement order: with the slice already in a main-window pan, that
  pan comes forward; with none showing it and an empty main-window pan,
  that pan shows it; with no empty pan, the main window grows to a layout
  that fits in the single window and the new pan shows it; when no larger
  layout fits, the operator is asked to name a destination pan (reading
  confirmed by the lead, 2026-09-28). In every
  case: slice count, DDC mask and the controller's frequency, mode and
  filter are unchanged, no floating window is created, and growing the
  layout for placement adds no new slice to any other empty pan.
- U2: Select RX (chooser row, flag or tab) on a slice showing in a
  floating pan raises that floater and sets the main window's RX to it; the
  floater's pan is not reparented or docked, the slice keeps its pan, and no
  second view of it is created.
- U3: a pan background click changes keyboard and scroll focus only; window
  RX, the pan's selected slice, the transmit selection and the wire are
  unchanged (signal spy). A flag or tab click still changes window RX.
- U7, listened slice: shrinking the layout so no remaining pan of this
  device shows listened B0 stops listening to B0 on this device through
  Task 4's stop-listening path, shows a plain notice naming the slice, and
  leaves B0, its controller's tuning, the other listeners' audio, slice
  count and DDC mask unchanged.
- U7, controlled slice: shrinking the layout rehomes a controlled slice
  into a remaining pan as today (`tst_slice_rehome_on_layout_shrink` still
  passes).
- U7 invariant: after any layout change, every slice this device controls
  or listens to is shown in one of this device's pans (checked over the
  preset layouts with a mix of controlled and listened slices).
- A layout change never recreates a listened slice or spends a DDC for it.

Verification: `tst_panadapter_stack_layouts`, `tst_pan_floating_window`,
`tst_slice_rehome_on_layout_shrink`, `tst_pan_active_slice_sync`,
`tst_remote_window_harness` captures.

## Task 17: Delivery verification

Implements: the design's "Verification required before delivery", each
bullet as acceptance. Depends on: every task above. Model tier: opus.

Acceptance (each is a named test or capture; hardware rows stay pending
until JJ authorizes a bench session):
- Three clients see the same slice identity and current tuning through
  listen, handoff, release and reconnect; their active RX, volume and mute
  stay independent; the previous controller hears continuous audio after
  handoff and cannot continue writing tuning. (Tasks 3, 4, 6, 8)
- A listener's forged property writes, tuning verbs and TX attempts are
  refused by the Core; old-session, reused-letter, double-take and
  stale-confirm cases keep the current authority and every unrelated slice.
  (Tasks 1, 2, 4)
- Idle TX selection clears on transfer, on-air transfer is refused, and RX
  control never keys or grants transmit. (Tasks 4, 11)
- The last listener leaving closes the slice; remaining listeners prevent
  closure; the final physical removal reaches a valid zero-slice state that
  can create a new slice; device expiry and explicit leave follow the same
  rules; a reconnect within grace survives an old expiry event. (Tasks 7, 8)
- Capacity exhaustion offers existing-slice actions; failed creation, pan
  move and restoration keep every victim and give honest notices. (Task 9)
- Hosting desktop, remote desktop and phone show pending and refusal
  feedback with equivalent semantics, zero-owned states included (desktop
  here; phone in its PR per Task 12). The real desktop layout is verified
  offscreen before any preview launch JJ authorizes; the HTML mockup is not
  evidence. (Tasks 10, 13 to 16)
- JJ's window rulings hold end to end on the desktop: placement without a
  new floating pan, receiver or tuning change (U1), floater raise without a
  move (U2), display-only background click (U3), stacking and wording (U4,
  U6), per-device volume and mute including the controller's (U5), a
  device hears only slices it can see (U7), and the TX applet letter row
  (U8). (Tasks 6, 11, 13 to 16)
- Full affected suites (labels `core`, `session`, `audio`, `gui`) once at the
  end, offscreen, at real load, with load average and step count recorded;
  whole-branch review by one reviewer.

Verification: the listed suites; `tst_remote_window_harness` and
`tst_multi_device_screens` captures to `~/.config/nereus/work/`; the bench
matrix rows (live HL2 and ANAN-G2 with two remote devices listening) marked
pending until JJ runs them.
