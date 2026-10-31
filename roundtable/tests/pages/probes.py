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
    c.wait("(function(){return window.RT&&RT.frequencyVersion()===1&&RT.summaries().length===200&&RT.linkState()==='live';})()", 40)


def set_query(c: Chrome, text: str) -> None:
    c.eval(f"(function(){{var q=document.getElementById('q');q.value={js(text)};q.dispatchEvent(new Event('input'));}})()")


def rooms_api(server: Server) -> List[Dict[str, Any]]:
    return server.get("/v1/api/rooms")["rooms"]


def quiet(r: Dict[str, Any]) -> bool:
    return r["state"] == "quiet" or r["folded"]


# ----------------------------------------------------------- public page


def probe_bands(c: Chrome, server: Server) -> None:
    """Band counts represent exact published slices, without RF-room inference."""
    open_public(c, server.base)
    got=c.eval("({people:RT.frequency.people(RT.summaries()),buttons:[...document.querySelectorAll('.band')].map(b=>({band:b.dataset.band,count:+b.querySelector('.c').textContent,disabled:b.disabled}))})")
    counts=Counter(p["band"] for p in got["people"])
    assert len(got["people"])>200
    for button in got["buttons"]:
        expected=counts[button["band"]] if button["band"] else len(got["people"])
        assert button["count"]==expected,button
        if button["band"]:
            assert button["disabled"]==(expected==0),button
    c.eval("document.querySelector('.band[data-band=\"80m\"]').click()")
    c.wait("RT.state.band==='80m' && document.querySelector('.band[data-band=\"80m\"]').classList.contains('on')")
    assert c.eval("[...document.querySelectorAll('#rows .person-row')].every(n=>{const p=RT.frequency.people(RT.summaries()).find(p=>RT.frequency.personKey(p)===n.dataset.person);return p&&p.band==='80m'})")
    no_errors(c,"frequency bands")


def probe_band_map(c: Chrome, server: Server) -> None:
    """Actual Hz ticks stay anchored; cochannel stacks differ from geometric folds."""
    open_public(c,server.base)
    c.eval("document.querySelector('.band[data-band=\"20m\"]').click()")
    c.wait("document.querySelectorAll('.frequency-tick').length>0")
    ticks=c.eval("({width:document.querySelector('.people-map').clientWidth,ticks:[...document.querySelectorAll('.frequency-tick')].map(t=>({hz:+t.dataset.hz,x:parseFloat(t.style.left)})),groups:RT.frequency.groups(RT.frequency.people(RT.summaries()).filter(p=>p.band==='20m')).length})")
    assert len(ticks["ticks"])==ticks["groups"]
    for tick in ticks["ticks"]:
        assert abs(tick["x"]-(tick["hz"]-14000000)/350000*ticks["width"])<0.6,tick
    axis=c.eval("[...document.querySelectorAll('#bmap .axis span')].map(n=>n.textContent)")
    assert axis[0]=="14.000" and axis[-1]=="14.350",axis
    folds=c.eval("[...document.querySelectorAll('.maptag.folded')].map(n=>n.dataset.groups.split(','))")
    assert folds and all(len(group)>1 for group in folds),folds
    c.eval("document.querySelector('.maptag.folded').click()")
    c.wait("document.querySelectorAll('#frequency-details .fold-choice').length>1")
    assert not re.search(r"\btune\b",c.eval("document.getElementById('frequency-details').textContent"),re.I)
    c.eval("document.querySelector('#frequency-details .fold-choice').click()")
    c.wait("document.getElementById('frequency-details')===null")
    before=c.eval("document.querySelectorAll('#rows .person-row').length")
    c.eval("document.getElementById('quiet-chip').click()")
    after=c.wait(f"RT.state.quiet===false && document.querySelectorAll('#rows .person-row').length<{before} && document.querySelectorAll('#rows .person-row').length")
    assert after<before,(before,after)
    c.eval("document.getElementById('quiet-chip').click()")
    no_errors(c,"frequency map")


