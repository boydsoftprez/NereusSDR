# PR 293 Stabilization 04 — Protocol Ownership and Diversity Lifecycle Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Preserve the complete multi-slice P2 DDC assignment across MOX/PS/diversity changes, publish the codec's physical routing consistently, and replace unsafe external-diversity setter calls with a complete create/configure/feed/stop/destroy lifecycle.

**Architecture:** `DdcAssignment` is the only object allowed to rewrite Protocol 2 DDC enable/rate/ADC state. The legacy `PsDdcConfig` writer remains Protocol 1-only. Effective PureSignal run state enters the codec context, and the published assignment updates ReceiverManager plus every hosted slice's DDC and physical chain. WDSP external diversity is owned globally by `WdspEngine` because its IDs index `pdiv[MAX_EXT_DIVS]`, while `RxDspWorker` synchronizes the two source streams and feeds one diversity output into the target slice channel.

**Tech Stack:** C++20, Qt 6, HPSDR Protocol 1/2, WDSP external diversity API, CMake/CTest, pinned Thetis provenance.

---

## Files

- Modify: `src/models/RadioModel.h`
- Modify: `src/models/RadioModel.cpp`
- Modify: `src/core/PureSignal.h`
- Modify: `src/core/P2RadioConnection.h`
- Modify: `src/core/P2RadioConnection.cpp`
- Modify: `src/core/WdspEngine.h`
- Modify: `src/core/WdspEngine.cpp`
- Modify: `src/core/RxChannel.h`
- Modify: `src/core/RxChannel.cpp`
- Modify: `src/core/wdsp_api.h`
- Modify: `src/models/RxDspWorker.h`
- Modify: `src/models/RxDspWorker.cpp`
- Test: `tests/tst_codec_5_slice_assignment.cpp`
- Test: `tests/tst_p2_ddc_mask_ownership.cpp`
- Test: `tests/tst_p2_radio_connection_apply_ddc_assignment.cpp`
- Test: `tests/tst_p2_ddc_assignment_marshalling.cpp`
- Test: `tests/tst_stream_pool_binding.cpp`
- Test: `tests/tst_alex_per_adc_bpf_wire.cpp`
- Test: `tests/tst_radio_model_puresignal_run_wiring.cpp`
- Test: `tests/tst_radio_model_hpsdr_model_push.cpp`
- Test: `tests/tst_rx_channel_ext_div_wrappers.cpp`
- Test: `tests/tst_rx_dsp_worker_multi_slice.cpp`

## Task 1: Make full `DdcAssignment` the sole Protocol 2 wire owner

- [ ] **Step 1: Add the seven-DDC MOX regression**

In `tests/tst_p2_ddc_mask_ownership.cpp`, apply a full assignment with distinct enable/rate/ADC routing across all seven DDC entries. Toggle MOX, effective PS, and diversity through `RadioModel`. Assert each transition produces one full assignment and that unrelated enabled DDCs/rates remain intact.

In `tests/tst_p2_radio_connection_apply_ddc_assignment.cpp`, seed all seven `rate[]`/enable fields and assert the resulting CmdRx state keeps every entry after a PS transition.

- [ ] **Step 2: Prove the legacy writer overwrites the assignment**

```bash
cmake --build build --target tst_p2_ddc_mask_ownership tst_p2_radio_connection_apply_ddc_assignment -j
ctest --test-dir build -R '^(tst_p2_ddc_mask_ownership|tst_p2_radio_connection_apply_ddc_assignment)$' --output-on-failure
```

Expected before repair: a transition reaches `applyPsDdcConfig()` and collapses or rewrites the full seven-DDC state.

- [ ] **Step 3: Remove Protocol 2's legacy writer path**

- Delete the `ReceiverManager::ddcConfigChanged -> P2RadioConnection::applyPsDdcConfig` connection.
- Remove `P2RadioConnection::applyPsDdcConfig` and its P2-only state/comments if no non-production caller remains.
- Keep `ReceiverManager::ddcConfigChanged -> P1RadioConnection::applyPsDdcConfig` unchanged.
- Make `refreshDdcAssignmentForRadioState()` call the same full assignment request path used by slice binding for P2.
- Keep P1's wire update on its legacy path while still publishing the computed client-side assignment.

The resulting protocol split is:

