# Remote Unkey Audio Resume Implementation Plan

> **Execution:** run with `crew` (`/crew <this file>`) under `cost-aware-execution`.
> Requirements and acceptance cases are binding; test order and review effort follow
> the risk-based policy; UI evidence follows `ui-verification`. No review between
> tasks; one whole-branch review at the end.

**Goal:** in a remote window, receive audio returns as soon as the Core unkeys, instead of
up to 4 s later.

**Architecture:** while the Core transmits it sends the keying device no receive audio
(DaemonMediaController: receive frames stop for the transmitting pan unless MON). The
window's `RemoteAudioReceiver` sees 500 ms without packets, raises `Fault::NoPackets`
through `restartRequested`, and `RemoteMediaController` stops the receiver and schedules
`requestAudio()` through `RemoteAudioRestartBackoff` (1 s, 2 s, 4 s). Each restarted
context again hears nothing while keyed and backs off further, so after the unkey the
audio waits for the next timer. The window already knows this silence is expected
(`Private::coreTransmitting()`, used to skip the media-stall recovery). The fix: a
no-packets restart while the Core transmits is held, not backed off, and the end of the
Core's transmit asks for audio at once.

**Tech Stack:** C++20, Qt6, the remote media client in `src/gui/RemoteMediaController.cpp`
and `src/core/session/media/`.

