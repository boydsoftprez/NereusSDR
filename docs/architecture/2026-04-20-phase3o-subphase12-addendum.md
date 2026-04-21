# Phase 3O Sub-Phase 12 — Design Addendum

**Date:** 2026-04-20
**Scope:** Setup → Audio page (Devices / VAX / TCI placeholder / Advanced)
**Parent spec:** [2026-04-19-vax-design.md](2026-04-19-vax-design.md) §6.5–§6.7, §7.5
**Parent plan:** [2026-04-19-phase3o-vax-plan.md](2026-04-19-phase3o-vax-plan.md) Sub-Phase 12 (§2217–§2303)
**Author:** J.J. Boyd (KG4VCF) with Claude Opus 4.7

---

## 1. Purpose

The parent spec and plan left five UX/scope decisions open for per-phase resolution. This addendum records the locked-in choices so Sub-Phase 12 subagents can implement without re-litigating. It also captures one **structural deviation** from spec §7.5 that was discovered during Sub-Phase 12 preflight: the existing `SetupDialog` already contains six stub Audio pages from Phase 3-UI, making the spec's single-page-with-inner-tabs approach redundant and inconsistent with every other category in the dialog.

---

## 2. Locked Decisions

### 2.1 Device-picker interaction model (§6.5 — Devices tab)

**Decision: live-edit + readonly "Negotiated" readout.**

Every control (`Driver API`, `Device`, `Sample rate`, `Bit depth`, `Channels`, `Buffer size`, WASAPI options) commits on `currentTextChanged` / `valueChanged` / `toggled`. Each commit:

1. Writes the corresponding `audio/<Role>/<Key>` AppSettings entry.
2. Builds an `AudioDeviceConfig` from the card's current state.
3. Calls `AudioEngine::set<Role>Config(cfg)`.
4. `AudioEngine` tears down the old `IAudioBus`, rebuilds with new config, emits `<role>ConfigChanged(AudioDeviceConfig)` signal carrying the negotiated format.
5. Card's "Negotiated" pill updates on receipt of the signal.

**Rationale:** matches existing Setup → Display page patterns (colour swatches, slider-live-apply) established in Phases 3G-8 / 3G-9 / 3I. Thetis-parity knob-rich UX. Avoids staged-apply indirection.

