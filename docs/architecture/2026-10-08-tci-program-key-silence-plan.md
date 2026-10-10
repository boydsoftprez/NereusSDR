# TCI program key: silence, never the microphone (remote window)

> Run with `crew` under `cost-aware-execution`. One task, one implementer, one
> whole-branch review at the end.

**Goal.** In a remote window, a TCI app that keys with its own audio
(`trx:N,true,tci;`) keys the Core at once, with silence on the window's
microphone line until the app's audio arrives. The PC microphone never goes out
while such a key is waiting or on. A failed microphone capture is retried when
a later key needs it.

**Why (bench, 2026-10-08, window profile `radxa_5c_r3`, Core on the Rock).**
WSJT-X showed "TCI failed to set ptt" on every transmit period.

- The window's microphone capture failed at launch: `capture: no microphone
  samples within 10000 ms ... Failed Timeout`. Nothing retries it. Only the
  Retry microphone button in Setup → Audio → Microphone does.
- On each `trx`, the window forwarded the key (`TciServer::handleRemoteTrx`).
  The Core then waited for the window's microphone line
  (`RemoteKeying.cpp`, `keyWaitsForMicrophone` → `m_mic.prime`), logged
  `Remote microphone: no packet on the line within 1000 ms of the key`, and
  refused with "No sound has reached the Core from this device's microphone".
- WSJT-X waits 1000 ms for the `trx` answer. In the JTDX `TCITransceiver.cpp`
  `do_ptt`, which WSJT-X's TCI support shares, the wait is `mysleep3(1000)`.
  It sends `trx:<rx>,<bool>,tci;` when the server identifies as ExpertSDR3,
  which is NereusSDR's default. So it dropped the connection about 930 ms in,
  before the Core's refusal.
- The two waited on each other: WSJT-X streams TX audio only after `TX_CHRONO`,
  which `handleRemoteTrx` starts only after the Core accepts the key.
- With a working microphone, the line instead carried this computer's
  microphone until WSJT-X's audio arrived. That put room sound on the air at
  the start of every FT8 transmission. `RemoteMediaController::reconcileMicUplink`
  sends `micPending` whenever `programUntilMs` has lapsed.

**Core: no change.** The Core's readiness counts frames, not loudness
(`RemoteMicReceiver::checkReady`: `framesSinceInUse() >= targetFrames()`,
target 30 ms, `RemoteMicConfig::kTargetDepthMs`). Silence starts the line and
fills it. No wire or version change: silence is ordinary microphone RTP.

**Requirements.** R-IOS-13 (desktop remote transmit, Task 35/36 of
`2026-09-23-iphone-app-plan.md`: the window forwards an app's key and sends the
app's audio on the microphone line); R-R3-42 (a remote window's TCI acts on the
Core).

**Worktree / branch.**
`/Users/j.j.boyd/NereusSDR/.claude/worktrees/tci-ptt-wsjtx-95dd87`, branch
`claude/tci-ptt-wsjtx-95dd87`, based on `main` at `a0681c508`.

## Global Constraints

- `CLAUDE.md` binds. In particular: braces on all control flow; no raw
  `new`/`delete` (Qt parent or `unique_ptr`); `constexpr`, never `#define`;
  `qCInfo`/`qCWarning(lcCategory)`, no exceptions; `m_camelCase`,
  `kPascalCase`; `auto` only when the type is obvious; don't remove code you
  didn't add.
- Rule R1: nothing under `src/core/` or `src/models/` includes a GUI header.
  `RemoteTransmitClient` and `TciServer` are core; `RemoteMediaController` and
  `RemoteTransmitForwarder` are gui.
- NereusSDR-original code (`// no-port-check:` files). No upstream headers. Each
  touched file's `Modification history (NereusSDR)` block gets a dated line:
  `2026-10-08: <what>. J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude
  Code.` Use the house style of each file's existing block.
- Commits are GPG-signed (never `--no-gpg-sign`). No `Co-Authored-By: Claude`
  trailer. A signing failure is reported, not worked around.
- No em-dashes in new comments, log lines, user-facing strings or commit
  messages. Log lines and comments use plain English, as the surrounding code
  does.
- Tests: read `docs/development/fast-test-loop.md`. Build single test targets,
  never the whole suite. Run test binaries directly with
  `QT_QPA_PLATFORM=offscreen` as a prefix on the command, never exported over
  `ctest`. This worktree has no `build/` yet. Configure it with
  `cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo` (a fresh build
  dir downloads Opus once; that is expected).
- Do not touch any device. No deploys to the Rock, no launching the app for JJ.
  The controller owns bench steps.

## Task 1: Program-audio keys send silence, never the microphone; a failed capture retries on a key

**Model tier.** Opus (keying path).

**Files.**

| File | Change |
|---|---|
| `src/core/TciServer.h` / `.cpp` | `RemoteTransmit::key` takes `bool programAudio` before the answer callback. `handleRemoteTrx` passes its `hasTciArg`. |
| `src/gui/RemoteTransmitForwarder.cpp` | Passes `programAudio` through to `RemoteTransmitClient::keyForProgram`. |
| `src/core/session/RemoteTransmitClient.h` / `.cpp` | `keyForProgram(bool programAudio, answer)` records it on the program key, before `publish()` emits. New `bool programAudioKey() const`: true while the program key is waiting or on and was asked with `programAudio`. It resets with the key (`m_program = Key{}` everywhere it already resets). |
| `src/gui/RemoteMediaController.h` / `.cpp` | The silence rule and the capture retry in `reconcileMicUplink` (below). |
| `tests/tst_tci_remote_window.cpp` | Update the one fake `forward.key`; add the `,tci` cases. |
| `tests/tst_remote_media_controller.cpp` | New cases below. Use the existing harness (`Test::RemoteAudioSessionHarness`, `FakeAudioBus`, `DaemonMediaController`) as the program-audio tests near `radioSourceAckAloneStopsMacMicrophoneAndRestoresCleanClientPackets` do. |
| A `RemoteTransmitClient` test, if one exists (find it) | `programAudioKey()` lifecycle. |

