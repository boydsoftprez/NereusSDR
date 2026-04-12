# Phase 3G-8 — RX1 Display Parity Verification Matrix

> **Status:** Structure drafted. Screenshot capture happens as the
> final step before opening the PR. This file lists every control
> landed in commits 1-8 and the expected live-visible effect so the
> user can capture before/after screenshots to attach to the PR.

## How to use

1. Build and launch: `cmake --build build && open build/NereusSDR.app`.
2. Open **Settings → Display** and pick each page in turn.
3. For every control below:
   - Capture the spectrum/waterfall area at the documented default,
     save as `S01-default.png` (or `W…` / `G…` prefix).
   - Change the control to the documented test value, click Apply.
   - Capture again, save as `S01-changed.png`.
4. Drop all screenshots in this directory.
5. Reference the matrix from the PR description.

Total expected: **47 controls × 2 = ~94 screenshots.** Some
controls (e.g. FPS slider) may produce visually subtle deltas; a
short animated gif or side-by-side comparison is fine in those cases.

---

## Spectrum Defaults (17)

| # | Control | Default | Test value | Expected visible change |
|---|---|---|---|---|
| S1 | FFT Size | 4096 | 16384 | Spectrum trace finer resolution |
| S2 | FFT Window | Blackman-Harris | Flat-Top | Sidelobes change shape |
| S3 | FPS | 30 | 60 | Frame rate visibly doubles |
| S4 | Averaging | Weighted | Logarithmic | Trace smooths over time in log-domain |
| S5 | Fill toggle | on | off | Area under trace no longer filled |
| S6 | Fill Alpha | 70 | 20 | Fill visibly more transparent |
| S7 | Line Width | 1 | 3 | Trace line visibly thicker (QPainter fallback) |
| S8 | Cal Offset | 0 dBm | +10 dBm | Trace shifts up by 10 dB |
| S9 | Peak Hold | off | on | Dotted peak line appears underneath trace |
| S10 | Peak Hold Delay | 2000 ms | 5000 ms | Peak hold decays slower |
| S11 | Data Line Color | cyan | red | Trace colour changes |
| S12 | Data Line Alpha | 230 | 100 | Trace visibly more transparent |
| S13 | Data Fill Color | cyan | green | Fill colour changes |
| S14 | Gradient | off | on | Fill becomes a vertical gradient |
| S15 | Averaging Time | 100 ms | 2000 ms | Smoothing gets visibly slower |
| S16 | Decimation | 1 | 4 | (scaffolded only — no renderer effect yet) |
| S17 | Thread Priority | Above Normal | Normal | No visible change; confirm no crash |

## Waterfall Defaults (17)

| # | Control | Default | Test value | Expected visible change |
|---|---|---|---|---|
| W1 | High Threshold | -40 dBm | -60 dBm | Waterfall hot colours appear on weaker signals |
| W2 | Low Threshold | -130 dBm | -110 dBm | Waterfall baseline moves up; less black |
| W3 | AGC | off | on | Thresholds auto-adjust to visible signal range |
| W4 | Update Period | 50 ms | 200 ms | Waterfall scroll visibly slower |
| W5 | Reverse Scroll | off | on | Newest row at bottom instead of top |
| W6 | Opacity | 100 | 30 | Waterfall becomes translucent |
| W7 | Color Scheme | Default | LinLog | Palette changes |
| W8 | Timestamp Position | None | Right | "hh:mm:ss" appears at top-right of waterfall |
| W9 | Timestamp Mode | UTC | Local | Timestamp shows local time instead of UTC |
| W10 | Waterfall Low Color | black | blue | (scaffolded; requires Custom scheme for runtime effect) |
| W11 | Show RX filter on WF | off | on | Cyan band marks filter passband on waterfall |
| W12 | Show TX filter on RX WF | off | on | (no effect without TX state model; verify no crash) |
| W13 | Show RX zero line on WF | off | on | Red vertical line at VFO frequency on waterfall |
| W14 | Show TX zero line on WF | off | on | (no effect without TX state model; verify no crash) |
| W15 | WF use spectrum min/max | off | on | Waterfall tracks spectrum grid Max/Min |
| W16 | WF Averaging | None | Weighted | Waterfall rows smooth in time |
| W17 | Color Scheme (count) | 7 | — | Verify all 7 options are present in combo |

## Grid & Scales (13)

| # | Control | Default | Test value | Expected visible change |
|---|---|---|---|---|
| G1 | Show Grid | on | off | Grid lines disappear |
| G2 | dB Max (per-band) | -40 | -20 | Spectrum top compresses on current band only |
| G3 | dB Min (per-band) | -140 | -160 | Spectrum bottom extends on current band only |
| G4 | dB Step (global) | 10 | 5 | More horizontal grid lines |
| G5 | Freq Label Align | Center | Right | Frequency tick labels right-justify in the scale bar |
| G6 | Band Edge Color | red | green | (if band edges drawn: colour changes) |
| G7 | Show Zero Line | off | on | Dashed 0 dBm line appears across spectrum |
| G8 | Show FPS Overlay | off | on | "N.N fps" appears top-right of spectrum |
| G9 | Grid Color | white 40α | yellow | Vertical grid lines change colour |
| G10 | Grid Fine Color | white 20α | gray | Fine dotted grid changes colour |
| G11 | H-Grid Color | white 40α | cyan | Horizontal dB grid lines change colour |
| G12 | Text Color | yellow | white | Axis tick labels change colour |
| G13 | Zero Line Color | red | blue | Zero line changes colour (when G7 on) |

**Per-band verification for G2/G3:** tune across a band boundary
(e.g. 40m → 30m) and confirm the "Editing band: N" label updates
and the spinboxes show the stored slot for the new band, and the
spectrum grid scale snaps to the new band's values. Then edit in
one band, switch to another, switch back — persistent.

---

## Known deferrals (no screenshot expected)

- S16 Decimation — UI scaffolded, no renderer effect.
- W10 Low Color — persisted only, runtime gradient rebuild
  requires Custom scheme parser (future commit).
- W12 / W14 TX overlays — wait on TX state model.
- GPU path line width and gradient — QPainter fallback wired;
  GPU pipeline wire-up in a future polish commit.
- Data Line / Data Fill colour split — shares `m_fillColor`
  until UX feedback justifies splitting them.
