# R3 user wording implementation plan

> **Execution:** run with `crew` under `cost-aware-execution`. Requirements and
> acceptance cases are binding; one whole-branch review at the end of the batch.
> Runs in the second builder lane (worktree
> `/Users/j.j.boyd/.codex/worktrees/nereus-lane-b/NereusSDR`, branch
> `codex/lane-b`, build directory `build-lane-b`).

**Goal:** everything an operator reads in a remote session is written for the
operator: the pan status line fits a narrow pan and explains itself on hover,
and toasts, refusal reasons, Network Diagnostics labels, the Connections window,
the remote audio section, the model manager and the 4O3A page use plain user
words. A shared test check keeps internal terms out from now on.

**Architecture:** one pure builder turns a pan's display state into a short line
and a full explanation. A display-time translation table maps the Core's wire
reason strings (which older apps compare as exact text and so never change) to
user words. A shared test helper lists the internal terms and is applied to the
builder, the table and every new operator string.

**Tech stack:** C++20, Qt 6 widgets, Qt Test (off-screen).

**Spec:** operator directive of 2026-09-23 ("we want all userland wording to be
just that for users"; saved as a standing rule) and R3 question 10 (a shorter
pan status line that fits narrow pans, the full explanation on hover); R3 plan
requirements R-R3-17, R-R3-21, R-R3-23, R-R3-35, R-R3-37. Scout list with
file:line: `/Users/j.j.boyd/.config/nereus/work/r3-next-control-ui-scout-2026-09-23.md`
sections 6 and 7.

## Global Constraints

- Work in the lane B worktree and build directory; commit only on
  `codex/lane-b`; never checkout, rebase, merge, reset or push; never touch the
  integration worktree.
- Commits: GPG-signed with hooks (`NEREUS_THETIS_DIR=/Users/j.j.boyd/Thetis`),
  never `--no-gpg-sign` or `--no-verify`, no `Co-Authored-By`, no em-dash
  characters. Stage explicit paths only. Every commit names its R-R3 IDs.
- Wire strings never change: reasons the Core sends and the app compares as exact
  text stay byte-for-byte (for example "operator disconnect" at
  `MainWindow.cpp:1139`; retire reasons at `SpectrumEndpoint.h:38-40` compared at
  `RemoteMediaController.cpp:2204-2205`). Translate at display time; log the raw
  text.
- Internal terms never appear in anything a user reads: grant, budget,
  allocation, capability, minor, slot, epoch, SSRC, endpoint, revision,
  handshake, snapshot, codec, payload, peer, protocol, session, telemetry, RTP,
  PCM, WebSocket, pong, ledger, reservation, descriptor, plane, context,
  matcher. Log lines and source comments may keep them.
- No behaviour changes beyond text and where it is shown.
- Tests run off-screen. Build exact targets, then run them with
  `ctest -R '^(...)$' --no-tests=error --output-on-failure`. No unfiltered suite.
  Never build the `NereusSDR` target. No hardware.

## Task 1: A pan status line that fits and explains itself

**Requirements:** R-R3-37, R-R3-21.

**Files:**
- Create: `src/gui/PanStatusText.{h,cpp}` (pure builder), a shared test helper
  `tests/OperatorWording.h` (move `isPlainOperatorReason` from
  `tests/tst_remote_gui_gating.cpp:222-231` and extend it with the internal-term
  list above)
- Modify: `src/gui/RemoteMediaController.cpp` (status strings around 1559-1701,
  the grant line 130-146, `statusWithGrant` 1341-1355),
  `src/gui/widgets/SpectrumStatusOverlay.cpp`, `src/gui/PanadapterApplet.cpp`,
  `tests/CMakeLists.txt`
- Test: `tests/tst_pan_status_overlay.cpp`, `tests/tst_spectrum_status_overlay.cpp`,
  `tests/tst_remote_media_controller.cpp` (replace the seven
  `startsWith("Display target")` checks), a new `tests/tst_pan_status_text.cpp`,
  `tests/tst_remote_gui_gating.cpp` (use the shared helper)

**Interfaces:**
- Produces: `struct PanStatusText { QString shortLine; QString explanation; };`
  and a builder from the pan's display state; `OperatorWording::isPlain(QString)`.

