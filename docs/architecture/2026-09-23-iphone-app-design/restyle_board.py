#!/usr/bin/env python3
"""Give the iPhone design board the desktop's workspace look.

The board's own stylesheet stays the source ("today", the navy). This script
writes a second stylesheet beside it in which every chrome colour is replaced
by its workspace role, by the same table and treatments as the workspace-look
boards (docs/architecture/2026-10-20-phone-workspace-look/make_board.py, PR
603), plus the treatments for the Display screens drawn since. The workspace
look is the default; ?look=today shows the navy. Spectrum, waterfall, band
plan, slice colours, meter scales and frequency readouts are left alone.

Edit the "today" stylesheet, then run this again: it is safe to rerun.
"""
import colorsys
import pathlib
import re

HERE = pathlib.Path(__file__).resolve().parent
BOARD = HERE / "board.html"

# ---- From make_board.py (PR 603), unchanged: the colour table and treatments. ----
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
# The desktop's lit green, computed as the desktop computes it (the workspace
# look's design, section 5); make_board.py sampled a screenshot instead.
LIT_GREEN = ("background: linear-gradient(180deg, #306247, rgba(37, 77, 56, 0.55) 50%, #1d3d2c); "
             "border-color: #3e7e5c; color: #bbe1c6; box-shadow: inset 0 1px 0 #48946b")
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

# Canvas colours drawn by the board's script.
CANVAS_MAP = {"'#10101c'": "#050507", "'#304050'": "#232329", "'#0a0a18'": "#050507",
              "'#203040'": "#232329", "'#0f0f1a'": "#08080a", "'#6888a0'": "#828288",
              "'#8090a0'": "#a6a6ac", "'#0a0a14'": "#0a0a0c", "'#507080'": "#58585e",
              "'#6080a0'": "#7e7e85", "'#80a0b0'": "#a6a6ac", "'#404858'": "#3d3d41",
              "'#1a2a3a'": "#1a1a1e", "'#2a3948'": "#2c2c31", "'#3b5467'": "#3d3d41",
              "'rgba(10, 10, 24, 0.863)'": "rgba(12, 12, 14, 0.88)",
              "'rgba(15, 15, 26, 0.784)'": "rgba(8, 8, 10, 0.78)"}
# ---- End of the part from make_board.py. ----

# The Display screens (the pill, the column, Setup's "Settings for" strip).
WS_DISPLAY = """
  /* Display, as on the desktop: tabs are a pick-one choice, so the open one is glass. */
  .dp-tab { background: linear-gradient(180deg, #222227, #141417); border-color: #2c2c31; color: #a6a6ac; border-radius: 7px; }
  .dp-tab.is-on, .dp-tab[aria-selected="true"] { background: linear-gradient(180deg, #2f5f92, #1e3d5f); border-color: #3d78b0; color: #dbe9f8; }
  .dp-sheet, .dp-pill, .dp-fly { background: linear-gradient(180deg, #17171b, #0e0e10); border-color: #2c2c31; border-radius: 10px; }
  .dp-range::-webkit-slider-runnable-track { background: #050507; border: 1px solid #232329; }
  .dp-range::-webkit-slider-thumb { background: linear-gradient(180deg, #3a3a40, #222227); border: 1px solid #3d78b0; }
  .dp-range::-moz-range-track { background: #050507; }
  .dp-range::-moz-range-progress { background: #3d78b0; }
  .dp-out { background: #050507; border-color: #232329; }
  .dp-cdot, .dp-dot { background: #d8a55f; }
  .dp-col .nbtn, .dp-foot .nbtn { border-radius: 7px; }
  .dp-col .nbtn.is-open { background: linear-gradient(180deg, #2f5f92, #1e3d5f); border-color: #3d78b0; color: #dbe9f8; }
  .dp-chip.is-on { background: linear-gradient(180deg, #2f5f92, #1e3d5f); border-color: #3d78b0; color: #dbe9f8; }
  .dp-chip.is-on span { color: #c4d6ea; }
"""

PICK = "<script>window.WS_LOOK = /look=today/.test(location.search) ? 'today' : 'workspace';</script>\n"
APPLY = ("<script>document.querySelectorAll('style[data-look]').forEach(function (st) {"
         "st.disabled = st.dataset.look !== window.WS_LOOK; });</script>")

