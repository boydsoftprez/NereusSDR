# Visual preview tool

`nereus_visual_preview` builds the real NereusSDR main window with no radio,
fills it with fixed illustrative data, and writes full-resolution PNGs. It
captures the app as it is today: it changes nothing about how the app looks.
`capture.py` runs it across scenes and screen profiles, and `build_gallery.py`
turns a run into a page you can browse. For a design review, `sourcemap.py`
colours each capture by where its parts come from, `compare_retained.py`
proves retained parts are pixel-identical to today's app, and
`build_gallery.py` on a run root builds the one review page from both.

## Build

The target is excluded from `all`, so build it on its own in an already
configured build directory:

```
cmake --build build --target nereus_visual_preview
```

## Run

```
python3 tests/visual/capture.py --out <run-dir>            # every scene, every profile
python3 tests/visual/capture.py --out <run-dir> --scenes 'baseline-*' --profiles 'mac-*'
python3 tests/visual/capture.py --list                     # the planned runs
python3 tests/visual/capture.py --verify --scenes baseline-two-band --profiles 'mac-1512x869@2'
python3 tests/visual/capture.py --verify --against <stored-run-dir> --scenes baseline-two-band
python3 tests/visual/build_gallery.py <run-dir>            # writes <run-dir>/index.html
python3 tests/visual/build_gallery.py <run-root>           # review page, see Design review
```

`--scenes` and `--profiles` take comma-separated names or shell globs.
`--jobs N` sets how many processes run at once (default 4).
`--verify --against <dir>` captures each pair once into a temporary
directory and compares it with the stored `<dir>/<profile>/<image>.png`;
it never writes into `<dir>`.

`nereus_visual_preview --dump-geometry <file>` writes one line per visible
widget of the right panel and the pan 1 toolbar (`region path x,y wxh`),
so two scenes can be diffed to prove the workspace look moves nothing.

Each run writes `<run-dir>/<profile>/<image>.png` with a `<image>.json`
sidecar next to it, and `<run-dir>/manifest.json`. The sidecar records the
scene, profile, logical and device sizes, DPR, `QT_SCALE_FACTOR`, the screen
DPR, build tag, git HEAD and dirty state, Qt versions, platform, OS, style,
font, spectrum RHI backend, the PNG's sha256, crop rectangles, the spectrum
level checks, and what is frozen, unfrozen or not shown.

Every window and dialog image also gets `<image>.geometry.tsv`: one line per
visible widget of the grabbed window or dialog, tab-separated, with its path
(child indexes from the root), class, object name, control kind (`slider`,
`scrollbar`, `checkbox`, `radio`, `button`, `combo`, `field`, `tabbar`,
`tabwidget`, `groupbox` or `other`) and its rectangle in the grabbed
widget's logical px. Two runs compare line for line for moved or resized
widgets, and a diff can be masked against the control rectangles.

The binary itself runs one scene per process:

```
nereus_visual_preview --list
nereus_visual_preview --probe
nereus_visual_preview --scene <name> --size WxH --dpr d --out <dir> [--profile <name>]
```

It exits non-zero when the window's DPR differs from `--dpr` by more than
0.001 or its client size differs from `--size`, so a profile the screen
cannot produce fails instead of producing a wrong image.

## Design review

A run root holds two capture runs side by side:

```
<run-root>/baseline/   capture.py --out of the baseline-* scenes (today's app)
<run-root>/proposed/   capture.py --out of the proposed scenes
<run-root>/verify.txt  optional: the output of capture.py --verify
```

```
python3 tests/visual/sourcemap.py <run-root-or-run-dir> [--out <dir>] [--force] [--quiet]
python3 tests/visual/compare_retained.py <run-root> [--out <dir>]
python3 tests/visual/build_gallery.py <run-root> [--out <dir>] [--verify-log <file>] [--no-tools]
python3 tests/visual/check_recapture.py [--binary <path>]   # re-capture never lists a source map
```

