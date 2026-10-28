#!/usr/bin/env python3
# no-port-check: NereusSDR-original visual preview tool; gallery page.
#
# tests/visual/build_gallery.py  (NereusSDR)
#
#   build_gallery.py <run-root> [--out <dir>] [--verify-log <file>] [--no-tools]
#   build_gallery.py <run-dir>
#
# Run-root mode (the directory holds baseline/ and proposed/): writes one
# review page, <out>/index.html (out defaults to the run root), over the
# whole run root. It first runs sourcemap.py and compare_retained.py into the
# same out dir (skip with --no-tools), then lays the scenes out by the
# handoff's coverage matrix: header (revisions, Qt, platform, style, fonts,
# profiles, verify status), one section per coverage item with per-profile
# tabs, a capture / source map toggle, baseline and proposed side by side,
# true scale and fit, detail crops with their @3x copies, the retained
# comparison, the status table and the known limits. Every image links to
# its full-resolution PNG and shows its sha256 and size. Only files inside
# the run root and the out dir are linked, never anything under reference/.
#
# Single-run mode (a capture.py run dir with manifest.json and no
# baseline/ or proposed/): writes <run-dir>/index.html, a self-contained
# dark page (no external CSS,
# fonts or scripts) over a capture.py run. One tab per profile; in each,
# the scenes grouped (main window, crops, dialogs, Setup pages); for every
# image a fit view, a true-scale view (CSS width = logical width, so the
# browser shows it at the size the app would draw on a screen of that DPR),
# a link to the PNG and a metadata table from its sidecar. The PNGs are
# linked relatively, not embedded, so the page travels with its run dir.
#
# Modification history (NereusSDR):
#   2026-10-07 - Written for the workspace visual review. J.J. Boyd
#                (KG4VCF), AI-assisted via Anthropic Claude Code.
#   2026-10-08 - Run-root review page: coverage sections, source maps,
#                retained comparison, status table. J.J. Boyd (KG4VCF),
#                AI-assisted via Anthropic Claude Code.
#   2026-10-08 - Integration: setup-look-* dropped (the Setup section uses
#                look-dialog-setup-*), clean-spectrum-reveal placed.
#                J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
#   2026-10-11 - Scene names follow the two header choices (header-shown,
#                mixed-container-hidden; the reveal scenes are gone).
#                J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
#   2026-10-11 - Placement section and status rows list the pinned
#                floating window and the two arrange drag scenes. J.J. Boyd
#                (KG4VCF), AI-assisted via Anthropic Claude Code.
#   2026-10-08 - Tall extra images held to 1000 px in Fit; "no longer
#                produced" counts as a verify failure; dialog-only scenes
#                say which profiles dialogs are captured at; status table
#                lists the source menu, property editor and CW page.
#                J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
#   2026-10-26 - Single-run mode lists images of any other kind (container,
#                editor, popup, sheet, ...) under "Other images" instead of
#                dropping them. J.J. Boyd (KG4VCF), AI-assisted via
#                Anthropic Claude Code.

import argparse
import datetime
import fnmatch
import hashlib
import html
import json
import os
import pathlib
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import compare_retained  # noqa: E402
import sourcemap  # noqa: E402

GROUPS = [
    ("window", "Main window"),
    ("floating", "Floating windows"),
    ("crop", "Detail crops"),
    ("crop-3x", "Detail crops, 3x nearest-neighbour"),
    ("dialog", "Dialogs"),
    ("setup", "Setup pages"),
]

# Single-run mode: images whose kind is not a GROUPS key land here.
OTHER_GROUP = ("other", "Other images")


def esc(v):
    return html.escape(str(v), quote=True)


def group_of(meta):
    kind = meta.get("kind", "window")
    if kind == "dialog" and str(meta.get("scene", "")).startswith("dialog-setup-"):
        return "setup"
    if kind not in (k for k, _ in GROUPS):
        return OTHER_GROUP[0]
    return kind


def fmt_list(items):
    if not items:
        return "none"
    out = []
    for it in items:
        if isinstance(it, dict):
            item = it.get("item") or it.get("name") or ""
            reason = it.get("reason") or it.get("region") or ""
            out.append("%s: %s" % (item, reason) if reason else json.dumps(it, sort_keys=True))
        else:
            out.append(str(it))
    return "<br>".join(esc(x) for x in out)


def fmt_checks(checks):
    if not checks:
        return "none"
    rows = []
    for c in checks:
        rows.append("%s: %.0f Hz target %.1f dBm, displayed %.2f dBm (%s); waterfall %.1f..%.1f dBm" % (
            c.get("pan"), c.get("target_hz", 0), c.get("target_dbm", 0),
            c.get("displayed_dbm", 0), "within 1 dB" if c.get("within_1db") else "OUTSIDE 1 dB",
            c.get("waterfall_low_dbm", 0), c.get("waterfall_high_dbm", 0)))
    return "<br>".join(esc(r) for r in rows)


def meta_table(meta):
    env = meta.get("environment", {})
    font = env.get("font", {})
    size = meta.get("logical_size", [0, 0])
    dev = meta.get("device_size", [0, 0])
    rows = [
        ("Scene", meta.get("scene")),
        ("Profile", meta.get("profile")),
        ("Kind", meta.get("kind")),
        ("Logical size", "%d x %d" % tuple(size)),
        ("DPR", meta.get("dpr")),
        ("Device size", "%d x %d" % tuple(dev)),
        ("QT_SCALE_FACTOR", meta.get("qt_scale_factor", env.get("qt_scale_factor"))),
        ("Primary screen DPR (probed)", meta.get("probed_primary_screen_dpr")),
        ("Screen DPR seen by the window", env.get("primary_screen_dpr_scaled")),
        ("Build tag", env.get("build_tag")),
        ("Git HEAD", "%s%s" % (meta.get("git_head", "?"), " (dirty)" if meta.get("git_dirty") else "")),
        ("Qt", "runtime %s, compiled %s" % (env.get("qt_runtime"), env.get("qt_compiled"))),
        ("Platform / OS", "%s / %s" % (env.get("platform"), env.get("os"))),
        ("Style", env.get("style")),
        ("Font", "%s, %s pt, %s px" % (font.get("family"), font.get("point_size"), font.get("pixel_size"))),
        ("RHI backend", env.get("spectrum_rhi_backend")),
        ("PNG sha256", meta.get("sha256")),
        ("Provenance", meta.get("provenance")),
    ]
    if meta.get("enlargement"):
        rows.append(("Enlargement", meta.get("enlargement")))
    body = "".join("<tr><th>%s</th><td>%s</td></tr>" % (esc(k), esc(v)) for k, v in rows)
    crops = meta.get("crops") or []
    crop_txt = "<br>".join(esc("%s: logical %s, device %s" % (c.get("name"), c.get("logical"), c.get("device")))
                           for c in crops) or "none"
    body += "<tr><th>Crops</th><td>%s</td></tr>" % crop_txt
    body += "<tr><th>Spectrum checks</th><td>%s</td></tr>" % fmt_checks(meta.get("spectrum_checks"))
    body += "<tr><th>Not shown</th><td>%s</td></tr>" % fmt_list(meta.get("not_shown"))
    body += "<tr><th>Unfrozen</th><td>%s</td></tr>" % fmt_list(meta.get("unfrozen"))
    body += "<tr><th>Frozen</th><td>%s</td></tr>" % fmt_list(meta.get("frozen"))
    body += "<tr><th>Notes</th><td>%s</td></tr>" % fmt_list(meta.get("notes"))
    return "<table class='meta'>%s</table>" % body


def card(rel_png, meta, idx):
    size = meta.get("logical_size", [0, 0])
    title = pathlib.Path(rel_png).stem
    return (
        "<figure class='card' id='img%d'>"
        "<figcaption><span class='name'>%s</span>"
        "<span class='ctl'>"
        "<button type='button' data-view='fit' class='on'>Fit</button>"
        "<button type='button' data-view='true'>True scale</button>"
        "<a href='%s' target='_blank' rel='noopener'>PNG</a>"
        "<span class='dim'>%d x %d @ %s</span></span></figcaption>"
        "<div class='frame fit'><img loading='lazy' src='%s' alt='%s' data-lw='%d'></div>"
        "<details><summary>Metadata</summary>%s</details>"
        "</figure>"
    ) % (idx, esc(title), esc(rel_png), size[0], size[1], esc(meta.get("dpr")),
         esc(rel_png), esc(title), size[0], meta_table(meta))


