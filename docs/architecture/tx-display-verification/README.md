# Phase 3M-5 TX Display — bench verification matrix

**Branch:** `claude/tx-display-revive`
**Status:** rows 1-8 verified on an ANAN-7000DLE (OrionMkII) 2026-08-05.
Rows 9-14 pending. Rows 15 and 16 are known open items, not tests.
Rows 19-27 (the remote window, display duplex, the transmit monitor, the keyed readings)
added 2026-09-26 by the remote-window parity plan
(`docs/architecture/2026-09-24-remote-window-parity-plan.md`, Tasks 27 to 34); pending.

Verified by J.J. Boyd (KG4VCF). AI-assisted implementation via Anthropic
Claude Code.

---

## Why this matrix exists

The transmit display is the only place in NereusSDR that uses WDSP's
analyzer at all — the receive path runs our own `FFTEngine`. So unlike most
of this codebase it has no proven twin to check against, and four of the five
defects found on 2026-08-05 were invisible from outside: they produced a
plausible-looking picture that was wrong. Two of them (`bf_sz`, the symmetric
clip) could only be seen by logging what WDSP was actually handed.

That is the lesson worth carrying: **for this subsystem, "it draws something"
is not evidence.** Rows below check numbers, not vibes.

---

## Setup

- Radio: any P2 SKU. Rows 9-11 additionally need an ORION-class radio (RX
  survives transmit) and rows 12-13 a HERMES-class one (it does not).
- Mode LSB or USB, TX BW 100-2900 Hz, TUNE power low (2-5 W) into a dummy
  load.
- `QT_LOGGING_RULES="*.debug=true"` to see the `TxAnalyzer SetAnalyzer`
  configuration line, which is what rows 3-5 read.

---

## Matrix

