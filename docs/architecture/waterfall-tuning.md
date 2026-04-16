# Waterfall Tuning Rationale

> **Status:** Active. Last revised 2026-04-15 for Phase 3G-9b PR2 (post live-tuning).

This doc explains *why* the NereusSDR smooth-default profile looks the way it does. Every value in `RadioModel::applyClaritySmoothDefaults()` has a one-line "because X" that comes from this file.

If you're tempted to change a default value in `RadioModel::applyClaritySmoothDefaults()`, read the corresponding section here first. If your change invalidates the rationale, update this doc at the same time.

## Framing

NereusSDR's DSP stack is **Thetis-lineage**: averaging modes, AGC algorithms, threshold semantics, WDSP channel structure — all come from Thetis/WDSP. The smooth-defaults profile uses Thetis's own controls set to Thetis's own defaults where applicable, and tunes NereusSDR-native controls (palette gradient stops, trace colour, update period) with the AetherSDR waterfall as a **visual reference image**.

**We are not porting AetherSDR parameters.** AetherSDR is a FlexRadio SmartSDR client; its DSP stack is entirely different and its internal parameters don't map to our knobs. We are picking NereusSDR/Thetis values to produce an outcome that visually resembles a reference screenshot we like. This is a genuine design divergence from the standing source-first protocol (CLAUDE.md), authorised for Phase 3G-9b as a one-off.

## Context

NereusSDR's display surface was feature-complete after Phase 3G-8 (47 controls wired) but the **default** look — what a fresh install presents to a user who has never touched Setup → Display — was the same high-contrast "Enhanced" rainbow palette that shipped in 3G-4 for meter system development. Side-by-side comparison against AetherSDR v0.8.12 showed a significant readability gap:

- AetherSDR renders noise as uniform near-black and strong signals as a full rainbow gradient (blue → cyan → green → yellow → orange → red → magenta)
- NereusSDR painted the entire noise floor in saturated reds/greens/cyans, burying signals in visual clutter

This doc is the per-knob rationale for the profile that fixes that.

## Visual comparison

**AetherSDR reference (80m SSB voice, 2026-04-14):**

![AetherSDR reference 80m](waterfall-tuning/aethersdr-reference-80m.png)

*The target look. Noise floor renders as uniform near-black. SSB voice signals show the full rainbow gradient — blue outer edges, cyan, green, yellow, red/magenta at voice formant peaks. High signal readability against a quiet background.*

**AetherSDR reference (40m zoomed, 2026-04-15):**

![AetherSDR reference 40m zoomed](waterfall-tuning/aethersdr-reference-zoomed.png)

*Zoomed view showing the full rainbow per-signal progression. Each strong signal has dark edges → blue → cyan → green → yellow → red/magenta at the core. This is the rainbow palette philosophy in action.*

**NereusSDR before (iter0, pre-tuning PR2):**

![NereusSDR iter0](waterfall-tuning/before-nereussdr-pr2.png)

*The initial PR2 palette attempt: narrow-band navy with near-white peaks. Too blue, trace was saturated cyan, fill-under-trace visible. Functionally wrong premise — AetherSDR isn't narrow-band blue-only.*

**NereusSDR after (iter6, post-tuning):**

![NereusSDR iter6](waterfall-tuning/after-clarity-blue.png)

*After live tuning against the reference. Noise floor near-black, white trace thin and not competing with palette, carriers showing visible colour variety thanks to the widened 12 dB AGC margin. Narrow carriers on 40m can't exercise the full palette middle (only SSB voice content does), but the philosophy now matches the reference.*

## Key insight (from live iterative tuning)

The AetherSDR "look" is **not** a narrow-band blue-only palette. It's a **full-spectrum rainbow** that appears mostly blue because AGC + tight thresholds keep normal signals in the cool region. Strong signals still progress cleanly through yellow and red. An earlier revision of this doc got that wrong — the current recipe table reflects the corrected philosophy after iter3–iter6 of live tuning against your radio.

## The seven recipes

### 1. Waterfall palette: `WfColorScheme::ClarityBlue`

**Value:** new 10-stop full-spectrum gradient with a deep-black floor:

| pos | r | g | b | meaning |
|---|---|---|---|---|
| 0.00 | 0x00 | 0x00 | 0x00 | pure black — noise floor bottom |
| 0.18 | 0x02 | 0x08 | 0x20 | very dark blue — noise floor top |
| 0.32 | 0x08 | 0x20 | 0x58 | dark blue — weak signal edge |
| 0.46 | 0x10 | 0x50 | 0xb0 | medium blue — weak signals |
| 0.58 | 0x10 | 0xa0 | 0xe0 | cyan — medium signals |
| 0.70 | 0x10 | 0xd0 | 0x60 | green — strong signals |
| 0.80 | 0xf0 | 0xe0 | 0x10 | yellow — very strong |
| 0.90 | 0xff | 0x80 | 0x00 | orange — extreme |
| 0.96 | 0xff | 0x20 | 0x20 | red — peak |
| 1.00 | 0xff | 0x40 | 0xc0 | magenta — absolute peak |

