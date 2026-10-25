# no-port-check: NereusSDR-original.
"""Web sign-in links and browser sessions (design 7.4, C14, C27; wire spec
5.3.12, 5.4.10, 7.9). Refusals first: a reused or expired link, a POST
without the session's CSRF token or from another origin, an expired or
ended cookie, and one station's cookie asking for another's archive."""

from __future__ import annotations

import pytest

from inproc import Harness
from nereus_roundtable import identity
from webclient import Browser, body_json, opening, stored_turn, weblink

MINUTES_5 = 300000
DAYS_30 = 2592000000


@pytest.fixture
def harness():
    h = Harness()
    yield h
    h.close()


@pytest.fixture
def browser(harness):
    return Browser(harness)


def _path(browser, url):
    assert url.startswith(browser.origin + "/signin?t=")
    return url[len(browser.origin) :]


# ----------------------------------------------------------- refusals


def test_a_link_works_once(harness, browser):
    station = harness.sign_in()
    path = _path(browser, weblink(station))
    first = browser.get(path)
    assert first.status == 303
    again = browser.get(path)
    assert again.status == 403
    assert again.head("Set-Cookie") is None
    assert harness.store.db.execute("SELECT COUNT(*) FROM web_sessions").fetchone()[0] == 1


def test_a_head_request_does_not_use_up_the_link(harness, browser):
    station = harness.sign_in()
    path = _path(browser, weblink(station))
    head = browser.request("HEAD", path)
    assert head.status == 200
    assert head.head("Set-Cookie") is None
    assert harness.store.db.execute("SELECT COUNT(*) FROM web_sessions").fetchone()[0] == 0
    first = browser.get(path)
    assert first.status == 303
    assert first.head("Set-Cookie") is not None
    assert browser.get(path).status == 403


def test_a_link_does_not_work_after_5_minutes(harness, browser):
    station = harness.sign_in()
    early = _path(browser, weblink(station))
    late = _path(browser, weblink(station))
    harness.advance(MINUTES_5 - 1)
    assert browser.get(early).status == 303
    harness.advance(1)
    r = browser.get(late)
    assert r.status == 403
    assert r.head("Set-Cookie") is None


@pytest.mark.parametrize(
    "target", ["/signin", "/signin?t=", "/signin?t=abc", "/signin?t=" + "A" * 43, "/signin?t=x&t=y"]
)
def test_a_link_that_was_never_made_is_refused(harness, browser, target):
    harness.sign_in()
    r = browser.get(target)
    assert r.status == 403
    assert r.head("Set-Cookie") is None


def test_a_link_with_a_known_token_twice_in_the_query_is_refused(harness, browser):
    station = harness.sign_in()
    path = _path(browser, weblink(station))
    token = path.split("=", 1)[1]
    assert browser.get(f"{path}&t={token}").status == 403
    assert browser.get(path).status == 303


@pytest.mark.parametrize("path", ["/signout-everywhere", "/archive/delete"])
def test_a_signed_in_post_needs_the_origin_and_the_csrf_token(harness, browser, path):
    station = harness.sign_in()
    oid = opening(harness)
    stored_turn(harness, oid, "talking", [(station.station_id, "W4XYZ")], 1000, 5000)
    cookie = browser.link_cookie(station)
    csrf = browser.csrf(cookie)
    other = browser.link_cookie(harness.sign_in(address="192.0.2.2"))
    form = {"confirm": "delete my archive"}
    refusals = [
        browser.post(path, form, cookie),
        browser.post(path, dict(form, csrf=""), cookie),
        browser.post(path, dict(form, csrf=csrf[:-1] + ("A" if csrf[-1] != "A" else "B")), cookie),
        # Another session's token.
        browser.post(path, dict(form, csrf=browser.csrf(other)), cookie),
        browser.post(path, dict(form, csrf=csrf), cookie, origin="https://evil.example"),
        browser.post(path, dict(form, csrf=csrf), cookie, origin=None),
        browser.post(path, dict(form, csrf=csrf), cookie, origin="null"),
        browser.post(path, dict(form, csrf=csrf), cookie, origin="http://roundtable.conformance.invalid"),
        browser.post(path, dict(form, csrf=csrf), cookie, origin=[browser.origin, browser.origin]),
    ]
    assert [r.status for r in refusals] == [403] * 9
    assert all(body_json(r)["error"] == "forbidden" for r in refusals)
    # Without a cookie it is not signed in.
    assert browser.post(path, dict(form, csrf=csrf)).status == 401
    # Nothing happened.
    assert browser.get("/v1/me", cookie).status == 200
    assert len(harness.store.sessions_of(station.station_id)) == 1


def test_a_cookie_does_not_last_past_30_days(harness, browser):
    station = harness.sign_in()
    cookie = browser.link_cookie(station)
    harness.advance(DAYS_30 - 1)
    assert browser.get("/v1/me", cookie).status == 200
    harness.advance(1)
    r = browser.get("/v1/me", cookie)
    assert r.status == 401 and body_json(r)["error"] == "signed-out"


@pytest.mark.parametrize("cookie", [None, "rt_session=", "rt_session=abc", "rt_session=" + "A" * 43, "other=1"])
def test_no_archive_without_a_session(harness, browser, cookie):
    for path in ("/v1/me", "/v1/me/sessions", "/v1/me/session/1", "/v1/me/histogram"):
        r = browser.get(path, cookie)
        assert r.status == 401, (path, cookie)


