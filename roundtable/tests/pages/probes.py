# no-port-check: NereusSDR-original.
"""The page checks: each probe drives one headless Chrome (cdp.Chrome)
against the scripted server (serve.py) and raises AssertionError with
what it saw when a page does not do what the brief says.

Used by test_pages.py and by run.py, which also takes the screenshots.
"""

from __future__ import annotations

import json
import re
import subprocess
import sys
import time
import urllib.request
from collections import Counter
from pathlib import Path
from typing import Any, Dict, List, Optional, Sequence

from cdp import Chrome

HERE = Path(__file__).resolve().parent
CONTRAST_JS = (HERE / "contrast.js").read_text("utf-8")
CORPUS = {c["name"]: c for c in json.loads((HERE.parent / "data" / "text-corpus.json").read_text("utf-8"))["cases"]}
PUBLIC_HOST = "roundtable.nereussdr.com"
AMBER = "rgb(216, 165, 95)"
HF = ["160m", "80m", "60m", "40m", "30m", "20m", "17m", "15m", "12m", "10m", "6m"]


# ----------------------------------------------------------- the server


class Server:
    """serve.py in a child process."""

    def __init__(self, timeout: float = 240.0) -> None:
        self.proc = subprocess.Popen(
            [sys.executable, str(HERE / "serve.py")],
            stdin=subprocess.PIPE,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
        )
        end = time.monotonic() + timeout
        self.base = ""
        assert self.proc.stdout is not None
        while time.monotonic() < end:
            line = self.proc.stdout.readline()
            if not line:
                err = self.proc.stderr.read() if self.proc.stderr else ""
                raise RuntimeError(f"serve.py stopped: {err[-2000:]}")
            if line.startswith("READY "):
                self.base = line.split()[1]
                break
        if not self.base:
            self.close()
            raise RuntimeError("serve.py did not get ready in time")
        self.port = int(self.base.rsplit(":", 1)[1])
        self.facts = self.get("/__test/facts")

    def get(self, path: str) -> Any:
        with urllib.request.urlopen(self.base + path, timeout=30) as r:
            return json.loads(r.read().decode("utf-8"))

    def control(self, path: str) -> Any:
        return self.get("/__test/" + path)

    def close(self) -> None:
        if self.proc.poll() is None:
            try:
                assert self.proc.stdin is not None
                self.proc.stdin.close()
                self.proc.wait(timeout=10)
            except (OSError, subprocess.TimeoutExpired):
                self.proc.terminate()
                try:
                    self.proc.wait(timeout=10)
                except subprocess.TimeoutExpired:
                    self.proc.kill()
                    self.proc.wait()


def chrome_for(server: Server, scratch: Path) -> Chrome:
    """One Chrome for the whole run. The public host name resolves to the
    scripted server so a room link can be opened as it is copied."""
    return Chrome(
        scratch,
        args=(
            f"--host-resolver-rules=MAP {PUBLIC_HOST} 127.0.0.1:{server.port}",
            "--disable-features=HttpsUpgrades,HttpsFirstBalancedModeAutoEnable",
        ),
    )


# ----------------------------------------------------------- helpers


def js(value: Any) -> str:
    return json.dumps(value)


def contrast_fails(c: Chrome, page: str) -> List[Dict[str, Any]]:
    """The size and contrast failures of the page as it stands."""
    got = c.eval(CONTRAST_JS)
    assert got["checked"] > 0, f"{page}: no text was checked"
    return [dict(f, page=page) for f in got["fails"]]


def no_errors(c: Chrome, page: str, refusals: Sequence[int] = ()) -> None:
    """No exception and no console error, except the network log of the
    refusals a probe asked for (Chrome logs every 4xx answer). A 401 is
    always expected: every page asks GET /v1/me, which answers 401 to a
    visitor who is not signed in."""
    expected = tuple(f"status of {n} " for n in (401, *refusals))
    errs = [e for e in c.exceptions() if "favicon" not in e and not (e.startswith("network:") and any(x in e for x in expected))]
    assert not errs, f"{page}: page errors: {errs[:8]}"
    c.events.clear()


def open_public(c: Chrome, base: str, path: str = "/") -> None:
    c.goto(base + path)
    c.wait("(function(){var n=document.querySelector('.band.all .c');return n&&n.textContent==='200'&&window.RT&&RT.linkState()==='live';})()", 40)


def set_query(c: Chrome, text: str) -> None:
    c.eval(f"(function(){{var q=document.getElementById('q');q.value={js(text)};q.dispatchEvent(new Event('input'));}})()")


def rooms_api(server: Server) -> List[Dict[str, Any]]:
    return server.get("/v1/api/rooms")["rooms"]


def quiet(r: Dict[str, Any]) -> bool:
    return r["state"] == "quiet" or r["folded"]


# ----------------------------------------------------------- public page