With no `--out` each tool writes into the run root. `--out` mirrors the
run root's layout under another directory and never writes into the run
root, which the tools then only read.

`build_gallery.py <run-root>` runs the other two first (`--no-tools` skips
them) and writes one self-contained `index.html`: a header with revision,
Qt, platform, style, fonts, profiles and the verify status; one section per
handoff item with its scenes; per-profile tabs; a Capture / Source map
toggle; proposed alone or side by side with its baseline counterpart; fit
or true scale; the crops with links to the @3x crops; the status of every
proposed element, including behaviour that stills cannot show; the retained
proof; and the known limits. Every image carries its sha256 and size.
Planned scenes with no capture yet show a "not captured yet" tile. The
build exits non-zero if any captured scene is missing from the page.
Nothing under a `reference/` folder is ever read or linked.

The verify status comes from `--verify-log`, else from `verify*.txt` or
`verify*.log` in the run root or `--out`; with none the header says so in
red. Deep links: `index.html?section=<id>&profile=<name>&view=map,pair,true`
shows one section with the source map, the baseline pair and true scale on.

### Region contract

`sourcemap.py` reads `regions` from each sidecar. A region has `rect` and
optionally `visible_rect` as `[x, y, w, h]` in logical px of the captured
top-level window, a `provenance` (`retained`, `longpath-port`, `proposed`,
`exists-not-wired`, `not-implemented`), and optionally `z`, `depth`, a
`label` and a widget `class`. A sidecar-level `"units": "device"` (or a
per-region one) switches to device px. A crop sidecar carries its parent's
regions in window coordinates; the tool offsets them by the crop's own rect
from `crops` and scaled by the image width over the crop's logical width. A `visible_rect` of
`[0, 0, 0, 0]`, or any region clipped to nothing, is skipped. The top-most
region wins: by `z` or `depth` when given (higher on top), else the
smaller of two overlapping regions.

Each `<image>.sourcemap.png` dims the capture to 45%, fills each region in
its colour at 35% and outlines it 2 device px: retained #3fa7a0, Longpath
port #d8a55f, proposed #4a7ba8, exists not wired #9a7fd1, not implemented
#7e7e85 hatched. A legend with counts sits under the image. Maps are
rewritten only when older than the capture or sidecar, or with `--force`.

### Retained proof

`compare_retained.py` pairs each proposed scene with a baseline scene at
the same profile from the sidecars, never from a fixed list: an explicit
`baseline_counterpart` key first, then `look-dialog-X` to `dialog-X`
(the Setup pages are `look-dialog-setup-<page>` beside
`dialog-setup-<page>`), then the same
`spectrum_checks` data (pan centre, sample rate, span, reference level,
seeded signals) and transmit keying, preferring `baseline-two-band`,
`baseline-four-rx` and `baseline-tx`. It compares the slice flag crop and
the spectrum plot area (SpectrumWidget regions if the sidecar has them,
else the area between the strip, bar and panel crops, with the flag and
pan 1 toolbar crops excluded) and writes
`retained/retained-comparison.json` with the differing pixel count, the
largest channel delta and the bounding box, plus a diff PNG per pair
(baseline dimmed, differing pixels magenta, bounding box amber, excluded
areas grey). Each entry in `comparisons` has `status` `compared` (with
`identical` true or false) or `not-comparable` (sizes differ, with the
reason); `not_possible` lists each scene and target with no counterpart,
crop or area, and why.
The flag has a translucent background, so a flag over a different spectrum
differs where the spectrum shows through.

## Safety

The window is never shown on screen (`WA_DontShowOnScreen` on every window,
captured with an in-process `grab()`), and on macOS the process stays
background-only. It runs on a throwaway settings profile in the Qt test
sandbox (removed at exit) and refuses to start if settings would resolve to
the real NereusSDR settings. No scene opens a connection, starts discovery,
or keys a radio: the transmit scene drives display-only seams and refuses to
run if a radio connection exists.

