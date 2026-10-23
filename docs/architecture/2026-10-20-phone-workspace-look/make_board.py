#!/usr/bin/env python3
"""Build the workspace-look mockup from the iPhone design board.

Reads docs/architecture/2026-09-23-iphone-app-design/board.html, keeps the
screens under review, and adds a second stylesheet in which every chrome
colour is replaced by its workspace role (src/gui/styles/WorkspacePalette.h).
?look=today shows the board as it is; the default shows the workspace look.
Spectrum, waterfall, band plan, slice colours and frequency readouts are
left alone.
"""
import colorsys
import pathlib
import re
import sys

HERE = pathlib.Path(__file__).resolve().parent
SRC = HERE / "source-board.html"  # the design board as of 2026-10-04 (finger-sized flag, TX redraw)
OUT = HERE / "workspace-board.html"

KEEP_ORDER = ["On the band", "Flag controls at finger size, new for review",
              "TX quick controls and Modes settings", "Tabs", "iPad", "iPhone turned sideways",
              "Spot Hub", "The lock screen"]
KEEP_SECTIONS = set(KEEP_ORDER)
TITLE = "The workspace look, proposal"

# `python3 make_board.py rest` builds the second board: the rest of the app.
REST_ORDER = ["First launch", "Pairing and connecting: the states in between", "Connecting",
              "Every slice on the Core from the Slice button, new for review", "Tuning dial",
              "TX EQ curve, new for review", "AM Mod Monitor, new for review",
              "Transmit stage meters, new for review", "Amps and tuner", "Spots",
              "When things aren't right"]

# `python3 make_board.py lock` builds the third: the lock screen and the island, close up.
LOCK_ORDER = ["The lock screen", "In another app"]

# `python3 make_board.py more` builds the fourth: every other screen on the design board, in its order.
MORE_ORDER = ["PTT overlay meter choices", "Diversity as a slice badge, new for review",
              "The WIDE chip on the band, new for review",
              "Receive level calibration and the preamp list, new for review",
              "Taking control of a slice, new for review", "RADE on the VFO flag, new for review",
              "The 3D spectrum view, new for review", "Settings backup and restore, new for review",
              "Connection header and details preview", "Approved Connection and performance diagnostics layout",
              "Moving the split, and the dBm scale's arrows", "Tuning on the band", "The Pan and Display buttons",
              "Several devices: states and notices", "Several devices at once", "Transmit time-out",
              "Long sessions", "A hardware PTT button", "Audio and data use", "FreeDV Reporter",
              "iPad held upright", "The station's side"]

