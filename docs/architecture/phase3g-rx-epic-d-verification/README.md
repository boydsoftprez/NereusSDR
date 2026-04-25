# Phase 3G RX Epic Sub-epic D — Bandplan Overlay Verification

12-row manual verification matrix for the bandplan port. Walk every row
on a connected radio (or the demo I/Q feed) before opening the PR.

| # | Action | Expected | Pass? |
|---|---|---|---|
| 1 | Launch app fresh (delete `~/.config/NereusSDR/NereusSDR.settings`), connect radio | Bandplan strip visible at bottom of spectrum on first launch; default region "ARRL (US)" | |
| 2 | Tune to 7.000 MHz on 40m | "CW" segment label visible at left edge of strip | |
| 3 | Pan to 7.150 MHz | Label transitions through "DATA" → "PHONE" as the segment boundaries cross center | |
| 4 | Tune to 14.225 MHz on 20m | "PHONE" + "General" suffix visible (license-class label kicks in for segments > 60 px wide) | |
| 5 | Hover near 14.074 MHz | White spot marker dot visible on the strip at the FT8 calling frequency | |
| 6 | Setup → Display → Bandplan → Region = "IARU Region 1" | Strip relabels live; segment count + boundaries change to R1 plan | |
| 7 | Setup → Display → Bandplan → Region = "IARU Region 2" / "IARU Region 3" / "RAC (Canada)" | All four non-default regions load and render | |
| 8 | Setup → Display → Bandplan → Label size = 0 (Off) | Strip disappears entirely; spectrum extends to bottom of widget | |
| 9 | Setup → Display → Bandplan → Label size = 12 pt | Strip thickens; labels render larger | |
| 10 | Quit, relaunch | Region + label size restored; same view as before quit | |
| 11 | Resize the SpectrumWidget very narrow (~200 px wide) | Labels disappear from segments < 20 px; strip itself still paints; no crash | |
| 12 | Switch panadapter colour scheme (overlay menu) | Bandplan strip background stays at fixed dark blue (`#0a0a14`); segment colours unchanged | |

## Regressions to watch

- dBm scale strip: still on right edge, behaviour unchanged
- Filter passband shading: unchanged
- VFO marker + cursor info: unchanged
- Off-screen slice indicator: unchanged
- FPS overlay: unchanged

## Deferred (explicit non-goals for this sub-epic)

- **Locale-driven default region**: design §D mentions auto-selecting from user locale; AetherSDR hardcodes "ARRL (US)" and we matched upstream. Revisit if user demand justifies the platform-specific code.
- **User-editable bandplans**: design §D explicitly defers — bundled JSONs only.
- **Custom colour overrides**: per-mode colour customisation deferred; segments use the JSON-baked colours.

## Upstream reference

Ported from AetherSDR `main@0cd4559` (2026-04-21):
- [`src/models/BandPlanManager.{h,cpp}`](https://github.com/ten9876/AetherSDR/blob/0cd4559/src/models/BandPlanManager.cpp)
- [`src/gui/SpectrumWidget.cpp:4220-4293`](https://github.com/ten9876/AetherSDR/blob/0cd4559/src/gui/SpectrumWidget.cpp) (`drawBandPlan`)
- [`resources/bandplans/*.json`](https://github.com/ten9876/AetherSDR/tree/0cd4559/resources/bandplans) (5 region files)