| # | What | How to check | Expected | Status |
|---|------|--------------|----------|--------|
| 1 | Transmit trace appears | Key TUNE | A trace, not a frozen or blank pan | PASS 2026-08-05 |
| 2 | Trace is on frequency | Key TUNE in LSB, read the peak against the dial | Peak sits `cw_pitch` (600 Hz) BELOW the dial in LSB, above in USB. This is correct and matches Thetis: the tone is generated at ±600 Hz (`console.cs:30077-30087`) and Thetis compensates only its numeric peak readout (`console.cs:21905-21931`), never the trace | PASS 2026-08-05 |
| 3 | Analyzer gets the real block size | Read the log line | `bf_sz` equals `blockSize`, and both equal the TXA `dsp_size` (2048 observed). NOT the FFT size | PASS 2026-08-05 |
| 4 | Symmetric clip stands down | Read the log line | `clp=0` whenever `window=[-4000,4000]`; `clp=1310` (the 0.04 fraction) only when `window=[0,0]` | PASS 2026-08-05 |
| 5 | Span clip leaves enough bins | Read the log line | `32768 - fsclipL - fsclipH` comfortably exceeds `n_pix`. Observed 2731 vs 1202 | PASS 2026-08-05 |
| 6 | Window setting is live | Change Setup → Display → TX → FFT → Window mid-transmit | Trace skirts visibly change. Rectangular clearly worse than Blackman-Harris 4T | PASS 2026-08-05 |
| 7 | No waterfall banding | Key TUNE, watch the waterfall | Smooth scroll. No alternating horizontal bands of transmit and stale receive | PASS 2026-08-05 |
| 8 | Graticule swaps on key-up | Key TUNE, watch the dBm axis | Range becomes the transmit grid (+20 / -80 seed), returns to the receive range on un-key | PASS 2026-08-05 |
| 9 | Transmit grid persists | Key up, drag the dBm strip until it looks right, un-key, key up again | Second key-up comes back to the dragged range, not the seed. Survives an app restart (`DisplayTxGridRefLevel` / `DisplayTxGridDynamicRange`) | PENDING |
| 10 | Receive grid unharmed | Note the receive range, transmit, un-key | Receive range is exactly what it was. Transmit adjustments do not leak into it | PENDING |
| 11 | RX suppressed on ORION class | On a radio whose RX survives transmit, key up with a receive signal present | Only the transmit trace renders on the transmitting pan. No frame-by-frame alternation between receive and transmit | PENDING |
| 12 | Correct pan on multi-pan | Two pans, slices on both, transmit on the second | Transmit trace and MOX overlay both land on the pan hosting the TX-bound slice, not on the active pan | PENDING |
| 13 | HERMES-class key-up | On a G2E with PureSignal armed, key TUNE | Transmit trace appears. Note the receiver is genuinely gone on this class (`console.cs:8387-8390`), so nothing should fall back to receive | PENDING |
| 14 | Restore after a mid-transmit layout change | Key up, change pan layout while keyed, un-key | No crash, no pan left stuck showing transmit. Restore is by the pan id recorded on the rise edge, so a moved TX binding cannot strand it | PENDING |
| 18 | Grids are genuinely independent | Set a receive range, key up, set a different transmit range, un-key, change receive again, key up | Transmit comes back to ITS range untouched by the receive edits, and vice versa. Two stored pairs selected by MOX (Thetis `SpectrumGridMaxMoxModified`), not one pair saved and restored | UNIT-TESTED, bench re-check |
| 17 | dBm labels survive repeated transmissions | Key TUNE, un-key, key again, watch the right-hand scale | Numbers stay. Regression 2026-08-05: on an ORION-class radio the receive noise-floor tracker kept dragging the grid DURING transmit, the fall edge captured that instead of the operator's choice, and the second key-up came up on a degenerate range with no labels | FIXED, re-check |
| 15 | **Known open:** residual skirt | Key TUNE in LSB into a dummy load, Blackman-Harris 4T, Peak detector, no averaging; compare with Thetis on the same radio and frequency | At least 90 dB down 66 Hz from the peak and beyond. Bench 2026-08-05 read ~35 dB. Not reproduced offline on 2026-09-26: `tst_tx_analyzer_skirt` puts the analyzer's arguments and a real TX channel's TUNE tone through WDSP and reads 125 dB or more down (see the section below) | NOT REPRODUCED OFFLINE, bench re-check |
| 16 | XIT while keyed | Key up, nudge XIT mid-transmission | **Known limitation:** the display does NOT follow. Centre, window and grid are all computed once on the MOX rise edge, so the trace stays where it started until the next key-up. Raised by Codex on PR #317; fixing it properly means a live-update path for the whole rise-edge set, not a special case for XIT | OPEN |
| 19 | Remote transmit trace appears | In a remote window on the Pi 4 Core (HL2) and the Rock Core (G2), key TUNE from any device | The pan hosting the transmit slice shows the transmit trace and waterfall. No receiver hump, no full-width band on the waterfall. Other pans keep receiving | PENDING |
| 20 | Remote trace on frequency | As row 2, in a remote window | Peak `cw_pitch` below the dial in LSB, above in USB, the same as a local window on the same Core's radio | PENDING |
| 21 | Remote overlay and restore | Key and un-key in a remote window; drag the dBm strip while keyed | Red border, transmit grid, transmit palette and transmit waterfall levels while keyed; the receive grid, levels and view exactly as before on un-key; rows 9 and 10 hold in the remote window | PENDING |
| 22 | XIT while keyed, both windows | Key up, change XIT mid-transmission, in a local and a remote window | The trace, the TX filter and the view follow the new carrier within a frame (closes row 16) | PENDING |
| 23 | TX Display settings from a remote window | Change the window type mid-transmission from a remote window's Setup > Display > TX Display | The skirts change as in row 6; a local window on the same Core shows the same setting | PENDING |
| 24 | DUP | Turn DUP on, key TUNE, in a local and a remote window; then off | DUP on: the receiver stays on the pan with the red border and transmit grid. DUP off: the transmit display | PENDING |
| 25 | Transmit monitor to the holder | MON on, transmit from a remote window with MON on speakers, then phones; a second device connected | The holder hears the monitor on the chosen output; the second device hears none | PENDING |
| 26 | CFC bars and PA Values while keyed | Open the CFC dialog and Setup > PA > PA Values in a remote window, transmit with CFC on | The bars move as on the Core's own window; PA Values shows the Core's readings | PENDING |
| 27 | Older Core | A current window on a Core without the transmit display, key TUNE | The pan shows the reason, keeps the red border and transmit grid, and its waterfall holds instead of painting the receiver | PENDING |
| 28 | Remote transmit meters while keyed | In a remote window on the Rock Core (G2) and the Pi 4 Core (HL2), key TUNE; watch the TX applet's RF Pwr and SWR bars and a container's Power, SWR and ALC meters beside a local window on the same Core | The remote bars and meters read what the local window reads (about 4 W forward on the G2's TUNE, found at zero on 2026-09-26) and fall to rest on un-key as the local window's do | PENDING |

---

## Row 15: what is known and what is not

Measured 2026-08-05 straight out of `GetPixels`, before the dBm-to-linear
bridge and before `updateSpectrumLinear`:

```
peak    = -9.3 dBm
±66 Hz  = -43 / -47      (~35 dB down)
±333 Hz = -50 / -59
±1332 Hz= -68 / -70
±4000 Hz= -78 / -79      (~70 dB down)
```

Ruled out: display range (the graticule now spans 100 dB), resolution (2731
bins for 1202 pixels), window type (changing it visibly moves the skirts, and
BH-4T is selected), tone magnitude (`0.99999`, identical to Thetis's
`MAX_TONE_MAG`), and our render path (the skirt exists before it).

### 2026-09-26: the offline comparison (remote-window parity plan, Task 27)

Every argument `TxAnalyzer` hands `SetAnalyzer` and `SetDisplaySampleRate`
now comes from one computation, `TxAnalyzer::currentArgs()` (a
`TxAnalyzerArgs`), which `applySetAnalyzer` passes to WDSP and a test can
read. At TUNE in LSB on a 1200-pixel pan with the MOX edge's +/-4 kHz window
it gives: `n_pixout` 2, `n_fft` 1, `typ` 1, `sz` 32768, `bf_sz` 2048 (the TX
channel's `dsp_size`), window as selected, `pi` 14.0, `ovrlp` 26368, `clp` 0,
`fscLin` 15019, `fscHin` 15018, `n_pix` 1200, `n_stch` 1, `calset` 0, `fmin`
and `fmax` 0, `max_w` 42368, 96000 Hz. Against Thetis v2.10.3.15's
`initAnalyzer` (`specHPSDR.cs:504-643`, the path its transmit panadapter
uses) only the clip arguments differ: Thetis passes `clp` 1310 and span clips
of 12108.8 bins each from its zoom and pan sliders (default zoom 150, pan
500), a +/-8684 Hz view, where NereusSDR clips to its own window the way
`CalcSpectrum` does. The tap is the same as Thetis's: the siphon after the
ALC meter and before PureSignal (`TXA.c:586`), at `dsp_rate` 96000, mode 1,
display 5. One build difference outside the arguments: Thetis builds WDSP
with `_Thetis`, so the analyzer's input ring holds doubles; NereusSDR's holds
floats. That limits nothing near 90 dB.

`tst_tx_analyzer_skirt` then measured, with Blackman-Harris 4T:

```
synthetic tone into Spectrum0, NereusSDR's arguments:
  peak -0.1 dB at -606 Hz; 66 Hz below -130.1 dB, above -134.1 dB;
  worst at or beyond 66 Hz -124.9 dB
real TX channel, TUNE generator, 64-frame blocks as TxWorkerThread pumps:
  the same figures; 30 siphon pushes for 32 DSP blocks fed (the last two
  still in the channel), 61197 sample steps checked, no break in the tone
Thetis's initAnalyzer arguments on the same samples:
  66 Hz -126.8 / -127.2 dB; worst beyond -124.9 dB
```

A real-time run (blocks every 1.33 ms, three key cycles in the MOX edge's
order, the analyzer's own 15 fps poll on the transmit lane) read the same.
The tests are sensitive to the failure: in the same set-up Rectangular reads
-41 dB at 66 Hz and Hamming -58 dB, and a skipped or repeated block breaks the
continuity check.

So on the current trunk nothing on NereusSDR's side of `GetPixels` makes the
skirt, and neither does WDSP's analyzer: no fix was made. Two clues in the
2026-08-05 numbers point at the samples the analyzer was fed rather than at
its set-up. A clean tone reads -0.1 dB at the peak with the Peak detector;
-9.3 dB is what a tone present for about a third of each FFT frame gives
(20 log 1/3 = -9.5 dB). And the skirt fell about 19 dB a decade (66 Hz to
1332 Hz), the slope of hard on/off steps in the signal, which a window cannot
remove.

What the bench re-check needs (the operator's, on the G2 in a local window):

1. Current build, LSB, TX filter 100 to 2900 Hz, TUNE at low power into a
   dummy load. Setup > Display > TX Display: FFT size 32768, Window
   Blackman-Harris 4T, Panadapter detector Peak, averaging None. Note the
   values actually set there if they differ. Record the build's SHA (the
   window title's build name, or `git rev-parse --short HEAD` of the tree it
   was built from). The 2026-08-05 reading and the analyzer block size fix
   (`f8cff594`, `TxAnalyzer::setBlockSize`) carry the same date, and a
   `bf_sz` that does not match the siphon's push size feeds the analyzer
   exactly this kind of gapped input, so a reading without its SHA cannot
   say which side of that fix it was taken on.
2. Launch with `QT_LOGGING_RULES="nereus.dsp.debug=true"` and keep the
   `TxAnalyzer SetAnalyzer` line from the key-up; it should match the
   arguments above with `win=1`.
3. Read the peak and the level 66 Hz either side, and record the tone's peak
   level every time, skirt or not (-0.1 dB expected; near -9 dB means the
   tone is not continuous at the analyzer). If the skirt is gone, row 15
   closes with the SHA and the peak level. If it is still there, take the
   A/B screenshot against Thetis on the same radio, frequency and TUNE power.

Two things about the build under test that bear on the reading:

- **The up-slew stage sits between the tone and the display.** In the TX
  chain (`third_party/wdsp/src/TXA.c:575-578`) the order is `xgen` (gen1,
  the TUNE tone), then `xuslew`, then the ALC meter, then `xsiphon`, which
  feeds the analyzer. A channel state restart or flush during TUNE re-runs
  the up-slew ramp and would show at the analyzer as periodic ramps on the
  tone: gaps of exactly the kind the -9.3 dB peak suggests. If the skirt is
  still there, a debug hook worth adding for the bench is a log line (on
  the `nereus.dsp` category) wherever the TX channel's state is restarted
  or flushed while TUNE is on; one or more per FFT frame names the cause.
- **The first key's SetAnalyzer now runs earlier on the transmit lane.**
  Since parity Task 28, `TxDisplayFeed` starts the analyzer from
  `MoxController::moxStateChanged`, which `onRfDelayElapsed` emits before
  `moxChanged`; the first key's deferred SetAnalyzer (the FFTW plan) is
  therefore posted on the transmit lane ahead of the work `moxChanged`
  handlers post on the same edge. `nereusd` already behaved this way. It is
  not a change to the transmit chain, but a first-key hitch or a
  first-frame oddity on the display belongs with this note.

Not yet ruled out, in the order worth trying:

1. **A/B against Thetis** on the same radio, same frequency, same TUNE
   power. One screenshot decides whether this is our analyzer setup or the
   signal itself. Cheapest decisive test.
2. **A/B our two transforms** on identical data: feed the receive stream
   through both `FFTEngine` and the WDSP analyzer and compare. Isolates the
   analyzer configuration with no radio and no transmit involved.
3. Overlap/advance alignment: `ovrlp=26368` gives a 6400-sample advance
   against 2048-sample pushes, which is not an integer number of blocks.
   Thetis computes overlap the same way, so this is a weak suspect, but it
   has not been positively excluded.
4. The up-slew debug hook above, when the peak reads near -9 dB.

---

## Not in scope, tracked elsewhere

- **TX Grid Scale Setup controls** remain a placeholder (3M-5e). The dBm
  strip is the working control and it persists, so this is polish. When
  built, those spinboxes should read and write `m_txGridRefLevel` /
  `m_txGridDynamicRange` rather than introduce a third source of truth.
- **`setNumPixels` strip-width error**: `n_pix` is the full widget width
  while the trace renders across `width() - effectiveStripW()`, about a 3%
  horizontal scale error. Invisible in practice, still wrong.
- **3M-5 Phases 4-6** (grid scale, appearance colours, cal offset) were never
  started; see `tx-display-settings-master-plan.md`.