**Why:** The palette is full-spectrum rainbow, not narrow-band. The "blue look" users see most of the time comes from the combination of AGC + tight thresholds compressing most signals into the blue/cyan range; strong signals still cleanly progress through green/yellow/red so peak energy remains distinguishable. The deep-black floor (`{0.00, 0x00, 0x00, 0x00}`) is what keeps the noise floor visually quiet — compare to Default/Enhanced which start at dark blue and spread bright colours across the entire range, producing a noisy noise floor.

**Source:** **NereusSDR native**, visual target: 2026-04-14 and 2026-04-15 AetherSDR reference screenshots. No Thetis equivalent.

### 2. Spectrum averaging mode: `AverageMode::Logarithmic`

**Value:** `sw->setAverageMode(AverageMode::Logarithmic)`

**Why:** Thetis's "Log Recursive" averaging mode smooths the dB-domain spectrum trace rather than the linear-power trace. On SSB voice the result is gentle voice-envelope hills instead of instantaneous grass. NereusSDR's previous default was `Weighted` (plain exponential) which produced visible trace jitter under noise. Logarithmic matches the dB axis the user is reading.

**Source:** **Thetis** — `console.specRX.GetSpecRX(0).AverageMode`, Log Recursive mode. C# P/Invoke declaration in `dsp.cs`, smoothing path in `display.cs`.

### 3. Averaging time constant: `0.05f` alpha

**Value:** `sw->setAverageAlpha(0.05f)`

**Why:** The alpha value is the weight given to each new FFT frame in the exponential smoothing `smoothed = alpha * new + (1-alpha) * previous`. At `0.05` each new frame contributes only 5% — the smoothing window is approximately `1/alpha = 20 frames`. At the default 30 FPS that's about 667 ms of settling time, which reads as "~500 ms smooth" to the eye once you factor in frame-to-frame correlation. Heavier values (0.01) feel laggy on tuning; lighter (0.20) still show grass.

**UI note:** the Setup → Display → Spectrum Defaults "Averaging Time" spinbox converts ms → alpha via `qBound(0.05f, 1.0f - ms/5000.0f, 0.95f)`. That formula is for the manual knob; the smooth-defaults profile bypasses it and sets 0.05 directly.

**Source:** **NereusSDR empirical** — chosen to produce a smooth trace against the 30 FPS output. Visual target: reference screenshots.

### 4. Trace colour: pure white `#ffffff` alpha 230

**Value:** `sw->setFillColor(QColor(0xff, 0xff, 0xff, 230))`

**Why:** A thin white trace sits cleanly in front of the waterfall without competing with the palette's own highlight colours. Alpha 230 (out of 255) keeps the trace visible without fully occluding the waterfall behind it. The previous default (`#00e5ff` saturated cyan) competed with the ClarityBlue palette's own cyan/green highlights, making it hard to tell where the trace ended and the signal colour began. An earlier attempt at `#e0e8f0` neutral gray was too dim against a dark waterfall.

**Source:** **NereusSDR native**, visual target: reference screenshots.

### 5. Pan fill disabled: `setPanFillEnabled(false)`

**Value:** `sw->setPanFillEnabled(false)`

**Why:** The trace renders as a thin line rather than a filled-under-curve area. Fill-on produces a solid block of colour underneath the trace line, which competes visually with the waterfall below and adds no information. AetherSDR and the reference screenshots show just a line. NereusSDR's inherited default is fill-on; the profile turns it off.

**Source:** **NereusSDR native**, visual target: reference screenshots.

### 6. Waterfall AGC: `true` (with widened 12 dB margin)

**Value:** `sw->setWfAgcEnabled(true)` + **SpectrumWidget AGC margin constant: 3 dB → 12 dB**

**Why:** AGC tracks band conditions automatically. Without it, users must manually retune thresholds when changing bands. With it on, the display is optimised across all bands with zero user intervention.

**AGC margin widening (SpectrumWidget.cpp):** the AGC follower in `pushWaterfallRow` computes `m_wfLowThreshold = m_wfAgcRunMin - margin; m_wfHighThreshold = m_wfAgcRunMax + margin`. The previous margin of 3 dB was too tight — narrow carriers pinned the running max at the very top of the palette with no breathing room for FFT skirt falloff, making signals visually binary (dark background, peak colour, nothing between). Widening to 12 dB gives the palette enough amplitude range to render `cyan → green → yellow → red` across a signal's shape from peak to noise. This affects **all** palettes when AGC is on, not just Clarity Blue — it's a pure quality improvement across the board.

**Note on dropped threshold recipe:** an earlier draft of this profile also set explicit Low/High threshold values (-110/-70 dBm). That recipe was dropped after discovering that AGC overwrites both thresholds every frame — setting them in the profile was redundant noise. With AGC on, the initial threshold values are replaced within milliseconds of the first FFT frame.