## Profiles

| Profile | Logical size | DPR | Scenes |
| --- | --- | --- | --- |
| `mac-1512x869@2` | 1512x869 | 2.0 | all, plus detail crops |
| `laptop-1280x800@1` | 1280x800 | 1.0 | all |
| `laptop-1280x800@1.25` | 1280x800 | 1.25 | main window scenes |
| `laptop-1280x800@1.5` | 1280x800 | 1.5 | main window scenes |
| `fhd-1920x1080@1` | 1920x1080 | 1.0 | main window scenes |

`capture.py` probes the primary screen's DPR and sets
`QT_SCALE_FACTOR = dpr / screen DPR`, so on a Retina Mac (screen DPR 2) the
1.0 profile runs with a factor of 0.5. At `mac-1512x869@2` each main window
image also gets five crops in device pixels (slice A flag, right panel, top
strip, status bar, pan 1 toolbar), each with a 3x nearest-neighbour copy.

## Scenes

`--list` prints them. `baseline-*` scenes pose the main window (layouts 1,
2v and 2x2, transmit, the Main Panel header modes, the Main Panel floating);
`dialog-*` scenes open one dialog each, including one per Setup page (the
list comes from the running Setup dialog). Scenes live in a table in
`PreviewScenes.cpp`; add a scene by appending a row.

A Setup page scene shows the page on its first tab, at the top. Each later
tab has its own scene, `dialog-setup-<page>--<tab>` (a tab inside a tab is
`--<outer>--<inner>`), from `setupTabSpecs()`. The HL2 tabs pose the model
as a Hermes Lite 2 (`RadioModel::setBoardForTest`), and Hardware Config is
told the posed radio through `HardwarePage::onCurrentRadioChanged`, so its
tab strip is that board's; nothing connects. Each page or tab whose content
runs past the dialog (`setupScrollSpecs()`) has a
`dialog-setup-<page>[--<tab>]--scrolled` scene. It writes one image per
step, `<scene>-1` to `<scene>-N`, set through the page's own scroll area
one viewport at a time less a 40 px overlap until the bottom (and the right
end, row by row, when the page scrolls both ways). A band under each image
names the page and the step; the sidecar's `scroll_step` has the offsets.
A table too wide for the window that scrolls sideways inside its own frame
(`SetupFit::SidewaysFrame`) is stepped across through that frame.
The pages that rearrange to fit the Settings window also have
`dialog-setup-<page>[--<tab>]--min` scenes at the dialog's minimum
(820 x 600), and Antenna / ALEX and OC Outputs have `--wide` scenes at
1440 x 900, from `setupSizeSpecs()`.
All of them have `look-` pairs.

Three scenes pose a Setup page the way a manual figure shows it:
`dialog-setup-tx-profile-active` seeds the TX profiles a first connect
writes ("Default" active, in the throwaway profile) before opening TX
Profile; `dialog-setup-digital-modes-full` grows the dialog down, at its
own width, until Digital modes has nothing left to scroll (the VAX 1 to 4
cards and the TCI audio settings in one image); and
`dialog-remote-setup-tci-server` does the same for TCI Server in a
never-dialed remote window, the only window that shows the Core's TCI
server group. Its Core link is posed with `tests/fakes/FakeStationLink.h`
and the Core's server values (on, port 50001, the rest at their defaults),
so that group shows them, enabled; nothing is dialed. The sidecar's
`grown_to_fit` or notes give the sizes.

`baseline-connected` is the manual's overview window: one pan on slice A
receiving 7.185 LSB, the TX applet moved to just under RX in the Main
Panel, "You control" on the flag (`SliceChooser::flagAccessFor` for a row
this window controls) and the fixture's ANAN-G2 named in the status bar
(`StationBlock`). Nothing connects or keys. `baseline-connected-tx` is
the same window with the Main Panel's S-meter and RX hidden
(`ContentEntry::visible`), so the whole TX applet, down to TX BW, is in
view.

