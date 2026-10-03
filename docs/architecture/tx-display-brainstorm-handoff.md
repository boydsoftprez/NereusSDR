# TX Panadapter Display — Brainstorm Handoff

**Date:** 2026-05-08
**Author:** J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code
**Status:** Attempt 1 stashed. Brainstorming attempt 2.
**Worktree:** `.claude/worktrees/tx-display` (branch `claude/tx-display`, at
`f993041` — head of PR #212 as of 2026-05-08)
**Branch base:** `f993041 fix(puresignal): unblock HL2 PureSignal AutoAtt
convergence + disconnect crash` (the rebased version of the original
attempt-1 baseline `c00382f`; same tree, sitting on top of `ef6a5d1` which
includes anti-VOX 3M-3a-iv work and merged PRs #211 / #213 / #215).
**Stashed attempt 1:** `stash@{0}` in worktree
`.claude/worktrees/wizardly-poitras-dbab75` (branch
`claude/wizardly-poitras-dbab75`, also at `f993041` after the same rebase).
Recover with `git stash apply stash@{0}` or inspect via
`git stash show -p stash@{0}`. The stash was created against c00382f's tree;
applying it on f993041 should be conflict-free because attempt-1 files
(`TxAnalyzer.{h,cpp}`, `wdsp_api.h` analyzer extras, `MainWindow.{h,cpp}` MOX
hooks, `SpectrumWidget.h` resetWaterfallAgc, `WdspEngine.cpp` siphon wire,
`CMakeLists.txt`) don't overlap with the 33 commits that landed in between.

---

## Goal

When the user keys up (MOX-up edge), the panadapter / waterfall should
display the **transmitted signal** as Thetis displays it: PureSignal-corrected
2-tone test should look like two narrow vertical lines with a sharp passband
cutoff ("pizza-cutter on the passband" was the user's phrase). On MOX-down,
RX1 spectrum returns to the panadapter without losing waterfall scrollback
continuity.

## Baseline state (branch tip `f993041`, before any attempt-2 work)

- Panadapter source during MOX = RX1 DDC stream. The user sees antenna
  readback (PA bleed, splatter, whatever the receiver hears while keyed).
- This is wrong: Thetis source-switches the panadapter to the WDSP
  TX-side analyzer during MOX so the user sees the intended TX signal,
  not the antenna response.
- All other PR #212 follow-up work (PureSignal SATT convergence fix,
  disconnect crash fix, AutoAtt sentinel, audio-volume listener) is
  already on PR #212 at commit `f993041`. The TX display work is the only
  remaining piece of the PR #212 follow-up epic.
- `f993041` also includes (via rebase onto `ef6a5d1`) the 3M-3a-iv
  anti-VOX feature plus merged PRs #211 / #213 / #215. Those changes touch
  `RadioModel`, `MoxController`, `TxChannel`, `TransmitModel` but DO NOT
  alter the WDSP TX channel siphon path or the MOX state machine that
  attempt 1 hooked into. TX display work can layer on top cleanly.

## Attempt 1 — what was built (now stashed)

Five files modified, two new files added. Full diff in `stash@{0}`.

1. **`src/core/wdsp_api.h`** — added 14 extern-C declarations for the
   WDSP analyzer + siphon API: `XCreateAnalyzer`, `DestroyAnalyzer`,
   `SetAnalyzer`, `GetPixels`, `Spectrum0`, `SetDisplayDetectorMode`,
   `SetDisplayAverageMode`, `SetDisplayNumAverage`, `SetDisplayAvBackmult`,
   `SetDisplaySampleRate`, `ResetPixelBuffers`, `TXASetSipMode`,
   `TXASetSipDisplay`, `TXASetSipPosition`.

2. **`src/core/TxAnalyzer.{h,cpp}`** (new) — Qt host class wrapping
   the WDSP analyzer for the TX panadapter:
   - Allocates one analyzer instance at `kTxDispId = 2` via
     `XCreateAnalyzer(2, ..., size=16384, m_LO=1, m_stitch=1)`.
   - `applySetAnalyzer()` calls `SetAnalyzer` with `typ=1` (complex IQ),
     `win_type=1` (Blackman-Harris 4-term), `sz=4096`, `bf_sz=4096`,
     `n_pix=display_width`, ovrlp computed per Thetis specHPSDR.cs:784.
   - `poll()` slot fires on a 30 fps QTimer, calls `GetPixels(disp=2)`,
     emits `txFftReady(receiverId=-1, bins)` on flag==1.

3. **`src/core/WdspEngine.cpp`** — after `createTxChannel` allocates
   the WDSP TX channel (channelId 1), wires the siphon to push to
   our analyzer:
   ```cpp
   TXASetSipMode(channelId, 1);       // push-to-Spectrum0 mode
   TXASetSipDisplay(channelId, 2);    // disp = kTxDispId
   ```

4. **`src/gui/MainWindow.{h,cpp}`** — MOX-aware source switch:
   - Creates `TxAnalyzer` at construction, sets `sampleRate=96000.0`
     and 30 fps.
   - On `MoxController::moxStateChanged(true)`:
     - Saves `m_savedSpectrumSampleRate` and `m_savedSpectrumDdcHz`.
     - Sets `m_spectrumWidget->setSampleRate(96000.0)` and
       `setDdcCenterFrequency(carrier_hz_from_active_slice)`.
     - Calls `m_txAnalyzer->setNumPixels(display_width)`.
     - `disconnect(m_fftEngine, &FFTEngine::fftReady, ...)` and
       `connect(m_txAnalyzer, &TxAnalyzer::txFftReady, ...)`.
     - Calls `m_spectrumWidget->resetWaterfallAgc()` so the AGC
       re-primes to the TX dynamic range on the first frame.
     - `m_txAnalyzer->start()`.
   - On `moxStateChanged(false)`: reverse — stop, restore sampleRate
     and ddcCenterHz, reconnect FFTEngine, reset AGC again.
   - **Critically**: `m_centerHz` and `m_bandwidthHz` are NOT touched
     across the MOX edge. This was the v2 design decision (after
     trying filter-passband zoom in v1) — preserve the user's RX
     zoom level so the carrier stays at the same x-position, AND
     so `setFrequencyRange` is never called and the largeShift
     waterfall-clear path never fires.

5. **`src/gui/SpectrumWidget.{h,cpp}`** — added
   `void resetWaterfallAgc() { m_wfAgcPrimed = false; }` so MOX-edge
   transitions can force the auto-AGC tracker to re-prime to the
   new dynamic range immediately, instead of letting the 5%-alpha
   follower take ~3 s to converge.

6. **`CMakeLists.txt`** — added `src/core/TxAnalyzer.cpp` to the
   `NereusSDRObjs` source list.

### Sub-attempts within attempt 1

- **v1 — filter-passband zoom:** computed Thetis `UpdateTXDisplayVars`
  10% padding, applied `fscLin`/`fscHin` clipping in `SetAnalyzer` so
  the analyzer's n_pix bins span only the TX filter passband (e.g.,
  USB 0–3190 Hz). Set `SpectrumWidget` sampleRate / bandwidth /
  centerHz to display ONLY that passband during MOX.
- **v2 — keep-RX-zoom (the version stashed):** reverted fscLin/fscHin
  to 0 (full 96 kHz analyzer span). Don't touch centerHz / bandwidth
  on MOX. Just update sampleRate (96000) and ddcCenterHz (carrier).
  This preserves the user's RX zoom and avoids the largeShift wipe.

### Visual outcome of attempt 1 v2 (current bench observation)

- Spectrum trace line graph during 2-tone shows clean two narrow peaks
  at the correct frequencies (carrier+700 Hz and carrier+1900 Hz). ✅
- Waterfall during 2-tone shows a heavily-saturated red band ~12 kHz
  wide centered on the carrier, with a colored halo extending another
  ~25 kHz on each side (~50 kHz total colored region). ❌
- User's words: "looks like we're still really splattering while
  transmitting", "hyper saturated", "not honoring some type of offset".
- Compared to Thetis on the same hardware doing the same 2-tone test,
  Thetis shows narrow tones with sharp passband cutoff. Our display
  is dramatically wider and brighter than Thetis's.

## What we ruled out via source-first verification

These were live hypotheses during attempt 1 that the verification
killed. Don't waste cycles on them again unless new evidence emerges.

### ❌ Hypothesis: Thetis applies a -50 dB calibration shift on TX bins

**Status: dead.** Source verification:

- `Display.cs:1407` — `private static float tx_display_cal_offset = 0f;`
- `setup.designer.cs:11837` — Setup spinbox default value = 0.
- `Display.cs:4824-4831` — `RX1Offset` getter during MOX returns
  `tx_display_cal_offset` (= 0 by default).
- `Display.cs:5249` — bins rendered as `data[i] + fOffset`. With
  `fOffset = 0`, raw bins are passed through.
- `PanDisplay.cs:4759` — explicit comment "calibration_data_set: not
  using calibration".
- `specHPSDR.cs:216` — `private int calibration_data_set = 0;` (default).

Thetis adds zero to TX bins by default. We're already doing the same
thing. The visual difference is NOT from a calibration constant.

### ❌ Hypothesis: Bin layout is non-FFT-shifted (raw FFT order)

**Status: dead.** Source verification:

- `analyzer.c:215-280` — `Celiminate()` iterates `fft_out` indexed
  from `out_size/2 + 1 + clip + fscL` to `out_size`, then from `0`
  to `out_size/2 - clip - fscH`. This is exactly an FFT-shift —
  second half of the raw FFT first, then first half — putting DC
  at the middle of the result buffer.

`SpectrumWidget::visibleBinRange()` correctly assumes bin n_pix/2 = DC.

### ❌ Hypothesis: Bin-to-frequency mapping is wrong

**Status: dead.** Diagnostic data confirms peaks land exactly where
2-tone test tones should be:

```
Frame 1 (TUN, single tone):
  Top 5 bins: 514, 515, 513, 516, 512  (clustered around bin 508 = DC)
  Peak at bin 514 = -6.2 dBm = ~565 Hz audio offset = TUN tone

Frame 2 (2-Tone test, USB):
  Top 5 bins: 515, 528, 516, 529, 527
  Peak 1 at bin 515 = -23.6 dBm = +700 Hz audio offset
  Peak 2 at bin 528 = -23.9 dBm = +1900 Hz audio offset
  Cluster spacing: 13 bins = 1200 Hz = exact 2-tone spacing
```

Sample rate = 96000 Hz, n_pix = 1017, bin width = 94.4 Hz/bin.
Expected positions: 700/94.4 = 7.4 → bin 515.4. 1900/94.4 = 20.1 → bin 528.1.
Match.

### ❌ Hypothesis: Window mismatch (BH4 vs Hamming)

**Status: dead.** Verified Thetis default `window_type = 4` (Hamming,
-42 dB side lobes) per `specHPSDR.cs:134`. We use `1` (BH4, -92 dB
side lobes). BH4 is *better* than Hamming, so this can only make
us look CLEANER than Thetis, not wider. Wrong direction for the bug.

### ❌ Hypothesis: Frame rate mismatch creates artifacts

**Status: probably dead.** Thetis default `frame_rate = 15`
(`specHPSDR.cs:335`); we use 30. Different overlap math (Thetis
overlap = 0 with 4096 FFT @ 15 fps @ 96 kHz; ours = 896). Steady-state
spectrum shape isn't affected by overlap value, only update rate.
Frame rate doesn't widen tones.

## Diagnostic data captured at end of attempt 1 v2

Three frames from a TUNE → 2-Tone test on ANAN-G2 (Saturn) at 14.241 MHz:

```
Frame 1 (TUN):
  bins=1017
  top5=[(514,-6.2) (515,-8.2) (513,-17.1) (516,-21.3) (512,-24.5)]
  probes=[bin0=-69.5  bin254=-66.6  binN/2=-36.1  bin762=-66.2  binEnd=-69.5]

Frame 2 (2-Tone):
  bins=1017
  top5=[(515,-23.6) (528,-23.9) (516,-24.8) (529,-25.7) (527,-27.9)]
  probes=[bin0=-75.5  bin254=-72.8  binN/2=-33.9  bin762=-72.1  binEnd=-75.5]

Frame 3 (2-Tone):
  bins=1017
  top5=[(528,-13.5) (515,-13.6) (516,-14.8) (529,-17.0) (527,-21.6)]
  probes=[bin0=-71.1  bin254=-68.5  binN/2=-32.3  bin762=-67.7  binEnd=-71.1]
```

**Two key empirical observations from this data:**

1. The carrier bin (bin 508 = DC = `binN/2` probe) reads **-32 to -36 dBm**
   even during 2-tone. That's only ~10–20 dB below the actual test tone
   peaks. SSB carrier suppression should be -50 to -70 dBc; we're seeing
   ~10 dBc which is terrible. **There's real DC / carrier energy in the
   I/Q baseband at the xsiphon tap point.** This is not BH4 side-lobe
   leakage (which would be -92 dBc = far below the noise floor).

2. There's a smooth gradient from `bin254 = -73 dBm` to `bin508 = -34 dBm`
   to `bin762 = -68 dBm` — a ~40 dB pedestal centered on DC, dropping
   over ~24 kHz on each side. This is what creates the wide colored band
   in the waterfall: bins between roughly -50 and -73 dBm are still
   above the colormap floor, so they render as warm colors across the
   entire central region.

## The unresolved question

**Why does our siphon at TXA.c:586 (post-PostGen, pre-xiqc) show a
wide DC pedestal, when Thetis tapping the same WDSP source at the
same line in the same TXA pipeline shows clean band-limited tones?**

Either:
- (a) The signal at line 586 is genuinely the same in both cases and
  Thetis's *rendering* hides the pedestal we expose (different colormap,
  different thresholds, different smoothing); we just need to render
  it differently.