CSS = """
:root { color-scheme: dark; --bg:#0f1218; --panel:#171c25; --line:#2a3240; --text:#d8dee9;
        --muted:#8a94a6; --accent:#38bdf8; }
* { box-sizing: border-box; }
body { margin:0; background:var(--bg); color:var(--text);
       font: 14px/1.45 -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, sans-serif; }
header { padding:16px; border-bottom:1px solid var(--line); }
h1 { font-size:18px; margin:0 0 4px; }
.sub { color:var(--muted); font-size:13px; }
nav.tabs { display:flex; flex-wrap:wrap; gap:4px; padding:8px 16px; border-bottom:1px solid var(--line);
           position:sticky; top:0; background:var(--bg); z-index:2; }
nav.tabs button { background:var(--panel); color:var(--text); border:1px solid var(--line);
                  border-radius:6px; padding:6px 10px; cursor:pointer; font:inherit; }
nav.tabs button.on { border-color:var(--accent); color:var(--accent); }
section.tab { display:none; padding:8px 16px 40px; }
section.tab.on { display:block; }
h2 { font-size:16px; margin:24px 0 8px; color:var(--accent); }
h3 { font-size:14px; margin:16px 0 8px; color:var(--muted); font-weight:600; }
.card { margin:0 0 20px; background:var(--panel); border:1px solid var(--line); border-radius:8px;
        overflow:hidden; }
figcaption { display:flex; flex-wrap:wrap; gap:8px; align-items:center; justify-content:space-between;
             padding:8px 10px; border-bottom:1px solid var(--line); }
.name { font-weight:600; word-break:break-all; }
.ctl { display:flex; gap:6px; align-items:center; flex-wrap:wrap; }
.ctl button { background:transparent; color:var(--muted); border:1px solid var(--line); border-radius:5px;
              padding:2px 8px; cursor:pointer; font:inherit; font-size:12px; }
.ctl button.on { color:var(--accent); border-color:var(--accent); }
.ctl a { color:var(--accent); font-size:12px; }
.dim { color:var(--muted); font-size:12px; }
.frame { overflow:auto; background:#000; }
.frame.fit img { display:block; max-width:100%; height:auto; }
.frame.true img { display:block; max-width:none; height:auto; image-rendering:auto; }
details { padding:6px 10px; }
summary { cursor:pointer; color:var(--muted); }
table.meta { border-collapse:collapse; margin-top:6px; font-size:12px; width:100%; }
table.meta th { text-align:left; color:var(--muted); font-weight:500; padding:3px 10px 3px 0;
                vertical-align:top; white-space:nowrap; width:1%; }
table.meta td { padding:3px 0; word-break:break-all; }
.empty { color:var(--muted); }
"""

JS = """
document.querySelectorAll('nav.tabs button').forEach(function (b) {
  b.addEventListener('click', function () {
    document.querySelectorAll('nav.tabs button').forEach(function (x) { x.classList.toggle('on', x === b); });
    document.querySelectorAll('section.tab').forEach(function (s) { s.classList.toggle('on', s.id === b.dataset.tab); });
    try { localStorage.setItem('nereusGalleryTab', b.dataset.tab); } catch (e) {}
  });
});
document.querySelectorAll('.card').forEach(function (c) {
  var frame = c.querySelector('.frame'), img = c.querySelector('img');
  c.querySelectorAll('.ctl button').forEach(function (b) {
    b.addEventListener('click', function () {
      c.querySelectorAll('.ctl button').forEach(function (x) { x.classList.toggle('on', x === b); });
      var t = b.dataset.view === 'true';
      frame.className = 'frame ' + (t ? 'true' : 'fit');
      img.style.width = t ? img.dataset.lw + 'px' : '';
    });
  });
});
try {
  var saved = localStorage.getItem('nereusGalleryTab');
  var btn = saved && document.querySelector('nav.tabs button[data-tab="' + saved + '"]');
  if (btn) { btn.click(); }
} catch (e) {}
"""


def build_run_page(run_dir):
    """The single-run page: <run-dir>/index.html over one capture.py run."""
    manifest_path = run_dir / "manifest.json"
    if not manifest_path.exists():
        sys.exit("no manifest.json in %s" % run_dir)
    manifest = json.loads(manifest_path.read_text())

    profiles = list(manifest.get("profiles", {}).keys())
    by_profile = {p: [] for p in profiles}
    failures = []
    for run in manifest.get("runs", []):
        if run.get("returncode") != 0:
            failures.append(run)
            continue
        for img in run.get("images", []):
            meta = json.loads((run_dir / img["json"]).read_text())
            by_profile.setdefault(run["profile"], []).append((img["png"], meta))

    tabs, sections = [], []
    idx = 0
    for n, profile in enumerate(p for p in profiles if by_profile.get(p)):
        tab_id = "tab%d" % n
        tabs.append("<button type='button' data-tab='%s' class='%s'>%s <span class='dim'>(%d)</span></button>" % (
            tab_id, "on" if n == 0 else "", esc(profile), len(by_profile[profile])))
        info = manifest["profiles"].get(profile, {})
        parts = ["<p class='sub'>Logical %s, DPR %s, QT_SCALE_FACTOR %s</p>" % (
            esc("x".join(str(v) for v in info.get("logical_size", []))), esc(info.get("dpr")),
            esc(info.get("qt_scale_factor")))]
        for key, label in GROUPS + [OTHER_GROUP]:
            items = [(png, meta) for png, meta in by_profile[profile] if group_of(meta) == key]
            if not items:
                continue
            parts.append("<h2>%s</h2>" % esc(label))
            current_scene = None
            for png, meta in items:
                if key in ("crop", "crop-3x", OTHER_GROUP[0]) and meta.get("scene") != current_scene:
                    current_scene = meta.get("scene")
                    parts.append("<h3>%s</h3>" % esc(current_scene))
                parts.append(card(png, meta, idx))
                idx += 1
        sections.append("<section class='tab %s' id='%s'>%s</section>" % (
            "on" if n == 0 else "", tab_id, "".join(parts)))

    fail_html = ""
    if failures:
        fail_html = "<p class='sub'>Failed runs: %s</p>" % esc(", ".join(
            "%s @ %s" % (f["scene"], f["profile"]) for f in failures))
    not_shown = "; ".join(x.get("item", "") for x in manifest.get("not_shown", []))
    head = "%s%s" % (str(manifest.get("git_head", ""))[:10], " (dirty)" if manifest.get("git_dirty") else "")
    page = (
        "<!doctype html><html lang='en'><head><meta charset='utf-8'>"
        "<meta name='viewport' content='width=device-width, initial-scale=1'>"
        "<title>NereusSDR Visual Baseline</title><style>%s</style></head><body>"
        "<header><h1>NereusSDR visual baseline</h1>"
        "<div class='sub'>%d images, captured %s from %s, provenance %s. Probed screen DPR %s. "
        "Not shown in any scene: %s.</div>%s</header>"
        "<nav class='tabs'>%s</nav>%s<script>%s</script></body></html>"
    ) % (CSS, manifest.get("image_count", 0), esc(manifest.get("started_utc", "")), esc(head),
         esc(manifest.get("provenance", "")), esc(manifest.get("probed_primary_screen_dpr")),
         esc(not_shown), fail_html, "".join(tabs), "".join(sections), JS)
    out = run_dir / "index.html"
    out.write_text(page)
    print("wrote %s (%d images)" % (out, idx))
    return 0


# ==========================================================================
# Run-root review page
# ==========================================================================

# The handoff's coverage matrix, in its order. Each section: id, title, what
# must be visible (from the handoff), the scenes shown in it (fnmatch
# patterns, primary placement) and scenes shown elsewhere that also cover it.
SECTIONS = [
    ("ssb", "Standard SSB station",
     "Full receiver operation, preserved flags, frequency/filter instruments, audio, complete relevant "
     "TX and microphone controls.",
     ["look-two-band"], ["profile-ssb", "mixed-container"]),
    ("multi-rx", "Multiple receivers",
     "More than one source with distinct flags, receiver controls, meters, and panadapter relationships.",
     ["look-four-rx", "four-rx-look"], ["profile-dx"]),
    ("dense", "Dense operating workspace",
     "A realistic larger set of widgets with usable dimensions, clear source assignment, and adequate "
     "spectrum area.",
     ["profile-contest", "edge-all-four"], ["mixed-container"]),
    ("clean", "Clean spectrum workspace",
     "Minimal visible container chrome while retaining access to operating controls and arrangement.",
     ["clean-spectrum", "edge-all-four-clean"],
     ["header-hidden", "mixed-container-hidden", "profile-ft8"]),
    ("cw-digital", "CW and digital arrangements",
     "Relevant PhoneCw, Cwx, Dvk, Digital, or other existing content selected from the inventory; label "
     "availability accurately.",
     ["cw-digital", "cw-digital-*"], ["profile-ft8"]),
    ("tx", "Transmit and processing screens",
     "EQ, CFC, PureSignal, modulation monitoring, and relevant TX profile controls in their actual forms.",
     ["look-tx", "tx-processing", "tx-processing-*", "look-dialog-tx-eq", "look-dialog-tx-cfc",
      "look-dialog-pure-signal"], []),
    ("mixed", "Mixed container composition",
     "Longpath frequency/filter/instruments alongside Thetis meter faces and native controls; show group "
     "and individual content headers.",
     ["mixed-container", "mixed-container-hidden", "instruments-sheet"], ["profile-dx"]),
    ("placement", "Placement states",
     "All four edge layouts, free placement, floating windows, and meaningful differences between a "
     "shown header, a hidden one, and Arrange.",
     ["header-shown", "header-hidden", "arrange-on",
      "edge-right", "edge-left", "edge-top-bottom", "free-container", "floating-container",
      "floating-container-pinned", "drag-preview-left", "drag-preview-between",
      "arrange-drag-edge-free", "arrange-drag-edge-floating"],
     ["edge-all-four", "edge-all-four-clean"]),
    ("editing", "Widget and container editing",
     "Add content, source selection, content properties, and mixed-content composition using actual or "
     "deliberately designed screens.",
     ["edit-add-content", "edit-source-select", "edit-properties", "edit-*", "look-dialog-container-settings"],
     ["mixed-container"]),
    ("profiles", "Workspace profiles",
     "Profile chooser, creation/duplication/save/manage states and distinctly different arrangements.",
     ["profile-rail-rest", "profile-rail-open", "profile-new-menu", "profile-manage-dialog", "profile-ssb",
      "profile-dx", "profile-ft8"], ["profile-contest"]),
    ("setup", "Setup and supporting screens",
     "Representative real setup and diagnostic screens, with sufficient coverage to judge theme consistency.",
     ["look-dialog-setup-*", "look-dialog-pan-layout", "look-dialog-diversity", "look-dialog-*"], []),
]