```text
P2: state change -> compute DdcAssignment -> applyDdcAssignment -> publishDdcAssignment
P1: state change -> applyPsDdcConfig (wire) + publishDdcAssignment (client state)
```

- [ ] **Step 4: Audit writer ownership**

```bash
rg -n "applyPsDdcConfig|applyDdcAssignment|ddcConfigChanged|refreshDdcAssignmentForRadioState" src/models src/core
```

Expected: P2 has one wire writer, `applyDdcAssignment`; P1 retains `applyPsDdcConfig`.

- [ ] **Step 5: Rerun protocol ownership tests**

```bash
cmake --build build --target tst_p2_ddc_mask_ownership \
  tst_p2_radio_connection_apply_ddc_assignment \
  tst_p2_ddc_assignment_marshalling tst_codec_ps_ddc_config \
  tst_p1_puresignal_ddc_freq -j
ctest --test-dir build -R '^(tst_p2_ddc_mask_ownership|tst_p2_radio_connection_apply_ddc_assignment|tst_p2_ddc_assignment_marshalling|tst_codec_ps_ddc_config|tst_p1_puresignal_ddc_freq)$' --output-on-failure
```

## Task 2: Feed effective PureSignal state into codec context

- [ ] **Step 1: Add preference-versus-run regressions**

In `tests/tst_radio_model_puresignal_run_wiring.cpp`, cover:

- auto-cal preference on, PS not actually enabled: `CodecContext::puresignalRun == false`;
- auto-cal preference off, Single Cal has enabled PS: `puresignalRun == true`;
- restore/disable path: the value returns to false and one DDC recompute occurs.

- [ ] **Step 2: Confirm `isAutoCalEnabled()` produces the wrong result**

```bash
cmake --build build --target tst_radio_model_puresignal_run_wiring -j
ctest --test-dir build -R '^tst_radio_model_puresignal_run_wiring$' --output-on-failure
```

- [ ] **Step 3: Expose the effective state**

Add to `PureSignal`:

```cpp
bool isPsEnabled() const noexcept { return m_psEnabled; }
```

Change `RadioModel::currentCodecContext()` to set `ctx.puresignalRun` from `isPsEnabled()`. Retain `autoCalEnabledChanged` only for UI/preference consumers; DDC state follows `psEnabledChanged`.

- [ ] **Step 4: Rerun PureSignal and assignment tests**

Build and run `tst_radio_model_puresignal_run_wiring`, `tst_codec_ps_ddc_config`, and `tst_codec_5_slice_assignment`.

## Task 3: Publish DDC, ADC, and chain state from the incoming assignment

- [ ] **Step 1: Add physical-routing publication regressions**

In `tests/tst_stream_pool_binding.cpp` and `tests/tst_alex_per_adc_bpf_wire.cpp`, create two streams whose assignment maps to different physical ADCs. Assert after publication:

- each hosted slice has the assignment's `ddcIndex`;
- each hosted slice has `chainIndex` equal to the physical ADC/chain decoded for its stream;
- co-hosted slices agree;
- changing antenna/assignment updates both ReceiverManager and SliceModel;
- a suspended stream retains its binding but publishes DDC `-1` and appears in `streamsSuspended`.

- [ ] **Step 2: Confirm `chainIndex` remains at its default**

```bash
cmake --build build --target tst_stream_pool_binding tst_alex_per_adc_bpf_wire -j
ctest --test-dir build -R '^(tst_stream_pool_binding|tst_alex_per_adc_bpf_wire)$' --output-on-failure
```

- [ ] **Step 3: Publish one decoded routing result**

In `RadioModel::publishDdcAssignment`:

1. decode `adcCtrl1/adcCtrl2` once into the existing per-stream physical ADC map;
2. update ReceiverManager from that map;
3. for every slice with a valid stream, set `ddcIndex` from `assignment.streamDdc[stream]`;
4. set `chainIndex` from the same decoded physical ADC/chain used by ReceiverManager;
5. derive suspension from `streamDdc == -1` without altering the slice's stream binding.

Validate the incoming commits `f678e9fe`, `7cc35f20`, `2f4c4f6a`, and `40cc9190`; retain their correct routing work and add only the missing SliceModel publication/invariants.

- [ ] **Step 4: Rerun routing, codec, and suspension tests**