def probe_bands(c: Chrome, server: Server) -> None:
    """Band buttons with their counts and the talking/unknown bar."""
    open_public(c, server.base)
    rooms = rooms_api(server)
    by_band = Counter(r["band"] for r in rooms)
    got = c.eval(
        """[...document.querySelectorAll('#band-buttons .band')].map(function(b){
          var i=b.querySelector('.act i'), u=b.querySelector('.act u');
          return {band:b.dataset.band, n:b.querySelector('.c').textContent, disabled:b.disabled,
                  i:i?parseFloat(i.style.width):0, u:u?parseFloat(u.style.width):0,
                  label:b.getAttribute('aria-label')};})"""
    )
    assert got[0]["band"] == "" and got[0]["n"] == "200", got[0]
    bands = [b["band"] for b in got[1:]]
    assert bands == HF, bands
    assert len(by_band) == 8, by_band
    for b in got[1:]:
        n = by_band.get(b["band"], 0)
        assert b["n"] == str(n), (b, n)
        assert b["disabled"] == (n == 0), b
        if n:
            band_rooms = [r for r in rooms if r["band"] == b["band"]]
            talking = sum(1 for r in band_rooms if r["state"] == "talking" and not r["folded"])
            unknown = sum(1 for r in band_rooms if r["state"] in ("unknown", "doubled") and not r["folded"])
            # The live list may move by a turn between the two reads.
            assert abs(b["i"] - 100 * talking / n) <= 100 / n + 0.01, (b, talking)
            assert abs(b["u"] - 100 * unknown / n) <= 100 / n + 0.01, (b, unknown)
            assert b["i"] > 0, b
    no_errors(c, "public")


def probe_band_map(c: Chrome, server: Server) -> None:
    """Picking 20m shows the band map, stems by member count; quiet rooms
    fold with a count and Show unfolds them."""
    open_public(c, server.base)
    c.eval("document.querySelector('.band[data-band=\"20m\"]').click()")
    c.wait("!document.getElementById('bmap').classList.contains('hidden') && document.querySelectorAll('#bmap .mk').length > 0")
    rooms = [r for r in rooms_api(server) if r["band"] == "20m"]
    marks = c.eval(
        """[...document.querySelectorAll('#bmap .mk')].map(function(m){return {id:m.dataset.id,
           members:+m.dataset.members, h:parseFloat(m.querySelector('.stem').style.height),
           left:parseFloat(m.style.left)};})"""
    )
    loud = {r["id"] for r in rooms if not quiet(r)}
    assert abs(len(marks) - len(loud)) <= 1, (len(marks), len(loud))
    heights = set()
    for m in marks:
        want = min(52.0, 6 + m["members"] * 2.2)
        assert abs(m["h"] - want) < 0.05, m
        heights.add(round(m["h"], 1))
        assert 0 <= m["left"] <= 100, m
    assert len(heights) >= 3, heights
    axis = c.eval("[...document.querySelectorAll('#bmap .axis span')].map(function(s){return s.textContent})")
    assert axis[0] == "14.000" and axis[-1] == "14.350", axis
    # Quiet rooms fold with a count.
    n_quiet = sum(1 for r in rooms if quiet(r))
    assert n_quiet > 0
    fold = c.eval("(function(){var b=[...document.querySelectorAll('#rows .linkbtn')].find(function(x){return x.textContent==='Show'});return b?b.parentElement.textContent:null})()")
    assert fold is not None and re.search(rf"\b{n_quiet} quiet folded", fold), (fold, n_quiet)
    shown = c.eval("document.querySelectorAll('#rows .row').length")
    assert shown == len(rooms) - n_quiet, (shown, len(rooms), n_quiet)
    assert c.eval("document.querySelectorAll('#rows .row.q').length") == 0
    c.eval("[...document.querySelectorAll('#rows .linkbtn')].find(function(x){return x.textContent==='Show'}).click()")
    c.wait(f"document.querySelectorAll('#rows .row').length === {len(rooms)}")
    assert c.eval("document.querySelectorAll('#rows .row.q').length") == n_quiet
    c.eval("[...document.querySelectorAll('#rows .linkbtn')].find(function(x){return x.textContent==='Fold them'}).click()")
    c.wait(f"document.querySelectorAll('#rows .row').length === {len(rooms) - n_quiet}")
    no_errors(c, "public band map")