# Scenes planned for this review that may not be captured yet: a tile says so.
PLANNED = {
    "clean-spectrum": "One pan, 20 m data, no docked containers, one free container (Frequency and needle), "
                      "header hidden: the headerless operating view.",
    "cw-digital": "Slice A 14.030 CW, slice B 7.074 DIGU; PhoneCw on its CW page, Cwx, Dvk and Digital "
                  "(exists in code, not wired) or a plate saying why one is not available.",
    "tx-processing": "The look in transmit: TxApplet with its profile selector, EqApplet (exists in code, not "
                     "wired), PureSignal, ModMonitor; Longpath needles for power and SWR, a bar for ALC; "
                     "the CFC and TX EQ dialogs.",
    "edit-add-content": "The Add Content picker on the Station container, Longpath entries next to the "
                        "Thetis presets.",
    "edit-source-select": "The source chip menu on a frequency entry: RX A to RX D and Active slice.",
    "edit-properties": "Container settings open on a Longpath needle entry: its binding and form.",
    "four-rx-look": "The four-receiver data in the look: four compact frequency instruments (A to D) in the "
                    "left strip and four compact Longpath S-meter bars in the bottom strip.",
}

# Collapsed by default: long runs of near-alike pages.
COLLAPSED = ["look-dialog-setup-*"]

STATUS_BUILT = "built in preview code"
STATUS_SCAFFOLD = "scaffolding only"
STATUS_NONE = "not implemented"
STATUS_NOT_WIRED = "exists in code, not wired in the app"
STILLS = "not verifiable from stills"

# (element, status, what it is, scenes, still-verifiable)
STATUS_ROWS = [
    ("Compact group header (18 px)", STATUS_BUILT,
     "Grip, title, source chip, eye, pop-out, options and hide inside one header; replaces the 22 px "
     "title bar. A floating window's header adds a pin beside the return arrow.",
     "header-shown, arrange-on, floating-container, floating-container-pinned", True),
    ("Hidden", STATUS_BUILT,
     "No header; the eye button, Keep Header Showing in the right-click menu, or Arrange brings it back.",
     "header-hidden, arrange-on", True),
    ("Arrange mode", STATUS_BUILT,
     "Containers > Arrange Workspace (Ctrl+Shift+A, Escape leaves) or Shift held alone: every header, "
     "accent outline, dot corners, strip target bands. Runtime only, never saved.",
     "arrange-on, drag-preview-left, arrange-drag-edge-free, arrange-drag-edge-floating", True),
    ("Entry headers (16 px)", STATUS_BUILT,
     "One per entry in a multi-content stack; a single-content container uses the group header title.",
     "mixed-container, header-shown", True),
    ("Inside handles", STATUS_BUILT,
     "The grip sits in the header and the dot corner inside the bounds; the external 20 px gutter is gone.",
     "header-shown, arrange-on", True),
    ("Left edge strip", STATUS_BUILT, "Containers docked on the left edge of the workspace.", "edge-left", True),
    ("Right edge strip", STATUS_BUILT,
     "Today's right panel position as a strip. The Main Panel keeps its 306 px, inside a 318 px strip and a "
     "6 px handle, so the spectrum area is narrower than today (1200 against 1206 logical at mac@2).",
     "edge-right, look-two-band", True),
    ("Top edge strip", STATUS_BUILT, "Containers docked along the top; sized to the tallest container.",
     "edge-top-bottom", True),
    ("Bottom edge strip", STATUS_BUILT, "Containers docked along the bottom.", "edge-top-bottom", True),
    ("Drag preview", STATUS_BUILT,
     "Target bands and the destination preview during a header drag, for a strip, a free and a floating "
     "source. The stills pose it through a test seam.",
     "drag-preview-left, drag-preview-between, arrange-drag-edge-free, arrange-drag-edge-floating", True),
    ("Free placement", STATUS_BUILT,
     "Options > Place Free, or a drop over the spectrum while arranging, puts a container over the "
     "spectrum. While arranging, a free container dropped at an edge docks there.",
     "free-container, arrange-drag-edge-free", True),
    ("Floating window", STATUS_BUILT,
     "The existing floating container, sized from the container. Its header's pin keeps it on top; while "
     "arranging, its header dragged over the main window docks it or sets it free.",
     "floating-container, floating-container-pinned, arrange-drag-edge-floating", True),
    ("FrequencyInstrument (Longpath port)", STATUS_BUILT, "Number only, band strip and flat arc forms.",
     "instruments-sheet, mixed-container", True),
    ("NeedleInstrument (Longpath port)", STATUS_BUILT, "RX signal and TX power needles.",
     "instruments-sheet, mixed-container", True),
    ("BarInstrument (Longpath port)", STATUS_BUILT, "SWR, and ALC on the Thetis ALC scale.",
     "instruments-sheet, mixed-container", True),
    ("BandwidthFilterPane (Longpath port)", STATUS_BUILT, "Passband over the slice's own trace.",
     "instruments-sheet, mixed-container", True),
    ("ProfileRail and pill style (Longpath port)", STATUS_BUILT, "The profile badges.",
     "profile-rail-open", True),
    ("Header source menu", STATUS_BUILT,
     "The group header's source chip opens RX A to RX D and Active slice; slices that are not open are "
     "disabled. It writes the container source as the Container Settings Slice combo does.",
     "edit-source-select", True),
    ("Longpath property editor", STATUS_BUILT,
     "Container Settings shows each Longpath entry's declared properties (reading, form, compact, label) "
     "in place of \"Unsupported configuration\".", "edit-properties", True),
    ("Factory entries", STATUS_BUILT,
     "Frequency, Custom frequency, Needle meter, Bar meter and Filter passband, under the Longpath instruments heading, as per-instance container content, "
     "registered only with the preview switch on.", "mixed-container, edit-add-content", True),
    ("Workspace profiles", STATUS_BUILT,
     "Named arrangements (layout only): switch, save, new, duplicate, rename, remove.",
     "profile-ssb, profile-dx, profile-contest, profile-ft8", True),
    ("Profile rail", STATUS_BUILT,
     "At rest one badge beside the title; hover, click or Tab fans out the rail as a popover.",
     "profile-rail-rest, profile-rail-open", True),
    ("Manage profiles dialog", STATUS_BUILT, "List with New, Duplicate, Rename, Save, Export, Import, Remove.",
     "profile-manage-dialog, profile-new-menu", True),
    ("Palette layer: call-site roles", STATUS_BUILT,
     "Glass and Depth roles set at the call sites listed in each look sidecar.", "look-two-band", True),
    ("Palette layer: scoped subtree pass", STATUS_SCAFFOLD,
     "Rewrites retained stylesheets of a subtree for the preview; stands in for the call-site migration.",
     "look-dialog-*, container content", True),
    ("Cwx, Dvk, Digital and Eq applets", STATUS_NOT_WIRED,
     "Present in the source, never constructed by the current app.", "cw-digital, tx-processing", True),
    ("Phone/CW applet CW page", STATUS_NONE,
     "The CW page is not built in NereusSDR; the scene shows a plate in its place.", "cw-digital", True),
    ("Second SIGNAL STRENGTH dial in another container", STATUS_NONE,
     "The dial is a singleton owned by the Main Panel; other containers use the S-Meter bar face.",
     "mixed-container", True),
    ("Noise floor and RADE SNR readings for instruments", STATUS_NONE,
     "No producer exists; an instrument bound to them shows offline.", "", True),
    ("Drag commit and cancel", STATUS_BUILT,
     "Commit on a valid release, Escape or an invalid release restores, release outside the window floats. "
     "Unit tested.", "drag-preview-left, drag-preview-between", False),
    ("Alt bypass of the 8 px grid", STATUS_BUILT,
     "With Align to 8 px Grid on, Alt places freely.", "", False),
    ("Profile import and export", STATUS_BUILT, "Import always gets a new id.", "profile-manage-dialog", False),
    ("Scrolling strips", STATUS_BUILT,
     "A strip or stack scrolls when its content does not fit.", "mixed-container", False),
]