```bash
cmake --build build --target tst_stream_pool_binding tst_alex_per_adc_bpf_wire \
  tst_codec_5_slice_assignment tst_radio_model_hpsdr_model_push -j
ctest --test-dir build -R '^(tst_stream_pool_binding|tst_alex_per_adc_bpf_wire|tst_codec_5_slice_assignment|tst_radio_model_hpsdr_model_push)$' --output-on-failure
```

## Task 4: Port the complete WDSP external-diversity API with provenance

- [ ] **Step 1: Perform the source-first checklist**

Before editing Nereus code, inspect without executing:

- pinned Thetis `Project Files/Source/ChannelMaster/cmaster.cs` for external-diversity ownership/call order;
- pinned Thetis WDSP `wdsp/div.h` and `wdsp/div.c`;
- vendored `third_party/wdsp/src/div.h` and `third_party/wdsp/src/div.c`;
- current `src/core/RxChannel.*`, `src/core/WdspEngine.*`, and `src/core/wdsp_api.h`.

Record the exact cited lines. Preserve the upstream license, author/version tags, and these exact signatures:

```c
void create_divEXT(int id, int run, int nr, int size);
void destroy_divEXT(int id);
void xdivEXT(int id, int nsamples, double **in, double *out);
void SetEXTDIVRun(int id, int run);
void SetEXTDIVNr(int id, int nr);
void SetEXTDIVOutput(int id, int output);
void SetEXTDIVRotate(int id, int nr, double *Irotate, double *Qrotate);
```

- [ ] **Step 2: Replace the compile-only test with lifecycle ordering**

In `tests/tst_rx_channel_ext_div_wrappers.cpp`, move the contract to `WdspEngine` and inject a test function table that records calls. Cover:

- configure before create is rejected/no-op;
- create ID `0` occurs before Nr/Output/Rotate/Run;
- process occurs only while created and running;
- disable calls Run(0) before destroy;
- repeated disable/destructor destroys exactly once;
- IDs outside `[0, 1]` are rejected.

Rename the test class, but keep the CTest target name to avoid unrelated CMake churn.

- [ ] **Step 3: Confirm the lifecycle test fails**

```bash
cmake --build build --target tst_rx_channel_ext_div_wrappers -j
ctest --test-dir build -R '^tst_rx_channel_ext_div_wrappers$' --output-on-failure
```

- [ ] **Step 4: Make `WdspEngine` own external-diversity IDs**

Add the missing declarations to `wdsp_api.h`. In `WdspEngine`, add:

```cpp
bool createExternalDiversity(int id, int inputs, int complexSamples);
void configureExternalDiversity(int id, int output,
                                const double* iRotate,
                                const double* qRotate,
                                int inputs);
bool processExternalDiversity(int id, int complexSamples,
                              double** inputs, double* output);
void setExternalDiversityRunning(int id, bool running);
void destroyExternalDiversity(int id);
```

Track created/running state for the two WDSP slots. `shutdown()` and the destructor stop then destroy every created slot. Use a test-only function-pointer table under `NEREUS_BUILD_TESTS`; production defaults call the real C API.

Remove the `RxChannel::setExtDiv*` wrappers: an RX channel ID is not an external-diversity ID and must never index `pdiv`.

- [ ] **Step 5: Run the lifecycle test and provenance checks**

```bash
cmake --build build --target tst_rx_channel_ext_div_wrappers -j
ctest --test-dir build -R '^tst_rx_channel_ext_div_wrappers$' --output-on-failure
python3 scripts/verify-inline-tag-preservation.py
```

## Task 5: Synchronize, combine, and feed the diversity pair

- [ ] **Step 1: Add paired-stream DSP tests**

Extend `tests/tst_rx_dsp_worker_multi_slice.cpp` with a fake `WdspEngine` external-diversity seam and two source streams. Cover:

- one source alone produces no diversity output;
- equal chunks from source A and B produce one process call with input order A then B;
- the combined output is fed only to the diversity target slice;
- ordinary slices on either stream continue through their normal `RxChannel::processIq`;
- disabling diversity flushes unmatched queued samples;
- source A removal disables/destroys the slot and never promotes a positional "first slice";
- differently chunked deliveries are accumulated until an equal complex-sample count is available.