**Acceptance:**
- At full quality the short line is empty. Reduced, paused, waiting and refused
  states have short forms that fit a 200 px pan without elision, for example
  "Slower updates: network busy", "Less detail: network busy",
  "Paused: network busy", "Waiting for the Core"; "Core busy" where the reason
  is the Core's capacity rather than the network. The explanation says what
  happened, why, and what happens next, in user words.
- The overlay and applet paint the short line and show the explanation on hover.
- The Core's refusal reasons shown on a pan pass through the translation table
  from Task 2's shared helper (build the table here if Task 2 has not; Task 2
  extends it).
- Every builder output passes `OperatorWording::isPlain`.

**Verification:** GUI text and layout, off-screen.
```sh
cmake --build build-lane-b --target tst_pan_status_text tst_pan_status_overlay tst_spectrum_status_overlay tst_remote_media_controller tst_remote_gui_gating -j6
ctest --test-dir build-lane-b -R '^(tst_pan_status_text|tst_pan_status_overlay|tst_spectrum_status_overlay|tst_remote_media_controller|tst_remote_gui_gating)$' --no-tests=error --output-on-failure
```
Operator review (pending, checkpoint): the wording itself.

**Execution note (advisory):** opus.

- [ ] **Step 1:** Builder, helper and overlay/applet wiring with tests; commit.

## Task 2: Every other message a user reads

**Requirements:** R-R3-17, R-R3-21, R-R3-23, R-R3-35.

**Files:**
- Create: `src/gui/OperatorReasonText.{h,cpp}` (display-time translation of Core
  and transport reasons) unless Task 1 already created it
- Modify: the strings the scout lists in section 7, including
  `src/gui/RemoteMediaController.cpp`, `src/gui/RemoteDisplayAllocator.cpp`,
  `src/gui/MainWindow.cpp` (the "Station media: %1" and "Station link lost: %1"
  toasts and their reasons, 10769, 10242, 10383), `src/gui/RemoteTelemetryController.cpp`,
  `src/gui/RemoteDiagnosticsDialog.cpp`, `src/gui/RemoteAudioStatus.cpp`
  (including the "Codec:" label), `src/gui/GuiConnectionController.cpp`,
  `src/gui/DspAssetDialog.cpp` ("Standard slot"/"Premium slot"),
  `src/gui/widgets/NnrControls.cpp`, `src/gui/setup/FourO3APage.cpp`, and
  refusal texts the Core sends for display only
  (`StationServer.cpp:705-728, 1061`, `SessionCommandDispatcher.cpp:291, 321,
  338, 374, 471, 556, 750, 777`, `StationClient.cpp:1332, 2169, 2307`) where
  the app shows them; `docs/architecture/2026-09-20-remote-daemon-r3-verification/README.md:393-395`
  (probe wording from the audio plan's Task 2)
- Test: the tests covering each changed string, and a new sweep test that runs
  every translation-table output and every changed literal through
  `OperatorWording::isPlain`

**Interfaces:**
- Consumes: Task 1's `OperatorWording::isPlain` and translation table.
- Produces: `OperatorReasonText::forDisplay(QString wireReason)`.

**Acceptance:**
- Run the scout's mechanical sweep (section 7's `git grep -P` command plus the
  reason call sites it names); every user-visible hit is rewritten in user words
  or shown to be log-only (list each in the report with its before and after
  text, for the operator's review).
- Wire strings, log lines and compare-by-text sites are unchanged (grep proof in
  the report); the app shows translated text for them.
- Diagnostics keep their meaning: numbers and units unchanged; only labels and
  explanations change.
- All affected tests pass; the sweep test passes.

**Verification:** GUI text, off-screen.
```sh
cmake --build build-lane-b --target all_tests -j6
ctest --test-dir build-lane-b -R '(remote|station|connection|diagnostic|telemetry|media|nnr|dsp_asset|fouro3a|four_o3a|gating|wording|pan_status)' --no-tests=error --output-on-failure
```
Operator review (pending, checkpoint): the before/after list.

**Execution note (advisory):** opus. Requires Task 1.

- [ ] **Step 1:** Translation table and sweep test.
- [ ] **Step 2:** Rewrite each string; report the before/after list; commit.