**Interfaces.**

```cpp
// TciServer.h
struct RemoteTransmit {
    /// Sends tx.key {trigger:"tci"}. programAudio: the app's trx carried
    /// ",tci", so its own audio follows on the TX audio stream. `answer` runs
    /// once, on this object's thread, with the Core's verdict.
    std::function<void(bool programAudio,
                       std::function<void(const RemoteKeyAnswer&)> answer)> key;
    ...
};

// RemoteTransmitClient.h
void keyForProgram(bool programAudio, std::function<void(const Answer&)> answer);
/// The program key (waiting or on) brings its own audio: the window's
/// microphone line carries that audio or silence, never the microphone.
bool programAudioKey() const;
```

**Behaviour (`RemoteMediaController::reconcileMicUplink`).**

1. While `programAudioKey()` is true and the uplink runs:
   - If program audio is current (`clock.elapsed() < programUntilMs`), send
     `programPending` as today.
   - Otherwise send silence at the line's real-time rate (48 kHz,
     `RemoteMicConfig::kSampleRate`), paced by `d->clock`. Do not count pump
     ticks: the pump is a 10 ms `QTimer` and can stall. Keep the first silent
     packet close to the uplink's start, so the Core's 1000 ms line-start
     deadline and 30 ms fill are met at once. Bound any catch-up after a
     stall (for example, at most `RemoteMicConfig::kMaxDepthMs` of silence in
     one pump) so a stall never floods the Core's buffer.
   - Microphone samples pulled from the capture are drained and dropped, never
     queued, never sent. This holds when program audio lapses mid-key too:
     silence resumes, never the microphone.
   - This path must not depend on the capture lease. With no capture, or a
     capture in `Failed`, silence still flows. Do not acquire capture demand
     for a program-audio key. Keep the existing preview rule, which may hold
     the lease for its own reasons.
2. Keys that are not program-audio keys (the screen key, VOX, a program key
   without `,tci`) behave exactly as today.
3. Capture retry: on the uplink's start edge (`wanted && !micRunning`), when
   the line will carry the microphone (not `programAudioKey()`) and
   `AudioEngine::captureStatus().state == Failed`, call
   `AudioEngine::retryCapture()` once and log it
   (`qCInfo(lcRemoteMedia) << "Microphone capture had failed; retrying for this key"`).
   Never on every pump; never for a program-audio key.

**Acceptance cases (each a test).**

- A. Program-audio key, capture delivering nothing (empty `FakeAudioBus`):
  after the uplink starts, microphone packets flow at real-time rate. Every
  sample decodes as zero. The `DaemonMediaController`/`RemoteKeying` side of
  the harness admits the key (it answers accepted, not "No sound has
  reached the Core").
- B. Program-audio key, capture delivering a non-zero tone: no tone sample
  reaches the line before, during or after `pushProgramAudio`. Packets are
  silence, then the program's samples. Prove the check can fail: the same
  setup with `programAudio == false` sends the tone.
- C. Program audio stops for longer than `kProgramAudioHoldMs` during the key:
  silence follows, not the tone.
- D. After the program-audio key ends, a screen key (`setMicKeyDown(true)`)
  sends the microphone again. Nothing stays silenced.
- E. `TciServer`: `trx:0,true,tci;` calls `forward.key` with
  `programAudio == true`; `trx:0,true;` calls it with `false`. Existing
  remote-window TCI tests stay green.
- F. Capture retry: capture in `Failed`, a screen key starts the uplink:
  `retryCapture` runs once (observe the capture leaving `Failed`, or a
  generation bump through `captureStatusChanged`). A second pump does not
  retry again. A program-audio key does not retry.
- G. Pacing: with a pump stall simulated (no pump for 300 ms), the silence
  sent across the stall is bounded as specified, and the rate settles back to
  real time.

**Verification.**

- Code: build and run, offscreen, `tst_remote_media_controller`,
  `tst_tci_remote_window`, the `RemoteTransmitClient` test and any other
  test that uses `RemoteTransmit::key` or `keyForProgram`. Find them with
  `grep -rln "keyForProgram\|RemoteTransmit::key\|forward.key" tests`. Then
  run `tests_gui` once on the finished revision. Put the test summary in the
  ledger's `complete` line.
- Hardware: **pending.** JJ keys WSJT-X (FT8, TCI audio) through the remote
  window to the Rock Core: the key is accepted within WSJT-X's 1000 ms, there
  is no "TCI failed to set ptt", and no room sound at the start of the
  transmission. That run is the controller's and JJ's step, not the
  implementer's.

## Settled calls (listed for JJ's veto)

1. Silence applies only to a key whose `trx` carried `,tci`. A key without it
   (an app on a sound card or VAX) keeps the microphone line as today.
2. While a program-audio key is waiting or on, the program is the only source.
   The window's MOX button or VOX does not put the microphone on the line
   during it.
3. A program-audio key does not open the microphone capture.
4. A failed capture retries once per key start, for microphone keys only, with
   no background retry loop.
5. Dropped from the earlier list: the window refusing a key at once while its
   microphone is down. WSJT-X keys no longer wait on the microphone, and a
   microphone key now retries the capture first. A refusal before the Core
   answers would race that retry.