def probe_row(c: Chrome, server: Server) -> None:
    """A row shows frequency, mode, state, talker, time, members, the
    reading strip nearest first, the last message and its age."""
    open_public(c, server.base)
    c.eval("document.querySelector('.band[data-band=\"20m\"]').click()")
    sel = '#rows .row[data-id="usb-14212000"]'
    c.wait(f"document.querySelector({js(sel)}) !== null")
    c.eval(f"document.querySelector({js(sel)}).scrollIntoView({{block:'center'}})")
    c.wait(f"!!(document.querySelector({js(sel + ' .strip:not(.none)')}) && document.querySelector({js(sel + ' .lm b')}))", 30)
    row = c.eval(
        f"""(function(){{var r=document.querySelector({js(sel)});function t(s){{var n=r.querySelector(s);return n?n.textContent:null}}
        return {{fq:t('.fq'), md:t('.md'), who:t('.who'), gl:r.querySelector('.who .gl').className, tm:t('.tm'), hb:t('.hb'),
        bars:r.querySelectorAll('.strip i').length, strip:r.querySelector('.strip').getAttribute('aria-label'),
        heights:[...r.querySelectorAll('.strip i')].map(function(i){{return parseFloat(i.style.height)}}),
        lm:t('.lm'), ago:t('.ago')}};}})()"""
    )
    summary = next(r for r in rooms_api(server) if r["id"] == "usb-14212000")
    detail = server.get("/v1/api/room/usb/14212000")["room"]
    assert row["fq"].replace(" ", "").startswith("14.212"), row
    assert row["md"] == "USB", row
    assert "K4ABC" in row["who"] and "talking" in row["gl"], row
    assert re.match(r"^\d+:\d\d$", row["tm"] or ""), row
    assert row["hb"] == str(summary["members"]) and summary["members"] == 10, (row, summary)
    readers = [m for m in detail["members"] if m["reading"] is not None and m["reading"]["turn"] is not None and not m["joining"]]
    assert row["bars"] >= 5 and row["strip"] == f"{row['bars']} readings, nearest first", row
    assert "VE3XYZ" in row["lm"] and "Thanks for the QSO" in row["lm"], row
    assert re.match(r"^\d+ ?[smh]", row["ago"] or ""), row
    assert len(readers) >= row["bars"] - 1, (readers, row)
    no_errors(c, "public row")


def probe_search(c: Chrome, server: Server) -> None:
    """Search finds a callsign talking, reading or posting; a typed
    frequency jumps to its room."""
    open_public(c, server.base)
    facts = server.facts
    set_query(c, "K4ABC")
    c.wait("document.querySelector('#rows .row[data-id=\"usb-14212000\"]') !== null")
    assert c.eval("document.querySelectorAll('#rows .row').length") >= 1
    assert "K4ABC" in c.eval("document.querySelector('#rows .row[data-id=\"usb-14212000\"] .who').textContent")
    set_query(c, "N4RDR")
    c.wait("[...document.querySelectorAll('#rows .row .lm .via')].some(function(v){return /heard by N4RDR/.test(v.textContent)})", 30)
    set_query(c, "VE3PST")
    poster_room = facts["poster"]["room"]
    c.wait(f"(function(){{var r=document.querySelector('#rows .row[data-id=\"{poster_room}\"] .lm b');return r&&r.textContent.indexOf('VE3PST')===0}})()", 30)
    for typed in ("14.212", "14212"):
        set_query(c, "")
        c.eval("RT.select(null)")
        set_query(c, typed)
        c.wait("RT.state.sel === 'usb-14212000'")
    set_query(c, "7.185")
    c.wait("RT.state.sel === 'lsb-7185000'")
    set_query(c, "")
    no_errors(c, "public search")


def probe_sort(c: Chrome, server: Server) -> None:
    """Sort by Frequency, Most stations or Newest activity."""
    open_public(c, server.base)
    c.eval("document.querySelector('.band[data-band=\"20m\"]').click(); document.getElementById('quiet-chip').click()")
    c.wait("document.querySelectorAll('#rows .row').length === 52")
    ids = "[...document.querySelectorAll('#rows .row')].map(function(r){return r.dataset.id})"

    def order(value: str) -> List[str]:
        c.eval(f"(function(){{var s=document.getElementById('sort');s.value='{value}';s.dispatchEvent(new Event('change'))}})()")
        c.pump(0.2)
        return c.eval(ids)

    by_id = {r["id"]: r for r in rooms_api(server)}
    f = order("f")
    assert [by_id[i]["hz"] for i in f] == sorted(by_id[i]["hz"] for i in f), f
    n = order("n")
    members = [by_id[i]["members"] for i in n]
    assert members == sorted(members, reverse=True) and members[0] > members[-1], members
    # Newest activity sorts on the page's own summaries: `last` moves with
    # every reading, so the API read a moment later would not match.
    got = c.eval(
        "(function(){var s=document.getElementById('sort');s.value='t';s.dispatchEvent(new Event('change'));"
        "var by={};RT.summaries().forEach(function(r){by[r.id]=r.last});"
        "return [...document.querySelectorAll('#rows .row')].map(function(r){return [r.dataset.id, by[r.dataset.id]]})})()"
    )
    t = [i for i, _ in got]
    lasts = [last for _, last in got]
    assert all(a >= b for a, b in zip(lasts, lasts[1:])) and t != f, lasts[:12]
    order("f")
    c.eval("document.getElementById('quiet-chip').click()")
    no_errors(c, "public sort")


