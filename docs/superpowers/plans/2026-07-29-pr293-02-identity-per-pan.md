# PR 293 Stabilization 02 — Stable Identity and Per-Pan Ownership Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Eliminate stable-ID/list-position confusion and make every panadapter surface operate on its own current slice, settings namespace, ADC data, and lifetime.

**Architecture:** `SliceModel::sliceIndex()` is the stable identity everywhere outside list iteration. `TxSliceArbiter` stores and persists that identity directly. Pan-local UI wiring resolves the pan's active slice at event time, so slice deletion, reassignment, and pan layout changes cannot leave captured positional state behind. Floating applets are docked synchronously before layout destruction, with `QPointer` as a final lifetime guard.

**Tech Stack:** C++20, Qt 6 Widgets, QSettings/AppSettings, CMake/CTest.

---

## Files

- Modify: `src/core/TxSliceArbiter.h`
- Modify: `src/core/TxSliceArbiter.cpp`
- Modify: `src/models/RadioModel.h`
- Modify: `src/models/RadioModel.cpp`
- Modify: `src/gui/MainWindow.h`
- Modify: `src/gui/MainWindow.cpp`
- Modify: `src/gui/widgets/VfoWidget.cpp`
- Modify: `src/gui/SpectrumOverlayPanel.h`
- Modify: `src/gui/SpectrumOverlayPanel.cpp`
- Modify: `src/gui/PanadapterApplet.cpp`
- Modify: `src/gui/PanadapterStack.h`
- Modify: `src/gui/PanadapterStack.cpp`
- Modify: `src/gui/PanFloatingWindow.h`
- Test: `tests/tst_tx_slice_arbiter.cpp`
- Test: `tests/tst_tx_slice_binding_invariant.cpp`
- Test: `tests/tst_pan_badge_click_wiring.cpp`
- Test: `tests/tst_vfo_widget_tx_badge_click.cpp`
- Test: `tests/tst_pan_active_slice_sync.cpp`
- Test: `tests/tst_pan_floating_window.cpp`
- Test: `tests/tst_panadapter_stack_layouts.cpp`
- Test: `tests/tst_spectrum_overlay_panel.cpp`
- Test: `tests/tst_panadapter_applet_slice_assoc.cpp`
- Test: `tests/tst_slice_agc_advanced.cpp`

## Task 1: Make the TX arbiter stable-ID-native

- [ ] **Step 1: Strengthen arbiter regressions before changing production code**

In `tests/tst_tx_slice_arbiter.cpp`, create slices with IDs `0`, `4`, and `9` in list positions `0`, `1`, and `2`. Add tests that:

- `requestHandoff(9)` selects the third pointer and emits `9`;
- removing ID `4` does not change the bound ID `9`;
- the new `TxBoundSliceId` key restores `9`;
- when only legacy `TxBoundSliceIndex=2` exists, load resolves it once to ID `9` and writes `TxBoundSliceId=9`.

Update `tests/tst_tx_slice_binding_invariant.cpp` and `tests/tst_pan_badge_click_wiring.cpp` so direct arbiter handoff receives a stable ID and succeeds even when ID and position differ.

- [ ] **Step 2: Build and prove the tests fail for positional storage**

```bash
cmake --build build --target tst_tx_slice_arbiter tst_tx_slice_binding_invariant tst_pan_badge_click_wiring -j
ctest --test-dir build -R '^(tst_tx_slice_arbiter|tst_tx_slice_binding_invariant|tst_pan_badge_click_wiring)$' --output-on-failure
```

Expected: failures show that `requestHandoff(9)` is treated as a list offset and/or the stable-ID persistence key is absent.

- [ ] **Step 3: Convert `TxSliceArbiter` to IDs**

In `src/core/TxSliceArbiter.h/.cpp`:

- replace `txBoundSliceIndex` with `txBoundSliceId`;
- replace `m_txBoundIndex` with `m_txBoundSliceId{-1}`;
- make `requestHandoff(int sliceId)` resolve by scanning `m_slices` for `slice->sliceIndex() == sliceId`;
- emit stable IDs in handoff/change signals;
- save under `hardware/<mac>/TxBoundSliceId`;
- when that key is absent and `TxBoundSliceIndex` exists, resolve the old position once, save the resulting stable ID, and thereafter ignore the legacy key;
- when the bound slice is removed, choose the surviving fallback pointer and store its stable ID.