**Evidence (2026-10-07, JJ's window on the Rock 5C Core, dev build c31c30856):** unkey at
00:57:49.765, the next "Remote audio receiving" at 00:57:52.767 (3.0 s); unkey at
00:57:18.169, receiving at 00:57:21.634 (3.5 s). Between keys the log shows "Remote audio
had no admitted/playable packets for 500 ms" followed by new contexts every 2 to 4 s.

## Global Constraints

- CLAUDE.md and CONTRIBUTING.md bind this task: no raw new/delete, braces on all control
  flow, `kPascalCase` constants, `m_camelCase` members, `qCWarning(lcCategory)` for errors,
  no exceptions, don't remove code you didn't add.
- Rule R1: nothing under `src/core/` or `src/models/` includes a GUI header.
- Cross-thread state is `std::atomic` or a queued signal; never hold a mutex in the audio
  callback.
- Requirement tracing: this repository traces remote-media behaviour to R-R3 IDs. Name the
  existing ID this extends (R-R3-21, the restart rules) and, if the behaviour needs its own,
  add the next free R-R3 ID where the R-R3 requirements are defined, in the same commit.
- Every edited source file gets a line in its "Modification history (NereusSDR)" block:
  date 2026-10-07, what changed, `J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.`
- Commits are GPG signed (`git commit -S`), never `--no-gpg-sign`, and carry NO
  `Co-Authored-By` trailer.
- The pre-commit hook needs: `NEREUS_THETIS_DIR=/Users/j.j.boyd/Thetis
  NEREUS_THETIS_V21015_DIR=/Users/j.j.boyd/Thetis-v2.10.3.15
  NEREUS_MI0BOT_DIR=/Users/j.j.boyd/mi0bot-Thetis NEREUS_DESKHPSDR_DIR=/Users/j.j.boyd/deskhpsdr
  NEREUS_FREEDV_DIR=/Users/j.j.boyd/freedv-gui`.
- Build: nothing may download. Configure the fresh build dir with
  `cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo -DFETCHCONTENT_FULLY_DISCONNECTED=ON
  -DOPUS_URL=/Users/j.j.boyd/.config/nereus/work/deps-cache/nereus-core-pins-0368ff16/opus/opus-940d4e5-with-model.zip
  -DOPUS_URL_HASH=SHA256=740a5220180aee546f97030118517eedafa7956fdfbc8e8432d91f2d20aac4b7`
  and confirm `grep -c "Performing download step"` on the configure output prints 0. If
  anything would download, stop and report BLOCKED.
- Tests: read docs/development/fast-test-loop.md first. Build and run single tests
  (`cmake --build build --target <test>` then `ctest --test-dir build -R '^<test>$'`); a
  normal build does not rebuild test programs. Never the whole suite. Do not set
  `QT_QPA_PLATFORM` globally; the build sets it per test.
- User-facing strings: plain English, no "yet", no em-dashes.

## What already exists

- `src/gui/RemoteMediaController.cpp`: the `restartRequested` handlers for the speakers
  receiver (`d->audio`, around line 1599, backoff `d->audioRestartBackoff`), the headphones
  receiver (`d->headphones`, around line 1640) and each per-slice receiver stream (around
  line 5456, `stream.restartBackoff`); `Private::coreTransmitting()` (keyed, tuning or
  txEnding from `client->transmitState()`, false when `txStateVersion < 1`); the media-stall
  check that already returns early while `coreTransmitting()`; `requestAudio()`.
- `src/core/session/media/RemoteAudioReceiver.cpp`: the 500 ms `Fault::NoPackets` check.
- `src/core/session/media/RemoteAudioRestartBackoff.h`: 1 s / 2 s / 4 s steps, `reset()`.
- `tests/tst_remote_media_controller.cpp`: the controller tests, including a test hook that
  holds the restart step (`audioRestartHeldForTest`).

## File Structure

| Path | Change |
| --- | --- |
| `src/gui/RemoteMediaController.cpp` (and `.h` if a slot is needed) | Hold no-packets restarts while the Core transmits; resume at once when it stops |
| `tests/tst_remote_media_controller.cpp` | The acceptance cases below |
| the R-R3 requirements doc, if a new ID is added | The new ID |

## Task 1: Resume remote receive audio at unkey

**Requirements:** R-R3-21 (restarts), extended: a no-packets restart while the Core
reports transmit is expected silence, not an outage; the end of transmit asks for audio
at once with a fresh backoff.

**Files:**
- Modify: `src/gui/RemoteMediaController.cpp`
- Test: `tests/tst_remote_media_controller.cpp`

**Interfaces:**
- Consumes: `Private::coreTransmitting()`, the existing transmit-state change notification
  from the `StationClient` (find the signal the controller or window already uses; do not
  invent one), `requestAudio()`, `RemoteAudioRestartBackoff::reset()`.
- Produces: nothing other code relies on.

**Acceptance:**
- While `coreTransmitting()` is true, a `restartRequested` with `Fault::NoPackets` from the
  speakers receiver stops it and schedules no backoff timer; the backoff streak does not
  advance. The status shown to the operator does not say audio failed.
- When the Core's transmit state goes from transmitting (keyed, tuning or txEnding) to not
  transmitting, and a restart was held, `requestAudio()` runs in that same event-loop pass
  (no timer), with the backoff reset. A test drives transmit on, a NoPackets restart,
  transmit off, and observes the audio request sent with no wait beyond processEvents.
- The same holds for the headphones receiver and for each per-slice receiver stream, each
  with its own backoff.
- A NoPackets restart while not transmitting behaves exactly as today (backoff 1 s, 2 s,
  4 s); the existing tests for that pass unchanged.
- A Core with `txStateVersion < 1` behaves exactly as today.
- Other faults (speaker, decoder, clock) while transmitting behave as today.
- If the unkey arrives while a held restart's context is already playing again, no second
  request is sent.

**Verification:** ordinary bug tier. Unit tests in `tst_remote_media_controller` for every
acceptance case, red before the fix where practical. Run `tst_remote_media_controller`,
`tst_remote_window_harness*`, `tst_station_session*` and any test with `remote_audio` in
its name. Hardware: JJ keys and unkeys on the Rock 5C Core and audio returns within about
a quarter second; pending until he does.

**Execution note (advisory):** opus. Touches the remote receive audio path (flag in the
PR), not networking config, secrets or anything that keys the radio.

- [ ] **Step 1:** Configure the build dir offline as in Global Constraints and build
  `tst_remote_media_controller`.
- [ ] **Step 2:** Write the acceptance tests and see the hold and resume cases fail.
- [ ] **Step 3:** Implement the hold and the immediate resume for speakers, headphones and
  per-slice streams.
- [ ] **Step 4:** Run the tests named in Verification; commit signed with the history lines.
