# Remote audio status and native-check fixes

## Scope

R-R3-23 (task 5a, first checklist item): the remote GUI shows a lasting,
plain-English audio status instead of a five-second toast, with Retry. Plus
five native-check fixes found on the running GUI on 2026-09-22: R-R3-34
(receive-layout restore on startup), R-R3-13 (S-meter and level bar show a
sentinel reading while disconnected), R-R3-21/25 (developer Tools test
entries reachable in a remote session), R-R3-24 (RADE transmit filter not
applied to a slice restored already in RADE mode), and R-R3-17/38 (macOS
Local Network permission blocking reconnect with an unhelpful reason).

## What changed

**Task 1 (5a).** Core now reports the accepted `OpusEncoderProfile` and the
actual reason audio is off inside the existing `audio-context` reply, to
GUIs that negotiated session protocol minor 8; minor-7 peers keep the exact
eight-field message unchanged. Commits 30ec9fd8, 3a7e9891, 911d87ff,
7d1e249b.

**Task 2 (5a).** The receiver now measures RFC 3550 arrival jitter and
missing packets and reports typed faults (speaker open/timing/stall/write,
decoder unavailable, and related cases). Commits 9e65d853, 45d68728.

**Task 3 (5a).** `RemoteMediaController` owns one `RemoteAudioStatus` with a
failure identity that only matching recovery plus real speaker progress can
clear, replacing the old toast-and-banner presentation. Commits 36b29167,
07cd24c1.

**Task 4 (5a).** The Core connection panel and title bar present the new
status: headline, problem line when present, codec, output, and health
lines (arrival jitter, missing packets, gaps filled, speaker buffer), with a
Retry audio action. Commit c9f943ac.

**Native-check Task 1 (R-R3-34).** A saved slice whose frequency falls
outside every receiver already restored for its pan now gets its own new
receiver on startup restore, keeping its saved pan id; it is refused only
when no receiver can be created or its saved values are invalid. Restore
messages were rewritten in plain words. Commit 03279ff3.

**Native-check Task 2 (R-R3-13).** The S-meter widget and the VFO flag level
bar show no reading ("-- dBm" / "--", needle at scale minimum, no peak-hold
marker) for the no-reading sentinel or a non-finite value, in both remote
and local modes, instead of printing "-400 dBm" or another placeholder
number. Commit b8ebdd48.

**Native-check Task 3 (R-R3-21/25).** "Test antenna switch toast" and "Test
TX-bound re-route dialog" in the Tools menu are disabled while connected to
a Core, with the same tooltip wording as the other unavailable remote
transmit controls, and cannot reach their signal-emitting handlers while
disabled. Commit b9e8811c.

**Native-check Task 4 (R-R3-24).** A slice that Core restores already in
RADE-U or RADE-L now gets the RADE transmit passband (650 to 2350 Hz)
applied once after wiring, matching the passband a live RADE mode change
already applies; leaving RADE still restores 100 to 3900 Hz. Commit
ff34a7e4.

**Native-check Task 5 (R-R3-17/38).** The macOS app bundle now sets
`NSLocalNetworkUsageDescription` at build time (Set-or-Add, alongside the
existing microphone key), and a reconnect failure to a private or
link-local address that is a network-reachability error now names the
macOS Local Network permission as a likely cause instead of showing "Host
unreachable" alone. Commit ec81cc1b. A pre-existing flaky slot in
`tst_session_link_loss` (polling in 50 ms steps against a 50 ms first
retry) was converted to a signal wait; the product was not at fault.
Commit 9982cbed.

## Automated evidence

Per-task targets, from the two ledgers:

- Task 1 (5a): 5 brief targets pass; `-L session` 4/4; wider 31/31
  session/remote/daemon plus the `core-no-gui` guard.
- Task 2 (5a): 5 targets, 100% pass, 26.9 s at load 3.6.
- Task 3 (5a): 5 targets pass, 31.9 s at load about 3.3; three mutations
  caught.
- Task 4 (5a): 4 targets plus the NereusSDR build pass, 5 runs, 0 flakes.
- Native-check Task 1 (R-R3-34): 5 brief targets pass, 49.4 s at load about
  5; runtime 17/17; native and session layout tests pass; the new daemon
  regression fails before the fix with the Rock's exact "Rejected
  stream=-1" and passes after.
- Native-check Task 2 (R-R3-13): `tst_smeter_widget_face` extended with new
  cases that failed first ("-400 dBm", NaN, "+inf"); `tst_remote_meter_poller`
  plus 6/6 SMeterWidget targets pass.
- Native-check Task 3 (R-R3-21/25): `tst_remote_gui_gating`, 40 cases pass,
  4.18 s at load about 7; a new case is red before the gate; sabotaging
  either handler guard is caught.
- Native-check Task 4 (R-R3-24): 4 tests pass; the new regression fails
  before the fix at 100/3900.
- Native-check Task 5 (R-R3-17/38): `tst_session_link_loss` new mapping
  slots pass 7/7; `PlistBuddy` Set-or-Add verified twice on a scratch
  plist; the bundle `Info.plist` check is deferred to the controller's
  final bundle build. Flaky-slot fix: before, 3/20 filtered ctest failures
  (count 10 at line 865); after, 0/30 at load about 2 and 0/30 at load
  3.7 to 7.4.

