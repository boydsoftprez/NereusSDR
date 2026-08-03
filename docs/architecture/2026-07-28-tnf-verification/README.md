# TNF Bench Verification Matrix

Per-SKU bench matrix for the tunable notch filter epic
(`docs/architecture/2026-07-28-tunable-notch-filter-design.md`).

## Status as of 2026-08-02

Partially verified on ANAN-G2E during the 2026-08-02 bench session. Rows 1, 2,
10, 11 and 16 are PASS on G2E; everything else is untested on every SKU.

**Read this before running the matrix.** That session found four defects the
598-test suite did not catch, and three of them were invisible from the UI: the
marker drew correctly over the carrier, WDSP genuinely held the notch, and the
audio was simply unaffected. Rows 14 through 19 exist specifically because of
what was found, and they are the rows most likely to fail on a SKU we have not
tried yet.

| Found | Root cause | Row that now covers it |
|---|---|---|
| Notch silent until the slice was retuned, pan 2 only | Notches installed into unbound pool channels with no RF origin, then inherited by a later slice | 16, 17 |
| Notch displaced by the drag distance after a CTUN pan drag | `tunefreq` and `shift` written by different call sites from two different stream centres | 14 |
| 50 Hz width selectable but silently widened to 100 Hz | Preset list did not consult `minNotchWidthHz()`; WDSP `autoincr` widens sub-minimum notches | 12, 13 |
| ~100 filter redesigns per second during a drag | One full `UpdateNBPFilters` per mouse-move per channel | 19 |

The per-SKU columns use empty `[ ]` checkboxes for testers to tick as rows
pass. Avoid unicode box characters that may render oddly across viewers.

## How to run a row

Unless a row says otherwise, "works" means **audible**: the tone drops out of
the receive audio. Do not accept the marker drawing over the carrier as
evidence. Three of the four defects above looked completely correct on screen.

Where a row says "cold", it means: quit the app, relaunch, reconnect, and do
not retune anything first. Several defects only appear on the first pass and
are masked forever afterwards by any retune.

## Matrix

| # | Check | G2 | HL2 | G2E | HermesII | Notes |
|---|-------|----|----|-----|----------|-------|
| 1 | Ctrl + right-click places a notch on the clicked carrier | [ ] | [ ] | [x] | [ ] | G2E 2026-08-02 |
| 2 | The notch is audible: the tone drops out | [ ] | [ ] | [x] | [ ] | G2E 2026-08-02 |
| 3 | Drag the notch body: the cut tracks it | [ ] | [ ] | [ ] | [ ] | |
| 4 | Drag an edge: the cut widens and narrows | [ ] | [ ] | [ ] | [ ] | 8 px gate, plus/minus 4 px zone |
| 5 | Wheel over a selected notch resizes it | [ ] | [ ] | [ ] | [ ] | 10 Hz/detent, 1 Hz with Shift |
| 6 | Right-click a notch: Bypass returns the tone | [ ] | [ ] | [ ] | [ ] | marker greys |
| 7 | Right-click a notch: Remove deletes it | [ ] | [ ] | [ ] | [ ] | |
| 8 | Master TNF off restores everything; on re-cuts | [ ] | [ ] | [ ] | [ ] | Ctrl+Shift+N and the status light |
| 9 | Status light is amber when notches exist and TNF is off | [ ] | [ ] | [ ] | [ ] | the D-a first-use trap |
| 10 | +TNF button places a notch at the listening frequency | [ ] | [ ] | [x] | [ ] | G2E 2026-08-02 |
| 11 | Settings > DSP > TNF lists every notch with exact numbers | [ ] | [ ] | [x] | [ ] | G2E 2026-08-02 |
| 12 | Min-width readout matches the filter: 100 Hz at nc 4096 / 48 kHz | [ ] | [ ] | [ ] | [ ] | `1600/(nc/256)*(rate/48000)` |
| 13 | Width presets below the minimum are disabled, not silently widened | [ ] | [ ] | [ ] | [ ] | 50 Hz greyed at nc 4096 |
| 14 | **CTUN pan drag, then place a notch: it lands on the carrier** | [ ] | [ ] | [ ] | [ ] | was off by the drag distance |
| 15 | Retune the slice within the band: the notch stays on its carrier | [ ] | [ ] | [ ] | [ ] | not on the VFO |
| 16 | **Cold: notch on pan 1 bites with no retune** | [ ] | [ ] | [x] | [ ] | G2E 2026-08-02 |
| 17 | **Cold: open pan 2, notch it, bites with no retune** | [ ] | [ ] | [ ] | [ ] | the pool-channel defect; the sharpest row here |
| 18 | Cold with saved notches: they restore and bite on connect | [ ] | [ ] | [ ] | [ ] | exercises the same path as 17 |
| 19 | Drag a notch hard: no audio glitching or CPU spike | [ ] | [ ] | [ ] | [ ] | coalesced to 20 Hz |
| 20 | Auto-increase on: a sub-minimum notch is widened, not dropped | [ ] | [ ] | [ ] | [ ] | |
| 21 | Live sample-rate change: notches survive and stay on frequency | [ ] | [ ] | [ ] | [ ] | min-width readout must follow |
| 22 | DSP filter size change: min-width readout follows | [ ] | [ ] | [ ] | [ ] | nc 8192 gives 50 Hz |
| 23 | Band change away and back: notches still correct | [ ] | [ ] | [ ] | [ ] | absolute RF, so inert off-band |
| 24 | Reconnect: notches restore on every open channel | [ ] | [ ] | [ ] | [ ] | |
| 25 | Restart: notches persist across sessions | [ ] | [ ] | [ ] | [ ] | AppSettings, global not per-MAC |
| 26 | A slice added after notches exist inherits them | [ ] | [ ] | [ ] | [ ] | |
| 27 | Visual notch on: the trace dents at the notch | [ ] | [ ] | [ ] | [ ] | default off |
| 28 | Visual notch on: the waterfall dents too | [ ] | [ ] | [ ] | [ ] | both planes |
| 29 | **Visual notch on: the S-Meter does not move** | [ ] | [ ] | [ ] | [ ] | MaxBin reads the pristine copy |
| 30 | Visual notch: no dent during MOX | [ ] | [ ] | [ ] | [ ] | |
| 31 | CW mode: a notch added at F sits at F, not F +/- pitch | [ ] | [ ] | [ ] | [ ] | CW-pitch correction NOT ported |
| 32 | +TNF with RIT on: the notch lands where you are listening | [ ] | [ ] | [ ] | [ ] | deliberate divergence, design 7.5 |
| 33 | TCI `rx_nf_enable` set and query round-trip | [ ] | [ ] | [ ] | [ ] | both rx indices |
| 34 | A UI flip of the master broadcasts to both TCI rx indices | [ ] | [ ] | [ ] | [ ] | |

## Rows most likely to fail on an untried SKU

**17** is the sharpest. The pool-channel defect depended on the pool opening
more channels than there are slices, which is SKU-dependent: `maxSlices` is 5
on G2 and G2E, 1 on HL2, 2 on HermesII. On HL2 the defect could not occur at
all, and on HermesII the window is narrower. A pass on G2E does not predict
the others.

**21** and **22** both move `min_notch_width`, which scales with `nc` and the
DSP rate. HL2 at 384 kHz P1 is the interesting case.

**31** has never been exercised. The CW-pitch non-port is reasoned from source
and covered by a unit test, but no one has listened to it.

## Deferred

None. Every row in the design's scope is listed here.
