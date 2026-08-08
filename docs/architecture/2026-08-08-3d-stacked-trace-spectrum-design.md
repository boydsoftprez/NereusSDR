# 3D Stacked-Trace Spectrum (3DSS) Design

**Status:** Approved (brainstorm complete, plan not yet written)
**Date:** 2026-08-08
**Author:** J.J. Boyd (KG4VCF), with AI-assisted drafting via Anthropic Claude Code
**Upstream reference:** AetherSDR `upstream/main` at `1872028c` (`v26.8.1-39`)

---

## 1. Goal

Bring AetherSDR's 3D stacked-trace spectrum view ("3DSS") to NereusSDR as a
per-panadapter display mode, with every upstream control, plus one
NereusSDR-original control that upstream does not have: an adjustable viewing
angle.

3DSS replaces **only the spectrum trace pane**. Quoting the upstream source at
`SpectrumWidget.cpp:13157 [@1872028c]`:

> "3DSS replaces only the spectrum trace: the surface fills specRect and the
> waterfall, divider, freq scale, and all overlays keep their normal 2D
> positions."

The flat scrolling waterfall, the frequency scale, the divider and every
overlay keep running unchanged beneath it. This is not a 3D replacement for
the waterfall.

Visually: instead of one live FFT trace, the pane shows a rolling history of
the last 96 traces drawn back to front as a receding trapezoid. The newest
spans the full width across the front; older ones narrow and rise toward a
vanishing point. Each ridge is filled down to the plot floor so nearer traces
occlude farther ones, fill colour follows amplitude, and depth adds
atmospheric haze.

---

## 2. Decisions taken

| # | Decision | Choice |
|---|---|---|
| 1 | Scope | Full parity in one epic |
| 2 | Method | Reference-guided reimplementation, not transliteration |
| 3 | 3D Span data source | Off-screen DDC bins from our own FFT |
| 4 | Scroll motion | Continuous glide, burst distance fixed at one row |
| 5 | Overlays in 3D | Match upstream: flat, plus optional slice shadow decal |
| 6 | Viewing angle | One combined slider, NereusSDR-original |

### 2.1 Why reimplementation rather than transliteration

The upstream 3DSS code splits into two halves that want opposite treatment.

The half that must be identical is small: the perspective geometry constants
and projection functions (`DssRenderer.h:45-51` and `:85-187 [@1872028c]`) plus
the ridge height mapping in `dss_mesh.vert:157-171 [@1872028c]`. Those are what
make the view read as AetherSDR's 3D rather than some other stacked-trace
display. They are lifted verbatim.

The much larger half is Flex-client-shaped, not 3D-shaped:
`pushRowWithSupplemental`, `reprojectFrequencyFrame`,
`rebuildDssViewportFromHistoryForFrame`, `flexDssFftScaleSettling`,
`armDssZoomFloorSyncAfterSettle`, `syncDssRangeFromFreshZoomFrame`, the
KiwiSDR source branches, and in the shader the whole
`scrollProgressRows` / `scrollDistanceRows` / `boundaryRow` / `vLayerAlpha`
burst crossfade. All of it exists because rows arrive from a radio
asynchronously, stamped with a centre and bandwidth that can lag a retune, and
smooth scroll has to interpolate between tile deliveries. NereusSDR generates
rows locally from its own FFT at a centre and bandwidth it already knows.

Transliterating that half means carrying state machines that can never enter
most of their states, and then debugging them.

Supporting evidence from the upstream changelog: the recurring 3D defects are
rear edge during delayed row arrivals, floor resync after bandwidth zoom, flat
traces after dBm scale changes, DC-edge comb, wide right-edge artifact. Those
are delivery bugs. NereusSDR would not have them, and could not cherry-pick the
fixes into transliterated code without also inheriting the conditions that
caused them.

### 2.2 The three tiers

Every file in this epic declares which tier it is in.