In `RadioModel::requestTxHandoffToSlice`, remove the ID-to-position conversion and delegate the stable ID directly.

- [ ] **Step 4: Replace every renamed accessor/caller**

```bash
rg -n "txBoundSliceIndex|m_txBoundIndex|TxBoundSliceIndex|requestHandoff\\(" src tests
```

Keep `TxBoundSliceIndex` only in the migration code and migration test. Every runtime request must pass a `SliceModel::sliceIndex()`.

- [ ] **Step 5: Run the stable-identity target set**

```bash
cmake --build build --target tst_tx_slice_arbiter tst_tx_slice_binding_invariant tst_pan_badge_click_wiring tst_vfo_widget_tx_badge_click tst_tx_frequency_follows_tx_slice -j
ctest --test-dir build -R '^(tst_tx_slice_arbiter|tst_tx_slice_binding_invariant|tst_pan_badge_click_wiring|tst_vfo_widget_tx_badge_click|tst_tx_frequency_follows_tx_slice)$' --output-on-failure
```

Expected: all pass.

## Task 2: Remove positional UI and signal callbacks

- [ ] **Step 1: Add deletion-gap tests**

Extend `tests/tst_vfo_widget_tx_badge_click.cpp` and `tests/tst_pan_active_slice_sync.cpp` with A/B/C, remove B, then assert:

- C's VFO context menu reads C via `sliceById(2)`;
- C's TX badge handoff selects ID `2`;
- an antenna selection from C changes C, not the remaining slice at position `1`;
- a `sliceAdded(2)` subscriber attaches to C.

- [ ] **Step 2: Confirm the gap cases fail**

```bash
cmake --build build --target tst_vfo_widget_tx_badge_click tst_pan_active_slice_sync -j
ctest --test-dir build -R '^(tst_vfo_widget_tx_badge_click|tst_pan_active_slice_sync)$' --output-on-failure
```

- [ ] **Step 3: Repair the call sites**

- In `MainWindow::createSliceFlag`, compare the badge with `txBoundSliceId()`, route handoff through `RadioModel::requestTxHandoffToSlice(sliceId)`, and resolve antenna changes with `sliceById(sliceId)`.
- In `VfoWidget.cpp`, replace `m_radioModel->slices().at(m_sliceIndex)` with `m_radioModel->sliceById(m_sliceIndex)` and retain null handling.
- Audit every `sliceAdded(int)` and `sliceRemoved(int)` connection in `MainWindow.cpp`; treat the payload as a stable ID and resolve with `sliceById`.
- Keep positional access only inside explicit list iteration where the value is created by that same loop and is never stored or emitted as an identity.

- [ ] **Step 4: Run a mechanical identity audit and tests**

```bash
rg -n "slices\\(\\)\\.at\\(|m_slices\\.at\\(|sliceAdded|sliceRemoved|requestHandoff\\(" src/gui src/models src/core
cmake --build build --target tst_vfo_widget_tx_badge_click tst_pan_badge_click_wiring tst_pan_active_slice_sync -j
ctest --test-dir build -R '^(tst_vfo_widget_tx_badge_click|tst_pan_badge_click_wiring|tst_pan_active_slice_sync)$' --output-on-failure
```

Review every remaining hit manually and annotate list-position-only code when its semantics are not obvious.

## Task 3: Scope AGC/RF gain changes to the emitting slice

- [ ] **Step 1: Add an inactive-slice AGC regression**

In `tests/tst_slice_agc_advanced.cpp`, construct active A and inactive C, enable automatic AGC behavior on A, then edit C's AGC threshold and RF gain. Assert A's automatic state and threshold are unchanged while C receives the edit.

- [ ] **Step 2: Confirm the test fails**

```bash
cmake --build build --target tst_slice_agc_advanced -j
ctest --test-dir build -R '^tst_slice_agc_advanced$' --output-on-failure
```

- [ ] **Step 3: Use the captured `slice` in both handlers**

