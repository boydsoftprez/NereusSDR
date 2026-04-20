# Phase 3P userland mockups

HTML wireframes produced during the brainstorming session for the all-board radio-control parity spec at `../2026-04-20-all-board-radio-control-parity-spec.md`.

Open any file in a browser. Each page is a self-contained mockup with:
- Full Setup-dialog tab path shown at top (matches the actual NereusSDR `HardwarePage` sub-tab structure)
- Wireframed controls with realistic data
- Phase tag annotations (A–H) showing which phase delivers each piece
- "Spin from Thetis" notes calling out NereusSDR-native deviations from the Thetis source

## Pages

| File | Maps to spec section | Notes |
|---|---|---|
| `sitemap-v2-real.html` | §2 phase roadmap | Side-by-side: Thetis Setup tree vs NereusSDR Setup tree, with Phase A–H landing points tagged in-place |
| `page-calibration.html` | §12 Phase G | Hardware → Calibration (renamed from PA Calibration); 5 group boxes modeled 1:1 on Thetis General → Calibration |
| `page-antenna-control.html` | §11 Phase F | Hardware → Antenna / ALEX → Antenna Control sub-sub-tab; per-band TX/RX1/RX2 antenna grid + Block-TX safety |
| `page-alex1-filters.html` | §7 Phase B | Hardware → Antenna / ALEX → Alex-1 Filters sub-sub-tab; HPF + LPF + Saturn BPF1 panels |
| `page-alex2-filters.html` | §7 Phase B | Hardware → Antenna / ALEX → Alex-2 Filters sub-sub-tab; live LED filter indicators |
| `page-oc-outputs.html` | §9 Phase D | Hardware → OC Outputs → HF; full 14×7×2 matrix + TX pin actions + USB BCD + ext PA + live pin state |
| `page-hl2-io.html` | §10 Phase E | Hardware → HL2 I/O (was empty placeholder); register table + 12-step state machine + I2C log + bandwidth mini |
| `page-rxapplet-evolution.html` | §6 / §7 / §8 / §11 | Per-receiver UI strip — today vs after Phase A–F on HL2 and Orion-MKII |
| `page-radio-status.html` | §13 Phase H | Diagnostics → Radio Status; PA / Power / PTT / Connection / Hygiene cards |

## Conventions used in the wireframes

- Phase tags color-coded:
  - A = red (P1 wire bytes)
  - B = orange (P2 + Alex filter UI)
  - C = yellow (preamp combo)
  - D = green (OC matrix)
  - E = blue (HL2 IoBoard)
  - F = purple (accessories)
  - G = pink (calibration)
  - H = grey (status / hygiene)
- Existing controls greyed; new content highlighted; gated/board-specific controls dimmed with explanatory caption
- Live LED rows on Alex-2 / OC / HL2 pages indicate Phase H wires up the live data; the layout itself ships in the earlier phase

## Status

Frozen as design reference for Phase 3P. Not the final visual design — the engineering team picks fonts, exact spacing, and chrome during implementation. These wireframes lock the **information architecture**, **page density**, **control inventory per page**, and **NereusSDR-vs-Thetis IA decisions** documented in the spec.