LOOK_BAR = """<!-- look bar -->
<style>
  .lookbar { position: sticky; top: 0; z-index: 50; display: flex; flex-wrap: wrap; gap: 8px; align-items: center;
    padding: 10px 16px; margin: -28px -16px 20px; background: rgba(8, 8, 10, 0.92);
    border-bottom: 1px solid #2c2c31; font: 13px system-ui, sans-serif; color: #a6a6ac; }
  .lookbar b { color: #dcdce1; margin-right: 8px; }
  .lookbar a { padding: 6px 14px; border-radius: 7px; border: 1px solid #2c2c31; color: #dcdce1;
    text-decoration: none; background: linear-gradient(180deg, #222227, #141417); }
  .lookbar a.on { background: linear-gradient(180deg, #2f5f92, #1e3d5f); border-color: #3d78b0; color: #dbe9f8; }
  .lookbar span { margin-left: auto; }
</style>
<div class="lookbar"><b>Look</b>
  <a href="?look=workspace" data-look="workspace">Workspace</a>
  <a href="?look=today" data-look="today">Before (navy)</a>
  <span>Press W or T to flip. The page keeps its place.</span>
</div>
<script>
(function () {
  var look = window.WS_LOOK;
  document.querySelectorAll('.lookbar a').forEach(function (a) {
    if (a.dataset.look === look) a.classList.add('on');
    a.addEventListener('click', function () { try { sessionStorage.setItem('y', String(scrollY)); } catch (e) {} });
  });
  document.addEventListener('keydown', function (e) {
    if (e.target && /INPUT|SELECT|TEXTAREA/.test(e.target.tagName)) return;
    var k = e.key.toLowerCase();
    if (k !== 't' && k !== 'w') return;
    try { sessionStorage.setItem('y', String(scrollY)); } catch (err) {}
    location.search = k === 't' ? '?look=today' : '?look=workspace';
  });
  addEventListener('load', function () {
    try { var y = sessionStorage.getItem('y'); if (y) { sessionStorage.removeItem('y'); setTimeout(function () { scrollTo(0, +y); }, 50); } } catch (e) {}
  });
})();
</script>
<!-- /look bar -->"""


def main():
    text = BOARD.read_text()
    m = re.search(r'<style data-look="today">(.*?)</style>', text, re.S)
    if m:
        css = m.group(1)
        start = text.index(PICK) if PICK in text else m.start()
        end = text.index(APPLY, m.end()) + len(APPLY)
    else:
        blocks = list(re.finditer(r"<style>(.*?)</style>", text, re.S))
        m = blocks[1]  # blocks[0] is the page shell's one-liner
        css, start, end = m.group(1), m.start(), m.end()
    ws = workspace_css(css) + WS_EXTRA + WS_DISPLAY
    text = (text[:start] + PICK + '<style data-look="today">' + css + '</style>\n<style data-look="workspace">'
            + ws + "</style>" + APPLY + text[end:])

    if "<!-- look bar -->" not in text:
        at = text.index("</header>") + len("</header>")
        text = text[:at] + "\n" + LOOK_BAR + text[at:]

    # Canvas grounds and frames drawn by the script follow the look too.
    at = text.index("<!-- /look bar -->")
    head, script = text[:at], text[at:]
    for old, new in CANVAS_MAP.items():
        script = re.sub(r"(?<!\? )" + re.escape(old), "(window.WS_LOOK === 'today' ? %s : '%s')" % (old, new), script)
    for old in ("#1a2a3a", "#2a3948", "#3b5467"):
        script = script.replace('stroke="%s"' % old,
                                "stroke=\"' + (window.WS_LOOK === 'today' ? '%s' : '%s') + '\""
                                % (old, CANVAS_MAP["'%s'" % old]))
    script = script.replace("grid(2, 'is-blue', ['SPEAKERS', 'PHONES']",
                            "grid(2, (window.WS_LOOK === 'today' ? 'is-blue' : 'is-dsp'), ['SPEAKERS', 'PHONES']")
    BOARD.write_text(head + script)
    print("restyled", BOARD)


if __name__ == "__main__":
    main()