# Old phone colour -> workspace colour. The first block is the desktop's own
# legacy table (WorkspacePalette.cpp legacyRows); the rest are the phone's
# extra values placed on the nearest role.
MAP = {
    # surfaces
    "#0f0f1a": "#08080a", "#0b0b13": "#08080a", "#0a0a18": "#0c0c0e", "#0a0a14": "#0a0a0c",
    "#1a1a2a": "#141417", "#07070d": "#050507", "#10101c": "#050507", "#0d1b28": "#141418",
    "#0f1a26": "#121215", "#0f1420": "#0c0c0e", "#131326": "#141417", "#1b1d26": "#18181b",
    "#1c1a24": "#18181b", "#2c303c": "#2c2c31", "#1c1c1e": "#141417",
    # text
    "#c8d8e8": "#dcdce1", "#e4eef8": "#ececf0", "#8090a0": "#a6a6ac", "#708090": "#909096",
    "#607080": "#7e7e85", "#405060": "#58585e", "#8899aa": "#828288", "#6888a0": "#828288",
    "#8aa8c0": "#c4c4ca", "#556070": "#4e4e53", "#a8b8c8": "#c4c4ca", "#a0b4c4": "#c4c4ca",
    "#404858": "#3d3d41",
    # buttons and borders
    "#1a2a3a": "#1a1a1e", "#204060": "#2f3138", "#205070": "#2c2c31", "#203040": "#1f1f23",
    "#304050": "#2c2c31", "#1e2e3e": "#232329", "#2a3040": "#232327", "#2a4a60": "#3d3d41",
    "#305070": "#2c2c31", "#2a3a4a": "#26262b", "#2a3a4c": "#26262b", "#233244": "#26262b",
    "#1a3a5a": "#1a1a1e",
    # title gradient
    "#3a4a5a": "#26262b", "#1a2a38": "#141417", "#0a1a28": "#0d0d0f",
    # chrome accent and selection
    "#00b4d8": "#4a7ba8", "#0070c0": "#2a5785", "#0090e0": "#3d78b0", "#0088d8": "#2d5885",
    "#0080d0": "#3d78b0", "#1a5ca8": "#2a5785", "#0a84ff": "#3d78b0", "#5fa8ff": "#5e8db8",
    "#3a7cc8": "#3d78b0",
    # confirmed and live (green)
    "#006040": "#1c3a2a", "#00ff88": "#6fa384", "#00a060": "#2c5c44", "#1a6030": "#1b3527",
    "#20a040": "#33684c", "#80ff80": "#7fae91", "#1a5030": "#1c3a2a", "#006030": "#1c3a2a",
    "#008040": "#2c5c44", "#20603a": "#1c3a2a", "#00e060": "#4caf6a", "#00d860": "#4caf6a",
    "#39c167": "#4caf6a", "#5fff8a": "#4caf6a", "#3fcf6a": "#4caf6a", "#a0d8a0": "#6fa384",
    # measured and warning (amber)
    "#604000": "#33280f", "#906000": "#6b5426", "#ffb800": "#d8a55f", "#ffd700": "#d8a55f",
    "#ddbb00": "#a8853f", "#ffc040": "#d8a55f", "#3a3010": "#33280f", "#a07000": "#6b5426",
    "#8a6000": "#6b5426", "#d39c2a": "#a8853f", "#ff9f0a": "#d8a55f",
    # transmit and danger (red)
    "#ff4444": "#c25a5c", "#cc2222": "#7a2c2e", "#ff8080": "#e0a3a4", "#6a3030": "#3f2224",
    "#4a2020": "#3f2224", "#c8a8a8": "#f0dcdc", "#c14848": "#c25a5c", "#ff6b6b": "#c25a5c",
    "#ff5050": "#a86b6d", "#ff8a8a": "#e0a3a4", "#ff6060": "#c25a5c", "#3a1c20": "#3f2224",
    "#2a1014": "#2a1416", "#a07078": "#a86b6d", "#6a4048": "#5a3a3c", "#ffd0d0": "#f0dcdc",
    "#ffb0b0": "#f0dcdc", "#d09098": "#e0a3a4", "#b07880": "#a86b6d", "#ff3b30": "#c25a5c",
}

# Kept on purpose: slice colours, frequency readouts, the TX filter, spot DXCC colours.
KEEP = {"#00d4ff", "#ff40ff", "#40ff40", "#ffff00", "#00e5ff", "#00c8ff", "#ff7833", "#ffffff",
        "#000000", "#fff", "#c8f8ff", "#ff3030", "#ff8c00", "#ffaa00", "#606060"}

RGBA_MAP = {
    (0, 180, 216): (74, 123, 168), (15, 15, 26): (8, 8, 10), (10, 10, 20): (10, 10, 12),
    (95, 168, 255): (94, 141, 184), (255, 68, 68): (194, 90, 92), (128, 144, 160): (144, 144, 150),
    (255, 96, 96): (194, 90, 92), (255, 80, 80): (168, 107, 109), (255, 215, 0): (216, 165, 95),
    (221, 187, 0): (168, 133, 63), (200, 216, 232): (220, 220, 225), (15, 20, 32): (12, 12, 14),
    (10, 12, 20): (10, 10, 12), (80, 100, 130): (60, 60, 66), (8, 14, 22): (8, 8, 10),
    (48, 80, 112): (44, 44, 49), (32, 12, 16): (42, 20, 22), (30, 36, 48): (28, 28, 32),
    (234, 241, 252): (236, 236, 240), (228, 236, 248): (236, 236, 240), (20, 30, 50): (20, 20, 23),
    (20, 30, 45): (20, 20, 23), (193, 72, 72): (194, 90, 92),
}