### Workspace look scenes

`look-two-band`, `look-four-rx`, `look-tx`, `look-connected`,
`look-connected-tx` and
`look-dialog-*` (one per
`dialog-*` scene) pose the same state as the scene they are named after
with the workspace preview switch on, so they show the proposed Longpath
look (the Glass and Depth palette in `src/gui/styles/WorkspacePalette`).
The binary sets the switch itself: on for a `look-*` scene, and for any
scene with `--workspace-preview`; off for every other scene. It never reads
the environment. In the app the look is on by default and the environment
variable `NEREUS_WORKSPACE_PREVIEW=0` turns it off (unset, empty, `1` and
any other value leave it on). With the switch off nothing changes, and the
`baseline-*` and `dialog-*` scenes stay pixel-identical (check with
`--verify --against`).

The look is applied two ways:

- (a) Explicit roles at call sites, under `if (WorkspacePreview::enabled())`:
  the application palette and baseline stylesheet, the main window
  stylesheet, the top strip and menu bar, the splitter handle, the pan
  toolbar and corner buttons, the pan badges, the status bar frame, the
  container title bars, borders and scroll bars.
- (b) A scoped pass that rewrites retained stylesheets through
  `QssTranslator` and the component treatment. It runs live on every
  widget when it is shown (windows, dialogs, Setup pages and tabs, menus,
  popups, tooltips, toasts) and once more when a shown widget sets a new
  stylesheet: `WorkspacePreview::installLiveTheming`, installed by
  `applyWorkspacePalette`. A few call sites also run
  `WorkspacePreview::themeSubtree` (container content after a reconcile,
  status bar content, pan toolbar flyouts). The tool does not re-theme
  before a grab, so every image shows what the running app shows.

(b) is preview scaffolding, not the final migration: it shows what the
look would be before each call site is moved to a role.

The slice flag, spectrum and waterfall plot colours, meter artwork and
EQ/CFC graph band colours are excluded. The sidecar of a look image has
`provenance: proposed-look`, a `look_coverage` block (explicit roles,
scoped pass, not migrated) and `unknown_literals_top30`, the colour
literals the scoped pass met and left alone.

### Proposed header scenes

`header-shown`, `header-hidden` and `arrange-on` pose the two-band data
with the workspace preview switch on (the binary sets it for these scenes)
and show the proposed compact container headers. A header is shown or
hidden; there is no hover mode. `header-shown` has the Main Panel's header
shown, with the eye button open, left of Pop out. `header-hidden` has it
hidden: no header is drawn and none takes room. `arrange-on` turns Arrange
on (`ContainerManager::setArrangeMode`) over a mix: the Main Panel keeps
its header (open eye) and a "Meters" container in the top strip hides its
own (crossed-out eye), which Arrange draws over the top of its content.
Each sidecar's notes give every drawn eye button's rect in window logical
px and its state.
At the crop profile each adds a `panel-top` crop, the Main Panel's top
120 logical px, and its @3x enlargement. Their sidecars have
`provenance: proposed` and a `proposed_values` block with the sizes and
colours the scene shows.

### Proposed four-edge workspace scenes

