# About Dialog — Design Spec

**Date:** 2026-04-16
**Status:** Draft

---

## Goal

Replace the current one-liner `QMessageBox::about()` with a proper About
dialog that credits the open-source projects and people NereusSDR stands on,
and gives the author (JJ Boyd, KG4VCF) prominent recognition.

## Architecture

A new `AboutDialog` class in `src/gui/AboutDialog.h` and
`src/gui/AboutDialog.cpp`, inheriting `QDialog`. Follows the existing dialog
pattern (see `SupportDialog`, `AddCustomRadioDialog`): constructor takes a
`QWidget* parent`, private `buildUI()` method constructs all layout, dark
theme styling applied inline.

The existing `QMessageBox::about()` lambda in `MainWindow.cpp` (Help menu,
line ~1274) is replaced with an `AboutDialog` instantiation + `exec()`.

## Layout — Single Scrollable Page

Fixed width ~420px, height sized to content. Four sections top-to-bottom:

### 1. Header (centered)

- App icon (64×64, from existing `resources/icons/`)
- **"NereusSDR"** — large bold title, `#00b4d8`
- Version string from `QCoreApplication::applicationVersion()`
- "Cross-platform SDR Console"
- Build info line: `Qt <runtime-version> · C++20 · Built <__DATE__>`
- **"Created by JJ Boyd · KG4VCF"**

### 2. "Standing on the Shoulders of Giants" (contributors)

Each entry is: `Name, CALLSIGN — ProjectName` where the project name is a
clickable `QLabel` with rich text (`<a href="...">`) that opens the URL via
`QDesktopServices::openUrl()`.

Order:

| # | Name                       | Callsign | Project (link)                | URL                                          |
|---|----------------------------|----------|-------------------------------|----------------------------------------------|
| 1 | Richie                     | MW0LGE   | Thetis                        | https://github.com/ramdor/Thetis             |
| 2 | Jeremy                     | KK7GWY   | AetherSDR                     | https://github.com/ten9876/AetherSDR         |
| 3 | Reid Campbell              | MI0BOT   | OpenHPSDR-Thetis (HL2)        | https://github.com/mi0bot/OpenHPSDR-Thetis   |
| 4 | Dr. Warren C. Pratt        | NR0V     | WDSP                          | https://github.com/TAPR/OpenHPSDR-wdsp       |
| 5 | John Melton                | G0ORX    | WDSP Linux Port               | https://github.com/g0orx/wdsp                |
| 6 | TAPR / OpenHPSDR Community | —        | OpenHPSDR                     | https://github.com/TAPR                      |

### 3. "Built With" (libraries)

Horizontal row of three cards, each with a library name and one-line
description:

| Library    | Description              |
|------------|--------------------------|
| Qt 6       | GUI, Networking & Audio  |
| FFTW3      | Spectrum & Waterfall FFT |
| WDSP v1.29 | DSP Engine               |

Card styling: `#1a2a3a` background, `6px` border-radius, library name in
`#c8d8e8` bold, description in `#aabbcc`.

### 4. Footer (centered)

- `© 2026 JJ Boyd`
- "Licensed under GPLv3" — clickable link to
  `https://www.gnu.org/licenses/gpl-3.0.html`
- `github.com/boydsoftprez/NereusSDR` — clickable link
- "HPSDR protocol © TAPR"

## Styling

Dark theme matching NereusSDR palette:

| Element          | Color     |
|------------------|-----------|
| Dialog background | `#0f0f1a` |
| Card background   | `#1a2a3a` |
| Accent / links    | `#00b4d8` |
| Primary text      | `#c8d8e8` |
| Secondary text    | `#8899aa` |
| Dividers          | `#334455` |

Section headings use the accent color, 13px, semi-bold.

## Integration

- New files: `src/gui/AboutDialog.h`, `src/gui/AboutDialog.cpp`
- Modified file: `src/gui/MainWindow.cpp` — replace `QMessageBox::about()`
  lambda with `AboutDialog` instantiation
- Modified file: `CMakeLists.txt` — add new source files to the build
- No new dependencies, no settings persistence needed

## What This Is NOT

- No version-check / update mechanism (can be added later)
- No dynamic GitHub contributor fetching (hardcoded list is sufficient)
- No tabs — single scrollable page
- No diagnostic buttons (SupportDialog already covers that)