def probe_keyboard(c: Chrome, server: Server) -> None:
    """/ searches; arrows move rooms and each shows; Enter enters the room
    and the arrows walk its turns; Escape steps back room, search, band."""
    open_public(c, server.base)
    active = "(function(){var a=document.activeElement;return a?(a.id||a.className||a.tagName):null})()"
    c.eval("document.body.focus()")
    c.key("/", "Slash", "/")
    assert c.eval(active) == "q", c.eval(active)
    c.key("Escape")
    assert c.eval(active) == "rows", c.eval(active)
    c.key("ArrowDown")
    first = c.wait("RT.state.sel")
    c.wait(f"document.querySelector('#room .rhead') && document.querySelector('#rows .row.sel').dataset.id === {js(first)}")
    c.key("ArrowDown")
    second = c.wait(f"RT.state.sel !== {js(first)} && RT.state.sel")
    head = c.eval("document.querySelector('#room .rhead .f').textContent")
    want = second.split("-")[1]
    assert head.replace(".", "").replace(" ", "").lstrip("0").startswith(want.rstrip("0")[:4]), (head, second)
    c.key("ArrowUp")
    c.wait(f"RT.state.sel === {js(first)}")
    # Into a room with earlier turns.
    c.eval("RT.select('usb-14212000', true); document.getElementById('rows').focus()")
    c.wait("document.querySelectorAll('#room .turn').length >= 2", 20)
    c.key("Enter")
    t1 = c.wait("document.activeElement.classList.contains('turn') && document.activeElement.dataset.turn")
    c.key("ArrowDown")
    t2 = c.wait(f"document.activeElement.classList.contains('turn') && document.activeElement.dataset.turn !== {js(t1)} && document.activeElement.dataset.turn")
    assert t2 != t1
    c.key("ArrowUp")
    c.wait(f"document.activeElement.dataset.turn === {js(t1)}")
    # Escape: room, then search, then band.
    c.key("Escape")
    assert c.eval(active) == "rows", c.eval(active)
    c.eval("document.querySelector('.band[data-band=\"20m\"]').click(); document.getElementById('rows').focus()")
    c.key("/", "Slash", "/")
    c.type_text("K4")
    c.wait("RT.state.q === 'K4'")
    c.key("Escape")
    c.wait("RT.state.q === '' && document.getElementById('q').value === ''")
    assert c.eval("RT.state.band") == "20m"
    c.key("Escape")
    c.wait("RT.state.band === null")
    no_errors(c, "public keyboard")


def probe_freshness(c: Chrome, server: Server) -> None:
    """A reading older than 20 s turns grey with an amber age; a room with
    nothing newer than 60 s turns grey."""
    open_public(c, server.base)
    c.eval("RT.select('usb-14212000', true)")
    c.wait("[...document.querySelectorAll('#room .rd')].length >= 8", 20)
    stale = c.wait(
        """(function(){var r=[...document.querySelectorAll('#room .rd.stale')].find(function(x){return /W5STL/.test(x.textContent)});
        if(!r)return null;var a=r.querySelector('.age');return {age:a.textContent, color:getComputedStyle(a).color,
        call:getComputedStyle(r.querySelector('.c')||r).color}})()""",
        20,
    )
    assert stale["color"] == AMBER, stale
    fresh = c.eval("document.querySelectorAll('#room .rd:not(.stale)').length")
    assert fresh >= 4, fresh
    # The readings are nearest first.
    kms = c.eval("[...document.querySelectorAll('#room .rd')].map(function(r){return r.dataset.km===undefined?null:+r.dataset.km})")
    detail = server.get("/v1/api/room/usb/14212000")["room"]
    by_ref = {m["ref"]: m for m in detail["members"]}
    refs = c.eval("[...document.querySelectorAll('#room .rd')].map(function(r){return r.dataset.ref})")
    dist = [by_ref[r]["km"] for r in refs if r in by_ref and by_ref[r]["km"] is not None]
    assert dist == sorted(dist), (dist, kms)
    room = server.facts["staleRoom"]
    c.eval(f"RT.select({js(room)}, true)")
    c.wait(f"document.querySelector('#rows .row[data-id=\"{room}\"].stale') !== null", 20)
    talk = c.eval("document.querySelector('#rows .row[data-id=\"usb-14212000\"]').classList.contains('stale')")
    assert talk is False
    no_errors(c, "public freshness")


