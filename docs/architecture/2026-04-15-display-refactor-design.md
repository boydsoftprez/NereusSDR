# Display Refactor — Design Spec

> **Status:** Draft for user review. Awaiting sign-off before implementation plans.
> **Drafted:** 2026-04-15
> **Predecessor:** Phase 3G-8 RX1 Display Parity (merged 2026-04-12, PR #8)
> **Successor:** PR3 (Clarity) feeds into Phase 3F multi-panadapter grid plumbing.

## 1. Goal

Take the existing Phase 3G-8 display wiring from "every control works" to "every control is **sourced, well-labeled, well-presented, and the default display looks great without the user touching anything**." The final piece adds an adaptive auto-tune feature called **Clarity** that keeps the waterfall dialed in as band conditions and tuning change.

The motivating observation: a side-by-side comparison between NereusSDR v0.1.4 on 80m and AetherSDR v0.8.12 on the same band shows a massive readability gap. AetherSDR's waterfall renders noise as uniform dark navy and signals as bright cyan/white; NereusSDR's "Enhanced" rainbow palette paints the entire noise floor in saturated colors, burying signals in visual clutter. The cause decomposes into ~6 specific knob choices, not one mystery setting — see §5 for the recipe.

## 2. Scope

**In scope** — the three Setup pages shipped in Phase 3G-8:

- `SpectrumDefaultsPage` (17 controls)
- `WaterfallDefaultsPage` (17 controls)
- `GridScalesPage` (13 controls)

47 controls total on the RX1 Display surface.

**Out of scope:**

- RX2 Display page (defers with RX2 receiver support)
- TX Display page (defers to Phase 3I-1 / 3I-3 TX work)
- Spectrum Overlay panel flyouts (runtime quick toggles, separate work)
- Meter editors, skin system, multi-panadapter display routing

**Scope boundary is locked.** If additional surface gets pulled in, that's a revision to this doc, not a drift inside implementation.

## 3. Sequence — three PRs

| PR | Subject | Risk | Gate |
|---|---|---|---|
| **PR1** | Source-first audit + tooltip port + slider readouts | Low — mechanical | Smoke + manual widget sweep |
| **PR2** | Smooth defaults + Clarity Blue palette + tuning doc | Medium — opinionated numbers | Live-radio sanity check before merge |
| **PR3** | **Clarity** adaptive display tuning | High — algorithmic | **Research doc gate before any code** |

Strict sequential dependency: PR1's helpers are used by PR2, PR2's static defaults are the fallback for PR3's adaptive mode.

### Common conventions

- All three PRs branch off `main`.
- All commits GPG-signed (no `--no-gpg-sign`, no `--no-verify`).
- Pause for review at each commit boundary per standing preference.
- Re-read `CLAUDE.md` and `CONTRIBUTING.md` at the start of each branch.
- Per-control Thetis citation in code comments: `// Thetis setup.cs:NNNN (controlName)`.
- TDD via `superpowers:test-driven-development` for new logic (Clarity's estimator especially).

## 4. PR1 — Source-First Audit + Tooltips + Slider Readouts

**Branch:** `feature/display-audit-pr1`

### 4.1 Deliverables

#### 4.1.1 Source-first audit

For every control in `DisplaySetupPages.cpp`, walk back to Thetis source and confirm the citation in the 3G-8 plan tables (§3.1, §4.1, §5.1) is still accurate against `../Thetis/`. Add a citation comment immediately above each `connect(...)` block:

```cpp
// Thetis: setup.designer.cs:33834 (udDisplayFPS) → console.DisplayFPS
connect(m_fpSlider, &QSlider::valueChanged, this, ...);
```

NereusSDR-only extensions get:

```cpp
// NereusSDR extension — no Thetis equivalent (peak hold on spectrum)
```

Goal: any future reader can jump from our code to the upstream in one click, and the source-first protocol is self-documenting in the code itself.

#### 4.1.2 Tooltip port

For every control with a Thetis equivalent, extract the matching `toolTip1.SetToolTip(controlName, "...")` string from `setup.designer.cs` (1,191 such calls exist in the Thetis tree). Decision rule:

- **Thetis tooltip is substantive** (>10 chars, says something useful): port verbatim. Comment: `// Tooltip from Thetis setup.designer.cs:NNNN`.
- **Thetis tooltip is empty, missing, or echoes the label**: write a new one. Comment: `// Tooltip rewritten — Thetis original: "<original or empty>"`.
- **NereusSDR-only controls**: write from scratch, no attribution needed.

Every control gets a tooltip. Current count: 10/47. Target: 47/47.

#### 4.1.3 Slider readout refactor

Add helpers to new `src/gui/setup/SetupHelpers.h`:

```cpp
struct SliderRow {
    QSlider*  slider;
    QSpinBox* spin;
    QWidget*  container;  // HBox with both, ready to drop into a QFormLayout
};

SliderRow makeSliderRow(int min, int max, int initial,
                        const QString& suffix = {},
                        QWidget* parent = nullptr);

struct SliderRowD {
    QSlider*        slider;
    QDoubleSpinBox* spin;
    QWidget*        container;
};

SliderRowD makeDoubleSliderRow(double min, double max, double initial,
                               int decimals, const QString& suffix,
                               QWidget* parent = nullptr);
```

Each helper returns a slider+spinbox pair already wired bidirectionally with `QSignalBlocker` so feedback loops are impossible. Refactor every existing `QSlider` in `DisplaySetupPages.cpp` to use the helper. Estimated ~12 sliders to convert.

The spinbox carries the unit suffix (` ms`, ` dBm`, ` Hz`, etc.). Keyboard users get a real entry path (typing `4096` beats nudging a slider 12 times). Drag the slider **or** type into the spinbox — they stay in sync.

### 4.2 Commit shape

1. `feat(setup): SliderRow/SliderRowD helpers in SetupHelpers.h`
2. `refactor(setup): SpectrumDefaultsPage uses SliderRow helpers`
3. `refactor(setup): WaterfallDefaultsPage uses SliderRow helpers`
4. `refactor(setup): GridScalesPage uses SliderRow helpers`
5. `feat(setup): port Thetis tooltips + add source-first citations (47 controls)`

### 4.3 Verification

- Build green, app launches, all three pages render with no missing widgets.
- Manual smoke: drag every slider, confirm spinbox tracks. Type into every spinbox, confirm slider tracks. Hover every control, confirm tooltip appears and reads sensibly.
- `tst_smoke` + existing tests stay green.
- **No** 47-screenshot matrix this time — the 3G-8 matrix already verified behavior, and this PR doesn't change behavior, only labels and plumbing.

### 4.4 Files touched

- `src/gui/setup/DisplaySetupPages.h` — minor (tooltip-related members if any)
- `src/gui/setup/DisplaySetupPages.cpp` — tooltips, citations, slider refactor
- `src/gui/setup/SetupHelpers.h` (new)
- `src/gui/setup/SetupHelpers.cpp` (new)
- `CMakeLists.txt` — register `SetupHelpers.cpp`

Two files modified, two new. No model or renderer changes. Estimated ~600 LOC delta (mostly tooltip block + helper file; slider refactor is net-reductive in the page file).

## 5. PR2 — Smooth Defaults + Clarity Blue Palette + Tuning Doc

**Branch:** `feature/display-smooth-defaults-pr2` (after PR1 merges)

### 5.1 The seven recipes

Side-by-side observation of NereusSDR vs AetherSDR on 80m (2026-04-14 reference screenshots) decomposed "butter-smooth waterfall" into seven knob choices:

| # | Knob | NereusSDR today | Smooth target | Source of target |
|---|---|---|---|---|
| 1 | Waterfall palette | Enhanced rainbow | **Clarity Blue** (new, narrow-band monochrome) | AetherSDR observation |
| 2 | Spectrum averaging mode | None | **Log Recursive** | Thetis default `display.cs` |
| 3 | Averaging time constant | 100 ms | **500 ms** | AetherSDR observation |
| 4 | Trace color | Saturated cyan `#00e5ff` | Neutral light-gray `#e0e8f0`, alpha 200 | AetherSDR observation |
| 5 | Waterfall Low/High gap | -130 / -40 | **-110 / -70** | centered on real 80m noise floor |
| 6 | Waterfall AGC (W3) | off (+ stubbed?) | **on** | Thetis default `chkRX1WaterfallAGC` |
| 7 | Waterfall update period | 50 ms | 30 ms | AetherSDR observation |

Knobs 1, 2, and 6 do ~80% of the visible work. The rest are polish. All seven values are the *defaults* — users can still tune freely.

### 5.2 Deliverables

#### 5.2.1 Clarity Blue palette

Add to `WfColorScheme` enum in `src/gui/SpectrumWidget.h`. Gradient stops in `wfSchemeStops()`:

- 0–60% of range → dark navy `#0a1428` → `#0d2540` (noise floor)
- 60–85% → `#1850a0` → `#3090e0` (weak signals)
- 85–100% → `#80d0ff` → `#ffffff` (strong signals)

80% of the dynamic range eats the noise, signals jump out as bright cyan-white. Philosophy: narrow-band mapping, not full-range mapping.

#### 5.2.2 Smooth defaults profile

New function on `PanadapterModel`:

```cpp
void applyClaritySmoothDefaults();  // sets the 7 recipe values above
```

Persists through AppSettings normally. Called once on first launch gated by a new `"DisplayProfileApplied"` AppSettings key (default `"False"` → applies → sets `"True"`). Existing users with custom values are **untouched**. Only fresh installs and explicit reset get the profile.

#### 5.2.3 Reset button

One button at the top of `SpectrumDefaultsPage` labeled **"Reset to Smooth Defaults"**. Calls `applyClaritySmoothDefaults()` + reloads all three pages from model. Confirmation dialog (`"This will overwrite your current display settings. Continue?"`) because it's destructive.

#### 5.2.4 Wire stub renderer hooks

Two known soft spots from the 3G-8 wiring pass:

- **Waterfall AGC (W3)**: verify the checkbox actually drives a renderer auto-level, not just a stored bool. If stubbed, implement it — `pushWaterfallRow()` already has the dynamic-range data it needs.
- **Waterfall update period (W4)**: confirm the slider actually changes texture write cadence, not just a stored value.

Whichever ones are stubs, PR2 fixes them. Both are prerequisites: PR2's smooth defaults need them working, and PR3's Clarity controller adapts them.

#### 5.2.5 Tuning rationale doc

New `docs/architecture/waterfall-tuning.md`. Covers:

- The seven recipes table with *why* for each value.
- Side-by-side reference screenshots (NereusSDR before/after, AetherSDR comparison).
- Color palette philosophy (narrow-band vs full-range mapping).
- Threshold gap math (how -110/-70 was chosen for the default).
- Pointers to Thetis source for each parameter (`display.cs:NNNN`).
- Open questions for PR3 (Clarity) — explicitly flags this as the static floor, with adaptive built on top.

### 5.3 Commit shape

1. `feat(spectrum): Clarity Blue waterfall palette`
2. `fix(spectrum): wire Waterfall AGC + update period through to renderer` *(may be one or two commits depending on stub count)*
3. `feat(model): applyClaritySmoothDefaults profile + first-launch gate`
4. `feat(setup): Reset to Smooth Defaults button on Spectrum Defaults page`
5. `docs(architecture): waterfall-tuning.md rationale + screenshots`

### 5.4 Verification

- Build, blow away `~/.config/NereusSDR/NereusSDR.settings`, launch — confirm smooth defaults applied automatically and the waterfall visibly matches the AetherSDR reference.
- Capture fresh NereusSDR-with-smooth-defaults screenshot, place next to AetherSDR reference in `docs/architecture/waterfall-tuning/`. If they're not visibly close, iterate the gradient stops before merging.
- Existing AppSettings preserved on upgrade: launch with old settings file present, confirm values untouched.
- "Reset to Smooth Defaults" button works, persists, survives relaunch.
- `tst_smoke` + existing tests stay green.

### 5.5 Files touched

- `src/gui/SpectrumWidget.h/.cpp` — `WfColorScheme::ClarityBlue`, `wfSchemeStops()` case, potentially AGC/update-period hook fixes
- `src/models/PanadapterModel.h/.cpp` — `applyClaritySmoothDefaults()`, first-launch gate
- `src/gui/setup/DisplaySetupPages.cpp` — Reset button
- `docs/architecture/waterfall-tuning.md` (new)
- `docs/architecture/waterfall-tuning/` (new, screenshots)
- `CHANGELOG.md` — entry

Estimated ~400 LOC code + ~250 lines doc + screenshots.

### 5.6 Risk and mitigation

PR2 is opinionated. Real chance the gradient stops or threshold values don't hit "smooth" on the first try and need iteration against the live radio. Plan accommodates:

- Commit 1 lands the palette alone → visual check.
- Commits 3–4 land the defaults → end-to-end check.
- Commit 5 (doc) is last, after the look is confirmed.
- If any value needs retuning mid-PR, amend the appropriate commit rather than stacking fix-up commits.

## 6. PR3 — Clarity (Adaptive Display Tuning)

**Branch:** `feature/display-clarity-pr3` (after PR2 merges)

### 6.1 The feature in one sentence

Clarity is a continuous adaptive algorithm that keeps the waterfall thresholds centered on the actual noise floor as band conditions, tuning, and antennas change, with a one-shot "Re-tune now" button for forcing it after a big change and a clean off switch when the user wants to lock the display.

Flavor: **continuous auto + one-shot re-tune button** (see design discussion Q5). Continuous is the default, one-shot is the escape hatch.

### 6.2 Phase A — Research doc (gate)

No code lands until the doc is written and user signs off on the algorithm. Reason: Clarity is the only piece of this effort with real failure modes — a noise-floor estimator that goes nuts on a strong carrier, hysteresis that pumps, an "off switch" that confuses users. Better to burn a session on the doc than ship something that needs ripping out.

File: `docs/architecture/clarity-design.md`. Contents:

**6.2.1 Prior art survey.** Read and summarize:

- Thetis `WaterfallAGC` — search `display.cs` for `WaterfallAGC`, `UpdateNoiseFloor*`, `AvgRX1Power*`. What does it actually do? Cadence? Estimator?
- Thetis `display.cs` spectrum averaging path — `AverageMode` enum + the smoothing math.
- WDSP `nob.c` / `anf.c` — any noise-floor tracking we can borrow conceptually.
- AetherSDR — if source is browsable, document the algorithm; if not, note and reverse-engineer from observed behavior.
- GQRX / SDR++ auto-range implementations — both open source, both take different approaches. Document each in 3–5 lines.

**6.2.2 Algorithm spec.** Locked choices for each:

- **Noise floor estimator.** Recommended: percentile-based (30th percentile of FFT bins after dB conversion). Robust against strong carriers, cheap to compute. Research doc either confirms or picks differently with reasoning.
- **Update cadence.** Recommended: 2 Hz polling on existing FFT output, EWMA smoothing with τ ≈ 3s, deadband ±2 dB before any threshold change.
- **What gets adapted.** Recommended v1: Waterfall Low and High thresholds only. Spectrum averaging τ stays user-controlled. Adapting averaging is a v2 feature.
- **Manual override behavior.** Recommended: dragging a threshold slider while Clarity is on triggers a "Clarity paused — click to resume" indicator on the slider. User resumes or clicks "Re-tune now" to restart adaptation.
- **Per-band memory.** Recommended: yes. Store last-known good `(low, high)` pair per band in `BandGridSettings`. Band-switch is instant and Clarity refines from there.
- **Off switch.** Master toggle in Setup → Display → Spectrum Defaults. Status indicator in Spectrum Overlay panel (small "C" badge).

**6.2.3 Test plan.** TDD unit tests for the estimator:

- Input: synthetic FFT bin arrays
- Scenarios: flat noise, noise + one carrier, noise + multiple SSB envelopes, empty band, all-bins-zero edge case
- Assertion: estimator returns the expected floor within 2 dB

The estimator is pure math on a `std::vector<float>` — perfect TDD target. Runs in `tst_noise_floor_estimator.cpp`.

**6.2.4 Failure modes catalog.** Explicit list with mitigations:

| Failure mode | Mitigation |
|---|---|
| Strong local carrier (broadcast spillover, neighbor's ham) | Percentile estimator handles it — carrier sits above 30th percentile |
| Empty band (50 MHz at noon, no signals) | Floor goes very low, thresholds collapse → clamp minimum gap to 30 dB |
| Actively TX'ing | Pause Clarity during TX — we already have the MOX signal |
| Cold start, no FFT data yet | First 5 seconds use PR2 smooth defaults, then Clarity takes over |
| User is trying to hand-tune | Manual override detection → paused indicator, explicit resume |
| Band switch mid-adaptation | Per-band memory snaps to last-known good, Clarity refines from there |

**6.2.5 UI spec.**

- Master toggle location: `SpectrumDefaultsPage`, top of page (near Reset button from PR2)
- Status badge: small "C" on Spectrum Overlay panel, green when active, amber when paused
- Override indicator: next to any slider the user drags while Clarity is on
- "Re-tune now" button: on Spectrum Overlay panel (quick access)
- What happens to W3 "Waterfall AGC" checkbox: Clarity supersedes it. W3 becomes a deprecation label pointing at Clarity. Not removed — users with existing configs see the label and understand the migration.

### 6.3 Phase B — Implementation (after doc sign-off)

#### 6.3.1 Commit shape

1. `docs(architecture): clarity-design.md research + algorithm spec` ← **gate**
2. `feat(core): NoiseFloorEstimator + unit tests` ← TDD, pure math
3. `feat(core): ClarityController (cadence, hysteresis, deadband, override detection)` + tests
4. `feat(model): per-band Clarity memory in BandGridSettings`
5. `feat(setup): Clarity master toggle + override indicator in Spectrum Defaults`
6. `feat(spectrum): Re-tune now button + Clarity status badge in overlay panel`
7. `docs(architecture): waterfall-tuning.md updated with Clarity section + final screenshots`

#### 6.3.2 Files touched

- `src/core/NoiseFloorEstimator.h/.cpp` (new)
- `src/core/ClarityController.h/.cpp` (new)
- `src/gui/SpectrumWidget.h/.cpp` — hook Clarity into threshold update path
- `src/models/PanadapterModel.h/.cpp` — per-band Clarity memory in `BandGridSettings`
- `src/gui/setup/DisplaySetupPages.cpp` — master toggle, override indicator, W3 deprecation label
- `src/gui/SpectrumOverlayPanel.cpp` — status badge, Re-tune now button
- `tests/tst_noise_floor_estimator.cpp` (new)
- `tests/tst_clarity_controller.cpp` (new)
- `docs/architecture/clarity-design.md` (new)
- `docs/architecture/waterfall-tuning.md` — updated
- `CHANGELOG.md` — entry
- `CMakeLists.txt` — register new core/test files

### 6.4 Verification

- Unit tests for `NoiseFloorEstimator` and `ClarityController` green.
- Manual: launch on 80m at night, confirm Clarity tightens thresholds to the actual noise floor within ~5s.
- Switch to 6m midday (empty), confirm clamp prevents collapse.
- TX a brief carrier, confirm Clarity pauses and resumes cleanly.
- Drag a threshold slider, confirm "paused" indicator appears, confirm resume.
- Side-by-side: NereusSDR with Clarity on vs AetherSDR. Document closeness.
- `tst_smoke` + all existing tests stay green.

### 6.5 Risk and mitigation

Highest risk of the three PRs. Mitigations:

- **Doc-first gate** — algorithm choices reviewed in writing before code.
- **TDD on estimator** — pure-math boundary, easy to lock against regression.
- **Clean off switch** — worst case is "user disables it" and they get PR2 smooth defaults back.
- **PR2 fallback always present** — Clarity sits on top of static defaults, never replaces them.

Estimated size: ~250 LOC algorithm + ~400 LOC tests + ~300 LOC UI/wiring + research doc. Largest of the three PRs by volume, smallest by user-facing surface — most of it is internal machinery.

## 7. Non-goals

Explicit list of things this design spec does NOT cover:

- RX2 or TX display surface changes
- Spectrum Overlay flyout panel refactors (only the Clarity status badge is added in PR3)
- Skin system integration
- Thetis default value adoption beyond the seven recipes in §5.1 (source-first protocol per CLAUDE.md governs everything else; 3G-8's §10 exception is **not** extended)
- Adding or removing Setup pages
- Changing the RX1/RX2/TX page taxonomy

## 8. Open questions

None at draft time. If questions emerge during PR1 or PR2 implementation, they get answered inline in commit messages. Questions for PR3 are intentionally deferred to the `clarity-design.md` research doc — that's its explicit purpose.

## 9. Dependencies and prerequisites

- Phase 3G-8 (PR #8) — **merged, confirmed**
- Windows startup crash fixes (PR #23) — **merged, confirmed**
- `Thetis` source tree available at `../Thetis/` — **confirmed**
- ColorSwatchButton widget from 3G-8 — **present, reused**
- `PanadapterModel` per-band grid machinery from 3G-8 — **present, extended by PR3**

No new third-party dependencies.

## 10. How to begin (next session checklist)

1. Read this file end to end.
2. Re-read `CLAUDE.md` and `CONTRIBUTING.md` per standing preference.
3. `git checkout main && git pull`
4. `git checkout -b feature/display-audit-pr1`
5. Invoke `superpowers:writing-plans` to convert PR1's §4 into a step-by-step implementation plan.
6. Execute PR1 commits in §4.2 order. Pause for review at each.
7. Build + auto-launch + smoke test after each commit.
8. Ship PR1, wait for merge. Then repeat for PR2 (§5), then PR3 (§6).
9. PR3 **must** start with the research doc; no code until user signs off on `clarity-design.md`.
