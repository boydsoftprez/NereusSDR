# Phase 3G RX Epic — Sub-epic A Verification Matrix

Manual verification for the right-edge dBm scale strip port. Run through
each row; mark PASS / FAIL. FAIL rows block the PR merge.

| # | Interaction | Expected | Result |
|---|---|---|---|
| 1 | Launch NereusSDR, look at spectrum | dBm labels on the RIGHT edge (not left); two arrow triangles at top of strip (▲▼); grid lines, trace, waterfall, freq scale all fill the remaining width and stop cleanly at the strip's left border | |
| 2 | Click the ▲ up arrow | Ref level increases by 10 dB, labels shift up, trace visibly moves down relative to the grid | |
| 3 | Click the ▼ down arrow | Ref level decreases by 10 dB | |
| 4 | Click ▼ repeatedly until range clamps | Dynamic range stops shrinking at 10 dB (minimum floor); further ▼ clicks have no effect | |
| 5 | Drag vertically on the strip body | Ref level pans with the drag; on release, new range persists | |
| 6 | Hover the arrow row without clicking | Cursor becomes pointing-hand | |
| 7 | Hover the strip body without clicking | Cursor becomes size-vertical | |
| 8 | Hover off the strip | Cursor reverts to whatever existing hover logic chooses (crosshair / filter-edge / slice-marker) | |
| 9 | Wheel up over the strip | Dynamic range narrows in 5 dB steps; labels redistribute adaptively (20/10/5/2 dB step boundaries visible) | |
| 10 | Wheel down over the strip | Dynamic range widens in 5 dB steps | |
| 11 | Wheel over the spectrum body | Bandwidth zoom (existing behaviour — strip wheel should NOT have stolen this) | |
| 12 | Adjust range via strip, quit, relaunch | Range restored to the last user-set value | |
| 13 | Switch bands while on one range | Per-band storage kicks in — returning to the original band restores its last range (Phase 3G-8 pre-existing behaviour — verify strip port didn't break it) | |
| 14 | Change panadapter theme / colour scheme | Strip background stays semi-opaque blue-dark; labels remain legible | |
| 15 | Narrow the widget window width aggressively | Strip stays at 36 px on the right; spectrum squeezes but never overlaps the strip | |
| 16 | Setup → Display → Grid & Scales → uncheck "Show dBm scale strip" | Strip disappears; spectrum trace, grid, waterfall, and freq scale expand to fill the full widget width | |
| 17 | Re-check the toggle, quit, relaunch | Strip reappears; setting persists across restart (per-panadapter `DisplayDbmScaleVisible` key via `settingsKey(..., m_panIndex)`) | |

## Regressions to watch

- Filter-edge drag (spectrum body): must still work unchanged
- Divider-bar drag (between spectrum and waterfall): unchanged
- Bandwidth drag on freq-scale bar: unchanged
- VFO tuning: unchanged
- Off-screen slice indicator: unchanged

## Upstream reference

Ported from AetherSDR `main@0cd4559` (2026-04-21) — file `src/gui/SpectrumWidget.cpp`. Specific line ranges cited inline in each commit's diff:

- `SpectrumWidget.cpp:4856-4925` — strip paint (Task 4, commit `b5deeaf`)
- `SpectrumWidget.cpp:1712-1745` — click-arrow hit-test (Task 5, commit `6777442`)
- `SpectrumWidget.cpp:2115` — drag-end signal emission (Task 5, commit `6777442`)
- `SpectrumWidget.cpp:2241-2248` — hover cursor (Task 6, commit `a088c41`)
- `SpectrumWidget.cpp:2630-2636` — wheel consume (Task 7, commit `f6d9c5b`)
- `SpectrumWidget.cpp:4858` — strip rect geometry (Task 3, commit `41c3e61`)
- `SpectrumWidget.h:539` — `DBM_ARROW_H` constant (Task 4, commit `b5deeaf`)

## Commit sequence

The full implementation landed across 9 commits (after the plan commit `59037d9`):

1. `935667a` — feat(spectrum): add pure dBm-strip geometry helpers
2. `80fdc16` — test(spectrum): unit tests for dBm-strip helpers (7 cases, all pass)
3. `41c3e61` — feat(spectrum): flip dBm strip layout from left to right edge
4. `b5deeaf` — feat(spectrum): port AetherSDR dBm strip visual
5. `6777442` — feat(spectrum): dBm strip arrow clicks + range-change signal
6. `a088c41` — feat(spectrum): hover cursor on dBm strip
7. `f6d9c5b` — feat(spectrum): wheel-zoom dynamic range inside dBm strip
8. `5302746` — feat(mainwindow): persist dBm range from strip to PanadapterModel
9. `37edbe6` — feat(spectrum): DisplayDbmScaleVisible toggle