def probe_connection(c: Chrome, server: Server) -> None:
    """The connection line reads Connecting, Live, Reconnecting and
    Offline since as the socket is refused, allowed and dropped."""
    server.control("live?do=refuse")
    try:
        c.goto(server.base + "/r/usb/14212000", settle=1.0)
        c.wait("document.getElementById('live-state').textContent === 'CONNECTING'")
        assert c.eval("RT.linkState()") == "connecting"
        server.control("live?do=allow")
        c.eval("RT.reconnectNow()")
        c.wait("RT.linkState() === 'live' && document.getElementById('conn') && /^Live/.test(document.getElementById('conn').textContent)", 30)
        server.control("live?do=drop-refuse")
        line = c.wait("/^Reconnecting/.test(document.getElementById('conn').textContent) && document.getElementById('conn').textContent", 20)
        assert re.match(r"^Reconnecting · nothing new for \d+ s$", line), line
        assert c.eval("document.getElementById('live-state').textContent") == "RECONNECTING"
        line = c.wait("/^Offline/.test(document.getElementById('conn').textContent) && document.getElementById('conn').textContent", 40)
        assert re.match(r"^Offline since \d\d:\d\d UTC$", line), line
        assert re.match(r"^since \d\d:\d\d UTC$", c.eval("document.getElementById('live-k').textContent"))
    finally:
        server.control("live?do=allow")
    c.eval("RT.reconnectNow()")
    c.wait("RT.linkState() === 'live'", 30)
    c.events.clear()


def probe_copy_link(c: Chrome, server: Server) -> None:
    """Copy link gives the room's public link, and opening it shows the
    room."""
    c.goto(f"http://{PUBLIC_HOST}/r/usb/14212000", settle=0.5)
    c.wait("window.RT && RT.state.sel === 'usb-14212000' && document.getElementById('copy-link') !== null", 30)
    c.eval("document.getElementById('copy-link').click()")
    copied = c.wait("window.__rtCopied")
    assert copied == "https://roundtable.nereussdr.com/r/usb/14212000", copied
    c.goto(server.base + "/r/usb/14212000")
    c.wait("RT.state.sel === 'usb-14212000' && /K4ABC/.test(document.querySelector('#room .rhead').textContent)", 30)
    assert c.eval("location.pathname") == "/r/usb/14212000"
    no_errors(c, "room link")


def probe_inert(c: Chrome, server: Server) -> None:
    """The text corpus renders inert: no script runs, links show their full
    address, only http and https become links."""
    facts = server.facts
    open_public(c, server.base)
    c.eval(f"RT.select({js(facts['corpusRoom'])}, true)")
    want = len(facts["corpus"]) + len(facts["injections"])
    c.wait(f"document.querySelectorAll('#room .msg[data-msg]').length >= {want}", 30)
    c.pump(1.0)
    got = c.eval(
        """[...document.querySelectorAll('#room .msg[data-msg]')].map(function(m){var x=m.querySelector('.x');
           return {id:m.dataset.msg, links:[...x.querySelectorAll('a')].map(function(a){return [a.getAttribute('href'), a.textContent, a.protocol]}),
           tags:[...x.querySelectorAll('*')].map(function(e){return e.tagName}), text:x.textContent};})"""
    )
    seen = 0
    for m in got:
        name = facts["corpus"].get(m["id"])
        if name is None:
            continue
        seen += 1
        case = CORPUS[name]
        hrefs = [h for h, _, _ in m["links"]]
        assert hrefs == case.get("links", []), (name, m["links"], case.get("links"))
        for href, text, proto in m["links"]:
            assert text == href and proto in ("http:", "https:"), (name, href, text)
        assert set(m["tags"]) <= {"A", "B", "I", "BR"}, (name, m["tags"])
    assert seen == len(facts["corpus"]), (seen, len(facts["corpus"]))
    inj = [m for m in got if int(m["id"]) in facts["injections"]]
    assert len(inj) == len(facts["injections"]), inj
    for m in inj:
        assert m["links"] == [] and set(m["tags"]) <= {"A", "B", "I", "BR"}, m
    assert "window.__rtInjected" in "".join(m["text"] for m in inj)
    assert c.eval("typeof window.__rtInjected") == "undefined"
    assert c.eval("document.querySelectorAll('#room img, #room svg, #room script, #room iframe, #room object').length") == 0
    assert c.dialogs() == 0
    no_errors(c, "inert text")


# ----------------------------------------------------------- signed pages


def sign_in(c: Chrome, server: Server, who: str = "KG4VCF") -> None:
    path = server.control(f"signin?who={who}")["path"]
    c.goto(server.base + path)
    c.wait("location.pathname === '/archive' && document.querySelector('[data-session]') !== null", 30)