KNOWN_LIMITS = [
    ("Font rasterisation",
     "Every capture is from macOS (Core Text). The 1, 1.25 and 1.5 profiles show the layout at those scales, "
     "but Windows ClearType and Linux FreeType give different glyph shapes and widths at the same size, so "
     "text can fit or clip differently there. Fractional scales are Qt scale factors on a Retina screen, not "
     "a real 125% or 150% display."),
    ("\"Click to connect\" strip",
     "No radio is connected. The title strip shows a connected state through a display-only seam, while the "
     "status bar still reads \"Click to connect\". Read it as an artefact of the preview, not an app state."),
    ("Entries not shown", None),
    ("Preview-only subtree theming",
     "Where a look sidecar lists a scoped pass, the look comes from rewriting retained stylesheets in the "
     "preview, not from the planned migration of each call site to a palette role. The final code may "
     "differ in detail where a call site sets colours the pass does not reach."),
]


class Page(object):
    def __init__(self, root, out):
        self.root = root
        self.out = out
        self.cat = compare_retained.catalogue(root)
        self.sha = {}
        self.images = set()
        self.shown = set()  # (side, scene)
        self.anchor = {}
        doc_path = out / compare_retained.RESULT_DIR / compare_retained.RESULT_FILE
        self.retained = json.loads(doc_path.read_text()) if doc_path.exists() else None
        self.retained_by = {}
        if self.retained:
            for c in self.retained["comparisons"]:
                self.retained_by.setdefault((c["profile"], c["scene"]), []).append(c)
        self.profiles = self.profile_order()

    # ---- paths -----------------------------------------------------------
    def check(self, path):
        p = pathlib.Path(path).resolve()
        ok = False
        for base in (self.root, self.out):
            try:
                rel = p.relative_to(base)
            except ValueError:
                continue
            if "reference" in rel.parts:
                raise SystemExit("refusing to link %s: under a reference/ folder" % p)
            ok = True
        if not ok:
            raise SystemExit("refusing to link %s: outside the run root and the out dir" % p)
        return p

    def link(self, path):
        p = self.check(path)
        return pathlib.Path(os.path.relpath(str(p), str(self.out))).as_posix()

    def info(self, path):
        p = self.check(path)
        if p not in self.sha:
            h = hashlib.sha256()
            with open(str(p), "rb") as f:
                for chunk in iter(lambda: f.read(1 << 20), b""):
                    h.update(chunk)
            self.sha[p] = (h.hexdigest(), p.stat().st_size)
        self.images.add(p)
        return self.sha[p]

    def map_path(self, png):
        rel = png.resolve().relative_to(self.root)
        p = (self.out / rel).with_name(png.stem + sourcemap.SUFFIX)
        return p if p.exists() else None

    # ---- catalogue -------------------------------------------------------
    def profile_order(self):
        order = []
        for side in ("baseline", "proposed"):
            for mf in sorted((self.root / side).glob("manifest*.json")):
                try:
                    m = json.loads(mf.read_text())
                except ValueError:
                    continue
                for p in m.get("profiles", {}):
                    if p not in order:
                        order.append(p)
        for side in self.cat.values():
            for p in sorted(side):
                if p not in order:
                    order.append(p)
        return order

    def scenes(self, side):
        names = set()
        for scenes in self.cat[side].values():
            names.update(scenes)
        return names

    def scene_profiles(self, side, scene):
        return [p for p in self.profiles if scene in self.cat[side].get(p, {})]

    def manifests(self):
        out = []
        for side in ("baseline", "proposed"):
            for mf in sorted((self.root / side).glob("manifest*.json")):
                try:
                    out.append((side, mf, json.loads(mf.read_text())))
                except ValueError:
                    continue
        return out


def esc_attr(v):
    return html.escape(str(v), quote=True)


def fmt_bytes(n):
    if n >= 1 << 20:
        return "%.2f MB" % (n / float(1 << 20))
    if n >= 1 << 10:
        return "%.1f KB" % (n / float(1 << 10))
    return "%d B" % n


def logical_width(meta):
    dev = meta.get("device_size") or [0, 0]
    dpr = float(meta.get("dpr") or 1.0)
    return int(round(dev[0] / dpr)) if dev[0] else (meta.get("logical_size") or [0])[0]


def file_line(page, path, label):
    sha, size = page.info(path)
    return ("<div class='file'><a href='%s' target='_blank' rel='noopener'>%s</a> "
            "<span class='size'>%s</span> <code class='sha'>sha256 %s</code></div>") % (
        esc_attr(page.link(path)), esc(label), esc(fmt_bytes(size)), esc(sha))


def figure(page, img, caption=None, cls="", lines=None):
    png, meta = img["png"], img["meta"]
    cap = page.link(png)
    mp = page.map_path(png)
    mlink = page.link(mp) if mp else ""
    sha, size = page.info(png)
    if meta.get("sha256") and meta.get("sha256") != sha:
        warn = "<div class='warn'>The sidecar records sha256 %s; the file differs.</div>" % esc(meta["sha256"])
    else:
        warn = ""
    dev = meta.get("device_size") or [0, 0]
    title = caption or png.name
    if lines is None:
        lines = [file_line(page, png, "PNG %s" % png.name)]
        if mp:
            lines.append(file_line(page, mp, "Source map %s" % mp.name))
    return (
        "<figure class='shot %s'>"
        "<div class='frame'><a href='%s' target='_blank' rel='noopener'>"
        "<img loading='lazy' src='%s' data-cap='%s' data-map='%s' data-lw='%d' width='%d' height='%d' alt='%s'></a></div>"
        "<figcaption><div class='cap-title'>%s <span class='dim'>%d x %d device px, %s logical @ %s</span></div>"
        "%s%s</figcaption></figure>"
    ) % (cls, esc_attr(cap), esc_attr(cap), esc_attr(cap), esc_attr(mlink), logical_width(meta),
         dev[0], dev[1], esc_attr(png.name), esc(title), dev[0], dev[1],
         esc("x".join(str(v) for v in (meta.get("logical_size") or []))), esc(meta.get("dpr")),
         "".join(lines), warn)


def slice_line(meta):
    states = meta.get("slice_states") or meta.get("slices") or []
    parts = []
    for s in states:
        if not isinstance(s, dict):
            continue
        f = s.get("frequency_hz")
        freq = "%.3f MHz" % (f / 1e6) if isinstance(f, (int, float)) else "?"
        parts.append("%s %s %s, filter %s..%s Hz" % (s.get("slice", "?"), freq, s.get("mode", ""),
                                                      s.get("filter_low_hz", "?"), s.get("filter_high_hz", "?")))
    return "; ".join(parts)


def regions_table(meta):
    regions = meta.get("regions") or []
    if not regions:
        return ""
    rows = []
    for r in regions:
        rows.append("<tr><td>%s</td><td><span class='prov p-%s'>%s</span></td><td>%s</td><td>%s</td><td>%s</td></tr>" % (
            esc(r.get("name", "")), esc_attr(r.get("provenance", "")), esc(r.get("provenance", "")),
            esc(r.get("rect")), esc(r.get("visible_rect", "")),
            esc(r.get("widget_class") or r.get("class") or r.get("type") or "")))
    units = meta.get("units") or "logical px of the captured window (assumed: no units field)"
    return ("<h5>Regions (%d), units: %s</h5><table class='grid small'><tr><th>Name</th><th>Provenance</th>"
            "<th>rect</th><th>visible_rect</th><th>Class or type</th></tr>%s</table>") % (
        len(regions), esc(units), "".join(rows))


