# Phase 3M-5 TX Display — bench verification matrix

**Branch:** `claude/tx-display-revive`
**Status:** rows 1-8 verified on an ANAN-7000DLE (OrionMkII) 2026-08-05.
Rows 9-14 pending. Rows 15 and 16 are known open items, not tests.

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
| 15 | **Known open:** residual skirt | Key TUNE, read the raw pixel profile | ~35 dB down at 66 Hz from the peak, which is far worse than Blackman-Harris 4T should give (>90 dB). Present in WDSP's raw `GetPixels` output, so it is upstream of everything this branch fixes | OPEN |
| 16 | XIT while keyed | Key up, nudge XIT mid-transmission | **Known limitation:** the display does NOT follow. Centre, window and grid are all computed once on the MOX rise edge, so the trace stays where it started until the next key-up. Raised by Codex on PR #317; fixing it properly means a live-update path for the whole rise-edge set, not a special case for XIT | OPEN |

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