`edge-right`, `edge-left`, `edge-top-bottom`, `edge-all-four`,
`edge-all-four-clean`, `free-container`, `floating-container`,
`floating-container-pinned`, `drag-preview-left` and
`drag-preview-between` pose the two-band data with the workspace preview
switch on and show containers docked in the strips on all four edges of
the central workspace (`src/gui/containers/EdgeDockStrip`), free over the
spectrum and floating in their own window. The scenes add containers
from existing content types: "Meters" (the S-Meter preset and the
Power/SWR face), "Band" (the band and mode button boxes, one row each)
and "Levels" (two bar meters). Every container's header is shown,
except in `edge-all-four-clean`, where every header is hidden,
so no header shows or takes room. `free-container` uses Options > Place
Free and moves the container to the lower right of the 20 m waterfall,
its header shown; `floating-container` also writes the
floating window as `floating-container--floating`, its header's pin off
beside the return arrow; `floating-container-pinned` clicks that pin first
and writes `floating-container-pinned--floating` (the sidecar's
`stays_on_top` is true); the drag previews turn
Arrange on and call `ContainerManager::showDropPreviewForTest`. At the
crop profile each adds `strip-left`, `strip-top` and `strip-bottom` crops
for the strips in use. Their sidecars have `provenance: proposed` and a
`proposed_values` block, and the notes list each strip's size and
containers.

`arrange-drag-edge-free` and `arrange-drag-edge-floating` pose a drag held
over an edge while arranging, with the landing box shown through
`ContainerManager::showDropPreviewForTest`. In the first the Levels
container is free; its header is pressed and held and the container moved
to where a pointer in the left edge zone has it, part of it past the edge
of the spectrum, and the drag is given up with Escape after the capture.
In the second the Levels container is a window moved so that its header
is over the right edge zone, above the middle of the Main Panel, with the
landing box above the Main Panel; the window is a top-level of
its own, so it is painted over the main window image where it sits,
written as `arrange-drag-edge-floating--floating`, and its sidecar records
`position_in_main_window`. The pointer is not part of a capture.

### Longpath instrument scenes

`instruments-sheet` builds the ported Longpath instruments directly, with
fixed values, on their own top-level window (not the main window): the
frequency instrument in its three forms, the bandwidth filter pane with a
fixed trace, the bar (TX: SWR, and TX: ALC at a fixed -3 dB on the
Thetis ALC scale) and the needle (RX: Signal Peak and TX: Forward
Power), each at its preferred and its minimum size.
At the crop profile it adds `frequency-digits`, `frequency-digits-min`,
`needle-hub` and `needle-hub-min` crops with @3x enlargements. Capture it
at `mac-1512x869@2` and `laptop-1280x800@1`
(`--profiles 'mac-1512x869@2,laptop-1280x800@1'`).

`mixed-container` adds a "Station" container in the left strip that
mixes Longpath instruments with Thetis meter faces, all on slice A:
frequency, filter passband, `meter.sMeter`, Longpath needle,
`meter.crossNeedle` and Longpath bar, headers shown. The Main Panel's
"SIGNAL STRENGTH" dial (`applet:s_meter`) is a singleton the Main Panel
holds, so the Station container uses the `meter.sMeter` face instead.
`mixed-container-hidden` is the same with every header hidden. After the
data is fed, each needle gets one reading through
`MeterPoller::readingUpdated` and each filter pane one trace refresh from
the fed pan. The sidecars carry a `regions` array (each entry's rect and
visible rect, headers, viewport and strips in window coordinates) with a
per-region provenance: `longpath-port`, `retained` or `proposed`.

### Workspace profile scenes

The profile scenes seed four workspace profiles into the isolated settings
profile (fixed save time 2026-10-07 14:32 UTC) on the same two-band
station: slice A 7.185 LSB on 40 m, slice B 14.225 USB on 20 m. A profile
holds container arrangement only, so each sidecar's `slices` array shows
the same slice states in all four.

- `profile-ssb`: Main Panel right, Station container left, headers hidden.
- `profile-dx`: Main Panel right, Longpath frequency A and B left, Longpath
  signal needles A and B top, headers Always with source chips. Needle B
  stays offline: the meter poller has no slice B reading in the preview, and the scene does not fake one.
- `profile-contest`: all four strips (Meters top, Levels and Filter A
  left, Band bottom, Main Panel right), headers Always.
- `profile-ft8`: a narrow right strip with frequency and filter for slice
  B, the Main Panel in the left strip, headers Hidden.