- [ ] **Step 2: Confirm no current feed path satisfies the tests**

```bash
cmake --build build --target tst_rx_dsp_worker_multi_slice -j
ctest --test-dir build -R '^tst_rx_dsp_worker_multi_slice$' --output-on-failure
```

- [ ] **Step 3: Add explicit diversity routing to `RxDspWorker`**

Add a queued control API:

```cpp
void setExternalDiversityRoute(int extDivId, int targetSliceId,
                               int primaryStream, int secondaryStream);
void clearExternalDiversityRoute();
```

Maintain separate reusable I/Q accumulators for the two source streams. When both contain at least the current target channel's `inSize`:

1. copy equal paired chunks into reusable `double` input vectors;
2. call `WdspEngine::processExternalDiversity`;
3. deinterleave the returned complex `double` vector into reusable float I/Q output;
4. call the target `RxChannel::processIq`;
5. consume exactly that paired count from both source queues.

Skip the target slice in the normal fan-out while this route is active; do not suppress other slices sharing either stream. Clear both accumulators when the route changes or disables.

- [ ] **Step 4: Wire RadioModel's stable diversity source**

Use the diversity-designated slice's stable ID, not `m_slices.value(0)` or list position. On enable:

1. resolve its two assigned ADC/DDC source streams;
2. create external-diversity slot `0`;
3. configure two inputs, output `2` (the WDSP contract's mixed-output selector for `nr == 2`), and current phase/gain rotation;
4. publish the worker route;
5. start the slot.

On phase/gain change, update rotation only after creation. On disable, clear the worker route, call Run(0), then destroy slot `0`. Slice removal and engine shutdown take the same path.

- [ ] **Step 5: Run paired-stream and lifecycle tests**

```bash
cmake --build build --target tst_rx_dsp_worker_multi_slice tst_rx_channel_ext_div_wrappers -j
ctest --test-dir build -R '^(tst_rx_dsp_worker_multi_slice|tst_rx_channel_ext_div_wrappers)$' --output-on-failure
```

## Task 6: Verify and commit protocol/diversity

- [ ] **Step 1: Run the complete focused set**

```bash
cmake --build build --target tst_codec_5_slice_assignment \
  tst_p2_ddc_mask_ownership tst_p2_radio_connection_apply_ddc_assignment \
  tst_p2_ddc_assignment_marshalling tst_stream_pool_binding \
  tst_alex_per_adc_bpf_wire tst_radio_model_puresignal_run_wiring \
  tst_radio_model_hpsdr_model_push tst_rx_channel_ext_div_wrappers \
  tst_rx_dsp_worker_multi_slice -j
ctest --test-dir build -R '^(tst_codec_5_slice_assignment|tst_p2_ddc_mask_ownership|tst_p2_radio_connection_apply_ddc_assignment|tst_p2_ddc_assignment_marshalling|tst_stream_pool_binding|tst_alex_per_adc_bpf_wire|tst_radio_model_puresignal_run_wiring|tst_radio_model_hpsdr_model_push|tst_rx_channel_ext_div_wrappers|tst_rx_dsp_worker_multi_slice)$' --output-on-failure
```

- [ ] **Step 2: Run core/model subsystem suites and pinned compliance**

```bash
cmake --build build --target all_tests -j
ctest --test-dir build -L core --output-on-failure
ctest --test-dir build -L models --output-on-failure
python3 scripts/verify-inline-tag-preservation.py
```

- [ ] **Step 3: Review and create a signed commit**

```bash
git diff --check
git diff --stat
git add src/models/RadioModel.* src/models/RxDspWorker.* src/core/PureSignal.h \
  src/core/P2RadioConnection.* src/core/WdspEngine.* src/core/RxChannel.* \
  src/core/wdsp_api.h tests
NEREUS_THETIS_DIR=/Users/j.j.boyd/Thetis \
NEREUS_MI0BOT_DIR=/Users/j.j.boyd/mi0bot-Thetis \
NEREUS_DESKHPSDR_DIR=/Users/j.j.boyd/deskhpsdr \
NEREUS_FREEDV_DIR=/Users/j.j.boyd/freedv-gui \
git commit -S -m "fix(protocol): unify P2 assignment and diversity lifecycle"
git log --show-signature -1
```
