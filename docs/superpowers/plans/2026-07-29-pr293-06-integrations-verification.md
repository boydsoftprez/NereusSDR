# PR 293 Stabilization 06 — Integrations, Smoke Test, and Delivery Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Close RF2KS and TCI identity/lifecycle defects, then prove the complete PR through focused tests, subsystem tests, the full suite, exact pinned-source compliance, a new application build, and an isolated smoke launch before pushing signed commits.

**Architecture:** Every RF2KS request carries a connection generation and is tracked until destruction. Manual disconnect invalidates and aborts all replies; initial failure enters backoff; reconnect probes only `/info`, and periodic polling starts only after success. TCI enumerates and publishes stable slice IDs consistently for both existing and newly added slices. Delivery uses fresh evidence from the final source state.

**Tech Stack:** C++20, Qt 6 Network/WebSockets, CMake/CTest, Python compliance scripts, macOS application bundle, Git/GPG/GitHub.

---

## Files

- Modify: `src/core/Rf2ksConnection.h`
- Modify: `src/core/Rf2ksConnection.cpp`
- Modify: `src/core/TciServer.cpp`
- Test: `tests/tst_rf2ks_connection_reconnect.cpp`
- Test: `tests/tst_rf2ks_connection_poll.cpp`
- Test: `tests/tst_rf2ks_connection_control.cpp`
- Test: `tests/tst_tci_init_burst_live_state.cpp`
- Test: `tests/tst_tci_dispatch_seam.cpp`

## Task 1: Invalidate in-flight RF2KS replies on disconnect/reconnect

- [ ] **Step 1: Add a delayed-reply generation regression**

Extend `tests/tst_rf2ks_connection_reconnect.cpp` with a local TCP server that accepts `/info` but delays its response. Then:

1. call `connectToAmp`;
2. wait until the request is observed;
3. call `disconnect`;
4. release a valid delayed response;
5. assert no `connected`, state-update, success-counter, or reconnect signal occurs;
6. connect to a second server/generation and assert the first generation cannot change the second's state.

Use `QSignalSpy` and `QTRY_*`; do not use fixed sleeps.

- [ ] **Step 2: Confirm the stale reply currently reconnects the object**

```bash
cmake --build build --target tst_rf2ks_connection_reconnect -j
ctest --test-dir build -R '^tst_rf2ks_connection_reconnect$' --output-on-failure
```

- [ ] **Step 3: Track generation and every reply**

Add to `Rf2ksConnection`:

```cpp
QSet<QNetworkReply*> m_inFlight;
quint64 m_generation{0};
bool m_operatorDisconnected{true};
```

Add `trackReply(QNetworkReply*)` that:

- stores `rfkitGeneration` on the reply;
- inserts it into `m_inFlight`;
- removes it on `destroyed`;
- connects `finished` to `onReplyFinished`.

Use it for GET, PUT, and POST.

- [ ] **Step 4: Make disconnect a hard generation boundary**

`disconnect()` must:

- set `m_operatorDisconnected=true`;
- increment `m_generation`;
- stop poll and reconnect timers;
- abort every tracked reply;
- clear the tracked set after abort requests are issued;
- emit `disconnected` only when the prior public state was connected.

`connectToAmp()` starts a new generation, sets `m_operatorDisconnected=false`, stores host/port, resets counters/backoff, and issues only the immediate `/info` probe.

In `onReplyFinished`, remove/delete the reply but perform no parsing, counters, signals, or scheduling when its generation differs or `m_operatorDisconnected` is true.

- [ ] **Step 5: Rerun the delayed-reply regression**

Build and run `tst_rf2ks_connection_reconnect`.

## Task 2: Put initial failures into real exponential backoff

- [ ] **Step 1: Add initial-connect and probe-only tests**

In `tests/tst_rf2ks_connection_poll.cpp` and `tests/tst_rf2ks_connection_reconnect.cpp`, assert:

- the first failed `/info` on an object that was never connected stops normal polling and arms reconnect;
- reconnect timeout sends exactly one `/info`;
- hot-path polling remains stopped while that probe is outstanding or fails;
- a successful `/info` marks connected, resets backoff, and only then starts staggered polling;
- manual disconnect suppresses all retries;
- auto-reconnect false suppresses all retries.

Use the existing compressed timing seams.

- [ ] **Step 2: Confirm the initial-failure test fails**

```bash
cmake --build build --target tst_rf2ks_connection_poll tst_rf2ks_connection_reconnect -j
ctest --test-dir build -R '^(tst_rf2ks_connection_poll|tst_rf2ks_connection_reconnect)$' --output-on-failure
```

- [ ] **Step 3: Separate probe state from polling**

- Do not start `m_pollTimer` in `connectToAmp`.
- When an initial `/info` fails, call `scheduleReconnect()` immediately; there are no parallel hot-path requests to drain.
- For an established connection, retain the three-consecutive-failure threshold, then stop polling, emit one disconnect transition, and schedule reconnect.
- `onReconnectTimeout()` issues `/info` only.
- A successful current-generation reply marks connected and starts the staggered poll timer if it is not already active.
- Prevent duplicate reconnect scheduling while `m_reconnectTimer` is active.

- [ ] **Step 4: Run all RF2KS tests**

```bash
cmake --build build --target tst_rf2ks_connection_reconnect \
  tst_rf2ks_connection_poll tst_rf2ks_connection_control -j
ctest --test-dir build -R '^(tst_rf2ks_connection_reconnect|tst_rf2ks_connection_poll|tst_rf2ks_connection_control)$' --output-on-failure
```

## Task 3: Enumerate TCI receivers by stable slice ID

- [ ] **Step 1: Add an existing-slice deletion-gap regression**

In `tests/tst_tci_init_burst_live_state.cpp`, create A/B/C, remove B, then attach TCI and request the initial state burst. Assert C is announced and subsequently updated as receiver ID `2`, not positional receiver `1`.

In `tests/tst_tci_dispatch_seam.cpp`, compare existing-slice wiring with a newly added slice of the same stable ID and assert identical receiver identifiers.

- [ ] **Step 2: Confirm existing and new wiring disagree**

```bash
cmake --build build --target tst_tci_init_burst_live_state tst_tci_dispatch_seam -j
ctest --test-dir build -R '^(tst_tci_init_burst_live_state|tst_tci_dispatch_seam)$' --output-on-failure
```

- [ ] **Step 3: Pass stable IDs from both paths**

In `TciServer::hookSliceBroadcasts()`, replace the positional loop argument with:

```cpp
wireSliceForBroadcast(slice, slice->sliceIndex());
```

Keep the `sliceAdded(int)` path resolving the stable ID through `sliceById`. Rename the `wireSliceForBroadcast` parameter to `sliceId` if its current `rxIndex` name encourages positional use.

- [ ] **Step 4: Audit and rerun TCI tests**

```bash
rg -n "wireSliceForBroadcast|slices\\(\\)\\.at\\(|sliceAdded" src/core/TciServer.cpp
cmake --build build --target tst_tci_init_burst_live_state tst_tci_dispatch_seam -j
ctest --test-dir build -R '^(tst_tci_init_burst_live_state|tst_tci_dispatch_seam)$' --output-on-failure
```

## Task 4: Verify and commit integrations

- [ ] **Step 1: Run focused integration tests**

Build and run the five exact targets listed above.

- [ ] **Step 2: Run core tests**

```bash
cmake --build build --target tests_core -j
ctest --test-dir build -L core --output-on-failure
```

- [ ] **Step 3: Review and sign the integration commit**