- (b) Thetis's TXA pipeline produces different content at line 586 due
  to a configuration delta we haven't found (some setter we're not
  calling, some default we're inheriting wrong, some upstream stage
  in different state).
- (c) Thetis's TX panadapter is NOT actually fed from the line 586
  siphon — it taps somewhere else, and our reading of cmaster.cs:539-540
  + console.cs:24243 is incomplete.
- (d) Some combination.

## TXA pipeline reference (verbatim from `third_party/wdsp/src/TXA.c:557-592`)

```c
void xtxa (int channel)
{
    xresample (txa[channel].rsmpin.p);              // input resampler
    xgen      (txa[channel].gen0.p);                // input signal generator (PreGen)
    xpanel    (txa[channel].panel.p);               // includes MIC gain
    xphrot    (txa[channel].phrot.p);               // phase rotator
    xmeter    (txa[channel].micmeter.p);
    xamsqcap  (txa[channel].amsq.p);                // downward expander capture
    xamsq     (txa[channel].amsq.p);                // downward expander action
    xeqp      (txa[channel].eqp.p);                 // pre-EQ
    xmeter    (txa[channel].eqmeter.p);
    xemphp    (txa[channel].preemph.p, 0);          // FM pre-emphasis (option 1)
    xwcpagc   (txa[channel].leveler.p);             // Leveler
    xmeter    (txa[channel].lvlrmeter.p);
    xcfcomp   (txa[channel].cfcomp.p, 0);           // CFC + post-EQ
    xmeter    (txa[channel].cfcmeter.p);
    xbandpass (txa[channel].bp0.p, 0);              // PRIMARY BANDPASS FILTER (line 573)
    xcompressor (txa[channel].compressor.p);        // COMP
    xbandpass (txa[channel].bp1.p, 0);              // aux bp (post-COMP)
    xosctrl   (txa[channel].osctrl.p);              // CESSB overshoot
    xbandpass (txa[channel].bp2.p, 0);              // aux bp (post-CESSB)
    xmeter    (txa[channel].compmeter.p);
    xwcpagc   (txa[channel].alc.p);                 // ALC
    xammod    (txa[channel].ammod.p);               // AM mod
    xemphp    (txa[channel].preemph.p, 1);          // FM pre-emphasis (option 2)
    xfmmod    (txa[channel].fmmod.p);               // FM mod
    xgen      (txa[channel].gen1.p);                // PostGen — TUN + 2-tone (line 583)
    xuslew    (txa[channel].uslew.p);               // up-slew envelope
    xmeter    (txa[channel].alcmeter.p);
    xsiphon   (txa[channel].sip1.p, 0);             // SIPHON for display (line 586)  ← OUR TAP
    xiqc      (txa[channel].iqc.p0);                // PureSignal pre-distortion (line 587)
    xcfir     (txa[channel].cfir.p);                // P2 compensating FIR
    xresample (txa[channel].rsmpout.p);             // output resampler
    xmeter    (txa[channel].outmeter.p);
}
```