| Tier | Treatment | Covers |
|---|---|---|
| 1 | Lift verbatim, cited | Geometry constants, `depthScale`, `projectPerspective`, `projectSurface`, `rowSpanFactorFor`, `wedgeFreeDepth`, `rowScreenCoverage`, shader ridge and colour mapping, `dssDepthVisibleSegments` occlusion test |
| 2 | Same class shape and public surface, bodies rewritten | Row ring store, GPU mesh pipeline, palette LUT upload, 3D dBm scale, slice shadow decals |
| 3 | NereusSDR-original | Row cadence, floor anchoring, zoom handling, span feed, angle control, control wiring, persistence |

Tier 2 keeping upstream's class boundaries and public surface is deliberate.
When the surface looks wrong we open their file beside ours and the functions
line up. That is most of the value of a port without the cost of one.

### 2.3 Tier 1 deviations are enumerated, not open-ended

"Verbatim" tier 1 carries exactly two intentional deviations, and no others:

1. **Angle parameterisation (section 5).** The three geometry constants become
   runtime values with upstream's numbers as defaults. The shader is untouched
   by this; it already receives them as uniforms.
2. **Scroll distance fixed at one row (decision 4).** `scrollDistanceRows` and
   the multi-row burst handling drop out of the shader; `scrollProgressRows`
   and the boundary-row crossfade stay.

Each deviation site inside otherwise-lifted code carries the in-region
modification marker `HOW-TO-PORT.md` prescribes:
`//-KG4VCF [vX.Y.Z] <description>`. Any further tier 1 edit discovered during
implementation gets added to this list in the same commit, or it does not
happen.

---

## 3. Architecture

### 3.1 Files

| File | Tier | Contents |
|---|---|---|
| `src/gui/DssRenderer.h` | 1 + 2 | Geometry constants and projection functions verbatim. Ring store declaration reshaped for our two channels. |
| `src/gui/DssRenderer.cpp` | 2 | Row ingest, peak-preserving downsample, smoothing, CPU fallback surface. |
| `src/gui/DssMeshGeometry.h` | 3 | Mesh vertex generation. Header-only and QRhi-free so it is unit-testable without a graphics context. |
| `resources/shaders/dss_mesh.vert` | 1 | Ridge, colour and perspective mapping. Burst-distance uniforms dropped, scroll phase kept. |
| `resources/shaders/dss_mesh.frag` | 1 | Haze, ribbon antialiasing, shadow decals. |
| `src/gui/SpectrumWidget.{h,cpp}` | 3 | Mode switch, GPU resources, control wiring, persistence. |
| `src/gui/SpectrumOverlayMenu.{h,cpp}` | 3 | The `3D VIEW` control section. |

`DssRenderer` stays a standalone class that knows nothing about QRhi or
`SpectrumWidget`, exactly as upstream has it, so it remains headlessly
testable.

### 3.2 Where the mesh pass sits

`SpectrumWidget::renderGpuFrame()` currently runs four passes: waterfall,
spectrum trace, static overlay, dynamic overlay.

In 3D mode the DSS mesh pass takes the spectrum trace pass's slot, scissored to
`specRect`. Everything downstream is untouched. This is what makes decision 5
(overlays stay flat) nearly free: the overlay textures composite over the
surface exactly as they composite over the 2D trace today.

### 3.3 GPU resources

Two per panadapter:

- an `RGBA16F` ring texture, `kCols` (768) by `kRows` (104)
- a 256-entry palette lookup texture baked per section 4.3

`kRows` stays at upstream's 96 visible plus 8 transition rows even though our
burst distance is fixed at one and could get by with fewer transition rows.
Keeping the reference dimensions keeps every ring-index formula comparable
line-for-line with upstream, and the cost of the spare rows is about 48 KB per
panadapter.

Plus a UBO and a mesh vertex buffer. Upstream guards on
`isTextureFormatSupported(RGBA16F)` and falls back to the CPU surface
(`SpectrumWidget.cpp:12655 [@1872028c]`); we inherit that guard, which also
covers `-DNEREUS_GPU_SPECTRUM=OFF` builds.

The CPU fallback surface is roughly 150 lines (`DssRenderer.cpp:781-889`
plus `image()` and the occlusion test at `:890 [@1872028c]`), not a second
renderer.

### 3.4 Per-panadapter