**Edge cases in scope for 12.2:**
- Driver rejection (e.g., user picks 192 kHz on a device that doesn't support it) → pill turns red, `Negotiated` line quotes the PortAudio error, bus falls back to last-good config so audio keeps flowing.
- Mid-edit buffer-size scrub → 200 ms debounce inside `setSpeakersConfig` to coalesce rapid changes (new helper).
- Driver API switch while audio is streaming → brief dropout; pill states "APPLYING" during the window.

### 2.2 VAX tab platform split (§6.6)

**Decision: full picker always shown. Native-override allowed on Mac/Linux.**

All four VAX channel cards render the full 7-row form (Driver API / Device / Rate / Bit depth / Channels / Buffer / Options) on all platforms. Default binding on Mac/Linux is the platform-native HAL plugin (`NereusSDR VAX N (native)`); user can override by picking a third-party virtual cable (BlackHole, Loopback 2, JACK routing, etc.).

**New UI state introduced by this decision:** when a Mac/Linux channel is bound to a non-native device but has zero detected consumers reading from it, the card shows an **amber "override — no consumer"** warning badge with a tooltip: *"BlackHole is bound but no app is reading from it. If this is unintentional, pick the default CoreAudio (native HAL) device."* This closes the loop on the "silent override footgun" the mockup-q2 review flagged.

**Rationale:** maximum flexibility for power users without penalizing the default path. Most Mac/Linux users will never touch the override; those who do get honest feedback when the override isn't doing anything.

### 2.3 Auto-detect mini-picker (Task 12.3, Step 3)

**Decision: inline `QMenu` popup anchored to the button.**

Clicking an unassigned slot's "Auto-detect…" button calls `VirtualCableDetector::scan()` and invokes `QMenu::exec()` at the button's position. Menu items:

- **No cables:** single disabled row "No virtual cables detected" + entry "Install virtual cables…" (opens vendor URL via `QDesktopServices::openUrl`).
- **Free cables:** one entry per detected cable with a ► icon and subtitle (`VB-Audio · 48 k`).
- **Already-assigned cable:** entry shown with "→ VAX 2" suffix; clicking opens a confirm modal "Reassign CABLE-A from VAX 2 to VAX 3? VAX 2 will become unassigned." before reassigning.
- **>10 cables:** Qt's native scroll indicators; no custom pagination.
- **Keyboard:** ↑/↓ navigate, Enter binds, Esc dismisses.

**Rationale:** matches existing Setup idioms (`BandButtonItem` right-click, `HardwarePage` antenna picker). Lightweight, one-click completion, native-feeling.

### 2.4 Feature-flag behavior (§6.7, Advanced tab)

**Decision: checkboxes enabled + `qCWarning` on toggle-on + per-checkbox note-inline.**

The `SendIqToVax` and `TxMonitorToVax` checkboxes on the Advanced page:

- Render enabled.
- Toggling on writes `audio/<Flag>=True` to AppSettings.
- Toggle-on handler emits `qCWarning(lcAudio) << "<Flag> reserved for Phase 3M — no routing yet"`.
- A grey-text note-inline to the right of each checkbox reads *"(reserved for Phase 3M — no routing yet)"* so the user sees the scope before clicking.
- When Phase 3M lands and wires the actual routing, the persisted user preference is honored on first launch.

The `MuteVaxDuringTxOnOtherSlice` checkbox is active **now** (implementation ships in Sub-Phase 9's VAX mute gating) — no note-inline needed.

**Rationale:** surfaces the full Thetis feature catalogue for discoverability. Captures user intent pre-3M without silently enabling inert behavior. Preserves the "every UI element does what it says" property by being explicit in the inline text.

### 2.5 Reset-to-defaults scope (§6.7, Advanced tab)

**Decision: nuclear within `audio/*`. Preserve per-slice and TX-path operating state.**

Clicking "Reset all audio to defaults" → amber confirm modal → on confirm:

**Keys cleared:**
- `audio/Speakers/*`, `audio/Headphones/*`, `audio/TxInput/*`
- `audio/Vax<1..4>/*`
- `audio/DspRate`, `audio/DspBlockSize`
- `audio/VacFeedback/<ch>/*`
- `audio/SendIqToVax`, `audio/TxMonitorToVax`, `audio/MuteVaxDuringTxOnOtherSlice`
- `audio/FirstRunComplete` (first-run dialog will re-fire on next launch)
- `audio/LastDetectedCables` (rescan-diff baseline)

**Keys preserved (NOT cleared):**
- `slice/<N>/VaxChannel` (owned by `SliceModel`; represents per-VFO operating state, not audio settings)
- `tx/OwnerSlot` (owned by `TransmitModel`; TX source selection, same category)

**Implementation:** new `AudioEngine::resetAudioSettings()` iterates the `audio/` key-prefix tree, removes matching entries, then rebuilds buses from seeded defaults and emits the `speakersConfigChanged` / `vaxConfigChanged(ch)` / etc. signals so all subscribed UIs refresh.

**Confirm modal copy (verbatim):**

> **Reset all audio to defaults?**
>
> This will clear:
> • All device bindings (Speakers / Headphones / TX Input / VAX 1–4)
> • DSP sample rate and block size
> • VAC feedback-loop tuning
> • Feature flags
>
> Your per-slice VAX channel assignments will be kept. The first-run setup will re-appear on next launch.
>
> `[Cancel]`   `[Reset all audio]` (amber)

**Rationale:** matches Thetis's "Restore Defaults" semantics. Re-firing the first-run dialog doubles as a sanity check. Slice-level VAX binding is the user's operating-context work, not an audio setting.

---

## 3. Structural Deviation from Spec §7.5

**Problem:** Spec §7.5 describes a single `SetupAudioPage` with four inner `QTabWidget` sub-tabs (Devices / VAX / TCI / Advanced). Preflight discovered that `SetupDialog::buildTree()` already registers six stub pages under the Audio category from Phase 3-UI:

- `DeviceSelectionPage` (61 lines)
- `AsioConfigPage` (stub)
- `Vac1Page` (stub)
- `Vac2Page` (stub)
- `NereusVaxPage` (stub)
- `RecordingPage` (stub)

Adding the spec-§7.5 page as a seventh entry under the same category results in:

- "VAX" content appears in three places (VAC 1 / VAC 2 / NereusVAX stubs **and** the VAX inner tab).
- Audio is the only category in the dialog using inner tabs (DSP: 9 flat children, Display: 5, Transmit: 4, Appearance: 5, CAT & Network: 3).

**Resolution: replace the six stubs with four flat pages.** The new left-nav structure under Audio:

```
Audio
├── Devices       (AudioDevicesPage)
├── VAX           (AudioVaxPage)
├── TCI           (AudioTciPage — placeholder "Coming in Phase 3J")
└── Advanced      (AudioAdvancedPage)
```

No inner `QTabWidget`. Matches the DSP / Display / Transmit convention exactly. `AudioSetupPages.h/cpp` is removed along with all six stub classes.

**Side-effects:**
- `VaxFirstRunDialog::openSetupAudioTab(QString which)` signal becomes `openSetupAudioPage(QString pageLabel)` and wires to `SetupDialog::selectPage(pageLabel)` directly — single-call, no inner-tab indirection. The `TODO(sub-phase-12-open-setup-audio)` markers in `VaxFirstRunDialog.cpp` (landing in PR #80) get updated during Task 12.3 wire-up.
- `TODO(sub-phase-12-release-blocker)` in `MainWindow.cpp` is removed when Task 12.3 ships.
- Parent spec §7.5 is rewritten inline (see commit associated with this addendum).

---

## 4. Task 12.0 Work Folded Into Task 12.2

The audio-engine foundations needed for live-edit (Q1) do not currently exist on `main`. Rather than a separate task, they're folded into Task 12.2 as **Step 0** (first sub-step of the Devices-tab commit):

1. **New signal:** `AudioEngine::speakersConfigChanged(AudioDeviceConfig)`. Emitted at the end of `setSpeakersConfig()` and `ensureSpeakersOpen()`. Parallel signals for headphones, TX input, and each of the four VAX channels.
2. **New helpers:** `AudioDeviceConfig::loadFromSettings(QString prefix)` and `saveToSettings(QString prefix)` round-trip every field (`DriverApi`, `DeviceName`, `SampleRate`, `BitDepth`, `Channels`, `BufferSamples`, `ExclusiveMode`, `EventDriven`, `BypassMixer`, `ManualLatencyMs`).
3. **Live-reconfig safety:** `m_speakersBusMutex` (`std::mutex`) added; `setSpeakersConfig` holds it during tear-down/rebuild; `rxBlockReady` uses `std::try_lock` and drops the block if it can't acquire (≤1 ms of silence on device swap is inaudible vs. a use-after-free).
4. **Boot-time load:** `ensureSpeakersOpen()` reads the full `audio/Speakers/*` tree via `AudioDeviceConfig::loadFromSettings("audio/Speakers")` instead of hardcoded defaults.
5. **Menu-bar sync:** `MasterOutputWidget` subscribes to `AudioEngine::speakersConfigChanged` and refreshes its displayed device label on receipt, replacing its current timer-polled AppSettings read.

**Tests added in Step 0:**
- `AudioDeviceConfigRoundtripTest` — round-trip save → load preserves every field.
- `AudioEngineSpeakersLiveReconfigTest` — call `setSpeakersConfig` from main thread while `rxBlockReady` simulates DSP-thread traffic; verify no crash, no use-after-free (use `NEREUS_BUILD_TESTS` test seam).
- `MasterOutputWidgetSignalRefreshTest` — `emit speakersConfigChanged(cfg)` → widget's device label updates within a 50 ms timeout.

---

## 5. Revised Task-by-Task Scope (supersedes phase3o-vax-plan.md §2217–§2303)

| Task | Revised scope |
|------|---------------|
| **12.1** Audio nav refactor | Delete `AudioSetupPages.h/cpp` (6 stub classes). Remove the 6 `add(audio, …)` calls in `SetupDialog::buildTree()`. Create empty `src/gui/setup/AudioDevicesPage.h/cpp`, `AudioVaxPage.h/cpp`, `AudioTciPage.h/cpp`, `AudioAdvancedPage.h/cpp` (each extends `SetupPage`, returns a placeholder `QLabel` until its follow-up task fills it in). Add the four `add(audio, …)` calls for the new pages. 1 commit. |
| **12.2** AudioDevicesPage | Step 0 scaffolding (§4 above). Step 1: `DeviceCard` class (`src/gui/setup/DeviceCard.h/cpp`) — `QGroupBox` subclass parameterized by settings-prefix + role enum (Output/Input). Step 2: three card instances (Speakers / Headphones / TX Input) inside `AudioDevicesPage`. Step 3: `QSignalBlocker` guard on `speakersConfigChanged` receipt. 1 commit. |
| **12.3** AudioVaxPage | Four channel cards + TX row + Auto-detect `QMenu` picker (Q3). Full picker on all platforms (Q2). Native-override amber badge. Wire `VaxFirstRunDialog::openSetupAudioPage("VAX")` signal. Remove `TODO(sub-phase-12-*)` markers. 1 commit. |
| **12.4** AudioAdvancedPage | DSP rate/block combos. Per-VAX VAC feedback-loop tuning editors. Three feature-flag checkboxes with note-inlines + `qCWarning` hooks (Q4). Detected-cables readonly row + Rescan button. Reset-all-audio amber button + confirm modal + `AudioEngine::resetAudioSettings()` (Q5). 1 commit. |
| **12.5** AudioTciPage placeholder | Single centered `QLabel` "TCI server — coming in Phase 3J (see design spec §11.1)". 1 commit. |

**Commit order for dispatch:** 12.1 → 12.3 (release-gate unblocker — removes `TODO(sub-phase-12-release-blocker)`) → 12.2 / 12.4 / 12.5 in any order.

---

## 6. Explicitly Out of Scope

- **Direct ASIO engine + cmASIO parity** — deferred per the 2026-04-19 GPL compliance review. ASIO users route through PortAudio's built-in ASIO host API.
- **Actual `SendIqToVax` / `TxMonitorToVax` routing** — Phase 3M. Only the persistence + note-inline + warning log lands here.
- **Headphones bus in `AudioEngine`** — already exists structurally (`m_headphonesBus` slot); Sub-Phase 12 just wires the UI to existing engine-side setters. If setters don't yet exist for Headphones (verify in 12.2 Step 0), add them as part of Step 0.
- **TCI server configuration UI** — Phase 3J. Sub-Phase 12 just adds the placeholder page.
- **Per-band audio profile switching** — not requested, not in parent spec.

---

## 7. Test Strategy Summary

New test files created by Sub-Phase 12:

- `tests/AudioDeviceConfigRoundtripTest.cpp` (Step 0)
- `tests/AudioEngineSpeakersLiveReconfigTest.cpp` (Step 0)
- `tests/MasterOutputWidgetSignalRefreshTest.cpp` (Step 0)
- `tests/DeviceCardTest.cpp` (12.2)
- `tests/AudioVaxPageAutoDetectTest.cpp` (12.3) — verifies `QMenu` populates correctly from mock `VirtualCableDetector::scan()` result.
- `tests/AudioEngineResetAudioSettingsTest.cpp` (12.4) — verifies the clear-vs-preserve boundary per §2.5 above.

All test seams live behind `NEREUS_BUILD_TESTS` per existing `AudioEngine` convention.

---

## 8. Acceptance

Sub-Phase 12 is complete when:

- All 4 left-nav Audio pages render and bind settings round-trip.
- Live-edit of any Devices-tab control reopens the bus and updates the Negotiated pill within 100 ms.
- `VaxFirstRunDialog`'s "Customize…" button navigates the user to Audio → VAX without extra clicks.
- "Reset all audio to defaults" clears exactly the keys in §2.5 above, no more, no less (verified by test).
- Build is green on all three platforms; `ctest` passes.
- Attribution verifier scripts pass (no new ported files in 12 — all pages are NereusSDR-native Qt widgets per the source-first-UI-vs-DSP rule).

---

*Addendum prepared during the Sub-Phase 12 brainstorming session, 2026-04-20. Interview protocol per handoff from Sub-Phase 11 completion. Locks five decisions + one structural deviation + Task-12.0-fold-in to set subagent context.*