Critical detail: PostGen (line 583) overwrites midbuff with the test tones.
The bandpass filters bp0/bp1/bp2 ALL run *before* PostGen, so they don't
filter the test tones at all. PostGen output → uslew envelope → siphon.

## Avenues to brainstorm in the next session

These are not solutions, just open paths the brainstorm should evaluate:

1. **Render-side fix only** — keep attempt-1 architecture, switch from
   AGC-driven colormap to fixed Thetis-style thresholds during MOX
   (e.g., low=-130 high=-30 dBm). The carrier pedestal at -34 dBm
   would render as a single warm pixel near the ceiling, the rest
   dark. Cheapest path. Open question: does Thetis's waterfall actually
   use fixed thresholds? Need to read Thetis Display.cs waterfall
   render path to confirm.

2. **Different siphon tap point** — vendor-modify WDSP TXA.c to add
   a second siphon BEFORE bp0 (line 573) or between bp0 and the
   compressor. After bp0 the signal IS bandpass-filtered to the
   audio passband. If the user's "pizza-cutter" Thetis observation
   means Thetis somehow shows a signal that's been through bp0,
   this is the path. Caveat: Thetis uses the same WDSP and only
   wires TXASetSipDisplay to the existing line-586 sip1.p, so this
   would diverge from Thetis behavior unless we discover Thetis
   has a second siphon we missed.