def retained_rows(page, comps, with_scene=False):
    rows = []
    for c in comps:
        if c["status"] == "compared":
            if c["identical"]:
                verdict = "<span class='ok'>IDENTICAL</span>"
            else:
                verdict = ("<span class='bad'>%d px differ</span> of %d, max channel delta %d, %d over %d; "
                           "bbox %s") % (c["differing_pixels"], c["total_pixels"], c["max_channel_delta"],
                                         c.get("pixels_over_delta_32", 0), compare_retained.kLargeDelta,
                                         esc(c["bbox"]))
            diff = page.out / c["diff_png"]
            thumb = ("<a href='%s' target='_blank' rel='noopener'><img class='thumb' loading='lazy' src='%s' "
                     "alt='diff'></a>") % (esc_attr(page.link(diff)), esc_attr(page.link(diff)))
            sha, size = page.info(diff)
            thumb += "<div class='file'><span class='size'>%s</span> <code class='sha'>sha256 %s</code></div>" % (
                esc(fmt_bytes(size)), esc(sha))
        else:
            verdict = "<span class='dim'>not comparable: %s</span>" % esc(c["reason"])
            thumb = ""
        excl = ", ".join(e["name"] for e in c.get("excluded") or []) or "none"
        rows.append("<tr>%s<td>%s</td><td>%s</td><td>%s<div class='dim'>%s; excluded: %s</div></td><td>%s</td></tr>" % (
            ("<td>%s</td><td><a href='#%s'>%s</a></td>" % (esc(c["profile"]), esc_attr(
                page.anchor.get(("proposed", c["scene"]), "")), esc(c["scene"]))) if with_scene else "",
            esc(c["target"]), esc(c["baseline_scene"]), verdict, esc(c.get("geometry", "")), esc(excl), thumb))
    head = "<tr>%s<th>Target</th><th>Baseline</th><th>Result</th><th>Diff</th></tr>" % (
        "<th>Profile</th><th>Scene</th>" if with_scene else "")
    return "<table class='grid'>%s%s</table>" % (head, "".join(rows))


def crop_cards(page, entry, bentry):
    groups = {}
    for img in entry["crops"]:
        base = img["part"].replace("@3x", "")
        groups.setdefault(base, {})[img["kind"]] = img
    cards = []
    for name in sorted(groups):
        g = groups[name]
        one, three = g.get("crop"), g.get("crop-3x")
        show = one or three
        lines = []
        for img in (one, three):
            if img:
                lines.append(file_line(page, img["png"], img["png"].name))
                mp = page.map_path(img["png"])
                if mp:
                    lines.append(file_line(page, mp, mp.name))
        if bentry:
            for img in bentry["crops"]:
                if img["part"] == name and img["kind"] == "crop":
                    lines.append(file_line(page, img["png"], "baseline " + img["png"].name))
        cards.append("<div class='crop'><div class='crop-name'>%s%s</div>%s</div>" % (
            esc(name), (" <a class='x3' href='%s' target='_blank' rel='noopener'>open @3x</a>" % esc_attr(
                page.link(three["png"]))) if three and one else "",
            figure(page, show, caption=show["png"].name, cls="cropfig", lines=lines)))
    if not cards:
        return ""
    return ("<details class='crops' open><summary>Detail crops (%d), each with its @3x nearest-neighbour copy"
            "</summary><div class='cropgrid'>%s</div></details>") % (len(cards), "".join(cards))


def scene_panel(page, side, scene, profile):
    entry = page.cat[side][profile][scene]
    parts = []
    bscene, how = (None, "")
    bentry = None
    if side == "proposed":
        bscene, how = compare_retained.counterpart(page.cat, scene, profile)
        if bscene:
            bentry = page.cat["baseline"][profile][bscene]
            page.shown.add(("baseline", bscene))
    mains = entry["main"]
    first = mains[0]["meta"] if mains else (entry["crops"][0]["meta"] if entry["crops"] else {})
    head = []
    if bscene:
        head.append("Baseline counterpart: <a href='#%s'>%s</a> (%s)." % (
            esc_attr(page.anchor.get(("baseline", bscene), "")), esc(bscene), esc(how)))
    elif side == "proposed":
        head.append("<span class='dim'>No baseline counterpart: %s.</span>" % esc(how))
    sl = slice_line(first)
    if sl:
        head.append("Slices: %s." % esc(sl))
    if head:
        parts.append("<p class='panel-note'>%s</p>" % " ".join(head))
    def main_cls(m):
        # A tall extra image (a full-content strip, a dialog) is held to a
        # readable height in Fit; True scale shows it whole.
        dev = m["meta"].get("device_size") or [0, 0]
        return "tall" if m["part"] and dev[1] > dev[0] else ""
    single = "".join(figure(page, m, cls=main_cls(m)) for m in mains) or \
        "<p class='dim'>No main image at this profile.</p>"
    parts.append("<div class='single'>%s</div>" % single)
    if bentry:
        bmain = bentry["main"][0] if bentry["main"] else None
        pmain = mains[0] if mains else None
        if bmain and pmain:
            parts.append("<div class='pair'><div><h5>Current app: %s</h5>%s</div><div><h5>Proposed: %s</h5>%s</div></div>" % (
                esc(bscene), figure(page, bmain), esc(scene), figure(page, pmain)))
    parts.append(crop_cards(page, entry, bentry))
    comps = page.retained_by.get((profile, scene)) if side == "proposed" else None
    if comps:
        parts.append("<h5>Retained components against %s</h5>%s" % (esc(bscene), retained_rows(page, comps)))
    meta_html = meta_table(first) + regions_table(first) if first else ""
    parts.append("<details class='meta-d'><summary>Sidecar metadata</summary>%s</details>" % meta_html)
    return "".join(parts)


def scene_article(page, side, scene, collapsed=False):
    page.shown.add((side, scene))
    aid = page.anchor[(side, scene)]
    profiles = page.scene_profiles(side, scene)
    has_map = any(page.map_path(i["png"]) for p in profiles
                  for i in page.cat[side][p][scene]["main"] + page.cat[side][p][scene]["crops"])
    has_pair = side == "proposed" and any(compare_retained.counterpart(page.cat, scene, p)[0] for p in profiles)
    first_meta = {}
    for p in profiles:
        e = page.cat[side][p][scene]
        if e["main"]:
            first_meta = e["main"][0]["meta"]
            break
    tabs = "".join("<button type='button' class='ptab%s' data-p='%s'>%s</button>" % (
        " on" if i == 0 else "", esc_attr(p), esc(p)) for i, p in enumerate(profiles))
    missing = [p for p in page.profiles if p not in profiles]
    panels = "".join("<div class='panel%s' data-p='%s'>%s</div>" % (
        " on" if i == 0 else "", esc_attr(p), scene_panel(page, side, scene, p)) for i, p in enumerate(profiles))

    def btn(group, value, label, on=False, enabled=True, why=""):
        return "<button type='button' class='vb%s' data-g='%s' data-v='%s'%s%s>%s</button>" % (
            " on" if on else "", group, value, "" if enabled else " disabled",
            (" title='%s'" % esc_attr(why)) if why else "", esc(label))
    tools = ("<div class='tools'><span class='seg'>%s%s</span><span class='seg'>%s%s</span>"
             "<span class='seg'>%s%s</span></div>") % (
        btn("map", "cap", "Capture", True), btn("map", "map", "Source map", enabled=has_map,
                                                 why="" if has_map else "no regions in this scene's sidecars"),
        btn("pair", "single", "Proposed only" if side == "proposed" else "Capture", True),
        btn("pair", "pair", "Side by side with baseline", enabled=has_pair,
            why="" if has_pair else "no baseline counterpart"),
        btn("scale", "fit", "Fit", True), btn("scale", "true", "True scale"))
    prov = first_meta.get("provenance", "")
    if prov == "proposed" and side == "proposed":
        prov = ""
    dialog_only = bool(profiles) and all(
        i["kind"] == "dialog" for p in profiles for i in page.cat[side][p][scene]["main"])
    if missing and dialog_only:
        miss = "<span class='dim'>Dialogs are captured at %s only.</span>" % esc(", ".join(profiles))
    else:
        miss = ("<span class='dim'>Not captured at %s.</span>" % esc(", ".join(missing))) if missing else ""
    body = "<div class='ptabs'>%s %s</div>%s%s" % (tabs, miss, tools, panels)
    title = "<h3>%s %s<span class='tag side-%s'>%s</span></h3>" % (
        esc(scene), ("<span class='tag'>%s</span> " % esc(prov)) if prov else "", side, "current app" if side == "baseline" else "proposed")
    if collapsed:
        return "<details class='scene-d' id='%s'><summary>%s</summary><article class='scene'>%s</article></details>" % (
            esc_attr(aid), title, body)
    return "<article class='scene' id='%s'>%s%s</article>" % (esc_attr(aid), title, body)


def not_captured_tile(name, text):
    return ("<div class='tile'><div class='tile-name'>%s</div><div>Not captured yet.</div>"
            "<div class='dim'>%s</div></div>") % (esc(name), esc(text))


def assign_sections(page):
    proposed = sorted(page.scenes("proposed"))
    placed = {}
    for sid, _, _, patterns, _ in SECTIONS:
        for pat in patterns:
            for s in proposed:
                if s not in placed and fnmatch.fnmatchcase(s, pat):
                    placed[s] = sid
    return placed


