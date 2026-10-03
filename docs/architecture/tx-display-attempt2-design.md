# TX Panadapter Attempt 2: Strict Thetis-Parity Design

**Date:** 2026-05-10
**Author:** J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code
**Status:** Design proposal awaiting review.
**Worktree:** `.claude/worktrees/tx-display` on branch `claude/tx-display`,
based on `4eef91e` (`Merge PR #226`) post-rebase. Probe commit `c2e62e6`
(throwaway gen0/gen1 state probe) sits on top.
**Thetis source pin:** `[v2.10.3.13+501e3f51]`
**Predecessor handoff:** [tx-display-brainstorm-handoff.md](tx-display-brainstorm-handoff.md)

---

## 1. Objective (plain English)

When the user keys up the radio, the panadapter and waterfall should display
the signal being transmitted, the way Thetis displays it: clean narrow
vertical lines for a 2-tone test (or one line for TUN), with a sharp cutoff
at the filter passband edges, no rainbow waterfall splattered across 180 kHz
of bandwidth. When the user un-keys, the display should return to RX1
spectrum without wiping the waterfall history.

Today, attempt 1 produces a -36 dBm DC pedestal smeared 24 kHz wide centered
on the carrier, drowning the actual tones in saturation. attempt 2 rebuilds
the analyzer side of attempt 1 with verbatim parity to Thetis's
`initAnalyzer` (the path Thetis uses for `PANAFALL`), instead of the
`CalcSpectrum` path that attempt 1 mistakenly copied. The fix is paired with
a diagnostic probe that lets a single bench cycle confirm whether the
parity correction kills the pedestal.

---

## 2. Evidence summary (what we already know)

### 2.1 Bench data from attempt 1 (frames 1-3 in the predecessor handoff)

ANAN-G2 Saturn, 14.241 MHz USB, 2-tone test PS-on, attempt 1 active:

| Probe bin | Frame 1 (TUN) | Frame 2 (2-Tone) | Frame 3 (2-Tone) |
|---|---|---|---|
| bin 0 (edge) | -69.5 dBm | -75.5 dBm | -71.1 dBm |
| bin n/4 | -66.6 | -72.8 | -68.5 |
| bin n/2 (DC) | **-36.1** | **-33.9** | **-32.3** |
| bin 3n/4 | -66.2 | -72.1 | -67.7 |
| bin n-1 (edge) | -69.5 | -75.5 | -71.1 |

Pattern: smooth pedestal centered on DC, ~24 kHz half-width, peaking ~30 dB
below the actual tone bins.

Thetis on the same hardware (W1AEX MXL-770-UMC reference at v2.10.3.9, 40m
2-tone, IMD3 = -62.53 dBc) shows clean narrow tones with no DC pedestal.

### 2.2 What is ruled out

**Hypothesis (i), gen0 vs gen1 confusion: KILLED.** Probe `c2e62e6` shows
`gen0.run=0` always; `gen1.run=1` with `mode=0` for TUN and `mode=1` for
2-Tone, with verbatim Thetis tt.f1/f2/mag1/mag2 values
(`setup.cs:11096-11107 [v2.10.3.13+501e3f51]`).

**Hypothesis (ii), WDSP runtime state diverges from setters: KILLED.**
Same probe confirms the runtime values match what our setters requested.