In `RadioModel::wireSliceSignals`, change the `agcThresholdChanged` and `rfGainChanged` lambdas so every model mutation and WDSP lookup uses the captured `slice`; do not read or write `m_activeSlice` inside those handlers.

- [ ] **Step 4: Rerun the test**

```bash
cmake --build build --target tst_slice_agc_advanced -j
ctest --test-dir build -R '^tst_slice_agc_advanced$' --output-on-failure
```

## Task 4: Bind overlay controls and RADE state to each pan's active slice

- [ ] **Step 1: Add per-pan overlay regressions**

Extend `tests/tst_spectrum_overlay_panel.cpp` and `tests/tst_panadapter_applet_slice_assoc.cpp` with two pans whose active slice IDs and antennas differ. Assert:

- the second panel's RX antenna, TX antenna, and VAX controls read/write the second pan's slice;
- changing a pan's active slice rebinds its panel;
- destroying the prior slice leaves the panel disabled until a valid slice is assigned;
- a persisted `Extended=false` overrides a constructor/default `true`.

Extend the RADE test seam in `tests/tst_pan_active_slice_sync.cpp`: emit sync and frequency-offset changes for both slice IDs and verify only the matching VFO flag updates.

- [ ] **Step 2: Confirm the multi-pan cases fail**

```bash
cmake --build build --target tst_spectrum_overlay_panel tst_panadapter_applet_slice_assoc tst_pan_active_slice_sync -j
ctest --test-dir build -R '^(tst_spectrum_overlay_panel|tst_panadapter_applet_slice_assoc|tst_pan_active_slice_sync)$' --output-on-failure
```

- [ ] **Step 3: Replace slice-zero binding with a resolver**

In `SpectrumOverlayPanel`:

```cpp
using SliceResolver = std::function<SliceModel*()>;
void setSliceResolver(SliceResolver resolver);
void bindToPanSlice();
```

Store the resolver, rename `bindToSliceZero()` to `bindToPanSlice()`, and use the resolved slice for VAX and both antenna controls. MainWindow supplies a resolver that calls `sliceForPan(panId)` at use time. Rebind when the pan's active slice changes and when slices are added/removed.

- [ ] **Step 4: Filter RADE callbacks by stable ID**

Create the RADE connections in the per-flag wiring. Each callback captures the flag's stable slice ID and returns immediately when the signal's `sliceId` differs.

- [ ] **Step 5: Apply persisted extended-view state once**

After `PanadapterApplet` restores its settings and configures its spectrum, invoke the existing setter with the stored value so both `true` and `false` are pushed to the widget and active slice.

- [ ] **Step 6: Rerun the per-pan tests**

```bash
cmake --build build --target tst_spectrum_overlay_panel tst_panadapter_applet_slice_assoc tst_pan_active_slice_sync -j
ctest --test-dir build -R '^(tst_spectrum_overlay_panel|tst_panadapter_applet_slice_assoc|tst_pan_active_slice_sync)$' --output-on-failure
```

## Task 5: Make spectrum settings and wideband routing pan-local

- [ ] **Step 1: Add two-pan settings and wideband tests**

Extend `tests/tst_panadapter_applet_slice_assoc.cpp` so pan 0 and pan 1 load different zoom/range values, save them, reconstruct, and recover their own values. Add a signal-level test that a wideband toggle on pan 1 changes only pan 1's active slice.

Add or extend a MainWindow seam test so `widebandSpectrumReady(adc, bins)` reaches every spectrum widget's per-ADC store, and each widget chooses rendering from its active slice's resolved chain.

- [ ] **Step 2: Confirm the new cases fail**

Build and run the exact updated targets. If the MainWindow seam is added to `tst_pan_active_slice_sync`, use:

```bash
cmake --build build --target tst_panadapter_applet_slice_assoc tst_pan_active_slice_sync -j
ctest --test-dir build -R '^(tst_panadapter_applet_slice_assoc|tst_pan_active_slice_sync)$' --output-on-failure
```

- [ ] **Step 3: Configure every spectrum widget once**

Add a deterministic pan-index helper for IDs of the form `pan-N`. During `wireSpectrumForPan`:

- call `setPanIndex(N)` before `loadSettings()`;
- connect wideband-extension state to `sliceForPan(panId)` resolved at signal time;
- connect pan-local click/tune actions through that same resolver.