def probe_row(c: Chrome, server: Server) -> None:
    """People keep exact Hz and transient identity; reporter rows join turn IDs."""
    open_public(c,server.base)
    c.eval("RT.select('usb-14212000')")
    c.wait("RT.currentView()&&RT.currentView().view==='frequency'&&document.querySelectorAll('.attributed-reading').length>=8")
    result=c.eval("(()=>{const room=RT.currentView(),turn=RT.frequency.lastTurn(room,{hz:14212000,family:'usb'});return {person:[...document.querySelectorAll('#rows .person-row')].slice(0,30).map(n=>({key:n.dataset.person,hz:+n.dataset.hz})),people:RT.frequency.people(RT.summaries()),rows:[...document.querySelectorAll('.attributed-reading')].map(n=>({ref:n.dataset.ref,turn:+n.dataset.turn,text:n.querySelector('.v').textContent})),expected:RT.frequency.readings(room,turn).map(RT.frequency.memberKey),turn:turn.id,label:document.querySelector('.attributed-talker').textContent};})()")
    by_key={f"{p['sourceRoom']}/{p['ref']}/{p['slice']}":p for p in result["people"]}
    for row in result["person"]:
        assert row["hz"]==by_key[row["key"]]["hz"],row
    assert [r["ref"] for r in result["rows"]]==result["expected"]
    assert all(r["turn"]==result["turn"] and "\n" in r["text"] for r in result["rows"])
    assert "K4ABC" in result["label"],result["label"]
    assert "Heard by" in c.eval("document.querySelector('.reading-head').textContent")
    # Initial data must reveal the talker, rather than follow the bottom of
    # the preceding empty Loading body as if it were an opened conversation.
    assert c.eval("(()=>{const body=document.querySelector('.rbody').getBoundingClientRect(),talker=document.querySelector('.attributed-talker').getBoundingClientRect();return talker.top>=body.top&&talker.bottom<=body.bottom})()"), "newly opened frequency hid its last talker"
    no_errors(c,"frequency people/report attribution")


def probe_signal_mobile(c: Chrome, server: Server) -> None:
    """A raw fractional report stays precise in data and fits beside age on a phone."""
    c.size(320, 800, mobile=True)
    try:
        open_public(c, server.base)
        c.eval("RT.select('usb-14212000', true)")
        result = c.wait("""(()=>{const row=[...document.querySelectorAll('.attributed-reading')].find(n=>n.textContent.includes('N4RDR'));
          if(!row)return false; const signal=row.querySelector('.v'),age=row.querySelector('.age');
          const a=signal.getBoundingClientRect(),b=age.getBoundingClientRect();
          const report=RT.currentView().members.find(m=>m.callsign==='N4RDR').reading;
          return {text:signal.textContent,title:row.title,raw:report.dbm,signalRight:a.right,ageLeft:b.left,signalScroll:signal.scrollWidth,signalWidth:signal.clientWidth,age:age.textContent};})()""")
        assert result["raw"] == -66.735748, result
        assert result["text"] == "S9+6\n-66.7", result
        for width in (320, 390):
            c.size(width, 800, mobile=True)
            c.screenshot(Path(f"/tmp/nereus-signal-both-{width}.png"))
        name = c.eval("[...document.querySelectorAll('.attributed-reading')].find(n=>n.textContent.includes('N4RDR')).querySelector('.signal-value').getAttribute('aria-label')")
        assert 'N4RDR' in name and 'S9+6' in name and '-66.7' in name and 'Both' in name, name
        c.eval("document.querySelector('.signal-heading').focus()")
        assert c.eval("document.activeElement.id") == "signal-heading"
        c.key("Enter", text="\r")
        c.wait("RT.signalFormat==='dbm' && document.querySelector('.signal-value').textContent.indexOf('\\n')<0")
        c.eval("document.querySelector('.signal-value').focus()")
        c.key("Enter", text="\r")
        c.wait("RT.signalFormat==='s'")
        c.eval("document.querySelector('.signal-value').click()")
        c.wait("RT.signalFormat==='both'")
        assert c.eval("localStorage.getItem('roundtable.signalFormat')") == "both"
        assert c.eval("RT.currentView().members.find(m=>m.callsign==='N4RDR').reading.dbm") == -66.735748
        assert "-66.7 dBm" in result["title"], result
        assert result["signalRight"] <= result["ageLeft"] and result["signalScroll"] <= result["signalWidth"], result
        assert re.fullmatch(r"\d+ s", result["age"]), result
        no_errors(c, "mobile signal precision")
    finally:
        c.size(1280, 800, mobile=False)