def section_html(page, placed):
    out = []
    for sid, title, need, patterns, see_also in SECTIONS:
        scenes = []
        for pat in patterns:
            for s in sorted(page.scenes("proposed")):
                if placed.get(s) == sid and fnmatch.fnmatchcase(s, pat) and s not in scenes:
                    scenes.append(s)
        arts = []
        for s in scenes:
            collapsed = any(fnmatch.fnmatchcase(s, c) for c in COLLAPSED)
            arts.append(scene_article(page, "proposed", s, collapsed))
        tiles = []
        for pat in patterns:
            if pat in PLANNED and not any(fnmatch.fnmatchcase(s, pat) for s in page.scenes("proposed")):
                tiles.append(not_captured_tile(pat.replace("*", "<page>"), PLANNED[pat]))
        also = [s for s in see_also if s in placed]
        also_html = ""
        if also:
            also_html = "<p class='also'>Also covered by: %s</p>" % ", ".join(
                "<a href='#%s'>%s</a>" % (esc_attr(page.anchor[("proposed", s)]), esc(s)) for s in also)
        count = "%d scene(s)" % len(scenes) + (", %d not captured yet" % len(tiles) if tiles else "")
        out.append("<section class='cov' id='sec-%s'><h2>%s <span class='dim'>%s</span></h2>"
                   "<p class='need'><b>What must be visible:</b> %s</p>%s%s%s</section>" % (
                       sid, esc(title), esc(count), esc(need), also_html,
                       "<div class='tiles'>%s</div>" % "".join(tiles) if tiles else "", "".join(arts)))
    return "".join(out)


def other_html(page, placed):
    others = sorted(s for s in page.scenes("proposed") if s not in placed)
    if not others:
        return ""
    return ("<section class='cov' id='sec-other'><h2>Other proposed scenes</h2><p class='need'>Scenes not "
            "assigned to a coverage item.</p>%s</section>") % "".join(
        scene_article(page, "proposed", s) for s in others)


def baseline_html(page):
    window_scenes = []
    for s in sorted(page.scenes("baseline")):
        if any(page.cat["baseline"][p][s]["main"] and page.cat["baseline"][p][s]["main"][0]["kind"] == "window"
               for p in page.scene_profiles("baseline", s)):
            window_scenes.append(s)
    rest = sorted(s for s in page.scenes("baseline") if s not in window_scenes)
    arts = "".join(scene_article(page, "baseline", s) for s in window_scenes)
    arts += "".join(scene_article(page, "baseline", s, collapsed=True) for s in rest)
    return ("<section class='cov' id='sec-baseline'><h2>Current app baseline</h2><p class='need'>The app as "
            "it is today, with the workspace preview switch off. Baseline dialogs and Setup pages also appear "
            "beside their proposed counterparts.</p>%s</section>") % arts


def header_html(page, verify):
    rows = []
    for side, mf, m in page.manifests():
        fails = [r for r in m.get("runs", []) if r.get("returncode") != 0]
        rows.append("<tr><td>%s</td><td>%s</td><td><code>%s</code>%s</td><td>%s</td><td>%d</td><td>%d</td><td>%s</td></tr>" % (
            esc(side + "/" + mf.name), esc(m.get("provenance", "")), esc(str(m.get("git_head", ""))[:10]),
            " <span class='bad'>dirty</span>" if m.get("git_dirty") else "", esc(m.get("started_utc", "")),
            len(m.get("runs", [])), m.get("image_count", 0),
            esc(", ".join("%s @ %s" % (f["scene"], f["profile"]) for f in fails)) if fails else "none"))
    revisions = ("<table class='grid'><tr><th>Manifest</th><th>Provenance</th><th>Revision</th><th>Captured (UTC)"
                 "</th><th>Runs</th><th>Images</th><th>Failed runs</th></tr>%s</table>") % "".join(rows)
    env = {}
    tags = {}
    for side in page.cat.values():
        for scenes in side.values():
            for entry in scenes.values():
                for img in entry["main"]:
                    e = img["meta"].get("environment") or {}
                    f = e.get("font") or {}
                    key = (e.get("qt_runtime"), e.get("qt_compiled"), e.get("platform"), e.get("os"),
                           e.get("style"), "%s %s pt" % (f.get("family"), f.get("point_size")),
                           e.get("spectrum_rhi_backend"))
                    env[key] = env.get(key, 0) + 1
                    t = e.get("build_tag")
                    if t:
                        tags[t] = tags.get(t, 0) + 1
    env_rows = "".join("<tr><td>%s</td><td>%s</td><td>%s / %s</td><td>%s</td><td>%s</td><td>%s</td><td>%d</td></tr>" % (
        esc(k[0]), esc(k[1]), esc(k[2]), esc(k[3]), esc(k[4]), esc(k[5]), esc(k[6]), n)
        for k, n in sorted(env.items(), key=lambda kv: -kv[1]))
    envt = ("<table class='grid'><tr><th>Qt runtime</th><th>Qt compiled</th><th>Platform / OS</th><th>Style</th>"
            "<th>Application font</th><th>Spectrum backend</th><th>Images</th></tr>%s</table>") % env_rows
    tagt = "<p class='dim'>Build tags: %s</p>" % esc("; ".join("%s (%d)" % kv for kv in sorted(tags.items())))
    profs = {}
    dialog_profiles = set()
    for _, _, m in page.manifests():
        for p, info in m.get("profiles", {}).items():
            profs.setdefault(p, info)
        dialog_profiles.update(m.get("dialog_profiles") or [])
    prow = []
    for p in page.profiles:
        info = profs.get(p, {})
        n = sum(len(e["main"]) + len(e["crops"]) for side in page.cat.values() for e in side.get(p, {}).values())
        prow.append("<tr><td>%s</td><td>%s</td><td>%s</td><td>%s</td><td>%s</td><td>%d</td></tr>" % (
            esc(p), esc("x".join(str(v) for v in info.get("logical_size", []))), esc(info.get("dpr", "")),
            esc(info.get("qt_scale_factor", "")), "yes" if p in dialog_profiles else "no", n))
    proft = ("<table class='grid'><tr><th>Profile</th><th>Logical size</th><th>DPR</th><th>QT_SCALE_FACTOR</th>"
             "<th>Dialogs captured</th><th>Images</th></tr>%s</table>") % "".join(prow)
    return ("<header class='top'><h1>NereusSDR workspace visual review</h1>"
            "<p class='sub'>The current app and the proposed Longpath workspace, rendered by the real Qt components "
            "in the preview tool. Generated %s.</p>"
            "<div class='hgrid'><div><h4>Capture runs and revisions</h4>%s%s</div>"
            "<div><h4>Environment</h4>%s<h4>Profiles</h4>%s<h4>Verify status</h4>%s</div></div></header>") % (
        esc(datetime.datetime.now(datetime.timezone.utc).replace(microsecond=0).isoformat()),
        revisions, tagt, envt, proft, verify)


def verify_html(page, verify_log):
    files = []
    if verify_log:
        files.append(pathlib.Path(verify_log).resolve())
    for pat in ("verify*.txt", "verify*.log"):
        files.extend(sorted(page.root.glob(pat)))
        if page.out != page.root:
            files.extend(sorted(page.out.glob(pat)))
    seen, blocks = set(), []
    for f in files:
        if f in seen or not f.exists():
            continue
        seen.add(f)
        text = f.read_text(errors="replace").splitlines()
        ident = sum(1 for l in text if "IDENTICAL" in l)
        differs = [l for l in text if "DIFFERS" in l or "RUN FAILED" in l or "missing" in l
                   or "no stored image" in l or "no longer produced" in l]
        summary = [l for l in text if l.startswith("verify")]
        state = "<span class='ok'>all identical</span>" if not differs and ident else (
            "<span class='bad'>%d difference(s) or failure(s)</span>" % len(differs) if differs else "no results")
        blocks.append("<p><b>%s</b>: %d IDENTICAL line(s), %s. %s</p>%s" % (
            esc(f.name), ident, state, esc(" ".join(summary)),
            ("<pre>%s</pre>" % esc("\n".join(differs[:40]))) if differs else ""))
    if not blocks:
        return ("<p class='bad'>No verify record in the run root. Save the output of capture.py --verify as "
                "verify.txt in the run root, or pass --verify-log.</p>")
    return "".join(blocks)


def status_html():
    rows = []
    for name, status, what, scenes, still in STATUS_ROWS:
        cls = {STATUS_BUILT: "ok", STATUS_SCAFFOLD: "warn", STATUS_NONE: "bad", STATUS_NOT_WIRED: "warn"}[status]
        rows.append("<tr><td>%s</td><td><span class='%s'>%s</span>%s</td><td>%s</td><td>%s</td></tr>" % (
            esc(name), cls, esc(status), "" if still else "<br><span class='stills'>%s</span>" % STILLS,
            esc(what), esc(scenes)))
    return ("<section class='cov' id='sec-status'><h2>Status of every proposed element</h2><p class='need'>"
            "Built in preview code: reachable only through the workspace preview switch, with tests where "
            "stated. Scaffolding only: stands in for the planned code. Behaviour marked not verifiable from "
            "stills is tested in code, not shown by these images.</p><table class='grid'><tr><th>Element</th>"
            "<th>Status</th><th>What it is</th><th>Scenes</th></tr>%s</table></section>") % "".join(rows)


