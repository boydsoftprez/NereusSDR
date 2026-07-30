# PR #293 stabilization — consolidated final-review fixes

## Scope and disposition

- Starting HEAD:
  `da3c92e6542d452bbd68fb4fce73dea3aea1e536`
- Findings verified and fixed: H1, H2, H3, M1, M2.
- Method: one red-green-refactor cycle per finding, followed by review of the
  consolidated diff and one exact focused build/CTest sweep.
- Correction commit: the single GPG-signed commit containing this report
  (`HEAD` at handoff).
- Signature verification: `git verify-commit HEAD` succeeds for that
  correction commit.
- Per the fix brief, no label-wide CTest, `all_tests`, smoke gate, or push was
  run in this wave. Those gates remain controller-owned after scoped
  re-review.

## H1 — TX filter source

### Verification

Verified. `pushTxModeAndBandpass()` took mode and filter bounds from the
TX-bound `SliceModel`, even though `SliceModel` filters are signed RX/IQ-space
values while `TxChannel::requestFilterChange()` accepts positive TX
audio-space cutoffs. The lower-sideband-family mapping therefore negated an
already signed input and could apply the TX passband on the wrong side of the
carrier.

### RED evidence

The focused test was changed to make the slice's signed RX filter deliberately
different from the `TransmitModel` TX filter, assert the positive cutoffs
emitted by `RadioModel`, and observe the final `TxChannel::txFilterApplied`
mapping for LSB, DIGL, CWL, and RADE_L.

Exact command:

```sh
cmake --build build --target tst_radio_model_push_tx_mode_and_bandpass -j2 && ctest --test-dir build -R '^tst_radio_model_push_tx_mode_and_bandpass$' --output-on-failure
```

Before the production change, the target failed: 1/1 CTest target failed and
7 QtTest cases exposed the signed-slice source. Representative mismatches
included an emitted `-3100` where positive `150` was expected and an applied
filter low of `300` where `-2350` was expected.

### Implementation

- Kept the TX-bound slice authoritative for `DSPMode`.
- Read low/high cutoffs from `TransmitModel`, preserving their positive
  audio-space contract.
- Added end-to-end `TxChannel` coverage for LSB, DIGL, CWL, and RADE_L, in
  addition to signal-level source checks.
- Updated the production contract comments to distinguish RX/IQ-space slice
  filters from TX audio-space cutoffs.

### GREEN evidence

The same exact command passed 1/1 focused target in 0.74 seconds.

### Self-review

The final path still resolves mode from the stable TX binding, does not fall
back to the active/listening slice, and does not introduce per-slice TX
cutoffs that the model does not own. Tests distinguish the two filter sources
instead of allowing equal values to hide a regression.

## H2 — TX antenna authority transfer

### Verification

Verified. A TX antenna change on a non-bound slice was intentionally ignored
by the Alex path, but a later TX handoff did not reconcile that slice's stored
antenna. The pre-MOX route could therefore use stale per-band Alex state.
`RxApplet` also wrote Alex state directly, making the result depend on which
UI surface performed the edit.

### RED evidence

The focused regression establishes a connected radio, stores stale ANT1 state
for the 10 m band, edits the non-bound transmitting slice to ANT3, hands TX to
that slice, enters MOX, and observes the physical routing.

Exact command:

```sh
cmake --build build --target tst_alex_tx_lpf_source -j2 && ctest --test-dir build -R '^tst_alex_tx_lpf_source$' --output-on-failure
```

Before the production change, the target failed with expected `txAnt=3` /
`trxAnt=3` but actual stale value `1`.

### Implementation

- Added one `RadioModel::applyTxAntennaFromBoundSlice()` authority path.
- Invoked it for a bound slice's TX antenna change, on TX binding change, and
  defensively before MOX routing.
- Kept non-bound edits as stored slice intent until the slice becomes bound.
- Removed direct RX/TX Alex writes from `RxApplet`; UI actions now mutate
  `SliceModel` only.
- Made the central RX antenna handler derive the band from the source slice
  rather than the unrelated global last-band value.

### GREEN evidence

The same exact command passed 1/1 focused target, including all 18 QtTest
cases plus init/cleanup, in 0.74 seconds.

### Self-review