3. **Inspect Thetis Display.cs waterfall render path in full** —
   we read fOffset application but not the waterfall colormap
   threshold defaults. Compare to our SpectrumWidget AGC behavior.
   Possibly the entire visual delta is one or two static threshold
   values.

4. **Capture a Thetis 2-tone waterfall on the same radio** for
   side-by-side comparison. If Thetis ALSO shows a pedestal that
   it just renders less obtrusively, fix is render-side. If Thetis
   shows truly clean tones with no DC pedestal, fix is signal-side
   and we need to find the configuration delta.

5. **Check ALC / leveler / compressor state during 2-tone in our
   build** — verify they're configured the same as Thetis. The
   pedestal might be from one of these stages adding DC bias even
   though PostGen overwrites their output. (Unlikely — overwrite
   is overwrite — but worth ruling out.)

6. **Check `sip1` siphon initialization parameters** — created with
   `sipsize=16384` and `fftsize=16384` (TXA.c:394-403). Our analyzer
   uses 4096-FFT. Could this 16k vs 4k mismatch in siphon's internal
   state cause anything? Probably not (mode-1 path bypasses sipsize),
   but worth a sanity check.

7. **Confirm `cmaster.inid(1, 0)` semantics** — Thetis's
   `cmsetup.c:195-214` returns `cmRCVR + id` for stype=1. With
   `cmRCVR=5` and `id=0`, that's disp ID **5**, NOT 2. Our attempt
   used `kTxDispId = 2` based on a wrong cite. Did this matter? The
   siphon was redirected to disp 2 via TXASetSipDisplay, so the data
   flowed correctly to our analyzer. But if Thetis's analyzer at
   disp 5 is configured with parameters that disp 2 doesn't pick up
   (analyzer state might be per-disp), this could be relevant.

