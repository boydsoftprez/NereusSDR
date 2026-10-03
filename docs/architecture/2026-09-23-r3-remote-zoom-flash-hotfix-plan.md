# R3 remote zoom flash hotfix implementation plan

> **Execution:** run with `crew` under `cost-aware-execution`. One task. Runs in the
> hotfix worktree `/Users/j.j.boyd/.codex/worktrees/nereus-hotfix/NereusSDR` (branch
> `codex/hotfix-zoom`, build directory `build-hotfix`), based on checkpoint
> `80f45e28` (the build the operator is testing); the controller carries it into the
> integration branch afterwards.

**Goal:** zooming a pan in a remote window never makes the screen flash. The operator
saw the screen flash while zooming in and out on checkpoint `80f45e28` (Rock 5C Core
with the ANAN-G2, three pans, C-Tune on); it stopped about a minute later.

**Architecture:** the window log shows, during the flashing, 116 `requestStreamCentre`
commands in about 40 seconds (up to about 20 a second), every one refused by the Core
with "C-Tune centre is invalid for this stream's cohosts", and one plain-words notice.
The window sends that command from `SpectrumWidget::centerChanged` whenever C-Tune is
on (`src/gui/RemoteMediaController.cpp`, the two `centreGesture` connections). The
previous build's session (the operator zoomed then too, with spectrum grant changes)
sent none. Find why zooming moved the pan centre and asked the Core to follow, what the
window drew each time the Core refused, and why it happened only in the first minute;
then fix it so a zoom gesture does not ask the Core to move a stream it cannot move, a
refused request never blanks or redraws the spectrum from nothing, and refusals during
one gesture do not repeat at gesture rate.

**Tech stack:** C++20, Qt 6, Qt Test (off-screen), the remote window harness.

**Spec:** the operator's report of 2026-09-23 ("when i zoom in and out on this build
the screen flashed", then "not doing it now"); the log excerpt
`/Users/j.j.boyd/.config/nereus/work/zoom-flash/rock-window-80f45e28-excerpt.log`
(window log of checkpoint `80f45e28`, profile `radxa_5c_r3`); the earlier session's full
log for comparison
`/Users/j.j.boyd/Library/Preferences/NereusSDR/profiles/radxa_5c_r3/nereussdr-20260923-194215.log`
(checkpoint `44584133`). R3 plan requirements R-R3-18 (remote C-Tune keeps the Core's
receive-window centre during in-window tuning; explicit pan motion moves it), R-R3-19
(remote zoom stops at the supplied bandwidth during the gesture), R-R3-20, R-R3-21.

## Global Constraints

- Work only in the hotfix worktree and its build directory; never checkout, rebase,
  merge, reset or push; never touch the integration, lane B or checkpoint worktrees.
- Commits: GPG-signed with hooks (`NEREUS_THETIS_DIR=/Users/j.j.boyd/Thetis`), never
  `--no-gpg-sign` or `--no-verify`, no `Co-Authored-By`, no em-dash characters. Stage
  explicit paths only. Every commit names its R-R3 IDs.
- Behaviour that stays: explicit pan motion (dragging the pan with C-Tune on) still asks
  the Core to move its window, and the Core's refusal rule for streams shared by
  several receivers stays as it is. Local (direct radio) zoom and C-Tune are unchanged.
- Debug before fixing: reproduce in a test first (the remote window harness with
  several slices on one stream, C-Tune on, a zoom gesture), record what the window sends
  and draws, and state the root cause in the report with file:line before changing code.
- Operator wording: any new or changed string passes `OperatorWording::isPlain`.
- The Mac is shared with the operator's live test: build with -j4 under `nice`, run
  tests with `QT_QPA_PLATFORM=offscreen`, never open audio devices. Build exact
  targets. Never build the `NereusSDR` target.

## Task 1: Zoom without asking the Core to move, and no flash on a refusal

**Requirements:** R-R3-18, R-R3-19, R-R3-21.

**Files:**
- Modify: `src/gui/RemoteMediaController.{h,cpp}` and, as the root cause requires,
  `src/gui/SpectrumWidget.{h,cpp}` and the refusal handling in
  `src/models/RadioModel.cpp` or `src/core/session/StationClient.cpp`
- Test: `tests/tst_remote_window_harness.cpp` or the remote C-Tune test the root cause
  points to (grep `tests/` for `requestStreamCentre` and `centreGesture`)

**Acceptance:**
- A zoom gesture (wheel and frequency-scale drag) on a pan whose stream is shared by
  other receivers, with C-Tune on, sends no stream-centre request the Core must refuse,
  and the spectrum is never cleared or blanked during or after the gesture (the test
  fails before the fix).
- Dragging the pan with C-Tune on still sends the request; a refusal leaves the picture
  where the Core has it, shows the plain notice once per gesture, and sends no more
  requests for the rest of that gesture.
- A pan whose stream is its own (no other receivers) behaves as today.
- The report explains why it happened only in the first minute after the Core started.

**Verification:** the new or extended test, plus `tst_remote_gui_gating`,
`tst_remote_window_harness` and every test that includes `RemoteMediaController.h`,
built and run by exact name. Hardware (pending, operator checkpoint): zoom in and out on
the Rock window with C-Tune on and three pans.

**Execution note (advisory):** opus.

- [ ] **Step 1:** Reproduce in a test, state the root cause, fix, commit.