Each `SpectrumWidget` owns its own `DssRenderer`, its own mode, and its own
control values, keyed by pan index through the existing
`settingsKey(base, panIndex)` path. One panadapter can be 3D while another
stays 2D. A 2D pan allocates no DSS resources.

---

## 4. Data flow

### 4.1 Row ingest

`WaterfallTicker` already pushes waterfall rows on a dedicated worker thread
with a precise timer and elevated QoS (`SpectrumWidget.cpp:443-475`), at
`DisplayWfUpdatePeriodMs` cadence (default 30 ms), independent of both FFT
arrival timing and `DisplaySpectrumFps`.

`pushWaterfallRow()` gains a second call into `DssRenderer::pushRow`,
**downstream of the stop-on-TX gate** at `SpectrumWidget.cpp:4728`. Same tick,
same cadence, same gate, so the 3D stack and the flat waterfall beneath it
advance and freeze in lockstep by construction. Teeing at the ticker callback
instead would sit upstream of that gate, and during TX the waterfall would
freeze while the 3D stack kept scrolling. Upstream fought for pane alignment
repeatedly across several releases; NereusSDR gets it because one timer and one
gate drive both.

### 4.2 Two channels

The exact and wide channels are both kept, but not for upstream's reason.

Upstream needs two because its exact FFT and its native FLEX waterfall tile are
different measurements at different resolutions and calibrations, so the exact
one must win wherever both exist. NereusSDR has a single measurement, so that
arbitration collapses to a bin-range test.

The channels stay anyway because of **resolution**:

- **exact channel:** 768 columns across the viewport, so the region being
  looked at keeps full resolution
- **wide channel:** 768 columns across `rowSpanFactor` times the viewport,
  sampled only out in the overhang

Collapsing to a single wide channel would cost up to `rowSpanFactor` times
worse resolution on screen. Per-column coverage bytes are also kept, because
they carry zoom-created gaps in retained rows.

Both channels are filled from one FFT frame. Bins inside `visibleBinRange()`
(`SpectrumWidget.cpp:4220`) fill the exact channel.

The wide channel does **not** store the full DDC row. At a deep zoom that
would spread 768 columns across the whole DDC (a 10 kHz view from a 768 kHz
DDC would get roughly 1 kHz per column) to feed an overhang that can never use
more than `kMaxRowSpanFactor` times the viewport. Instead it stores a window
of `kMaxRowSpanFactor(t = 0)` times the viewport bandwidth, centred on the
view, clamped to the DDC extent, with coverage bytes zero wherever the DDC has
no data. Sizing the window for the **widest angle** rather than the current
one keeps stored rows valid across runtime angle changes, so moving the angle
slider never requires re-ingesting history.

At full DDC width there is nothing outside the view, and the 3D Span control
correctly reports no span available.

### 4.3 Floor and colour

`dssFloorDbm()` is `NoiseFloorTracker::noiseFloor()`
(`src/core/NoiseFloorTracker.h:74`, itself a Thetis `display.cs:4628` port)
minus the 3D Floor value. That is the strength-zero baseline. Range is the
current dBm display span.

Colour is independent of height and reads through a fixed 45 dB aperture
(`kColorSpanDb`, `DssRenderer.h:51 [@1872028c]`), so a high Ref level cannot
compress every real signal into blue.

The 256-entry LUT is deliberately **not** built from `dbmToRgb()`. Upstream's
`uploadDssPaletteLut` (`SpectrumWidget.cpp:12573-12602 [@1872028c]`) documents
that it bypasses `dbmToRgb()`'s waterfall gain and black-level window on
purpose: otherwise the waterfall's own sliders would silently reshape the 3D
surface colours. Instead the LUT is baked from the raw `wfSchemeStops()`
gradient through the 3D Gain gamma, per `dssStrengthToRgb`
(`SpectrumWidget.cpp:11602 [@1872028c]`):

```
gamma = 4 ^ ((50 - dssGain) / 50)
rgb   = interpolateGradient(pow(clamp(s, 0, 1), gamma), schemeStops)
```