- `profile-rail-rest`: the profile rail at rest (only the active badge, left
  of the centre title), plus a `top-strip` crop.
- `profile-rail-open`: the rail fanned out as a popover, the pointer over
  the D badge and its tooltip composited.
- `profile-new-menu` and `profile-manage-dialog`: the rail's New menu and
  the Workspace Profiles dialog, composited over the window and also
  captured alone (`--menu`, `--dialog`). Capture them at
  `mac-1512x869@2` and `laptop-1280x800@1`.

Every profile scene's sidecar carries a `rail_measure` object: the
collapsed and open rail widths, the title and network readout edges, the
free space between them and how far the open popover covers the readout.

### Controls sheet scene

`controls-sheet` puts the same real Qt controls, at the same fixed sizes,
in three columns:

- Today: the shipped look, workspace preview switch off.
- Preview now: the workspace preview as built (QssTranslator and
  Workspace::componentize over today's stylesheets, plus the workspace
  palette and app stylesheet).
- Longpath: Longpath's own control stylesheets, palette and app baseline
  stylesheet, ported verbatim in `LongpathControlStyles.h` (preview tool
  only).

Rows: horizontal slider (resting, hover, pressed, disabled), vertical
slider, checkbox and radio (off, on, disabled), combo (closed, and open
with its popup), line edit and spin box (resting, focused), tab bar with
its pane, scroll bar, group box, a menu with checked and disabled items, a
tooltip, push buttons (resting, hovered, checked, disabled) and
MeterSlider, which paints itself and takes no stylesheets. The last rows
are real panels in each look: the RX applet slider rows, TX power and tune
power, and microphone gain (PhoneCwApplet), each grabbed from the applet
itself at its full width so every column shows the whole applet; the RX
rows again as the main window shows them through the Main Panel viewport
(labelled as the real clipping, in today's window layout); and the TX EQ
dialog sliders. Today's RX rows match `baseline-two-band` where the
viewport shows them, and the EQ row matches `dialog-tx-eq`. The TX and mic
bands are below the fold in the baseline window, so they have no stored
counterpart.

`controls-sheet--zoom` cuts a 32x24 logical patch around each slider
handle and indicator and scales it 5x nearest-neighbour, the three looks
side by side.

The sidecar lists every placement (sheet rect and crop source), each
control's sizeHint and minimumSizeHint per look, and which look each
column used. Offscreen, Qt paints the open combo popup's selected row
without the stylesheet's selection colour; the sheet notes this, so check
that row against a live popup before judging it.

```
python3 tests/visual/capture.py --out <dir> --scenes controls-sheet \
    --profiles 'mac-1512x869@2,laptop-1280x800@1'
```

### Choice sheets

These scenes put a few candidate looks for one kind of control in rows,
one candidate per row, for JJ to pick from. Every cell is a real Qt widget
with the candidate's stylesheet, on a background painted as the preview
paints it. Each row's note names every colour and the palette role it
comes from; the sidecar repeats them under `variants`.

- `controls-sheet--indicators` (and `--zoom`, 5x): checkbox and radio
  off, on, disabled off and disabled on at today's 16 px row height, on
  the Main Panel (`kPanelBg`) and a Setup page (`kAppBg`). Rows: today,
  Longpath as is, then three brighter off states built on Longpath's
  sheet.
- `controls-sheet--lit-buttons` (and `--zoom`, 5x, one row per state):
  checked push buttons with real labels, resting and hovered, on the Main
  Panel. Rows: today's three meanings (green, blue, amber), the same three
  in Longpath's tones (the amber is a composition from Longpath's roles),
  and Longpath's one blue.
- `controls-sheet--lit-green` (and `--zoom`, 5x): the same lit buttons
  in Longpath's tones, one row per green: Longpath's flat green, then
  three raised greens built from palette roles with Longpath's
  `raisedFill` and `shiftL`. Each row has one unlit NR beside it.
