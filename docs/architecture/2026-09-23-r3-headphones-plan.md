# R3 headphones implementation plan

> **Execution:** run with `crew` under `cost-aware-execution`. Requirements and
> acceptance cases are binding; one whole-branch review at the end of the batch.
> Task 1 can run in either lane; Task 2 runs after the receiver audio plan's Tasks
> 1-3 (it reuses their extra-stream machinery).

**Goal:** the Headphones output works, as the audio design always intended: each
receiver plays on the speakers or on the headphones device, chosen on its VFO flag,
with a local radio and in a remote window.

**Architecture:** each slice carries an output route (speakers or headphones). The
master mixer builds two mixes, one per output, from the same per-slice gain, mute
and pan it applies today; the audio engine opens the headphones output at startup
when one is configured and feeds it the headphones mix. In a remote window the route
is a mirrored slice property owned by the Core; while any receiver is routed to the
headphones the Core builds and sends a second mix on its own stream, and the window
plays it on the headphones device with its own rate matching.

**Tech stack:** C++20, Qt 6, Qt Test (off-screen), `SliceModel`, `MasterMixer`,
`AudioEngine`, `VfoWidget`, `MirrorPolicy`, `DaemonAudioSource`,
`DaemonMediaController`, `RemoteMediaController`, `RemoteAudioReceiver`.

**Spec:** operator answer of 2026-09-23: build the headphones as originally designed.
Design: `docs/architecture/2026-04-19-vax-design.md` sections 3.4, 5.1, 6.2
("SPEAKERS/HDPHN buttons: exclusive group -> `SliceModel::setOutputRoute(Output)`;
`MasterMixer` routes the slice to speakers OR headphones bus; `Slice<sliceId>/OutputRoute`")
and 6.3 (master volume and mute act on the speakers output). Scout notes:
`/Users/j.j.boyd/.config/nereus/work/r3-remote-vax-audio-pages-scout-2026-09-23.md`
section B (the headphones output is opened only on a Devices change and nothing feeds
it; the remote player drives one device). R3 plan requirement new R-R3-45 (added to
the R3 plan's requirement table with this plan), R-R3-06, R-R3-23.

## Global Constraints

- Work in the worktree, branch and build directory the controller names at dispatch.
- Commits: GPG-signed with hooks (`NEREUS_THETIS_DIR=/Users/j.j.boyd/Thetis`), never
  `--no-gpg-sign` or `--no-verify`, no `Co-Authored-By`, no em-dash characters. Stage
  explicit paths only. Every commit names its R-R3 IDs.
- Follow the design where it is specific; where it is silent, the simplest behaviour
  that matches it, named in the report for the operator's checkpoint review.
- Nothing on the audio callback allocates, locks or does network work.
- Protocol: a per-feature capability version for the headphones mix; never a change
  to `kSessionProtocolMinor`; older apps and Cores see today's wire.
- Operator wording: plain user words; every new string passes `OperatorWording::isPlain`.
- Tests: prefix every ctest and test binary with `QT_QPA_PLATFORM=offscreen`. Build
  exact targets, then `ctest -R '^(...)$' --no-tests=error --output-on-failure`. No
  unfiltered suite. Never build the `NereusSDR` target. No hardware.

## Task 1: Speakers or headphones per receiver, with a local radio

**Requirements:** R-R3-45, R-R3-06.

**Files:**
- Modify: `src/models/SliceModel.{h,cpp}` (`outputRoute` property, speakers by default,
  persisted as `Slice<sliceId>/OutputRoute`), `src/core/audio/MasterMixer.{h,cpp}` (two
  mixes from the same per-slice gain, mute and pan), `src/core/AudioEngine.{h,cpp}` (open
  the headphones output at startup when configured, feed it the headphones mix; master
  volume and mute on the speakers output as designed), `src/gui/widgets/VfoWidget.cpp`
  (SPEAKERS and HDPHN exclusive buttons in the per-receiver audio block)
- Test: `tests/tst_slice_model_phase3f_properties.cpp` (the route property and
  persistence), `tests/tst_master_mixer.cpp` (two mixes), `tests/tst_audio_engine_multi_slice_mix.cpp`,
  `tests/tst_audio_engine_master_mute.cpp`, `tests/tst_audio_engine_speakers_live_reconfig.cpp`
  (the headphones output beside the speakers)

**Acceptance:**
- Slice A on speakers and slice B on headphones: the speakers output carries only A,
  the headphones output only B, each at its slice gain, pan and mute.
- The route persists per slice and restores on restart.
- With no headphones device configured, a slice routed to headphones is silent and its
  flag says why in plain words.
- The master volume and mute act on the speakers output (design 6.3).

**Verification:**
```sh
cmake --build <build dir> --target tst_slice_model_phase3f_properties tst_master_mixer tst_audio_engine_multi_slice_mix tst_audio_engine_master_mute tst_audio_engine_speakers_live_reconfig -j6
QT_QPA_PLATFORM=offscreen ctest --test-dir <build dir> -R '^(tst_slice_model_phase3f_properties|tst_master_mixer|tst_audio_engine_multi_slice_mix|tst_audio_engine_master_mute|tst_audio_engine_speakers_live_reconfig)$' --no-tests=error --output-on-failure
```
Hardware (pending, operator checkpoint): two receivers split between speakers and
headphones.

**Execution note (advisory):** opus.

- [ ] **Step 1:** Route property, two mixes, headphones output, flag buttons; commit.

## Task 2: Headphones in a remote window

**Requirements:** R-R3-45, R-R3-23, R-R3-43.

**Files:**
- Modify: `src/core/session/MirrorPolicy.cpp` (`outputRoute` mirrored two-way),
  `src/core/session/media/DaemonAudioSource.{h,cpp}` and
  `src/core/session/media/DaemonMediaController.{h,cpp}` (a headphones mix stream,
  built and sent only while some receiver is routed to the headphones and the app
  declared support, at the session's audio quality), `src/core/session/StationCapabilities.{h,cpp}`
  (`headphonesMixVersion`), `src/gui/RemoteMediaController.{h,cpp}` and
  `src/core/session/media/RemoteAudioReceiver.{h,cpp}` (play the headphones mix on the
  headphones device with its own rate matching, isolated so a headphones failure never
  stops the speakers), the media control document
- Test: `tests/tst_daemon_media_controller.cpp`, `tests/tst_remote_media_controller.cpp`,
  `tests/tst_remote_audio_session.cpp`

**Interfaces:**
- Consumes: the receiver audio plan's declared extra streams and consumer machinery
  (Tasks 1-3).

**Acceptance:**
- In a remote window, routing slice B to headphones starts a second mix from the Core;
  the headphones device plays B, the speakers keep A; routing B back stops the second
  stream.
- The lossless link trial counts the headphones mix; one fallback moves every stream.
- A headphones device failure is reported in plain words and leaves the speakers
  playing.
- Older apps and Cores: today's wire, and a plain reason on the flag when the Core
  cannot send a headphones mix.

**Verification:**
```sh
cmake --build <build dir> --target tst_daemon_media_controller tst_remote_media_controller tst_remote_audio_session -j6
QT_QPA_PLATFORM=offscreen ctest --test-dir <build dir> -R '^(tst_daemon_media_controller|tst_remote_media_controller|tst_remote_audio_session)$' --no-tests=error --output-on-failure
```
Hardware (pending, operator checkpoint): the same split through the Rock.

**Execution note (advisory):** opus. Requires Task 1 and the receiver audio plan's
Tasks 1-3.

- [ ] **Step 1:** Core headphones mix, window playback, capability; commit.