def probe_archive(c: Chrome, server: Server) -> None:
    """Sessions, per-station rows with WATCHED and timelines, keyed-and-lost
    hatching, the first 12 then Show all 34, the hourly histogram, an
    empty session and a message removed after a year."""
    sign_in(c, server)
    s = server.facts["sessions"]
    c.eval("[...document.querySelectorAll('#range button')].find(function(b){return b.textContent==='All'}).click()")
    c.wait("document.querySelectorAll('[data-session]').length === 7")
    c.goto(f"{server.base}/archive?s={s['busy']}")
    c.wait("document.querySelectorAll('.lane[data-lane]').length > 0", 20)
    lanes = c.eval("document.querySelectorAll('.lane[data-lane]').length")
    assert lanes == 12, lanes
    assert c.eval("document.getElementById('show-all').textContent") == f"Show all {s['busyReaders']}"
    assert s["busyReaders"] == 34
    hdr = c.eval("document.querySelector('.lane.hdr').textContent")
    assert "WATCHED" in hdr and "STATION" in hdr, hdr
    assert c.eval("document.querySelectorAll('.lane.you .trk span, .lane.you .trk i, .lane.you .trk div').length") > 0
    lost = c.eval("document.querySelectorAll('.lane[data-lane] .trk .lost').length")
    assert lost > 0, lost
    hatch = c.eval("getComputedStyle(document.querySelector('.lane[data-lane] .trk .lost')).backgroundImage")
    assert "repeating-linear-gradient" in hatch and "216, 165, 95" in hatch, hatch
    watched = c.eval("[...document.querySelectorAll('.lane[data-lane]')].map(function(l){return l.textContent})")
    assert all(re.search(r"\d+ min|\d+ s", w) for w in watched), watched[:3]
    c.eval("document.getElementById('show-all').click()")
    c.wait("document.querySelectorAll('.lane[data-lane]').length === 34")
    c.eval("document.querySelector('.lane[data-lane]').click()")
    c.wait("document.getElementById('spk') !== null")
    hist = c.eval("[...document.querySelectorAll('#hist [data-hour]')].map(function(b){return +b.dataset.count})")
    assert len(hist) == 24 and sum(hist) > 0, hist
    c.goto(f"{server.base}/archive?s={s['empty']}")
    c.wait("/Nobody reported you/.test(document.body.textContent) && document.querySelector('.det') !== null", 20)
    c.goto(f"{server.base}/archive?s={s['old']}")
    c.wait("/Message removed after a year/.test(document.body.textContent)", 20)
    no_errors(c, "archive")


def _entries(c: Chrome) -> List[Dict[str, Any]]:
    return c.eval("fetch('/v1/review/reports').then(function(r){return r.json()}).then(function(b){return b.reports})")


def _pick(c: Chrome, entry_id: int) -> None:
    c.eval(f"document.querySelector('.it[data-entry=\"{entry_id}\"]').click()")
    c.wait(f"document.querySelector('.it.sel[data-entry=\"{entry_id}\"]') !== null")


def _click(c: Chrome, selector: str) -> None:
    c.wait(f"document.querySelector({js(selector)}) !== null && !document.querySelector({js(selector)}).disabled")
    c.eval(f"document.querySelector({js(selector)}).click()")