def limits_html(page):
    not_shown = {}
    for _, _, m in page.manifests():
        for it in m.get("not_shown") or []:
            if isinstance(it, dict):
                not_shown.setdefault(it.get("item", ""), it.get("reason", ""))
    items = []
    for title, text in KNOWN_LIMITS:
        if text is None:
            text = ("Not shown in any scene, because each appears only once a device setting that starts "
                    "discovery or a connection is on: %s.") % "; ".join(
                "%s (%s)" % kv for kv in sorted(not_shown.items()))
        items.append("<li><b>%s.</b> %s</li>" % (esc(title), esc(text)))
    return "<section class='cov' id='sec-limits'><h2>Known limits</h2><ul class='limits'>%s</ul></section>" % "".join(items)


def retained_html(page):
    if not page.retained:
        return ("<section class='cov' id='sec-retained'><h2>Retained-component proof</h2><p class='bad'>"
                "compare_retained.py has not run for this out dir.</p></section>")
    doc = page.retained
    comps = doc["comparisons"]
    compared = [c for c in comps if c["status"] == "compared"]
    reasons = {}
    for n in doc["not_possible"]:
        reasons.setdefault((n.get("target", "any"), n["reason"]), []).append("%s @ %s" % (n["scene"], n["profile"]))
    np_rows = "".join("<tr><td>%s</td><td>%s</td><td>%d</td><td class='dim small'>%s</td></tr>" % (
        esc(t), esc(r), len(v), esc(", ".join(v))) for (t, r), v in sorted(reasons.items()))
    method = doc.get("method", {})
    return ("<section class='cov' id='sec-retained'><h2>Retained-component proof</h2><p class='need'>The slice "
            "flag crop and the spectrum plot area of each proposed scene, compared pixel by pixel with the "
            "baseline scene that shows the same station data at the same profile. Expected: identical, or "
            "different only where the flag's translucent background shows what is behind it. In each diff the "
            "baseline is dim grey, differing pixels are magenta, their bounding box is amber and excluded "
            "areas are outlined grey.</p><p class='dim'>Counterparts: %s. Spectrum: %s.</p>"
            "<p>%d compared, %d identical, %d not comparable, %d not possible.</p>%s"
            "<h4>Comparisons not possible</h4><table class='grid'><tr><th>Target</th><th>Reason</th><th>Count</th>"
            "<th>Scene @ profile</th></tr>%s</table></section>") % (
        esc(method.get("counterparts", "")), esc(method.get("spectrum", "")), len(compared),
        sum(1 for c in compared if c["identical"]), len(comps) - len(compared), len(doc["not_possible"]),
        retained_rows(page, comps, with_scene=True), np_rows)


REVIEW_CSS = """
:root { color-scheme: dark; --bg:#08080a; --panel:#0c0c0e; --panel2:#141417; --raise:#1a1a1e; --line:#2c2c31;
        --line2:#1f1f23; --text:#dcdce1; --text2:#a6a6ac; --muted:#909096; --accent:#4a7ba8; --sel:#2f5f92;
        --amber:#d8a55f; --ok:#6fa384; --bad:#c25a5c; --warn:#a8853f; }
* { box-sizing:border-box; }
body { margin:0; background:var(--bg); color:var(--text);
       font:14px/1.45 -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, sans-serif; }
a { color:#7fb0dc; }
code, .sha, pre { font-family: Menlo, "SF Mono", Consolas, monospace; }
header.top { padding:16px; border-bottom:1px solid var(--line); background:var(--panel); }
h1 { font-size:20px; margin:0 0 4px; letter-spacing:.02em; }
h2 { font-size:17px; margin:0 0 6px; color:var(--amber); }
h3 { font-size:15px; margin:0 0 6px; }
h4 { font-size:12px; margin:14px 0 6px; color:var(--text2); text-transform:uppercase; letter-spacing:.08em; }
h5 { font-size:12px; margin:10px 0 4px; color:var(--text2); }
.sub, .dim { color:var(--muted); }
.hgrid { display:grid; grid-template-columns:repeat(auto-fit, minmax(460px, 1fr)); gap:16px; }
nav.bar { position:sticky; top:0; z-index:5; background:rgba(8,8,10,.96); border-bottom:1px solid var(--line);
          padding:6px 16px; display:flex; flex-wrap:wrap; gap:6px 14px; align-items:center; }
nav.bar .secs { flex:1 1 600px; min-width:0; line-height:1.9; }
nav.bar .secs a { color:var(--text2); text-decoration:none; font-size:12px; margin-right:12px; white-space:nowrap; display:inline-block; }
nav.bar .secs a:hover { color:var(--text); }
.gprof button, .ptab, .vb { background:linear-gradient(#222227,#141417); color:var(--text2); border:1px solid #38383e;
       border-bottom-color:#1f1f23; border-radius:4px; padding:3px 9px; font:inherit; font-size:12px; cursor:pointer; }
.gprof button.on, .ptab.on, .vb.on { background:linear-gradient(#2f5f92,#1e3d5f); color:#dbe9f8; border-color:#3d78b0; }
.vb[disabled] { opacity:.35; cursor:not-allowed; }
.gprof { display:flex; gap:4px; flex-wrap:wrap; }
main { padding:0 16px 60px; }
section.cov { border-top:1px solid var(--line); padding:20px 0 8px; scroll-margin-top:90px; }
article.scene, details.scene-d { scroll-margin-top:90px; }
.need { color:var(--text2); max-width:1100px; }
.also { font-size:13px; }
article.scene { background:var(--panel); border:1px solid var(--line2); border-radius:8px; padding:12px; margin:12px 0; }
details.scene-d { background:var(--panel); border:1px solid var(--line2); border-radius:8px; margin:8px 0; padding:6px 12px; }
details.scene-d > summary { cursor:pointer; } details.scene-d > summary h3 { display:inline; }
details.scene-d article.scene { border:0; padding:6px 0; margin:0; }
.tag { font-size:11px; font-weight:500; color:var(--text2); border:1px solid var(--line); border-radius:9px; padding:1px 7px; }
.tag.side-baseline { color:#3fa7a0; border-color:#2a5f5b; } .tag.side-proposed { color:#7fb0dc; border-color:#2d4a66; }
.ptabs { display:flex; flex-wrap:wrap; gap:4px; align-items:center; margin:4px 0; }
.tools { display:flex; flex-wrap:wrap; gap:10px; margin:6px 0 8px; }
.seg { display:inline-flex; gap:2px; }
.panel { display:none; } .panel.on { display:block; }
.panel-note { font-size:13px; color:var(--text2); margin:4px 0 8px; }
.pair { display:none; grid-template-columns:1fr 1fr; gap:10px; }
.scene.pairmode .pair { display:grid; } .scene.pairmode .single { display:none; }
figure.shot { margin:0 0 10px; background:var(--panel2); border:1px solid var(--line2); border-radius:6px; overflow:hidden; }
.frame { overflow:auto; background:#000; max-height:none; }
.frame img { display:block; max-width:100%; height:auto; }
.scene.truemode .frame img { max-width:none; }
figure.shot.tall .frame img { max-height:1000px; width:auto; }
.scene.truemode figure.shot.tall .frame img { max-height:none; }
.scene.truemode .pair { grid-template-columns:1fr; }
figcaption { padding:6px 8px; font-size:12px; }
.cap-title { font-weight:600; margin-bottom:2px; word-break:break-all; }
.file { font-size:11px; color:var(--muted); word-break:break-all; }
.size { color:var(--text2); }
.sha { font-size:10.5px; color:var(--muted); }
.warn { color:var(--warn); font-size:12px; }
.cropgrid { display:grid; grid-template-columns:repeat(auto-fill, minmax(330px, 1fr)); gap:10px; margin-top:6px; }
.scene.truemode .cropgrid { grid-template-columns:1fr; }
.crop-name { font-size:12px; font-weight:600; color:var(--amber); margin-bottom:2px; }
.crop-name a.x3 { font-weight:500; margin-left:6px; }
.cropfig .frame { max-height:420px; }
.scene.truemode .cropfig .frame { max-height:none; }
details.crops > summary, details.meta-d > summary { cursor:pointer; color:var(--text2); font-size:13px; margin:6px 0; }
table.grid { border-collapse:collapse; width:100%; font-size:12px; margin:4px 0 8px; }
table.grid th { text-align:left; color:var(--text2); font-weight:600; border-bottom:1px solid var(--line); padding:4px 8px 4px 0; }
table.grid td { border-bottom:1px solid var(--line2); padding:4px 8px 4px 0; vertical-align:top; }
table.grid.small td { font-size:11px; }
table.meta { border-collapse:collapse; margin-top:6px; font-size:12px; width:100%; }
table.meta th { text-align:left; color:var(--muted); font-weight:500; padding:3px 10px 3px 0; vertical-align:top;
                white-space:nowrap; width:1%; }
table.meta td { padding:3px 0; word-break:break-all; }
img.thumb { max-width:280px; display:block; border:1px solid var(--line); }
.ok { color:var(--ok); font-weight:600; } .bad { color:var(--bad); font-weight:600; } .warn { color:var(--warn); }
.stills { color:var(--amber); font-size:11px; }
.prov { font-size:11px; padding:0 5px; border-radius:3px; }
.p-retained { background:#1d3b39; } .p-longpath-port { background:#3b2f1d; } .p-proposed { background:#1e2c3b; }
.p-exists-not-wired { background:#2e2640; } .p-not-implemented { background:#2a2a2e; }
.tiles { display:grid; grid-template-columns:repeat(auto-fill, minmax(300px, 1fr)); gap:10px; margin:10px 0; }
.tile { border:1px dashed #3d3d41; border-radius:8px; padding:12px; background:var(--panel); }
.tile-name { font-family:Menlo, monospace; color:var(--amber); }
ul.limits li { margin-bottom:6px; max-width:1100px; }
pre { background:var(--panel2); padding:8px; font-size:11px; overflow:auto; }
@media (max-width: 900px) { .pair { grid-template-columns:1fr; } .hgrid { grid-template-columns:1fr; } }
"""

