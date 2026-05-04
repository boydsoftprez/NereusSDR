# Phase 3M — TX Epic Master Design

**Status:** Draft — 2026-04-25
**Author:** J.J. Boyd / KG4VCF (with Claude Opus 4.7 as collaborator)
**Thetis source baseline:** `v2.10.3.13-7-g501e3f51` (commit `501e3f51`)

---

## 1. Goal

Add full transmit capability to NereusSDR — voice (SSB/AM/FM/digital), CW,
PureSignal predistortion, EER for E-class PAs, and complete userland surface
parity with Thetis's TX feature set. Ship as a series of 16 independently
mergeable sub-PRs, each fully functional within its scope (no NYI placeholders).

After this epic merges, NereusSDR is a two-way radio. The 3F multi-panadapter
phase that depends on PureSignal's DDC state machine becomes unblocked.

## 2. Cross-cutting decisions

These apply to every sub-phase below. They were locked in interview-style
brainstorm and govern all subsequent implementation.

### 2.1 Architecture decisions

| # | Decision | Source-first verification |
|---|---|---|
| **Q1** | **Mic input** is dual-path: PC-mic via PortAudio + PipeWireBus (default, reuses 3O VAX stack) and radio-mic via P2 port 1026 / P1 EP2 (Thetis-faithful, opt-in). HL2 forced to PC-mic by `BoardCapabilities`. cmASIO-equivalent (full codec replacement) deferred to 3M-7. | Thetis HERMES default is radio-mic per `cmaster.c:177-190` `OpenChannel(... type=1 ...)`; PC-mic divergence documented per `feedback_source_first_ui_vs_dsp` (DSP/radio governs source-first; audio plumbing is NereusSDR-native) |
| **Q2** | **MOX state** lives in a new `MoxController` class, separate from `TransmitModel`. `TransmitModel.m_mox` retained as derived property. `MoxController` exposes phase-granular Qt signals (`txAboutToBegin`, `hardwareFlipped`, `txReady`, `txAboutToEnd`, `txaFlushed`, `rxReady`) replacing Thetis's pre/post delegate pair with richer temporal sequencing. | Thetis structurally has the same seam at `console.cs:44851-44852` `MoxChangeHandlers` / `MoxPreChangeHandlers`; we promote it to primary integration. Phase-granular signals match the load-bearing temporal ordering inside `chkMOX_CheckedChanged2` (`console.cs:29311-29678`) |
| **Q3** | **`TxChannel` ownership:** `WdspEngine` owns both RX and TX channels in a single shared channel-ID namespace. Mirrors Thetis cmaster.c structure. PureSignal cross-channel wiring works naturally because both sides addressable through one engine. | WDSP itself uses `struct _ch ch[MAX_CHANNELS]` shared array (`channel.c:29`); same `OpenChannel` API for RX (type=0) and TX (type=1). PureSignal indexes by raw channel ID across the shared namespace (`sync.c:70-76`) |
| **Q4** | **MOX delays** scheduled via `QTimer::singleShot` chain on main thread (NOT `Thread.Sleep`, NOT a worker thread). Defers PTT-down request until in-flight PTT-up sequence completes, matching Thetis's uninterruptible-sleep behavior. | Thetis uses inline `Thread.Sleep` (5 calls in `chkMOX_CheckedChanged2`); we cannot block Qt main thread. `QTimer::singleShot` accuracy ±1-3ms matches Thetis's scheduler accuracy |
| **Q5** | **PA telemetry** flows from `RadioStatus` (3P-H) → `MeterPoller` cache via Qt signal/slot push. `MeterBinding::TxPower/TxReversePower/TxSwr` (100/101/102) cache fills automatically on signal; meter widgets repaint at 10 Hz. | `RadioStatus::paFwdChanged`/`paRevChanged`/`paSwrChanged` signals already exist from 3P-H Diagnostics work |
| **Q6** | **`BandPlanGuard`** uses compiled C++ `constexpr` tables for 24 regions + per-band frequency edges + per-band mode/BW restrictions (full 60m channel table per region: UK 11-channel w/ per-channel BW, US 5×2.8 kHz, JA 4.63 MHz). **`Extended` toggle** (Thetis-faithful name) in `Setup → General → Hardware Configuration` bypasses guards. | Thetis hard-codes equivalent at `clsBandStackManager.cs:1063-1083` + `console.cs:6770-6816, 29401-29481`. Pre-code review (2026-04-25) confirmed Thetis canonical name is `Extended` not `MARS/CAP`; corrected. Compiled tables are type-safe at compile time, can't be silently broken by a malformed JSON resource at runtime |
| **Q7** | **3M-0 packaging:** ships its own PR (likely v0.2.4) before any TX code lands. Safety net is in main BEFORE 3M-1a writes the first MOX byte. | Matches established pattern: 3G-13, 3G-14, 3P-I-a, 3P-I-b each shipped own PR. Reduces 3M-1a risk; surfaces user-visible value (PA meters live during RX) earlier |

### 2.2 Meta rules

These are workflow rules, applied to every sub-phase below.

#### 2.2.1 Fully functional first pass

Every PR ships scope-complete. No NYI placeholders, no greyed-out controls
"awaiting future work." Out-of-phase controls are **hidden** (not present in
the widget tree at all) until their phase ships, then they appear fully
working. TxApplet visibly grows over the epic — users see new controls
appear as phases land.

#### 2.2.2 Two-phase Thetis review per sub-phase

Each sub-PR has two formal review gates.

**Pre-code review (gates implementation start):** A research agent enumerates
every Thetis surface in the sub-phase's scope:
- DSP / protocol bytes / WDSP setters
- Setup pages, sub-tabs, group boxes
- Menu items + keyboard shortcuts
- Modal + modeless dialogs
- Status bar elements + tooltips + badges
- Right-click popups / context menus
- Drag/wheel/scroll gestures on widgets
- Andromeda front-panel mappings
- Persistence keys

The output is a coverage checklist that becomes the sub-phase's verification
matrix. The implementation plan can't be written until this checklist is
signed off.

**Post-code review (gates merge):** A second agent (independent eyes) walks
the same checklist against the merged code. Each item gets one of:
- ✅ shipped per Thetis
- 🔄 explicitly deferred (target sub-phase named in the implementation plan)
- ❌ missed

❌ items block merge until reconciled (either ship in this PR, or
explicitly classify as 🔄).

This pairs naturally with TDD: pre-code review is the spec; post-code
review verifies against the spec.

#### 2.2.3 No source citations in user-visible strings

User-visible strings (tooltips, status bar text, dialog labels, button
captions, log messages a user might read) contain plain English only.
Thetis source attribution (`[file:line @sha]`) lives as inline comments
adjacent to the string assignment in source code, never in the displayed
string itself. This is per `feedback_no_cites_in_user_strings`.

```cpp
// CORRECT
// Tooltip text from Thetis setup.cs:14257 [v2.10.3.13]
btn->setToolTip(tr("Mic boost +20 dB — for low-output mics"));

// WRONG
btn->setToolTip(tr("Mic boost +20 dB — for low-output mics  [setup.cs:14257]"));
```

The pre-code Thetis review produces the cite map; cites go into source
comments, not into UI text. CLAUDE.md inline-cite-versioning continues to
govern the source-side cite format.

## 3. Phase index

> **Schedule swap (2026-04-29):** rows 5-6 (3M-2) and 7-11 (3M-3) swapped
> in execution order — 3M-3 now runs immediately after 3M-1, with 3M-2
> deferred until 3M-3 ships AND the HL2 ATT/filter safety audit closes.
> See §6 (3M-2) and §7 (3M-3) for rationale.  Numeric IDs (`3M-2` / `3M-3`)
> stay as-is (mode-gate strings, BandPlanGuard toasts, and downstream
> doc cites are stable); only the run order changes.