def neutral(hexv):
    """Fallback for unlisted navy greys: same lightness, the blue cast removed."""
    h = hexv.lstrip("#")
    if len(h) == 3:
        h = "".join(c * 2 for c in h)
    r, g, b = (int(h[i:i + 2], 16) / 255 for i in (0, 2, 4))
    hue, light, sat = colorsys.rgb_to_hls(r, g, b)
    if sat > 0.55 and light > 0.2:
        return hexv  # a real colour, not a navy grey
    v = round(light * 255)
    return "#%02x%02x%02x" % (v, v, min(255, v + 4))


def map_hex(m):
    hexv = m.group(0).lower()
    if hexv in KEEP:
        return hexv
    return MAP.get(hexv) or neutral(hexv)


def map_rgba(m):
    rgb = tuple(int(x) for x in m.group(1, 2, 3))
    r, g, b = RGBA_MAP.get(rgb, rgb)
    return "rgba(%d, %d, %d" % (r, g, b)


HEX = re.compile(r"#[0-9a-fA-F]{6}\b|#[0-9a-fA-F]{3}\b")
RGBA = re.compile(r"rgba\((\d+),\s*(\d+),\s*(\d+)")

RAISED = ("background: linear-gradient(180deg, #222227, #141417); "
          "box-shadow: inset 0 1px 0 rgba(255, 255, 255, 0.07), 0 1px 0 rgba(0, 0, 0, 0.55)")
SELECTED = ("background: linear-gradient(180deg, #2f5f92, #1e3d5f); border-color: #3d78b0; color: #dbe9f8; "
            "box-shadow: inset 0 1px 0 rgba(255, 255, 255, 0.10)")
PANEL = "background: linear-gradient(180deg, #17171b, #0e0e10)"
DANGER_ON = "background: #7a2c2e; border-color: #a86b6d; color: #f0dcdc"
# The desktop's lit green (WorkspaceComponents.cpp litTreatment), sampled from
# docs/manual/images/desktop-tx-workspace-original.png (SPEAKERS, LEV).
LIT_GREEN = ("background: linear-gradient(180deg, #2f6146, #1f412f); border-color: #3e7e5c; color: #bbe1c6; "
             "box-shadow: inset 0 1px 0 rgba(255,255,255,0.06)")
KEYED = "background: #c25a5c; border-color: #e0a3a4; color: #ffffff"


def workspace_css(css):
    out = []
    pos = 0
    for m in re.finditer(r"([^{}]+)\{([^{}]*)\}", css):
        out.append(css[pos:m.start()])
        sel, body = m.group(1), m.group(2)
        low = body.lower()
        extra = []
        if re.search(r"background:\s*#1a2a3a\s*(;|$)", low):
            extra.append(RAISED)
            if re.search(r"border-radius:\s*[2-6]px", low):
                extra.append("border-radius: 7px")
        if re.search(r"background:\s*#(0070c0|1a5ca8)\s*(;|$)", low):
            extra.append(SELECTED)
            if re.search(r"border-radius:\s*[2-6]px", low):
                extra.append("border-radius: 7px")
        if re.search(r"background:\s*#0d1b28\s*(;|$)", low):
            extra.append(PANEL)
            if re.search(r"border-radius:\s*[2-9]px", low):
                extra.append("border-radius: 10px")
        if re.search(r"background:\s*#(1a6030|006040)\s*(;|$)", low):
            extra.append(LIT_GREEN)
            if re.search(r"border-radius:\s*[2-6]px", low):
                extra.append("border-radius: 7px")
        if re.search(r"background:\s*#cc2222", low):
            extra.append(KEYED if ".ptt" in sel else DANGER_ON)
        body = HEX.sub(map_hex, body)
        body = RGBA.sub(map_rgba, body)
        if extra:
            body = body.rstrip().rstrip(";") + ";\n    " + ";\n    ".join(extra) + ";\n  "
        out.append(sel + "{" + body + "}")
        pos = m.end()
    out.append(css[pos:])
    return "".join(out)