def probe_search(c: Chrome, server: Server) -> None:
    """Search finds published talkers/readers/posters and exact numeric browsing."""
    open_public(c,server.base)
    for call in ("K4ABC","N4RDR","VE3PST"):
        set_query(c,call)
        c.wait("document.querySelectorAll('#rows .person-row').length>0")
        assert c.eval("document.querySelectorAll('#rows mark').length")>0 or call=="VE3PST"
    set_query(c,"14.212007")
    c.wait("RT.state.sel==='usb-14212007'")
    c.wait("document.querySelector('#room .rhead .f').textContent==='14.212.007'")
    assert c.eval("document.querySelector('#room .rhead .frequency-range').textContent").startswith("±5 kHz")
    no_errors(c,"frequency search")


def probe_sort(c: Chrome, server: Server) -> None:
    """Frequency and crowd sorting use exact groups, not RF-room centers."""
    open_public(c,server.base)
    c.eval("document.querySelector('.band[data-band=\"20m\"]').click()")
    c.wait("document.querySelectorAll('.frequency-group').length>5")
    freq=c.eval("[...document.querySelectorAll('.frequency-group')].map(n=>+n.dataset.frequency.split('-')[1])")
    assert freq==sorted(freq)
    c.eval("(()=>{let s=document.getElementById('sort');s.value='n';s.dispatchEvent(new Event('change'))})()")
    c.wait("RT.state.sort==='n'")
    counts=c.eval("[...document.querySelectorAll('.frequency-group')].map(n=>n.querySelectorAll('.person-row').length)")
    assert counts==sorted(counts,reverse=True),counts
    c.eval("(()=>{let s=document.getElementById('sort');s.value='t';s.dispatchEvent(new Event('change'))})()")
    c.wait("RT.state.sort==='t'")
    # State changes before the scheduled DOM render; sample rendered order
    # and its current source atomically, rather than inspecting the old DOM.
    stamps=c.wait("(()=>{const stamps=[...document.querySelectorAll('.frequency-group')].map(n=>{const row=n.querySelector('.person-row'),p=RT.frequency.people(RT.summaries()).find(p=>RT.frequency.personKey(p)===row.dataset.person);return RT.summaries().find(r=>r.id===p.sourceRoom).last});return stamps.length&&stamps.every((t,i)=>!i||stamps[i-1]>=t)?stamps:false})()")
    assert stamps==sorted(stamps,reverse=True),stamps
    no_errors(c,"frequency sorting")


def probe_keyboard(c: Chrome, server: Server) -> None:
    """Search, group navigation, turn navigation and Escape retain keyboard access."""
    open_public(c,server.base)
    c.key("/")
    assert c.eval("document.activeElement.id")=="q"
    c.eval("document.getElementById('q').blur();document.getElementById('rows').focus()")
    before=c.eval("RT.state.sel")
    c.key("ArrowDown")
    c.wait(f"RT.state.sel!=={js(before)}")
    c.eval("RT.select('usb-14212000');document.getElementById('rows').focus()")
    c.wait("document.querySelectorAll('#room .turn').length>=2")
    c.key("Enter")
    c.wait("document.activeElement.classList.contains('turn')")
    first=c.eval("document.activeElement.dataset.turn")
    c.key("ArrowDown")
    c.wait(f"document.activeElement.dataset.turn!=={js(first)}")
    c.key("Escape")
    assert c.eval("document.activeElement.id")=="rows"
    set_query(c,"K4ABC")
    c.key("Escape")
    c.wait("RT.state.q===''")
    no_errors(c,"frequency keyboard")