## Source-first cite map (for next session, ready to grep)

| Topic | File | Line |
|---|---|---|
| TXA pipeline | `third_party/wdsp/src/TXA.c` | 557-592 |
| Siphon mode dispatch | `third_party/wdsp/src/siphon.c` | 101-137 |
| Siphon create (TX) | `third_party/wdsp/src/TXA.c` | 394-403 |
| Generator 2-tone (mode 1) | `third_party/wdsp/src/gen.c` | 241-269 |
| `Spectrum0` (data ingest) | `third_party/wdsp/src/analyzer.c` | 1726-1769 |
| `Cspectra` (complex FFT path) | `third_party/wdsp/src/analyzer.c` | 837-918 |
| `Celiminate` (FFT-shift logic) | `third_party/wdsp/src/analyzer.c` | 215-281 |
| Window types | `third_party/wdsp/src/analyzer.c` | 55-176 |
| `SetAnalyzer` (api) | `third_party/wdsp/src/analyzer.c` | 1173-1240 |
| Thetis SetAnalyzer caller | `../Thetis/Project Files/Source/Console/HPSDR/specHPSDR.cs` | 624-643 + 786-805 |
| Thetis CalcSpectrum | `../Thetis/Project Files/Source/Console/HPSDR/specHPSDR.cs` | 738-806 |
| Thetis SpecHPSDR class defaults | `../Thetis/Project Files/Source/Console/HPSDR/specHPSDR.cs` | 134, 195, 216, 335 |
| Thetis cmaster siphon wiring | `../Thetis/Project Files/Source/Console/cmaster.cs` | 539-540 |
| Thetis MOX-aware GetPixels | `../Thetis/Project Files/Source/Console/console.cs` | 24234-24297 |
| Thetis fOffset (cal) usage | `../Thetis/Project Files/Source/Console/Display.cs` | 4814-4904 (RX1Offset etc) |
| Thetis fOffset application | `../Thetis/Project Files/Source/Console/Display.cs` | 5245-5260 (data[i]+fOffset) |
| Thetis tx_display_cal_offset default | `../Thetis/Project Files/Source/Console/setup.designer.cs` | 11837 |
| `inid()` definition | `../Thetis/Project Files/Source/ChannelMaster/cmsetup.c` | 195-214 |
| Thetis cmRCVR=5 | `../Thetis/Project Files/Source/Console/cmaster.cs` | 411 |
| `UpdateTXDisplayVars` (TX panadapter padding) | `../Thetis/Project Files/Source/Console/console.cs` | 8015-8049 |

## Recommendation for the next session

Before touching code, do **one more verification step** that attempt 1
skipped:

**Capture a side-by-side bench comparison** of the same 2-tone test on
the same radio (ANAN-G2) in Thetis vs in our app, with the panadapter
at the same zoom level. If Thetis shows the pedestal we see (just
rendered prettier), the fix is render-side (Path 1 above). If Thetis
shows truly clean tones with no DC pedestal, the fix is signal-side
(Path 2/3/5/6/7) and we need to find the configuration delta.

Without that comparison, we'll keep guessing whether the bug is in
the data or in the rendering. Get the empirical answer first, then
brainstorm the fix from solid ground.
