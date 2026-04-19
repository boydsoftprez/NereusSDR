# Third-Party License Notices

NereusSDR binaries are shipped under GPL-3.0 (see root `LICENSE`). The
binary conveys a combined work that links or vendors the dependencies
listed below, each under its own licence. Full licence text for each
licence that governs code compiled into the binary ships in this
directory alongside NereusSDR's own `LICENSE`.

## Dependencies compiled into the binary

| Dependency | Role | Licence | Notice file | Full licence text |
| --- | --- | --- | --- | --- |
| NereusSDR | application | GPL-3.0 | (see root `LICENSE`) | `GPLv3.txt` |
| Qt 6 | GUI / network / audio framework | LGPL-3.0 (dynamic linking) | `qt6.txt` | `LGPLv3.txt` (+ `GPLv3.txt` by LGPL §4 reference) |
| FFTW3 | FFT library | GPL-2.0-or-later | `fftw3.txt` | `GPLv2.txt` |
| WDSP | DSP engine | GPL-2.0-or-later | `wdsp.txt` | `GPLv2.txt` |

## Upstream projects whose code is ported into the binary

| Upstream | How used | Licence | Notice file |
| --- | --- | --- | --- |
| Thetis (ramdor/Thetis) | C# → C++/Qt6 port (logic, DSP integration, protocol, UI behaviour) | GPL-2.0-or-later | `thetis.txt` |
| mi0bot/Thetis-HL2 fork | HL2-specific discovery + capability ports | GPL-2.0-or-later | `mi0bot-thetis.txt` |
| AetherSDR (ten9876/AetherSDR) | Qt6 architectural template; ~45 derived files | GPL-3.0 | `aethersdr.txt` |

Per-file attribution for ported code lives in the source-file headers
and is indexed in `docs/attribution/THETIS-PROVENANCE.md`,
`docs/attribution/aethersdr-reconciliation.md`, and
`docs/attribution/WDSP-PROVENANCE.md` inside the NereusSDR source tree.

## Corresponding Source

A binary recipient's source-code rights under GPLv3 §6 / GPLv2 §3 are
detailed in `SOURCE-OFFER.txt`, which points at the public git tag
matching this binary and extends a three-year written source offer as
the §6(b) fallback.

## Combination of licences

- NereusSDR's own code: GPL-3.0.
- GPL-2.0-or-later dependencies (Thetis-derived ports, mi0bot-derived
  ports, WDSP, FFTW3): combined with NereusSDR under GPL-3.0 by
  exercising the "or later" grant (GPLv3 §5(b)).
- Qt 6 dynamic linking: permitted under LGPLv3 §4 when the combined
  work conveys (a) a prominent notice that the library is used and
  covered by LGPLv3 (this file), (b) a copy of the LGPLv3 text
  (`LGPLv3.txt`), and (c) a mechanism for the user to relink with a
  modified Qt (dynamic linking + the upstream Qt source pointer in
  `SOURCE-OFFER.txt` §3).

## File inventory

- `README.md`           — this index
- `GPLv2.txt`           — full GNU General Public License, version 2
- `GPLv3.txt`           — full GNU General Public License, version 3
- `LGPLv3.txt`          — full GNU Lesser General Public License, version 3
- `qt6.txt`             — Qt 6 dependency notice
- `fftw3.txt`           — FFTW 3 dependency notice
- `wdsp.txt`            — WDSP dependency notice
- `thetis.txt`          — Thetis upstream-port notice
- `mi0bot-thetis.txt`   — mi0bot/Thetis-HL2 upstream-port notice
- `aethersdr.txt`       — AetherSDR upstream-port notice
- `SOURCE-OFFER.txt`    — written source offer + corresponding-source pointers

This directory is installed to:

- Linux AppImage: `AppDir/usr/share/doc/nereussdr/licenses/`
- macOS DMG:      `NereusSDR.app/Contents/Resources/licenses/`
- Windows NSIS:   `$INSTDIR\licenses\`
- Windows ZIP:    `deploy\licenses\`

…via `install(DIRECTORY packaging/third-party-licenses/ ...)` in the
root `CMakeLists.txt` and matching copy steps in
`.github/workflows/release.yml`.