def test_one_station_s_cookie_never_reads_another_s_archive(harness, browser):
    a = harness.sign_in(address="192.0.2.1")
    b = harness.sign_in(address="192.0.2.2")
    oid = opening(harness)
    stored_turn(harness, oid, "talking", [(a.station_id, "W4XYZ")], 1000, 5000, [(b.station_id, "K4ABC", [True] * 4)])
    stored_turn(harness, oid, "talking", [(b.station_id, "K4ABC")], 10000, 15000, [(a.station_id, "W4XYZ", [True] * 5)])
    (sa,) = harness.store.sessions_of(a.station_id)
    (sb,) = harness.store.sessions_of(b.station_id)
    cookie_b = browser.link_cookie(b)
    assert browser.get(f"/v1/me/session/{sb['id']}", cookie_b).status == 200
    r = browser.get(f"/v1/me/session/{sa['id']}", cookie_b)
    assert r.status == 404 and body_json(r)["error"] == "not-found"
    listed = body_json(browser.get("/v1/me/sessions", cookie_b))["sessions"]
    assert [s["id"] for s in listed] == [sb["id"]]
    for bad in ("0", "-1", "01", "x", str(sa["id"]) + "/", "99999"):
        assert browser.get(f"/v1/me/session/{bad}", cookie_b).status == 404, bad
    # A's archive is untouched by B's deleting and signing out.
    csrf = browser.csrf(cookie_b)
    assert browser.post("/archive/delete", {"csrf": csrf, "confirm": "delete my archive"}, cookie_b).status == 200
    assert [s["id"] for s in harness.store.sessions_of(a.station_id)] == [sa["id"]]


def test_the_archive_json_is_private(harness, browser):
    cookie = browser.link_cookie(harness.sign_in())
    for path in ("/v1/me", "/v1/me/sessions", "/v1/me/histogram"):
        r = browser.get(path, cookie)
        assert r.status == 200
        assert r.head("Cache-Control") == "no-store"
        assert r.head("Access-Control-Allow-Origin") is None
    assert "station" not in body_json(browser.get("/v1/me", cookie))["station"]


# ----------------------------------------------------------- flows


def test_the_fixture_link_and_its_expiry(harness, browser):
    station = harness.sign_in()
    station.send({"type": "weblink"})
    (ok,) = station.take()
    assert ok["type"] == "weblinkOk"
    assert ok["expires"] == harness.clock.wall_ms() + MINUTES_5
    token = ok["url"].split("?t=", 1)[1]
    assert ok["url"] == f"https://{harness.config.public_host}/signin?t={token}"
    raw = identity.b64url_of_length(token, 32)
    assert raw is not None
    # Only the token's hash is stored.
    assert [bytes(r[0]) for r in harness.store.db.execute("SELECT token_hash FROM weblinks")] == [identity.key_hash(raw)]


def test_the_redirect_leaves_the_token_out_of_the_final_address(harness, browser):
    station = harness.sign_in()
    path = _path(browser, weblink(station))
    token = path.split("=", 1)[1]
    r = browser.get(path)
    assert r.status == 303
    assert r.head("Location") == "/archive"
    assert token not in "".join(v for _, v in r.headers if _ != "Set-Cookie")
    assert r.head("Referrer-Policy") == "no-referrer"
    assert r.head("Cache-Control") == "no-store"
    cookie = r.head("Set-Cookie")
    assert cookie.endswith("; Secure; HttpOnly; SameSite=Strict; Path=/; Max-Age=2592000")
    assert token not in cookie
    me = body_json(browser.get("/v1/me", browser.cookie_from(r)))
    assert me["station"]["kind"] == "core"
    assert identity.b64url_of_length(me["csrf"], 32) is not None


def test_a_hidden_station_can_still_ask_for_a_link(harness, browser):
    station = harness.sign_in()
    station.send({"type": "hidden"})
    station.take()
    url = weblink(station)
    assert browser.get(_path(browser, url)).status == 303


def test_sign_out_everywhere_ends_every_cookie_of_that_station(harness, browser):
    a = harness.sign_in(address="192.0.2.1")
    b = harness.sign_in(address="192.0.2.2")
    first = browser.link_cookie(a)
    second = browser.link_cookie(a)
    other = browser.link_cookie(b)
    r = browser.post("/signout-everywhere", {"csrf": browser.csrf(first)}, first)
    assert r.status == 303
    assert r.head("Location") == "/"
    assert r.head("Set-Cookie") == "rt_session=; Secure; HttpOnly; SameSite=Strict; Path=/; Max-Age=0"
    assert browser.get("/v1/me", first).status == 401
    assert browser.get("/v1/me", second).status == 401
    assert browser.get("/v1/me", other).status == 200
    # A new link signs in again.
    assert browser.get("/v1/me", browser.link_cookie(a)).status == 200


def test_the_web_routes_over_a_real_socket(tmp_path):
    """The routes are on the app the server runs, over HTTP."""
    import asyncio

    from helpers import live_service, sign_in

    async def go():
        async with live_service(http_app=True, static_dir=str(tmp_path)) as live:
            ws, _, _ = await sign_in(live)
            await ws.send('{"type":"weblink"}')
            import json

            ok = json.loads(await asyncio.wait_for(ws.recv(), 5))
            path = ok["url"].split(live.service.config.public_host, 1)[1]
            reader, writer = await asyncio.open_connection("127.0.0.1", live.http_port)
            writer.write(f"GET {path} HTTP/1.1\r\nHost: x\r\n\r\n".encode("ascii"))
            await writer.drain()
            head = (await asyncio.wait_for(reader.read(65536), 5)).decode("latin-1")
            writer.close()
            await ws.close()
            return head

    head = asyncio.run(go())
    assert head.startswith("HTTP/1.1 303 ")
    assert "\r\nLocation: /archive\r\n" in head
    assert "\r\nSet-Cookie: rt_session=" in head
