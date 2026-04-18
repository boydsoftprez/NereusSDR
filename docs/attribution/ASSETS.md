# NereusSDR Asset Provenance

Inventory of every graphical / binary asset shipped in NereusSDR.
Per-asset origin, author, license, and the date first added to the repo.

All assets listed here are GPL-2.0-or-later unless noted otherwise.

---

## Fonts

### Style-7 Digital-7 Font (Not Used)

Thetis `console.cs` credits "Sizenko Alexander of Style-7 (http://www.styleseven.com/) for the Digital-7 font."

**Status in NereusSDR:** The Digital-7 / Style-7 font is **NOT used** anywhere in NereusSDR.
All meter labels, VFO displays, clocks, and textual overlays use native Qt font rendering
with system or built-in typefaces. No `.ttf` or `.otf` files are present in `resources/`.

Therefore, preservation of the Thetis Style-7 credit is not required for NereusSDR;
the font credit does not propagate to this derivative work.

---

## resources/meters/

| File | Origin | Author | License | Added |
| --- | --- | --- | --- | --- |
| `ananMM.png` | Original NereusSDR artwork designed by J.J. Boyd (KG4VCF) using AI image-generation tooling. 1360×768 PNG produced specifically for NereusSDR; no ANAN / Apache Labs / OE3IDE bitmap was used as input, reference, or training material; no Ernst-style (Digital-7) typography is present. | J.J. Boyd (KG4VCF) | GPL-2.0-or-later | 2026-04-17 |
| `cross-needle.png` | Original NereusSDR artwork designed by J.J. Boyd (KG4VCF) using AI image-generation tooling. 1104×928 PNG. Prior to the compliance remediation this file was a byte-identical placeholder copy of `ananMM.png` (commit `84f77e8`, 2026-04-11); the placeholder was never shipped in a release and has been replaced with this original artwork. No Thetis / OE3IDE skin image was used as input or reference. | J.J. Boyd (KG4VCF) | GPL-2.0-or-later | 2026-04-17 |
| `cross-needle-bg.png` | Original NereusSDR artwork designed by J.J. Boyd (KG4VCF) using AI image-generation tooling. 1104×928 PNG, same source as `cross-needle.png`; the two files exist so the rendering code's two-layer image path continues to function. | J.J. Boyd (KG4VCF) | GPL-2.0-or-later | 2026-04-17 |

### Source artwork files

The JPG masters from which the shipping PNGs were exported are
committed alongside for reference and direct byte-for-byte inspection.
Both were designed by J.J. Boyd (KG4VCF) using AI image-generation
tooling, specifically for NereusSDR; neither derives from, references,
or reproduces any ANAN / Apache Labs / OE3IDE / Thetis-skin asset:

- `docs/attribution/source-artwork/NereusMeter.jpg` — 1360×768 JPG master for `ananMM.png`
- `docs/attribution/source-artwork/NereusMeter-Dual.jpg` — 1104×928 JPG master for `cross-needle.png` and `cross-needle-bg.png`

The PNGs under `resources/meters/` are deterministic `sips` exports of
these JPG masters (lossless pixel-for-pixel conversion); re-running
`sips -s format png <source.jpg> --out <target.png>` reproduces the
exact shipping PNG byte-for-byte.

### Provenance of earlier placeholders

Interim programmatic generators (`tools/generate-meter-face.py`,
`tools/generate-cross-needle.py`) were used during the compliance sweep
before the AI-designed artwork was final. They have been removed from
the repository now that the AI-designed artwork is in place; the
shipping artwork above is authoritative.

---

## resources/icons/

All NereusSDR brand icons are original artwork. They were created specifically
for the NereusSDR project and do not derive from any upstream SDR project's
branding.

| File | Origin | Author | License | Added |
| --- | --- | --- | --- | --- |
| `NereusSDR.png` | Original NereusSDR application icon | J.J. Boyd (KG4VCF) | GPL-2.0-or-later | 2026-04-13 (commit `c25b2f3`) |
| `NereusSDR.ico` | Windows ICO version of the same icon | J.J. Boyd (KG4VCF) | GPL-2.0-or-later | 2026-04-13 (commit `c25b2f3`) |
| `NereusSDR.icns` | macOS ICNS bundle version of the same icon | J.J. Boyd (KG4VCF) | GPL-2.0-or-later | 2026-04-13 (commit `c25b2f3`) |
| `NereusSDR.iconset/icon_16x16.png` | 16×16 variant | J.J. Boyd (KG4VCF) | GPL-2.0-or-later | 2026-04-13 |
| `NereusSDR.iconset/icon_16x16@2x.png` | 32×32 HiDPI variant | J.J. Boyd (KG4VCF) | GPL-2.0-or-later | 2026-04-13 |
| `NereusSDR.iconset/icon_32x32.png` | 32×32 variant | J.J. Boyd (KG4VCF) | GPL-2.0-or-later | 2026-04-13 |
| `NereusSDR.iconset/icon_32x32@2x.png` | 64×64 HiDPI variant | J.J. Boyd (KG4VCF) | GPL-2.0-or-later | 2026-04-13 |
| `NereusSDR.iconset/icon_128x128.png` | 128×128 variant | J.J. Boyd (KG4VCF) | GPL-2.0-or-later | 2026-04-13 |
| `NereusSDR.iconset/icon_128x128@2x.png` | 256×256 HiDPI variant | J.J. Boyd (KG4VCF) | GPL-2.0-or-later | 2026-04-13 |
| `NereusSDR.iconset/icon_256x256.png` | 256×256 variant | J.J. Boyd (KG4VCF) | GPL-2.0-or-later | 2026-04-13 |
| `NereusSDR.iconset/icon_256x256@2x.png` | 512×512 HiDPI variant | J.J. Boyd (KG4VCF) | GPL-2.0-or-later | 2026-04-13 |
| `NereusSDR.iconset/icon_512x512.png` | 512×512 variant | J.J. Boyd (KG4VCF) | GPL-2.0-or-later | 2026-04-13 |
| `NereusSDR.iconset/icon_512x512@2x.png` | 1024×1024 HiDPI variant | J.J. Boyd (KG4VCF) | GPL-2.0-or-later | 2026-04-13 |
| `NereusSDR.rc` | Windows resource script — references `NereusSDR.ico` for the app icon | J.J. Boyd (KG4VCF) | GPL-2.0-or-later | 2026-04-13 (commit `c25b2f3`) |

