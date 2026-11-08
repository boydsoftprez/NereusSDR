#!/usr/bin/env python3
"""Build the Roundtable overlay board.

Takes the established Roundtable board (../reporter-brainstorm/mockups/board.html)
for its styles, phone templates and live engine, swaps its sections for sections.html,
and adds overlay.css, overlay.js, a real capture of the desktop and the real web page's
stylesheet and markup. Output: board.html,
self-contained, works offline.
"""
import base64
import json
import pathlib
import re
import subprocess
import tempfile

HERE = pathlib.Path(__file__).resolve().parent
ROOT = HERE.parent.parent.parent
SRC = HERE.parent / "reporter-brainstorm" / "mockups" / "board.html"
CAPTURE = ROOT / "docs" / "manual" / "images" / "desktop-overview-workspace-original.png"
OUT = HERE / "board.html"
WEB = ROOT / "roundtable" / "server" / "static"


def capture_data_uri():
    with tempfile.TemporaryDirectory() as tmp:
        jpg = pathlib.Path(tmp) / "desktop.jpg"
        subprocess.run(
            ["sips", "-s", "format", "jpeg", "-s", "formatOptions", "90", str(CAPTURE), "--out", str(jpg)],
            check=True, capture_output=True,
        )
        return "data:image/jpeg;base64," + base64.b64encode(jpg.read_bytes()).decode()


def web_bundle():
    """The real web page's own stylesheet and markup, with its scripts left out."""
    html = (WEB / "index.html").read_text(encoding="utf-8")
    body = html[html.index("<body"):html.index("</body>")]
    body = body[body.index(">") + 1:]
    icon = "data:image/png;base64," + base64.b64encode((WEB / "nereus-64.png").read_bytes()).decode()
    return {
        "css": (WEB / "roundtable.css").read_text(encoding="utf-8"),
        "extra": (HERE / "web.css").read_text(encoding="utf-8"),
        "body": body.replace("/nereus-64.png", icon),
    }


def main():
    src = SRC.read_text(encoding="utf-8")
    head_end = src.index('<header class="head">')
    templates_start = src.index('<template id="tpl-phone">')
    head = src[:head_end]
    rest = src[templates_start:]

    # Roundtable's own frequency is driven by the scenario here, not the board's band.
    rest, n = re.subn(r"(var freqs = \[[^\]]*?)7236400, ", r"\1", rest, count=1)
    assert n == 1, "the band's 7.2364 channel moved"

    css = (HERE / "overlay.css").read_text(encoding="utf-8")
    js = (HERE / "overlay.js").read_text(encoding="utf-8")
    sections = (HERE / "sections.html").read_text(encoding="utf-8")
    head = head.replace("<title>Roundtable Board</title>", "<title>Roundtable Overlay</title>")
    head = head.replace("</style>\n<header", "</style>\n<header")

    close = rest.rindex("</body>") if "</body>" in rest else len(rest)
    page = (
        head
        + "<style>\n" + css + "\n</style>\n"
        + sections
        + rest[:close]
        + "<script>window.RC_DESKTOP_CAPTURE = " + repr(capture_data_uri()) + ";</script>\n"
        + "<script>window.RC_WEB = " + json.dumps(web_bundle()).replace("</", "<\\/") + ";</script>\n"
        + "<script>\n" + js + "\n</script>\n"
        + rest[close:]
    )
    OUT.write_text(page, encoding="utf-8")
    print(f"wrote {OUT.relative_to(ROOT)} ({len(page) // 1024} KB)")


if __name__ == "__main__":
    main()