def probe_freshness(c: Chrome, server: Server) -> None:
    """Stale raw readings retain amber age; path units preserve canonical ordering."""
    open_public(c,server.base)
    c.eval("RT.select('usb-14212000')")
    c.wait("document.querySelectorAll('.attributed-reading').length>=8")
    stale=c.wait("(()=>{const n=[...document.querySelectorAll('.attributed-reading.stale')].find(n=>n.textContent.includes('W5STL'));return n&&{age:n.querySelector('.age').textContent,color:getComputedStyle(n.querySelector('.age')).color}})()")
    assert stale["color"]==AMBER,stale
    before=c.eval("[...document.querySelectorAll('.attributed-reading')].map(n=>({ref:n.dataset.ref,km:n.dataset.km,bearing:n.querySelector('.path-value small')?.textContent||''}))")
    assert c.eval("document.getElementById('distance-unit').value")=="mi"
    c.eval("(()=>{const u=document.getElementById('distance-unit');u.value='km';u.dispatchEvent(new Event('change'))})()")
    c.wait("document.getElementById('distance-unit').value==='km'")
    after=c.eval("[...document.querySelectorAll('.attributed-reading')].map(n=>({ref:n.dataset.ref,km:n.dataset.km,bearing:n.querySelector('.path-value small')?.textContent||''}))")
    assert before==after,(before,after)
    known=[float(n["km"]) for n in after if n["km"]!=""]
    assert known==sorted(known)
    c.eval("(()=>{const u=document.getElementById('distance-unit');u.value='mi';u.dispatchEvent(new Event('change'))})()")
    no_errors(c,"frequency freshness and paths")


def probe_copy_link(c: Chrome, server: Server) -> None:
    """A copied frequency link restores its context without radio authority."""
    open_public(c,server.base)
    c.eval("RT.select('usb-14212000')")
    c.wait("document.getElementById('copy-link')!==null")
    c.eval("document.getElementById('copy-link').click()")
    copied=c.wait("window.__rtCopied")
    assert copied.endswith("/r/usb/14212000"),copied
    c.goto(server.base+"/r/usb/14212000")
    c.wait("RT.state.sel==='usb-14212000'&&document.querySelector('.attributed-talker')?.textContent.includes('K4ABC')")
    assert not re.search(r"\btune\b",c.eval("document.getElementById('room').textContent"),re.I)
    no_errors(c,"frequency link")


def probe_owner_frequency(c: Chrome, server: Server) -> None:
    """Real owner cookies follow accepted presence; drafts and deletion retain context."""
    path=server.control("signin?who=K4ABC")["path"]
    c.goto(server.base+path)
    c.wait("location.pathname==='/archive'")
    open_public(c,server.base)
    c.wait("document.getElementById('follow-station')!==null&&!document.getElementById('follow-station').disabled")
    c.eval("document.getElementById('follow-station').click()")
    c.wait("RT.state.following&&!document.querySelector('.frequency-composer textarea').disabled")
    actual=c.eval("RT.state.sel")
    text="Website frequency ownership regression"
    c.eval(f"(()=>{{const t=document.querySelector('.frequency-composer textarea');t.value={js(text)};t.dispatchEvent(new Event('input'));}})()")
    c.eval("RT.select('lsb-7185000')")
    c.wait("document.querySelector('.frequency-composer textarea').disabled")
    c.eval("document.getElementById('earlier-conversation').click()")
    c.wait("RT.state.earlier===true")
    assert c.eval("document.querySelector('.frequency-composer textarea').disabled")
    c.eval("document.getElementById('follow-station').click()")
    c.wait(f"RT.state.sel==={js(actual)}&&!RT.state.earlier&&!document.querySelector('.frequency-composer textarea').disabled")
    assert c.eval("document.querySelector('.frequency-composer textarea').value")==text
    c.eval("document.querySelector('.frequency-composer button[type=submit]').click()")
    c.wait(f"[...document.querySelectorAll('#room .msg.me')].some(n=>n.querySelector('.x').textContent==={js(text)})",30)
    c.eval(f"[...document.querySelectorAll('#room .msg.me')].find(n=>n.querySelector('.x').textContent==={js(text)}).querySelector('button').click()")
    c.wait("[...document.querySelectorAll('#room .msg.me')].some(n=>n.querySelector('.x').textContent==='Deleted by sender')")
    assert not re.search(r"\btune\b",c.eval("document.getElementById('room').textContent"),re.I)
    no_errors(c,"real owner frequency follow")


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
        assert c.eval("RT.frequencyVersion()===1 && document.querySelectorAll('.person-row[data-person]').length>0 && !document.querySelector('.legacy-notice')"), "reconnect must preserve last known exact directory presentation"
        line = c.wait("/^Offline/.test(document.getElementById('conn').textContent) && document.getElementById('conn').textContent", 40)
        assert re.match(r"^Offline since \d\d:\d\d UTC$", line), line
        assert re.match(r"^since \d\d:\d\d UTC$", c.eval("document.getElementById('live-k').textContent"))
    finally:
        server.control("live?do=allow")
    c.eval("RT.reconnectNow()")
    c.wait("RT.linkState() === 'live'", 30)
    c.events.clear()




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