Full gate at HEAD 9982cbed. Build:
`cmake --build build-integration --target nereusd all_tests -j6` rc=0 in
138 s (load before 2.86/4.25/3.63, after 9.88/6.18/4.47); only warning: the
known linker note "ignoring duplicate libraries:
third_party/wdsp/libwdsp_static.a". Unfiltered
`ctest --test-dir build-integration -j6 --no-tests=error --output-on-failure`:
"100% tests passed, 0 tests failed out of 743", Total Test time (real) =
254.84 sec (load before 8.57/6.07/4.46, after 6.10/6.38/5.02). Inner Qt
skip lines were not enumerated in this run; ctest shows a passing test's
output only on failure, so no skip count is claimed here.

## Review

Final review (crew-reviewer, opus, read-only, 0 builds) covered both plans
together over 5a1c7ef3..ff34a7e4: verdict "Ready to merge: With fixes",
0 Critical. Important: (1) the panel capture test wrote to a hard-coded
`.crew` path with `QVERIFY`; (2) R-R3-13 unmet while disconnected in
Signal/SigAvg/Peak (-140 dBm) and the VFO level bar (raw -400); (3) restore
and save messages still leaked internal store reasons and "no selected
owner" wording. Minor findings: `nullopt` profile on a minor-8 enabled
context, a misplaced doc comment, a duplicated `newer()` helper, the panel's
fixed height and a timer that ran while hidden, `reorderQueuedMs` not shown
in telemetry, a stale plan constraint, and codec/health lines wrapping
between a number and its unit.

Rulings from the final review: the early return in the radio-change handler
is not a defect (only `stop()`/`start()` change peer, epoch or connection,
and both refresh); the RADE re-snap on every reconnect stands; the stale
plan constraint is the controller's own documentation fix; the vintage-face
"--" rendering is the operator's call.

Fix wave (opus) addressed Important 1-3 and Minor 1, 2, 3, 4, 5, 7, plus the
live "+RX" wording that used the internal words "receiver DDCs". Commits
639ce370, 0d284ae2, 85c4ab27, e9857fc7, 1a2dc8ee, 3d1ee77a, 1c43dd7e,
75b0e9be, 11d7d463, 4655946c. All 13 commits (including native-check Task 5
and its flaky-slot fix) are GPG-signed, carry no `Co-Authored-By` trailer
and contain no em-dashes.

Re-review (crew-reviewer, opus, ff34a7e4..9982cbed): Important 1-3
ADDRESSED; Minor 1-5 ADDRESSED; Minor 7 NOT ADDRESSED for the restore
string "Receiver %1 (%2 MHz %3)" (`RadioModel.cpp:4483,4486`); the flaky-slot
fix MET.

Parked items with their rulings, all deferred to the post-checkpoint polish
batch and none blocking this checkpoint:

- Minor 7 remainder: no non-breaking space in the restore refusal string.
  Ruling: cosmetic, nothing builds on it.
- `plainReceiveLayoutProblem` maps "invalid RADE receive owner"
  (`ReceiveLayoutStore.cpp:291`) to "not in RADE mode"
  (`RadioModel.cpp:4343-4346`). Ruling: wrong text only for a damaged
  record; match the invalid-owner reason first, map it to the
  damaged/different-version sentence, and add a case.
- No test covers the rewritten save-refusal sentences
  (`RadioModel.cpp:13460,13479-13480`). Ruling: add coverage in the polish
  batch.
- Container `MeterWidget` bindings still show -140 dBm while disconnected;
  the `joinStream` reason "receiver window (stream %1)"
  (`SliceStreamAllocator.cpp:76-78`, pre-existing); the Local Network
  reason repeats in a 5 s toast on every retry (`MainWindow.cpp:1124-1125`).
  Ruling: in scope later, in the polish batch after the operator
  checkpoint; none of these block the checkpoint.

## Hardware and operator acceptance

All rows are PENDING. The Core install/restart and the GUI relaunch happen
after this commit, ahead of the operator's hardware smoke test.

| Row | What the operator should see | Status |
| --- | --- | --- |
| Core connection panel, Remote audio section | Headline (for example Playing), codec line, output, and health lines (arrival jitter, missing packets, gaps filled, speaker buffer) | PENDING |
| Master mute | "Muted on this computer" | PENDING |
| Removed output device | A persistent playback problem, cleared by Retry audio once playback resumes | PENDING |
| Title bar | The audio word in the title bar tracks the same status | PENDING |
| Receive layout after Core restart | A 40 m RADE receiver restored on its own receiver after the Core install restart | PENDING |
| S-meter and slice flag while disconnected | "--" instead of a sentinel number | PENDING |
| TX BW at launch | 650-2350 Hz right after launch with slice A in RADE-U | PENDING |
| Tools menu while connected | The two test entries greyed while connected to a Core | PENDING |
| macOS Local Network prompt | Prompt appears at first launch; reconnect works on the station LAN afterward | PENDING |
| Version mix | minor-7 GUI against the minor-8 Core, if observable | PENDING |

## Known limitations

- The 24 versus 48 kbit/s listening comparison and any quality selector
  remain open (task 5a items 2 and 3); this checkpoint only covers item 1.
- Container `MeterWidget` items still show -140 dBm while disconnected; only
  the S-meter widget and the slice flags show no reading.
- Host names (for example a `.local` name) do not get the Local Network
  hint; only literal private or link-local addresses do.
- Vintage-face (Futura) rendering of "--" as a single dash, versus two
  hyphens on the Classic face, is the operator's call.
- Parked review items above (Minor 7 remainder, `plainReceiveLayoutProblem`
  invalid-owner mapping, missing save-refusal test coverage, and the three
  out-of-scope container/joinStream/Local-Network-toast items) are deferred
  to the post-checkpoint polish batch.