**Source:** **Thetis** (`setup.designer.cs:34069 chkRX1WaterfallAGC` default enabled). Margin value is **NereusSDR empirical** — 12 dB picked after observing 3 dB produced visually-binary waterfall on carrier-heavy content.

### 7. Waterfall update period: 30 ms

**Value:** `sw->setWfUpdatePeriodMs(30)`

**Why:** 30 ms between waterfall row pushes produces ~33 rows/second, which reads as smooth scroll motion rather than discrete steps. The previous NereusSDR default of 50 ms (20 rows/sec) was visibly steppy on long fade-ins. Going below 30 ms starts wasting GPU bandwidth without perceptible improvement — the eye can't distinguish 40 rows/sec from 33 on a typical monitor. Thetis's 2 ms default is unnecessarily aggressive.

**Source:** **NereusSDR empirical** — Thetis's 2 ms is wasteful; we picked 30 for smooth scroll with reasonable GPU cost.

## Why no first-launch auto-apply

An earlier design revision had the profile apply automatically on fresh install via a `DisplayProfileApplied` AppSettings gate. **That has been removed per user decision 2026-04-15**: the user wants the out-of-box default to remain the existing NereusSDR default (`WfColorScheme::Default`), with Clarity Blue reachable only via:

1. Manual selection in `Setup → Display → Waterfall Defaults → Color Scheme` (one of 8 palette options)
2. One-click "Reset to Smooth Defaults" button at the top of `Setup → Display → Spectrum Defaults`

The Reset button remains the opt-in entry point for the smooth-defaults profile.

> **Note:** as of the initial PR2 commit, the first-launch gate code is still present in `RadioModel::RadioModel()` and `MainWindow`. A follow-up commit before PR2 opens removes those call sites, keeping the gate key name available for PR3 (Clarity) to repurpose.

## Reset button

`Setup → Display → Spectrum Defaults` has a "Reset to Smooth Defaults" button at the top of the page. Guarded by a confirmation dialog because it overwrites the user's Spectrum / Waterfall settings. It does NOT touch FFT size, frequency, band stack, or per-band grid slots — those are navigation/identity state, not display preferences.

## Known limitations of live-tuning against carrier-heavy bands

Tuning the palette against a band dominated by narrow carriers (CW, digital, unmodulated SSB carriers) is harder than tuning against SSB voice content. Narrow signals have almost no amplitude variation across their bandwidth — a few FFT bins hit near-peak, edges drop sharply to noise floor, nothing in between. That means the middle of the palette (cyan/green/yellow) doesn't get exercised visually even when the stops are correctly positioned.

**The palette is correctly tuned when:**
- Noise floor renders as uniform dark/near-black
- Strong SSB voices show a full rainbow gradient from blue edges through cyan/green/yellow to red/magenta at the voice formant peaks
- Weak distant stations appear as faint blue/cyan without being lost in the noise floor

**Recommended test content:** 80m SSB at night, 40m SSB evenings, 20m SSB daytime.

## Clarity adaptive tuning (PR3 — Phase 3G-9c, resolved)

PR3 shipped `ClarityController` as an **add-on** to the existing Waterfall AGC, not a replacement. Both coexist:

- **Waterfall AGC on, Clarity off** — legacy running-min/max one-pole follower with 12 dB margin (PR2 behavior, unchanged).
- **Clarity on** — percentile-based noise-floor estimator (30th percentile, EWMA τ=3s, ±2 dB deadband) drives Waterfall Low/High thresholds. Legacy AGC yields via `m_clarityActive` flag in `pushWaterfallRow()`.
- **Clarity paused** (TX, manual slider drag) — legacy AGC resumes if its checkbox is on.
- **Both off** — thresholds are purely user-set (manual mode).

Resolved open questions from the PR2 draft:

1. **Does Clarity supersede Waterfall AGC?** No — they coexist. Clarity takes priority when active; AGC runs when Clarity is off or paused. User chooses their preferred mode.
2. **Should PR2 static defaults be replaced by adaptive initial values?** No — the smooth-defaults profile stays as the fallback when Clarity is off. Clarity sits on top.
3. **AGC margin (12 dB) user-configurable?** Not in PR3 — the margin stays at 12 dB for the legacy AGC path. Clarity uses its own threshold margins (-5 / +55 dB around the smoothed floor, 60 dB total range) with a 30 dB minimum-gap clamp for empty bands.

See `docs/architecture/2026-04-15-display-refactor-design.md` §6 for the full PR3 design.

## Related follow-ups flagged during PR2 tuning

- **Phase 3G-11: zoom level persistence.** Spectrum zoom resets across app restarts, which made iterative visual comparison inconsistent. Captured as a Phase 3G-11 polish item — not fixed in PR2.