---

## resources/shaders/

GLSL shaders for Qt6 QRhi-based spectrum, waterfall, and meter rendering.
Most are original NereusSDR work; one (`waterfall.frag`) inherits its
ring-buffer UV-offset sampling pattern from AetherSDR
(ten9876/AetherSDR, GPLv3) and is annotated as such in its file header
per `docs/attribution/HOW-TO-PORT.md` rule 6 (project-level citation —
AetherSDR has no per-file shader headers to copy verbatim). No shader
source was ported from Thetis (which uses GDI+/WinForms rendering).

| File | Purpose | License |
| --- | --- | --- |
| `overlay.frag` | Fragment shader for the SpectrumWidget overlay layer (band edges, grid lines, VFO markers) | GPL-2.0-or-later |
| `overlay.vert` | Vertex shader for the overlay layer | GPL-2.0-or-later |
| `spectrum.frag` | Fragment shader for the spectrum trace (filled gradient trace + peak hold) | GPL-2.0-or-later |
| `spectrum.vert` | Vertex shader for spectrum trace geometry | GPL-2.0-or-later |
| `waterfall.frag` | Fragment shader for the waterfall display (color-maps dBm to palette); UV-fract sampling pattern derived from AetherSDR `texturedquad.frag` (GPLv3) | GPL-2.0-or-later (NereusSDR contributions) + GPLv3 (AetherSDR-derived line) |
| `waterfall.vert` | Vertex shader for waterfall geometry | GPL-2.0-or-later |
| `meter_geometry.frag` | Fragment shader for MeterWidget vector-geometry pipeline (bar meters, needle arcs) | GPL-2.0-or-later |
| `meter_geometry.vert` | Vertex shader for meter geometry pipeline | GPL-2.0-or-later |
| `meter_textured.frag` | Fragment shader for MeterWidget textured pipeline (background images: ananMM, cross-needle) | GPL-2.0-or-later |
| `meter_textured.vert` | Vertex shader for meter textured pipeline | GPL-2.0-or-later |

---

## resources/help/

In-app help content shipped through the Qt resource bundle (`qrc:/help/`).

| File | Purpose | License |
| --- | --- | --- |
| `getting-started.md` | First-launch help text shown via the Help menu / Welcome flow | GPL-2.0-or-later (NereusSDR original work) |

---

## resources/bandplans/

Band-plan data files are derived from publicly published IARU and ARRL band-plan
documents. Frequency ranges and mode allocations are facts not subject to
copyright. The JSON arrangement, key naming, and encoding are original NereusSDR
work.

| File | Source data | License |
| --- | --- | --- |
| `iaru-region1.json` | IARU Region 1 band plan (https://www.iaru-r1.org/reference/band-plans/) | GPL-2.0-or-later (arrangement) |
| `iaru-region2.json` | IARU Region 2 band plan (https://www.iaru-r2.org/) | GPL-2.0-or-later (arrangement) |
| `iaru-region3.json` | IARU Region 3 band plan (https://www.iaru-r3.org/) | GPL-2.0-or-later (arrangement) |
| `arrl-us.json` | ARRL US Amateur Radio Band Plan (https://www.arrl.org/band-plan) | GPL-2.0-or-later (arrangement) |

---

## docs/images/

| File | Origin | Author | License | Added |
| --- | --- | --- | --- | --- |
| `nereussdr-v016-screenshot.jpg` | Screenshot of NereusSDR v0.1.6 running on macOS, captured by the maintainer | J.J. Boyd (KG4VCF) | GPL-2.0-or-later | 2026-04-16 (commit `45fc9a0`) |

---

## tests/fixtures/discovery/

See `tests/fixtures/discovery/README.md` for per-file capture provenance.

OpenHPSDR discovery reply packets contain only wire-protocol data (MAC
address, firmware version, radio model byte, in-use flag). These are
factual data values with no creative expression; they are not subject to
copyright. The arrangement and annotation of the hex fixtures are original
NereusSDR work (GPL-2.0-or-later).

| File | Radio / source |
| --- | --- |
| `p1_angelia_reply.hex` | Captured from a real Apache Labs ANAN-100D (Angelia board, firmware 21) running OpenHPSDR Protocol 1 |
| `p1_hermeslite_reply.hex` | Captured from a real Hermes Lite 2 (firmware 72) running OpenHPSDR Protocol 1 |
| `p2_saturn_reply.hex` | Hand-crafted from the OpenHPSDR Protocol 2 spec (no live ANAN-G2 capture available at time of authorship; see README for review note) |