**Hypothesis (iii), buffer aliasing: KILLED.**
[TXA.c:361-400 [v2.10.3.13+501e3f51]](../../third_party/wdsp/src/TXA.c#L361)
shows `gen1.out`, `uslew.in`, `uslew.out`, and `sip1.in` are all the same
pointer (`txa[ch].midbuff`). gen1 mode-0 and mode-1 OVERWRITE `out` per
[gen.c:217-269 [v2.10.3.13+501e3f51]](../../third_party/wdsp/src/gen.c#L217)
(`=`, not `+=`).

**Hypothesis (iv), pollution between gen1 and Spectrum0: KILLED.**
- `xuslew` short-circuits in steady state (`a->out == a->in`,
  [slew.c:154 [v2.10.3.13+501e3f51]](../../third_party/wdsp/src/slew.c#L154));
  `ON` state is straight passthrough.
- `xiqc` runs at TXA.c:587, after `xsiphon` at :586. Cannot retroactively
  pollute the siphon's view.
- PSCC pump does not touch `txa[]` or `midbuff` (grep across
  `PsccPump.cpp` + `PureSignal.cpp` returned empty).
- `Spectrum0`
  ([analyzer.c:1726-1769 [v2.10.3.13+501e3f51]](../../third_party/wdsp/src/analyzer.c#L1726))
  is a straight memcpy with cosmetic I/Q axis swap. No DC injection.
- `Cspectra`
  ([analyzer.c:859-863 [v2.10.3.13+501e3f51]](../../third_party/wdsp/src/analyzer.c#L859))
  applies `a->window[i]` per sample BEFORE FFT. Window is correctly applied.
- `dsp_size = 4096`
  ([WdspEngine.cpp:642](../../src/core/WdspEngine.cpp#L642)) matches
  attempt 1's `bf_sz = 4096`. No mismatch.

### 2.3 What is left

The bug is in (v) attempt 1's analyzer configuration or (vi) attempt 1's
bin-to-pixel rendering. Both live in the stashed attempt 1 code
(`stash@{0}`, viewable via `git stash show -p stash@{0}`).

---

## 3. Smoking-gun candidate

attempt 1's `TxAnalyzer.cpp::applySetAnalyzer()` documents its source as:

> Parameter derivation from Thetis specHPSDR.cs:738-806 CalcSpectrum
> [v2.10.3.13], adapted for the TX siphon source.

`CalcSpectrum` is the SPECTRUM/HISTOGRAM/SPECTRASCOPE path. Per
`console.cs:8015-8020 + :8098-8108 [v2.10.3.13+501e3f51]`, the only caller
of `CalcSpectrum` (`UpdateTXDisplayVars`) early-returns unless the display
mode is `SPECTRUM`, `HISTOGRAM`, or `SPECTRASCOPE`, and `SetTXFilters`
explicitly skips it for `PANADAPTER/WATERFALL/PANAFALL/PANASCOPE`. The
PANAFALL path uses `initAnalyzer()`
([specHPSDR.cs:504+ [v2.10.3.13+501e3f51]](../../../../../Thetis/Project%20Files/Source/Console/HPSDR/specHPSDR.cs#L504)).

attempt 1 sourced its params from the wrong Thetis function.

### 3.1 Param deviations from `initAnalyzer`

| Param | Thetis `initAnalyzer` | attempt 1 | Notes |
|---|---|---|---|
| `clip` | `floor(0.04 * fft_size)` ≈ **163** | **0** | Edge-bin trim. Does not directly explain DC pedestal but is a verifiable parity miss. |
| `window_type` | **4** (Hamming, -42 dB sidelobes) | **1** (BH4, -92 dB) | BH4 is technically better; we revert to 4 for verbatim parity. |
| `frame_rate` | **15** | **30** | Different overlap math; no DC effect. Reverting for parity. |
| disp ID via `cmaster.inid(1, 0)` | `cmRCVR + 0` = **5** (`cmaster.cs:411 [v2.10.3.13+501e3f51]`) | **2** | Routing label only; both work, but 5 matches Thetis and avoids future multi-pan collisions. |
| `kaiser_pi` | 14.0 (unused for non-Kaiser) | 0.0 (unused) | Both ignored when `window_type != 4 Kaiser`. Matching to 14.0 for parity. |
| typ / sz / bf_sz / fscLin / fscHin / n_stch / calset / fmin / fmax | match | match | No change. |
| `ovrlp` formula | `max(0, ceil(fft_size - rate / frame_rate))` | same formula | Value differs because frame_rate differs. |
| `max_w` formula | `fft_size + min(KEEP_TIME * rate, KEEP_TIME * fft_size * frame_rate)`, KEEP_TIME = 0.1 | same formula | Same. |

Changing only these four (`clip`, `window_type`, `frame_rate`, disp ID)
brings attempt 2 to verbatim Thetis-parity.

---

## 4. attempt 2 architecture

Preserve attempt 1's framework. Six files, same shape as attempt 1:

| File | Role | Change vs attempt 1 |
|---|---|---|
| `src/core/TxAnalyzer.{h,cpp}` (new) | Qt analyzer wrapper at disp 5, polled at 15 fps. | `kTxDispId = 5` (was 2). Param block sourced from initAnalyzer. New diagnostic probe. |
| `src/core/WdspEngine.cpp` | After `createTxChannel`: `TXASetSipMode(ch, 1)` + `TXASetSipDisplay(ch, 5)`. | disp arg `2 → 5`. |
| `src/core/wdsp_api.h` | 14 analyzer + siphon C declarations. | No change. |
| `src/gui/MainWindow.{h,cpp}` | Construct TxAnalyzer at startup; MOX-aware source-switch lambda. | No change. |
| `src/gui/SpectrumWidget.h` | `resetWaterfallAgc()`. | No change. |
| `CMakeLists.txt` | Add `TxAnalyzer.cpp` to `CORE_SOURCES`. | No change. |

### 4.1 Exact `SetAnalyzer` call (attempt 2)

```cpp
// From Thetis specHPSDR.cs:534-643 [v2.10.3.13+501e3f51] — initAnalyzer
// case 1 (complex FFT) + the SetAnalyzer call at :624.
//
// CLIP_FRACTION = 0.04 per specHPSDR.cs:529 [v2.10.3.13+501e3f51].
// KEEP_TIME = 0.1 per specHPSDR.cs:779 [v2.10.3.13+501e3f51].
// frame_rate default = 15 per specHPSDR.cs:335 [v2.10.3.13+501e3f51].
// window_type default = 4 (Hamming) per specHPSDR.cs:134 [v2.10.3.13+501e3f51].
// kaiser_pi default = 14.0 per specHPSDR.cs:145 [v2.10.3.13+501e3f51].
constexpr double kClipFraction = 0.04;
constexpr double kKeepTime     = 0.1;
const int clip   = static_cast<int>(std::floor(kClipFraction * m_fftSize));
const double samplesPerFrame = m_sampleRate / static_cast<double>(m_outputFps);
const int ovrlp  = std::max(0,
    static_cast<int>(std::ceil(m_fftSize - samplesPerFrame)));
const int max_w  = m_fftSize + static_cast<int>(std::min(
    kKeepTime * m_sampleRate,
    kKeepTime * static_cast<double>(m_fftSize) * static_cast<double>(m_outputFps)));
int flp[1] = {0};

SetAnalyzer(
    m_dispId,                  // disp = kTxDispId = 5
    /*n_pixout=*/1,
    /*n_fft=*/1,               // no spur elimination
    /*typ=*/1,                 // complex I/Q
    flp,
    /*sz=*/m_fftSize,          // 4096
    /*bf_sz=*/m_fftSize,       // 4096 (matches dsp_size)
    /*win_type=*/4,            // Hamming (Thetis default)
    /*pi=*/14.0,               // unused for Hamming, matches Thetis default
    /*ovrlp=*/ovrlp,
    /*clp=*/clip,              // 0.04 * 4096 = 163
    /*fscLin=*/0.0,
    /*fscHin=*/0.0,
    /*n_pix=*/m_numPixels,
    /*n_stch=*/1,
    /*calset=*/0,
    /*fmin=*/0.0,
    /*fmax=*/0.0,
    /*max_w=*/max_w);

// From Thetis specHPSDR.cs:301-322 [v2.10.3.13+501e3f51] — DetTypePan /
// DetTypeWF setters drive SetDisplayDetectorMode.  Default UI state is
// peak detection (mode 0).
SetDisplayDetectorMode(m_dispId, /*pixout=*/0, /*mode=*/0);
SetDisplayAverageMode (m_dispId, /*pixout=*/0, /*mode=*/0);
SetDisplayNumAverage  (m_dispId, /*pixout=*/0, /*num=*/1);
SetDisplaySampleRate  (m_dispId, static_cast<int>(m_sampleRate));
```

Construction defaults in `TxAnalyzer.h`:

```cpp
static constexpr int    kTxDispId   = 5;          // Thetis cmaster.inid(1, 0)
                                                  //    = cmRCVR + 0 = 5
double  m_sampleRate = 96000.0;   // TX dsp_rate per WdspEngine::createTxChannel
int     m_outputFps  = 15;        // Thetis specHPSDR.cs:335 default
int     m_fftSize    = 4096;      // matches dsp_size per cmaster.c:180
int     m_numPixels  = 1024;      // re-set on MOX rise to SpectrumWidget width
```

### 4.2 Disp ID = 5 rationale

Thetis allocates per-channel disp slots via
[cmaster.cs:411 [v2.10.3.13+501e3f51]](../../../../../Thetis/Project%20Files/Source/Console/cmaster.cs#L411)
`const int cmRCVR = 5`. The TX panadapter siphon target is
`cmaster.inid(1, 0) = cmRCVR + 0 = 5`. NereusSDR's RX-side spectrum uses
`FFTEngine` (a separate FFTW3 path, not a WDSP analyzer at all), so the
WDSP-analyzer disp namespace is currently empty for NereusSDR. Using disp 5
keeps us in lock-step with Thetis's slot allocation and removes any future
collision risk if multi-pan work later allocates disps 0-4 for RX
panadapters.

---

## 5. Diagnostic probe (built into attempt 2)

Throwaway logging in `TxAnalyzer::applySetAnalyzer()` and
`TxAnalyzer::poll()`. Marked `TXDIAG-attempt2-` prefix for grep, all
guarded by a `#ifdef HAVE_WDSP` block. Removed before the PR ships.

### 5.1 At `applySetAnalyzer` time

One log line per call:

```
[TXDIAG-attempt2-cfg] disp=5 fft_size=4096 sample_rate=96000 fps=15
                       clip=163 win_type=4 pi=14.0 ovrlp=N
                       n_pix=M max_w=K
```

### 5.2 At `poll` time, first 20 frames after each MOX rise

Per-frame log:

```
[TXDIAG-attempt2-frame] mox_rise_seq=N frame=K
   top5=[(idx0,dBm0) (idx1,dBm1) ... (idx4,dBm4)]
   dc_neighborhood=[binN/2-3=v binN/2-2=v binN/2-1=v binN/2=v
                    binN/2+1=v binN/2+2=v binN/2+3=v]
   t_ms=T
```

Frame counter resets on each MOX rise so we capture cold-start behavior of
the analyzer reliably. Rate-limit at 20 frames per rise to keep the log
readable.

---

## 6. Bench plan + acceptance criteria

### 6.1 Plan

Hardware: ANAN-G2 (Saturn) preferred (matches the original handoff bench),
or HL2 if Saturn isn't available.

1. Connect, USB on 14.241 MHz.
2. Key TUN once. Capture 20-frame log.
3. Tools → PureSignal → Two-tone toggle. Capture 20-frame log.
4. Visual check of panadapter and waterfall during 2-tone.
5. Un-key. Verify RX1 panadapter returns and waterfall continues without
   history wipe.

### 6.2 Acceptance criteria

The fix is considered confirmed if:

- **TUN frame:** peak bin near `binN/2 + 6` (gen1 tone freq 600 Hz at 96 kHz
  / 1024 pixels = ~6.4 bins above DC). DC bin (`binN/2`) value must be
  ≥60 dB below peak. Pedestal width (bins within 20 dB of DC) ≤3 bins
  centered on DC.
- **2-tone frame:** two peaks, separated by `(1900 - 700) Hz / bin_width`
  pixels. DC bin value must be ≥40 dB below the lower peak (the carrier
  suppression standard for SSB; Thetis target is -50 to -70 dBc).
- **Visual:** narrow tones, sharp passband cutoff visible on the waterfall,
  no rainbow saturation centered on the carrier.
- **MOX-down:** RX1 spectrum returns; waterfall scrollback continues.

If any criterion fails, the bug is in territory we have not audited yet.
See §8 for follow-up paths.

---

## 7. Implementation notes

### 7.1 Re-applying attempt 1 stash on rebased tree

`stash@{0}` was created against `f993041`. Current tree is at `c2e62e6` on
top of `4eef91e` (origin/main, 130 commits past `f993041`). Conflict expected
on:

- `src/core/wdsp_api.h`
- `src/core/WdspEngine.{h,cpp}`
- `src/gui/MainWindow.{h,cpp}`
- `src/gui/SpectrumWidget.{h,cpp}`
- `CMakeLists.txt`

`TxAnalyzer.{h,cpp}` are new files, no conflict.

Conflict resolution strategy: apply the stash with `git stash apply`,
resolve each conflicted file by keeping the post-rebase tree as the base and
re-applying attempt 1's additions in their post-drift positions. The
attempt 1 changes are additive (679 insertions, 0 deletions in stash diff).

### 7.2 Param + disp-ID deltas vs attempt 1

After stash applies cleanly, edit:

- `TxAnalyzer.h`: `kTxDispId = 5` (was 2). `m_outputFps = 15` (was 30).
- `TxAnalyzer.cpp`: `applySetAnalyzer` body per §4.1 above. Update header
  cite block from `CalcSpectrum` to `initAnalyzer`. Add diagnostic probe
  per §5.
- `WdspEngine.cpp`: `TXASetSipDisplay(channelId, 5)` (was 2). Update the
  inline comment that mentions `kTxDispId=2` and the `cmaster.inid(1, 0)`
  arithmetic.

### 7.3 Out of scope for attempt 2

- Multi-pan support (RX2 + TX panadapters simultaneously). Disp 5 keeps
  the slot reservation consistent with Thetis but no UI work for multi-pan
  is included here.
- Filter-passband zoom on MOX rise. attempt 1 v1 tried this and reverted to
  v2 (keep RX zoom). attempt 2 inherits v2's behavior (do not touch
  centerHz / bandwidth on MOX edge).
- TX-side calibration offset. Thetis default `tx_display_cal_offset = 0`
  per the predecessor handoff §"Hypothesis: Thetis applies -50 dB
  calibration shift on TX bins" (already ruled out).

---

## 8. Open follow-ups if attempt 2 fails

If the bench shows the DC pedestal still present after attempt 2, the next
candidates to audit (in order):

1. **Cold-start `I_samples` buffer state.** If the analyzer's input ring is
   not zeroed before first push, stale memory could feed the FFT. Check
   `XCreateAnalyzer` and `SetAnalyzer` impl in `analyzer.c` for memset
   coverage of the input rings.
2. **`XCreateAnalyzer` + `SetAnalyzer` ordering.** attempt 1 calls
   `XCreateAnalyzer` synchronously in the constructor and `SetAnalyzer`
   immediately after. If `XCreateAnalyzer` defers initialization (spawns a
   thread), `SetAnalyzer` could land before init completes.
3. **First-push timing.** xsiphon mode-1 may push to Spectrum0 before
   `applySetAnalyzer` has run if MOX-up beats the lambda's
   `setNumPixels`/`applySetAnalyzer` chain.
4. **Compare bin-for-bin against Thetis.** Add a parallel logging hook on a
   Thetis bench (Patrick or another tester) and compare the same probe
   indices for the same input. Flushes out any non-source-readable
   difference.

---

## 9. Estimated implementation cost

- Stash apply + conflict resolution: 60-90 min (six files; conflicts are
  expected to be additive-only, not semantic).
- Param + disp-ID deltas: 15 min.
- Diagnostic probe: 30 min (header + impl + grep-friendly tag).
- Build + bench: 30 min.
- Interpret bench: 15 min.

Total: 2.5-3 hours from approval to bench result. Implementation runs in
the `claude/tx-display` worktree as a single throwaway commit on top of
`c2e62e6`; reverted before any PR ships.

---

## 10. Sign-off

This design is awaiting review by JJ Boyd before implementation begins.
Implementation will not start until the design is approved. After
approval, a writing-plans phase produces the per-file/per-task
implementation plan, then implementation runs in this worktree.