WS_EXTRA = """
  /* Workspace additions: things the colour swap alone does not carry. */
  .tbtn.is-open { color: #dbe9f8; background: linear-gradient(180deg, #2f5f92, #1e3d5f); }
  .tab[aria-current="page"] { color: #8fb4d9; }
  .af input[type="range"]::-webkit-slider-thumb { background: linear-gradient(180deg, #3a3a40, #222227); border: 1px solid #3d78b0; }
  .af input[type="range"]::-webkit-slider-runnable-track { background: linear-gradient(to right, #3d78b0 var(--p), #1f1f23 var(--p)); }
  /* Setup switches when on are the desktop's checked blue (JJ, 2026-10-20). */
  .scr .stog.is-on, .win .dtog.is-on { background: linear-gradient(180deg, #2f5f92, #1e3d5f); border-color: #3d78b0; color: #dbe9f8; }
  .bkp-btn--go { background: linear-gradient(180deg, #2f5f92, #1e3d5f); border-color: #3d78b0; color: #dbe9f8; }
"""

LOOK_PICK = "<script>window.WS_LOOK = /look=today/.test(location.search) ? 'today' : 'workspace';</script>\n"
LOOK_APPLY = ("<script>document.querySelectorAll('style[data-look]').forEach(function (st) {"
              "st.disabled = st.dataset.look !== window.WS_LOOK; });</script>")

LOOK_BAR = """
<style>
  .lookbar { position: sticky; top: 0; z-index: 50; display: flex; gap: 8px; align-items: center;
    padding: 10px 16px; margin: -28px -16px 20px; background: rgba(8, 8, 10, 0.92);
    border-bottom: 1px solid #2c2c31; font: 13px system-ui, sans-serif; color: #a6a6ac; }
  .lookbar b { color: #dcdce1; margin-right: 8px; }
  .lookbar a { padding: 6px 14px; border-radius: 7px; border: 1px solid #2c2c31; color: #dcdce1;
    text-decoration: none; background: linear-gradient(180deg, #222227, #141417); }
  .lookbar a.on { background: linear-gradient(180deg, #2f5f92, #1e3d5f); border-color: #3d78b0; color: #dbe9f8; }
  .lookbar span { margin-left: auto; }
</style>
<div class="lookbar"><b>Look</b>
  <a href="?look=today" data-look="today">Today (navy)</a>
  <a href="?look=workspace" data-look="workspace">Workspace</a>
  <span>Press T or W to flip. The page keeps its place.</span>
</div>
<script>
(function () {
  var look = /look=today/.test(location.search) ? 'today' : 'workspace';
  document.querySelectorAll('.lookbar a').forEach(function (a) {
    if (a.dataset.look === look) a.classList.add('on');
    a.addEventListener('click', function () { try { sessionStorage.setItem('y', String(scrollY)); } catch (e) {} });
  });
  document.addEventListener('keydown', function (e) {
    var k = e.key.toLowerCase();
    if (k !== 't' && k !== 'w') return;
    try { sessionStorage.setItem('y', String(scrollY)); } catch (err) {}
    location.search = k === 't' ? '?look=today' : '?look=workspace';
  });
  addEventListener('load', function () {
    try { var y = sessionStorage.getItem('y'); if (y) setTimeout(function () { scrollTo(0, +y); }, 50); } catch (e) {}
  });
})();
</script>
"""