def probe_review(c: Chrome, server: Server) -> None:
    """Oldest first with the older-than-a-day count, every action, the
    conflict message, the Undo countdown, the lifts, helpers and the log."""
    sign_in(c, server)
    c.goto(server.base + "/review")
    c.wait("document.querySelectorAll('.it').length >= 4", 20)
    entries = _entries(c)
    order = c.eval("[...document.querySelectorAll('.it')].map(function(i){return +i.dataset.entry})")
    assert order == [e["id"] for e in sorted(entries, key=lambda e: e["first"])], (order, entries)
    assert c.eval("document.querySelectorAll('.it .t.old').length") == 1
    assert c.eval("document.getElementById('older').textContent") == "1 report is older than a day"
    by = {(e["target"], (e.get("message") or {}).get("callsign") or (e.get("station") or {}).get("callsign")): e for e in entries}
    spam = by[("message", "N0SPM")]
    old = by[("message", "W0OLD")]
    station = by[("station", "W5STL")]
    turn = next(e for e in entries if e["target"] == "turn")
    # Remove, with the Undo countdown, then Undo.
    _pick(c, spam["id"])
    c.wait("document.querySelector('#entry .card .msg.hit') !== null", 20)
    _click(c, "[data-act=remove]")
    first = c.wait("document.getElementById('undo-left') && document.getElementById('undo-left').textContent")
    m = re.match(r"^Undo possible for (\d+):(\d\d)$", first)
    assert m and 9 * 60 <= int(m.group(1)) * 60 + int(m.group(2)) <= 600, first
    c.pump(2.2)
    later = c.eval("document.getElementById('undo-left').textContent")
    assert later != first and later.startswith("Undo possible for "), (first, later)
    _click(c, "#undo")
    c.wait("document.getElementById('done') === null && document.querySelector('[data-act=remove]') !== null")
    # Block for 24 hours.
    _click(c, "[data-act=block]")
    c.wait("document.getElementById('duration-form') !== null")
    c.eval("(function(){var s=document.getElementById('duration');s.value='24h';s.dispatchEvent(new Event('change'))})()")
    _click(c, "[data-act=block-go]")
    done = c.wait("document.getElementById('done') && document.getElementById('done').textContent")
    assert "N0SPM is blocked from posting until" in done, done
    # Stop counting for good.
    _pick(c, old["id"])
    _click(c, "[data-act=stop-counting]")
    c.eval("(function(){var s=document.getElementById('duration');s.value='forever';s.dispatchEvent(new Event('change'))})()")
    _click(c, "[data-act=stop-counting-go]")
    done = c.wait("document.getElementById('done') && document.getElementById('done').textContent")
    assert "W0OLD's readings and keyed state are not counted for good" in done, done
    # Another helper acts first.
    _pick(c, station["id"])
    got = server.control(f"helper?act=leave&report={station['id']}")
    assert got["status"] == 200, got
    _click(c, "[data-act=leave]")
    text = c.wait("document.getElementById('conflict') && document.getElementById('conflict').textContent")
    assert re.match(r"^Another helper acted on this report first: W8DXR left it as it is at \d\d:\d\d UTC\.", text), text
    # Leave it.
    _pick(c, turn["id"])
    assert "W4XYZ" in c.eval("document.getElementById('entry').textContent")
    _click(c, "[data-act=leave]")
    done = c.wait("document.getElementById('done') && document.getElementById('done').textContent")
    assert done.startswith("Left as it is."), done
    # The lifts, from the Blocked stations tab.
    c.eval("document.querySelector('[data-tab=blocked]').click()")
    c.wait("document.querySelectorAll('#blocked .b').length === 2", 20)
    rows = c.eval("[...document.querySelectorAll('#blocked .b')].map(function(b){return b.textContent})")
    assert any("N0SPM" in r and "blocked from posting until" in r for r in rows), rows
    assert any("W0OLD" in r and "not counted for good" in r for r in rows), rows
    _click(c, "#blocked .b [data-act=unblock]")
    c.wait("document.querySelectorAll('#blocked .b').length === 1", 20)
    _click(c, "#blocked .b [data-act=resume-counting]")
    c.wait("document.querySelectorAll('#blocked .b').length === 0", 20)
    # Helpers by station id.
    c.eval("(function(){var f=document.getElementById('find');f.value='K4ABC';f.form.requestSubmit()})()")
    _click(c, "#found [data-act=helper-add]")
    c.wait("[...document.querySelectorAll('#helpers .b')].some(function(b){return /K4ABC/.test(b.textContent)})", 20)
    c.eval("[...document.querySelectorAll('#found button')].find(function(b){return b.textContent==='Remove helper'}).click()")
    c.wait("document.querySelector('#found [data-act=helper-add]') !== null", 20)
    # The log, newest first, with the undone action marked.
    c.eval("document.querySelector('[data-tab=log]').click()")
    c.wait("document.querySelectorAll('#log .l').length >= 9", 20)
    kinds = c.eval("[...document.querySelectorAll('#log .l')].map(function(l){return l.dataset.action})")
    for k in ("remove", "undo", "block", "stop-counting", "leave", "unblock", "resume-counting", "helper-add", "helper-remove"):
        assert k in kinds, (k, kinds)
    assert c.eval("[...document.querySelectorAll('#log .l')].some(function(l){return /\\(undone\\)/.test(l.textContent)})")
    no_errors(c, "review", refusals=(409,))


def probe_key(c: Chrome, server: Server) -> None:
    """A key shown once; sign in with a key; change the callsign; move my
    history to a new key, which only the key itself can do."""
    c.goto(server.base + "/key")
    c.wait("document.getElementById('new-key') !== null")
    text = c.eval("document.body.textContent")
    assert "no email and no password" in text and "Move my history to a new key" in text, text[:300]
    c.eval(
        "(function(){document.getElementById('new-callsign').value='W4KEY';document.getElementById('new-grid').value='EM73';"
        "document.getElementById('new-key').requestSubmit()})()"
    )
    key = c.wait("document.getElementById('new-key-shown') && document.getElementById('new-key-shown').textContent")
    assert re.match(r"^[A-Za-z0-9_-]{43}$", key), key
    assert "This is the only time it is shown" in c.eval("document.body.textContent")
    c.eval(
        f"(function(){{document.getElementById('profile-key').value={js(key)};document.getElementById('profile-callsign').value='W4KEZ';"
        "document.getElementById('profile-grid').value='EM74';document.getElementById('profile').requestSubmit()})()"
    )
    saved = c.wait("document.querySelector('#profile + .ok') && document.querySelector('#profile + .ok').textContent")
    assert saved == "Saved: W4KEZ in EM74.", saved
    assert c.eval("document.querySelector('#move button[type=submit]').disabled") is True
    c.eval(f"document.getElementById('move-key').value={js(key)}; document.getElementById('move-confirm').click()")
    c.eval("document.querySelector('#move button[type=submit]').click()")
    new_key = c.wait("document.getElementById('moved-key-shown') && document.getElementById('moved-key-shown').textContent", 20)
    assert new_key != key and re.match(r"^[A-Za-z0-9_-]{43}$", new_key)
    # The old key stops working; the new one signs in.
    c.eval(f"document.getElementById('signin-key').value={js(key)}; document.getElementById('signin').requestSubmit()")
    err = c.wait("document.querySelector('#signin + .err') && document.querySelector('#signin + .err').textContent")
    assert err, err
    c.eval(f"document.getElementById('signin-key').value={js(new_key)}; document.getElementById('signin').requestSubmit()")
    c.wait("location.pathname === '/archive'", 20)
    c.wait("document.body.textContent.indexOf('W4KEZ') >= 0", 20)
    no_errors(c, "key", refusals=(403,))