Gain 50 is linear, gain 100 is gamma 0.25 (colour lifted to the noise floor),
gain 0 is gamma 4 (colour only on the strongest peaks). The default of 70
lands at gamma 0.574. `dssStrengthToRgb` is a tier 1 lift. All six
`WfColorScheme` palettes work in 3D because the scheme **stops** are shared;
the waterfall's gain, black-level and min-dBm knobs intentionally are not.

The LUT re-bakes only when the scheme or 3D Gain changes, keyed by a token
folding both, never per frame.

### 4.4 Zoom, retune and scroll

Every row is stamped with the centre and bandwidth it was captured at, and the
shader remaps older rows into the current frame per row. This is the one piece
of upstream's frequency-frame machinery NereusSDR genuinely needs, because the
operator can zoom or tune with history on screen. What is dropped is the
settle-window and echo-latency handling, because our retune is synchronous.

Scroll phase advances from 0 to 1 by wall clock between ticker ticks, with
burst distance fixed at one row. The stack recedes continuously at any
`DisplaySpectrumFps` setting.

---

## 5. The 3D Angle control (NereusSDR-original)

Upstream renders at one fixed viewing angle. NereusSDR adds a slider.

### 5.1 Why it is cheap

`dss_mesh.vert:19-21 [@1872028c]` already declares the three shape numbers as
UBO uniforms:

```glsl
float backWidthFrac;      // back row width as a fraction of the front
float depthSpanFrac;      // how far up the plot the back row recedes
float frontMaxRidgeFrac;  // max ridge height (front) as a fraction of plot H
```

Upstream simply feeds the same three values every frame. The GPU side needs no
shader change at all.

The work is CPU-side: those three are `static constexpr` in `DssRenderer.h`,
and `depthScale`, `projectPerspective` and `projectSurface` read them directly.
They are promoted to values the widget owns and passes in. That edits tier 1
code, so upstream's values are preserved as the defaults and the
parameterisation is marked as a NereusSDR modification.

### 5.2 One slider, not two

Tilt (how far the stack spreads up the display) and convergence (how hard it
narrows toward the vanishing point) are not independent for a real camera. As
the eye lowers toward the plane, the spread compresses and the convergence
increases together. A single slider moving both along one curve is more
physically honest than two independent controls, and it cannot be set to a
combination that looks wrong.

At the top-down end the view approaches the flat waterfall look. At the low end
it approaches an edge-on wall of tall ridges.

### 5.3 Mapping

Slider value `v` in 0..100 maps to `t = v / 100`, then:

```
backWidthFrac(t)  = lerp(0.35, 0.85, t)
depthSpanFrac(t)  = lerp(0.36, 0.80, t)
```

Both curves are chosen to pass through AetherSDR's exact constants at
`t = 0.5`:

- `backWidthFrac(0.5) = 0.60` (upstream `kBackWidthFrac`)
- `depthSpanFrac(0.5) = 0.58` (upstream `kDepthSpanFrac`)

So the slider default of 50 reproduces upstream's fixed look exactly, and that
is a directly testable property.

### 5.4 Ridge height is derived, not a second slider

From the shader, the back row's baseline sits at `1 - depthSpanFrac` and its
ridge rises by `frontMaxRidgeFrac * backWidthFrac`. Staying inside the plot
requires:

```
frontMaxRidgeFrac <= (1 - depthSpanFrac) / backWidthFrac
```

At upstream's constants that ceiling is `(1 - 0.58) / 0.60 = 0.70`, and
upstream uses 0.46, so it sits at a fixed fraction of the ceiling. NereusSDR
preserves that fraction, defined from the upstream constants rather than
written as a rounded literal so the identity below holds by construction:

```
kRidgeHeadroom     = kFrontMaxRidgeFrac * kBackWidthFrac / (1 - kDepthSpanFrac)
                   = 0.46 * 0.60 / 0.42          // approximately 0.6571
frontMaxRidgeFrac  = min(0.75, kRidgeHeadroom * (1 - depthSpanFrac) / backWidthFrac)
```

At `t = 0.5` the two inner terms cancel and the expression returns
`kFrontMaxRidgeFrac` exactly. Above that it shortens ridges automatically as
the angle rises toward top-down; below it, ridges grow into the headroom that
opens up toward edge-on. The 0.75 clamp bounds the low end.