```bash
git diff --check
git diff -- src/core/Rf2ksConnection.h src/core/Rf2ksConnection.cpp src/core/TciServer.cpp tests
git add src/core/Rf2ksConnection.* src/core/TciServer.cpp \
  tests/tst_rf2ks_connection_reconnect.cpp tests/tst_rf2ks_connection_poll.cpp \
  tests/tst_rf2ks_connection_control.cpp tests/tst_tci_init_burst_live_state.cpp \
  tests/tst_tci_dispatch_seam.cpp
NEREUS_THETIS_DIR=/Users/j.j.boyd/Thetis \
NEREUS_MI0BOT_DIR=/Users/j.j.boyd/mi0bot-Thetis \
NEREUS_DESKHPSDR_DIR=/Users/j.j.boyd/deskhpsdr \
NEREUS_FREEDV_DIR=/Users/j.j.boyd/freedv-gui \
git commit -S -m "fix(integrations): harden RF2KS and TCI lifecycles"
git log --show-signature -1
```

## Task 5: Reconcile the final branch with the live PR

- [ ] **Step 1: Fetch immediately before final verification**

```bash
git fetch origin feature/phase3f-sub-epic-a-foundation
git log --oneline --decorate --left-right HEAD...origin/feature/phase3f-sub-epic-a-foundation
git merge-base --is-ancestor origin/feature/phase3f-sub-epic-a-foundation HEAD
```

Expected: the remote branch is an ancestor. If it moved independently, inspect every new commit and integrate it before continuing; do not force-push or discard it.

- [ ] **Step 2: Confirm every implementation commit is signed**

```bash
git log --format='%H %G? %s' 40cc91907870f6811ac8ac2a661f6f862795f2ad..HEAD
```

Expected: each locally created commit shows `%G?` value `G`.

## Task 6: Run final focused and subsystem verification

- [ ] **Step 1: Build every modified regression target**

Use the target lists in Plans 02–06. Because tests are excluded from the default build, do not skip this explicit build.

- [ ] **Step 2: Run the combined defect ledger**

```bash
ctest --test-dir build -R '^(tst_tx_slice_arbiter|tst_tx_slice_binding_invariant|tst_pan_badge_click_wiring|tst_vfo_widget_tx_badge_click|tst_pan_active_slice_sync|tst_pan_floating_window|tst_panadapter_stack_layouts|tst_spectrum_overlay_panel|tst_panadapter_applet_slice_assoc|tst_slice_agc_advanced|tst_radio_model_push_tx_mode_and_bandpass|tst_band_plan_guard_mox_rejection|tst_alex_tx_lpf_source|tst_audio_engine_mox_gate_release|tst_audio_engine_rx_leak_during_mox|tst_radio_model_mox_hardware_flip|tst_tx_frequency_follows_tx_slice|tst_codec_5_slice_assignment|tst_p2_ddc_mask_ownership|tst_p2_radio_connection_apply_ddc_assignment|tst_p2_ddc_assignment_marshalling|tst_stream_pool_binding|tst_alex_per_adc_bpf_wire|tst_radio_model_puresignal_run_wiring|tst_radio_model_hpsdr_model_push|tst_rx_channel_ext_div_wrappers|tst_rx_dsp_worker_multi_slice|tst_master_mixer|tst_audio_engine_multi_slice_mix|tst_rf2ks_connection_reconnect|tst_rf2ks_connection_poll|tst_rf2ks_connection_control|tst_tci_init_burst_live_state|tst_tci_dispatch_seam)$' --output-on-failure
```

- [ ] **Step 3: Build and run each affected subsystem**

```bash
cmake --build build --target tests_core tests_models tests_gui -j
ctest --test-dir build -L core --output-on-failure
ctest --test-dir build -L models --output-on-failure
ctest --test-dir build -L gui --output-on-failure
```

Expected: all pass.

## Task 7: Run the complete suite and exact pinned compliance

- [ ] **Step 1: Build every test executable from the final source**

```bash
cmake --build build --target all_tests -j
```

