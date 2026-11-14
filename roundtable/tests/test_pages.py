# no-port-check: NereusSDR-original.
"""The web pages (design 7, wire spec 2.1 and 10.1): the page routes serve
their files with the CSP header, a room link serves the public page, the
static files carry no inline script or style, and, where headless Chrome
is installed, the page checks in tests/pages run against the scripted
server (200 rooms over 8 bands): every view screenshotted with the size
and contrast check, then each functional probe.

Without Chrome the browser checks skip with the reason printed. The
screenshots go to $ROUNDTABLE_SHOTS when set, else to a temporary folder.
"""

from __future__ import annotations

import asyncio
import os
import re
import sys
from pathlib import Path
from typing import Dict, List, Tuple

import pytest

from helpers import live_service
from nereus_roundtable.http import CSP

HERE = Path(__file__).resolve().parent
STATIC = HERE.parent / "server" / "static"
sys.path.insert(0, str(HERE / "pages"))

import probes  # noqa: E402
from cdp import chrome_path  # noqa: E402

PAGES = {"/": "index.html", "/archive": "archive.html", "/review": "review.html", "/key": "key.html"}


async def _get(port: int, target: str) -> Tuple[int, Dict[str, str], bytes]:
    reader, writer = await asyncio.open_connection("127.0.0.1", port)
    writer.write(f"GET {target} HTTP/1.1\r\nHost: roundtable.conformance.invalid\r\n\r\n".encode("ascii"))
    await writer.drain()
    data = await asyncio.wait_for(reader.read(), 5)
    writer.close()
    head, _, body = data.partition(b"\r\n\r\n")
    lines = head.decode("latin-1").split("\r\n")
    headers = {}
    for line in lines[1:]:
        name, _, value = line.partition(":")
        headers[name.strip().lower()] = value.strip()
    return int(lines[0].split()[1]), headers, body


def test_page_routes_and_room_links_serve_their_files_with_the_csp():
    async def go():
        async with live_service(http_app=True, static_dir=str(STATIC)) as live:
            port = live.http_port
            for path, name in PAGES.items():
                status, headers, body = await _get(port, path)
                assert status == 200, path
                assert body == (STATIC / name).read_bytes(), path
                assert headers["content-type"] == "text/html; charset=utf-8"
                assert headers["content-security-policy"] == CSP
            status, _, body = await _get(port, "/r/usb/14212000")
            assert status == 200 and body == (STATIC / "index.html").read_bytes()
            for bad in ("/r/usb/abc", "/r/xyz/14212000", "/r/usb", "/r/usb/14212000/x", "/archive/x"):
                status, _, _ = await _get(port, bad)
                assert status == 404, bad
            for name in ("roundtable.css", "roundtable.js", "archive.js", "review.js", "key.js", "nereus-64.png"):
                status, headers, _ = await _get(port, "/" + name)
                assert status == 200 and headers["content-security-policy"] == CSP, name

    asyncio.run(go())


def test_no_inline_script_or_style_and_every_file_marked():
    for path in sorted(STATIC.iterdir()):
        if path.suffix == ".png":
            continue
        text = path.read_text(encoding="utf-8")
        assert "no-port-check: NereusSDR-original." in text, path.name
        if path.suffix != ".html":
            continue
        for m in re.finditer(r"<script\b([^>]*)>(.*?)</script>", text, re.S | re.I):
            assert "src=" in m.group(1) and not m.group(2).strip(), path.name
        assert not re.search(r"<style\b", text, re.I), path.name
        assert not re.search(r"\sstyle\s*=", text, re.I), path.name
        assert not re.search(r"\son[a-z]+\s*=", text, re.I), path.name
        assert not re.search(r"javascript:", text, re.I), path.name


def test_scripts_build_text_as_text():
    """Text from the server goes in as text: no innerHTML, outerHTML,
    insertAdjacentHTML or document.write anywhere."""
    for path in sorted(STATIC.glob("*.js")):
        text = path.read_text(encoding="utf-8")
        for bad in ("innerHTML", "outerHTML", "insertAdjacentHTML", "document.write", "eval(", "new Function"):
            assert bad not in text, (path.name, bad)


def test_frequency_client_helpers_without_browser():
    """Exact contexts, paths, authority and exposure need no UI fixture."""
    import shutil
    import subprocess
    node = shutil.which("node")
    if node is None:
        pytest.skip("Node.js is not installed for the lightweight client checks")
    result = subprocess.run([node, str(HERE / "pages" / "frequency-unit.js")], capture_output=True, text=True)
    assert result.returncode == 0, result.stdout + result.stderr


# ----------------------------------------------------------- headless Chrome


@pytest.fixture(scope="module")
def server():
    if chrome_path() is None:
        print("SKIP: headless Chrome is not installed; the page checks did not run")
        pytest.skip("headless Chrome is not installed")
    s = probes.Server()
    yield s
    s.close()


@pytest.fixture(scope="module")
def chrome(server, tmp_path_factory):
    c = probes.chrome_for(server, tmp_path_factory.mktemp("chrome"))
    yield c
    c.close()


def test_screenshots_size_and_contrast(chrome, server, tmp_path_factory):
    """Runs first: the review probe changes what the review page shows."""
    out = Path(os.environ["ROUNDTABLE_SHOTS"]) if os.environ.get("ROUNDTABLE_SHOTS") else tmp_path_factory.mktemp("shots")
    names: List[str] = probes.shots(chrome, server, out)
    assert len(names) == 19, names


@pytest.mark.parametrize("probe", probes.PROBES, ids=[p.__name__[len("probe_"):] for p in probes.PROBES])
def test_page_probe(chrome, server, probe):
    probe(chrome, server)