All three TX authority boundaries use the same helper, AlexController remains
the ANT2/ANT3 safety gate, and the regression observes the connected
`AntennaRouting` result rather than only model state. Existing LPF assertions
also confirm that the bound slice's transmit band remains the routing source.

## H3 — mixer withdrawal boundary

### Verification

Verified. `MasterMixer::tryDrain()` checked streaming/generation before reading
the ring, but withdrawal could complete between that check and return. The
audio thread could then return and push an old-generation block after
`setSliceStreaming(false)` had returned. The existing sequential test could
not enter that race window.

### RED evidence

A deterministic two-thread seam pauses `tryDrain()` after barrier admission
and before ring consumption. The control thread withdraws the slice while the
drain is paused.

Exact command:

```sh
cmake --build build --target tst_master_mixer -j2 && ctest --test-dir build -R '^tst_master_mixer$' --output-on-failure
```

Before the production change, 22 QtTest cases passed and
`withdrawalDuringAdmittedDrainInvalidatesTheResult` failed: the paused drain
returned 1 frame instead of 0.

### Implementation

- Added a global mixer membership epoch published after each streaming-state
  change.
- Made `tryDrain()` stage ring cursors, available counts, per-slice gain
  ramps, and master slew position; it commits that audio-thread-owned state
  only after validating both the global epoch and participating slice
  generations/streaming state.
- Added a deterministic mixer admission seam for the race regression.
- Added an AudioEngine admission boundary covering the complete
  accumulate/drain/push region. The audio callback uses atomics only and
  never waits; withdrawal closes admission, invalidates both speaker and
  anti-VOX mixers, waits for already admitted regions to acknowledge exit,
  and then reopens admission.
- Added an AudioEngine two-thread regression proving withdrawal cannot return
  while an old mix is admitted and that no old speaker block is pushed after
  it returns.

### GREEN evidence

Exact command:

```sh
cmake --build build --target tst_master_mixer tst_audio_engine_multi_slice_mix -j2 && ctest --test-dir build -R '^(tst_master_mixer|tst_audio_engine_multi_slice_mix)$' --output-on-failure
```

Both focused targets passed (2/2) in 1.38 seconds.

### Self-review

Ring ownership remains on the audio thread. The audio callback does not take a
mutex, block, or wait on the control thread. Validation occurs before
transactional cursor/gain commit and before output can leave the admitted
AudioEngine region. Test cleanup was hardened so every spawned thread is
released and joined before an assertion can return from the test function.
The final sweep also retained the MOX gate-release and RX-leak coverage.

## M1 — extended-view policy versus actual state

### Verification

Verified. The applet toggle wrote actual `extendedMode` directly, while later
rate/range changes re-derived and overwrote it. As a result, “allowed” at
normal zoom incorrectly enabled wideband, “forced off” could be undone by
zoom, and settings-derived actual state could be established before the
MainWindow bridge existed without ever seeding the slice.

### RED evidence

Three regressions cover forced-off followed by wide zoom, allowed at normal
zoom, and restored 5 MHz zoom with a 192 kHz sample rate before bridge
installation.

Exact command:

```sh
cmake --build build --target tst_panadapter_applet_slice_assoc -j2 && ctest --test-dir build -R '^tst_panadapter_applet_slice_assoc$' --output-on-failure
```

Before the production change, 10 QtTest cases passed and all 3 new cases
failed: zoom re-enabled a forced-off pan, allowing at normal zoom incorrectly
made actual mode true, and installing the bridge did not seed the slice.

### Implementation

- Added persistent `extendedViewAllowed` policy state to `SpectrumWidget`.
- Derived actual mode with the required formula:
  `allowed && sampleRate > 0 && bandwidth > sampleRate`.
- Recomputed actual state on toggle, settings-loaded bandwidth, frequency
  range, and sample-rate changes.
- Changed `PanadapterApplet` to set policy, not actual state.
- Seeded the resolved slice from current actual state immediately after
  MainWindow installs the wideband bridge.

### GREEN evidence

The same exact command passed 1/1 target, all 13 QtTest cases, in 0.83 seconds.

### Self-review

Policy and derived state now have separate names and storage. Only actual
state is published downstream. The initialization regression intentionally
creates the true state before bridge installation, so it covers the lost-edge
case rather than relying on a later toggle. The related active-slice-sync
target also passes in the consolidated sweep.