- [ ] **Step 2: Run all registered tests**

```bash
ctest --test-dir build --output-on-failure
```

Expected: zero failures and zero "Not Run".

- [ ] **Step 3: Run compliance against the exact CI pins from Plan 01**

```bash
NEREUS_THETIS_DIR="$PR293_PIN_ROOT/Thetis" \
NEREUS_MI0BOT_DIR="$PR293_PIN_ROOT/mi0bot-Thetis" \
NEREUS_DESKHPSDR_DIR=/Users/j.j.boyd/deskhpsdr \
NEREUS_FREEDV_DIR="$PR293_PIN_ROOT/freedv-gui" \
pre-commit run --all-files
```

Expected: every hook passes.

## Task 8: Configure a new application build and smoke-launch it

- [ ] **Step 1: Configure a build directory with no stale objects**

```bash
PR293_SMOKE_BUILD="$(mktemp -d /tmp/nereus-pr293-smoke-build.XXXXXX)"
cmake -S . -B "$PR293_SMOKE_BUILD" \
  -DCMAKE_BUILD_TYPE=RelWithDebInfo \
  -DNEREUS_BUILD_TESTS=OFF
```

- [ ] **Step 2: Build the application from scratch**

```bash
cmake --build "$PR293_SMOKE_BUILD" --target NereusSDR -j
```

Expected on macOS: `"$PR293_SMOKE_BUILD/NereusSDR.app/Contents/MacOS/NereusSDR"` exists.

- [ ] **Step 3: Launch with an isolated settings profile**

Choose a unique valid profile name such as `pr293_smoke_20260729`. Launch the bundle executable with that profile and capture output:

```bash
"$PR293_SMOKE_BUILD/NereusSDR.app/Contents/MacOS/NereusSDR" \
  --profile pr293_smoke_20260729 \
  >"$PR293_SMOKE_BUILD/smoke.log" 2>&1 &
PR293_SMOKE_PID=$!
```

This profile writes only beneath NereusSDR's profile-specific settings directory and cannot overwrite the operator's default settings.

- [ ] **Step 4: Observe liveness, logs, and graceful termination**

For ten one-second intervals, verify `kill -0 "$PR293_SMOKE_PID"` succeeds. Inspect `smoke.log` for fatal/assert/crash markers. Send `SIGTERM`, wait for the process, and confirm it exits through the installed graceful Qt shutdown path rather than a crash.

Record the isolated profile and smoke-build paths in the final handoff. Do not delete them during this workflow; they are evidence and are narrowly scoped.

## Task 9: Final diff audit, signed correction if needed, and push

- [ ] **Step 1: Inspect final repository state**

```bash
git diff --check
git status --short --branch
git log --show-signature --oneline 40cc91907870f6811ac8ac2a661f6f862795f2ad..HEAD
```

Expected: clean worktree, only intended signed commits, no untracked build products in the repository.

- [ ] **Step 2: Commit any verification-driven correction separately**

If final verification required a source/test change, rerun its focused test plus the affected subsystem, stage only that correction, run pinned hooks, and create a signed `fix(...)` commit. Do not amend already reviewed implementation commits.

- [ ] **Step 3: Fetch once more and push without force**

```bash
git fetch origin feature/phase3f-sub-epic-a-foundation
git merge-base --is-ancestor origin/feature/phase3f-sub-epic-a-foundation HEAD
git push origin HEAD:feature/phase3f-sub-epic-a-foundation
```

Expected: normal fast-forward push succeeds and updates pull request #293.

- [ ] **Step 4: Verify the published head**

```bash
git ls-remote origin refs/heads/feature/phase3f-sub-epic-a-foundation
git rev-parse HEAD
```

Expected: both hashes match. Report focused/subsystem/full-suite counts, pinned compliance, clean-build path, smoke PID outcome/log path, commit hashes/signature status, and the pushed branch.