def probe_no_move_for_others(c: Chrome, server: Server) -> None:
    """D26: no page moves history for someone else. The only move control
    is on the key page, and it takes the key itself."""
    for path in ("/", "/archive", "/review"):
        c.goto(server.base + path, settle=1.5)
        words = c.eval("[...document.querySelectorAll('button, a, input, label')].map(function(e){return e.textContent + ' ' + (e.value||'')}).join(' | ')")
        assert not re.search(r"\bmove\b", words, re.I), (path, words)
    c.goto(server.base + "/key")
    c.wait("document.getElementById('move') !== null")
    fields = c.eval("[...document.querySelectorAll('#move input')].map(function(i){return i.id})")
    assert fields == ["move-key", "move-confirm"], fields
    c.events.clear()


PROBES = [
    probe_bands,
    probe_band_map,
    probe_row,
    probe_search,
    probe_sort,
    probe_keyboard,
    probe_freshness,
    probe_inert,
    probe_copy_link,
    probe_connection,
    probe_archive,
    probe_review,
    probe_no_move_for_others,
    probe_key,
]


# ----------------------------------------------------------- screenshots and contrast


def shots(c: Chrome, server: Server, out: Path) -> List[str]:
    """Every page at 1480x1000 and 390x844, with the size and contrast
    check run on each view. Signs in as KG4VCF first."""
    done: List[str] = []
    fails: List[Dict[str, Any]] = []
    s = server.facts["sessions"]
    views = [
        ("public", "/", "document.querySelector('.band[data-band=\"20m\"]').click(); RT.select('usb-14212000', true)", "document.querySelector('#rows .row[data-id=\"usb-14212000\"] .strip:not(.none)') && document.querySelectorAll('#room .rd').length >= 8"),
        ("public-all", "/", "", "document.querySelectorAll('#rows .row .strip:not(.none)').length >= 5"),
        ("public-corpus", "/r/lsb/7185000", "", "document.querySelectorAll('#room .msg').length >= 40"),
        ("archive", f"/archive?s={s['busy']}", "", "document.querySelectorAll('.lane[data-lane]').length === 12"),
        ("archive-empty", f"/archive?s={s['empty']}", "", "/Nobody reported you/.test(document.body.textContent)"),
        ("archive-old", f"/archive?s={s['old']}", "", "/Message removed after a year/.test(document.body.textContent)"),
        ("review", "/review", "", "document.querySelectorAll('.it').length >= 1 && document.getElementById('entry') !== null"),
        ("key", "/key", "", "document.getElementById('new-key') !== null"),
    ]
    sign_in(c, server)
    for width, height, tag in ((1480, 1000, "desktop"), (390, 844, "phone")):
        c.size(width, height, mobile=width < 768)
        for name, path, act, ready in views:
            c.goto(server.base + path, settle=0.3)
            if name.startswith("public"):
                c.wait("window.RT && RT.linkState() === 'live'", 40)
            if act:
                c.eval(act)
            c.wait(ready, 40)
            c.pump(1.2)
            file = out / f"{name}-{tag}.png"
            c.screenshot(file)
            done.append(file.name)
            fails += contrast_fails(c, f"{name} {tag}")
            if name == "public" and tag == "phone":
                # The room as a phone shows it after a tap.
                c.eval("document.querySelector('#rows .row[data-id=\"usb-14212000\"]').click()")
                c.wait("document.body.classList.contains('in-room')")
                c.pump(0.8)
                file = out / "public-room-phone.png"
                c.screenshot(file)
                done.append(file.name)
                fails += contrast_fails(c, "public room phone")
    c.size(1480, 1000)
    # The signed-out pages.
    c.call("Network.clearBrowserCookies")
    for name, path, ready in (("archive-signed-out", "/archive", "document.querySelector('.center') !== null"), ("review-signed-out", "/review", "document.querySelector('.center') !== null")):
        c.goto(server.base + path, settle=0.5)
        c.wait(ready, 20)
        file = out / f"{name}-desktop.png"
        c.screenshot(file)
        done.append(file.name)
        fails += contrast_fails(c, name)
    assert not fails, f"text under 11 px or under 4.5:1: {json.dumps(fails, indent=1)}"
    return done