## M2 — RF2KS live-retarget generation boundary

### Verification

Verified. `connectToAmp()` advanced the generation and stopped timers but left
old replies owned and preserved the previous connected state. Retargeting from
connected amp A to unavailable amp B could therefore remain falsely connected
and miss the reconnect path, while delayed A work remained in flight.

### RED evidence

Regressions cover connected A to dead B and direct A-to-B retargeting while
both `/info` responses are held. The latter asserts that only B's reply
remains owned and that a delayed A response cannot mutate B's state.

Exact command:

```sh
cmake --build build --target tst_rf2ks_connection_reconnect -j2 && ctest --test-dir build -R '^tst_rf2ks_connection_reconnect$' --output-on-failure
```

Before the production change, 10 QtTest cases passed and both new cases
failed: the direct retarget remained connected, and two replies remained in
flight where only B's single probe was expected.

### Implementation

Every explicit `connectToAmp()` call now:

1. stops poll and reconnect timers;
2. advances the connection generation;
3. copies, clears, and aborts all prior replies;
4. transitions a previously connected generation to disconnected and resets
   `connectedSinceMs`;
5. resets generation counters/backoff state and applies the new host/port;
6. issues the new target's `/info` admission probe.

Manual disconnect now also resets `connectedSinceMs`.

### GREEN evidence

The same exact command passed 1/1 target, all 12 QtTest cases, in 2.62 seconds.

### Self-review

Generation checks remain as a second defense against an abort racing a queued
finished signal. The new in-flight-count seam observes ownership without
changing reply behavior. The connected-to-dead-target test confirms the
observable disconnected transition and retry scheduling; the delayed-reply
test confirms endpoint isolation.

## Consolidated focused verification

Exact command:

```sh
cmake --build build --target \
  tst_radio_model_push_tx_mode_and_bandpass \
  tst_alex_tx_lpf_source \
  tst_audio_engine_mox_gate_release \
  tst_audio_engine_rx_leak_during_mox \
  tst_master_mixer \
  tst_audio_engine_multi_slice_mix \
  tst_panadapter_applet_slice_assoc \
  tst_pan_active_slice_sync \
  tst_rf2ks_connection_reconnect \
  tst_rf2ks_connection_poll -j2 && \
ctest --test-dir build -R '^(tst_radio_model_push_tx_mode_and_bandpass|tst_alex_tx_lpf_source|tst_audio_engine_mox_gate_release|tst_audio_engine_rx_leak_during_mox|tst_master_mixer|tst_audio_engine_multi_slice_mix|tst_panadapter_applet_slice_assoc|tst_pan_active_slice_sync|tst_rf2ks_connection_reconnect|tst_rf2ks_connection_poll)$' --output-on-failure
```

Outcome: 10/10 named targets passed, 0 failures, 9.64 seconds total.

| Target | Time |
|---|---:|
| `tst_radio_model_push_tx_mode_and_bandpass` | 0.76 s |
| `tst_alex_tx_lpf_source` | 0.67 s |
| `tst_audio_engine_mox_gate_release` | 0.79 s |
| `tst_audio_engine_rx_leak_during_mox` | 0.69 s |
| `tst_master_mixer` | 0.40 s |
| `tst_audio_engine_multi_slice_mix` | 0.75 s |
| `tst_panadapter_applet_slice_assoc` | 0.66 s |
| `tst_pan_active_slice_sync` | 0.81 s |
| `tst_rf2ks_connection_reconnect` | 2.45 s |
| `tst_rf2ks_connection_poll` | 1.65 s |

## Final self-review and concerns

- Reviewed the complete production/test diff after all five fixes.
- `git diff --check` is clean.
- No code path moved mixer ring ownership off the audio thread or introduced
  a blocking audio-thread wait.
- No direct UI Alex mutation remains in the touched RxApplet antenna paths.
- No new or renamed CTest target was added.
- Focused builds emitted pre-existing linker warnings that the deepfilter
  archive was built for macOS 26.4 while the current link targets macOS 26.0.
  They did not affect any focused test.
- A broad incremental rebuild also surfaced an existing unrelated
  `AppearanceSetupPages` unused-lambda-capture compiler warning; that file was
  not changed in this wave.
- Full/label/smoke verification and push remain intentionally unperformed and
  controller-owned.