- `controls-sheet--scrollbars` (and `--zoom`, 5x): a real scrolled
  QScrollArea the size of the Main Panel's at laptop 1280x800, with
  Longpath's `kScrollBarStyle` at 8, 10 and 14 px, resting and with the
  handle hovered. The sidecar has each bar's measured width and viewport
  (`measured`), and the live Main Panel and Setup page bars
  (`live_bars`; the Setup page again with the preview switch on).
- `controls-sheet--sizes`: combo, line edit, spin box, tabs and a push
  button at Longpath's own sizes, then at today's sizes with Longpath's
  shading, with today's corners and with corners scaled to the control
  height. Captions give each widget's logical size; the sidecar has the
  radii and today's sizes.

`capture.py` treats each of these as its own scene: capturing
`controls-sheet` neither clears nor claims their files.

```
python3 tests/visual/capture.py --out <dir> --scenes 'controls-sheet--*' \
    --profiles 'mac-1512x869@2,laptop-1280x800@1'
```

### Strip fit scenes

`strip-fit-today` (the baseline two-band window) and `strip-fit-preview`
(the preview switch on, the Main Panel in the right strip as the preview
places it) measure every scroll area under the Main Panel container and
every applet in it: viewport width, vertical bar width, body width and
minimum, and each applet's width, minimum width and the logical px the
viewport hides. The result is `panel_fit` in the window sidecar and in
`<scene>--panel`, a crop of the Main Panel container.

```
python3 tests/visual/capture.py --out <dir> --scenes 'strip-fit-*' \
    --profiles 'mac-1512x869@2,laptop-1280x800@1'
```

### Container item parity scenes