Worked endpoints, confirming the ridge stays on screen across the travel
(plot space runs 0 at the top to 1 at the bottom, so a top must be at or above
0):

| `t` | `backWidthFrac` | `depthSpanFrac` | `frontMaxRidgeFrac` | back ridge top |
|---|---|---|---|---|
| 0.0 | 0.35 | 0.36 | 0.75 (clamped from 1.20) | 0.378 |
| 0.5 | 0.60 | 0.58 | 0.460 | 0.144 |
| 1.0 | 0.85 | 0.80 | 0.155 | 0.069 |

### 5.5 Mesh sizing consequence

`kMaxRowSpanFactor` is `1 / backWidthFrac`, and `kMeshCols` is sized from it so
that a widened row never samples the height texture more sparsely than one
texel per on-screen column. Upstream computes this once at compile time via
`static_assert` because the value is constant.

With a variable angle, `kMeshCols` must be sized for the **widest** span across
the whole slider travel, which occurs at the minimum `backWidthFrac`:

| | `backWidthFrac` | `kMaxRowSpanFactor` | `kMeshCols` |
|---|---|---|---|
| Upstream fixed | 0.60 | 1.667 | 1281 |
| NereusSDR at `t=0` | 0.35 | 2.857 | 2195 |

That is 1.71x upstream's mesh column count, and the vertex buffer scales with
it. The compile-time `static_assert` is replaced by a runtime sweep test
(section 8). **The actual byte cost per panadapter must be measured before the
low end of the slider travel is fixed**; if it is unacceptable at four
panadapters, the low clamp moves up from 0.35 and the lerp endpoints are
re-solved to keep `t = 0.5` on upstream's constants.

---

## 6. Controls and persistence

| Control | Range | Default | Surface |
|---|---|---|---|
| Spectrum | 2D Waterfall / 3D Stacked Trace | 2D Waterfall | `3D VIEW` section of the right-click display menu |
| 3D Floor | 0-24 dB | 6 | same |
| 3D Gain | 0-100 | 70 | same |
| 3D Span | 0-100 | 100 | same |
| 3D Angle | 0-100 | 50 | same (NereusSDR-original) |
| 3D Slice Shadow | on / off | off | right-click context menu, shown only in 3D mode |

Plus a **Reset 3D to defaults** button, matching upstream's
`resetDisplay3DSettings()` and following the precedent set by the Reset to
Smooth Defaults button in 3G-9b.

Labels, ranges and default values for the five ported controls match upstream
exactly (`SpectrumOverlayMenu.cpp:1874-1940 [@1872028c]`), as do their tooltip
wordings.

### 6.1 Two surfaces

Upstream has only the overlay menu. NereusSDR also has Setup -> Display, which
is where the other 47 display controls live, so operators will look for these
there. All six are mirrored.

That mirroring requires two-way sync, which is the feedback-loop shape CLAUDE.md
calls out under "GUI to Model Sync". Both sides get the `m_updatingFromModel`
guard treatment rather than naive signal wiring.

### 6.2 Keys

Flat `AppSettings` keys per project convention, rather than upstream's grouped
JSON object.

Five are per panadapter, keyed through `settingsKey(base, panIndex)`:

- `DisplaySpectrumRenderMode`
- `Display3DGain`
- `Display3DSpan`
- `Display3DAngle`
- `Display3DSliceShadow`

3D Floor is per band, keyed exactly like the existing per-band grid keys built
at `PanadapterModel.cpp:85-87`:

- `Display3DFloorDepth_<bandKeyName>`

Note that the existing per-band keys (`DisplayGridMax_<band>` and friends)
carry **no pan index**; they are effectively global today because the shipped
build has a single panadapter. 3D Floor follows that convention exactly rather
than inventing a pan-scoped variant of it. When 3F multi-panadapter lands,
these keys inherit whatever per-pan migration the grid keys get, in the same
change.

### 6.3 Why 3D Floor is per band

3D Floor is anchored to the measured noise floor, and the noise floor is
strongly a per-band property. `PanadapterModel` already stores grid ceiling and
floor per band across all fourteen bands (3G-8) using
`QStringLiteral("DisplayGridMax_") + bandKeyName(b)`, so both the storage
pattern and the key-naming convention exist. 3D Floor follows them, so
switching bands recalls the depth set there.