def main():
    global OUT, KEEP_ORDER, TITLE
    if sys.argv[1:] == ["rest"]:
        OUT, KEEP_ORDER, TITLE = HERE / "workspace-board-rest.html", REST_ORDER, "The workspace look, the rest of the app"
    elif sys.argv[1:] == ["more"]:
        OUT, KEEP_ORDER, TITLE = HERE / "workspace-board-more.html", MORE_ORDER, "The workspace look, every other screen"
    elif sys.argv[1:] == ["lock"]:
        OUT, KEEP_ORDER, TITLE = HERE / "workspace-board-lock.html", LOCK_ORDER, "The workspace look, lock screen and island"
    lines = SRC.read_text().split("\n")
    text = "\n".join(lines)
    blocks = list(re.finditer(r"<style>(.*?)</style>", text, re.S))
    pieces, pos = [], 0
    for i, m in enumerate(blocks):
        pieces.append(text[pos:m.start()])
        css = m.group(1)
        ws = workspace_css(css) + (WS_EXTRA if i == len(blocks) - 1 else "")
        pieces.append('<style data-look="today">' + css + '</style>\n<style data-look="workspace">' + ws + "</style>" + LOOK_APPLY)
        pos = m.end()
    pieces.append(text[pos:])
    text = "".join(pieces)
    text = text.replace("<style data-look=\"today\">", LOOK_PICK + "<style data-look=\"today\">", 1)

    # Keep the sections under review, in review order; the rest are hidden.
    found = {}
    for m in re.finditer(r'<section class="sect[ "][^>]*aria-label="([^"]+)"', text):
        depth = 0
        for tag in re.finditer(r"<section\b|</section>", text[m.start():]):
            depth += 1 if tag.group(0) == "<section" else -1
            if depth == 0:
                found[m.group(1)] = (m.start(), m.start() + tag.end())
                break
    missing = [name for name in KEEP_ORDER if name not in found]
    assert not missing, missing
    kept = "".join(text[slice(*found[name])].replace('class="sect', 'class="keep sect', 1) for name in KEEP_ORDER)
    for name in sorted(KEEP_ORDER, key=lambda n: -found[n][0]):
        a, b = found[name]
        text = text[:a] + text[b:]
    first = text.index("<section", text.index('<header class="head">'))
    text = text[:first] + kept + "\n" + text[first:]
    text = text.replace("<style data-look=\"today\">",
                        "<style>.sect:not(.keep) { display: none !important; }</style>\n<style data-look=\"today\">", 1)

    # Header and look switch.
    text = re.sub(r'<header class="head">.*?</header>',
                  '<header class="head"><div><p class="eyebrow">NereusSDR on iPhone and iPad</p>'
                  '<h1>' + TITLE + '</h1>'
                  '<p class="lede">The phone screens from the design board, recoloured with the desktop '
                  '2026.10.1 workspace palette. Spectrum, waterfall, band plan, slice colours and '
                  'frequency readouts are unchanged.</p></div></header>' + LOOK_BAR,
                  text, count=1, flags=re.S)

    # Canvas grounds and frames drawn by the script follow the look too.
    at = text.index("</header>")
    head, script = text[:at], text[at:]
    canvas_map = {"'#10101c'": "#050507", "'#304050'": "#232329", "'#0a0a18'": "#050507",
                  "'#203040'": "#232329", "'#0f0f1a'": "#08080a", "'#6888a0'": "#828288",
                  "'#8090a0'": "#a6a6ac", "'#0a0a14'": "#0a0a0c", "'#507080'": "#58585e",
                  "'#6080a0'": "#7e7e85", "'#80a0b0'": "#a6a6ac", "'#404858'": "#3d3d41",
                  "'#1a2a3a'": "#1a1a1e", "'#2a3948'": "#2c2c31", "'#3b5467'": "#3d3d41",
                  "'rgba(10, 10, 24, 0.863)'": "rgba(12, 12, 14, 0.88)",
                  "'rgba(15, 15, 26, 0.784)'": "rgba(8, 8, 10, 0.78)"}
    for old, new in canvas_map.items():
        script = script.replace(old, "(window.WS_LOOK === 'today' ? %s : '%s')" % (old, new))
    # SVG built as strings carries its colours in attributes.
    for old in ("#1a2a3a", "#2a3948", "#3b5467"):
        script = script.replace('stroke="%s"' % old,
                                "stroke=\"' + (window.WS_LOOK === 'today' ? '%s' : '%s') + '\"" % (old, canvas_map["'%s'" % old]))
    # A button the desktop also has takes the desktop's colour for it: the
    # desktop lights SPEAKERS green (JJ, 2026-10-20).
    script = script.replace("grid(2, 'is-blue', ['SPEAKERS', 'PHONES']",
                            "grid(2, (window.WS_LOOK === 'today' ? 'is-blue' : 'is-dsp'), ['SPEAKERS', 'PHONES']")
    OUT.write_text(head + script)
    print("wrote", OUT)


if __name__ == "__main__":
    main()