The `container-parity-*` scenes show the container items and editors the
container item parity work changed, on the two-band data with the
workspace preview switch on. Each commits one or more containers beside
the Main Panel the way the settings dialog does: new items come from
`ContainerContentRegistry::makeEntry` (Thetis's defaults), saved-layout
items from the record a layout saved before the change holds. Each scene
writes the window (`<scene>`), each container as the window shows it
(`<scene>--<part>`, plus `<part>-content` when the container scrolls) and
the property editor Containers > Edit Container shows for an entry, at its
full height (`<scene>--editor-*`). A disabled row's tooltip is painted
under it in its own editor image; an open token picker or Units list is
composited over the dialog (`--token-picker`, `--units`) and also written
alone (`--popup`).

- `container-parity-button-grids`: new mode, tune step, antenna, filter and
  other button grids and their editors' Visible rows; the antenna XVTR and
  Rx/Tx swap rows and the filter Var1 and Var2 rows with their reasons.
- `container-parity-mode-grid`: a new mode grid, one from a saved layout
  and a new Contest face; then SPEC and DRM ticked in the saved grid's
  Visible modes and applied (`--grids-spec-drm`).
- `container-parity-vfo-display` and `-roomy`: new VFO display faces on
  slice A and on all slices at the default face height, each screen of the
  slice A face opened by a click or long press (`--screen-*`), and the same
  faces 240 px tall in a 300 px top strip.
- `container-parity-clock`: new clocks with Large time and Centered, a
  saved clock and a new clock face, all reading 14:32:10 local
  (`ClockItem::setTimeSource`), and their editors.
- `container-parity-overlay-led`: new and saved text overlays with slice
  tokens typed into Text 1 (a new overlay starts empty, as Thetis's does)
  and a new LED; the overlay editor (with its LED gate rows), its "%" token picker
  open, and the LED editor before and after a condition with a mistake.
- `container-parity-smeter-dial`: added S-meter dials with dark mode and
  history off and on, fed a swinging level so history has a wedge; new and
  saved-look tuning dials.
- `container-parity-web-image`: a web image item reading a local PNG (no
  network) on Pan 1, and pan 1 with that picture behind its spectrum, with
  the pan's own background off (the picture at full strength) and on. The
  words "Web image (preview picture)" are the picture's own caption.
- `container-parity-artwork-options`: the ANAN face's Dark mode and the
  magic eye's Built-in bezel, disabled with their reason.
- `container-parity-signal-text`: new and saved signal text items and
  faces fed -73 dBm, the editor and its Units list.
- `container-parity-sections`: a TX and an RX section item as built and
  with a part hidden and the order reversed, and the reordered one's editor.
- `container-parity-long-title`: "Signal max bin bar" and a short title
  ("Signal": a one-entry container's header shows its entry's name) in
  a 260 px left strip; the notes give the header and title widths.

```
python3 tests/visual/capture.py --out <dir> --scenes 'container-parity-*' \
    --profiles 'mac-1512x869@2,laptop-1280x800@1'
```

## Determinism

The same scene at the same profile on the same machine and build gives
identical pixels; `capture.py --verify` runs each pair twice and prints
`IDENTICAL`, or the differing pixel count and bounding box, and exits
non-zero on any difference. What makes it repeatable:

- The spectrum and waterfall come from a seeded band model, never from a
  radio or the clock. Each band has its own noise floor (80 m -100,
  40 m -104, 20 m -110, 15 m -114 dBm, measured as the median of the
  displayed trace). The spectrum is one settled frame with about 3 dB of
  per-bin jitter. Waterfall rows are generated one per row period (30 ms by
  default, the waterfall's own setting), each row a fresh draw of Rayleigh
  noise (its deepest nulls held at -4 dB, see below) under the band's
  signals at that instant: SSB voices with a syllabic envelope, pauses and
  formant contours across 300-2700 Hz, CW keyed in Morse at 22 wpm, and FT8
  as 8-tone FSK at 6.25 Hz spacing and 0.16 s symbols in 15 s cycles. Each
  pan logs one check: the displayed level at its strongest signal, which
  must be within 1 dB of the stated level, and the displayed noise median.
- The waterfall AGC (on by default) keys its palette to each row's minimum
  and maximum less and plus 12 dB. Unbounded Rayleigh nulls drag the low end
  down and paint the noise green, so the row noise is held at no more than
  4 dB under its mean, and rows run hotter than the averaged trace (speech
  peaks +6 dB, CW key-down +3 dB), as live rows do. 120 rows are pushed
  ahead of the visible ones so the AGC has settled. A band whose strongest
  signal is only a little over the noise still paints its noise lighter,
  as the app does live.
- The waterfall ticker is paused and every row is pushed explicitly.
- Every running timer under the window is stopped, as are the meter poller
  and the slice meter pump; the UTC clock reads 14:32:10 UTC, the network,
  latency and CPU readouts are fixed, and the S-meter needle is allowed to
  settle before capture.
- Anything that could not be frozen is listed under `unfrozen` in the
  sidecar.

## Limits

- Pixels are only comparable on the same OS, Qt version and fonts. On a Mac,
  the 1, 1.25 and 1.5 profiles show the layout at those sizes, but Core Text
  rasterises the glyphs: Windows ClearType and Linux FreeType hinting give
  different text shapes and widths at the same scale.
- Fractional scale factors are applied by Qt on top of the screen's DPR, not
  by a real 125% or 150% Windows display.
- Amp, Tuner and RF-Kit applets are not shown: they appear only once a
  device setting that starts discovery or a connection is on.
- The tool shows the current app, plus the proposed workspace look in the
  `look-*` scenes, the proposed headers in the `header-*` and
  `arrange-on` scenes and the four-edge workspace in the `edge-*`,
  `free-container`, `floating-container*` and `drag-preview-*` scenes and the
  Longpath instruments in `instruments-sheet` and `mixed-container*` and the
  workspace profiles in the `profile-*` scenes. Other proposed designs are added as new scenes.