Storage lives with the other per-band display settings on `PanadapterModel`,
not on `SpectrumWidget`, and is applied on the existing `bandChanged(Band)`
signal that already drives the per-band grid recall.

The other five settings are per panadapter, not per band.

Upstream makes its equivalent per display source (Flex versus Kiwi), which is
the same instinct expressed against a different axis.

---

## 7. Divergences from upstream

| Upstream mechanism | NereusSDR treatment | Reason |
|---|---|---|
| Supplemental channel fed by native FLEX waterfall tiles | Fed by off-screen DDC bins from the same FFT | No native tiles; we own the FFT |
| `DssSupplementalCoverage.h` tile-intensity to dBm calibration (quantile floor/span alignment) | Dropped entirely | Exists only because FLEX tiles are an arbitrary display scale; our wide channel is already dBm from the same FFT |
| `DssDcEdgeMath.h` leading-spike flattener (#4413) | Dropped; named bench-watch item (section 10) | Fix targets a FLEX firmware 4.2.18 spike when a view starts exactly at DC; our DC artifact, if any, sits at DDC centre instead |
| Cross-source coverage arbitration | Bin-range test | One measurement, not two |
| `reprojectFrequencyFrame` settle windows, `flexDssFftScaleSettling`, `armDssZoomFloorSyncAfterSettle` | Dropped | Retune is synchronous here |
| KiwiSDR source branches | Dropped | No such source |
| Multi-row burst scroll distance | Fixed at one row | Producer delivers one row per tick |
| Fixed viewing angle | 3D Angle slider, defaults to upstream's exact geometry | New capability |
| 3D Floor per display source | 3D Floor per band | Matches our per-band grid storage |
| Grouped JSON settings object | Flat `AppSettings` keys | Project convention |
| Controls in overlay menu only | Overlay menu plus Setup -> Display | Our operators expect Setup parity |

Per-row frequency frames are **kept**, because the operator can zoom or tune
with history on screen.

---

## 8. Testing

All headless. No graphics context required.

1. **Geometry golden test.** Feed `depthScale`, `projectPerspective` and
   `projectSurface` upstream's own constants and assert their exact outputs.
   This is what separates "reference-guided reimplementation" from "we
   accidentally built something else", and it is the single most important test
   in the epic.
2. **Angle identity test.** Assert that at slider value 50 the derived
   `backWidthFrac`, `depthSpanFrac` and `frontMaxRidgeFrac` equal upstream's
   0.60, 0.58 and 0.46 exactly.
3. **Angle safety sweep.** Step the slider across its whole travel and assert
   at every step that the tallest possible ridge top stays inside the plot and
   that the mesh column density invariant holds. This replaces the compile-time
   `static_assert` that promoting the constants to runtime values removes.
4. **Ring store.** A single-bin carrier must survive the peak-preserving
   downsample from full FFT width to 768 columns; ring wrap at the
   `kRows` boundary; coverage bytes correct on zoom-created gaps; temporal
   smoothing reset does not blend across a raw-scale change.
5. **Palette.** All 256 LUT entries match the `dssStrengthToRgb` formula
   (scheme stops through the 3D Gain gamma, section 4.3) across all six
   `WfColorScheme` values and gains {0, 50, 70, 100}, including the gamma
   identities: gain 50 is exactly linear, gain 100 is gamma 0.25, gain 0 is
   gamma 4. Deliberately **not** compared against `dbmToRgb()`: proving the
   waterfall gain and black-level knobs do NOT move 3D colours is part of the
   test.
6. **Settings round-trip.** All six keys, plus per-band 3D Floor recall across
   a simulated band change.
7. **Alignment.** A synthetic carrier at a known frequency lands at the same
   horizontal position in the front 3D row as in the flat waterfall row
   directly beneath it.
8. **TX lockstep.** With stop-on-TX enabled and TX active, `pushWaterfallRow`
   advances neither the waterfall ring nor the DSS ring; with it disabled,
   both advance together. Guards the tee placement in section 4.1.

Test labels and build wiring follow `docs/development/fast-test-loop.md`.

---

## 9. Attribution

Both projects are GPLv3, so there is no licence conflict.

AetherSDR ships no per-file headers. `docs/attribution/HOW-TO-PORT.md` rule 6
covers this case: reference the project URL and primary author at the NereusSDR
header-block level; there is no verbatim block to copy.
`src/gui/SpectrumOverlayMenu.h` already carries exactly that shape and is the
template for the five new files.

Requirements, all landing in the same commits that introduce the ported logic:

1. NereusSDR header block on every new file naming the AetherSDR project URL,
   primary author, and the specific upstream source files.
2. Inline cites on every tier 1 lift, stamped `[@1872028c]`.
3. Bucket A rows in `docs/attribution/aethersdr-reconciliation.md` for all
   seven files (five new, two modified).
4. `docs/attribution/ASSETS.md` entries for `dss_mesh.vert` and
   `dss_mesh.frag`. Shader attribution lives there because AetherSDR ships no
   per-file shader headers; `waterfall.frag`'s existing entry is the template.
5. `scripts/check-new-ports.py`, `scripts/verify-inline-cites.py` and
   `scripts/verify-provenance-sync.py` green.

### 9.1 Known enforcement gap

`scripts/verify-inline-tag-preservation.py` walks Thetis and mi0bot cites only.
Nothing mechanically catches a dropped AetherSDR annotation.

AetherSDR's equivalent of Thetis author tags is inline issue numbers, and the
DSS code carries many: `(#4539)`, `(#3937)`, `(#2724)`, `(#1921)`, `(#3482)`.
These are load-bearing history and must survive onto lifted lines. Because no
script enforces it, **preserving upstream issue-number annotations is an
explicit human review item on every PR in this epic.**

---

## 10. Risks

1. **Whether it looks right is not unit-testable.** The geometry golden test
   proves the math did not drift, but not that the result is pleasing. The real
   acceptance test is running AetherSDR and NereusSDR side by side on the same
   band and comparing. This requires operator eyes and is a named bench task.
2. **The angle slider has no upstream reference for its extremes.** Expect to
   clamp its travel narrower than the math permits after looking at it.
3. **Vertex count at maximum span combined with maximum angle is unmeasured.**
   1.71x upstream's mesh columns is arithmetic; the byte cost per panadapter,
   and at four panadapters, is not. Measure before fixing the low clamp
   (section 5.5).
4. **The `RGBA16F` fallback path is inherited and sound but unproven here**,
   particularly on Windows and Intel graphics, until somebody runs it there.
5. **DC-centre ridge wall (bench-watch).** The DDC's DC bin sits at the centre
   of our full-width FFT. In 2D any residual centre spike is one column of
   trace and waterfall; the 3D perspective history repeats it through 96 rows
   and turns it into a front-to-back wall. Upstream hit the same class of
   artifact at the DC **edge** and shipped `DssDcEdgeMath.h` (#4413), whose
   approach (replace a contiguous outlier with the nearby noise median, DSS
   rows only, 2D trace untouched) is the template remedy if the bench finds
   our centre wall objectionable. Not built pre-emptively: whether our DDC
   even shows a visible DC spike after existing processing is a bench
   observation, not a code-reading conclusion.
6. **Scope.** Full parity in one epic is a wide surface. The renderer, mesh and
   shaders can land and be judged independently. Slice shadows and the Setup
   mirroring are the tail and are the parts most likely to slip.

---

## 11. Out of scope

- Any change to the 2D trace, the flat waterfall, or the frequency scale.
- Perspective projection of overlays other than the optional slice shadow
  decal. Spots, notches, cursor info, IMD, TX filter and text overlays stay
  flat (decision 5).
- Drag-to-tilt gestures on the surface. The slider ships first; a gesture would
  have to resolve conflicts with existing panadapter drags and is not part of
  this epic.
- A second independent perspective or convergence control (section 5.2).
- Upstream's automation-bridge DSS hooks (`automationDssSnapshot`,
  `automationDssInjectRows`, `automationDssSetScrollback`,
  `automationDssReset`). NereusSDR has no equivalent bridge.