REVIEW_JS = """
function setMap(scene, on) {
  scene.querySelectorAll('img[data-cap]').forEach(function (img) {
    var src = on && img.dataset.map ? img.dataset.map : img.dataset.cap;
    if (img.getAttribute('src') !== src) { img.setAttribute('src', src); }
    img.parentNode.setAttribute('href', src);
  });
}
function setScale(scene, trueScale) {
  scene.classList.toggle('truemode', trueScale);
  scene.querySelectorAll('img[data-lw]').forEach(function (img) {
    img.style.width = trueScale ? img.dataset.lw + 'px' : '';
  });
}
function showProfile(scene, p) {
  var tab = scene.querySelector('.ptab[data-p="' + p + '"]');
  if (!tab) { return false; }
  scene.querySelectorAll('.ptab').forEach(function (t) { t.classList.toggle('on', t === tab); });
  scene.querySelectorAll('.panel').forEach(function (x) { x.classList.toggle('on', x.dataset.p === p); });
  return true;
}
document.querySelectorAll('article.scene').forEach(function (scene) {
  scene.querySelectorAll('.ptab').forEach(function (t) {
    t.addEventListener('click', function () { showProfile(scene, t.dataset.p); });
  });
  scene.querySelectorAll('.vb').forEach(function (b) {
    b.addEventListener('click', function () {
      if (b.disabled) { return; }
      scene.querySelectorAll('.vb[data-g="' + b.dataset.g + '"]').forEach(function (x) { x.classList.toggle('on', x === b); });
      if (b.dataset.g === 'map') { setMap(scene, b.dataset.v === 'map'); }
      if (b.dataset.g === 'pair') { scene.classList.toggle('pairmode', b.dataset.v === 'pair'); }
      if (b.dataset.g === 'scale') { setScale(scene, b.dataset.v === 'true'); }
    });
  });
});
document.querySelectorAll('.gprof button').forEach(function (b) {
  b.addEventListener('click', function () {
    document.querySelectorAll('.gprof button').forEach(function (x) { x.classList.toggle('on', x === b); });
    document.querySelectorAll('article.scene').forEach(function (s) { showProfile(s, b.dataset.p); });
    try { localStorage.setItem('nereusReviewProfile', b.dataset.p); } catch (e) {}
  });
});
try {
  var saved = localStorage.getItem('nereusReviewProfile');
  var btn = saved && document.querySelector('.gprof button[data-p="' + saved + '"]');
  if (btn) { btn.click(); }
} catch (e) {}
// Deep links: ?section=<id>&profile=<name>&view=map,pair,true shows one coverage
// section only and opens every scene at that profile and in that view.
(function () {
  var q = new URLSearchParams(location.search);
  var only = q.get('section');
  if (only) {
    document.querySelectorAll('section.cov').forEach(function (x) { x.style.display = x.id === 'sec-' + only ? '' : 'none'; });
    var h = document.querySelector('header.top'); if (h) { h.style.display = 'none'; }
  }
  var p = q.get('profile');
  var pb = p && document.querySelector('.gprof button[data-p="' + p + '"]');
  if (pb) { pb.click(); }
  (q.get('view') || '').split(',').forEach(function (v) {
    if (!v) { return; }
    document.querySelectorAll('.vb[data-v="' + v + '"]').forEach(function (b) { if (!b.disabled) { b.click(); } });
  });
  if (location.hash) { var t = document.getElementById(location.hash.slice(1)); if (t) { t.scrollIntoView(); } }
})();
"""


def build_review_page(root, out, verify_log=None):
    page = Page(root, out)
    placed = assign_sections(page)
    n = 0
    for side in ("proposed", "baseline"):
        for s in sorted(page.scenes(side)):
            n += 1
            page.anchor[(side, s)] = "s%d-%s" % (n, s)
    verify = verify_html(page, verify_log)
    sections = section_html(page, placed) + other_html(page, placed)
    baseline = baseline_html(page)
    retained = retained_html(page)
    nav = ("<nav class='bar'><div class='gprof'>%s</div><div class='secs'>%s</div></nav>") % (
        "".join("<button type='button' data-p='%s'>%s</button>" % (esc_attr(p), esc(p)) for p in page.profiles),
        "".join("<a href='#sec-%s'>%s</a>" % (sid, esc(t)) for sid, t, _, _, _ in SECTIONS) +
        "<a href='#sec-baseline'>Current app</a><a href='#sec-retained'>Retained proof</a>"
        "<a href='#sec-status'>Status</a><a href='#sec-limits'>Known limits</a>")
    body = header_html(page, verify) + nav + "<main>" + sections + baseline + retained + status_html() + \
        limits_html(page) + "</main>"
    doc = ("<!doctype html><html lang='en'><head><meta charset='utf-8'>"
           "<meta name='viewport' content='width=device-width, initial-scale=1'>"
           "<title>Workspace Visual Review</title><style>%s</style></head><body>%s<script>%s</script></body></html>") % (
        REVIEW_CSS, body, REVIEW_JS)
    out.mkdir(parents=True, exist_ok=True)
    target = out / "index.html"
    target.write_text(doc)
    # Every capture scene listed in a manifest must be on the page.
    missing = []
    for side, mf, m in page.manifests():
        for r in m.get("runs", []):
            if r.get("returncode") == 0 and (side, r["scene"]) not in page.shown:
                missing.append("%s/%s" % (side, r["scene"]))
    unshown = sorted("%s/%s" % (side, s) for side in ("baseline", "proposed") for s in page.scenes(side)
                     if (side, s) not in page.shown)
    print("wrote %s: %d proposed and %d baseline scenes, %d images linked" % (
        target, len(page.scenes("proposed")), len(page.scenes("baseline")), len(page.images)))
    for sid, _, _, patterns, _ in SECTIONS:
        names = [s for s in sorted(page.scenes("proposed")) if placed.get(s) == sid]
        print("  %-11s %s" % (sid, ", ".join(names) if names else "(none captured)"))
    if missing or unshown:
        print("NOT ON THE PAGE: %s" % ", ".join(sorted(set(missing + unshown))))
        return 1
    return 0


def main(argv=None):
    ap = argparse.ArgumentParser(description="Build the visual review page.")
    ap.add_argument("root", help="a run root (baseline/ and proposed/) or a single capture.py run dir")
    ap.add_argument("--out", help="run-root mode: write index.html, source maps and retained/ here "
                                  "(default: the run root)")
    ap.add_argument("--verify-log", help="run-root mode: a saved capture.py --verify output to show")
    ap.add_argument("--no-tools", action="store_true",
                    help="run-root mode: do not run sourcemap.py and compare_retained.py first")
    args = ap.parse_args(argv)
    root = pathlib.Path(args.root).resolve()
    if not ((root / "baseline").is_dir() and (root / "proposed").is_dir()):
        if args.out or args.verify_log:
            sys.exit("--out and --verify-log apply to a run root (baseline/ and proposed/)")
        return build_run_page(root)
    out = pathlib.Path(args.out).resolve() if args.out else root
    if not args.no_tools:
        sm = ["--quiet", str(root)] + (["--out", str(out)] if out != root else [])
        rc = sourcemap.main(sm)
        if rc:
            return rc
        doc, path = compare_retained.run(root, out)
        print("retained: %d comparison(s), %d not possible; %s" % (
            len(doc["comparisons"]), len(doc["not_possible"]), path))
    return build_review_page(root, out, args.verify_log)


if __name__ == "__main__":
    sys.exit(main())