| # | Phase | Sub-PRs | Goal | Cumulative | Status |
|---|---|---|---|---|---|
| 1 | 3M-0 | PA Safety Foundation | SWR protect, TX-inhibit, watchdog, BandPlanGuard, telemetry routing, HighSWR overlay. No MOX yet. | 1 | ✅ Complete |
| 2-4 | 3M-1 | a / b / c | First RF (TUNE-only) → Mic + SSB voice (with MON pulled in) → Polish + persistence | 4 | ✅ Complete (PRs #144 / #149 / #152) |
| 5-9 | 3M-3 | a-i / a-ii / a-iii / b / c | Speech chain (top → dynamics → gating) → mode-specific TX → audio routing | 9 | ▶ Next (pulled forward) |
| 10-11 | 3M-2 | a / b | Sidetone + keyer + QSK → CWX + paddle USB | 11 | ⏸ Deferred (after 3M-3 + HL2 audit) |
| 12-13 | 3M-4 | a / b | PureSignal core (PSForm + calcc + iqc) → AmpView | 13 | Planned |
| 14 | 3M-5 | (single) | EER (Ganymede / E-class PA) | 14 | Planned |
| 15 | 3M-6 | (single) | TX polish (TXA siphon spectrum source, CAT TX, AndromedaIndicator generalization, TUNE pulse, Aries ATU full integration) | 15 | Planned |
| 16 | 3M-7 | (single) | cmASIO-equivalent host audio codec replacement | 16 | Planned |

**16 PRs total.** Comparable to the 3P all-board parity epic (9 PRs); rough
wall-clock estimate 3-4 months serial, less with parallel branches after
3M-4a.

## 4. Phase 3M-0 — PA Safety Foundation

**Goal:** Land the entire PA-safety net before the first MOX. After this
PR ships, attempting TX is impossible (no MOX state machine yet) but every
safety bit, telemetry feed, and guard is plumbed and tested. When 3M-1a
wires the first MOX, the safety layer activates automatically.

### 4.1 Components

1. **`SwrProtectionController`** — Thetis port of the `PollPAPWR` async loop
   (`console.cs:25933-26120 [v2.10.3.13]`). Inputs: forward power (W),
   reverse power (W), tune-active flag. Computes
   `ρ = √(alex_rev / alex_fwd); swr = (1+ρ)/(1−ρ)` (verbatim
   `console.cs:25972-25976`); clamps to 1.0 when both values ≤ 2.0.
   Outputs: `swrProtectFactor` (1.0 = no foldback, multiplies drive in
   `SetOutputPower`) + `highSwr` flag. Two-stage foldback math:
   (a) per-sample foldback when `swr ≥ _swrProtectionLimit`:
   `swrProtectFactor = limit / (swr + 1.0f)`;
   (b) latched windback after 4 consecutive trip samples:
   `swrProtectFactor = 0.01f` until MOX-off (`UIMOXChangedFalse`).
   Defaults from Thetis: `_swrProtectionLimit = 2.0f`,
   `_tunePowerSwrIgnore = 35.0f`, `alex_fwd_limit = 5.0f` (8000D uses
   `2.0 × ptbPWR.Value` per K2UE recommendation). Open-antenna detection:
   `(alex_fwd > 10 && (fwd-rev) < 1)` → `swr = 50.0f`,
   `swrProtectFactor = 0.01f`, drop MOX (8000D excluded). Poll cadence:
   1 ms during MOX, 10 ms during RX. EMA alpha for forward-power
   smoothing: 0.90. Plumbed but not yet routed to
   `RadioConnection::setOutputPower()` — that wiring is 3M-1a.

2. **`TxInhibitMonitor`** — Polls user-IO input pin at 100 ms cadence
   (Thetis `PollTXInhibit` `console.cs:25801-25839 [v2.10.3.13]`);
   emits `txInhibited(bool, source)` where source ∈ {`UserIo01`,
   `Rx2OnlyRadio`, `OutOfBand`, `BlockTxAntenna`}. Per-board pin map:
   ANAN7000D / 8000D / RedPitaya use `getUserI02()` (P1) /
   `getUserI05_p2()` (P2); other boards use `getUserI01()` (P1) /
   `getUserI04_p2()` (P2). Active-low at GPIO; `_reverseTxInhibit`
   inverts further. The aggregated `source` enum is NereusSDR-original;
   each source's predicate cites a different Thetis line:
   `UserIo01` → `console.cs:25801-25839`;
   `Rx2OnlyRadio` → `console.cs:15283-15307` (`RXOnly`);
   `OutOfBand` → `console.cs:6770-6806` (`CheckValidTXFreq`) +
   `console.cs:29435-29481` (MOX-entry rejection);
   `BlockTxAntenna` → `Andromeda/Andromeda.cs:285-306`. Status-bar
   label `toolStripStatusLabel_TXInhibit` re-targeted from 3P-H.

3. **Watchdog** — `RadioConnection::setWatchdogEnabled(bool)` wraps
   `SetWatchdogTimer(int bits)` (`NetworkIOImports.cs:197-198`). Boolean
   only, not parameterized in milliseconds — radio firmware owns the
   timeout. Set once on Setup toggle (`chkNetworkWDT`, default ON), no
   host-side renewal cadence. (Earlier draft proposed `setWatchdogTimer(ms)`;
   pre-code review confirmed Thetis API is bool-only — corrected.)

4. **`BandPlanGuard`** — Compiled C++ `constexpr` tables, ported from
   Thetis `clsBandStackManager.cs:1063-1083` (`IsOKToTX`),
   `console.cs:6770-6816` (`CheckValidTXFreq` + `checkValidTXFreq_local`).
   24 region entries (Australia / Europe / India / Italy / Israel /
   Japan / Spain / United Kingdom / United States / Norway / Denmark /
   Sweden / Latvia / Slovakia / Bulgaria / Greece / Hungary /
   Netherlands / France / Russia / Region1 / Region2 / Region3 /
   Germany — `setup.designer.cs:8084-8108`). Per-band frequency edges
   from `clsBandStackManager.cs:1334-1497`. **Full Thetis 60m channel
   table** (`Init60mChannels()`, `console.cs:2643-2669`):
   - UK: 11 channels with per-channel BW from 3.0 to 12.5 kHz
     (`5.26125 / 5.500 kHz BW`, `5.2800 / 8.0`, `5.29025 / 3.5`,
     `5.3025 / 9.0`, `5.3180 / 10.0`, `5.3355 / 5.0`, `5.3560 / 4.0`,
     `5.36825 / 12.5`, `5.3800 / 4.0`, `5.39825 / 6.5`, `5.4050 / 3.0`)
   - US: 5 channels @ 2.8 kHz (`5.3320`, `5.3480`, `5.3585`, `5.3730`,
     `5.4050` MHz)
   - Default (other regions): same as US
   - Japan: discrete `4.629995-4.630005 MHz` allocation (different from
     IARU 60m)
   60m mode-restriction (US-only): mode ∈ {USB, CWL, CWU, DIGU}
   (`console.cs:29416-29433`). Different-band guard
   `_preventTXonDifferentBandToRXband` (`console.cs:29401-29414`).
   **Override toggle: `Extended`** (matches Thetis `chkExtended`,
   `setup.designer.cs:8065-8077`) — bypasses `IsOKToTX` and
   `CheckValidTXFreq` entirely. Tooltip: `"Enable extended TX (out of band)"`.
   Companion red warning label: `"Changing this setting will reset your
   band stack entries"`. (Earlier draft used name "MARS/CAP" — pre-code
   review confirmed Thetis canonical name is `Extended`; corrected.)
   The "US Technician 10m phone restriction" mentioned in earlier scope
   sketches is **not in Thetis** and is dropped — license-class is not
   modelled in Thetis's region tables.

5. **PA telemetry → `MeterPoller`** — `RadioStatus::paFwdChanged`/
   `paRevChanged`/`paSwrChanged` signals connect to `MeterPoller`'s cache
   updater. Bindings 100 (TxPower), 101 (TxReversePower), 102 (TxSwr) start
   delivering live values. **First user-visible deliverable** — PA meters
   paint values during RX (typically 0/0/1.0 baseline). Per-board scaling
   formulas ported verbatim from Thetis `computeAlexFwdPower`
   (`console.cs:25008-25072`) — each board has its own
   `bridge_volt`/`refvoltage`/`adc_cal_offset` triplet:
   - ANAN100/100B: `0.095, 3.3, 6`
   - ANAN100D: `0.095, 3.3, 6`
   - ANAN200D: `0.108, 5.0, 4`
   - ANAN7000D / AnvelinaPro3 / G2 / G2-1K / RedPitaya: `0.12, 5.0, 32`
     (RedPitaya tag `//DH1KLM` preserved per attribution rules)
   - ORIONMKII / ANAN8000D: `0.08, 5.0, 18`
   - Default: `0.09, 3.3, 6`
   ADC→volts: `volts = (adc - adc_cal_offset) / 4095.0f * refvoltage`;
   `watts = volts² / bridge_volt`. Low-power-board exciter formula is a
   piecewise-linear breakpoint table (`console.cs:25074-25140`); ported
   verbatim. `CalibratedPAPower()` integration with 3P-G calibration
   table is preserved.

6. **`HighSwr` overlay** — Thetis-parity static red overlay on
   `SpectrumWidget` tied to `SwrProtectionController::highSwrChanged`.
   When set, paint `"HIGH SWR"` text + 6 px red border around the spectrum
   area (verbatim from `display.cs:4183-4201 [v2.10.3.13]`). When power
   foldback latches, append `"\n\nPOWER FOLD BACK"`. (Earlier draft
   proposed 2 Hz blink + 2s recovery hold — pre-code review confirmed
   Thetis is static; reverting to Thetis-parity. Blink/hold UX filed as
   post-3M epic enhancement issue.)

7. **Block-TX safety** — `MoxController` (in 3M-1a) refuses to enter
   `RxToTxPending` if `AlexController::m_blockTxAnt2`/`3` matches the
   selected TX antenna. Surface 3M-0 prepares; activation is 3M-1a.

8. **RX-only and PA-trip predicates** — split into capability vs live
   state per pre-code review findings:
   - `BoardCapabilities::isRxOnlySku()` — hard-coded SKU list (e.g.,
     HL2-receive-only kits) AND `appSettings.RxOnly` user-toggle
     (`chkGeneralRXOnly` mirror, `console.cs:15283-15307`). Combined
     predicate `caps.isRxOnlySku() || appSettings.RxOnly` hard-blocks
     MOX entry. (Thetis treats this purely as a user toggle; we add the
     SKU bit because some HL2 variants ship without TX hardware.)
   - `RadioModel::paTripped()` — **live state**, not a capability. Set
     by Ganymede CAT trip messages (`Andromeda/Andromeda.cs:914-920`),
     cleared by `GanymedeResetPressed` or by `GanymedePresent = false`.
     Hard-blocks MOX entry while set. (Earlier draft mis-named this as
     `boardHasGanymedePaIssue()` on `BoardCapabilities` — corrected.)
   - `BoardCapabilities::canDriveGanymede()` — real capability flag for
     boards that connect to a Ganymede 500W PA (Andromeda console
     family). Stays on `BoardCapabilities`.

### 4.2 UI surfaces (deep-wired)

| Surface | Controls | Depth |
|---|---|---|
| **Setup → Transmit → SWR Protection** *(group box `grpSWRProtectionControl`)* | `chkSWRProtection` (toggle, default off, tooltip `"Show a visual SWR warning in the spectral area"`), `udSwrProtectionLimit` (1.0-5.0, default 2.0, suffix `:1`), `chkSWRTuneProtection` (toggle, tooltip `"Disables SWR Protection during Tune."`), `udTunePowerSwrIgnore` (5-50, default 35, suffix `W`), `chkWindBackPowerSWR` (toggle, tooltip `"Winds back the power if high swr protection kicks in"`) | Full Thetis parity (`setup.designer.cs:5793-5924`) |
| **Setup → Transmit → External TX Inhibit** *(group box `grpExtTXInhibit`)* | `chkTXInhibit` (toggle, default off, tooltip `"Thetis will update on TX inhibit state change"`), `chkTXInhibitReverse` (toggle, tooltip `"Reverse the input state logic"`) | Full Thetis parity (`setup.designer.cs:46626-46657`) |
| **Setup → Transmit → "Disable HF PA"** | `chkHFTRRelay` (`hf_tr_relay`, toggle, tooltip `"Disables HF PA."`) | Full (`setup.designer.cs:5780-5791`) |
| **Setup → Transmit → Block-TX antennas** *(panel `panelAlexRXAntControl`)* | `chkBlockTxAnt2` + `chkBlockTxAnt3` — Thetis ships these as unlabelled column-header checkboxes (15×14 px, no Text); we add accessibility labels: `"Block TX on Ant 2"` / `"Block TX on Ant 3"` (NereusSDR-original copy, comment with `setup.designer.cs:6704-6724` cite) | NereusSDR-original labels |
| **Setup → General → Hardware Configuration** | `comboFRSRegion` (24 entries, default `"United States"`, tooltip `"Select Region for your location"`), `chkExtended` (toggle, tooltip `"Enable extended TX (out of band)"`, with red warning label `"Changing this setting will reset your band stack entries"`), `chkGeneralRXOnly` (toggle, tooltip `"Check to disable transmit functionality."`, default hidden — visibility per board model), `chkNetworkWDT` (toggle, default ON, tooltip `"Resets software/firmware if network becomes inactive."`) | Full Thetis parity |
| **Setup → General → Options** *(group `grpGeneralOptions`)* | `chkPreventTXonDifferentBandToRX` (toggle, default off, tooltip `"Prevent TX'ing on a different band to the RX band"`) | Full |
| **SpectrumWidget overlays** | `HIGH SWR` text + 6 px red border (Thetis-parity static, `display.cs:4183-4201`) — appends `\n\nPOWER FOLD BACK` when latched windback active. TX-attenuator dBm offset, basic Display.MOX paint | Active during RX (no MOX yet); Display.MOX paint dormant |
| **Status bar** (3P-H integration) | `toolStripStatusLabel_TXInhibit` re-targeted (image `Properties.Resources.stop`, tooltip `"Hardware TX Inhibit"`); `toolStripStatusLabel_PAstatus` (live, click = reset Ganymede when resettable, right-click = jump to PA setup) | Live |

### 4.3 Verification

- **Synthetic SWR=4.0:** unit test feeds high-SWR values into
  `SwrProtectionController`. Verify per-sample foldback factor
  `swrProtectFactor = limit/(swr+1.0)`. Verify trip latch only after
  ≥4 consecutive trip samples (Thetis `console.cs:26070`). Verify
  windback latches at `0.01f` until MOX-off.
- **`BandPlanGuard` table-driven tests:** All 24 regions × 14 bands ×
  all modes × 60m corner cases (UK 11 channels, US 5 channels, JA
  4.63 MHz allocation) verified against Thetis
  `clsBandStackManager.cs:1063-1083` and `console.cs:29401-29481`.
  Verify `Extended` toggle bypasses both predicates as in Thetis
  (`console.cs:6772, 6810`).
- **Per-board PA scaling:** unit test each board's
  `bridge_volt`/`refvoltage`/`adc_cal_offset` triplet against
  Thetis `computeAlexFwdPower` reference values for known ADC inputs.
- **Hardware:** ANAN-G2 with intentional 3:1 antenna-mismatch jig.
  Static `"HIGH SWR"` overlay paints on RX; PA meters paint values.
  Open-antenna case (`fwd > 10W && (fwd-rev) < 1W`): SWR jumps to 50,
  windback latches.
- **Two-phase Thetis review:** Pre-code review complete (output
  consumed into this design). Post-code agent verifies coverage of
  Setup → Transmit → SWR Protection / External TX Inhibit / Disable
  HF PA / Block-TX antennas / Setup → General → Hardware Configuration /
  Options + status-bar items + per-board PA scaling tables + 60m
  channel tables. ❌ items block merge.

### 4.4 Files

**New:**
- `src/core/safety/SwrProtectionController.{h,cpp}`
- `src/core/safety/TxInhibitMonitor.{h,cpp}`
- `src/core/safety/BandPlanGuard.{h,cpp}`

**Modified:**
- `src/core/RadioConnection.{h,cpp}` — `setWatchdogEnabled(bool)`
- `src/core/RadioModel.{h,cpp}` — own the safety controllers; add
  `paTripped()` live state property
- `src/gui/SpectrumWidget.{h,cpp}` — Thetis-parity static `"HIGH SWR"`
  text + 6 px red border overlay
- `src/gui/setup/TransmitSetupPages.cpp` — SWR Protection group, External
  TX Inhibit group, Block-TX antennas, Disable HF PA toggle
- `src/gui/setup/GeneralOptionsPage.cpp` — `Extended` toggle (renamed
  from MARS/CAP), `chkPreventTXonDifferentBandToRX`, `chkNetworkWDT`,
  region combo, RX-only toggle (visibility per board)
- `src/models/BoardCapabilities.{h,cpp}` — `isRxOnlySku()`,
  `canDriveGanymede()` (NOT `boardHasGanymedePaIssue()` — that's live
  state on `RadioModel`)
- `src/gui/meters/MeterPoller.{h,cpp}` — telemetry routing
- `src/core/RadioStatus.{h,cpp}` — verify per-board PA scaling matches
  Thetis `computeAlexFwdPower` (already present from 3P-H; add
  cross-check tests)

### 4.5 Risk

**Low.** No MOX, no RF, all changes observable but inactive until 3M-1a
fires. Worst case: a `SwrProtectionController` bug leaves it in permanent
foldback; we'd notice the moment 3M-1a tries to TX. That's a 3M-1a-blocking
risk, not a 3M-0 risk.

## 5. Phase 3M-1 — First RF + SSB Voice + Polish

3M-1 splits into three sub-phases (a/b/c), each its own PR.

### 5.1 Phase 3M-1a — TUNE-only First RF

**Goal:** Reduced-power TUNE carrier on the air. Smallest possible RF-on-air
milestone. Exercises the entire MOX path and TX I/Q wire format with zero
modulation logic.

#### 5.1.1 Components

| Subsystem | What lights up |
|---|---|
| **MoxController** | RX→TX→RX state machine; 6 QTimer chains (`rfDelay` 30ms / `moxDelay` 10ms / `spaceDelay` 0ms / `keyUpDelay` 10ms / `pttOutDelay` 20ms / `breakInDelay` 300ms); phase signals (`txAboutToBegin`, `hardwareFlipped`, `txReady`, `txAboutToEnd`, `txaFlushed`, `rxReady`); PTT source enum (9 values, Thetis `enums.cs:346-359`); manual-MOX vs PTT-MOX distinction (`_manual_mox`) |
| **TxChannel (minimal)** | `WdspEngine::createTxChannel(WDSP.id(1,0))`. 9 stages active: `rsmpin`, `bp0` (mandatory BPF), `alc` (always-on), `gen1` (TUNE carrier), `uslew` (5ms ramp), `cfir`, `rsmpout`, `sip1`, `outmeter`. Mode set to AM with carrier-only postgen. All other TXA stages built but `Run=false` (will activate progressively in 3M-1b/3M-3a) |
| **TUNE function** | `chkTUN_CheckedChanged` ported (`console.cs:29978-30143`); CW→LSB/USB swap during tune; per-band `tune_power[14]` limit; tune meter mode lockin (Fwd/Rev/SWR/SWR-Pwr/Off only). Pulse-mode and Aries ATU integration deferred to 3M-6 |
| **TX I/Q output** | TX I/Q replaces silence frames already streaming on P2 port 1029 / P1 EP2 TX zones. `TxChannel::pushIqToConnection()` runs on audio thread |
| **Hardware flip path** | `HdwMOXChanged` equivalent (`console.cs:29033-29122`): P1 sets MOX bit, sets PttOut, sets TRXrelay, sets BPF2Gnd; P2 sends `composeCmdTx()` with mox flag. **Alex `applyAlexAntennaForBand(band, isTx=true)` fires for the first time** (3P-I-b infrastructure activates here). Penny external PA bits update. OcMatrix triggers. Apollo auto-tune trigger via existing controller |
| **ATT-on-TX auto** | `m_bATTonTX` + `_forceATTwhenPSAoff` (per-band stored ATT applied during TX, force 31 dB when PS off or in CW). Reuses `StepAttenuatorController` from 3G-13 |
| **Display.MOX overlay** | TX-mode color scheme on SpectrumWidget; W12 TX filter overlay activates (3G-8 stub fires); W14 TX zero-line activates; `Display.TXAttenuatorOffset` shifts dBm scale |
| **PA telemetry live** | 3M-0's MeterPoller routing has data flowing now — Fwd/Rev/SWR meters paint real values during TUNE |

#### 5.1.2 UI surfaces (deep-wired in 3M-1a)

| Surface | Controls visible | Hidden until later |
|---|---|---|
| **TxApplet** | RF Power, Tune Power, MOX, TUN, Fwd/Rev/SWR/ALC gauges | Mic Gain, Profile, PROC/LEV/EQ/CFC/DEXP/VOX, 2-TONE, PS-A |
| **Setup → Transmit → Power & PA** | Max power, per-band tune-power table, SWR thresholds, watchdog, fan, PA telemetry | (3M-0 already shipped surface; 3M-1a wires `setOutputPower` to drive the actual TX) |
| **SpectrumWidget** | TX-mode color scheme during TX, W12/W14 overlays, TXAttenuatorOffset | (none) |
| **Status bar** | PTT source label = "MANUAL" only in 3M-1a (no other PTT sources yet) | MIC/CW/CAT/VOX/SPACE/X2/TCI |
| **Keyboard shortcuts** | Space = MOX (matches Thetis pattern), Ctrl+T = TUN | (3M-2 adds CW-related) |

#### 5.1.3 Verification

- ANAN-G2 + dummy load: hit TUN, observe ~25 W tune carrier, verify
  Fwd/Rev/SWR meters paint, confirm Alex routing per `txAnt[band]`,
  confirm StepAtt-on-TX value changes preamp/att appropriately, confirm
  tune carrier drops cleanly when TUN released (no audible click —
  `uslew` working).
- Hermes Lite 2: same test on P1 protocol path.
- **Two-phase Thetis review** focused on `chkMOX_CheckedChanged2` (TX
  direction), `chkTUN_CheckedChanged`, `HdwMOXChanged`,
  `tune_power_by_band`, `Display.MOX` paths, ATT-on-TX logic. Userland:
  TxApplet TUN button + tune-power slider + Setup → Transmit → Power & PA
  + tune-meter mode dropdown.

#### 5.1.4 Files

**New:**
- `src/core/MoxController.{h,cpp}`
- `src/core/TxChannel.{h,cpp}`
- `src/core/TxMicRouter.{h,cpp}` (interface only; sources stubbed for 3M-1a)

**Modified:**
- `src/core/WdspEngine.{h,cpp}` — `createTxChannel` / `destroyTxChannel` /
  `setPureSignalSource` API
- `src/core/RadioModel.{h,cpp}` — own `MoxController` + `TxChannel` +
  `TxMicRouter`
- `src/models/TransmitModel.{h,cpp}` — `MoxController` integration
- `src/core/RadioConnection.{h,cpp}` — `sendTxIq()` virtual
- `src/core/P1RadioConnection.cpp` — TX I/Q in EP2 zones, MOX bits, tune
  power
- `src/core/P2RadioConnection.cpp` — TX I/Q on port 1029, sendCmdTx
  populates `m_tx[0].driveLevel`
- `src/gui/SpectrumWidget.{h,cpp}` — Display.MOX overlay paint +
  TXAttenuatorOffset
- `src/gui/meters/MeterPoller.{h,cpp}` — TX bindings live during TX
- `src/gui/applets/TxApplet.{h,cpp}` — TUN/Tune-Power/RF-Power/MOX wired;
  out-of-phase controls hidden
- `src/gui/setup/TransmitSetupPages.cpp` — Power & PA page wiring activates

#### 5.1.5 Risk

**High.** First MOX. Things that can go wrong: MOX bit mistimed → rejected
by radio; TX I/Q frame format wrong → garbled tune carrier or PA fault;
Alex routing wrong → carrier into wrong antenna (potential damage if no
dummy load); ATT-on-TX mistimed → preamp damage. 3M-0's safety net catches
some but not all. Worth allocating extra review time and hardware test
time.

### 5.2 Phase 3M-1b — Mic + SSB Voice (with MON pulled in)

**Goal:** First voice transmission. PC mic + radio mic both wired and
selectable. Clean unprocessed SSB through the TXA pipeline with all
speech-processing stages built but `Run=false`. **MON (TX monitor) pulled
forward from 3M-3c** so users hear themselves talk through headphones.

**Mode scope:** SSB modes (LSB / USB / DIGU / DIGL) are TX-enabled in 3M-1b
because they share the SSB chain upstream of the mode-gated `ammod` /
`fmmod` stages. AM, SAM, DSB, FM, and DRM modes produce no usable TX
output until 3M-3b activates the mode-gated stages — MOX in those modes is
rejected by `BandPlanGuard` with a tooltip ("AM/FM TX available in 3M-3b
phase") to avoid a "MOX works but no carrier" surprise. CW is rejected
until 3M-2a (different code path entirely).

#### 5.2.1 Components

| Subsystem | What lights up |
|---|---|
| **`TxMicRouter` implementations** | `PcMicSource` (PortAudioBus + PipeWireBus reuse from 3O VAX), `RadioMicSource` (subscribes to P2 port 1026 / P1 EP2 mic byte zone). Strategy pattern with selector logic, MOX-locked. HL2 forced to PC-mic |
| **TxChannel full pipeline** | All 22 stages present, but only `rsmpin`, `panel` (mic gain), `micmeter`, `bp0`, `alc`, `uslew`, `alcmeter`, `cfir`, `rsmpout`, `outmeter` actively running. Speech-processing stages built but `Run=false` |
| **Per-mode TXA configuration** | `SetTXAMode` per slice mode; `SetTXABandpassFreqs` per filter (per-mode defaults from `InitFilterPresets` already in SliceModel from 3E) |
| **Mic mute** | `chkMicMute` → `SetTXAPanelGain1(0)` |
| **PTT sources active** | All 9 PTTMode values wired except `CW` (3M-2) and `TCI` (3J): `MIC` (radio mic-jack PTT), `MANUAL` (MOX button), `CAT` (basic — full CAT in 3K), `VOX` (basic — full DEXP in 3M-3a-iii), `SPACE` (rear PTT-in), `X2` (rear dot-dash), `NONE` |
| **Hardware mic-jack bits** | `MicXlr` / `MicBoost` / `MicTipRing` / `MicBias` / `MicPTT` — only on radio-mic path |
| **Anti-VOX** | Path-agnostic: both sources expose anti-vox tap. `MoxController::shouldVoxKey()` reads from active source. Anti-vox source selector |
| **VOX (basic)** | Threshold + gain + hang-time. Full DEXP parameter set deferred to 3M-3a-iii |
| **TX meters live** | `MeterBinding::TxMic` (103) reads from `TxChannel::getMeter(TXA_MIC_PK/AV)`. ALC meter live, mic meter live. EQ/Lev/CFC/Comp meters return zero until 3M-3a |
| **MON (pulled forward from 3M-3c)** | TXA output siphon → audio mixer → RX speaker. Separate monitor volume slider. Lets user hear themselves through headphones during voice TX |

#### 5.2.2 UI surfaces deep-wired in 3M-1b (additions)

| Surface | Controls |
|---|---|
| **TxApplet** | Mic Gain slider, basic VOX toggle, MON toggle + monitor volume slider |
| **Setup → Audio → TX Input** *(new page)* | PC mic / Radio mic radio buttons (HL2 hides Radio jack), PC backend selector, PC device selector, buffer size slider with ms-latency readout, sample rate combo, Test Mic button + live VU. Radio jack settings: source jack selector, MicBoost / MicTipRing / MicBias / MicPTT checkboxes. Mic gain slider mirrors TxApplet |
| **Setup → Audio → Mic** *(extended)* | Mic mute toggle, anti-VOX source selector, anti-VOX gain slider, basic VOX threshold/gain/hang-time |
| **Status bar** | PTT source label updates as PTT fires from different sources; VOX active indicator |

#### 5.2.3 Verification

- ANAN-G2 + dummy load: PC mic, talk, observe SSB out on a separate
  receiver. Switch to radio-mic-jack input, verify mic-jack hardware bits
  flip correctly via wireshark on C&C frames.
- HL2: PC mic only (no jack), verify Radio jack option hidden. Talk,
  verify SSB out.
- MON: enable, talk, hear self in headphones. Verify monitor volume slider
  is independent of RX audio volume.
- Latency target: PTT-press → first audible audio on receiver side <50ms
  total round-trip.
- **Two-phase Thetis review:** `audio.cs` mic pipeline,
  VOX / anti-VOX logic, `cmaster.CMSetTXAPanelGain1`, all
  `MicXlr`/`MicBoost`/etc. setters, `Setup → Audio → Primary` page.

#### 5.2.4 Files

**New:**
- `src/core/audio/PcMicSource.{h,cpp}`
- `src/core/audio/RadioMicSource.{h,cpp}`
- `src/core/audio/TxMicRouter.{h,cpp}` (full impl)
- `src/gui/setup/audio/TxInputPage.{h,cpp}`

**Modified:**
- `src/core/TxChannel.{h,cpp}` — full TXA chain construction with all
  stages off
- `src/core/AudioEngine.{h,cpp}` — MON path (TXA siphon → speaker mixer)
- `src/core/RadioConnection.{h,cpp}` — `micFrameDecoded()` signal
- `src/core/P1RadioConnection.cpp` — parse mic from EP2
- `src/core/P2RadioConnection.cpp` — subscribe to port 1026
- `src/gui/applets/TxApplet.{h,cpp}` — mic gain, VOX, MON
- `src/gui/setup/audio/MicSetupPage.cpp` — anti-VOX source, VOX basics
- `src/core/MoxController.{h,cpp}` — PTT source dispatch

#### 5.2.5 Risk

**Medium.** Latency-sensitive — PortAudio buffer tuning matters. Mic gain
calibration vs. ALC interaction can produce TX overdrive if defaults wrong
(3M-0's HighSWR catches gross faults; audible distortion that doesn't trip
SWR can damage front-end). Need careful unity-gain testing.

### 5.3 Phase 3M-1c — Polish & Persistence

**Goal:** Wrap 3M-1 with VFO TX badge, two-tone test, mic profile schema,
full TxApplet for currently-active scope, persistence audit.

#### 5.3.1 Components

| Subsystem | What lights up |
|---|---|
| **VFO Flag TX badge** | `VfoDisplayItem::setTransmitting(bool)` (already exists from 3G-8 stubs) wired to `MoxController::moxChanged` |
| **Two-tone test** | `chk2TONE_CheckedChanged` ported (`console.cs:27397-27444`); mutually exclusive with TUN; uses `SetTXAPostGenTTMag/TTFreq/TTPulse*` setters; separate `twotone_tune_power` slider |
| **Mic profile schema** | Per-MAC AppSettings keys: `tx/profile/active`, `tx/profile/<name>/{micGain, txEqProfile, levelerOn, cpdrOn, cfcOn, dexpOn, voxOn}`. Save/load from TxApplet profile combo. Factory presets shipped (placeholders for fields populated by 3M-3a stages) |
| **TxApplet completes for shipped scope** | Profile combo with save/load, RF Power, Tune Power, Mic Gain, MOX, TUN, 2-TONE active. PROC/LEV/EQ/CFC/DEXP/VOX/PS-A still hidden (3M-3+) |
| **Persistence audit** | Every TX-side AppSettings key written and round-tripped in tests |
| **`MoxChangeHandlers` / `MoxPreChangeHandlers`** | Generalized as Qt signals on `MoxController` for plugin/observer hookpoint |
| **TX recording (audio side)** | Mic input recorded through TXA tap; WAV output. Reuses existing recording infrastructure |

#### 5.3.2 UI surfaces (additions in 3M-1c)

| Surface | Controls |
|---|---|
| **TxApplet** | Profile combo with save/load, 2-TONE button |
| **VFO Flag** | TX badge appears during MOX (color per mode, mode label) |
| **Setup → Transmit → TX Profiles** | Profile bank list with save/load/delete/rename, factory presets, bundled fields (most placeholders for 3M-3a) |
| **Setup → Recording → Mic** | File path, format, bitrate, auto-record on MOX |
| **Right-click popups** | VFO Flag right-click for TX-related options (source-first Thetis check via pre-code review) |

#### 5.3.3 Verification

- Hit 2-TONE on dummy load + spectrum analyzer; observe two clean tones
  at ±700 Hz, 3rd-order intermod products visible; verify SWR/Fwd/Rev
  meters track.
- Save mic profile "Heil PR40", restart app, verify settings reload.
- VFO Flag TX badge visible during all TX modes (TUN, 2-TONE, mic SSB).
- **Two-phase Thetis review:** `chk2TONE_*` paths, mic profile manager
  (Setup → Audio → Mic Profile sub-tab), persistence schema in
  `console.cs:3099-3101`/`4922-4927`, AndromedaIndicator hooks.

#### 5.3.4 Files

**New:**
- `src/core/MicProfileManager.{h,cpp}`

**Modified:**
- `src/gui/meters/VfoDisplayItem.{h,cpp}` — badge wire-up
- `src/gui/applets/TxApplet.{h,cpp}` — profile combo + 2-TONE
- `src/gui/setup/TransmitSetupPages.cpp` — TX Profiles page wiring
- `src/core/MoxController.{h,cpp}` — handler signals generalized

#### 5.3.5 Risk

**Low.** Mostly UI + persistence work, no new RF behavior.

## 6. Phase 3M-2 — CW TX  *(Deferred — runs after §7 3M-3 + HL2 ATT/filter audit)*

> **Schedule swap (2026-04-29):** 3M-2 originally scheduled to run before
> 3M-3.  After 3M-1c shipped (PR #152), JJ flagged HL2 ATT/filter code
> paths that could damage the radio if TX is engaged → HL2 hardware bench
> was deferred for that audit.  3M-3 (§7) doesn't need the HL2 bench (DSP
> stages introspectable on ANAN-G2), so it was pulled forward.  3M-2 CW
> TX waits until 3M-3 ships AND the HL2 ATT/filter audit closes (so an
> HL2 can be CW-bench'd safely).
>
> 3M-2 also absorbs the HL2 CWX bit-3 follow-up (`networkproto1.c:1247-
> 1252 [@c26a8a4]` — desk-review B3, "HL2 firmware uses bit 3 of I-low
> byte for CWX PTT").

CW splits cleanly into two sub-phases. Different MOX semantics (TXA NOT
enabled in CW per Thetis special case at `console.cs:29589-29598`),
different audio path (sidetone via `gen0` or hardware), separate keyer
hardware.

### 6.1 Phase 3M-2a — Sidetone + Keyer + QSK

**Goal:** Full CW transmit with sidetone (SW + HW paths), firmware keyer,
semi-break-in/QSK timing. Keyboard-keying via menu shortcuts; full paddle
USB driver in 3M-2b.

**Key components:**
- CW MOX special case: `MoxController` skips
  `WDSP.SetChannelState(WDSP.id(1,0), 1, 0)` in CW mode; routes via sidetone
- SW sidetone path: `gen0` stage + `create_sidetone()` port from `cmaster.c:235-251`
- HW sidetone path: `SetSidetoneRun` + `SetCWSidetone`
- All 10 CW NetworkIO setters: `SetCWKeyerSpeed/Mode/Iambic/Weight/EdgeLength/Spacing/PaddleReverse/PTTDelay/HangTime/BreakIn`
- CW pitch + VFO offset on MOX
- APF (Audio Peak Filter) RX-side coupling (already partial in 3G-10)
- CW filter presets per `console.cs:5363-5438`

**UI surfaces:** PhoneCwApplet → CW tab activates fully (sidetone toggle
SW/HW, pitch slider, volume slider, WPM, keyer mode combo, weight, paddle
reverse, edge length, break-in mode, QSK delay, hang time, spacing toggle,
firmware keyer toggle). VFO Flag TX badge shows "CW" label. Setup → CW
mirrors all controls.

**Risk:** Medium. CW timing is unforgiving — operators feel <10ms timing
variation as "rough" sending. QSK round-trip especially sensitive.

### 6.2 Phase 3M-2b — CWX + Paddle USB Driver

**Goal:** Canned-text sender (CWX) and external paddle USB driver. Round
out CW operation.

**Key components:**
- CWX form port from `cwx.cs:69-815` — 10 message slots, repeat-with-delay,
  callsign macros, play list, abort
- CWInput USB driver port from `Console/CW/CWInput.cs` — `QSerialPort`
  cross-platform, DTR/RTS bits → dot/dash dispatch
- Iambic emulation in software for external paddle when SW keyer mode

**UI surfaces:** Menu → DSP → CWX opens modeless dialog. Setup → CW →
External Paddle for device + DTR/RTS pin mapping.

**Risk:** Low. Well-defined Thetis ports.

## 7. Phase 3M-3 — TXA Speech Processing + Mode-Specific + Audio Routing  *(Next — pulled forward 2026-04-29)*

> **Schedule swap (2026-04-29):** Was originally scheduled after 3M-2.
> Pulled forward — see §6 for rationale.  Branched at
> `feature/phase3m-3-tx-processing` worktree on 2026-04-29 from
> `origin/main` `1d40670` (post-PR-#152 merge).

The largest sub-phase by surface area. Five sub-PRs total: 3M-3a-i / -ii /
-iii (the speech chain), then 3M-3b (mode-specific), then 3M-3c (TX audio
routing).

### 7.1 Phase 3M-3a-i — Speech Chain Top: Phase Rotator + TX EQ + Leveler + ALC tuning

**Goal:** Activate front-of-chain speech-processing stages.

**TXA stages activated:** `phrot` → `eqp` (10-band TX EQ) → `leveler` → ALC
tuning surface.

**WDSP setters:**
- Phase Rotator: `SetTXAPHROTRun/Corner/Nstages/Reverse`
- 10-band TX EQ: `SetTXAEQRun/GrphEQ10/EQNC/EQMP/EQCtfmode/EQWintype`
- Leveler: `SetTXALevelerSt/Top/Decay`
- ALC tuning: `SetTXAALCMaxGain/Decay`

**TX EQ profile bank:** Per-MAC AppSettings, save/load/delete/rename,
factory presets ported from Thetis (Heil PR40 SSB, Yeti, vintage radio,
etc.).

**UI surfaces:** TxApplet quick toggles `[PROC]` (placeholder until
3M-3a-ii), `[LEV]`, `[EQ]`. TxApplet right-click popups (TX EQ popup
mirrors `DspParamPopup` from 3G-10). Setup → Transmit → Speech Processor
sections for each stage with full parameters + plot. Mic profile bundle
gains EQ section.

**Risk:** Medium. Each WDSP setter must use exact Thetis parameter ranges.
Defaults wrong = blown PA. Source-first verification critical.

### 7.2 Phase 3M-3a-ii — Dynamics: CFC + CPDR + CESSB

**Status:** **Complete (pending bench)** — `feature/phase3m-3-tx-processing`,
2026-04-30, 17 GPG-signed commits stacked on 3M-3a-i, 253/253 ctest green,
PR #TBD. Awaiting JJ bench on ANAN-G2.

**Scope adjustment carried during implementation:** Phase Rotator (`phrot`)
was deferred from 3M-3a-i with a code comment in `TransmitModel.h:737`;
folded into 3M-3a-ii at JJ's direction during prep so all
top-of-chain DSP guards land together. Pre-emphasis was de-scoped from this
sub-PR to 3M-3b (FM-mode work).

**Goal:** Activate dynamics/overshoot section. CFC gets a dedicated config
dialog (Thetis pattern).

**TXA stages activated:** `phrot` (Phase Rotator, 0-stage to N-stage all-pass
chain) → `cfcomp` (CFC, 5-band freq compander; 10-band F/COMP/POST-EQ + Qg/Qe
ceiling-Q skirts after surgical WDSP cfcomp.c sync to Thetis v2.10.3.13) →
`compressor` (CPDR) → `osctrl` (CESSB; gated on CPDR via
`TXASetupBPFilters: bp2.run = compressor.run && osctrl.run`).

**WDSP setters (delivered):**
- CFC (6): `SetTXACFCOMPRun/Position/profile/Precomp/PeqRun/PrePeq`
- CPDR (2): `SetTXACompressorRun/Gain`
- CESSB (1): `SetTXAosctrlRun`
- PhRot (3): `SetTXAPhaseRotCorner/Nstages/Reverse` (folded from 3M-3a-i)

**CFC profile bank:** delivered — Save/load/delete/rename via
MicProfileManager schema (+41 keys → bundle now 91 keys); 19 of 21 factory
profiles ported verbatim from `database.cs:9282-9418 [v2.10.3.13]` (155
overrides) including AM 10k's unique `PhRotStages=9`.

**UI surfaces (delivered):**
- **CfcSetupPage** (Setup → Transmit → CFC) rewrite: Phase Rotator group
  (Enable + Reverse + Corner + Stages), CFC group (Enable + Post-EQ + Precomp +
  Post-EQ Gain + Open Dialog button), CESSB group (Enable + status text)
- **TxCfcDialog** (modeless): profile combo + Save/SaveAs/Delete/Reset +
  Precomp/PostEQ-Gain spinboxes + 10×3 per-band F/COMP/POST-EQ grid.
  Originally landed scalar-complete but spartan. Full Thetis `ucParametricEq`
  widget port shipped via PR #159 (10 batches). `ParametricEqWidget` lives at
  `src/gui/widgets/ParametricEqWidget.{h,cpp}`; both TxCfcDialog and
  TxEqDialog use it (drag handles, per-band Q sliders, freq sliders, live
  curve overlay). Design hand-off doc
  `docs/architecture/phase3m-3a-ii-cfc-eq-parametriceq-handoff.md` retained
  as historical record.
- **PhoneCwApplet** PROC button/slider wired to `cpdrOn`/`cpdrLevelDb` (0..2
  step → 0..20 dB linear); replaces NOR/DX/DX+ tick labels with numeric
  "X dB". TxApplet `[PROC]` button removed (duplicate caught by JJ; surfaced
  feedback memory `feedback_survey_before_adding_controls.md`).
- **SpeechProcessorPage** dashboard live-status bindings (CFC/CPDR/CESSB on/off
  + level + position).

**Risk:** Medium-high → realized as predicted: CFC dialog is intricate UI
work, hence the scalar-complete landing + ParametricEq follow-up. CPDR +
CESSB defaults sourced byte-for-byte from Thetis factory profiles.

### 7.3 Phase 3M-3a-iii — Gating: DEXP/VOX (full parameters)

**Goal:** Full DEXP downward expander parameter set with Thetis-faithful main-console peak meters and Setup-form parity.

**WDSP setters (14 per `cmaster.cs:160-206 [v2.10.3.13]`):** `SetDEXPRun`,
`SetDEXPRunVox`, `SetDEXPDetectorTau`, `SetDEXPAttackTime`,
`SetDEXPReleaseTime`, `SetDEXPHoldTime`, `SetDEXPExpansionRatio`,
`SetDEXPHysteresisRatio`, `SetDEXPAttackThreshold`, `SetDEXPLowCut`,
`SetDEXPHighCut`, `SetDEXPRunSideChannelFilter`, `SetDEXPRunAudioDelay`,
`SetDEXPAudioDelay`. Three already shipped in 3M-1b
(RunVox / AttackThreshold / HoldTime); 11 remain. Plus 2 read-only meter
imports: `GetDEXPPeakSignal` and `WDSP.CalculateTXMeter(MIC)` for the
live peak displays.

**UI surfaces:** PhoneCwApplet wires the existing un-wired VOX + DEXP stubs
(Phone-page Controls #10 + #11) per the PROC-consolidation pattern from
3M-3a-ii. Each row gets `[ON]` toggle + threshold slider with thin
peak-meter strip stacked under it (matches Thetis `picVOX` / `picNoiseGate`
layout). VOX row also keeps the Hold (ms) slider (the 3M-1b stub
"VOX delay" renamed to match `lblDEXPHold.Text = "Hold (ms)"`). Right-click
on either button opens Setup → Transmit → DEXP/VOX page. The duplicate VOX
button on TxApplet (`m_voxBtn` at TxApplet.cpp:374) and the entire
`VoxSettingsPopup.{h,cpp}` widget are removed. Setup gains a new
`DexpVoxPage` with two group boxes matching `tpDSPVOXDE` 1:1: `grpDEXPVOX`
(VOX/DEXP enables + 7 numeric controls) and `grpDEXPLookAhead`
(enable + look-ahead spinbox). Mic profile bundle gains ~14 keys
(baseline 94 → 108).

**Out of scope (revised after 2026-05-03 archaeology):**
- **TX AMSQ** (`SetTXAAMSQRun/MutedGain/Threshold`): Thetis itself does not
  wire any of these in stock (zero hits across cmaster.cs / dsp.cs /
  radio.cs / setup.cs / setup.designer.cs); the WDSP setters exist but the
  TXA `amsq` stage runs at WDSP defaults forever. Per source-first
  ("if Thetis has it we want it"), TX AMSQ is moved to **3M-3b** where it
  belongs alongside other AM-mode TX work (mode-gated to AM/SAM/DSB).
- ~~**Side-channel filter UI**~~: pulled IN-SCOPE 2026-05-03 mid-port.
  Original spec wrongly claimed Thetis had no SCF UI; source-first
  read by the Batch B agent surfaced `grpSCF` on `tpDSPVOXDE`
  (setup.Designer.cs:45157+ [v2.10.3.13]). Now bound to TransmitModel
  + Setup page DexpVoxPage (3rd group box).
- **`ptbNoiseGate` writes to WDSP**: replicated as decorative per Thetis
  quirk (the slider value drives only the meter's threshold marker, never
  pushed to WDSP). Inline comment documents the faithful-quirk.

**Risk:** Medium. DEXP timing parameters interact subtly. Decorative
`ptbNoiseGate` may confuse first-time users (mitigated by tooltip).

**Design doc:** `docs/architecture/phase3m-3a-iii-dexp-vox-design.md`.

### 7.4 Phase 3M-3b — Mode-Specific TX (AM / FM / Digital)

**Goal:** Activate `ammod` and `fmmod` TXA stages with full per-mode
controls.

**Key components:**
- AM: `SetTXAAMCarrierLevel`, AM/SAM/DSB mode variants
- FM: `SetTXAFMDeviation`, `SetTXAFMAFFilter`, `SetTXACTCSSRun/Freq`,
  `SetTXAFMEmphPosition/MP/NC`, `SetTXAFMPreEmphFreqs`
- FM TX offset: `current_fm_tx_mode` (Simplex/Low/High), `fm_tx_offset_mhz`,
  `chkFMTXRev`
- Digital modes: `DSPMode.DIGU/DIGL/DRM` route through SSB chain upstream
  of `ammod`/`fmmod`

**UI surfaces:** Setup → DSP → Modes → AM (carrier level + mode), Setup →
DSP → Modes → FM (deviation + AF filter + pre-emphasis + CTCSS + TX offset
+ reverse), Setup → DSP → Modes → Digital. VFO Flag mode label updates
(DIGU/DRM/CTCSS-XX).

**Risk:** Medium. AM carrier level + FM deviation are user-adjustable but
blow PA if set badly.

### 7.5 Phase 3M-3c — TX Audio Routing (without MON)

**Goal:** TX-side audio routing not covered elsewhere — VAX integration for
digital modes, LR audio swap, TX I/Q recording, anti-VOX deep UI.

**Key components:**
- VAX per-mode auto-routing: DIGU/DIGL mode auto-switches `TxMicRouter` to
  VAX as audio source. Per `feedback_vax_not_vac_port` — surface as VAX
  features, NOT as Thetis VAC port
- `LRAudioSwap` toggle (`SetLRAudioSwap`)
- TX I/Q recording (different from mic recording in 3M-1c)
- Anti-VOX deep UI: multi-source selector, `cmaster.CMSetAntiVoxSourceWhat`
- `UpdateAAudioMixerStates` equivalent during MOX transitions
- `BypassVACWhenPlayingWAV` — play a WAV file as TX audio source

**UI surfaces:** Setup → Audio → TX Routing (LR swap, DIGU→VAX auto-route
toggle, multi-source anti-VOX selector). Setup → Recording → TX I/Q. Menu
→ Tools → Play WAV as TX (modeless dialog).

**Risk:** Low-medium. Per-mode source switching needs careful state
management.

## 8. Phase 3M-4 — PureSignal

### 8.1 Phase 3M-4a — PureSignal core: PSForm + calcc + iqc

**Goal:** Predistortion / PA linearization. The biggest risk-weighted phase
in the entire epic — timing-sensitive, hardware-dependent. Pure Thetis
port; no AetherSDR template (Flex radios do PS server-side).

**Key components:**
- calcc 10-state machine port from `wdsp/calcc.c`. States: `LRESET → LWAIT
  → LMOXDELAY → LSETUP → LCOLLECT → MOXCHECK → LCALC → LDELAY → LSTAYON →
  LTURNON`. Amplitude-binned sample collection, piecewise cubic Hermite
  spline correction
- iqc real-time correction port from `wdsp/iqc.c`. Runs on every TX sample
- All `SetPS*` setters (~15 in total): Control, Mox, Reset, Mancal,
  Automode, Turnon, MoxDelay, LoopDelay, TXDelay, HWPeak, Ptol, PinMode,
  MapMode, Stabilize, IntsAndSpi. Plus radio-side `SetPureSignal(int enable)`
  to route ADC1 to PS feedback DDC
- Feedback DDC routing: `WdspEngine::setPureSignalSource(rxId=0, txId=1)`,
  ADC cntrl1 override `(rx_adc_ctrl1 & 0xf3) | 0x08`
- TX/RX delay alignment: fractional delay lines, 20ns step
- Auto-attenuation polish: `m_bATTonTX` + `_forceATTwhenPSAoff`, target
  feedback level 128-181 of 256
- State save/restore per-MAC per-band

**UI surfaces:** Menu → DSP → PureSignal opens PSForm modeless dialog.
PSForm: Enable button, Calibrate, Auto-Cal toggle, feedback level meter
(green/yellow/red), delay sliders (Mox/Loop/TX), Stabilize/Pin/Map radio
buttons, Tolerance selector, Save/Restore coefficients, two-tone test
button, calcc state indicator. TxApplet `[PS-A]` button. Setup → Transmit →
PureSignal mirrors PSForm. Status bar PS active indicator.

**Risk:** Very high. Timing-sensitive cross-channel state machine. PS
misconfiguration drives PA into non-linear region with predistortion
making things *worse*.

### 8.2 Phase 3M-4b — AmpView (PS visualization)

**Goal:** Time-domain visualization of TX-output vs feedback-RX correlation.
Power-user diagnostic.

**Key components:** State save/restore per Thetis pattern at
`PSForm.cs:781`. Time-domain capture via `SetPSChannel`. Correction curve
overlay.

**UI surfaces:** Menu → Tools → AmpView opens modeless dialog. TX-side +
feedback-RX time-domain plots, correction curve, capture/pause/single-shot
buttons, cursor measurement tools.

**Risk:** Low. Visualization-only, no RF impact.

## 9. Phase 3M-5 — EER (Ganymede / Class-D/E PA)

**Goal:** Envelope Elimination & Restoration support for E-class PAs. Niche
but well-defined Thetis feature.

**Key components:**
- EER engine: `SetEERRun/AMIQ/Mgain/Pgain/RunDelays/Mdelay/Pdelay/Size/Samplerate`,
  cmaster routing via `CMSetEERRun`
- E-class enable: `EnableEClassModulation`
- TX channel split outputs: `pcm->xmtr[i].out[0]` = normal I/Q, `out[1]` =
  magnitude stream

**UI surfaces:** Setup → Hardware → PA → EER sub-tab (only visible when
`board.hasEer`). Full 9-parameter UI + E-class enable.

**Risk:** Low (gated to specific hardware).

## 10. Phase 3M-6 — TX Polish

**Goal:** All small TX-side touches that don't fit elsewhere.

**Key components:**
- TX-mode spectrum source switch: `Display.MOX = true` → SpectrumWidget
  reads from `txa[ch].sip1` siphon
- `Display.TXAttenuatorOffset` polish for edge cases
- `_pause_DisplayThread` full implementation
- `MoxChangeHandlers` / `MoxPreChangeHandlers` API documented for plugins
- AndromedaIndicator → generalized Qt signal (`MoxController::frontPanelStateChanged`)
- CAT TX surface: CATPTT (RTS/DTR/BitBang), CATBIN, CATReadSWR
- N1MM focus delay
- TUNE pulse mode: `_tune_pulse_enabled`, `SetupTunePulse`
- Aries ATU full integration: `LimitTXRXAntenna`, `ATU_Tune`

**UI surfaces:** Setup → CAT → TX Commands. Setup → Transmit → TUNE Pulse.
Setup → Hardware → Antenna → Aries ATU sub-section.

**Risk:** Low. Polish-level work.

## 11. Phase 3M-7 — cmASIO-equivalent (full audio codec replacement)

**Goal:** Power-user feature — replace radio's onboard codec with host audio
for sub-5ms latency. Cross-platform without Steinberg SDK issue.

**Key components:**
- New `AudioCodec` enum: `RadioOnboard` (HERMES default), `HostNative`
  (cmASIO equivalent)
- Windows: WASAPI Exclusive Mode (already in PortAudio's `PA_USE_WASAPI`)
- macOS: Core Audio raw (or via PortAudio's `PA_USE_CORE_AUDIO`)
- Linux: PipeWire-native (already exists from 3O VAX)
- Setup gating: hidden behind `Setup → Audio → Advanced → Replace radio audio with host audio` toggle. Warning text + per-MAC persisted
- Hot-swap behavior: requires reconnect to take effect (matches Thetis)

**UI surfaces:** Setup → Audio → Advanced (hidden by default). Master
toggle, host audio device selector (in + out), buffer size slider with
latency readout, backend selector (Auto / WASAPI Exclusive / Core Audio /
PipeWire-native), Apply + Restart button. Status bar "Host Audio"
indicator.

**Risk:** Medium. New cross-platform code paths. Power-user feature, small
initial user pool.

## 12. Risk register

| Phase | Risk | Mitigation |
|---|---|---|
| 3M-0 | Permanent foldback bug → noticed at 3M-1a | Unit tests for SwrProtectionController; observable but inactive in 3M-0 |
| **3M-1a** | **First MOX. Wrong wire format → garbled carrier or PA fault. Wrong Alex routing → into wrong antenna. Wrong ATT timing → preamp damage.** | **Extra review window. Hardware verification on multiple boards (ANAN-G2 + HL2 minimum). 3M-0 safety net active.** |
| 3M-1b | Latency-sensitive. Mic gain × ALC interaction → TX overdrive distortion that doesn't trip SWR. | Careful unity-gain testing. Latency measurement on real hardware |
| 3M-1c | Low — UI + persistence | Standard tests |
| 3M-2a | CW timing <10ms variation feels rough. QSK round-trip. | Hardware timing measurement. Multiple keyer modes verified |
| 3M-2b | Low — well-defined ports | Standard tests |
| 3M-3a-i | WDSP setter parameter ranges. Defaults wrong = blown PA | Source-first verification. Range clamping in setters |
| 3M-3a-ii | CFC dialog intricate. CPDR+CESSB splatter | Source-first defaults. Spectrum analyzer verification |
| 3M-3a-iii | DEXP timing interactions | Audible test cases |
| 3M-3b | AM carrier / FM deviation user-adjustable, can blow PA | Source-first ranges enforced in setters |
| 3M-3c | Per-mode source switching state management | MOX-locked switching |
| **3M-4a** | **Timing-sensitive cross-channel state machine. PS misconfig → PS makes IMD worse, not better. Auto-att MUST work or feedback DDC overdrives.** | **Highest review priority. Hardware testing on multiple boards. Extensive logging during initial PS calibrations.** |
| 3M-4b | Low — visualization only | Standard tests |
| 3M-5 | Niche hardware (Ganymede). Limited test fleet | Feature flag; field-verification post-merge |
| 3M-6 | Low — polish | Standard tests |
| 3M-7 | Cross-platform code paths. Hot-swap edge cases | Per-platform hardware testing |

## 13. Dependency map

```
3M-0 (PA Safety) ──→ 3M-1a (TUNE)  ──→ 3M-1b (SSB+MON) ──→ 3M-1c (polish)
                                                                  │
                                                                  ▼
                                       3M-2a (CW chain) ──→ 3M-2b (CWX/paddle)
                                                                  │
                                                                  ▼
                              3M-3a-i ──→ 3M-3a-ii ──→ 3M-3a-iii
                                                            │
                                                            ▼
                                                       3M-3b (modes)
                                                            │
                                                            ▼
                                                       3M-3c (audio routing)
                                                            │
                                                            ▼
                                                       3M-4a (PS core)
                                                       │      │       │
                              ┌────────────────────────┘      │       │
                              ▼                               ▼       ▼
                         3M-4b (AmpView)               3M-5 (EER)  3M-6 (polish)
                                                                       │
                                                                       ▼
                                                                  3M-7 (cmASIO-eq)
```

3M-4b, 3M-5, 3M-6, 3M-7 are independent of each other after 3M-4a. Could
parallelize on multiple branches; for solo work, sequential makes sense.

## 14. Out of scope (deferred outside the 3M epic)

- **Full TCI server** — depends on 3O IAudioBus contract; lives in 3J
- **CW autotune** — no WDSP API, deferred indefinitely
- **FreeDV codec** — Thetis doesn't have it either; out of scope for parity
- **Mic profile import from Thetis databases** — port format compatibility
  is its own project; cosmetic for native NereusSDR users
- **Multi-radio simultaneous TX** — out of scope for any 3M phase

## 15. Verification approach

Every sub-PR ships with:

1. **Pre-code Thetis review** (gates implementation start)
2. **Implementation per the checklist**
3. **Unit tests** for pure logic (BandPlanGuard, SwrProtectionController,
   TxMicRouter strategy switching, CW timing, calcc state machine
   transitions)
4. **Integration tests** for cross-thread paths (MoxController phase
   sequencing, TxChannel WDSP lifecycle, telemetry-to-meter routing)
5. **Hardware verification matrix** — physical things to verify on real
   hardware. ANAN-G2 + HL2 minimum, more boards as available.
6. **Post-code Thetis review** (gates merge)
7. **Per-MAC persistence audit** — every new AppSettings key written and
   round-tripped
8. **Tooltip / user-string audit** — plain English, cites in source
   comments only

The hardware verification matrix per phase lives in
`docs/architecture/phase3m-XX-verification/README.md` (matches the 3G-RX
epic verification doc pattern). Each row is a physical test the user runs
against hardware before merge sign-off.

## 16. Glossary

| Term | Meaning |
|---|---|
| **MOX** | Manual Operate Transmit; UI/state for "TX is active" |
| **TUNE** | Reduced-power carrier transmit for antenna tuning |
| **TXA** | WDSP transmit-side processing chain (22 stages) |
| **RXA** | WDSP receive-side processing chain |
| **DDC** | Digital Down-Converter (in radio FPGA) |
| **CFC** | Continuous Frequency Compressor (5-band TXA stage) |
| **CPDR** | Compressor (hard-clip + low-pass) |
| **CESSB** | Controlled-Envelope SSB (osctrl TXA stage) |
| **DEXP** | Downward Expander (a.k.a. VOX gating) |
| **AMSQ** | TX voice squelch |
| **uslew** | TX up-slew envelope ramp (5ms key-up click suppressor) |
| **bp0/1/2** | TX bandpass filters at 3 chain positions |
| **ALC** | Automatic Level Control (always-on TXA stage) |
| **EER** | Envelope Elimination & Restoration (E-class PA modulation) |
| **APF** | Audio Peak Filter (RX-side narrowband CW filter) |
| **CTCSS** | Continuous Tone-Coded Squelch System (FM sub-audible PL tone) |
| **VAX** | NereusSDR-native virtual audio bridge (NOT a Thetis VAC port) |
| **VAC1/VAC2** | Thetis virtual audio cable feature for digital modes |
| **cmASIO** | Thetis-bundled native ASIO host (Steinberg SDK 2.3.3) replacing radio codec |
| **PSForm** | PureSignal control form |
| **AmpView** | PureSignal correction-curve visualization dialog |

## 17. Open questions

None blocking. Items that may surface during pre-code reviews:
- Per-band TUNE-pulse defaults (3M-6) — verify Thetis source has them
- AndromedaIndicator subscription API surface (3M-6) — could be richer
- `BypassVACWhenPlayingWAV` UX (3M-3c) — modeless vs menu-action

Pre-code reviews per phase will resolve these as they come up; none affect
the architecture or phase split locked in this design.

## 18. References

**Thetis source baseline:** `v2.10.3.13-7-g501e3f51` (commit `501e3f51`)

Key Thetis files referenced across phases:
- `Project Files/Source/Console/console.cs` — MOX, TUNE, meters,
  persistence
- `Project Files/Source/Console/setup.cs` — OC, ATT-on-TX, TXA stage
  toggles
- `Project Files/Source/Console/audio.cs` — VAC, MON, anti-VOX, MOX hooks
- `Project Files/Source/Console/cmaster.cs` — EER, audio mixer, channel
  routing
- `Project Files/Source/Console/dsp.cs` — 146 TXA P/Invokes
- `Project Files/Source/Console/PSForm.cs` — PureSignal state machine
- `Project Files/Source/Console/AmpView.cs` — PS time-domain UI
- `Project Files/Source/Console/HPSDR/Alex.cs` — TX antenna composer
- `Project Files/Source/Console/HPSDR/NetworkIO.cs` — CW NetworkIO setters
- `Project Files/Source/Console/cwx.cs` — CWX canned sender
- `Project Files/Source/Console/CW/CWInput.cs` — paddle USB driver
- `Project Files/Source/wdsp/TXA.c` — 31-stage chain construction
- `Project Files/Source/wdsp/calcc.c` / `iqc.c` — PureSignal core
- `Project Files/Source/wdsp/eq.c` / `cfcomp.c` / `emph.c` / `amsq.h` — TXA
  stage setters

**NereusSDR design refs (related):**
- `docs/architecture/overview.md` — layer + thread architecture
- `docs/architecture/wdsp-integration.md` — RxChannel/TxChannel patterns
- `docs/architecture/radio-abstraction.md` — P1/P2 connections
- `docs/architecture/2026-04-19-vax-design.md` — VAX (used by 3M-1b
  PC-mic path and 3M-3c digital-mode routing)

**NereusSDR feedback / behavior memories that govern this epic:**
- `feedback_no_cites_in_user_strings` — tooltip rule
- `feedback_thetis_userland_parity` — match Thetis Setup IA / density
- `feedback_source_first_ui_vs_dsp` — DSP source-first; UI plumbing native
- `feedback_inline_cite_versioning` — `[vX.Y.Z.W]` / `[@sha]` cite stamps
- `feedback_vax_not_vac_port` — VAX is native, not a VAC port
- `feedback_design_docs_in_architecture` — design docs in
  `docs/architecture/`, topic-only names
- `feedback_options_with_recommendation` — interview-style brainstorm
  pattern that produced this design

---

## End of design