def probe_retained_history(c: Chrome, server: Server) -> None:
    """Retuned cached chat validates deletion/removal/expiry without revisiting."""
    path=server.control("signin?who=K4ABC")["path"]
    c.goto(server.base+path)
    open_public(c,server.base)
    c.wait("document.getElementById('follow-station')&&!document.getElementById('follow-station').disabled")
    for action,label in (("delete","Deleted by sender"),("remove","Message removed"),("expire","Message removed after a year")):
        c.eval("document.getElementById('follow-station').click()")
        c.wait("RT.state.following&&!document.querySelector('.frequency-composer textarea').disabled")
        text="Retained status regression "+action
        c.eval(f"(()=>{{const t=document.querySelector('.frequency-composer textarea');t.value={js(text)};t.dispatchEvent(new Event('input'));document.querySelector('.frequency-composer button[type=submit]').click();}})()")
        ident=c.wait(f"(()=>{{const m=[...document.querySelectorAll('#room .msg.me')].find(n=>n.querySelector('.x').textContent==={js(text)});return m&&+m.dataset.msg;}})()",30)
        c.eval("RT.select('usb-14350000');document.getElementById('earlier-conversation').click()")
        c.wait(r"RT.state.earlier&&/checked \d\d:\d\d UTC/.test(document.querySelector('#room .rbody').textContent)")
        result=server.control(f"history?act={action}&msg={ident}")
        assert result["state"]=={"delete":"deleted","remove":"removed","expire":"expired"}[action]
        # No revisit, render call or test-triggered request: the real 10s
        # periodic known-ID query must update this already-open history view.
        got=c.wait(f"document.querySelector('.msg[data-msg=\"{ident}\"] .x')?.textContent==={js(label)}",15)
        assert got and c.eval("RT.state.sel")=="usb-14350000"
        assert c.eval("document.querySelector('.frequency-composer textarea').disabled")
    no_errors(c,"retained known-ID history refresh")


PROBES = [
    probe_bands,
    probe_band_map,
    probe_row,
    probe_signal_mobile,
    probe_search,
    probe_sort,
    probe_keyboard,
    probe_freshness,
    probe_inert,
    probe_copy_link,
    probe_owner_frequency,
    probe_connection,
    probe_archive,
    probe_review,
    probe_no_move_for_others,
    probe_key,
    probe_retained_history,
]


# ----------------------------------------------------------- screenshots and contrast


def shots(c: Chrome, server: Server, out: Path) -> List[str]:
    """Every page at 1480x1000 and 390x844, with the size and contrast
    check run on each view. Signs in as KG4VCF first."""
    done: List[str] = []
    fails: List[Dict[str, Any]] = []
    s = server.facts["sessions"]
    views = [
        ("public", "/", "document.querySelector('.band[data-band=\"20m\"]').click(); RT.select('usb-14212000', true)", "document.querySelector('#rows .person-row[data-id=\"usb-14212000\"]') && document.querySelectorAll('#room .attributed-reading').length >= 8"),
        ("public-all", "/", "", "document.querySelectorAll('#rows .person-row').length > 200"),
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