On close, iterate all pan applets and call each spectrum widget's `saveSettings()`; remove the active-widget-only save.

- [ ] **Step 4: Fan per-ADC wideband bins to all pan widgets**

Each `SpectrumWidget` already stores bins separately for ADC 0 and ADC 1. Forward every `RadioModel::widebandSpectrumReady` payload to every live pan widget. The widget's active slice/chain selects which stored vector to paint; no pan is chosen globally in MainWindow.

- [ ] **Step 5: Rerun the settings/wideband tests**

Build the modified targets and run their anchored CTest names.

## Task 6: Make layout teardown safe for floating pans

- [ ] **Step 1: Add the lifetime regression**

In `tests/tst_pan_floating_window.cpp` and `tests/tst_panadapter_stack_layouts.cpp`:

1. create pans A and B;
2. float B;
3. apply a layout that omits B;
4. process deferred deletes;
5. assert no dangling applet/window access, B is removed exactly once, and the remaining layout contains A;
6. repeat with a layout that retains B and assert it is docked before reparenting.

Use `QPointer` assertions after `QCoreApplication::sendPostedEvents`.

- [ ] **Step 2: Confirm the regression fails or crashes under the current raw-pointer path**

```bash
cmake --build build --target tst_pan_floating_window tst_panadapter_stack_layouts -j
ctest --test-dir build -R '^(tst_pan_floating_window|tst_panadapter_stack_layouts)$' --output-on-failure
```

- [ ] **Step 3: Dock floaters before any splitter destruction**

Add `PanadapterStack::dockAllFloatingPans()` and call it at the start of `applyLayout()`, before `clearSplitters()`. It must synchronously:

- take each entry from `m_floating`;
- disconnect the floater's dock callback;
- reparent a still-live applet to the stack;
- close/delete the floater;
- leave layout reconstruction to the existing `applyLayout` path.

Change `PanFloatingWindow::m_applet` to `QPointer<PanadapterApplet>` and null-check it in destructor/event paths.

- [ ] **Step 4: Rerun the lifetime tests under the normal test sandbox**

```bash
cmake --build build --target tst_pan_floating_window tst_panadapter_stack_layouts -j
ctest --test-dir build -R '^(tst_pan_floating_window|tst_panadapter_stack_layouts)$' --output-on-failure
```

## Task 7: Verify and commit stable identity/per-pan ownership

- [ ] **Step 1: Build and run the focused set**

```bash
cmake --build build --target tst_tx_slice_arbiter tst_tx_slice_binding_invariant \
  tst_pan_badge_click_wiring tst_vfo_widget_tx_badge_click \
  tst_pan_active_slice_sync tst_pan_floating_window \
  tst_panadapter_stack_layouts tst_spectrum_overlay_panel \
  tst_panadapter_applet_slice_assoc tst_slice_agc_advanced -j
ctest --test-dir build -R '^(tst_tx_slice_arbiter|tst_tx_slice_binding_invariant|tst_pan_badge_click_wiring|tst_vfo_widget_tx_badge_click|tst_pan_active_slice_sync|tst_pan_floating_window|tst_panadapter_stack_layouts|tst_spectrum_overlay_panel|tst_panadapter_applet_slice_assoc|tst_slice_agc_advanced)$' --output-on-failure
```

- [ ] **Step 2: Run GUI/model subsystem tests**

```bash
cmake --build build --target all_tests -j
ctest --test-dir build -L gui --output-on-failure
ctest --test-dir build -L models --output-on-failure
```

- [ ] **Step 3: Review, sign, and verify**

```bash
git diff --check
git diff --stat
git add src/core/TxSliceArbiter.* src/models/RadioModel.* src/gui tests
NEREUS_THETIS_DIR=/Users/j.j.boyd/Thetis \
NEREUS_MI0BOT_DIR=/Users/j.j.boyd/mi0bot-Thetis \
NEREUS_DESKHPSDR_DIR=/Users/j.j.boyd/deskhpsdr \
NEREUS_FREEDV_DIR=/Users/j.j.boyd/freedv-gui \
git commit -S -m "fix(ui): make slice and pan ownership stable"
git log --show-signature -1
```
